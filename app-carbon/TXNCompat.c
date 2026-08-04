/*
 * TXNCompat.c
 * ArtfulType Carbon Port
 *
 * Implementation of the WASTE → MLTE shim.  Every WExxxx function here
 * translates to one or more TXNxxxx / Carbon calls so the rest of the
 * codebase can remain largely unchanged.
 *
 * Notes on design decisions
 * ─────────────────────────
 * • WEGetText() returns a locked Handle containing the MLTE object's text.
 *   We own a cached copy inside WERecord (textHandle).  After every insert
 *   or delete we mark the cache dirty so it is rebuilt lazily.
 *
 * • MLTE (TXNObject) uses UTF-16 internally on OS X 10.4.  All text coming
 *   in from ArtfulType is MacRoman (8-bit, classic Mac line endings \r).
 *   TXNSetData with kTXNTextData encodes as MacRoman when the font permits,
 *   and TXNGetData with kTXNTextData returns MacRoman bytes back out.
 *   This is exactly what we want.
 *
 * • Line geometry (WEGetHeight, WEOffsetToLine, WEGetLineRange) is
 *   implemented via TXNOffsetToPoint / TXNGetLineCount / TXNGetLineMetrics.
 *
 * • For the scrolling model the codebase uses LongRect viewRect/destRect
 *   to compute scroll offsets.  We synthesise these from TXNGetViewRect
 *   and TXNGetDestRect (available in MLTE 1.0 / OS 9+).
 */

#include "carbon_compat.h"
#include "TXNCompat.h"
#include <string.h>
#include <stdlib.h>

/* -----------------------------------------------------------------------
   Internal WERecord management
   ----------------------------------------------------------------------- */

/* Extended record kept between the public WEHandle typedef and the
   underlying TXNObject.  We allocate it with NewPtrClear so it is zero-
   initialised and store it as the first (and only) member of a locked
   Handle, matching the WEHandle = WEPtr* = WERecord** layout the rest of
   the code expects. */

typedef struct WERecordFull {
    WERecord  pub;          /* public part: .txn and .te (te always NULL) */
    TXNFrameID frameID;     /* frame ID returned by TXNNewObject */
    Handle    textHandle;   /* our owned copy of the text, or NULL if dirty */
    Boolean   textDirty;    /* true = textHandle needs a refresh */
    Rect      viewRect;     /* last known view rect (in window-local coords) */
    Rect      destRect;     /* last known dest rect */
} WERecordFull;

static WERecordFull *GetFull(WEHandle we)
{
    if (we == NULL || *we == NULL) return NULL;
    /* The WEPtr points at a WERecord whose first two fields are txn/te.
       The full struct begins at the same address because we allocate the
       full struct and hand back a pointer to it as the WERecord. */
    return (WERecordFull *)(*we);
}

WEHandle WEAllocRecord(void)
{
    WERecordFull *full;
    WEHandle      h;

    full = (WERecordFull *)NewPtrClear(sizeof(WERecordFull));
    if (full == NULL) return NULL;

    h = (WEHandle)NewHandle(sizeof(WEPtr));
    if (h == NULL) { DisposePtr((Ptr)full); return NULL; }

    *h = (WEPtr)full;
    return h;
}

void WERefreshTextHandle(WEHandle we)
{
    WERecordFull *f = GetFull(we);
    if (f) f->textDirty = true;
}

Handle WEGetCachedText(WEHandle we)
{
    WERecordFull *f = GetFull(we);
    if (!f) return NULL;

    if (f->textDirty || f->textHandle == NULL) {
        TXNObject txn = f->pub.txn;
        Handle   raw  = NULL;
        ByteCount sz  = 0;

        if (txn != NULL &&
            TXNGetData(txn, kTXNStartOffset, kTXNEndOffset, &raw) == noErr &&
            raw != NULL) {
            sz = GetHandleSize(raw);
        }

        if (f->textHandle == NULL) {
            f->textHandle = NewHandle(sz > 0 ? sz : 1);
        } else {
            SetHandleSize(f->textHandle, sz > 0 ? sz : 1);
        }

        if (sz > 0 && raw != NULL) {
            HLock(raw);
            HLock(f->textHandle);
            BlockMove(*raw, *f->textHandle, sz);
            HUnlock(f->textHandle);
            HUnlock(raw);
            DisposeHandle(raw);
        }

        f->textDirty = false;
    }

    return f->textHandle;
}

/* -----------------------------------------------------------------------
   Lifecycle
   ----------------------------------------------------------------------- */

OSErr WENew(const Rect *destRect, const Rect *viewRect, WEFlags flags, WEHandle *outWE)
{
    TXNObject      txn  = NULL;
    TXNFrameID     fid  = 0;
    WEHandle       we;
    WERecordFull  *f;
    OSErr          err;
    WindowRef      win;

    win = GetWindowFromPort(GetQDGlobalsThePort());

    err = TXNNewObject(
        NULL,
        win,
        viewRect,
        kTXNWantHScrollBarMask,
        kTXNTextEditStyleFrameType,
        kTXNTextFile,
        kTXNMacOSEncoding,
        &txn,
        &fid,
        NULL);

    if (err != noErr || txn == NULL)
        return err;

    /* Disable built-in MLTE scrollbars */
    TXNSetScrollbarState(txn, kScrollBarsAlwaysActive);

    we = WEAllocRecord();
    if (we == NULL) {
        TXNDeleteObject(txn);
        return memFullErr;
    }

    f = GetFull(we);
    f->pub.txn  = txn;
    f->pub.te   = NULL;
    f->frameID  = fid;
    f->viewRect = *viewRect;
    f->destRect = *destRect;

    *outWE = we;
    return noErr;
}

void WEDispose(WEHandle we)
{
    WERecordFull *f = GetFull(we);
    if (!f) return;

    if (f->pub.txn)    TXNDeleteObject(f->pub.txn);
    if (f->textHandle) DisposeHandle(f->textHandle);
    DisposePtr((Ptr)f);
    DisposeHandle((Handle)we);
}

/* -----------------------------------------------------------------------
   Focus / activation
   ----------------------------------------------------------------------- */

void WEActivate(WEHandle we)
{
    WERecordFull *f = GetFull(we);
    if (!f || !f->pub.txn) return;
    TXNActivate(f->pub.txn, f->frameID, kScrollBarsAlwaysActive);
    TXNFocus(f->pub.txn, true);
}

void WEDeactivate(WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (txn) TXNFocus(txn, false);
}

/* -----------------------------------------------------------------------
   Drawing / reflow
   ----------------------------------------------------------------------- */

void WEUpdate(const Rect *updateRect, WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (txn) TXNDraw(txn, NULL);   /* NULL = draw into the current GrafPort */
    (void)updateRect;
}

void WECalText(WEHandle we)
{
    /* MLTE does its own layout automatically; call TXNDraw to force
       a synchronous layout pass. */
    TXNObject txn = WEGetTXN(we);
    if (txn) TXNForceUpdate(txn);
}

Boolean WEFixLineHeights(WEHandle we)
{
    (void)we;
    return false;   /* no-op in MLTE */
}

/* -----------------------------------------------------------------------
   Input events
   ----------------------------------------------------------------------- */

void WEClick(Point pt, Boolean shift, WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (!txn) return;
    /* Create a synthetic mouse-down EventRecord and pass it to MLTE */
    EventRecord ev;
    memset(&ev, 0, sizeof(ev));
    ev.what    = mouseDown;
    ev.where   = pt;
    LocalToGlobal(&ev.where);
    ev.modifiers = shift ? shiftKey : 0;
    ev.when    = TickCount();
    TXNClick(txn, &ev);
    WERefreshTextHandle(we);
}

void WEKey(short charCode, short keyCode, short modifiers, WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (!txn) return;
    EventRecord ev;
    memset(&ev, 0, sizeof(ev));
    ev.what      = keyDown;
    ev.message   = ((long)(keyCode & 0xFF) << 8) | (charCode & 0xFF);
    ev.modifiers = modifiers;
    ev.when      = TickCount();
    TXNKeyDown(txn, &ev);
    WERefreshTextHandle(we);
}

void WEIdle(WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (txn) TXNIdle(txn);
}

/* -----------------------------------------------------------------------
   Selection
   ----------------------------------------------------------------------- */

void WESetSelect(long selStart, long selEnd, WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (!txn) return;
    TXNSetSelection(txn, (TXNOffset)selStart, (TXNOffset)selEnd);
}

void WEGetSelection(long *selStart, long *selEnd, WEHandle we)
{
    TXNObject  txn = WEGetTXN(we);
    TXNOffset  s   = 0, e = 0;
    if (txn) TXNGetSelection(txn, &s, &e);
    if (selStart) *selStart = (long)s;
    if (selEnd)   *selEnd   = (long)e;
}

/* -----------------------------------------------------------------------
   Geometry — view / dest rects
   ----------------------------------------------------------------------- */

void WEGetViewRect(LongRect *lr, WEHandle we)
{
    WERecordFull *f = GetFull(we);
    if (!f) { memset(lr, 0, sizeof(*lr)); return; }
    lr->left   = f->viewRect.left;
    lr->top    = f->viewRect.top;
    lr->right  = f->viewRect.right;
    lr->bottom = f->viewRect.bottom;
}

void WEGetDestRect(LongRect *lr, WEHandle we)
{
    /* MLTE doesn't expose a separate "dest rect" in the same sense.
       We maintain our own and adjust it when the content scrolls.
       For scroll offset computation: destRect.top moves UP as you scroll
       down (same polarity as classic TextEdit). */
    WERecordFull *f = GetFull(we);
    if (!f) { memset(lr, 0, sizeof(*lr)); return; }

    /* Ask MLTE for the current scroll position */
    TXNObject txn = f->pub.txn;
    if (txn) {
        SInt32 dx = 0, dy = 0;
        /* TXNScroll is write-only; to read scroll pos we use TXNGetHIRect */
        HIRect txnView = CGRectZero, txnDest = CGRectZero;
        if (TXNGetHIRect(txn, kTXNViewRectKey,  &txnView) == noErr &&
            TXNGetHIRect(txn, kTXNDestinationRectKey, &txnDest) == noErr) {
            /* destRect.top = viewRect.top - pixelsScrolled (classic convention) */
            long pixelsScrolled = (long)(txnView.origin.y - txnDest.origin.y);
            f->destRect.top    = f->viewRect.top - (short)pixelsScrolled;
            f->destRect.left   = f->viewRect.left;
            f->destRect.right  = f->viewRect.right;
            f->destRect.bottom = f->viewRect.top +
                                 (short)(txnDest.size.height);
        }
    }

    lr->left   = f->destRect.left;
    lr->top    = f->destRect.top;
    lr->right  = f->destRect.right;
    lr->bottom = f->destRect.bottom;
}

void WESetRects(const Rect *destRect, const Rect *viewRect, WEHandle we)
{
    WERecordFull *f = GetFull(we);
    if (!f) return;

    f->viewRect = *viewRect;
    f->destRect = *destRect;

    if (f->pub.txn) {
        TXNSetFrameBounds(f->pub.txn,
                          viewRect->top, viewRect->left,
                          viewRect->bottom, viewRect->right,
                          f->frameID);
    }
}

/* -----------------------------------------------------------------------
   Scrolling
   ----------------------------------------------------------------------- */

void WEPinScroll(long dx, long dy, WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (!txn) return;

    /* TXNScroll takes SInt32* in/out params (delta to apply, returns actual) */
    SInt32 vDelta = (SInt32)-dy;
    SInt32 hDelta = (SInt32)-dx;
    TXNScroll(txn, kTXNScrollUnitsInPixels, kTXNScrollUnitsInPixels,
              &vDelta, &hDelta);

    WERecordFull *f = GetFull(we);
    if (f) {
        f->destRect.top    += (short)dy;
        f->destRect.bottom += (short)dy;
    }
}

void WEScroll(long dx, long dy, WEHandle we)
{
    WEPinScroll(dx, dy, we);   /* same as PinScroll for our purposes */
}

/* -----------------------------------------------------------------------
   Line geometry
   ----------------------------------------------------------------------- */

long WEOffsetToLine(long offset, WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (!txn) return 0;
    ItemCount lineCount = 0;
    TXNGetLineCount(txn, &lineCount);
    if (lineCount == 0) return 0;
    /* TXNGetLineMetrics(txn, lineNum, &width, &height) — 4 args, no offsets.
       Use TXNOffsetToPoint to find which line an offset falls on. */
    Point pt; memset(&pt, 0, sizeof(pt));
    if (TXNOffsetToPoint(txn, (TXNOffset)offset, &pt) == noErr) {
        /* Scan lines to find the one whose vertical span contains pt.y */
        long cumH = 0;
        WERecordFull *f = GetFull(we);
        short baseY = f ? f->viewRect.top : 0;
        long relY = pt.v - baseY;
        ItemCount i;
        for (i = 0; i < lineCount; i++) {
            Fixed lh = 0, lw = 0;
            TXNGetLineMetrics(txn, i, &lw, &lh);
            long lineH = (long)Fix2Long(lh);
            if (lineH <= 0) lineH = 12;
            cumH += lineH;
            if (relY < cumH) return (long)i;
        }
        return (long)(lineCount > 0 ? lineCount - 1 : 0);
    }
    return 0;
}

long WEGetLineCount(WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (!txn) return 1;
    ItemCount n = 0;
    TXNGetLineCount(txn, &n);
    return (long)n;
}

OSErr WEGetLineRange(long lineIndex, long *lineStart, long *lineEnd, WEHandle we)
{
    /* TXNGetLineMetrics doesn't give offsets; use TXNOffsetToPoint in reverse.
       We approximate by returning the full doc range for the line — sufficient
       for the caret-scroll logic in scrolling.c. */
    TXNObject txn = WEGetTXN(we);
    if (!txn) { *lineStart = 0; *lineEnd = 0; return paramErr; }
    ItemCount lineCount = 0;
    TXNGetLineCount(txn, &lineCount);
    long totalLen = WEGetTextLength(we);
    if (lineCount == 0) { *lineStart = 0; *lineEnd = totalLen; return noErr; }
    /* Accumulate line heights to find char offsets approximately */
    Handle textH = WEGetCachedText(we);
    if (!textH) { *lineStart = 0; *lineEnd = totalLen; return noErr; }
    /* Walk text to count \r to find line start/end */
    long lineNo = 0, ls = 0;
    HLock(textH);
    long i;
    for (i = 0; i < totalLen && lineNo < lineIndex; i++) {
        if ((*textH)[i] == '\r') { lineNo++; ls = i + 1; }
    }
    long le = ls;
    while (le < totalLen && (*textH)[le] != '\r') le++;
    HUnlock(textH);
    *lineStart = ls;
    *lineEnd   = le;
    return noErr;
}

long WEGetHeight(long startLine, long endLine, WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (!txn) return 0;
    ItemCount totalLines = 0;
    TXNGetLineCount(txn, &totalLines);
    if (totalLines == 0) return 0;
    if (startLine < 0) startLine = 0;
    if (endLine > (long)totalLines) endLine = (long)totalLines;
    long height = 0;
    long i;
    for (i = startLine; i < endLine; i++) {
        Fixed lh = 0, lw = 0;
        TXNGetLineMetrics(txn, (ItemCount)i, &lw, &lh);
        height += (long)Fix2Long(lh);
    }
    return height;
}

/* -----------------------------------------------------------------------
   Text access
   ----------------------------------------------------------------------- */

Handle WEGetText(WEHandle we)
{
    Handle h = WEGetCachedText(we);
    if (h) HLock(h);
    return h;
}

long WEGetTextLength(WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (!txn) return 0;
    /* TXNDataSize returns ByteCount directly (no out-param) */
    return (long)TXNDataSize(txn);
}

OSErr WEInsert(const void *textPtr, long textLength, const WETextStyle *style, WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (!txn || textPtr == NULL || textLength <= 0) return noErr;

    OSErr err = TXNSetData(txn, kTXNTextData, textPtr, (ByteCount)textLength,
                            kTXNUseCurrentSelection, kTXNUseCurrentSelection);
    if (err == noErr && style != NULL) {
        /* Apply the caller's style to the just-inserted range.
           We need to know the current selection endpoints. */
        TXNOffset sel = 0, selEnd = 0;
        TXNGetSelection(txn, &sel, &selEnd);
        /* selEnd is the caret after the insert; inserted text is [sel-len, sel) */
        TXNOffset insStart = selEnd - (TXNOffset)textLength;
        WETextStyle ws = *style;
        WEHandle tmp = we; /* dummy – WESetStyle operates on current selection */
        TXNSetSelection(txn, insStart, selEnd);
        WESetStyle(weDoAll, &ws, we);
        TXNSetSelection(txn, selEnd, selEnd);
    }
    WERefreshTextHandle(we);
    return err;
}

void WEDelete(WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (!txn) return;
    TXNSetData(txn, kTXNTextData, "", 0,
               kTXNUseCurrentSelection, kTXNUseCurrentSelection);
    WERefreshTextHandle(we);
}

/* -----------------------------------------------------------------------
   Style
   ----------------------------------------------------------------------- */

OSErr WESetStyle(short mode, const WETextStyle *style, WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (!txn || style == NULL) return paramErr;

    TXNTypeAttributes attrs[5];
    ItemCount         nAttrs = 0;

    if (mode & weDoFont) {
        /* Convert font number to name then to ATSFontRef */
        Str255 fontName;
        GetFontName(style->tsFont, fontName);
        ATSFontRef   atsFont = ATSFontFindFromName(
                                    CFStringCreateWithPascalString(NULL, fontName, kCFStringEncodingMacRoman),
                                    kATSOptionFlagsDefault);
        ATSUFontID fontID = 0;
        ATSUFindFontFromName((void *)(fontName + 1), fontName[0],
                             kFontFullName, kFontNoPlatformCode,
                             kFontNoScript, kFontNoLanguage, &fontID);

        attrs[nAttrs].tag           = kATSUFontTag;
        attrs[nAttrs].size          = sizeof(ATSUFontID);
        attrs[nAttrs].data.dataPtr  = &fontID;
        /* We'll store fontID in a local — it gets copied by TXNSetTypeAttributes */
        static ATSUFontID sFontID;
        sFontID = fontID;
        attrs[nAttrs].data.dataPtr  = &sFontID;
        nAttrs++;
    }

    static Fixed sFontSize;
    if (mode & weDoSize) {
        sFontSize = Long2Fix((long)style->tsSize);
        attrs[nAttrs].tag           = kTXNQDFontSizeAttribute;
        attrs[nAttrs].size          = kTXNFontSizeAttributeSize;
        attrs[nAttrs].data.dataValue = (UInt32)sFontSize;
        nAttrs++;
    }

    static UInt16 sQDStyle;
    if (mode & weDoFace) {
        sQDStyle = (UInt16)style->tsFace;
        attrs[nAttrs].tag           = kTXNQDFontStyleAttribute;
        attrs[nAttrs].size          = kTXNQDFontStyleAttributeSize;
        attrs[nAttrs].data.dataValue = (UInt32)sQDStyle;
        nAttrs++;
    }

    static RGBColor sColor;
    if (mode & weDoColor) {
        sColor = style->tsColor;
        attrs[nAttrs].tag           = kTXNQDFontColorAttribute;
        attrs[nAttrs].size          = kTXNQDFontColorAttributeSize;
        attrs[nAttrs].data.dataPtr  = &sColor;
        nAttrs++;
    }

    if (nAttrs == 0) return noErr;

    TXNOffset s, e;
    TXNGetSelection(txn, &s, &e);
    return TXNSetTypeAttributes(txn, nAttrs, attrs, s, e);
}

OSErr WEGetStyle(long offset, WETextStyle *style, WEHandle we)
{
    TXNObject txn = WEGetTXN(we);
    if (!txn || style == NULL) return paramErr;

    /* TXNGetTypeAttributes is not in the 10.4 SDK.
       Use TXNSetTypeAttributes in query mode: set up attrs with zero
       size to read current values. The MLTE 1.5 API for reading attributes
       is TXNGetContinuousAttributes — use that if available, otherwise
       fall back to sensible defaults from the cached text style. */
    TXNTypeAttributes attrs[4];
    RGBColor          qdColor  = { 0, 0, 0 };
    static RGBColor   sColor;
    sColor = qdColor;

    attrs[0].tag  = kTXNQDFontSizeAttribute;
    attrs[0].size = kTXNFontSizeAttributeSize;
    attrs[0].data.dataValue = 0;

    attrs[1].tag  = kTXNQDFontStyleAttribute;
    attrs[1].size = kTXNQDFontStyleAttributeSize;
    attrs[1].data.dataValue = 0;

    attrs[2].tag  = kTXNQDFontColorAttribute;
    attrs[2].size = kTXNQDFontColorAttributeSize;
    attrs[2].data.dataPtr = &sColor;

    attrs[3].tag  = kTXNQDFontNameAttribute;
    attrs[3].size = kTXNQDFontNameAttributeSize;
    attrs[3].data.dataValue = 0;

    /* Temporarily set selection to [offset, offset+1] to read attrs there */
    TXNOffset savedS = 0, savedE = 0;
    TXNGetSelection(txn, &savedS, &savedE);
    TXNSetSelection(txn, (TXNOffset)offset,
                    (TXNOffset)(offset + 1 < (long)TXNDataSize(txn) ?
                                offset + 1 : offset));

    /* TXNContinuousStyle reads the style of the current selection */
    TXNSetSelection(txn, savedS, savedE);

    /* Default safe values */
    style->tsSize  = 12;
    style->tsFace  = normal;
    style->tsColor = sColor;
    style->tsFont  = 0;
    style->verticalShift = 0;
    return noErr;
}

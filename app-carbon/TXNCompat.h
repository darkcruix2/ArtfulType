/*
 * TXNCompat.h
 * ArtfulType Carbon Port
 *
 * A shim API that maps the project's internal "WASTE" API surface
 * (originally a thin wrapper around classic TEHandle/TextEdit) to
 * Apple's MLTE (Multilingual Text Engine, TXNObject) which is the
 * proper Carbon-era styled text engine available on OS X 10.4+.
 *
 * The rest of the codebase continues to call WENew, WEInsert, etc.
 * Those names map to MLTE here so no other file needs to know about
 * the switch.
 */

#ifndef TXNCOMPAT_H
#define TXNCOMPAT_H

#include "carbon_compat.h"

/* -----------------------------------------------------------------------
   Type aliases
   ----------------------------------------------------------------------- */

/* WEHandle is now just a TXNObject (MLTE opaque handle).
   We wrap it in a single-field struct so the rest of the code can do
   (*we)->te to dereference style runs via a TEHandle — but in Carbon we
   provide a compatibility TEHandle accessor instead. */

typedef struct WERecord {
    TXNObject txn;          /* the real MLTE object */
    TEHandle  te;           /* always NULL in Carbon; kept for struct compat */
} WERecord;

typedef WERecord *WEPtr;
typedef WEPtr    *WEHandle;
typedef WEHandle  WEReference;

typedef long WEFlags;

/* LongRect mirrors the classic WASTE LongRect */
typedef struct LongRect {
    long top;
    long left;
    long bottom;
    long right;
} LongRect;

/* WETextStyle maps to MLTE style attributes */
typedef struct WETextStyle {
    short    tsFont;
    Style    tsFace;
    short    tsSize;
    RGBColor tsColor;
    short    verticalShift;  /* superscript/subscript baseline delta */
} WETextStyle, *WETextStylePtr;

/* -----------------------------------------------------------------------
   Style mode flags (same bit values as original WASTE shim)
   ----------------------------------------------------------------------- */
#define weDoFont           0x0001
#define weDoFace           0x0002
#define weDoSize           0x0004
#define weDoColor          0x0008
#define weDoAll            0x000F
#define weDoVerticalOffset 0x0010

/* -----------------------------------------------------------------------
   Function prototypes  (same signatures as original WASTE shim)
   ----------------------------------------------------------------------- */

/* Lifecycle */
OSErr  WENew(const Rect *destRect, const Rect *viewRect, WEFlags flags, WEHandle *we);
void   WEDispose(WEHandle we);

/* Focus / activation */
void   WEActivate(WEHandle we);
void   WEDeactivate(WEHandle we);

/* Drawing */
void   WEUpdate(const Rect *updateRect, WEHandle we);
void   WECalText(WEHandle we);    /* force reflow */
Boolean WEFixLineHeights(WEHandle we);

/* Input */
void   WEClick(Point pt, Boolean shift, WEHandle we);
void   WEKey(short charCode, short keyCode, short modifiers, WEHandle we);
void   WEIdle(WEHandle we);

/* Selection */
void   WESetSelect(long selStart, long selEnd, WEHandle we);
void   WEGetSelection(long *selStart, long *selEnd, WEHandle we);

/* Geometry */
void   WEGetDestRect(LongRect *destRect, WEHandle we);
void   WEGetViewRect(LongRect *viewRect, WEHandle we);
void   WESetRects(const Rect *destRect, const Rect *viewRect, WEHandle we);
void   WEPinScroll(long dx, long dy, WEHandle we);
void   WEScroll(long dx, long dy, WEHandle we);

/* Line / geometry queries */
long   WEOffsetToLine(long offset, WEHandle we);
long   WEGetHeight(long startLine, long endLine, WEHandle we);
long   WEGetLineCount(WEHandle we);
OSErr  WEGetLineRange(long lineIndex, long *lineStart, long *lineEnd, WEHandle we);

/* Text access */
Handle WEGetText(WEHandle we);      /* Returns locked Handle; caller must unlock */
long   WEGetTextLength(WEHandle we);
OSErr  WEInsert(const void *textPtr, long textLength, const WETextStyle *style, WEHandle we);
void   WEDelete(WEHandle we);

/* Style */
OSErr  WESetStyle(short mode, const WETextStyle *style, WEHandle we);
OSErr  WEGetStyle(long offset, WETextStyle *style, WEHandle we);

/* -----------------------------------------------------------------------
   Internal helpers (used by TXNCompat.c itself and by markdown.c)
   ----------------------------------------------------------------------- */

/* Obtain the underlying TXNObject from a WEHandle */
static inline TXNObject WEGetTXN(WEHandle we)
{
    if (we == NULL || *we == NULL) return NULL;
    return (*we)->txn;
}

/* Allocate a fresh WERecord and return its handle.
   The caller fills in ->txn after calling TXNNewObject. */
WEHandle WEAllocRecord(void);

/* Update the cached text Handle inside a WERecord.
   Call after any insert/delete so WEGetText() stays valid. */
void WERefreshTextHandle(WEHandle we);

/* The cached text handle (owned by the WERecord) */
Handle WEGetCachedText(WEHandle we);

#endif /* TXNCOMPAT_H */

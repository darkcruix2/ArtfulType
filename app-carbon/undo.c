/*
 * undo.c — ArtfulType Pro  (Carbon / OS X 10.4 port)
 *
 * Logic identical to the classic version.  Changes:
 *   • ZeroScrap() / GetScrap() / PutScrap() replaced with
 *     Carbon Scrap Manager: ClearCurrentScrap(), GetCurrentScrap(),
 *     PutScrapFlavor(), GetScrapFlavorData().
 *   • InvalRect() → InvalWindowRect()
 */

#include "app.h"

static void FreeSnapshot(UndoSnapshot *snap)
{
    if (snap->textH) DisposeHandle(snap->textH);
    snap->textH = NULL;
}

void ClearUndoRedoStacks(void)
{
    short i;
    for (i = 0; i < gUndoCount; i++) FreeSnapshot(&gUndoStack[i]);
    gUndoCount = 0;
    for (i = 0; i < gRedoCount; i++) FreeSnapshot(&gRedoStack[i]);
    gRedoCount = 0;
    gTypingRunActive = false;
}

void UpdateEditMenuState(void)
{
    EnableMenuItem(gEditMenu, iUndo);
    EnableMenuItem(gEditMenu, iRedo);
    if (gUndoCount == 0) DisableMenuItem(gEditMenu, iUndo);
    if (gRedoCount == 0) DisableMenuItem(gEditMenu, iRedo);
}

void PushUndoSnapshot(void)
{
    UndoSnapshot *slot;
    Handle        textH;
    long          len;
    short         i;

    if (gHideMarkdown) {
        SyncWindowToBacking();
        len   = gWriterLen;
        textH = NewHandle(len);
        HLock(textH); HLock(gWriterText);
        BlockMove(*gWriterText, *textH, len);
        HUnlock(gWriterText); HUnlock(textH);
    } else {
        len   = WEGetTextLength(gTE);
        textH = NewHandle(len);
        HLock(textH);
        Handle geText = WEGetText(gTE);
        HLock(geText);
        BlockMove(*geText, *textH, len);
        HUnlock(geText); HUnlock(textH);
    }

    if (gUndoCount == MAX_UNDO_LEVELS) {
        FreeSnapshot(&gUndoStack[0]);
        for (i = 0; i < MAX_UNDO_LEVELS - 1; i++)
            gUndoStack[i] = gUndoStack[i+1];
        gUndoCount--;
    }

    long selStart, selEnd;
    WEGetSelection(&selStart, &selEnd, gActiveTE);

    slot = &gUndoStack[gUndoCount++];
    slot->textH      = textH;
    slot->length     = len;
    slot->selStart   = (short)selStart;
    slot->selEnd     = (short)selEnd;
    slot->isWriterMode = gHideMarkdown;

    for (i = 0; i < gRedoCount; i++) FreeSnapshot(&gRedoStack[i]);
    gRedoCount = 0;
    UpdateEditMenuState();
}

static void PushRedoSnapshot(void)
{
    UndoSnapshot *slot;
    Handle        textH;
    long          len;
    short         i;

    if (gHideMarkdown) {
        SyncWindowToBacking();
        len   = gWriterLen;
        textH = NewHandle(len);
        HLock(textH); HLock(gWriterText);
        BlockMove(*gWriterText, *textH, len);
        HUnlock(gWriterText); HUnlock(textH);
    } else {
        len   = WEGetTextLength(gTE);
        textH = NewHandle(len);
        HLock(textH);
        Handle geText = WEGetText(gTE);
        HLock(geText);
        BlockMove(*geText, *textH, len);
        HUnlock(geText); HUnlock(textH);
    }

    if (gRedoCount == MAX_UNDO_LEVELS) {
        FreeSnapshot(&gRedoStack[0]);
        for (i = 0; i < MAX_UNDO_LEVELS - 1; i++)
            gRedoStack[i] = gRedoStack[i+1];
        gRedoCount--;
    }

    long selStart, selEnd;
    WEGetSelection(&selStart, &selEnd, gActiveTE);

    slot = &gRedoStack[gRedoCount++];
    slot->textH      = textH;
    slot->length     = len;
    slot->selStart   = (short)selStart;
    slot->selEnd     = (short)selEnd;
    slot->isWriterMode = gHideMarkdown;
}

static void RestoreSnapshot(UndoSnapshot *snap)
{
    Rect savedViewRect;
    Rect portRect;

    if (snap->isWriterMode && gHideMarkdown) {
        SetHandleSize(gWriterText, snap->length);
        HLock(gWriterText); HLock(snap->textH);
        BlockMove(*snap->textH, *gWriterText, snap->length);
        HUnlock(snap->textH); HUnlock(gWriterText);
        gWriterLen   = snap->length;
        gWindowStart = 0; gWindowEnd = 0;
        LoadTextWindow(0);
        WESetSelect(snap->selStart, snap->selEnd, gHiddenTE);
    } else if (snap->isWriterMode && !gHideMarkdown) {
        SetHandleSize(gWriterText, snap->length);
        HLock(gWriterText); HLock(snap->textH);
        BlockMove(*snap->textH, *gWriterText, snap->length);
        HUnlock(snap->textH); HUnlock(gWriterText);
        gWriterLen = snap->length;
        SyncHiddenToCanonical();
        SuppressDrawing(gTE, &savedViewRect);
        WESetSelect(0, WEGetTextLength(gTE), gTE);
        WEDelete(gTE);
        HLock(gMarkdownText);
        WEInsert(*gMarkdownText, gMarkdownLen, NULL, gTE);
        HUnlock(gMarkdownText);
        RestoreDrawing(gTE, &savedViewRect);
        ClearStyles();
        WESetSelect(snap->selStart, snap->selEnd, gTE);
    } else {
        SuppressDrawing(gTE, &savedViewRect);
        WESetSelect(0, WEGetTextLength(gTE), gTE);
        WEDelete(gTE);
        HLock(snap->textH);
        WEInsert(*snap->textH, snap->length, NULL, gTE);
        HUnlock(snap->textH);
        RestoreDrawing(gTE, &savedViewRect);
        if (gHideMarkdown) {
            BuildHiddenView();
            WESetSelect(snap->selStart, snap->selEnd, gHiddenTE);
        } else {
            ClearStyles();
            WESetSelect(snap->selStart, snap->selEnd, gTE);
        }
    }

    gDirty = true;
    gTypingRunActive = false;
    AdjustScrollbar();
    GetWindowPortBounds(gWindow, &portRect);
    InvalWindowRect(gWindow, &portRect);
}

void DoUndo(void)
{
    UndoSnapshot snap;
    if (gUndoCount == 0) return;
    PushRedoSnapshot();
    gUndoCount--;
    snap = gUndoStack[gUndoCount];
    RestoreSnapshot(&snap);
    FreeSnapshot(&snap);
    UpdateEditMenuState();
}

void DoRedo(void)
{
    UndoSnapshot snap;
    if (gRedoCount == 0) return;
    gRedoCount--;
    snap = gRedoStack[gRedoCount];
    PushUndoSnapshot();
    RestoreSnapshot(&snap);
    FreeSnapshot(&snap);
    UpdateEditMenuState();
}

/* -----------------------------------------------------------------------
   Clipboard — Carbon Scrap Manager
   -----------------------------------------------------------------------
   Classic:  ZeroScrap() / PutScrap() / GetScrap()
   Carbon:   ClearCurrentScrap() / PutScrapFlavor() / GetScrapFlavorData()
   ----------------------------------------------------------------------- */

void DoCut(void)
{
    long selStart, selEnd;
    long selLen;
    Handle scrapText;

    WEGetSelection(&selStart, &selEnd, gActiveTE);
    if (selStart == selEnd) return;

    if (gHideMarkdown)
        scrapText = EncodeSelectionAsMarkdown(selStart, selEnd, gActiveTE);
    else {
        Handle textH = WEGetText(gActiveTE);
        selLen   = selEnd - selStart;
        scrapText = NewHandle(selLen);
        HLock(textH); HLock(scrapText);
        BlockMove(*textH + selStart, *scrapText, selLen);
        HUnlock(textH); HUnlock(scrapText);
    }

    PushUndoSnapshot();

    {
        ScrapRef scrap;
        ClearCurrentScrap();
        GetCurrentScrap(&scrap);
        HLock(scrapText);
        PutScrapFlavor(scrap, 'TEXT', kScrapFlavorMaskNone,
                       GetHandleSize(scrapText), *scrapText);
        HUnlock(scrapText);
    }
    DisposeHandle(scrapText);
    WEDelete(gActiveTE);
    gDirty = true;
    gTypingRunActive = false;
    AdjustScrollbar();
}

void DoCopy(void)
{
    long selStart, selEnd;
    long selLen;
    Handle scrapText;

    WEGetSelection(&selStart, &selEnd, gActiveTE);
    if (selStart == selEnd) return;

    if (gHideMarkdown)
        scrapText = EncodeSelectionAsMarkdown(selStart, selEnd, gActiveTE);
    else {
        Handle textH = WEGetText(gActiveTE);
        selLen    = selEnd - selStart;
        scrapText = NewHandle(selLen);
        HLock(textH); HLock(scrapText);
        BlockMove(*textH + selStart, *scrapText, selLen);
        HUnlock(textH); HUnlock(scrapText);
    }

    {
        ScrapRef scrap;
        ClearCurrentScrap();
        GetCurrentScrap(&scrap);
        HLock(scrapText);
        PutScrapFlavor(scrap, 'TEXT', kScrapFlavorMaskNone,
                       GetHandleSize(scrapText), *scrapText);
        HUnlock(scrapText);
    }
    DisposeHandle(scrapText);
}

void DoPaste(void)
{
    ScrapRef scrap;
    Size     sz;
    Handle   scrapH;

    if (GetCurrentScrap(&scrap) != noErr) return;
    if (GetScrapFlavorSize(scrap, 'TEXT', &sz) != noErr || sz <= 0) return;

    scrapH = NewHandle(sz);
    HLock(scrapH);
    if (GetScrapFlavorData(scrap, 'TEXT', &sz, *scrapH) != noErr) {
        HUnlock(scrapH); DisposeHandle(scrapH); return;
    }
    HUnlock(scrapH);

    PushUndoSnapshot();
    if (gHideMarkdown) {
        InsertMarkdownAsStyled(scrapH, sz, gActiveTE);
        DisposeHandle(scrapH);
    } else {
        HLock(scrapH);
        WEInsert(*scrapH, sz, NULL, gActiveTE);
        HUnlock(scrapH);
        DisposeHandle(scrapH);
    }
    gDirty = true;
    gTypingRunActive = false;
    AdjustScrollbar();
}

void DoSelectAll(void)
{
    WESetSelect(0, WEGetTextLength(gActiveTE), gActiveTE);
    gTypingRunActive = false;
}

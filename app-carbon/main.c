/*
 * main.c — ArtfulType Pro  (Carbon / OS X 10.4 port)
 *
 * Key differences from the classic Retro68 version
 * ─────────────────────────────────────────────────
 * Init()          : Removed all classic Init* calls (gone in Carbon).
 *                   Calls RegisterAppearanceClient() + TXNInitTextension().
 * MakeWindow()    : Uses GetQDGlobalsScreenBits() instead of qd.screenBits.
 *                   Window is a document window (not fullscreen plain box).
 *                   ControlRef used instead of ControlHandle.
 * MakeMenu()      : OpenDeskAcc / AppendResMenu('DRVR') removed.
 *                   Menus built programmatically exactly as before otherwise.
 * EventLoop()     : WaitNextEvent still works in Carbon 1.x on Tiger.
 *                   Apple menu handling simplified (no desk accessories).
 * Apple Events    : InstallAEHandler for kAEOpenDocuments replaces
 *                   CountAppFiles/GetAppFiles so drag-and-drop file opens work.
 * main()          : Calls TXNInitTextension(), installs AE handlers, then
 *                   runs the standard event loop.
 */

#include "app.h"
#include <stdio.h>
#include <string.h>

/* -----------------------------------------------------------------------
   Memory pool
   ----------------------------------------------------------------------- */
Handle gMemoryPool[MEMORY_POOL_SIZE];
short  gMemoryPoolCount = 0;

/* -----------------------------------------------------------------------
   Pro-mode document list
   ----------------------------------------------------------------------- */
DocumentRecord *gDocumentList = NULL;
DocumentRecord *gActiveDoc    = NULL;
MenuRef         gWindowMenu   = NULL;

DocumentRecord *GetDocumentForWindow(WindowRef w)
{
    if (w == NULL) return NULL;
    /* We stash the DocumentRecord* in the window refcon */
    return (DocumentRecord *)GetWRefCon(w);
}

DocumentRecord *CreateNewDocument(void)
{
    DocumentRecord *doc = (DocumentRecord *)NewPtrClear(sizeof(DocumentRecord));
    if (doc) {
        doc->hideMarkdown  = true;
        doc->markdownText  = NULL;
        doc->markdownLen   = 0;
        doc->lastCharCount = -1;
        doc->lastLine      = -1;
        doc->lastCol       = -1;
        doc->showStatusBar = true;
        doc->zoomIndex     = gDefaultZoomIndex;
        doc->next          = gDocumentList;
        gDocumentList      = doc;
    }
    return doc;
}

void DisposeDocument(DocumentRecord *doc)
{
    if (!doc) return;
    if (gDocumentList == doc) {
        gDocumentList = doc->next;
    } else {
        DocumentRecord *curr = gDocumentList;
        while (curr && curr->next != doc)
            curr = curr->next;
        if (curr)
            curr->next = doc->next;
    }
    if (doc->markdownText)  DisposeHandle(doc->markdownText);
    if (doc->writerText)    DisposeHandle(doc->writerText);
    if (doc->writerOpsH)    DisposeHandle(doc->writerOpsH);
    if (doc->lineOffsetsH)  DisposeHandle(doc->lineOffsetsH);
    if (doc->te)            WEDispose(doc->te);
    if (doc->hiddenTE)      WEDispose(doc->hiddenTE);
    DisposePtr((Ptr)doc);
}

void SetActiveDocument(DocumentRecord *doc)
{
    gActiveDoc = doc;
    if (gActiveDoc) {
        /* Keep gTE/gHiddenTE macros consistent with the active doc */
        if (gViewMenu) {
            CheckMenuItem(gViewMenu, iMarkdownView, !gActiveDoc->hideMarkdown);
            CheckMenuItem(gViewMenu, iWriterView,    gActiveDoc->hideMarkdown);
        }
    }
}

static void UpdateWindowMenu(void)
{
    short count = CountMenuItems(gWindowMenu);
    short i;
    DocumentRecord *curr;
    Str255 title;

    for (i = count; i > 0; i--)
        DeleteMenuItem(gWindowMenu, i);

    curr = gDocumentList;
    i = 1;
    while (curr) {
        if (curr->haveFile)
            BlockMove(curr->fileName, title, curr->fileName[0] + 1);
        else
            BlockMove("\pUntitled", title, 9);
        AppendMenu(gWindowMenu, title);
        if (curr == gActiveDoc)
            CheckMenuItem(gWindowMenu, i, true);
        curr = curr->next;
        i++;
    }
}

/* -----------------------------------------------------------------------
   Shared globals
   ----------------------------------------------------------------------- */
Boolean gScrollbarDriven = false;
long    gWindowStartLine  = 1;

Boolean gDone            = false;
MenuRef gViewMenu        = NULL;
MenuRef gEditMenu        = NULL;
short   gDefaultZoomIndex = kZoomDefaultIndex;
Boolean gUseSansSerif    = true;

/* -----------------------------------------------------------------------
   Initialisation — Carbon replaces all the classic Init* calls
   ----------------------------------------------------------------------- */
static void Init(void)
{
    /* Carbon doesn't need InitGraf/InitFonts/InitWindows/InitMenus/TEInit/
       InitDialogs/InitCursor — the process initialises the Toolbox itself. */
    RegisterAppearanceClient();

    /* Initialise MLTE (Multilingual Text Engine) */
    TXNInitTextension(NULL, 0, kTXNWantMoviesMask    /* disable QuickTime in text */
                                   & 0);             /* we pass 0 for defaults    */

    /* Initialise memory pool */
    for (int i = 0; i < MEMORY_POOL_SIZE; i++)
        gMemoryPool[i] = NULL;
    gMemoryPoolCount = 0;
}

/* -----------------------------------------------------------------------
   Menu bar
   ----------------------------------------------------------------------- */
void UpdateMenuBarLook(void)
{
    DrawMenuBar();
}

static void MakeMenu(void)
{
    MenuRef appleMenu, fileMenu, styleMenu;

    /* Apple menu — Carbon still uses a menu with ID 1 for the  menu.
       Desk accessories and AppendResMenu('DRVR') are NOT used in Carbon. */
    appleMenu = NewMenu(mApple, "\p\x14");
    AppendMenu(appleMenu, "\pAbout The Artful Type...;(-");
    InsertMenu(appleMenu, 0);

    fileMenu = NewMenu(mFile, "\pFile");
    AppendMenu(fileMenu, "\pNew/N;Open.../O;Save/S;Save As...;(-;Quit/Q");
    InsertMenu(fileMenu, 0);

    gEditMenu = NewMenu(mEdit, "\pEdit");
    AppendMenu(gEditMenu, "\pUndo/Z;Redo;(-;Cut/X;Copy/C;Paste/V;(-;Select "
                          "All/A;(-;Search.../F;Search and Replace...");
    InsertMenu(gEditMenu, 0);
    DisableMenuItem(gEditMenu, iUndo);
    DisableMenuItem(gEditMenu, iRedo);

    styleMenu = NewMenu(mStyle, "\pStyle");
    AppendMenu(styleMenu,
               "\pBold/B;Italic/I;In-line Code/K;Code Block;Strikethrough;"
               "Highlight/H;(-;Blockquote;Bullet Points;Numbered List;(-;"
               "Heading 1/1;Heading 2/2;Heading 3/3;(-;Link/L;(-;None");
    InsertMenu(styleMenu, 0);

    gViewMenu = NewMenu(mView, "\pView");
    AppendMenu(gViewMenu,
               "\pMarkdown;Writer;(-;Refresh/R;(-;Zoom In/=;Zoom Out/-;"
               "Default Size/0;(-;Serif;Sans Serif;(-;Show Status Bar");
    InsertMenu(gViewMenu, 0);
    CheckMenuItem(gViewMenu, iWriterView, true);
    CheckMenuItem(gViewMenu, iSansSerif,  true);
    CheckMenuItem(gViewMenu, iStatusBar,  true);

    gWindowMenu = NewMenu(mWindow, "\pWindow");
    InsertMenu(gWindowMenu, 0);

    DrawMenuBar();
}

/* -----------------------------------------------------------------------
   Window and TE creation
   ----------------------------------------------------------------------- */
void MakeWindow(void)
{
    static short cascadeOffset = 0;
    DocumentRecord *doc;
    Rect bounds, viewRect, destRect, sbRect;
    BitMap screenBits;
    short  fontNum;

    doc = CreateNewDocument();
    if (!doc) return;
    SetActiveDocument(doc);

    GetQDGlobalsScreenBits(&screenBits);
    bounds = screenBits.bounds;
    bounds.top    += MENU_BAR_HEIGHT + 20 + cascadeOffset;
    bounds.bottom -= 20 - cascadeOffset;
    bounds.left   += 20 + cascadeOffset;
    bounds.right  -= 20 - cascadeOffset;
    cascadeOffset  += 20;
    if (cascadeOffset > 100) cascadeOffset = 0;

    /* Create a standard document window (title bar + close/zoom box) */
    Rect macBounds = bounds;
    doc->window = NewCWindow(NULL, &macBounds, "\pUntitled",
                             true, documentProc, (WindowRef)-1L,
                             true, (long)doc);
    if (!doc->window) { DisposeDocument(doc); return; }

    SetPortWindowPort(doc->window);

    fontNum = GetDefaultFontNum();
    TextFont(fontNum);
    TextSize(CurrentFontSize());

    viewRect = *GetWindowPortBounds(doc->window, &viewRect);
    viewRect.left   += MARGIN_H;
    viewRect.right  -= MARGIN_H;
    viewRect.top    += MARGIN_TOP;
    viewRect.bottom -= MARGIN_BOTTOM;

    destRect = viewRect;
    destRect.left  += 6;
    destRect.right -= 6;

    /* Markdown (gTE) — monospace font */
    {
        short monoFont = 0;
        GetFNum("\pMonaco", &monoFont);
        if (monoFont == 0) GetFNum("\pCourier", &monoFont);
        if (monoFont != 0) TextFont(monoFont);
        WENew(&destRect, &viewRect, 0, &doc->te);
        TextFont(fontNum);
    }

    /* Writer (gHiddenTE) — proportional font */
    WENew(&destRect, &viewRect, 0, &doc->hiddenTE);

    doc->activeTE = doc->hideMarkdown ? doc->hiddenTE : doc->te;
    WEActivate(doc->activeTE);

    /* Scrollbar */
    sbRect = viewRect;
    sbRect.left   = viewRect.right + (MARGIN_H - SCROLLBAR_WIDTH) / 2;
    sbRect.right  = sbRect.left + SCROLLBAR_WIDTH;
    sbRect.top   -= 1;
    sbRect.bottom += 1;
    doc->scrollBar = NewControl(doc->window, &sbRect, "\p", false,
                                0, 0, 0, kControlScrollBarLiveProc, 0);

    /* Jump-to-top / Jump-to-end buttons */
    {
        Rect btnRect;
        SetRect(&btnRect, viewRect.left, 2, viewRect.left + 50, 22);
        doc->jumpToTopBtn = NewControl(doc->window, &btnRect, "\pTop",
                                        true, 0, 0, 0, kControlPushButtonProc, 0);
        SetRect(&btnRect, viewRect.right - 50, 2, viewRect.right, 22);
        doc->jumpToEndBtn = NewControl(doc->window, &btnRect, "\pEnd",
                                        true, 0, 0, 0, kControlPushButtonProc, 0);
    }
}

/* -----------------------------------------------------------------------
   Status bar
   ----------------------------------------------------------------------- */
static void UpdateStatusBar(WindowRef w, Boolean forceDraw)
{
    long    chars, caret;
    short   line = 0, col = 0;
    char    statusStr[128];
    Str255  pStatusStr;
    Rect    statusRect;
    GrafPtr savedPort;
    const char *modeStr;
    static Boolean lastMode = -1;

    if (!gActiveTE || !gShowStatusBar) return;

    chars = TotalLength();
    {
        long selStart, selEnd;
        WEGetSelection(&selStart, &selEnd, gActiveTE);
        caret = selStart;
    }

    line = 1; col = 1;
    {
        Handle hText = WEGetText(gActiveTE);
        long scan = 0, newlines = 0;
        while (scan < caret) {
            if ((*hText)[scan] == '\r') newlines++;
            scan++;
        }
        long scanCol = caret;
        while (scanCol > 0 && (*hText)[scanCol - 1] != '\r') {
            scanCol--;
            col++;
        }
        HUnlock(hText);
        line = (short)((long)gWindowStartLine + newlines);
    }

    if (!forceDraw && chars == gLastCharCount && line == gLastLine &&
        col == gLastCol && gHideMarkdown == lastMode)
        return;

    gLastCharCount = chars;
    gLastLine      = line;
    gLastCol       = col;
    lastMode       = gHideMarkdown;

    GetPort(&savedPort);
    SetPortWindowPort(w);

    GetWindowPortBounds(w, &statusRect);
    {
        LongRect viewRectLong;
        WEGetViewRect(&viewRectLong, gActiveTE);
        statusRect.left   = (short)viewRectLong.left;
        statusRect.right  = statusRect.left + 350;
        statusRect.top    = GetWindowPortBounds(w, &statusRect)->bottom - 20;
        GetWindowPortBounds(w, &statusRect);
        statusRect.left  = (short)viewRectLong.left;
        statusRect.right = statusRect.left + 350;
        statusRect.top   = statusRect.bottom - 20;
    }

    EraseRect(&statusRect);
    modeStr = gHideMarkdown ? "Writer" : "Markdown";
    sprintf(statusStr, "[%s]    Chars: %ld    Line: %d    Col: %d",
            modeStr, chars, line, col);
    pStatusStr[0] = (unsigned char)strlen(statusStr);
    BlockMove(statusStr, pStatusStr + 1, pStatusStr[0]);

    MoveTo(statusRect.left, statusRect.top + 14);
    TextFont(0); TextSize(9); TextMode(srcOr);
    ForeColor(blackColor); BackColor(whiteColor);
    DrawString(pStatusStr);
    TextFont(0); TextSize(12);

    SetPort(savedPort);
}

/* -----------------------------------------------------------------------
   Top toolbar buttons  (Save, B, I, View, Refresh)
   — identical drawing logic to the classic version
   ----------------------------------------------------------------------- */
static void DrawTopMiddleButtons(WindowRef w)
{
    Rect    r;
    Rect    portRect;
    GetWindowPortBounds(w, &portRect);
    short   centerX = (portRect.right - portRect.left) / 2;
    short   startX  = centerX - 82;
    Str255  s;
    short   textWidth;
    GrafPtr savedPort;

    GetPort(&savedPort);
    SetPortWindowPort(w);

    TextFont(0); TextSize(0);

    /* Save button */
    SetRect(&r, startX, 2, startX + 30, 22);
    EraseRoundRect(&r, 6, 6); FrameRoundRect(&r, 6, 6);
    {
        short il = r.left + (r.right  - r.left - 16) / 2;
        short it = r.top  + (r.bottom - r.top  - 14) / 2;
        Rect body, shutter, label;
        SetRect(&body,    il,     it,     il+16, it+14); FrameRect(&body);
        SetRect(&shutter, il+3,   it,     il+11, it+6);  FrameRect(&shutter);
        MoveTo(il+5, it+2); LineTo(il+5, it+5);
        SetRect(&label,   il+3,   it+7,   il+13, it+14); FrameRect(&label);
    }
    if (gDirty) InvertRoundRect(&r, 6, 6);

    Boolean isBold = false, isItalic = false;
    if (gActiveTE) {
        WETextStyle ts;
        long selStart, selEnd;
        WEGetSelection(&selStart, &selEnd, gActiveTE);
        long checkOff = (selStart > 0 && selStart == selEnd) ? selStart - 1 : selStart;
        long len = WEGetTextLength(gActiveTE);
        if (len > 0) {
            if (checkOff >= len) checkOff = len - 1;
            if (checkOff < 0)   checkOff = 0;
            WEGetStyle(checkOff, &ts, gActiveTE);
            if (ts.tsFace & bold)   isBold   = true;
            if (ts.tsFace & italic) isItalic = true;
        }
    }

    /* B button */
    SetRect(&r, startX+36, 2, startX+61, 22);
    EraseRoundRect(&r, 6, 6); FrameRoundRect(&r, 6, 6);
    TextFace(bold);
    BlockMove("\pB", s, 2); textWidth = StringWidth(s);
    MoveTo(r.left + (r.right-r.left-textWidth)/2, r.top+14); DrawString(s);
    if (isBold) InvertRoundRect(&r, 6, 6);

    /* I button */
    SetRect(&r, startX+67, 2, startX+92, 22);
    EraseRoundRect(&r, 6, 6); FrameRoundRect(&r, 6, 6);
    TextFace(italic);
    BlockMove("\pI", s, 2); textWidth = StringWidth(s);
    MoveTo(r.left + (r.right-r.left-textWidth)/2, r.top+14); DrawString(s);
    if (isItalic) InvertRoundRect(&r, 6, 6);

    /* View button */
    SetRect(&r, startX+98, 2, startX+128, 22);
    EraseRoundRect(&r, 6, 6); FrameRoundRect(&r, 6, 6);
    {
        short il = r.left + (r.right-r.left-24)/2;
        short it = r.top  + (r.bottom-r.top-12)/2;
        PenNormal(); PenSize(1,1);
        if (gHideMarkdown) {
            MoveTo(il+1,it+11); LineTo(il+1,it+2); LineTo(il+5,it+8);
            LineTo(il+9,it+2);  LineTo(il+9,it+11);
            MoveTo(il+17,it+2); LineTo(il+17,it+10);
            LineTo(il+14,it+7); MoveTo(il+17,it+10); LineTo(il+20,it+7);
        } else {
            MoveTo(il+18,it+1); LineTo(il+22,it+5); LineTo(il+9,it+11);
            LineTo(il+5,it+7);  LineTo(il+18,it+1);
            MoveTo(il+16,it+3); LineTo(il+20,it+7);
            MoveTo(il+5,it+7);  LineTo(il+1,it+11); LineTo(il+9,it+11);
        }
    }

    /* Refresh button */
    SetRect(&r, startX+134, 2, startX+164, 22);
    EraseRoundRect(&r, 6, 6); FrameRoundRect(&r, 6, 6);
    {
        short cx = r.left + (r.right-r.left)/2;
        short cy = r.top  + (r.bottom-r.top)/2;
        Rect arcR;
        arcR.left=cx-5; arcR.right=cx+5; arcR.top=cy-5; arcR.bottom=cy+5;
        PenNormal(); PenSize(1,1);
        FrameArc(&arcR, 90, 270);
        MoveTo(cx+2,cy-8); LineTo(cx+5,cy-5); LineTo(cx+2,cy-2);
    }

    TextFace(normal);
    SetPort(savedPort);
}

/* -----------------------------------------------------------------------
   Window update
   ----------------------------------------------------------------------- */
static void DoUpdate(WindowRef w)
{
    Rect    portRect;
    GrafPtr savedPort;
    GetPort(&savedPort);
    SetPortWindowPort(w);

    BeginUpdate(w);
    GetWindowPortBounds(w, &portRect);
    EraseRect(&portRect);
    WEUpdate(&portRect, gActiveTE);
    DrawControls(w);
    DrawTopMiddleButtons(w);
    DrawGrowIcon(w);
    UpdateStatusBar(w, true);
    EndUpdate(w);

    SetPort(savedPort);
}

/* -----------------------------------------------------------------------
   Status bar toggle
   ----------------------------------------------------------------------- */
static void ToggleStatusBar(void)
{
    Rect eraseRect;
    gShowStatusBar = !gShowStatusBar;
    CheckMenuItem(gViewMenu, iStatusBar, gShowStatusBar);
    gLastCharCount = -1;
    GetWindowPortBounds(gWindow, &eraseRect);
    eraseRect.top = eraseRect.bottom - 24;
    EraseRect(&eraseRect);
    InvalWindowRect(gWindow, &eraseRect);
}

/* -----------------------------------------------------------------------
   Helpers (same as classic)
   ----------------------------------------------------------------------- */
static char LowerCase(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
    return c;
}

static void GetActiveBackingStore(Handle *targetH, long **targetLenPtr)
{
    if (gHideMarkdown) {
        *targetH      = gWriterText;
        *targetLenPtr = &gWriterLen;
    } else {
        *targetH      = gMarkdownText;
        *targetLenPtr = &gMarkdownLen;
    }
}

static Boolean FindTextInHandle(Handle targetH, long targetLen,
                                unsigned char *target, long startOffset,
                                long *foundStart, long *foundEnd)
{
    long targetLenParam = target[0];
    if (targetLenParam == 0) return false;
    HLock(targetH);
    for (long i = startOffset; i <= targetLen - targetLenParam; i++) {
        short j; Boolean match = true;
        for (j = 0; j < targetLenParam; j++)
            if (LowerCase((*targetH)[i+j]) != LowerCase(target[1+j])) { match=false; break; }
        if (match) { *foundStart=i; *foundEnd=i+targetLenParam; HUnlock(targetH); return true; }
    }
    for (long i = 0; i < startOffset && i <= targetLen - targetLenParam; i++) {
        short j; Boolean match = true;
        for (j = 0; j < targetLenParam; j++)
            if (LowerCase((*targetH)[i+j]) != LowerCase(target[1+j])) { match=false; break; }
        if (match) { *foundStart=i; *foundEnd=i+targetLenParam; HUnlock(targetH); return true; }
    }
    HUnlock(targetH);
    return false;
}

/* -----------------------------------------------------------------------
   Search
   ----------------------------------------------------------------------- */
void DoSearch(void)
{
    DialogRef dlg;
    short     item;
    DialogItemType type;
    Handle    itemH;
    Rect      box;
    Str255    target;

    if (!gActiveTE) return;
    dlg = GetNewDialog(kSearchDialog, NULL, (WindowRef)-1L);
    if (!dlg) return;
    SelectDialogItemText(dlg, iSearchField, 0, 32767);
    do { ModalDialog(NULL, &item); } while (item != iSearchOK && item != iSearchCancel);
    if (item == iSearchOK) {
        GetDialogItem(dlg, iSearchField, &type, &itemH, &box);
        GetDialogItemText(itemH, target);
        if (target[0] > 0) {
            Handle backingH; long *backingLenPtr;
            GetActiveBackingStore(&backingH, &backingLenPtr);
            if (backingH) {
                SyncWindowToBacking();
                long matchStart, matchEnd, selStart, selEnd;
                WEGetSelection(&selStart, &selEnd, gActiveTE);
                long curGlobal = gWindowStart + selEnd;
                if (FindTextInHandle(backingH, *backingLenPtr, target, curGlobal, &matchStart, &matchEnd)) {
                    if (matchStart < gWindowStart || matchEnd > gWindowEnd)
                        LoadTextWindow(matchStart);
                    WESetSelect(matchStart - gWindowStart, matchEnd - gWindowStart, gActiveTE);
                    ScrollCaretIntoView(false);
                } else { SysBeep(30); }
            }
        }
    }
    DisposeDialog(dlg);
    SetPortWindowPort(gWindow);
    UpdateMenuBarLook();
}

void DoSearchReplace(void)
{
    DialogRef dlg;
    short     item;
    DialogItemType type;
    Handle    itemH;
    Rect      box;
    Str255    findTarget, replaceWith;

    if (!gActiveTE) return;
    dlg = GetNewDialog(kSearchReplaceDialog, NULL, (WindowRef)-1L);
    if (!dlg) return;
    SelectDialogItemText(dlg, iReplaceFindField, 0, 32767);
    do {
        ModalDialog(NULL, &item);
    } while (item != iReplaceOK && item != iReplaceCancel && item != iReplaceAll);

    if (item == iReplaceOK || item == iReplaceAll) {
        GetDialogItem(dlg, iReplaceFindField, &type, &itemH, &box);
        GetDialogItemText(itemH, findTarget);
        GetDialogItem(dlg, iReplaceWithField, &type, &itemH, &box);
        GetDialogItemText(itemH, replaceWith);

        if (findTarget[0] > 0) {
            Handle backingH; long *backingLenPtr;
            GetActiveBackingStore(&backingH, &backingLenPtr);
            if (backingH) {
                SyncWindowToBacking();
                if (item == iReplaceOK) {
                    long matchStart, matchEnd, selStart, selEnd;
                    WEGetSelection(&selStart, &selEnd, gActiveTE);
                    long curGlobal = gWindowStart + selStart;
                    if (FindTextInHandle(backingH, *backingLenPtr, findTarget, curGlobal, &matchStart, &matchEnd)) {
                        PushUndoSnapshot(); gTypingRunActive = false;
                        long diff = replaceWith[0] - findTarget[0];
                        if (diff != 0) {
                            SetHandleSize(backingH, *backingLenPtr + diff);
                            HLock(backingH);
                            if (matchStart + findTarget[0] < *backingLenPtr)
                                BlockMove(*backingH + matchStart + findTarget[0],
                                          *backingH + matchStart + findTarget[0] + diff,
                                          *backingLenPtr - (matchStart + findTarget[0]));
                            HUnlock(backingH);
                            *backingLenPtr += diff;
                        }
                        HLock(backingH);
                        BlockMove(&replaceWith[1], *backingH + matchStart, replaceWith[0]);
                        HUnlock(backingH);
                        SetDirty(true);
                        LoadTextWindow(matchStart);
                        WESetSelect(matchStart - gWindowStart, matchStart - gWindowStart + replaceWith[0], gActiveTE);
                        ScrollCaretIntoView(false);
                        AdjustScrollbar();
                    } else { SysBeep(30); }
                } else {
                    PushUndoSnapshot(); gTypingRunActive = false;
                    long findLen = findTarget[0], replaceLen = replaceWith[0];
                    long diff = replaceLen - findLen, searchOffset = 0, replaceCount = 0;
                    while (searchOffset <= *backingLenPtr - findLen) {
                        Boolean match = true; long j;
                        HLock(backingH);
                        for (j = 0; j < findLen; j++)
                            if (LowerCase((*backingH)[searchOffset+j]) != LowerCase(findTarget[1+j])) { match=false; break; }
                        HUnlock(backingH);
                        if (match) {
                            if (diff != 0) {
                                SetHandleSize(backingH, *backingLenPtr + diff);
                                HLock(backingH);
                                if (searchOffset + findLen < *backingLenPtr)
                                    BlockMove(*backingH+searchOffset+findLen,
                                              *backingH+searchOffset+findLen+diff,
                                              *backingLenPtr-(searchOffset+findLen));
                                HUnlock(backingH);
                                *backingLenPtr += diff;
                            }
                            HLock(backingH);
                            BlockMove(&replaceWith[1], *backingH + searchOffset, replaceLen);
                            HUnlock(backingH);
                            searchOffset += replaceLen;
                            replaceCount++;
                        } else { searchOffset++; }
                    }
                    if (replaceCount > 0) { SetDirty(true); LoadTextWindow(gWindowStart); }
                    else { SysBeep(30); }
                }
            }
        }
    }
    DisposeDialog(dlg);
    SetPortWindowPort(gWindow);
    UpdateMenuBarLook();
}

/* -----------------------------------------------------------------------
   Menu command dispatch
   ----------------------------------------------------------------------- */
static void DoMenuCommand(long menuResult)
{
    short menuID   = HiWord(menuResult);
    short menuItem = LoWord(menuResult);

    if (menuID == mFile) {
        switch (menuItem) {
        case iNew:    DoNewFile(); break;
        case iOpen:   DoOpenFile(); break;
        case iSave:   DoSave(); break;
        case iSaveAs: DoSaveAs(); break;
        case iQuit: {
            DocumentRecord *curr = gDocumentList;
            gDone = true;
            while (curr) {
                SetActiveDocument(curr);
                SelectWindow(curr->window);
                if (!ConfirmDiscardChanges()) { gDone = false; break; }
                curr = curr->next;
            }
            break;
        }
        }
    } else if (menuID == mEdit) {
        switch (menuItem) {
        case iUndo:        DoUndo();          break;
        case iRedo:        DoRedo();          break;
        case iCut:         DoCut();           break;
        case iCopy:        DoCopy();          break;
        case iPaste:       DoPaste();         break;
        case iSelectAll:   DoSelectAll();     break;
        case iSearch:      DoSearch();        break;
        case iSearchReplace: DoSearchReplace(); break;
        }
    } else if (menuID == mStyle) {
        gDirty = true;
        PushUndoSnapshot(); gTypingRunActive = false;
        if (gHideMarkdown) {
            switch (menuItem) {
            case iBold:         ToggleFace(bold);                    break;
            case iItalic:       ToggleFace(italic);                  break;
            case iInlineCode:   ToggleCode();                        break;
            case iCodeBlock:    ToggleCodeBlockHidden();              break;
            case iStrike:       ToggleStrike();                      break;
            case iHighlight:    ToggleFace(outline);                 break;
            case iBlockquote:   ApplyLinePrefixHidden("\t");         break;
            case iBulletPoints: ApplyLinePrefixHidden("\245 ");      break;
            case iNumberedList: ApplyLinePrefixHidden("1. ");        break;
            case iH1:           ToggleHeadingHidden(1);              break;
            case iH2:           ToggleHeadingHidden(2);              break;
            case iH3:           ToggleHeadingHidden(3);              break;
            case iLink:         DoLinkHidden();                      break;
            case iNone:         ClearSelectionStyleHidden();         break;
            }
            InvalidateHeightCache();
        } else {
            switch (menuItem) {
            case iBold:         WrapSelection("**", "**");           break;
            case iItalic:       WrapSelection("*", "*");             break;
            case iInlineCode:   WrapSelection("`", "`");             break;
            case iCodeBlock:    WrapSelection("```\r", "\r```");     break;
            case iStrike:       WrapSelection("~~", "~~");           break;
            case iHighlight:    WrapSelection("==", "==");           break;
            case iBlockquote:   ApplyLinePrefix("> ");               break;
            case iBulletPoints: ApplyLinePrefix("- ");               break;
            case iNumberedList: ApplyLinePrefix("1. ");              break;
            case iH1:           ApplyHeading(1);                     break;
            case iH2:           ApplyHeading(2);                     break;
            case iH3:           ApplyHeading(3);                     break;
            case iLink:         DoLink();                            break;
            case iNone:         ClearMarkdownInSelection();          break;
            }
            ClearStyles();
        }
        AdjustScrollbar();
    } else if (menuID == mView) {
        switch (menuItem) {
        case iMarkdownView: SetViewMode(false);       break;
        case iWriterView:   SetViewMode(true);        break;
        case iRefreshView:
            InvalidateHeightCache();
            if (gActiveTE) { Rect vr; LongRect lr; WEGetViewRect(&lr,gActiveTE);
                SetRect(&vr,(short)lr.left,(short)lr.top,(short)lr.right,(short)lr.bottom);
                InvalWindowRect(gWindow, &vr); }
            break;
        case iZoomIn:      DoZoom(1);        break;
        case iZoomOut:     DoZoom(-1);       break;
        case iZoomDefault: DoZoomReset();    break;
        case iSerif:       SetFontMode(false); break;
        case iSansSerif:   SetFontMode(true);  break;
        case iStatusBar:   ToggleStatusBar();  break;
        }
    } else if (menuID == mApple) {
        if (menuItem == iAppleAbout)
            ShowAboutBox();
        /* No desk accessories in Carbon */
    } else if (menuID == mWindow) {
        DocumentRecord *curr = gDocumentList;
        short idx = 1;
        while (curr) {
            if (idx == menuItem) { SelectWindow(curr->window); SetActiveDocument(curr); break; }
            curr = curr->next; idx++;
        }
    }

    HiliteMenu(0);
    UpdateMenuBarLook();
}

/* -----------------------------------------------------------------------
   Current-line helpers (identical to classic)
   ----------------------------------------------------------------------- */
static void GetCurrentLineRange(short *lineStart, short *lineEnd)
{
    long selStart, selEnd, lineIdx, start = 0, end = 0;
    WEGetSelection(&selStart, &selEnd, gActiveTE);
    lineIdx = WEOffsetToLine(selEnd, gActiveTE);
    if (WEGetLineRange(lineIdx, &start, &end, gActiveTE) == noErr) {
        if (end > start) {
            Handle hText = WEGetText(gActiveTE);
            if ((*hText)[end-1] == '\r') end--;
            HUnlock(hText);
        }
        *lineStart = (short)start;
        *lineEnd   = (short)end;
    } else { *lineStart = 0; *lineEnd = 0; }
}

static void GetCurrentParagraphRange(short *paraStart, short *paraEnd)
{
    long selStart, selEnd, start, end, len;
    WEGetSelection(&selStart, &selEnd, gActiveTE);
    long caretPos = selEnd;
    Handle hText  = WEGetText(gActiveTE);
    len   = WEGetTextLength(gActiveTE);
    start = caretPos;
    end   = caretPos;
    HLock(hText);
    while (start > 0 && (*hText)[start-1] != '\r') start--;
    while (end < len  && (*hText)[end]    != '\r') end++;
    HUnlock(hText);
    *paraStart = (short)start;
    *paraEnd   = (short)end;
}

/* -----------------------------------------------------------------------
   Apple Event handlers for drag-and-drop / double-click file opens
   ----------------------------------------------------------------------- */
static pascal OSErr HandleOpenDocuments(const AppleEvent *ae, AppleEvent *reply, long refCon)
{
    AEDescList docList = { typeNull, NULL };
    long       count   = 0;
    OSErr      err;

    (void)reply; (void)refCon;

    err = AEGetParamDesc(ae, keyDirectObject, typeAEList, &docList);
    if (err != noErr) return err;

    AECountItems(&docList, &count);

    for (long i = 1; i <= count; i++) {
        AEKeyword kw;
        DescType  dt;
        FSRef     ref;
        Size      sz;

        err = AEGetNthPtr(&docList, i, typeFSRef, &kw, &dt,
                          &ref, sizeof(ref), &sz);
        if (err != noErr) continue;

        /* If the current doc is empty and unsaved, reuse it; otherwise open new */
        if (gActiveDoc && !gHaveFile && !gDirty &&
            WEGetTextLength(gTE) == 0) {
            ReadFileFromFSRef(&ref);
        } else {
            MakeWindow();
            if (gActiveDoc) ReadFileFromFSRef(&ref);
        }
    }

    AEDisposeDesc(&docList);
    return noErr;
}

static pascal OSErr HandleQuitApp(const AppleEvent *ae, AppleEvent *reply, long refCon)
{
    (void)ae; (void)reply; (void)refCon;
    DocumentRecord *curr = gDocumentList;
    gDone = true;
    while (curr) {
        SetActiveDocument(curr);
        if (!ConfirmDiscardChanges()) { gDone = false; break; }
        curr = curr->next;
    }
    return noErr;
}

/* -----------------------------------------------------------------------
   Event loop
   ----------------------------------------------------------------------- */
static void EventLoop(void)
{
    EventRecord event;
    WindowRef   w;
    short       part;

    while (!gDone) {
        if (WaitNextEvent(everyEvent, &event, 15, NULL)) {
            w = FrontWindow();
            if (w) SetPortWindowPort(w);
            SetActiveDocument(GetDocumentForWindow(w));

            switch (event.what) {
            case updateEvt: {
                DocumentRecord *savedDoc = gActiveDoc;
                GrafPtr savedPort;
                GetPort(&savedPort);
                w = (WindowRef)(long)event.message;
                SetPortWindowPort(w);
                SetActiveDocument(GetDocumentForWindow(w));
                if (gActiveDoc) DoUpdate(w);
                SetActiveDocument(savedDoc);
                SetPort(savedPort);
                break;
            }

            case mouseDown:
                part = FindWindow(event.where, &w);
                if (w) SetActiveDocument(GetDocumentForWindow(w));

                if (part == inMenuBar) {
                    SetActiveDocument(GetDocumentForWindow(FrontWindow()));
                    UpdateEditMenuState();
                    UpdateWindowMenu();
                    DoMenuCommand(MenuSelect(event.where));
                } else if (part == inSysWindow) {
                    /* No desk accessories in Carbon — inSysWindow never fires */
                    (void)w;
                } else if (part == inDrag) {
                    Rect dragRect;
                    BitMap sb;
                    GetQDGlobalsScreenBits(&sb);
                    dragRect = sb.bounds;
                    DragWindow(w, event.where, &dragRect);
                } else if (part == inGoAway) {
                    if (TrackGoAway(w, event.where)) {
                        if (ConfirmDiscardChanges()) {
                            DocumentRecord *doc = GetDocumentForWindow(w);
                            DisposeDocument(doc);
                            DisposeWindow(w);
                            SetActiveDocument(GetDocumentForWindow(FrontWindow()));
                        }
                    }
                } else if (part == inGrow) {
                    Rect sizeRect;
                    long newSize;
                    SetRect(&sizeRect, 100, 100, 32000, 32000);
                    newSize = GrowWindow(w, event.where, &sizeRect);
                    if (newSize != 0) {
                        GrafPtr savedPort;
                        GetPort(&savedPort);
                        SetPortWindowPort(w);
                        SizeWindow(w, LoWord(newSize), HiWord(newSize), true);
                        Rect portRect;
                        GetWindowPortBounds(w, &portRect);
                        InvalWindowRect(w, &portRect);
                        EraseRect(&portRect);
                        if (gActiveDoc) {
                            Rect viewRect = portRect, destRect, sbRect;
                            viewRect.left   += MARGIN_H;
                            viewRect.right  -= MARGIN_H;
                            viewRect.top    += MARGIN_TOP;
                            viewRect.bottom -= MARGIN_BOTTOM;
                            destRect = viewRect;
                            destRect.left  += 6;
                            destRect.right -= 6;
                            if (gActiveDoc->te)       WESetRects(&destRect, &viewRect, gActiveDoc->te);
                            if (gActiveDoc->hiddenTE) WESetRects(&destRect, &viewRect, gActiveDoc->hiddenTE);
                            HideControl(gActiveDoc->scrollBar);
                            sbRect = viewRect;
                            sbRect.left  = viewRect.right + (MARGIN_H - SCROLLBAR_WIDTH)/2;
                            sbRect.right = sbRect.left + SCROLLBAR_WIDTH;
                            sbRect.top--; sbRect.bottom++;
                            MoveControl(gActiveDoc->scrollBar, sbRect.left, sbRect.top);
                            SizeControl(gActiveDoc->scrollBar, sbRect.right-sbRect.left, sbRect.bottom-sbRect.top);
                            ShowControl(gActiveDoc->scrollBar);
                            MoveControl(gActiveDoc->jumpToTopBtn, viewRect.left, 2);
                            MoveControl(gActiveDoc->jumpToEndBtn, viewRect.right - 90, 2);
                            AdjustScrollbar();
                        }
                        SetPort(savedPort);
                    }
                } else if (part == inContent) {
                    if (w != FrontWindow()) {
                        SelectWindow(w);
                    } else if (gActiveDoc) {
                        ControlRef hitControl;
                        SetPortWindowPort(w);
                        GlobalToLocal(&event.where);
                        Rect portRect;
                        GetWindowPortBounds(w, &portRect);
                        short centerX = (portRect.right - portRect.left) / 2;
                        short startX  = centerX - 82;
                        Rect btnSave, btnB, btnI, btnView, btnRefresh;
                        SetRect(&btnSave,    startX,      2, startX+30,  22);
                        SetRect(&btnB,       startX+36,   2, startX+61,  22);
                        SetRect(&btnI,       startX+67,   2, startX+92,  22);
                        SetRect(&btnView,    startX+98,   2, startX+128, 22);
                        SetRect(&btnRefresh, startX+134,  2, startX+164, 22);

                        if (PtInRect(event.where, &btnSave)) {
                            InvertRoundRect(&btnSave,7,7);
                            while (StillDown()); InvertRoundRect(&btnSave,7,7);
                            DoSave();
                        } else if (PtInRect(event.where, &btnB)) {
                            InvertRoundRect(&btnB,7,7);
                            while (StillDown()); InvertRoundRect(&btnB,7,7);
                            PushUndoSnapshot(); gTypingRunActive=false; SetDirty(true);
                            if (gHideMarkdown) ToggleFace(bold);
                            else { WrapSelection("**","**"); ClearStyles(); }
                            AdjustScrollbar();
                        } else if (PtInRect(event.where, &btnI)) {
                            InvertRoundRect(&btnI,7,7);
                            while (StillDown()); InvertRoundRect(&btnI,7,7);
                            PushUndoSnapshot(); gTypingRunActive=false; SetDirty(true);
                            if (gHideMarkdown) ToggleFace(italic);
                            else { WrapSelection("*","*"); ClearStyles(); }
                            AdjustScrollbar();
                        } else if (PtInRect(event.where, &btnView)) {
                            InvertRoundRect(&btnView,7,7);
                            while (StillDown()); InvertRoundRect(&btnView,7,7);
                            SetViewMode(!gHideMarkdown);
                        } else if (PtInRect(event.where, &btnRefresh)) {
                            InvertRoundRect(&btnRefresh,7,7);
                            while (StillDown()); InvertRoundRect(&btnRefresh,7,7);
                            InvalidateHeightCache();
                            if (gActiveTE) {
                                LongRect lr; Rect vr;
                                WEGetViewRect(&lr, gActiveTE);
                                SetRect(&vr,(short)lr.left,(short)lr.top,(short)lr.right,(short)lr.bottom);
                                InvalWindowRect(gWindow, &vr);
                            }
                        } else if (FindControl(event.where, w, &hitControl)) {
                            if (hitControl == gScrollBar)
                                DoScrollClick(event.where);
                            else if (hitControl == gJumpToTopBtn) {
                                if (TrackControl(gJumpToTopBtn, event.where, NULL) == kControlButtonPart)
                                    HandleJumpToTop();
                            } else if (hitControl == gJumpToEndBtn) {
                                if (TrackControl(gJumpToEndBtn, event.where, NULL) == kControlButtonPart)
                                    HandleJumpToEnd();
                            }
                        } else {
                            gTypingRunActive = false;
                            WEClick(event.where, (event.modifiers & shiftKey) != 0, gActiveTE);
                        }
                    }
                }
                break;

            case keyDown:
            case autoKey: {
                SetActiveDocument(GetDocumentForWindow(FrontWindow()));
                if (!gActiveDoc && (event.modifiers & cmdKey) == 0) break;

                gScrollbarDriven = false;
                char key     = event.message & charCodeMask;
                char keyCode = (event.message & keyCodeMask) >> 8;
                Boolean isContentKey = (key < 0x1C || key > 0x1F);
                Boolean handled      = false;

                if (keyCode == 0x75) { /* Forward delete */
                    long selStart, selEnd;
                    WEGetSelection(&selStart, &selEnd, gActiveTE);
                    PushUndoSnapshot(); gTypingRunActive = false;
                    if (selStart == selEnd) {
                        if (selStart < WEGetTextLength(gActiveTE)) {
                            WESetSelect(selStart, selStart+1, gActiveTE);
                            WEDelete(gActiveTE);
                        }
                    } else { WEDelete(gActiveTE); }
                    gDirty = true;
                    ScrollCaretIntoView(false); UpdateScrollbarRange();
                    handled = true;
                } else if (keyCode == 0x73) { /* Home */
                    short ls, le; GetCurrentLineRange(&ls, &le);
                    long scan = ls;
                    Handle hText = WEGetText(gActiveTE);
                    HLock(hText);
                    while (scan < le && ((*hText)[scan]==' ' || (*hText)[scan]=='\t')) scan++;
                    HUnlock(hText);
                    WESetSelect(scan,scan,gActiveTE); ScrollCaretIntoView(true); handled=true;
                } else if (keyCode == 0x74) { HandlePageUp();   handled=true; }
                  else if (keyCode == 0x79) { HandlePageDown(); handled=true; }
                  else if (keyCode == 0x77) {
                    short ls,le; GetCurrentLineRange(&ls,&le);
                    WESetSelect(le,le,gActiveTE); ScrollCaretIntoView(false); handled=true;
                } else if ((event.modifiers & cmdKey) && keyCode == 0x7E) { HandleJumpToTop(); handled=true; }
                  else if ((event.modifiers & cmdKey) && keyCode == 0x7D) { HandleJumpToEnd(); handled=true; }
                  else if ((event.modifiers & cmdKey) && keyCode == 0x7B) {
                    short ls,le; GetCurrentLineRange(&ls,&le);
                    WESetSelect(ls,ls,gActiveTE); ScrollCaretIntoView(true); handled=true;
                } else if ((event.modifiers & cmdKey) && keyCode == 0x7C) {
                    short ls,le; GetCurrentLineRange(&ls,&le);
                    WESetSelect(le,le,gActiveTE); ScrollCaretIntoView(false); handled=true;
                }

                if (!handled) {
                    if (event.modifiers & cmdKey) {
                        if (event.what == keyDown) {
                            if ((key=='z'||key=='Z') && (event.modifiers & shiftKey))
                                DoRedo();
                            else if (key=='d'||key=='D') {
                                PushUndoSnapshot(); gTypingRunActive=false; SetDirty(true);
                                InsertDateHeading(2); ScrollCaretIntoView(false); UpdateScrollbarRange();
                            } else if (key=='t'||key=='T') {
                                PushUndoSnapshot(); gTypingRunActive=false; SetDirty(true);
                                InsertTimeHeading(3); ScrollCaretIntoView(false); UpdateScrollbarRange();
                            } else {
                                UpdateEditMenuState();
                                DoMenuCommand(MenuKey(key));
                            }
                        }
                    } else {
                        Boolean isArrowKey = (!isContentKey && key >= 0x1C && key <= 0x1F);
                        if (isContentKey) {
                            if (!gTypingRunActive) { PushUndoSnapshot(); gTypingRunActive = true; }
                        } else { gTypingRunActive = false; }

                        if (isArrowKey && (event.modifiers & shiftKey)) {
                            long selStart, selEnd, activeEnd, newPos;
                            WEGetSelection(&selStart, &selEnd, gActiveTE);
                            if (!gShiftSelectionActive) { gShiftSelectionActive=true; gShiftAnchor=(short)selStart; }
                            activeEnd = (gShiftAnchor == selStart) ? selEnd : selStart;
                            WESetSelect(activeEnd, activeEnd, gActiveTE);
                            WEKey(key, keyCode, event.modifiers, gActiveTE);
                            WEGetSelection(&selStart, &selEnd, gActiveTE);
                            newPos = selStart;
                            if (newPos < gShiftAnchor) WESetSelect(newPos, gShiftAnchor, gActiveTE);
                            else                       WESetSelect(gShiftAnchor, newPos, gActiveTE);
                        } else {
                            if (isArrowKey || key==0x09 || key==0x0D || key==0x08 || isContentKey)
                                gShiftSelectionActive = false;

                            if (key == 0x0D) {
                                /* Return key with list-continuation logic — identical to classic */
                                short lineStart, lineEnd, caret;
                                Handle hText = WEGetText(gActiveTE);
                                short prefixLen = 0;
                                char  prefixBuf[64];
                                Boolean doInsertCR = true;
                                long selStart, selEnd;
                                WEGetSelection(&selStart, &selEnd, gActiveTE);
                                caret = (short)selStart;
                                GetCurrentParagraphRange(&lineStart, &lineEnd);
                                HLock(hText);
                                short scan = lineStart;
                                while (scan < caret && ((*hText)[scan]==' ' || (*hText)[scan]=='\t')) scan++;
                                if (gHideMarkdown) {
                                    if (scan < caret && ((unsigned char)(*hText)[scan]==0xA5 ||
                                        (*hText)[scan]=='o' || (*hText)[scan]=='s' || (*hText)[scan]=='-') &&
                                        scan+1 < caret && (*hText)[scan+1]==' ')
                                        prefixLen = (scan+2) - lineStart;
                                } else {
                                    if (scan < caret && (*hText)[scan]=='-' && scan+1 < caret && (*hText)[scan+1]==' ')
                                        prefixLen = (scan+2) - lineStart;
                                }
                                if (prefixLen == 0) {
                                    short ii = scan;
                                    while (ii < caret && (*hText)[ii]>='0' && (*hText)[ii]<='9') ii++;
                                    if (ii > scan && ii < caret && (*hText)[ii]=='.' && ii+1 < caret && (*hText)[ii+1]==' ')
                                        prefixLen = (ii+2) - lineStart;
                                }
                                if (prefixLen > 0 && prefixLen <= 63)
                                    BlockMove(*hText + lineStart, prefixBuf, prefixLen);
                                HUnlock(hText);

                                if (prefixLen > 0) {
                                    if (lineEnd == lineStart + prefixLen) {
                                        WESetSelect(lineStart, lineEnd, gActiveTE);
                                        WEDelete(gActiveTE);
                                        doInsertCR = false;
                                        SetDirty(true);
                                    } else {
                                        Boolean isNumbered = false;
                                        for (short ii=0; ii < prefixLen; ii++)
                                            if (prefixBuf[ii]>='0' && prefixBuf[ii]<='9') isNumbered=true;
                                        if (isNumbered) {
                                            long num; char newNumBuf[64];
                                            short numStart = scan - lineStart;
                                            prefixBuf[prefixLen-2] = 0;
                                            sscanf(prefixBuf+numStart, "%ld", &num);
                                            sprintf(newNumBuf, "%ld. ", num+1);
                                            prefixLen = numStart + (short)strlen(newNumBuf);
                                            if (prefixLen > 63) prefixLen = 63;
                                            BlockMove(newNumBuf, prefixBuf+numStart, strlen(newNumBuf));
                                        }
                                    }
                                }

                                if (doInsertCR) {
                                    WEKey(key, keyCode, event.modifiers, gActiveTE);
                                    if (prefixLen > 0)
                                        WEInsert(prefixBuf, prefixLen, NULL, gActiveTE);
                                }
                            } else {
                                WEKey(key, keyCode, event.modifiers, gActiveTE);
                            }

                            if (isContentKey) {
                                SetDirty(true);
                                if (gHideMarkdown) DetectInlineMarkdown(key);
                                /* @today / @time expansion (same as classic) */
                                {
                                    long selStart, selEnd;
                                    WEGetSelection(&selStart, &selEnd, gActiveTE);
                                    short caret = (short)selStart;
                                    if (caret >= 5) {
                                        Handle hText = WEGetText(gActiveTE);
                                        Boolean isToday=false, isTime=false;
                                        HLock(hText);
                                        if (caret>=6) isToday = (memcmp(*hText+caret-6,"@today",6)==0);
                                        if (!isToday) isTime  = (memcmp(*hText+caret-5,"@time", 5)==0);
                                        HUnlock(hText);
                                        if (isToday) {
                                            CFGregorianDate gd = CFAbsoluteTimeGetGregorianDate(CFAbsoluteTimeGetCurrent(), NULL);
                                            char buf[16]; sprintf(buf,"%04d-%02d-%02d",(int)gd.year,(int)gd.month,(int)gd.day);
                                            WESetSelect(caret-6,caret,gActiveTE);
                                            WEDelete(gActiveTE);
                                            WEInsert(buf,(long)strlen(buf),NULL,gActiveTE);
                                        } else if (isTime) {
                                            CFGregorianDate gd = CFAbsoluteTimeGetGregorianDate(CFAbsoluteTimeGetCurrent(), NULL);
                                            char buf[16]; sprintf(buf,"%02d:%02d",(int)gd.hour,(int)gd.minute);
                                            WESetSelect(caret-5,caret,gActiveTE);
                                            WEDelete(gActiveTE);
                                            WEInsert(buf,(long)strlen(buf),NULL,gActiveTE);
                                        }
                                    }
                                }
                            }
                        }
                        ScrollCaretIntoView(key==0x1E || key==0x1C || key==0x08);
                        UpdateScrollbarRange();
                    }
                }
                break;
            }

            case activateEvt:
                SetActiveDocument(GetDocumentForWindow((WindowRef)event.message));
                if (gActiveDoc) {
                    if ((event.modifiers & activeFlag) != 0) {
                        SetPortWindowPort(gWindow);
                        WEActivate(gActiveTE);
                        Rect portRect;
                        GetWindowPortBounds(gWindow, &portRect);
                        InvalWindowRect(gWindow, &portRect);
                    } else {
                        WEDeactivate(gActiveTE);
                    }
                }
                break;

            case kHighLevelEvent:
                /* Dispatch Apple Events (file open, quit, etc.) */
                AEProcessAppleEvent(&event);
                break;
            }
        }

        SetActiveDocument(GetDocumentForWindow(FrontWindow()));
        if (gActiveDoc) {
            WEIdle(gActiveTE);
            UpdateStatusBar(FrontWindow(), false);
        }
    }
}

/* -----------------------------------------------------------------------
   Font helpers (unchanged from classic)
   ----------------------------------------------------------------------- */
short GetDefaultFontNum(void)
{
    short fontNum;
    if (gUseSansSerif) GetFNum("\pHelvetica", &fontNum);
    else               GetFNum("\pTimes",     &fontNum);
    return fontNum;
}

void SetFontMode(Boolean useSans)
{
    gUseSansSerif = useSans;
    CheckMenuItem(gViewMenu, iSerif,     !useSans);
    CheckMenuItem(gViewMenu, iSansSerif,  useSans);
    if (!gActiveDoc) return;
    SyncWindowToBacking();
    if (gHideMarkdown) { SyncHiddenToCanonical(); ClearStyles(); BuildHiddenView(); }
    else               { ClearStyles(); BuildHiddenView(); }
    AdjustScrollbar();
    Rect portRect;
    GetWindowPortBounds(gWindow, &portRect);
    InvalWindowRect(gWindow, &portRect);
}

void SetDirty(Boolean dirty)
{
    Boolean oldDirty = gDirty;
    gDirty = dirty;
    if (dirty && gHideMarkdown) gWriterDirty = true;
    if (oldDirty != dirty && gWindow != NULL)
        DrawTopMiddleButtons(gWindow);
}

/* -----------------------------------------------------------------------
   Entry point
   ----------------------------------------------------------------------- */
int main(void)
{
    Init();
    LoadZoomPref();
    MakeMenu();
    MakeWindow();

    /* Install Apple Event handlers */
    AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments,
                          NewAEEventHandlerUPP(HandleOpenDocuments), 0, false);
    AEInstallEventHandler(kCoreEventClass, kAEQuitApplication,
                          NewAEEventHandlerUPP(HandleQuitApp), 0, false);

    /* Force first paint before showing splash */
    if (gActiveDoc) DoUpdate(gWindow);

    ShowSplashScreen();
    EventLoop();
    return 0;
}

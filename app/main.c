/*
    Milestone 2: a real distraction-free Markdown editor.
    Full-screen window, wide margins, 14pt Times, File menu with
    Save/Open backed by the classic File Manager. Saving straight to
    the BlueSCSI SD card (bypassing this disk's HFS volume) is a
    later milestone -- this still saves onto the boot disk itself.
*/

#include "app.h"
#include <stdio.h>
#include <string.h>

#ifndef ARTFUL_PRO

WindowPtr gWindow;
TEHandle gTE = NULL;
TEHandle gHiddenTE = NULL;
Handle gMarkdownText = NULL;
long gMarkdownLen = 0;
Handle gWriterText = NULL;
long gWriterLen = 0;
Handle gWriterOpsH = NULL;
short gWriterOpCount = 0;
long gWindowStart = 0;
long gWindowEnd = 0;
Handle gLineOffsetsH = NULL;
long gNumLines = 0;
long gLastCharCount = -1;
short gLastLine = -1;
short gLastCol = -1;
Boolean gShowStatusBar = true;
TEHandle gActiveTE;
ControlHandle gScrollBar;
ControlHandle gJumpToTopBtn = NULL;
ControlHandle gJumpToEndBtn = NULL;
Boolean gScrollBarVisible = false;




Boolean gHaveFile = false;
Boolean gDirty = false;
Str255 gFileName;
short gVRefNum;
Boolean gHideMarkdown = true;

Boolean gShiftSelectionActive = false;
short gShiftAnchor = 0;

UndoSnapshot gUndoStack[MAX_UNDO_LEVELS];
short gUndoCount = 0;
UndoSnapshot gRedoStack[MAX_UNDO_LEVELS];
short gRedoCount = 0;
Boolean gTypingRunActive = false;

Str255 gLinkURLs[MAX_LINKS + 1];
short gLinkCount = 0;

#else

DocumentRecord *gDocumentList = NULL;
DocumentRecord *gActiveDoc = NULL;
MenuHandle gWindowMenu = NULL;

DocumentRecord* GetDocumentForWindow(WindowPtr w)
{
    if (w == NULL) return NULL;
    return (DocumentRecord*) GetWRefCon(w);
}

DocumentRecord* CreateNewDocument(void)
{
    DocumentRecord *doc = (DocumentRecord*) NewPtrClear(sizeof(DocumentRecord));
    if (doc) {
        doc->hideMarkdown = true;
        doc->markdownText = NULL;
        doc->markdownLen = 0;
        doc->lastCharCount = -1;
        doc->lastLine = -1;
        doc->lastCol = -1;
        doc->showStatusBar = true;
        doc->zoomIndex = gDefaultZoomIndex;
        doc->next = gDocumentList;
        gDocumentList = doc;
    }
    return doc;
}

void DisposeDocument(DocumentRecord *doc)
{
    if (doc) {
        if (gDocumentList == doc) {
            gDocumentList = doc->next;
        } else {
            DocumentRecord *curr = gDocumentList;
            while (curr && curr->next != doc) {
                curr = curr->next;
            }
            if (curr) {
                curr->next = doc->next;
            }
        }
        if (doc->markdownText) DisposeHandle(doc->markdownText);
        if (doc->writerText) DisposeHandle(doc->writerText);
        if (doc->writerOpsH) DisposeHandle(doc->writerOpsH);
        if (doc->lineOffsetsH) DisposeHandle(doc->lineOffsetsH);
        if (doc->te) TEDispose(doc->te);
        if (doc->hiddenTE) TEDispose(doc->hiddenTE);
        DisposePtr((Ptr)doc);
    }
}

void SetActiveDocument(DocumentRecord *doc)
{
    gActiveDoc = doc;
#ifdef ARTFUL_PRO
    if (gActiveDoc) {
        gTE = gActiveDoc->te;
        gHiddenTE = gActiveDoc->hiddenTE;
        gWindow = gActiveDoc->window;
        gActiveTE = gActiveDoc->hideMarkdown ? gHiddenTE : gTE;
    }
    if (gActiveDoc && gViewMenu) {
        CheckItem(gViewMenu, iMarkdownView, !gActiveDoc->hideMarkdown);
        CheckItem(gViewMenu, iWriterView, gActiveDoc->hideMarkdown);
    }
#endif
}

void UpdateWindowMenu(void)
{
    short count = CountMItems(gWindowMenu);
    short i;
    DocumentRecord *curr;
    Str255 title;

    for (i = count; i > 0; i--) {
        DeleteMenuItem(gWindowMenu, i);
    }
    
    curr = gDocumentList;
    i = 1;
    while (curr) {
        if (curr->haveFile) {
            BlockMove(curr->fileName, title, curr->fileName[0] + 1);
        } else {
            BlockMove("\pUntitled", title, 9);
        }
        AppendMenu(gWindowMenu, title);
        if (curr == gActiveDoc) {
            CheckItem(gWindowMenu, i, true);
        }
        curr = curr->next;
        i++;
    }
}

#endif

Boolean gScrollbarDriven = false; /* set during scrollbar-driven loads to suppress caret-based window shifting */
long gWindowStartLine = 1;        /* global line number at top of current TE window */

Boolean gDone = false;
MenuHandle gViewMenu;
MenuHandle gEditMenu;
short gDefaultZoomIndex = kZoomDefaultIndex;
#ifndef ARTFUL_PRO
short gZoomIndex = kZoomDefaultIndex;
#endif
Boolean gUseSansSerif = true;

static void Init(void)
{
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();
}

/*
    Writer mode gets a black menu bar with white text; Markdown mode gets
    the standard look. There's no Menu Manager API for this on classic Mac
    OS (that's a much later Appearance Manager concept) -- on a 1-bit
    display, drawing the normal bar and then XOR-inverting that strip
    achieves the same thing trivially. Must target the Window Manager
    port (global screen coordinates), not whatever window's port happens
    to be current, since the menu bar isn't part of any window.
*/
void UpdateMenuBarLook(void)
{
    GrafPtr savePort;
    GrafPtr wMgrPort;
    Rect bar;
    Boolean doHide;

#ifdef ARTFUL_PRO
    doHide = (gActiveDoc != NULL) ? gActiveDoc->hideMarkdown : false;
#else
    doHide = gHideMarkdown;
#endif

    DrawMenuBar();

    if (doHide) {
        GetPort(&savePort);
        GetWMgrPort(&wMgrPort);
        SetPort(wMgrPort);

        SetRect(&bar, 0, 0, qd.screenBits.bounds.right, MENU_BAR_HEIGHT);
        InvertRect(&bar);

        SetPort(savePort);
    }
}

static void MakeMenu(void)
{
    MenuHandle fileMenu;
    MenuHandle styleMenu;
    MenuHandle helpMenu;

    fileMenu = NewMenu(mFile, "\pFile");
    AppendMenu(fileMenu, "\pNew/N;Open.../O;Save/S;Save As...;(-;Quit/Q");
    InsertMenu(fileMenu, 0);

    /* No "/" shortcut on Redo -- it would register as a second cmd-key
       equivalent for the same letter as Undo, ambiguous to MenuKey.
       Cmd-Shift-Z for Redo is instead handled directly in EventLoop,
       intercepted before MenuKey ever sees it. */
    gEditMenu = NewMenu(mEdit, "\pEdit");
    AppendMenu(gEditMenu, "\pUndo/Z;Redo;(-;Cut/X;Copy/C;Paste/V;(-;Select All/A;(-;Search.../F;Search and Replace...");
    InsertMenu(gEditMenu, 0);

    DisableItem(gEditMenu, iUndo);
    DisableItem(gEditMenu, iRedo);

    styleMenu = NewMenu(mStyle, "\pStyle");
    AppendMenu(styleMenu, "\pBold/B;Italic/I;Code/K;Strikethrough;(-;Heading 1/1;Heading 2/2;Heading 3/3;(-;Link/L;(-;None");
    InsertMenu(styleMenu, 0);

    gViewMenu = NewMenu(mView, "\pView");
    AppendMenu(gViewMenu, "\pMarkdown;Writer;(-;Zoom In/=;Zoom Out/-;Default Size/0;(-;Serif;Sans Serif;(-;Show Status Bar");
    InsertMenu(gViewMenu, 0);
    CheckItem(gViewMenu, iWriterView, true);
    CheckItem(gViewMenu, iSansSerif, true);
    CheckItem(gViewMenu, iStatusBar, true);

#ifdef ARTFUL_PRO
    gWindowMenu = NewMenu(mWindow, "\pWindow");
    InsertMenu(gWindowMenu, 0);
#endif

    helpMenu = NewMenu(mHelp, "\pHelp");
    AppendMenu(helpMenu, "\pAbout The Artful Type...");
    InsertMenu(helpMenu, 0);

    UpdateMenuBarLook();
}

void MakeWindow(void)
{
    Rect bounds;
    Rect viewRect;
    Rect sbRect;
    short fontNum;
    
#ifdef ARTFUL_PRO
    static short cascadeOffset = 0;
    DocumentRecord *doc = CreateNewDocument();
    if (!doc) return;
    SetActiveDocument(doc);
    
    bounds = qd.screenBits.bounds;
    bounds.top += MENU_BAR_HEIGHT + 20 + cascadeOffset;
    bounds.bottom -= 20 - cascadeOffset;
    bounds.left += 20 + cascadeOffset;
    bounds.right -= 20 - cascadeOffset;
    
    cascadeOffset += 20;
    if (cascadeOffset > 100) cascadeOffset = 0;
    
    gWindow = NewWindow(NULL, &bounds, "\pUntitled", true, documentProc,
                         (WindowPtr) -1L, true, (long)doc);
#else
    bounds = qd.screenBits.bounds;
    bounds.top += MENU_BAR_HEIGHT;

    gWindow = NewWindow(NULL, &bounds, "\p", true, plainDBox,
                         (WindowPtr) -1L, false, 0);
#endif

    SetPort(gWindow);

    fontNum = GetDefaultFontNum();
    TextFont(fontNum);
    TextSize(CurrentFontSize());

    viewRect = gWindow->portRect;
    viewRect.left += MARGIN_H;
    viewRect.right -= MARGIN_H;
    viewRect.top += MARGIN_TOP;
    viewRect.bottom -= MARGIN_BOTTOM;

    {
        short monoFont;
        GetFNum("\pMonaco", &monoFont);
        if (monoFont == 0) GetFNum("\pCourier", &monoFont);
        if (monoFont != 0) TextFont(monoFont);
    }
    gTE = TEStyleNew(&viewRect, &viewRect);

    TextFont(fontNum);
    gHiddenTE = TEStyleNew(&viewRect, &viewRect);
    gActiveTE = gHideMarkdown ? gHiddenTE : gTE;

#ifdef ARTFUL_PRO
    if (gActiveDoc) {
        gActiveDoc->te = gTE;
        gActiveDoc->hiddenTE = gHiddenTE;
        gActiveDoc->window = gWindow;
    }
#endif

    TEActivate(gActiveTE);

    sbRect = viewRect;
    sbRect.left = viewRect.right + (MARGIN_H - SCROLLBAR_WIDTH) / 2;
    sbRect.right = sbRect.left + SCROLLBAR_WIDTH;
    sbRect.top -= 1;
    sbRect.bottom += 1;
    gScrollBar = NewControl(gWindow, &sbRect, "\p", false, 0, 0, 0, scrollBarProc, 0);

    {
        Rect btnRect;
        
        /* Jump to Top Button (Top Left) */
        btnRect.top = 2;
        btnRect.bottom = 22;
        btnRect.left = viewRect.left;
        btnRect.right = btnRect.left + 90;
        gJumpToTopBtn = NewControl(gWindow, &btnRect, "\pJump to Top", true, 0, 0, 0, pushButProc, 0);

        /* Jump to End Button (Top Right) */
        btnRect.top = 2;
        btnRect.bottom = 22;
        btnRect.right = viewRect.right;
        btnRect.left = btnRect.right - 90;
        gJumpToEndBtn = NewControl(gWindow, &btnRect, "\pJump to End", true, 0, 0, 0, pushButProc, 0);

    }
}




static void UpdateStatusBar(WindowPtr w, Boolean forceDraw)
{
    long chars;
    long caret;
    short line = 0, col = 0;
    short i;
    char statusStr[128];
    Str255 pStatusStr;
    Rect statusRect;
    GrafPtr savedPort;
    const char *modeStr;
    static Boolean lastMode = -1;

    if (!gActiveTE || !gShowStatusBar) return;
    
    chars = TotalLength();
    caret = (**gActiveTE).selStart;

    line = 1;
    col = 1;
    {
        short scan;
        short newlines = 0;
        Handle hText = (Handle)(**gActiveTE).hText;
        HLock(hText);
        for (scan = 0; scan < caret; scan++) {
            if ((*hText)[scan] == '\r') {
                newlines++;
            }
        }
        
        scan = caret;
        while (scan > 0 && (*hText)[scan - 1] != '\r') {
            scan--;
            col++;
        }
        HUnlock(hText);
        line = (short)((long)gWindowStartLine + newlines);
    }


    if (!forceDraw && chars == gLastCharCount && line == gLastLine && col == gLastCol && gHideMarkdown == lastMode)
        return;

    gLastCharCount = chars;
    gLastLine = line;
    gLastCol = col;
    lastMode = gHideMarkdown;

    statusRect = w->portRect;
    
    GetPort(&savedPort);
    SetPort(w);

    statusRect.left = (**gActiveTE).viewRect.left;
    statusRect.right = statusRect.left + 300;
    statusRect.top = (**gActiveTE).viewRect.bottom;
    statusRect.bottom = w->portRect.bottom;

    EraseRect(&statusRect);

    modeStr = gHideMarkdown ? "Writer" : "Markdown";
    sprintf(statusStr, "[%s]    Chars: %ld    Line: %d    Col: %d", modeStr, chars, line, col);


    pStatusStr[0] = strlen(statusStr);
    BlockMove(statusStr, pStatusStr + 1, pStatusStr[0]);

    MoveTo(statusRect.left, statusRect.top + 14);
    
    PenNormal();
    TextFont(0);
    TextSize(9);
    TextMode(srcOr);
    ForeColor(blackColor);
    BackColor(whiteColor);
    DrawString(pStatusStr);

    {
        short fontNum = 0;
        GetFNum("\pTimes", &fontNum); /* Default is Times based on earlier logic, or use system default */
        TextFont(fontNum);
        TextSize(12);
    }

    SetPort(savedPort);
}

static void DrawTopMiddleButtons(WindowPtr w)
{
    Rect r;
    short centerX = (w->portRect.right - w->portRect.left) / 2;
    short startX = centerX - 56;
    Str255 s;
    short textWidth;
    
    GrafPtr savedPort;
    GetPort(&savedPort);
    SetPort(w);
    
    TextFont(0); /* System Font */
    TextSize(0);
    
    /* 1. Draw B Button */
    SetRect(&r, startX, 2, startX + 25, 22);
    FrameRoundRect(&r, 6, 6);
    TextFace(bold);
    BlockMove("\pB", s, 2);
    textWidth = StringWidth(s);
    MoveTo(r.left + (r.right - r.left - textWidth) / 2, r.top + 14);
    DrawString(s);
    
    /* 2. Draw I Button */
    SetRect(&r, startX + 31, 2, startX + 56, 22);
    FrameRoundRect(&r, 6, 6);
    TextFace(italic);
    BlockMove("\pI", s, 2);
    textWidth = StringWidth(s);
    MoveTo(r.left + (r.right - r.left - textWidth) / 2, r.top + 14);
    DrawString(s);
    
    /* 3. Draw View Button */
    SetRect(&r, startX + 62, 2, startX + 112, 22);
    FrameRoundRect(&r, 6, 6);
    TextFace(normal);
    BlockMove("\pView", s, 5);
    textWidth = StringWidth(s);
    MoveTo(r.left + (r.right - r.left - textWidth) / 2, r.top + 14);
    DrawString(s);
    
    TextFace(normal); /* restore */
    SetPort(savedPort);
}

static void DoUpdate(WindowPtr w)
{
    GrafPtr savedPort;
    GetPort(&savedPort);
    SetPort(w);
    
    BeginUpdate(w);
    EraseRect(&w->portRect);
    TEUpdate(&w->portRect, gActiveTE);
    DrawControls(w);
    DrawTopMiddleButtons(w);
    DrawGrowIcon(w);
    UpdateStatusBar(w, true);
    EndUpdate(w);
    
    SetPort(savedPort);
}


static void ToggleStatusBar(void)
{
    gShowStatusBar = !gShowStatusBar;
    CheckItem(gViewMenu, iStatusBar, gShowStatusBar);
    gLastCharCount = -1; /* force full redraw */
#ifdef ARTFUL_PRO
    if (gActiveDoc) {
        Rect eraseRect = gWindow->portRect;
        eraseRect.top = eraseRect.bottom - 24;
        EraseRect(&eraseRect); /* Clean up old status bar area */
        InvalRect(&gWindow->portRect);
    }
#else
    {
        Rect eraseRect = gWindow->portRect;
        eraseRect.top = eraseRect.bottom - 24;
        EraseRect(&eraseRect);
        InvalRect(&gWindow->portRect);
    }
#endif
}

static char LowerCase(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    return c;
}

static void GetActiveBackingStore(Handle *targetH, long **targetLenPtr)
{
    if (gHideMarkdown) {
        *targetH = gWriterText;
        *targetLenPtr = &gWriterLen;
    } else {
        *targetH = gMarkdownText;
        *targetLenPtr = &gMarkdownLen;
    }
}

static Boolean FindTextInHandle(Handle targetH, long targetLen, unsigned char *target, long startOffset, long *foundStart, long *foundEnd)
{
    long targetLenParam = target[0];
    if (targetLenParam == 0) return false;
    
    HLock(targetH);
    long i;
    /* Search from startOffset to end */
    for (i = startOffset; i <= targetLen - targetLenParam; i++) {
        short j;
        Boolean match = true;
        for (j = 0; j < targetLenParam; j++) {
            if (LowerCase((*targetH)[i + j]) != LowerCase(target[1 + j])) {
                match = false;
                break;
            }
        }
        if (match) {
            *foundStart = i;
            *foundEnd = i + targetLenParam;
            HUnlock(targetH);
            return true;
        }
    }
    
    /* Wrap around: search from 0 to startOffset */
    for (i = 0; i < startOffset && i <= targetLen - targetLenParam; i++) {
        short j;
        Boolean match = true;
        for (j = 0; j < targetLenParam; j++) {
            if (LowerCase((*targetH)[i + j]) != LowerCase(target[1 + j])) {
                match = false;
                break;
            }
        }
        if (match) {
            *foundStart = i;
            *foundEnd = i + targetLenParam;
            HUnlock(targetH);
            return true;
        }
    }
    
    HUnlock(targetH);
    return false;
}

void DoSearch(void)
{
    DialogPtr dlg;
    short item;
    DialogItemType type;
    Handle itemH;
    Rect box;
    Str255 target;
    
    if (!gActiveTE) return;
    
    dlg = GetNewDialog(kSearchDialog, NULL, (WindowPtr) -1L);
    if (dlg == NULL) return;
    
    SelectDialogItemText(dlg, iSearchField, 0, 32767);
    
    do {
        ModalDialog(NULL, &item);
    } while (item != iSearchOK && item != iSearchCancel);
    
    if (item == iSearchOK) {
        GetDialogItem(dlg, iSearchField, &type, &itemH, &box);
        GetDialogItemText(itemH, target);
        
        if (target[0] > 0) {
            Handle backingH;
            long *backingLenPtr;
            GetActiveBackingStore(&backingH, &backingLenPtr);
            
            if (backingH != NULL) {
                SyncWindowToBacking();
                
                long matchStart, matchEnd;
                long currentGlobalPos = gWindowStart + (**gActiveTE).selEnd;
                if (FindTextInHandle(backingH, *backingLenPtr, target, currentGlobalPos, &matchStart, &matchEnd)) {
                    if (matchStart < gWindowStart || matchEnd > gWindowEnd) {
                        LoadTextWindow(matchStart);
                    }
                    
                    short localStart = matchStart - gWindowStart;
                    short localEnd = matchEnd - gWindowStart;
                    TESetSelect(localStart, localEnd, gActiveTE);
                    ScrollCaretIntoView(false);
                } else {
                    SysBeep(30);
                }
            }
        }
    }
    DisposeDialog(dlg);
    SetPort(gWindow);
    UpdateMenuBarLook();
}

void DoSearchReplace(void)
{
    DialogPtr dlg;
    short item;
    DialogItemType type;
    Handle itemH;
    Rect box;
    Str255 findTarget;
    Str255 replaceWith;
    
    if (!gActiveTE) return;
    
    dlg = GetNewDialog(kSearchReplaceDialog, NULL, (WindowPtr) -1L);
    if (dlg == NULL) return;
    
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
            Handle backingH;
            long *backingLenPtr;
            GetActiveBackingStore(&backingH, &backingLenPtr);
            
            if (backingH != NULL) {
                SyncWindowToBacking();
                
                if (item == iReplaceOK) {
                    /* Single Replace */
                    long matchStart, matchEnd;
                    long currentGlobalPos = gWindowStart + (**gActiveTE).selStart;
                    if (FindTextInHandle(backingH, *backingLenPtr, findTarget, currentGlobalPos, &matchStart, &matchEnd)) {
                        PushUndoSnapshot();
                        gTypingRunActive = false;
                        
                        long diff = replaceWith[0] - findTarget[0];
                        
                        if (diff != 0) {
                            SetHandleSize(backingH, *backingLenPtr + diff);
                            HLock(backingH);
                            if (matchStart + findTarget[0] < *backingLenPtr) {
                                BlockMove(*backingH + matchStart + findTarget[0],
                                          *backingH + matchStart + findTarget[0] + diff,
                                          *backingLenPtr - (matchStart + findTarget[0]));
                            }
                            HUnlock(backingH);
                            *backingLenPtr += diff;
                            
                            if (gHideMarkdown && gWriterOpsH != NULL) {
                                short k;
                                StyleOp *ops;
                                HLock(gWriterOpsH);
                                ops = (StyleOp *) *gWriterOpsH;
                                for (k = 0; k < gWriterOpCount; k++) {
                                    if (ops[k].start >= matchStart + findTarget[0]) {
                                        ops[k].start += diff;
                                        ops[k].end += diff;
                                    } else if (ops[k].end > matchStart) {
                                        ops[k].end += diff;
                                        if (ops[k].end < ops[k].start) ops[k].end = ops[k].start;
                                    }
                                }
                                HUnlock(gWriterOpsH);
                            }
                        }
                        
                        HLock(backingH);
                        BlockMove(&replaceWith[1], *backingH + matchStart, replaceWith[0]);
                        HUnlock(backingH);
                        
                        gDirty = true;
                        LoadTextWindow(matchStart);
                        
                        short localStart = matchStart - gWindowStart;
                        short localEnd = localStart + replaceWith[0];
                        TESetSelect(localStart, localEnd, gActiveTE);
                        ScrollCaretIntoView(false);
                        AdjustScrollbar();
                    } else {
                        SysBeep(30);
                    }
                } else if (item == iReplaceAll) {
                    /* Replace Everywhere (Replace All) */
                    PushUndoSnapshot();
                    gTypingRunActive = false;
                    
                    long findLen = findTarget[0];
                    long replaceLen = replaceWith[0];
                    long diff = replaceLen - findLen;
                    long searchOffset = 0;
                    long replaceCount = 0;
                    
                    while (searchOffset <= *backingLenPtr - findLen) {
                        Boolean match = true;
                        long j;
                        
                        HLock(backingH);
                        for (j = 0; j < findLen; j++) {
                            if (LowerCase((*backingH)[searchOffset + j]) != LowerCase(findTarget[1 + j])) {
                                match = false;
                                break;
                            }
                        }
                        HUnlock(backingH);
                        
                        if (match) {
                            if (diff != 0) {
                                SetHandleSize(backingH, *backingLenPtr + diff);
                                HLock(backingH);
                                if (searchOffset + findLen < *backingLenPtr) {
                                    BlockMove(*backingH + searchOffset + findLen,
                                              *backingH + searchOffset + findLen + diff,
                                              *backingLenPtr - (searchOffset + findLen));
                                }
                                HUnlock(backingH);
                                *backingLenPtr += diff;
                                
                                if (gHideMarkdown && gWriterOpsH != NULL) {
                                    short k;
                                    StyleOp *ops;
                                    HLock(gWriterOpsH);
                                    ops = (StyleOp *) *gWriterOpsH;
                                    for (k = 0; k < gWriterOpCount; k++) {
                                        if (ops[k].start >= searchOffset + findLen) {
                                            ops[k].start += diff;
                                            ops[k].end += diff;
                                        } else if (ops[k].end > searchOffset) {
                                            ops[k].end += diff;
                                            if (ops[k].end < ops[k].start) ops[k].end = ops[k].start;
                                        }
                                    }
                                    HUnlock(gWriterOpsH);
                                }
                            }
                            
                            HLock(backingH);
                            BlockMove(&replaceWith[1], *backingH + searchOffset, replaceLen);
                            HUnlock(backingH);
                            
                            searchOffset += replaceLen;
                            replaceCount++;
                        } else {
                            searchOffset++;
                        }
                    }
                    
                    if (replaceCount > 0) {
                        gDirty = true;
                        LoadTextWindow(gWindowStart);
                    } else {
                        SysBeep(30);
                    }
                }
            }
        }
    }
    DisposeDialog(dlg);
    SetPort(gWindow);
    UpdateMenuBarLook();
}



static void DoMenuCommand(long menuResult)

{
    short menuID = HiWord(menuResult);
    short menuItem = LoWord(menuResult);

    if (menuID == mFile) {
        switch (menuItem) {
            case iNew:
#ifdef ARTFUL_PRO
                DoNewFile();
#else
                if (ConfirmDiscardChanges())
                    DoNewFile();
#endif
                break;
            case iOpen:
#ifdef ARTFUL_PRO
                DoOpenFile();
#else
                if (ConfirmDiscardChanges())
                    DoOpenFile();
#endif
                break;
            case iSave:   DoSave(); break;
            case iSaveAs: DoSaveAs(); break;
            case iQuit:
#ifdef ARTFUL_PRO
                {
                    DocumentRecord *curr = gDocumentList;
                    gDone = true;
                    while (curr) {
                        SetActiveDocument(curr);
                        SelectWindow(curr->window);
                        if (!ConfirmDiscardChanges()) {
                            gDone = false;
                            break;
                        }
                        curr = curr->next;
                    }
                }
#else
                if (ConfirmDiscardChanges())
                    gDone = true;
#endif
                break;
        }
    } else if (menuID == mEdit) {
        switch (menuItem) {
            case iUndo:      DoUndo(); break;
            case iRedo:      DoRedo(); break;
            case iCut:       DoCut(); break;
            case iCopy:      DoCopy(); break;
            case iPaste:     DoPaste(); break;
            case iSelectAll: DoSelectAll(); break;
            case iSearch:    DoSearch(); break;
            case iSearchReplace: DoSearchReplace(); break;
        }

    } else if (menuID == mStyle) {
        gDirty = true;
        PushUndoSnapshot();
        gTypingRunActive = false;
        if (gHideMarkdown) {
            switch (menuItem) {
                case iBold:   ToggleFace(bold); break;
                case iItalic: ToggleFace(italic); break;
                case iCode:   ToggleCode(); break;
                case iStrike: break; /* no native strikethrough on classic Mac text styles */
                case iH1:     ToggleHeadingHidden(1); break;
                case iH2:     ToggleHeadingHidden(2); break;
                case iH3:     ToggleHeadingHidden(3); break;
                case iLink:   DoLinkHidden(); break;
                case iNone:   ClearSelectionStyleHidden(); break;
            }
        } else {
            switch (menuItem) {
                case iBold:   WrapSelection("**", "**"); break;
                case iItalic: WrapSelection("*", "*"); break;
                case iCode:   WrapSelection("`", "`"); break;
                case iStrike: WrapSelection("~~", "~~"); break;
                case iH1:     ApplyHeading(1); break;
                case iH2:     ApplyHeading(2); break;
                case iH3:     ApplyHeading(3); break;
                case iLink:   DoLink(); break;
                case iNone:   ClearMarkdownInSelection(); break;
            }
            ClearStyles();
        }
        AdjustScrollbar();
    } else if (menuID == mView) {
        switch (menuItem) {
            case iMarkdownView: SetViewMode(false); break;
            case iWriterView:   SetViewMode(true); break;
            case iZoomIn:       DoZoom(1); break;
            case iZoomOut:      DoZoom(-1); break;
            case iZoomDefault:  DoZoomReset(); break;
            case iSerif:        SetFontMode(false); break;
            case iSansSerif:    SetFontMode(true); break;
            case iStatusBar:    ToggleStatusBar(); break;
        }
    } else if (menuID == mHelp) {
        switch (menuItem) {
            case iAbout: ShowAboutBox(); break;
        }
#ifdef ARTFUL_PRO
    } else if (menuID == mWindow) {
        DocumentRecord *curr = gDocumentList;
        short idx = 1;
        while (curr) {
            if (idx == menuItem) {
                SelectWindow(curr->window);
                SetActiveDocument(curr);
                break;
            }
            curr = curr->next;
            idx++;
        }
#endif
    }
    HiliteMenu(0);
    /* HiliteMenu un-hilites the clicked title assuming the Menu Manager's
       own standard white-bar/black-text look, which clobbers our inverted
       Writer-mode bar -- reassert it now that the menu has closed. */
    UpdateMenuBarLook();
}

static void GetCurrentLineRange(short *lineStart, short *lineEnd)

{
    short caretPos = (**gActiveTE).selEnd;
    short numLines = (**gActiveTE).nLines;
    short lineIdx = 0;

    while (lineIdx < numLines - 1 && (**gActiveTE).lineStarts[lineIdx + 1] <= caretPos) {
        lineIdx++;
    }

    *lineStart = (**gActiveTE).lineStarts[lineIdx];
    if (lineIdx < numLines - 1) {
        short end = (**gActiveTE).lineStarts[lineIdx + 1];
        if (end > *lineStart) {
            Handle hText = (**gActiveTE).hText;
            HLock(hText);
            if ((*hText)[end - 1] == '\r') {
                end--;
            }
            HUnlock(hText);
        }
        *lineEnd = end;
    } else {
        *lineEnd = (**gActiveTE).teLength;
    }
}

static void GetCurrentParagraphRange(short *paraStart, short *paraEnd)
{
    short caretPos = (**gActiveTE).selEnd;
    Handle hText = (**gActiveTE).hText;
    short len = (**gActiveTE).teLength;
    short start = caretPos;
    short end = caretPos;

    HLock(hText);
    while (start > 0 && (*hText)[start - 1] != '\r') {
        start--;
    }
    while (end < len && (*hText)[end] != '\r') {
        end++;
    }
    HUnlock(hText);

    *paraStart = start;
    *paraEnd = end;
}


static void EventLoop(void)
{
    EventRecord event;
    WindowPtr w;
    short part;

    while (!gDone) {
        if (WaitNextEvent(everyEvent, &event, 15, NULL)) {
            /* Disposing a dialog/window doesn't restore the caller's port
               -- cheap insurance against any path (found or not) leaving
               thePort dangling at a freed window's memory. */
#ifndef ARTFUL_PRO
            SetPort(gWindow);
#else
            w = FrontWindow();
            if (w) SetPort(w);
            SetActiveDocument(GetDocumentForWindow(w));
#endif
            switch (event.what) {
                case updateEvt:
                    w = (WindowPtr) (long) event.message;
#ifdef ARTFUL_PRO
                    {
                        GrafPtr savedPort;
                        DocumentRecord *savedDoc = gActiveDoc;
                        GetPort(&savedPort);
                        SetPort(w);
                        SetActiveDocument(GetDocumentForWindow(w));
                        if (gActiveDoc) DoUpdate(w);
                        SetActiveDocument(savedDoc);
                        SetPort(savedPort);
                    }
#else
                    SetPort(w);
                    DoUpdate(w);
#endif
                    break;

                case mouseDown:
                    part = FindWindow(event.where, &w);
#ifdef ARTFUL_PRO
                    if (w) SetActiveDocument(GetDocumentForWindow(w));
                    if (part == inSysWindow) {
                        SystemClick(&event, w);
                    } else if (part == inDrag) {
                        Rect dragRect = qd.screenBits.bounds;
                        DragWindow(w, event.where, &dragRect);
                    } else if (part == inGoAway) {
                        if (TrackGoAway(w, event.where)) {
                            if (ConfirmDiscardChanges()) {
                                DisposeDocument(gActiveDoc);
                                DisposeWindow(w);
                                SetActiveDocument(GetDocumentForWindow(FrontWindow()));
                            }
                        }
                    } else if (part == inMenuBar) {
                        SetActiveDocument(GetDocumentForWindow(FrontWindow()));
                        UpdateEditMenuState();
#ifdef ARTFUL_PRO
                        UpdateWindowMenu();
#endif
                        DoMenuCommand(MenuSelect(event.where));
                    } else if (part == inContent) {
                        if (w != FrontWindow()) {
                            SelectWindow(w);
                        } else if (gActiveDoc) {
                            ControlHandle hitControl;
                            SetPort(w);
                            GlobalToLocal(&event.where);
                            
                            /* Check custom top-middle buttons first */
                            short centerX = (w->portRect.right - w->portRect.left) / 2;
                            short startX = centerX - 56;
                            Rect btnB, btnI, btnView;
                            
                            SetRect(&btnB, startX, 2, startX + 25, 22);
                            SetRect(&btnI, startX + 31, 2, startX + 56, 22);
                            SetRect(&btnView, startX + 62, 2, startX + 112, 22);
                            
                            if (PtInRect(event.where, &btnB)) {
                                InvertRoundRect(&btnB, 6, 6);
                                while (StillDown()) ;
                                InvertRoundRect(&btnB, 6, 6);
                                
                                PushUndoSnapshot();
                                gTypingRunActive = false;
                                gDirty = true;
                                if (gHideMarkdown) {
                                    ToggleFace(bold);
                                } else {
                                    WrapSelection("**", "**");
                                    ClearStyles();
                                }
                                AdjustScrollbar();
                            } else if (PtInRect(event.where, &btnI)) {
                                InvertRoundRect(&btnI, 6, 6);
                                while (StillDown()) ;
                                InvertRoundRect(&btnI, 6, 6);
                                
                                PushUndoSnapshot();
                                gTypingRunActive = false;
                                gDirty = true;
                                if (gHideMarkdown) {
                                    ToggleFace(italic);
                                } else {
                                    WrapSelection("*", "*");
                                    ClearStyles();
                                }
                                AdjustScrollbar();
                            } else if (PtInRect(event.where, &btnView)) {
                                InvertRoundRect(&btnView, 6, 6);
                                while (StillDown()) ;
                                InvertRoundRect(&btnView, 6, 6);
                                
                                SetViewMode(!gHideMarkdown);
                            } else if (FindControl(event.where, w, &hitControl) != 0) {
                                if (hitControl == gScrollBar) {
                                    DoScrollClick(event.where);
                                } else if (hitControl == gJumpToTopBtn) {
                                    if (TrackControl(gJumpToTopBtn, event.where, NULL) == inButton) {
                                        HandleJumpToTop();
                                    }
                                } else if (hitControl == gJumpToEndBtn) {
                                    if (TrackControl(gJumpToEndBtn, event.where, NULL) == inButton) {
                                        HandleJumpToEnd();
                                    }
                                }
                            } else {
                                gTypingRunActive = false;
                                TEClick(event.where, (event.modifiers & shiftKey) != 0, gActiveTE);
                            }
                        }
                    }
#else
                    if (part == inMenuBar) {
                        UpdateEditMenuState();
                        DoMenuCommand(MenuSelect(event.where));
                    } else if (part == inContent) {
                        ControlHandle hitControl;

                        SetPort(w);
                        GlobalToLocal(&event.where);
                        
                        /* Check custom top-middle buttons first */
                        short centerX = (w->portRect.right - w->portRect.left) / 2;
                        short startX = centerX - 56;
                        Rect btnB, btnI, btnView;
                        
                        SetRect(&btnB, startX, 2, startX + 25, 22);
                        SetRect(&btnI, startX + 31, 2, startX + 56, 22);
                        SetRect(&btnView, startX + 62, 2, startX + 112, 22);
                        
                        if (PtInRect(event.where, &btnB)) {
                            InvertRoundRect(&btnB, 6, 6);
                            while (StillDown()) ;
                            InvertRoundRect(&btnB, 6, 6);
                            
                            PushUndoSnapshot();
                            gTypingRunActive = false;
                            gDirty = true;
                            if (gHideMarkdown) {
                                ToggleFace(bold);
                            } else {
                                WrapSelection("**", "**");
                                ClearStyles();
                            }
                            AdjustScrollbar();
                        } else if (PtInRect(event.where, &btnI)) {
                            InvertRoundRect(&btnI, 6, 6);
                            while (StillDown()) ;
                            InvertRoundRect(&btnI, 6, 6);
                            
                            PushUndoSnapshot();
                            gTypingRunActive = false;
                            gDirty = true;
                            if (gHideMarkdown) {
                                ToggleFace(italic);
                            } else {
                                WrapSelection("*", "*");
                                ClearStyles();
                            }
                            AdjustScrollbar();
                        } else if (PtInRect(event.where, &btnView)) {
                            InvertRoundRect(&btnView, 6, 6);
                            while (StillDown()) ;
                            InvertRoundRect(&btnView, 6, 6);
                            
                            SetViewMode(!gHideMarkdown);
                        } else if (FindControl(event.where, w, &hitControl) != 0) {
                            if (hitControl == gScrollBar) {
                                DoScrollClick(event.where);
                            } else if (hitControl == gJumpToTopBtn) {
                                if (TrackControl(gJumpToTopBtn, event.where, NULL) == inButton) {
                                    HandleJumpToTop();
                                }
                            } else if (hitControl == gJumpToEndBtn) {
                                if (TrackControl(gJumpToEndBtn, event.where, NULL) == inButton) {
                                    HandleJumpToEnd();
                                }
                            }
                        } else {
                            gTypingRunActive = false;
                            TEClick(event.where, (event.modifiers & shiftKey) != 0, gActiveTE);
                        }
                    }
#endif
                    break;

                case keyDown:
                case autoKey: {
#ifdef ARTFUL_PRO
                    SetActiveDocument(GetDocumentForWindow(FrontWindow()));
                    if (!gActiveDoc && (event.modifiers & cmdKey) == 0) break;
#endif
                    /* User pressed a key: keyboard navigation is now in control.
                       Clear the scrollbar-driven flag so ScrollCaretIntoView can
                       do backward window shifts again if the caret reaches an edge. */
                    gScrollbarDriven = false;
                    
                    char key = event.message & charCodeMask;
                    char keyCode = (event.message & keyCodeMask) >> 8;
                    Boolean isContentKey = (key < 0x1C || key > 0x1F);
                    Boolean handled = false;

                    if (keyCode == 0x75) { /* Del (Forward Delete) */
                        short selStart = (**gActiveTE).selStart;
                        short selEnd = (**gActiveTE).selEnd;

                        PushUndoSnapshot();
                        gTypingRunActive = false;

                        if (selStart == selEnd) {
                            if (selStart < (**gActiveTE).teLength) {
                                TESetSelect(selStart, selStart + 1, gActiveTE);
                                TEDelete(gActiveTE);
                            }
                         } else {
                            TEDelete(gActiveTE);
                        }
                        gDirty = true;
                        ScrollCaretIntoView(false);
                        UpdateScrollbarRange();
                        handled = true;
                    } else if (keyCode == 0x73) { /* Home */
                        TESetSelect(0, 0, gActiveTE);
                        ScrollCaretIntoView(true);
                        handled = true;
                    } else if (keyCode == 0x77) { /* End */
                        short len = (**gActiveTE).teLength;
                        TESetSelect(len, len, gActiveTE);
                        ScrollCaretIntoView(false);
                        handled = true;
                    } else if ((event.modifiers & cmdKey) && keyCode == 0x7B) { /* Cmd+Cursor Left */
                        short lineStart, lineEnd;
                        GetCurrentLineRange(&lineStart, &lineEnd);
                        TESetSelect(lineStart, lineStart, gActiveTE);
                        ScrollCaretIntoView(true);
                        handled = true;
                    } else if ((event.modifiers & cmdKey) && keyCode == 0x7C) { /* Cmd+Cursor Right */
                        short lineStart, lineEnd;
                        GetCurrentLineRange(&lineStart, &lineEnd);
                        TESetSelect(lineEnd, lineEnd, gActiveTE);
                        ScrollCaretIntoView(false);

                        handled = true;
                    }

                    if (handled) {
                        /* Already handled */
                    } else if (event.modifiers & cmdKey) {
                        if (event.what == keyDown) {
                            if ((key == 'z' || key == 'Z') && (event.modifiers & shiftKey))
                                DoRedo();
                            else {
                                UpdateEditMenuState();
                                DoMenuCommand(MenuKey(key));
                            }
                        }
                    } else {
                        Boolean isArrowKey = (!isContentKey && key >= 0x1C && key <= 0x1F);
                        if (isContentKey) {
                            if (!gTypingRunActive) {
                                PushUndoSnapshot();
                                gTypingRunActive = true;
                            }
                        } else {
                            gTypingRunActive = false;
                        }

                        if (isArrowKey && (event.modifiers & shiftKey)) {
                            short activeEnd, newPos;
                            if (!gShiftSelectionActive) {
                                gShiftSelectionActive = true;
                                gShiftAnchor = (**gActiveTE).selStart; 
                            }
                            activeEnd = (gShiftAnchor == (**gActiveTE).selStart) ? (**gActiveTE).selEnd : (**gActiveTE).selStart;
                            TESetSelect(activeEnd, activeEnd, gActiveTE);
                            
                            TEKey(key, gActiveTE);
                            newPos = (**gActiveTE).selStart;
                            
                            if (newPos < gShiftAnchor) {
                                TESetSelect(newPos, gShiftAnchor, gActiveTE);
                            } else {
                                TESetSelect(gShiftAnchor, newPos, gActiveTE);
                            }
                        } else {
                            if (isArrowKey || key == 0x09 || key == 0x0D || key == 0x08 || isContentKey) {
                                gShiftSelectionActive = false;
                            }
                            if (key == 0x0D) {
                                short lineStart, lineEnd, caret;
                                Handle hText = (**gActiveTE).hText;
                                short prefixLen = 0;
                                char prefixBuf[16];
                                Boolean doInsertCR = true;
                                
                                caret = (**gActiveTE).selStart;
                                GetCurrentParagraphRange(&lineStart, &lineEnd);
                                
                                HLock(hText);
                                if (gHideMarkdown) {
                                    if (lineStart < caret && (unsigned char)(*hText)[lineStart] == 0xA5 && 
                                        lineStart + 1 < caret && (*hText)[lineStart + 1] == ' ') {
                                        prefixLen = 2;
                                        BlockMove(*hText + lineStart, prefixBuf, prefixLen);
                                    } else {
                                        short i = lineStart;
                                        while (i < caret && (*hText)[i] >= '0' && (*hText)[i] <= '9') i++;
                                        if (i > lineStart && i < caret && (*hText)[i] == '.' && i + 1 < caret && (*hText)[i + 1] == ' ') {
                                            prefixLen = (i + 2) - lineStart;
                                            BlockMove(*hText + lineStart, prefixBuf, prefixLen);
                                        }
                                    }
                                } else {
                                    if (lineStart < caret && (*hText)[lineStart] == '-' && 
                                        lineStart + 1 < caret && (*hText)[lineStart + 1] == ' ') {
                                        prefixLen = 2;
                                        BlockMove(*hText + lineStart, prefixBuf, prefixLen);
                                    } else {
                                        short i = lineStart;
                                        while (i < caret && (*hText)[i] >= '0' && (*hText)[i] <= '9') i++;
                                        if (i > lineStart && i < caret && (*hText)[i] == '.' && i + 1 < caret && (*hText)[i + 1] == ' ') {
                                            prefixLen = (i + 2) - lineStart;
                                            BlockMove(*hText + lineStart, prefixBuf, prefixLen);
                                        }
                                    }
                                }
                                HUnlock(hText);
                                
                                if (prefixLen > 0) {
                                    if (lineEnd == lineStart + prefixLen) {
                                        TESetSelect(lineStart, lineEnd, gActiveTE);
                                        TEDelete(gActiveTE);
                                        doInsertCR = false;
                                        gDirty = true;
                                    } else {
                                        Boolean isNumbered = false;
                                        short i;
                                        for (i = 0; i < prefixLen; i++) {
                                            if (prefixBuf[i] >= '0' && prefixBuf[i] <= '9') isNumbered = true;
                                        }
                                        if (isNumbered) {
                                            long num;
                                            char newNumBuf[16];
                                            prefixBuf[prefixLen-2] = 0;
                                            sscanf(prefixBuf, "%ld", &num);
                                            sprintf(newNumBuf, "%ld. ", num + 1);
                                            prefixLen = strlen(newNumBuf);
                                            BlockMove(newNumBuf, prefixBuf, prefixLen);
                                        }
                                    }
                                }
                                
                                if (doInsertCR) {
                                    TEKey(key, gActiveTE);
                                    if (prefixLen > 0) {
                                        TEInsert(prefixBuf, prefixLen, gActiveTE);
                                    }
                                }
                            } else {
                                TEKey(key, gActiveTE);
                            }
                            
                            if (isContentKey) {
                                short caret;
                                gDirty = true;
                                if (gHideMarkdown)
                                    DetectInlineMarkdown(key);
                                
                                caret = (**gActiveTE).selStart;
                                if (caret >= 5) {
                                    Handle hText = (**gActiveTE).hText;
                                    Boolean isToday = false, isTime = false;
                                    HLock(hText);
                                    if (caret >= 6) {
                                        isToday = (memcmp(*hText + caret - 6, "@today", 6) == 0);
                                    }
                                    if (!isToday) {
                                        isTime = (memcmp(*hText + caret - 5, "@time", 5) == 0);
                                    }
                                    HUnlock(hText);
                                    if (isToday) {
                                        unsigned long secs;
                                        DateTimeRec date;
                                        Str255 dateStr;
                                        char tempBuf[16];
                                        GetDateTime(&secs);
                                        SecondsToDate(secs, &date);
                                        sprintf(tempBuf, "%04d-%02d-%02d", date.year, date.month, date.day);
                                        dateStr[0] = strlen(tempBuf);
                                        BlockMove(tempBuf, dateStr + 1, dateStr[0]);
                                        
                                        TESetSelect(caret - 6, caret, gActiveTE);
                                        TEDelete(gActiveTE);
                                        TEInsert(dateStr + 1, dateStr[0], gActiveTE);
                                    } else if (isTime) {
                                        unsigned long secs;
                                        DateTimeRec date;
                                        Str255 timeStr;
                                        char tempBuf[16];
                                        GetDateTime(&secs);
                                        SecondsToDate(secs, &date);
                                        sprintf(tempBuf, "%02d:%02d", date.hour, date.minute);
                                        timeStr[0] = strlen(tempBuf);
                                        BlockMove(tempBuf, timeStr + 1, timeStr[0]);
                                        
                                        TESetSelect(caret - 5, caret, gActiveTE);
                                        TEDelete(gActiveTE);
                                        TEInsert(timeStr + 1, timeStr[0], gActiveTE);
                                    }
                                }

                                if (gHideMarkdown && caret >= 3) {
                                    Handle hText = (**gActiveTE).hText;
                                    Boolean isDashes;
                                    short lineStart, lineEnd;
                                    
                                    GetCurrentParagraphRange(&lineStart, &lineEnd);
                                    HLock(hText);
                                    isDashes = (caret - lineStart == 3 && caret == lineEnd && memcmp(*hText + lineStart, "---", 3) == 0);
                                    HUnlock(hText);
                                    
                                    if (isDashes) {
                                        TextStyle opStyle;
                                        opStyle.tsColor.red = 0;
                                        opStyle.tsColor.green = 0;
                                        opStyle.tsColor.blue = 0;
                                        opStyle.tsFace = normal;
                                        opStyle.tsSize = 0;

                                        TESetSelect(lineStart, caret, gActiveTE);
                                        TEDelete(gActiveTE);
                                        TEInsert("--------------------", 20, gActiveTE);
                                        
                                        opStyle.tsFace = bold;
                                        opStyle.tsColor.blue = 1;
                                        TESetSelect(lineStart, lineStart + 20, gActiveTE);
                                        TESetStyle(doFace + doColor, &opStyle, false, gActiveTE);
                                        
                                        TESetSelect(lineStart + 20, lineStart + 20, gActiveTE);
                                        opStyle.tsFace = normal;
                                        opStyle.tsColor.blue = 0;
                                        TESetStyle(doFace + doColor, &opStyle, false, gActiveTE);
                                    }
                                }
                            }
                        }
                        ScrollCaretIntoView(key == 0x1E || key == 0x1C || key == 0x08);
                        UpdateScrollbarRange();
                    }

                    break;
                }

                case activateEvt:
#ifdef ARTFUL_PRO
                    SetActiveDocument(GetDocumentForWindow((WindowPtr) event.message));
                    if (gActiveDoc) {
                        if ((event.modifiers & activeFlag) != 0) {
                            SetPort(gWindow);
                            TEActivate(gActiveTE);
                            InvalRect(&gWindow->portRect); /* Force redraw when coming to front */
                        } else {
                            TEDeactivate(gActiveTE);
                        }
                    }
#else
                    if ((event.modifiers & activeFlag) != 0) {
                        TEActivate(gActiveTE);
                        InvalRect(&gWindow->portRect);
                    } else {
                        TEDeactivate(gActiveTE);
                    }
#endif
                    break;
            }
        }
#ifdef ARTFUL_PRO
        SetActiveDocument(GetDocumentForWindow(FrontWindow()));
        if (gActiveDoc) {
            TEIdle(gActiveTE);
            UpdateStatusBar(FrontWindow(), false);
        }
#else
        TEIdle(gActiveTE);
        UpdateStatusBar(gWindow, false);
#endif
    }
}

short GetDefaultFontNum(void)
{
    short fontNum;
    if (gUseSansSerif) {
        GetFNum("\pHelvetica", &fontNum);
    } else {
        GetFNum("\pTimes", &fontNum);
    }
    return fontNum;
}

void SetFontMode(Boolean useSans)
{
    gUseSansSerif = useSans;
    CheckItem(gViewMenu, iSerif, !useSans);
    CheckItem(gViewMenu, iSansSerif, useSans);

#ifdef ARTFUL_PRO
    if (!gActiveDoc) return;
#endif

    if (gHideMarkdown) {
        SyncHiddenToCanonical();
        BuildHiddenView();
        ClearStyles();
    } else {
        ClearStyles();
        BuildHiddenView();
    }

    AdjustScrollbar();
    InvalRect(&gWindow->portRect);
}

int main(void)
{
    short message, count;

    Init();
    LoadZoomPref();
    MakeMenu();
    MakeWindow();

    /* A newly-created visible window has its whole content area marked
       invalid automatically, but the splash dialog appears before the
       event loop ever gets a chance to dequeue and process that update
       event -- force the real BeginUpdate/TEUpdate/EndUpdate cycle to
       happen now, so the window has gone through one proper paint before
       the user can type anything. Without this, the very first line typed
       (before any other update has occurred) doesn't render reliably. */
    DoUpdate(gWindow);

    CountAppFiles(&message, &count);
    if (count >= 1 && message == appOpen)
        DoStartupOpen();
    else
        ShowSplashScreen();

    EventLoop();
    return 0;
}

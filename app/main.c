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

// Memory pool for handle allocations to reduce overhead
Handle gMemoryPool[MEMORY_POOL_SIZE];
short gMemoryPoolCount = 0;

// Simple memory pool management functions
static Handle GetFromPool(void) {
    if (gMemoryPoolCount > 0) {
        return gMemoryPool[--gMemoryPoolCount];
    }
    return NULL;
}

static void ReturnToPool(Handle h) {
    if (h && gMemoryPoolCount < MEMORY_POOL_SIZE) {
        gMemoryPool[gMemoryPoolCount++] = h;
    }
}

#ifndef ARTFUL_PRO

WindowPtr gWindow;
WEHandle gTE = NULL;
WEHandle gHiddenTE = NULL;
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
WEHandle gActiveTE;
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
        if (doc->te) WEDispose(doc->te);
        if (doc->hiddenTE) WEDispose(doc->hiddenTE);
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
    
    // Initialize memory pool
    for (int i = 0; i < MEMORY_POOL_SIZE; i++) {
        gMemoryPool[i] = NULL;
    }
    gMemoryPoolCount = 0;
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
    AppendMenu(styleMenu, "\pBold/B;Italic/I;Code/K;Strikethrough;Highlight/H;(-;Heading 1/1;Heading 2/2;Heading 3/3;(-;Link/L;(-;None");
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
    
    gWindow = NewCWindow(NULL, &bounds, "\pUntitled", true, documentProc,
                          (WindowPtr) -1L, true, (long)doc);
#else
    bounds = qd.screenBits.bounds;
    bounds.top += MENU_BAR_HEIGHT;

    gWindow = NewCWindow(NULL, &bounds, "\p", true, plainDBox,
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
    WENew(&viewRect, &viewRect, 0, &gTE);

    TextFont(fontNum);
    WENew(&viewRect, &viewRect, 0, &gHiddenTE);
    gActiveTE = gHideMarkdown ? gHiddenTE : gTE;

#ifdef ARTFUL_PRO
    if (gActiveDoc) {
        gActiveDoc->te = gTE;
        gActiveDoc->hiddenTE = gHiddenTE;
        gActiveDoc->window = gWindow;
    }
#endif

    WEActivate(gActiveTE);

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
    long selStart, selEnd;
    WEGetSelection(&selStart, &selEnd, gActiveTE);
    caret = selStart;

    line = 1;
    col = 1;
    {
        // Optimized line/column calculation using cached information
        long newlines = 0;
        Handle hText = WEGetText(gActiveTE);
        HLock(hText);
        
        // Use a more efficient approach - only scan to the current line
        long scan = 0;
        long caretPos = caret;
        while (scan < caretPos) {
            if ((*hText)[scan] == '\r') {
                newlines++;
            }
            scan++;
        }
        
        // Calculate column position efficiently
        long scanCol = caretPos;
        while (scanCol > 0 && (*hText)[scanCol - 1] != '\r') {
            scanCol--;
            col++;
        }
        HUnlock(hText);
        line = (short)((long)gWindowStartLine + newlines);
    }


    if (!forceDraw && chars == gLastCharCount && line == gLastLine && col == gLastCol && gHideMarkdown == lastMode)
        return;

    // Cache the current values for next comparison
    gLastCharCount = chars;
    gLastLine = line;
    gLastCol = col;
    lastMode = gHideMarkdown;

    statusRect = w->portRect;
    
    GetPort(&savedPort);
    SetPort(w);

    LongRect viewRect;
    WEGetViewRect(&viewRect, gActiveTE);
    statusRect.left = viewRect.left;
    statusRect.right = statusRect.left + 300;
    statusRect.top = viewRect.bottom;
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
    WEUpdate(&w->portRect, gActiveTE);
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
                long selStart, selEnd;
                WEGetSelection(&selStart, &selEnd, gActiveTE);
                long currentGlobalPos = gWindowStart + selEnd;
                if (FindTextInHandle(backingH, *backingLenPtr, target, currentGlobalPos, &matchStart, &matchEnd)) {
                    if (matchStart < gWindowStart || matchEnd > gWindowEnd) {
                        LoadTextWindow(matchStart);
                    }
                    
                    long localStart = matchStart - gWindowStart;
                    long localEnd = matchEnd - gWindowStart;
                    WESetSelect(localStart, localEnd, gActiveTE);
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
                    long selStart, selEnd;
                    WEGetSelection(&selStart, &selEnd, gActiveTE);
                    long currentGlobalPos = gWindowStart + selStart;
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
                        
                        long localStart = matchStart - gWindowStart;
                        long localEnd = localStart + replaceWith[0];
                        WESetSelect(localStart, localEnd, gActiveTE);
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
                case iStrike: ToggleStrike(); break;
                case iHighlight: ToggleFace(outline); break;
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
                case iHighlight: WrapSelection("==", "=="); break;
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
    long selStart, selEnd;
    WEGetSelection(&selStart, &selEnd, gActiveTE);
    long lineIdx = WEOffsetToLine(selEnd, gActiveTE);
    long start = 0, end = 0;
    if (WEGetLineRange(lineIdx, &start, &end, gActiveTE) == noErr) {
        if (end > start) {
            Handle hText = WEGetText(gActiveTE);
            HLock(hText);
            if ((*hText)[end - 1] == '\r') {
                end--;
            }
            HUnlock(hText);
        }
        *lineStart = (short) start;
        *lineEnd = (short) end;
    } else {
        *lineStart = 0;
        *lineEnd = 0;
    }
}

static void GetCurrentParagraphRange(short *paraStart, short *paraEnd)
{
    long selStart, selEnd;
    WEGetSelection(&selStart, &selEnd, gActiveTE);
    long caretPos = selEnd;
    Handle hText = WEGetText(gActiveTE);
    long len = WEGetTextLength(gActiveTE);
    long start = caretPos;
    long end = caretPos;

    HLock(hText);
    while (start > 0 && (*hText)[start - 1] != '\r') {
        start--;
    }
    while (end < len && (*hText)[end] != '\r') {
        end++;
    }
    HUnlock(hText);

    *paraStart = (short) start;
    *paraEnd = (short) end;
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
                                WEClick(event.where, (event.modifiers & shiftKey) != 0, gActiveTE);
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
                            WEClick(event.where, (event.modifiers & shiftKey) != 0, gActiveTE);
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
                        long selStart, selEnd;
                        WEGetSelection(&selStart, &selEnd, gActiveTE);

                        PushUndoSnapshot();
                        gTypingRunActive = false;

                        if (selStart == selEnd) {
                            if (selStart < WEGetTextLength(gActiveTE)) {
                                WESetSelect(selStart, selStart + 1, gActiveTE);
                                WEDelete(gActiveTE);
                            }
                         } else {
                            WEDelete(gActiveTE);
                        }
                        gDirty = true;
                        ScrollCaretIntoView(false);
                        UpdateScrollbarRange();
                        handled = true;
                    } else if (keyCode == 0x73) { /* Home */
                        short lineStart, lineEnd;
                        GetCurrentLineRange(&lineStart, &lineEnd);
                        
                        long scan = lineStart;
                        Handle hText = WEGetText(gActiveTE);
                        HLock(hText);
                        while (scan < lineEnd && ((*hText)[scan] == ' ' || (*hText)[scan] == '\t')) {
                            scan++;
                        }
                        HUnlock(hText);
                        
                        WESetSelect(scan, scan, gActiveTE);
                        ScrollCaretIntoView(true);
                        handled = true;
                    } else if (keyCode == 0x77) { /* End */
                        short lineStart, lineEnd;
                        GetCurrentLineRange(&lineStart, &lineEnd);
                        WESetSelect(lineEnd, lineEnd, gActiveTE);
                        ScrollCaretIntoView(false);
                        handled = true;
                    } else if ((event.modifiers & cmdKey) && keyCode == 0x7B) { /* Cmd+Cursor Left */
                        short lineStart, lineEnd;
                        GetCurrentLineRange(&lineStart, &lineEnd);
                        WESetSelect(lineStart, lineStart, gActiveTE);
                        ScrollCaretIntoView(true);
                        handled = true;
                    } else if ((event.modifiers & cmdKey) && keyCode == 0x7C) { /* Cmd+Cursor Right */
                        short lineStart, lineEnd;
                        GetCurrentLineRange(&lineStart, &lineEnd);
                        WESetSelect(lineEnd, lineEnd, gActiveTE);
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
                            long activeEnd, newPos;
                            long selStart, selEnd;
                            WEGetSelection(&selStart, &selEnd, gActiveTE);
                            if (!gShiftSelectionActive) {
                                gShiftSelectionActive = true;
                                gShiftAnchor = selStart; 
                            }
                            activeEnd = (gShiftAnchor == selStart) ? selEnd : selStart;
                            WESetSelect(activeEnd, activeEnd, gActiveTE);
                            
                            WEKey(key, keyCode, event.modifiers, gActiveTE);
                            WEGetSelection(&selStart, &selEnd, gActiveTE);
                            newPos = selStart;
                            
                            if (newPos < gShiftAnchor) {
                                WESetSelect(newPos, gShiftAnchor, gActiveTE);
                            } else {
                                WESetSelect(gShiftAnchor, newPos, gActiveTE);
                            }
                        } else {
                            if (isArrowKey || key == 0x09 || key == 0x0D || key == 0x08 || isContentKey) {
                                gShiftSelectionActive = false;
                            }
                            if (key == 0x0D) {
                                short lineStart, lineEnd, caret;
                                Handle hText = WEGetText(gActiveTE);
                                short prefixLen = 0;
                                char prefixBuf[64];
                                Boolean doInsertCR = true;
                                
                                long selStart, selEnd;
                                WEGetSelection(&selStart, &selEnd, gActiveTE);
                                caret = (short) selStart;
                                GetCurrentParagraphRange(&lineStart, &lineEnd);
                                
                                HLock(hText);
                                short scan = lineStart;
                                while (scan < caret && ((*hText)[scan] == ' ' || (*hText)[scan] == '\t')) scan++;
                                
                                if (gHideMarkdown) {
                                    if (scan < caret && ((unsigned char)(*hText)[scan] == 0xA5 || (*hText)[scan] == 'o' || (*hText)[scan] == '-') && 
                                        scan + 1 < caret && (*hText)[scan + 1] == ' ') {
                                        prefixLen = (scan + 2) - lineStart;
                                    }
                                } else {
                                    if (scan < caret && (*hText)[scan] == '-' && 
                                        scan + 1 < caret && (*hText)[scan + 1] == ' ') {
                                        prefixLen = (scan + 2) - lineStart;
                                    }
                                }
                                
                                if (prefixLen == 0) {
                                    short i = scan;
                                    while (i < caret && (*hText)[i] >= '0' && (*hText)[i] <= '9') i++;
                                    if (i > scan && i < caret && (*hText)[i] == '.' && i + 1 < caret && (*hText)[i + 1] == ' ') {
                                        prefixLen = (i + 2) - lineStart;
                                    }
                                }
                                
                                if (prefixLen == 0) {
                                    short i = scan;
                                    while (i < caret && (*hText)[i] == '>') i++;
                                    if (i > scan && i < caret && (*hText)[i] == ' ') {
                                        prefixLen = (i + 1) - lineStart;
                                    }
                                }
                                
                                if (prefixLen == 0) {
                                    if (gHideMarkdown) {
                                        if (scan + 3 < caret && (*hText)[scan] == '[' && ((*hText)[scan + 1] == ' ' || (*hText)[scan + 1] == 'x' || (*hText)[scan + 1] == 'X') && (*hText)[scan + 2] == ']' && (*hText)[scan + 3] == ' ') {
                                            prefixLen = (scan + 4) - lineStart;
                                        }
                                    } else {
                                        if (scan + 5 < caret && ((*hText)[scan] == '-' || (*hText)[scan] == '+' || (*hText)[scan] == '*') && (*hText)[scan + 1] == ' ' &&
                                            (*hText)[scan + 2] == '[' && ((*hText)[scan + 3] == ' ' || (*hText)[scan + 3] == 'x' || (*hText)[scan + 3] == 'X') && (*hText)[scan + 4] == ']' && (*hText)[scan + 5] == ' ') {
                                            prefixLen = (scan + 6) - lineStart;
                                        }
                                    }
                                }
                                
                                if (prefixLen > 0) {
                                    if (prefixLen > 63) prefixLen = 63;
                                    BlockMove(*hText + lineStart, prefixBuf, prefixLen);
                                }
                                HUnlock(hText);
                                
                                if (prefixLen > 0) {
                                    if (lineEnd == lineStart + prefixLen) {
                                        short spaceCount = scan - lineStart;
                                        Boolean isQuote = prefixBuf[spaceCount] == '>';
                                        if (isQuote) {
                                            short quoteCount = 0;
                                            while (prefixBuf[spaceCount + quoteCount] == '>') quoteCount++;
                                            
                                            if (quoteCount > 1) {
                                                char newBuf[64];
                                                short newPrefixLen = prefixLen - 1;
                                                BlockMove(prefixBuf, newBuf, spaceCount);
                                                BlockMove(prefixBuf + spaceCount + 1, newBuf + spaceCount, prefixLen - spaceCount - 1);
                                                
                                                WESetSelect(lineStart, lineEnd, gActiveTE);
                                                WEDelete(gActiveTE);
                                                WESetSelect(lineStart, lineStart, gActiveTE);
                                                WEInsert(newBuf, newPrefixLen, NULL, gActiveTE);
                                                doInsertCR = false;
                                                gDirty = true;
                                            } else {
                                                WESetSelect(lineStart, lineEnd, gActiveTE);
                                                WEDelete(gActiveTE);
                                                doInsertCR = false;
                                                gDirty = true;
                                            }
                                        } else if (spaceCount >= 2) {
                                            char newBuf[64];
                                            short newPrefixLen = prefixLen - 2;
                                            BlockMove(prefixBuf + 2, newBuf, newPrefixLen);
                                            
                                            Boolean isUnordered = false;
                                            if (gHideMarkdown) {
                                                unsigned char bulletChar = prefixBuf[spaceCount];
                                                if (bulletChar == 0xA5 || bulletChar == 'o' || bulletChar == '-') {
                                                    isUnordered = true;
                                                }
                                            } else {
                                                if (prefixBuf[spaceCount] == '-') {
                                                    isUnordered = true;
                                                }
                                            }
                                            
                                            if (isUnordered) {
                                                short newSpaceCount = spaceCount - 2;
                                                short newNesting = newSpaceCount / 2;
                                                char newBullet = '\245';
                                                if (gHideMarkdown) {
                                                    if (newNesting == 1) newBullet = 'o';
                                                    else if (newNesting >= 2) newBullet = '-';
                                                } else {
                                                    newBullet = '-';
                                                }
                                                newBuf[newSpaceCount] = newBullet;
                                            }
                                            
                                            WESetSelect(lineStart, lineEnd, gActiveTE);
                                            WEDelete(gActiveTE);
                                            WESetSelect(lineStart, lineStart, gActiveTE);
                                            WEInsert(newBuf, newPrefixLen, NULL, gActiveTE);
                                            doInsertCR = false;
                                            gDirty = true;
                                        } else {
                                            WESetSelect(lineStart, lineEnd, gActiveTE);
                                            WEDelete(gActiveTE);
                                            doInsertCR = false;
                                            gDirty = true;
                                        }
                                    } else {
                                        Boolean isNumbered = false;
                                        short i;
                                        for (i = 0; i < prefixLen; i++) {
                                            if (prefixBuf[i] >= '0' && prefixBuf[i] <= '9') isNumbered = true;
                                        }
                                        if (isNumbered) {
                                            long num;
                                            char newNumBuf[64];
                                            short numStart = scan - lineStart;
                                            prefixBuf[prefixLen-2] = 0;
                                            sscanf(prefixBuf + numStart, "%ld", &num);
                                            sprintf(newNumBuf, "%ld. ", num + 1);
                                            
                                            prefixLen = numStart + strlen(newNumBuf);
                                            if (prefixLen > 63) prefixLen = 63;
                                            BlockMove(newNumBuf, prefixBuf + numStart, strlen(newNumBuf));
                                        }
                                    }
                                }
                                
                                if (doInsertCR) {
                                    if (lineEnd == lineStart) {
                                         WETextStyle ts;
                                         WEGetStyle(lineStart, &ts, gActiveTE);
                                         if (ts.tsColor.blue >= 2) {
                                             if (ts.tsColor.blue > 2) {
                                                 ts.tsColor.blue--;
                                             } else {
                                                 ts.tsColor.blue = 0;
                                                 ts.tsFace &= ~italic;
                                             }
                                             WESetSelect(lineStart, lineEnd, gActiveTE);
                                             WESetStyle(weDoFace + weDoColor, &ts, gActiveTE);
                                            doInsertCR = false;
                                            gDirty = true;
                                        }
                                    }
                                }
                                
                                if (doInsertCR) {
                                    WEKey(key, keyCode, event.modifiers, gActiveTE);
                                    if (prefixLen > 0) {
                                        char nextPrefix[64];
                                        BlockMove(prefixBuf, nextPrefix, prefixLen);
                                        if (gHideMarkdown) {
                                            if (prefixLen >= 4 && nextPrefix[prefixLen - 4] == '[' && nextPrefix[prefixLen - 2] == ']') {
                                                nextPrefix[prefixLen - 3] = ' ';
                                            }
                                        } else {
                                            if (prefixLen >= 6 && nextPrefix[prefixLen - 4] == '[' && nextPrefix[prefixLen - 2] == ']') {
                                                nextPrefix[prefixLen - 3] = ' ';
                                            }
                                        }
                                        WEInsert(nextPrefix, prefixLen, NULL, gActiveTE);
                                    }
                                }
                            } else if (key == 0x09) {
                                short lineStart, lineEnd, caret;
                                Handle hText = WEGetText(gActiveTE);
                                short prefixLen = 0;
                                char prefixBuf[64];
                                
                                long selStart, selEnd;
                                WEGetSelection(&selStart, &selEnd, gActiveTE);
                                caret = (short) selStart;
                                GetCurrentParagraphRange(&lineStart, &lineEnd);
                                
                                HLock(hText);
                                short scan = lineStart;
                                while (scan < caret && ((*hText)[scan] == ' ' || (*hText)[scan] == '\t')) scan++;
                                
                                if (gHideMarkdown) {
                                    if (scan < caret && ((unsigned char)(*hText)[scan] == 0xA5 || (*hText)[scan] == 'o' || (*hText)[scan] == '-') && 
                                        scan + 1 < caret && (*hText)[scan + 1] == ' ') {
                                        prefixLen = (scan + 2) - lineStart;
                                    }
                                } else {
                                    if (scan < caret && (*hText)[scan] == '-' && 
                                        scan + 1 < caret && (*hText)[scan + 1] == ' ') {
                                        prefixLen = (scan + 2) - lineStart;
                                    }
                                }
                                
                                if (prefixLen == 0) {
                                    short i = scan;
                                    while (i < caret && (*hText)[i] >= '0' && (*hText)[i] <= '9') i++;
                                    if (i > scan && i < caret && (*hText)[i] == '.' && i + 1 < caret && (*hText)[i + 1] == ' ') {
                                        prefixLen = (i + 2) - lineStart;
                                    }
                                }
                                
                                if (prefixLen == 0) {
                                    if (gHideMarkdown) {
                                        if (scan + 3 < caret && (*hText)[scan] == '[' && ((*hText)[scan + 1] == ' ' || (*hText)[scan + 1] == 'x' || (*hText)[scan + 1] == 'X') && (*hText)[scan + 2] == ']' && (*hText)[scan + 3] == ' ') {
                                            prefixLen = (scan + 4) - lineStart;
                                        }
                                    } else {
                                        if (scan + 5 < caret && ((*hText)[scan] == '-' || (*hText)[scan] == '+' || (*hText)[scan] == '*') && (*hText)[scan + 1] == ' ' &&
                                            (*hText)[scan + 2] == '[' && ((*hText)[scan + 3] == ' ' || (*hText)[scan + 3] == 'x' || (*hText)[scan + 3] == 'X') && (*hText)[scan + 4] == ']' && (*hText)[scan + 5] == ' ') {
                                            prefixLen = (scan + 6) - lineStart;
                                        }
                                    }
                                }
                                
                                if (prefixLen > 0) {
                                    if (prefixLen > 63) prefixLen = 63;
                                    BlockMove(*hText + lineStart, prefixBuf, prefixLen);
                                }
                                HUnlock(hText);
                                
                                if (prefixLen > 0 && prefixLen + 2 <= 63) {
                                    short bulletOffset = scan - lineStart;
                                    char newBuf[64];
                                    newBuf[0] = ' '; newBuf[1] = ' ';
                                    BlockMove(prefixBuf, newBuf + 2, prefixLen);
                                    
                                    Boolean isUnordered = false;
                                    if (gHideMarkdown) {
                                        unsigned char bulletChar = prefixBuf[bulletOffset];
                                        if (bulletChar == 0xA5 || bulletChar == 'o' || bulletChar == '-') {
                                            isUnordered = true;
                                        }
                                    } else {
                                        if (prefixBuf[bulletOffset] == '-') {
                                            isUnordered = true;
                                        }
                                    }
                                    
                                    if (isUnordered) {
                                        short newSpaceCount = bulletOffset + 2;
                                        short newNesting = newSpaceCount / 2;
                                        char newBullet = '\245';
                                        if (gHideMarkdown) {
                                            if (newNesting == 1) newBullet = 'o';
                                            else if (newNesting >= 2) newBullet = '-';
                                        } else {
                                            newBullet = '-';
                                        }
                                        newBuf[2 + bulletOffset] = newBullet;
                                    }
                                    
                                    WESetSelect(lineStart, lineStart + prefixLen, gActiveTE);
                                    WEDelete(gActiveTE);
                                    WESetSelect(lineStart, lineStart, gActiveTE);
                                    WEInsert(newBuf, prefixLen + 2, NULL, gActiveTE);
                                    
                                    short newCaret = caret + 2;
                                    WESetSelect(newCaret, newCaret, gActiveTE);
                                    gDirty = true;
                                } else {
                                    WEKey(key, keyCode, event.modifiers, gActiveTE);
                                }
                            } else {
                                WEKey(key, keyCode, event.modifiers, gActiveTE);
                            }
                            
                            if (isContentKey) {
                                short caret;
                                gDirty = true;
                                if (gHideMarkdown)
                                    DetectInlineMarkdown(key);
                                
                                long selStart, selEnd;
                                WEGetSelection(&selStart, &selEnd, gActiveTE);
                                caret = (short) selStart;
                                if (caret >= 5) {
                                    Handle hText = WEGetText(gActiveTE);
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
                                        
                                        WESetSelect(caret - 6, caret, gActiveTE);
                                        WEDelete(gActiveTE);
                                        WEInsert(dateStr + 1, dateStr[0], NULL, gActiveTE);
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
                                        
                                        WESetSelect(caret - 5, caret, gActiveTE);
                                        WEDelete(gActiveTE);
                                        WEInsert(timeStr + 1, timeStr[0], NULL, gActiveTE);
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
                            WEActivate(gActiveTE);
                            InvalRect(&gWindow->portRect); /* Force redraw when coming to front */
                        } else {
                            WEDeactivate(gActiveTE);
                        }
                    }
#else
                    if ((event.modifiers & activeFlag) != 0) {
                        WEActivate(gActiveTE);
                        InvalRect(&gWindow->portRect);
                    } else {
                        WEDeactivate(gActiveTE);
                    }
#endif
                    break;
            }
        }
#ifdef ARTFUL_PRO
        SetActiveDocument(GetDocumentForWindow(FrontWindow()));
        if (gActiveDoc) {
            WEIdle(gActiveTE);
            UpdateStatusBar(FrontWindow(), false);
        }
#else
        WEIdle(gActiveTE);
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

    SyncWindowToBacking();

    if (gHideMarkdown) {
        SyncHiddenToCanonical();
        ClearStyles();
        BuildHiddenView();
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

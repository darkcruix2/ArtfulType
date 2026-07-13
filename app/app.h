#ifndef ARTFULTYPE_APP_H
#define ARTFULTYPE_APP_H

#include <Quickdraw.h>
#include <Windows.h>
#include <Fonts.h>
#include <Menus.h>
#include <TextEdit.h>
#include "WASTE.h"
#include <Dialogs.h>
#include <Events.h>
#include <OSUtils.h>
#include <ToolUtils.h>
#include <Memory.h>
#include <Files.h>
#include <StandardFile.h>
#include <SegLoad.h>
#include <TextUtils.h>
#include <Multiverse.h>


#define MARGIN_H     64
#define MARGIN_TOP   32
#define MARGIN_BOTTOM 24
#define MENU_BAR_HEIGHT 20
#define FONT_SIZE 12
#define SCROLLBAR_WIDTH 16

#define mFile    128
#define iNew     1
#define iOpen    2
#define iSave    3
#define iSaveAs  4
#define iQuit    6

#define mEdit    131
#define iUndo    1
#define iRedo    2
#define iCut     4
#define iCopy    5
#define iPaste   6
#define iSelectAll 8
#define iSearch 10
#define iSearchReplace 11


#define mStyle   129
#define iBold    1
#define iItalic  2
#define iCode    3
#define iStrike  4
#define iHighlight 5
#define iH1      7
#define iH2      8
#define iH3      9
#define iLink    11
#define iNone    13

#define kSaveChangesAlert 130
#define kSaveBtn          1
#define kCancelBtn        2
#define kDontSaveBtn      3

#define kSplashDialog 131
#define iSplashNew    1
#define iSplashOpen   2
#define iSplashTitle  3

#define kLinkDialog  132
#define iLinkOK      1
#define iLinkCancel  2
#define iLinkField   4

#define kAboutDialog 133
#define iAboutOK     1
#define iAboutTitle  2

#define kSearchDialog 134
#define iSearchOK     1
#define iSearchCancel 2
#define iSearchField  4

#define kSearchReplaceDialog 135
#define iReplaceOK           1
#define iReplaceCancel       2
#define iReplaceFindField    4
#define iReplaceWithField    6
#define iReplaceAll          7


#define mView        130
#define iMarkdownView 1
#define iWriterView  2
#define iZoomIn      4
#define iZoomOut     5
#define iZoomDefault 6
#define iSerif       8
#define iSansSerif   9
#define iStatusBar   11

#define mWindow  134

#define mHelp    132
#define iAbout   1

#define MAX_STYLE_OPS 8192

#define kNumZoomLevels 5
#define kZoomBaselineIndex 2
#ifdef ARTFUL_PRO
#define kZoomDefaultIndex 0
#else
#define kZoomDefaultIndex kZoomBaselineIndex
#endif

#define kZoomPrefType 'ZLvl'
#define kZoomPrefID   128

/*
    Undo/redo snapshots store the *canonical markdown text* regardless
    of which mode is active, not gActiveTE's raw buffer -- gHiddenTE's
    styling (bold/heading/link runs) has no simple "get it all, restore
    it all" API in classic styled TextEdit, but canonical markdown text
    already round-trips styling correctly through the existing
    BuildHiddenView/SyncHiddenToCanonical machinery. So: push a
    snapshot by syncing to canonical first (if in Writer mode) and
    copying gTE's text; restore one by replacing gTE's text and, if in
    Writer mode, rebuilding gHiddenTE from it. Both syncing and
    rebuilding are full-document operations, but they only happen at
    undo/redo-relevant moments (pushes are coalesced per typing run,
    not per keystroke), never per character.

    Undo history is intentionally cleared on every view-mode switch
    and on new/open -- simpler and more predictable than trying to
    make snapshots meaningful across two independently-edited buffers.
*/
#define MAX_UNDO_LEVELS 100

typedef struct {
    Handle textH;
    long length;
    short selStart, selEnd;
} UndoSnapshot;

/*
    Link URLs in Writer mode live here, keyed by a small ID (1-based;
    0 means "no link"). The ID rides along in each run's otherwise-unused
    tsColor.red -- TextEdit already tracks style-run boundaries through
    every insert/delete, so the ID (and therefore the URL) follows the
    linked text automatically with no manual range bookkeeping. Reset
    (gLinkCount = 0) at the start of every BuildHiddenView, since that's
    a full reparse of gTE and re-derives whichever links currently exist.
*/
#define MAX_LINKS 512
#define WINDOW_SIZE 4000  /* max chars loaded into the TE at a time for large files */

typedef struct {
    long start, end;
    short kind, level;
    short linkID;
} StyleOp;

/* One-shot flag set during scrollbar-driven window loads to suppress
   the caret-based auto-shift in ScrollCaretIntoView. Global (not per-doc). */
extern Boolean gScrollbarDriven;

/* First line of the document that gWindowStart maps to (1-based, global).
   Computed by LoadTextWindow; used by UpdateStatusBar to show true line numbers. */
extern long gWindowStartLine;

#ifdef ARTFUL_PRO

typedef struct DocumentRecord {
    WindowPtr window;
    WEHandle te;
    WEHandle hiddenTE;
    WEHandle activeTE;
    ControlHandle scrollBar;
    ControlHandle jumpToTopBtn;
    ControlHandle jumpToEndBtn;
    Boolean scrollBarVisible;




    Boolean haveFile;
    Boolean dirty;
    Str255 fileName;
    short vRefNum;
    Boolean hideMarkdown;
    Handle markdownText;
    long markdownLen;
    Handle writerText;
    long writerLen;
    Handle writerOpsH;
    short writerOpCount;
    long windowStart;
    long windowEnd;
    Handle lineOffsetsH;
    long numLines;
    long lastCharCount;
    short lastLine;
    short lastCol;
    Boolean showStatusBar;
    UndoSnapshot undoStack[MAX_UNDO_LEVELS];
    short undoCount;
    UndoSnapshot redoStack[MAX_UNDO_LEVELS];
    short redoCount;
    Boolean typingRunActive;
    Str255 linkURLs[MAX_LINKS + 1];
    short linkCount;
    Boolean shiftSelectionActive;
    short shiftAnchor;
    short zoomIndex;
    struct DocumentRecord *next;
} DocumentRecord;

extern DocumentRecord *gActiveDoc;
extern DocumentRecord *gDocumentList;

#define gWindow (gActiveDoc->window)
#define gTE (gActiveDoc->te)
#define gHiddenTE (gActiveDoc->hiddenTE)
#define gActiveTE (gActiveDoc->activeTE)
#define gScrollBar (gActiveDoc->scrollBar)
#define gJumpToTopBtn (gActiveDoc->jumpToTopBtn)
#define gJumpToEndBtn (gActiveDoc->jumpToEndBtn)
#define gScrollBarVisible (gActiveDoc->scrollBarVisible)




#define gHaveFile (gActiveDoc->haveFile)
#define gDirty (gActiveDoc->dirty)
#define gFileName (gActiveDoc->fileName)
#define gVRefNum (gActiveDoc->vRefNum)
#define gHideMarkdown (gActiveDoc->hideMarkdown)
#define gMarkdownText (gActiveDoc->markdownText)
#define gMarkdownLen (gActiveDoc->markdownLen)
#define gWriterText (gActiveDoc->writerText)
#define gWriterLen (gActiveDoc->writerLen)
#define gWriterOpsH (gActiveDoc->writerOpsH)
#define gWriterOpCount (gActiveDoc->writerOpCount)
#define gWindowStart (gActiveDoc->windowStart)
#define gWindowEnd (gActiveDoc->windowEnd)
#define gLineOffsetsH (gActiveDoc->lineOffsetsH)
#define gNumLines (gActiveDoc->numLines)
#define gLastCharCount (gActiveDoc->lastCharCount)
#define gLastLine (gActiveDoc->lastLine)
#define gLastCol (gActiveDoc->lastCol)
#define gShowStatusBar (gActiveDoc->showStatusBar)
#define gZoomIndex (gActiveDoc->zoomIndex)
#define gUndoStack (gActiveDoc->undoStack)
#define gUndoCount (gActiveDoc->undoCount)
#define gRedoStack (gActiveDoc->redoStack)
#define gRedoCount (gActiveDoc->redoCount)
#define gTypingRunActive (gActiveDoc->typingRunActive)
#define gLinkURLs (gActiveDoc->linkURLs)
#define gLinkCount (gActiveDoc->linkCount)
#define gShiftSelectionActive (gActiveDoc->shiftSelectionActive)
#define gShiftAnchor (gActiveDoc->shiftAnchor)

DocumentRecord* GetDocumentForWindow(WindowPtr w);
DocumentRecord* CreateNewDocument(void);
void DisposeDocument(DocumentRecord *doc);
void SetActiveDocument(DocumentRecord *doc);

#else

/* Global state -- actual storage lives in main.c */
extern WindowPtr gWindow;
extern WEHandle gTE;
extern WEHandle gHiddenTE;
extern Handle gMarkdownText;
extern long gMarkdownLen;
extern Handle gWriterText;
extern long gWriterLen;
extern Handle gWriterOpsH;
extern short gWriterOpCount;
extern ControlHandle gScrollBar;
extern ControlHandle gJumpToTopBtn;
extern ControlHandle gJumpToEndBtn;
extern long gWindowStart;




extern long gWindowEnd;
extern Handle gLineOffsetsH;
extern long gNumLines;
extern long gLastCharCount;
extern short gLastLine;
extern short gLastCol;
extern Boolean gShowStatusBar;
extern WEHandle gActiveTE;
extern ControlHandle gScrollBar;
extern Boolean gScrollBarVisible;
extern Boolean gHaveFile;
extern Boolean gDirty;
extern Str255 gFileName;
extern short gVRefNum;
extern Boolean gHideMarkdown;
extern Boolean gShiftSelectionActive;
extern short gShiftAnchor;
extern UndoSnapshot gUndoStack[MAX_UNDO_LEVELS];
extern short gUndoCount;
extern UndoSnapshot gRedoStack[MAX_UNDO_LEVELS];
extern short gRedoCount;
extern Boolean gTypingRunActive;
extern Str255 gLinkURLs[MAX_LINKS + 1];
extern short gLinkCount;

#endif

extern Boolean gDone;
extern MenuHandle gViewMenu;
extern MenuHandle gEditMenu;
extern MenuHandle gWindowMenu;
#ifndef ARTFUL_PRO
extern short gZoomIndex;
#endif
extern short gDefaultZoomIndex;
extern Boolean gUseSansSerif;

/* main.c */
void MakeWindow(void);
void UpdateMenuBarLook(void);
short GetDefaultFontNum(void);
void SetFontMode(Boolean useSans);

/* scrolling.c */
void UpdateScrollbarRange(void);
void AdjustScrollbar(void);
void ScrollCaretIntoView(Boolean movingBackward);
void HandleJumpToTop(void);
void HandleJumpToEnd(void);




void SuppressDrawing(WEHandle te, Rect *saved);
void RestoreDrawing(WEHandle te, Rect *saved);
long TotalLength(void);
short CurrentFontSize(void);
void DoScrollClick(Point pt);
void InvalidateHeightCache(void);

/* markdown.c */
void ClearStyles(void);
void BuildHiddenView(void);
void SyncHiddenToCanonical(void);
Handle EncodeSelectionAsMarkdown(long start, long end, WEHandle te);
void InsertMarkdownAsStyled(Handle srcH, long srcLen, WEHandle te);
void WrapSelection(char *prefix, char *suffix);
void ApplyHeading(short level);
void DoLink(void);
void ToggleFace(Style face);
void DoLinkHidden(void);
void ToggleCode(void);
void ToggleStrike(void);
void ToggleHeadingHidden(short level);
void DetectInlineMarkdown(char justTyped);
void ClearSelectionStyleHidden(void);
void ClearMarkdownInSelection(void);
short AddLinkURL(const unsigned char *url);
void LoadTextWindow(long startOffset);
void SyncWindowToBacking(void);

/* undo.c */
void ClearUndoRedoStacks(void);
void UpdateEditMenuState(void);
void PushUndoSnapshot(void);
void DoUndo(void);
void DoRedo(void);
void DoCut(void);
void DoCopy(void);
void DoPaste(void);
void DoSelectAll(void);

/* zoom.c */
short CurrentFontSize(void);
void LoadZoomPref(void);
void DoZoom(short direction);
void DoZoomReset(void);

/* file.c */
void SetViewMode(Boolean hideMarkdown);
void DoStartupOpen(void);
Boolean DoSaveAs(void);
Boolean DoSave(void);
Boolean ConfirmDiscardChanges(void);
Boolean DoOpenFile(void);
void DoNewFile(void);

/* splash.c */
void ShowSplashScreen(void);
void ShowAboutBox(void);

#endif

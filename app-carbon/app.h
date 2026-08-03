/*
 * app.h  — ArtfulType Pro  (Carbon / OS X 10.4 port)
 *
 * Derived from the classic Retro68 app.h; updated to:
 *   • Use Carbon/OS X headers instead of Retro68-only headers
 *   • Replace WEHandle (WASTE/TEHandle) with the TXNCompat shim
 *   • Replace desk-accessory, CountAppFiles, and qd.xxx references
 *   • Keep all constants, menu IDs, document record layout, and
 *     function prototypes identical to the original so the .c files
 *     need only minimal mechanical changes.
 */

#ifndef ARTFULTYPE_APP_H
#define ARTFULTYPE_APP_H

#include "carbon_compat.h"   /* <Carbon/Carbon.h> + compatibility aliases */
#include "TXNCompat.h"       /* WEHandle = TXNObject shim                 */
#include <stdio.h>
#include <string.h>

/* -----------------------------------------------------------------------
   Retro68 compatibility aliases still used in the .c files
   ----------------------------------------------------------------------- */
#ifndef HiWord
# define HiWord(x) ((short)(((x) >> 16) & 0xFFFF))
#endif
#ifndef LoWord
# define LoWord(x) ((short)((x) & 0xFFFF))
#endif

/* -----------------------------------------------------------------------
   Layout constants
   ----------------------------------------------------------------------- */
#define MARGIN_H        64
#define MARGIN_TOP      32
#define MARGIN_BOTTOM   38
#define MENU_BAR_HEIGHT 20
#define FONT_SIZE       12
#define SCROLLBAR_WIDTH 16

/* -----------------------------------------------------------------------
   Memory pool (unchanged from classic version)
   ----------------------------------------------------------------------- */
#define MEMORY_POOL_SIZE 10
extern Handle gMemoryPool[MEMORY_POOL_SIZE];
extern short  gMemoryPoolCount;

#define PoolNewHandle(size) \
    (gMemoryPoolCount > 0 ? \
     (HLock(gMemoryPool[--gMemoryPoolCount]), gMemoryPool[gMemoryPoolCount]) : \
     NewHandle(size))

#define PoolDisposeHandle(h) \
    do { \
        if (h && gMemoryPoolCount < MEMORY_POOL_SIZE) { \
            HUnlock(h); \
            gMemoryPool[gMemoryPoolCount++] = h; \
        } else { \
            DisposeHandle(h); \
        } \
    } while(0)

/* -----------------------------------------------------------------------
   Menu IDs and item numbers  (unchanged)
   ----------------------------------------------------------------------- */
#define mApple        1
#define iAppleAbout   1
#define mFile         128
#define iNew          1
#define iOpen         2
#define iSave         3
#define iSaveAs       4
#define iQuit         6

#define mEdit         131
#define iUndo         1
#define iRedo         2
#define iCut          4
#define iCopy         5
#define iPaste        6
#define iSelectAll    8
#define iSearch       10
#define iSearchReplace 11

#define mStyle        129
#define iBold         1
#define iItalic       2
#define iInlineCode   3
#define iCodeBlock    4
#define iStrike       5
#define iHighlight    6
#define iBlockquote   8
#define iBulletPoints 9
#define iNumberedList 10
#define iH1           12
#define iH2           13
#define iH3           14
#define iLink         16
#define iNone         18

#define kSaveChangesAlert 130
#define kSaveBtn          1
#define kCancelBtn        2
#define kDontSaveBtn      3

#define kSplashDialog 131
#define iSplashNew    1
#define iSplashOpen   2
#define iSplashTitle  3

#define kLinkDialog   132
#define iLinkOK       1
#define iLinkCancel   2
#define iLinkField    4

#define kAboutDialog  133
#define iAboutOK      1
#define iAboutTitle   2

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

#define mView           130
#define iMarkdownView   1
#define iWriterView     2
#define iRefreshView    4
#define iZoomIn         6
#define iZoomOut        7
#define iZoomDefault    8
#define iSerif          10
#define iSansSerif      11
#define iStatusBar      13

#define mWindow  134
#define mHelp    132
#define iAbout   1

/* -----------------------------------------------------------------------
   Limits
   ----------------------------------------------------------------------- */
#define MAX_STYLE_OPS     8192
#define kNumZoomLevels    3
#define kZoomBaselineIndex 1
#define kZoomDefaultIndex  0

/* Resource type/ID for zoom preference — stored in app's resource fork */
#define kZoomPrefType 'ZLvl'
#define kZoomPrefID   128

/* -----------------------------------------------------------------------
   Undo / redo snapshots
   ----------------------------------------------------------------------- */
#define MAX_UNDO_LEVELS 100

typedef struct {
    Handle textH;
    long   length;
    short  selStart, selEnd;
    Boolean isWriterMode;
} UndoSnapshot;

/* -----------------------------------------------------------------------
   Link URL table
   ----------------------------------------------------------------------- */
#define MAX_LINKS   512
#define WINDOW_SIZE 4000  /* max chars loaded into the TE at a time */

/* -----------------------------------------------------------------------
   Style ops
   ----------------------------------------------------------------------- */
typedef struct {
    long  start, end;
    short kind, level;
    short linkID;
} StyleOp;

/* -----------------------------------------------------------------------
   Global flags
   ----------------------------------------------------------------------- */
extern Boolean gScrollbarDriven;
extern long    gWindowStartLine;

/* -----------------------------------------------------------------------
   Document record (ARTFUL_PRO path only — Carbon build is always Pro)
   ----------------------------------------------------------------------- */
#define ARTFUL_PRO  1   /* always on for Carbon build */

typedef struct DocumentRecord {
    WindowRef   window;
    WEHandle    te;
    WEHandle    hiddenTE;
    WEHandle    activeTE;
    ControlRef  scrollBar;
    ControlRef  jumpToTopBtn;
    ControlRef  jumpToEndBtn;
    Boolean     scrollBarVisible;

    Boolean     haveFile;
    Boolean     dirty;
    Boolean     writerDirty;
    Str255      fileName;
    short       vRefNum;
    Boolean     hideMarkdown;
    Handle      markdownText;
    long        markdownLen;
    Handle      writerText;
    long        writerLen;
    Handle      writerOpsH;
    short       writerOpCount;
    long        windowStart;
    long        windowEnd;
    Handle      lineOffsetsH;
    long        numLines;
    long        lastCharCount;
    short       lastLine;
    short       lastCol;
    Boolean     showStatusBar;
    UndoSnapshot undoStack[MAX_UNDO_LEVELS];
    short        undoCount;
    UndoSnapshot redoStack[MAX_UNDO_LEVELS];
    short        redoCount;
    Boolean      typingRunActive;
    Str255       linkURLs[MAX_LINKS + 1];
    short        linkCount;
    Boolean      shiftSelectionActive;
    short        shiftAnchor;
    short        zoomIndex;
    struct DocumentRecord *next;
} DocumentRecord;

extern DocumentRecord *gActiveDoc;
extern DocumentRecord *gDocumentList;

/* Convenience macros identical to classic version */
#define gWindow              (gActiveDoc->window)
#define gTE                  (gActiveDoc->te)
#define gHiddenTE            (gActiveDoc->hiddenTE)
#define gActiveTE            (gActiveDoc->activeTE)
#define gScrollBar           (gActiveDoc->scrollBar)
#define gJumpToTopBtn        (gActiveDoc->jumpToTopBtn)
#define gJumpToEndBtn        (gActiveDoc->jumpToEndBtn)
#define gScrollBarVisible    (gActiveDoc->scrollBarVisible)
#define gHaveFile            (gActiveDoc->haveFile)
#define gDirty               (gActiveDoc->dirty)
#define gWriterDirty         (gActiveDoc->writerDirty)
#define gFileName            (gActiveDoc->fileName)
#define gVRefNum             (gActiveDoc->vRefNum)
#define gHideMarkdown        (gActiveDoc->hideMarkdown)
#define gMarkdownText        (gActiveDoc->markdownText)
#define gMarkdownLen         (gActiveDoc->markdownLen)
#define gWriterText          (gActiveDoc->writerText)
#define gWriterLen           (gActiveDoc->writerLen)
#define gWriterOpsH          (gActiveDoc->writerOpsH)
#define gWriterOpCount       (gActiveDoc->writerOpCount)
#define gWindowStart         (gActiveDoc->windowStart)
#define gWindowEnd           (gActiveDoc->windowEnd)
#define gLineOffsetsH        (gActiveDoc->lineOffsetsH)
#define gNumLines            (gActiveDoc->numLines)
#define gLastCharCount       (gActiveDoc->lastCharCount)
#define gLastLine            (gActiveDoc->lastLine)
#define gLastCol             (gActiveDoc->lastCol)
#define gShowStatusBar       (gActiveDoc->showStatusBar)
#define gZoomIndex           (gActiveDoc->zoomIndex)
#define gUndoStack           (gActiveDoc->undoStack)
#define gUndoCount           (gActiveDoc->undoCount)
#define gRedoStack           (gActiveDoc->redoStack)
#define gRedoCount           (gActiveDoc->redoCount)
#define gTypingRunActive     (gActiveDoc->typingRunActive)
#define gLinkURLs            (gActiveDoc->linkURLs)
#define gLinkCount           (gActiveDoc->linkCount)
#define gShiftSelectionActive (gActiveDoc->shiftSelectionActive)
#define gShiftAnchor         (gActiveDoc->shiftAnchor)

DocumentRecord* GetDocumentForWindow(WindowRef w);
DocumentRecord* CreateNewDocument(void);
void            DisposeDocument(DocumentRecord *doc);
void            SetActiveDocument(DocumentRecord *doc);

/* -----------------------------------------------------------------------
   Remaining globals
   ----------------------------------------------------------------------- */
extern Boolean     gDone;
extern MenuRef     gViewMenu;
extern MenuRef     gEditMenu;
extern MenuRef     gWindowMenu;
extern short       gDefaultZoomIndex;
extern Boolean     gUseSansSerif;

/* -----------------------------------------------------------------------
   Function prototypes — same as classic version
   ----------------------------------------------------------------------- */

/* main.c */
void   MakeWindow(void);
void   UpdateMenuBarLook(void);
short  GetDefaultFontNum(void);
void   SetFontMode(Boolean useSans);
void   SetDirty(Boolean dirty);
void   InsertDateHeading(short level);
void   InsertTimeHeading(short level);
void   ToggleCodeBlockHidden(void);

/* scrolling.c */
void   UpdateScrollbarRange(void);
void   AdjustScrollbar(void);
void   ScrollCaretIntoView(Boolean movingBackward);
void   HandleJumpToTop(void);
void   HandleJumpToEnd(void);
void   HandlePageUp(void);
void   HandlePageDown(void);
void   SuppressDrawing(WEHandle te, Rect *saved);
void   RestoreDrawing(WEHandle te, Rect *saved);
long   TotalLength(void);
short  CurrentFontSize(void);
void   DoScrollClick(Point pt);
void   InvalidateHeightCache(void);

/* markdown.c */
void   ClearStyles(void);
void   BuildHiddenView(void);
void   SyncHiddenToCanonical(void);
Handle EncodeSelectionAsMarkdown(long start, long end, WEHandle te);
void   InsertMarkdownAsStyled(Handle srcH, long srcLen, WEHandle te);
void   WrapSelection(char *prefix, char *suffix);
void   ApplyHeading(short level);
void   InsertDateHeading(short level);
void   ApplyLinePrefix(const char *prefix);
void   ApplyLinePrefixHidden(const char *prefix);
void   InsertTimeHeading(short level);
void   DoLink(void);
void   ApplyZoomIndex(short newIndex);
void   ToggleFace(Style face);
void   DoLinkHidden(void);
void   ToggleCode(void);
void   ToggleCodeBlockHidden(void);
void   ToggleStrike(void);
void   ToggleHeadingHidden(short level);
void   DetectInlineMarkdown(char justTyped);
void   ClearSelectionStyleHidden(void);
void   ClearMarkdownInSelection(void);
short  AddLinkURL(const unsigned char *url);
void   LoadTextWindow(long startOffset);
void   SyncWindowToBacking(void);

/* undo.c */
void   ClearUndoRedoStacks(void);
void   UpdateEditMenuState(void);
void   PushUndoSnapshot(void);
void   DoUndo(void);
void   DoRedo(void);
void   DoCut(void);
void   DoCopy(void);
void   DoPaste(void);
void   DoSelectAll(void);

/* zoom.c */
short  CurrentFontSize(void);
void   LoadZoomPref(void);
void   DoZoom(short direction);
void   DoZoomReset(void);

/* file.c */
void    SetViewMode(Boolean hideMarkdown);
void    DoStartupOpen(void);    /* now called from Apple Event handler */
Boolean DoSaveAs(void);
Boolean DoSave(void);
Boolean ConfirmDiscardChanges(void);
Boolean DoOpenFile(void);
void    DoNewFile(void);
void    ReadFileFromFSRef(const FSRef *ref);  /* new: called from AE handler */

/* splash.c */
void ShowSplashScreen(void);
void ShowAboutBox(void);

#endif /* ARTFULTYPE_APP_H */

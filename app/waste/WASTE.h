#ifndef WASTE_H
#define WASTE_H

#include <Quickdraw.h>
#include <Windows.h>
#include <TextEdit.h>
#include <Events.h>

// Types
typedef struct WERecord {
    TEHandle te;
} WERecord;

typedef WERecord *WEPtr;
typedef WEPtr *WEHandle;
typedef WEHandle WEReference;

typedef long WEFlags;

typedef struct LongRect {
    long top;
    long left;
    long bottom;
    long right;
} LongRect;

typedef struct WETextStyle {
    short tsFont;
    Style tsFace;
    short tsSize;
    RGBColor tsColor;
    short verticalShift; // custom extension for superscript/subscript baseline offset
} WETextStyle, *WETextStylePtr;

// Constants (Flags)
#define weDoFont            0x0001
#define weDoFace            0x0002
#define weDoSize            0x0004
#define weDoColor           0x0008
#define weDoAll             0x000F
#define weDoVerticalOffset  0x0010

// Opaque selectors/tags for WESetInfo / WEGetInfo
typedef unsigned long WETag;
#define weDrawTextHook      'dtxt'

// Hooks
typedef pascal void (*WEDrawTextProcPtr)(const void *text, long length, WEHandle we);

// Prototypes
OSErr WENew(const Rect *destRect, const Rect *viewRect, WEFlags flags, WEHandle *we);
void WEDispose(WEHandle we);
void WEActivate(WEHandle we);
void WEDeactivate(WEHandle we);
void WEUpdate(const Rect *rect, WEHandle we);
void WEClick(Point pt, Boolean shift, WEHandle we);
void WEKey(short charCode, short keyCode, short modifiers, WEHandle we);
void WEIdle(WEHandle we);
void WESetSelect(long selStart, long selEnd, WEHandle we);
void WEGetSelection(long *selStart, long *selEnd, WEHandle we);
long WEOffsetToLine(long offset, WEHandle we);
long WEGetHeight(long startLine, long endLine, WEHandle we);
OSErr WESetStyle(short mode, const WETextStyle *style, WEHandle we);
OSErr WEGetStyle(long offset, WETextStyle *style, WEHandle we);
void WEGetDestRect(LongRect *destRect, WEHandle we);
void WEGetViewRect(LongRect *viewRect, WEHandle we);
void WEPinScroll(long dx, long dy, WEHandle we);
void WEScroll(long dx, long dy, WEHandle we);
Handle WEGetText(WEHandle we);
long WEGetTextLength(WEHandle we);
OSErr WEInsert(const void *textPtr, long textLength, const WETextStyle *style, WEHandle we);
void WEDelete(WEHandle we);
long WEGetLineCount(WEHandle we);
OSErr WEGetLineRange(long lineIndex, long *lineStart, long *lineEnd, WEHandle we);
void WECalText(WEHandle we);
Boolean WEFixLineHeights(WEHandle we);
void WESetRects(const Rect *destRect, const Rect *viewRect, WEHandle we);

#endif // WASTE_H

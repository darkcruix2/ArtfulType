#include "app.h"

long TotalLength(void) {
    if (gHideMarkdown) return gWriterLen;
    return gMarkdownLen;
}

static short CurrentScrollOffset(WEHandle te)
{
    LongRect viewRect, destRect;
    WEGetViewRect(&viewRect, te);
    WEGetDestRect(&destRect, te);
    return (short)(viewRect.top - destRect.top);
}

static void SyncScrollbarToOffset(void)
{
    static long lastTotal = -1;
    static short lastVal = -1;
    
    long total = TotalLength();
    if (total == 0) {
        SetControlValue(gScrollBar, 0);
        return;
    }
    
    // We are at gWindowStart in the document, plus the local scroll offset.
    long currentOffset = gWindowStart;
    
    // Convert currentOffset to a 0-32767 range
    short val = (short) (((double)currentOffset * 32767.0) / (double)total);

    // Only update if value actually changed to avoid unnecessary updates
    if (val != lastVal || total != lastTotal) {
        if (val != GetControlValue(gScrollBar))
            SetControlValue(gScrollBar, val);
        lastVal = val;
        lastTotal = total;
    }
}

void InvalidateHeightCache(void)
{
    // No longer used, but kept for compatibility with other files
}

void UpdateScrollbarRange(void)
{
    static long lastTotal = -1;
    static short lastMaxVal = -1;
    
    long total = TotalLength();
    short maxVal = (total > 0) ? 32767 : 0;

    // Only update if value actually changed to avoid unnecessary updates
    if (maxVal != lastMaxVal) {
        if (maxVal != GetControlMaximum(gScrollBar))
            SetControlMaximum(gScrollBar, maxVal);
        lastMaxVal = maxVal;
        
        if (!gScrollBarVisible) {
            ShowControl(gScrollBar);
            gScrollBarVisible = true;
        }
    }
}

void AdjustScrollbar(void)
{
    static Boolean lastUpdate = false;
    
    // Only update scrollbar range if needed
    UpdateScrollbarRange();
    
    // Only sync offset if it's not already being driven by the scrollbar
    if (!gScrollbarDriven) {
        SyncScrollbarToOffset();
    }
}

static short LineContaining(WEHandle te, long pos)
{
    return (short) WEOffsetToLine(pos, te);
}

void ScrollCaretIntoView(Boolean movingBackward)
{
    short caretLine;
    long heightToLine, heightToLineNext;
    short lineTop, lineBottom;
    short viewTop, viewBottom;
    long selStart, selEnd;

    if (!gActiveTE) return;

    WEGetSelection(&selStart, &selEnd, gActiveTE);
    caretLine = LineContaining(gActiveTE, selEnd);

    heightToLine     = WEGetHeight(0, caretLine, gActiveTE);
    heightToLineNext = WEGetHeight(0, caretLine + 1, gActiveTE);
    
    LongRect viewRect, destRect;
    WEGetViewRect(&viewRect, gActiveTE);
    WEGetDestRect(&destRect, gActiveTE);

    lineTop    = destRect.top + (short)heightToLine;
    lineBottom = destRect.top + (short)heightToLineNext;

    viewTop    = viewRect.top;
    viewBottom = viewRect.bottom;

    /* --- Step 1: use WEPinScroll to bring the caret into the visible rectangle --- */
    if (lineBottom > viewBottom) {
        short halfScreen = (viewBottom - viewTop) / 2;
        WEPinScroll(0, viewBottom - lineBottom - halfScreen, gActiveTE);
    } else if (lineTop < viewTop) {
        WEPinScroll(0, viewTop - lineTop, gActiveTE);
    }

    /* --- Step 2: window shift (only when caret is still off-screen) --- */
    if (!gScrollbarDriven) {
        /* re-compute lineTop after the possible WEPinScroll above */
        LongRect destRect2;
        WEGetDestRect(&destRect2, gActiveTE);
        lineTop = destRect2.top + (short)WEGetHeight(0, caretLine, gActiveTE);

        if (selEnd > WINDOW_SIZE - 200 && gWindowEnd < TotalLength()) {
            /* Caret is near the end of the loaded window. */
            long globalCaretPos = gWindowStart + selEnd;
            long newStart = globalCaretPos - WINDOW_SIZE / 8;
            if (newStart < 0) newStart = 0;
            SyncWindowToBacking();
            LoadTextWindow(newStart);
            /* Reposition caret to the same global document position */
            {
                long localCaret = globalCaretPos - gWindowStart;
                long len = WEGetTextLength(gActiveTE);
                if (localCaret < 0) localCaret = 0;
                if (localCaret > len) localCaret = len;
                WESetSelect(localCaret, localCaret, gActiveTE);
            }
        } else if (movingBackward && selEnd < 200 && gWindowStart > 0) {
            /* Caret is near the top of the loaded window and user is navigating backward: */
            long globalCaretPos = gWindowStart + selEnd;
            long newStart = globalCaretPos - (3 * WINDOW_SIZE / 4);
            if (newStart < 0) newStart = 0;
            SyncWindowToBacking();
            LoadTextWindow(newStart);
            /* Reposition caret to the same global document position */
            {
                long localCaret = globalCaretPos - gWindowStart;
                long len = WEGetTextLength(gActiveTE);
                if (localCaret < 0) localCaret = 0;
                if (localCaret > len) localCaret = len;
                WESetSelect(localCaret, localCaret, gActiveTE);
            }
        }
    }

    SyncScrollbarToOffset();
}

static pascal void ScrollAction(ControlHandle control, short part)
{
    short max = GetControlMaximum(control);
    short cur = GetControlValue(control);
    long total = TotalLength();
    long delta = 0;

    if (part == 0 || total <= 0) return;

    /* Scale delta dynamically */
    long lineDelta = (long) ((80.0 * 32767.0) / (double)total);
    if (lineDelta < 1) lineDelta = 1;
    long pageDelta = (long) ((1600.0 * 32767.0) / (double)total);
    if (pageDelta < 5) pageDelta = 5;

    switch (part) {
        case inUpButton:   delta = -lineDelta; break;
        case inDownButton: delta = lineDelta; break;
        case inPageUp:     delta = -pageDelta; break;
        case inPageDown:   delta = pageDelta; break;
        default:           delta = 0; break;
    }

    cur += delta;
    if (cur < 0) cur = 0;
    if (cur > max) cur = max;
    
    SetControlValue(control, cur);
    
    long newOffset = (long) (((double)cur * (double)total) / 32767.0);
    
    gScrollbarDriven = true;
    SyncWindowToBacking();
    LoadTextWindow(newOffset);
}

void DoScrollClick(Point pt)
{
    ControlHandle control;
    short part;
    short desired;

    part = FindControl(pt, gWindow, &control);
    if (part == 0 || control != gScrollBar)
        return;

    if (part == inThumb) {
        TrackControl(gScrollBar, pt, NULL);
        desired = GetControlValue(gScrollBar);
        
        long total = TotalLength();
        long newOffset = (long) (((double)desired * (double)total) / 32767.0);
        
        gScrollbarDriven = true;
        SyncWindowToBacking();
        LoadTextWindow(newOffset);
    } else {
        TrackControl(gScrollBar, pt, NewControlActionUPP(ScrollAction));
    }
}

void HandleJumpToTop(void)
{
    SyncWindowToBacking();
    LoadTextWindow(0);
    WESetSelect(0, 0, gActiveTE);
    ScrollCaretIntoView(true);
    UpdateScrollbarRange();
}

void HandleJumpToEnd(void)
{
    long total = TotalLength();
    long newStart = total - (WINDOW_SIZE / 2);
    if (newStart < 0) newStart = 0;
    
    SyncWindowToBacking();
    LoadTextWindow(newStart);
    
    {
        long len = WEGetTextLength(gActiveTE);
        WESetSelect(len, len, gActiveTE);
    }
    
    ScrollCaretIntoView(false);
    UpdateScrollbarRange();
}

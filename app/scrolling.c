#include "app.h"

long TotalLength(void) {
    if (gHideMarkdown) return gWriterLen;
    return gMarkdownLen;
}

static short CurrentScrollOffset(TEHandle te)
{
    return (**te).viewRect.top - (**te).destRect.top;
}

static void SyncScrollbarToOffset(void)
{
    long total = TotalLength();
    if (total == 0) {
        SetControlValue(gScrollBar, 0);
        return;
    }
    
    // We are at gWindowStart in the document, plus the local TEScroll offset.
    long currentOffset = gWindowStart;
    
    // Convert currentOffset to a 0-32767 range
    short val = (short) (((double)currentOffset * 32767.0) / (double)total);

    
    if (val != GetControlValue(gScrollBar))
        SetControlValue(gScrollBar, val);
}

void InvalidateHeightCache(void)
{
    // No longer used, but kept for compatibility with other files
}

void UpdateScrollbarRange(void)
{
    long total = TotalLength();
    short maxVal = (total > 0) ? 32767 : 0;

    if (maxVal != GetControlMaximum(gScrollBar))
        SetControlMaximum(gScrollBar, maxVal);

    if (!gScrollBarVisible) {
        ShowControl(gScrollBar);
        gScrollBarVisible = true;
    }
}

void AdjustScrollbar(void)
{
    UpdateScrollbarRange();
    SyncScrollbarToOffset();
}

static short LineContaining(TEHandle te, short pos)
{
    short low = 0;
    short high = (**te).nLines - 1;

    while (low < high) {
        short mid = low + (high - low + 1) / 2;

        if ((**te).lineStarts[mid] <= pos)
            low = mid;
        else
            high = mid - 1;
    }
    return low;
}

void ScrollCaretIntoView(Boolean movingBackward)
{
    short caretLine;
    long heightToLine, heightToLineNext;
    short lineTop, lineBottom;
    short viewTop, viewBottom;

    if (!gActiveTE) return;

    caretLine = LineContaining(gActiveTE, (**gActiveTE).selEnd);

    heightToLine     = TEGetHeight(caretLine,     0, gActiveTE);
    heightToLineNext = TEGetHeight(caretLine + 1, 0, gActiveTE);
    
    lineTop    = (**gActiveTE).destRect.top + (short)heightToLine;
    lineBottom = (**gActiveTE).destRect.top + (short)heightToLineNext;

    viewTop    = (**gActiveTE).viewRect.top;
    viewBottom = (**gActiveTE).viewRect.bottom;

    /* --- Step 1: use TEScroll to bring the caret into the visible rectangle --- */
    if (lineBottom > viewBottom) {
        short halfScreen = (viewBottom - viewTop) / 2;
        TEScroll(0, viewBottom - lineBottom - halfScreen, gActiveTE);
    } else if (lineTop < viewTop) {
        TEScroll(0, viewTop - lineTop, gActiveTE);
    }

    /* --- Step 2: window shift (only when caret is still off-screen) ---
       Re-read lineTop *after* TEScroll -- destRect.top may have changed.
       The backward shift must NOT fire just because selEnd < 200; that
       caused a cascade snap-back every time an arrow key was pressed after
       a scrollbar jump.  We only shift if the caret is actually above the
       view even after TEScroll (i.e. we have scrolled within the window
       so far that the beginning of the TE is above the visible area, which
       means we need to load the *previous* backing-store chunk). */
    if (!gScrollbarDriven) {
        /* re-compute lineTop after the possible TEScroll above */
        lineTop = (**gActiveTE).destRect.top + (short)TEGetHeight(caretLine, 0, gActiveTE);

        if ((**gActiveTE).selEnd > WINDOW_SIZE - 200 && gWindowEnd < TotalLength()) {
            /* Caret is near the end of the loaded window.
               Save the global caret position, load the next chunk starting just
               before the caret so it stays visible near the top of the new window. */
            long globalCaretPos = gWindowStart + (long)(**gActiveTE).selEnd;
            long newStart = globalCaretPos - WINDOW_SIZE / 8;
            if (newStart < 0) newStart = 0;
            SyncWindowToBacking();
            LoadTextWindow(newStart);
            /* Reposition caret to the same global document position */
            {
                short localCaret = (short)(globalCaretPos - gWindowStart);
                if (localCaret < 0) localCaret = 0;
                if (localCaret > (**gActiveTE).teLength) localCaret = (**gActiveTE).teLength;
                TESetSelect(localCaret, localCaret, gActiveTE);
            }
        } else if (movingBackward && (**gActiveTE).selEnd < 200 && gWindowStart > 0) {
            /* Caret is near the top of the loaded window and user is navigating backward:
               load the previous chunk so it becomes visible. */
            long globalCaretPos = gWindowStart + (long)(**gActiveTE).selEnd;
            long newStart = globalCaretPos - (3 * WINDOW_SIZE / 4);
            if (newStart < 0) newStart = 0;
            SyncWindowToBacking();
            LoadTextWindow(newStart);
            /* Reposition caret to the same global document position */
            {
                short localCaret = (short)(globalCaretPos - gWindowStart);
                if (localCaret < 0) localCaret = 0;
                if (localCaret > (**gActiveTE).teLength) localCaret = (**gActiveTE).teLength;
                TESetSelect(localCaret, localCaret, gActiveTE);
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

    /* Scale delta dynamically so that:
       - line scroll (Up/Down Arrow on scrollbar) scrolls by ~80 characters (1 line)
       - page scroll (Page Up/Down on scrollbar track) scrolls by ~1600 characters (~20 lines) */
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
    /* NOTE: gScrollbarDriven stays true until the next key event clears it.
       This prevents ScrollCaretIntoView (called from key events) from
       mistaking the freshly-loaded caret at pos 0 as a request to shift
       the window backward, which would cascade all the way to line 1. */
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
        /* gScrollbarDriven stays true until next key event clears it */
    } else {
        TrackControl(gScrollBar, pt, NewControlActionUPP(ScrollAction));
    }
}

void HandleJumpToTop(void)
{
    SyncWindowToBacking();
    LoadTextWindow(0);
    TESetSelect(0, 0, gActiveTE);
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
        short len = (**gActiveTE).teLength;
        TESetSelect(len, len, gActiveTE);
    }
    
    ScrollCaretIntoView(false);
    UpdateScrollbarRange();
}




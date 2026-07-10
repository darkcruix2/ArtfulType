import os

content = r"""#include "app.h"

static long TotalLength(void) {
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
    short val = (short) ((currentOffset * 32767L) / total);
    
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
    Boolean shouldShow;

    if (maxVal != GetControlMaximum(gScrollBar))
        SetControlMaximum(gScrollBar, maxVal);

    shouldShow = (maxVal > 0);
    if (shouldShow != gScrollBarVisible) {
        if (shouldShow)
            ShowControl(gScrollBar);
        else
            HideControl(gScrollBar);
        gScrollBarVisible = shouldShow;
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

void ScrollCaretIntoView(void)
{
    short caretLine;
    long heightToLine, heightToLineNext;
    short lineTop, lineBottom;
    short viewTop, viewBottom;

    caretLine = LineContaining(gActiveTE, (**gActiveTE).selEnd);

    heightToLine = TEGetHeight(caretLine, 0, gActiveTE);
    heightToLineNext = TEGetHeight(caretLine + 1, 0, gActiveTE);
    
    lineTop = (**gActiveTE).destRect.top + heightToLine;
    lineBottom = (**gActiveTE).destRect.top + heightToLineNext;

    viewTop = (**gActiveTE).viewRect.top;
    viewBottom = (**gActiveTE).viewRect.bottom;

    if (lineBottom > viewBottom) {
        short halfScreen = (viewBottom - viewTop) / 2;
        TEScroll(0, viewBottom - lineBottom - halfScreen, gActiveTE);
    } else if (lineTop < viewTop) {
        TEScroll(0, viewTop - lineTop, gActiveTE);
    }
    
    // If we scrolled past the loaded window bounds, we need to load a new window.
    // For now, if the caret is near the edges of our 4000 char window, we shift the window.
    if ((**gActiveTE).selEnd > 3800 && gWindowEnd < TotalLength()) {
        SyncWindowToBacking();
        LoadTextWindow(gWindowStart + 2000);
    } else if ((**gActiveTE).selEnd < 200 && gWindowStart > 0) {
        SyncWindowToBacking();
        long newStart = gWindowStart - 2000;
        if (newStart < 0) newStart = 0;
        LoadTextWindow(newStart);
    }

    SyncScrollbarToOffset();
}

static pascal void ScrollAction(ControlHandle control, short part)
{
    short max = GetControlMaximum(control);
    short cur = GetControlValue(control);
    short delta;

    if (part == 0) return;

    switch (part) {
        case inUpButton:   delta = -1000; break;
        case inDownButton: delta = 1000; break;
        case inPageUp:     delta = -4000; break;
        case inPageDown:   delta = 4000; break;
        default:           delta = 0; break;
    }

    cur += delta;
    if (cur < 0) cur = 0;
    if (cur > max) cur = max;
    
    SetControlValue(control, cur);
    
    long total = TotalLength();
    long newOffset = (cur * total) / 32767L;
    
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
        long newOffset = (desired * total) / 32767L;
        
        SyncWindowToBacking();
        LoadTextWindow(newOffset);
    } else {
        TrackControl(gScrollBar, pt, NewControlActionUPP(ScrollAction));
    }
}
"""

with open("app/scrolling.c", "w") as f:
    f.write(content)

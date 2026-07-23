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

static long GetMaxScrollPixels(void)
{
    LongRect viewRect;
    long viewHeight, totalHeight, maxScroll;
    
    if (!gActiveTE) return 0;
    
    WEGetViewRect(&viewRect, gActiveTE);
    viewHeight = viewRect.bottom - viewRect.top;
    totalHeight = WEGetHeight(0, WEGetLineCount(gActiveTE), gActiveTE);
    
    maxScroll = totalHeight - viewHeight;
    if (maxScroll < 0) maxScroll = 0;
    return maxScroll;
}

static void SafeScroll(long dy)
{
    long maxScroll = GetMaxScrollPixels();
    LongRect viewRect, destRect;
    long currentPixelsScrolled, newPixelsScrolled;
    
    if (!gActiveTE) return;
    
    WEGetViewRect(&viewRect, gActiveTE);
    WEGetDestRect(&destRect, gActiveTE);
    currentPixelsScrolled = viewRect.top - destRect.top;
    
    newPixelsScrolled = currentPixelsScrolled - dy;
    if (newPixelsScrolled < 0) {
        dy = currentPixelsScrolled;
    } else if (newPixelsScrolled > maxScroll) {
        dy = currentPixelsScrolled - maxScroll;
    }
    
    if (dy != 0) {
        WEPinScroll(0, dy, gActiveTE);
    }
}

static void SyncScrollbarToOffset(void)
{
    long total = TotalLength();
    short val = 0;

    if (total == 0) {
        SetControlValue(gScrollBar, 0);
        return;
    }
    
    if (total <= WINDOW_SIZE) {
        long maxScroll = GetMaxScrollPixels();
        if (maxScroll > 0) {
            LongRect viewRect, destRect;
            WEGetViewRect(&viewRect, gActiveTE);
            WEGetDestRect(&destRect, gActiveTE);
            long currentPixelsScrolled = viewRect.top - destRect.top;
            if (currentPixelsScrolled < 0) currentPixelsScrolled = 0;
            if (currentPixelsScrolled > maxScroll) currentPixelsScrolled = maxScroll;
            double calcVal = ((double)currentPixelsScrolled * 32767.0) / (double)maxScroll;
            if (calcVal < 0.0) calcVal = 0.0;
            if (calcVal > 32767.0) calcVal = 32767.0;
            val = (short) calcVal;
        }
    } else {
        /* For large documents, combine the window start offset with the
           pixel scroll position within the current window.  The window
           covers gWindowStart..gWindowEnd in the backing store; within
           that window, the TE may be scrolled by some number of pixels.
           Map the pixel offset to a fractional character offset so the
           scrollbar moves smoothly during arrow/page scrolling. */
        double effectiveOffset = (double)gWindowStart;
        long maxScroll = GetMaxScrollPixels();
        if (maxScroll > 0) {
            LongRect viewRect, destRect;
            long windowChars = gWindowEnd - gWindowStart;
            WEGetViewRect(&viewRect, gActiveTE);
            WEGetDestRect(&destRect, gActiveTE);
            long currentPixelsScrolled = viewRect.top - destRect.top;
            if (currentPixelsScrolled < 0) currentPixelsScrolled = 0;
            if (currentPixelsScrolled > maxScroll) currentPixelsScrolled = maxScroll;
            /* fraction of the window that has been scrolled past */
            effectiveOffset += ((double)currentPixelsScrolled / (double)maxScroll) * (double)windowChars;
        }
        double calcVal = (effectiveOffset * 32767.0) / (double)total;
        if (calcVal < 0.0) calcVal = 0.0;
        if (calcVal > 32767.0) calcVal = 32767.0;
        val = (short) calcVal;
    }

    if (val < 0) val = 0;
    if (val > 32767) val = 32767;
    if (val != GetControlValue(gScrollBar))
        SetControlValue(gScrollBar, val);
}

void InvalidateHeightCache(void)
{
    /* Called after DetectInlineMarkdown applies styles to gActiveTE.
       Reflow and repaint immediately so bold/italic/header changes
       appear without requiring a scroll. */
    if (!gActiveTE || !gWindow) return;
    LongRect viewRectLong;
    Rect viewRect;
    WEGetViewRect(&viewRectLong, gActiveTE);
    viewRect.left   = (short)viewRectLong.left;
    viewRect.top    = (short)viewRectLong.top;
    viewRect.right  = (short)viewRectLong.right;
    viewRect.bottom = (short)viewRectLong.bottom;
    WECalText(gActiveTE);
    EraseRect(&viewRect);
    WEUpdate(&viewRect, gActiveTE);
}

void UpdateScrollbarRange(void)
{
    long total = TotalLength();
    short maxVal;
    
    if (total <= WINDOW_SIZE) {
        long maxScroll = GetMaxScrollPixels();
        maxVal = (maxScroll > 0) ? 32767 : 0;
    } else {
        maxVal = (total > 0) ? 32767 : 0;
    }

    if (maxVal != GetControlMaximum(gScrollBar))
        SetControlMaximum(gScrollBar, maxVal);
    
    if (maxVal > 0 && !gScrollBarVisible) {
        ShowControl(gScrollBar);
        gScrollBarVisible = true;
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
        WEPinScroll(0, viewBottom - lineBottom, gActiveTE);
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

static void ApplyScrollbarValue(short cur)
{
    long total = TotalLength();
    if (total <= 0 || !gActiveTE) return;

    if (total <= WINDOW_SIZE) {
        long maxScroll = GetMaxScrollPixels();
        if (maxScroll > 0) {
            long targetPixels = (long) (((double)cur * (double)maxScroll) / 32767.0);
            long currentPixels = CurrentScrollOffset(gActiveTE);
            long dy = currentPixels - targetPixels;
            if (dy != 0) {
                WEPinScroll(0, dy, gActiveTE);
            }
        }
    } else {
        double desiredEffective = ((double)cur * (double)total) / 32767.0;
        long windowLen = gWindowEnd - gWindowStart;
        long maxStart = total - WINDOW_SIZE;
        if (maxStart < 0) maxStart = 0;
        
        Boolean insideWindow = false;
        if (desiredEffective >= (double)gWindowStart && windowLen > 0) {
            if (desiredEffective <= (double)gWindowEnd) {
                insideWindow = true;
            } else if (gWindowStart >= maxStart || gWindowEnd >= total) {
                insideWindow = true;
            }
        }
        
        /* Check if desired position is inside current loaded window */
        if (insideWindow) {
            double fraction = (desiredEffective - (double)gWindowStart) / (double)windowLen;
            if (fraction < 0.0) fraction = 0.0;
            if (fraction > 1.0) fraction = 1.0;
            
            long maxScroll = GetMaxScrollPixels();
            long targetPixels = (long) (fraction * (double)maxScroll);
            long currentPixels = CurrentScrollOffset(gActiveTE);
            long dy = currentPixels - targetPixels;
            if (dy != 0) {
                WEPinScroll(0, dy, gActiveTE);
            }
        } else {
            /* Desired position is outside current window: calculate new window start */
            long newStart = (long) desiredEffective - (WINDOW_SIZE / 2);
            if (newStart < 0) newStart = 0;
            if (newStart > maxStart) newStart = maxStart;
            
            gScrollbarDriven = true;
            SyncWindowToBacking();
            LoadTextWindow(newStart);
            
            windowLen = gWindowEnd - gWindowStart;
            if (windowLen > 0) {
                double fraction = (desiredEffective - (double)gWindowStart) / (double)windowLen;
                if (fraction < 0.0) fraction = 0.0;
                if (fraction > 1.0) fraction = 1.0;
                
                long maxScroll = GetMaxScrollPixels();
                long targetPixels = (long) (fraction * (double)maxScroll);
                if (targetPixels > 0) {
                    WEPinScroll(0, -targetPixels, gActiveTE);
                }
            }
        }
    }
}

static pascal void ScrollAction(ControlHandle control, short part)
{
    short max = GetControlMaximum(control);
    short cur = GetControlValue(control);
    long total = TotalLength();
    long delta = 0;

    if (part == 0 || total <= 0) return;

    if (total <= WINDOW_SIZE) {
        LongRect viewRect;
        long viewHeight;
        WEGetViewRect(&viewRect, gActiveTE);
        viewHeight = viewRect.bottom - viewRect.top;
        
        switch (part) {
            case inUpButton:   delta = -20; break;
            case inDownButton: delta = 20; break;
            case inPageUp:     delta = -(viewHeight - 20); break;
            case inPageDown:   delta = viewHeight - 20; break;
            default:           delta = 0; break;
        }
        
        if (delta != 0) {
            SafeScroll(-delta);
            SyncScrollbarToOffset();
        }
    } else {
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

        long newCur = (long)cur + delta;
        if (newCur < 0) newCur = 0;
        if (newCur > (long)max) newCur = (long)max;
        cur = (short)newCur;
        
        SetControlValue(control, cur);
        
        gScrollbarDriven = true;
        ApplyScrollbarValue(cur);
    }
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
        gScrollbarDriven = true;
        ApplyScrollbarValue(desired);
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

void HandlePageUp(void)
{
    if (!gActiveTE) return;
    
    LongRect viewRect;
    WEGetViewRect(&viewRect, gActiveTE);
    long viewHeight = viewRect.bottom - viewRect.top;
    long pageScroll = viewHeight - 20;
    if (pageScroll < 20) pageScroll = 20;
    
    long total = TotalLength();
    if (total <= WINDOW_SIZE) {
        SafeScroll(pageScroll);
        SyncScrollbarToOffset();
    } else {
        short cur = GetControlValue(gScrollBar);
        long pageDelta = (long) ((1600.0 * 32767.0) / (double)total);
        if (pageDelta < 5) pageDelta = 5;
        cur -= pageDelta;
        if (cur < 0) cur = 0;
        SetControlValue(gScrollBar, cur);
        gScrollbarDriven = true;
        ApplyScrollbarValue(cur);
    }
}

void HandlePageDown(void)
{
    if (!gActiveTE) return;
    
    LongRect viewRect;
    WEGetViewRect(&viewRect, gActiveTE);
    long viewHeight = viewRect.bottom - viewRect.top;
    long pageScroll = viewHeight - 20;
    if (pageScroll < 20) pageScroll = 20;
    
    long total = TotalLength();
    if (total <= WINDOW_SIZE) {
        SafeScroll(-pageScroll);
        SyncScrollbarToOffset();
    } else {
        short cur = GetControlValue(gScrollBar);
        short max = GetControlMaximum(gScrollBar);
        long pageDelta = (long) ((1600.0 * 32767.0) / (double)total);
        if (pageDelta < 5) pageDelta = 5;
        cur += pageDelta;
        if (cur > max) cur = max;
        SetControlValue(gScrollBar, cur);
        gScrollbarDriven = true;
        ApplyScrollbarValue(cur);
    }
}

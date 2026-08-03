/*
 * scrolling.c — ArtfulType Pro  (Carbon / OS X 10.4 port)
 *
 * The logic is identical to the classic version.  Changes:
 *   • NewControlActionUPP   still works in Carbon
 *   • inButton → kControlButtonPart  (Carbon control part code)
 *   • WEHandle geometry calls go through TXNCompat.c shim
 *   • InvalRect → InvalWindowRect
 */

#include "app.h"

long TotalLength(void)
{
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

static short LineContaining(WEHandle te, long pos)
{
    return (short)WEOffsetToLine(pos, te);
}

static long GetMaxScrollPixels(void)
{
    LongRect viewRect;
    long viewHeight, totalHeight, maxScroll;
    if (!gActiveTE) return 0;
    WEGetViewRect(&viewRect, gActiveTE);
    viewHeight  = viewRect.bottom - viewRect.top;
    totalHeight = WEGetHeight(0, WEGetLineCount(gActiveTE), gActiveTE);
    maxScroll   = totalHeight + (viewHeight / 2);
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
    newPixelsScrolled     = currentPixelsScrolled - dy;
    if (newPixelsScrolled < 0)         dy = currentPixelsScrolled;
    else if (newPixelsScrolled > maxScroll) dy = currentPixelsScrolled - maxScroll;
    if (dy != 0) WEPinScroll(0, dy, gActiveTE);
}

static void SyncScrollbarToOffset(void)
{
    long total = TotalLength();
    short val  = 0;
    if (total == 0) { SetControlValue(gScrollBar, 0); return; }

    if (total <= WINDOW_SIZE) {
        long maxScroll = GetMaxScrollPixels();
        if (maxScroll > 0) {
            LongRect viewRect, destRect;
            WEGetViewRect(&viewRect, gActiveTE);
            WEGetDestRect(&destRect, gActiveTE);
            long cur = viewRect.top - destRect.top;
            if (cur < 0) cur = 0;
            if (cur > maxScroll) cur = maxScroll;
            double v = ((double)cur * 32767.0) / (double)maxScroll;
            if (v < 0.0) v = 0.0;
            if (v > 32767.0) v = 32767.0;
            val = (short)v;
        }
    } else {
        double eff = (double)gWindowStart;
        long maxScroll = GetMaxScrollPixels();
        if (maxScroll > 0) {
            LongRect viewRect, destRect;
            long wc = gWindowEnd - gWindowStart;
            WEGetViewRect(&viewRect, gActiveTE);
            WEGetDestRect(&destRect, gActiveTE);
            long cur = viewRect.top - destRect.top;
            if (cur < 0) cur = 0;
            if (cur > maxScroll) cur = maxScroll;
            eff += ((double)cur / (double)maxScroll) * (double)wc;
        }
        double v = (eff * 32767.0) / (double)total;
        if (v < 0.0) v = 0.0;
        if (v > 32767.0) v = 32767.0;
        val = (short)v;
    }

    if (val < 0)     val = 0;
    if (val > 32767) val = 32767;
    if (val != GetControlValue(gScrollBar))
        SetControlValue(gScrollBar, val);
}

void InvalidateHeightCache(void)
{
    if (!gActiveTE || !gWindow) return;
    LongRect viewRectLong;
    Rect     viewRect;
    WEGetViewRect(&viewRectLong, gActiveTE);
    SetRect(&viewRect, (short)viewRectLong.left, (short)viewRectLong.top,
                       (short)viewRectLong.right, (short)viewRectLong.bottom);
    WECalText(gActiveTE);
    EraseRect(&viewRect);
    WEUpdate(&viewRect, gActiveTE);
}

void UpdateScrollbarRange(void)
{
    long  total  = TotalLength();
    short maxVal;
    if (total <= WINDOW_SIZE) {
        long ms = GetMaxScrollPixels();
        maxVal = (ms > 0) ? 32767 : 0;
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
    UpdateScrollbarRange();
    if (!gScrollbarDriven) SyncScrollbarToOffset();
}

void ScrollCaretIntoView(Boolean movingBackward)
{
    short caretLine;
    long  heightToLine, heightToLineNext;
    long  selStart, selEnd;

    if (!gActiveTE) return;

    WEGetSelection(&selStart, &selEnd, gActiveTE);
    caretLine        = LineContaining(gActiveTE, selEnd);
    heightToLine     = WEGetHeight(0, caretLine,   gActiveTE);
    heightToLineNext = WEGetHeight(0, caretLine+1, gActiveTE);

    short fontHeight = CurrentFontSize() + 4;
    if (fontHeight < 16) fontHeight = 16;
    if (heightToLineNext <= heightToLine) heightToLineNext = heightToLine + fontHeight;

    LongRect viewRectL, destRectL;
    WEGetViewRect(&viewRectL, gActiveTE);
    WEGetDestRect(&destRectL, gActiveTE);
    short lineTop    = (short)(destRectL.top + heightToLine);
    short lineBottom = (short)(destRectL.top + heightToLineNext);
    short viewTop    = (short)viewRectL.top;
    short viewBottom = (short)viewRectL.bottom;

    if (lineBottom > viewBottom)    SafeScroll(viewBottom - lineBottom);
    else if (lineTop < viewTop)     SafeScroll(viewTop    - lineTop);

    if (!gScrollbarDriven) {
        LongRect destRect2;
        WEGetDestRect(&destRect2, gActiveTE);
        lineTop = (short)(destRect2.top + WEGetHeight(0, caretLine, gActiveTE));

        if (selEnd > WINDOW_SIZE - 200 && gWindowEnd < TotalLength()) {
            long globalCaretPos = gWindowStart + selEnd;
            long newStart = globalCaretPos - WINDOW_SIZE / 8;
            if (newStart < 0) newStart = 0;
            SyncWindowToBacking();
            LoadTextWindow(newStart);
            {
                long localCaret = globalCaretPos - gWindowStart;
                long len = WEGetTextLength(gActiveTE);
                if (localCaret < 0) localCaret = 0;
                if (localCaret > len) localCaret = len;
                WESetSelect(localCaret, localCaret, gActiveTE);
            }
        } else if (movingBackward && selEnd < 200 && gWindowStart > 0) {
            long globalCaretPos = gWindowStart + selEnd;
            long newStart = globalCaretPos - (3 * WINDOW_SIZE / 4);
            if (newStart < 0) newStart = 0;
            SyncWindowToBacking();
            LoadTextWindow(newStart);
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
            long target  = (long)(((double)cur * (double)maxScroll) / 32767.0);
            long current = CurrentScrollOffset(gActiveTE);
            long dy      = current - target;
            if (dy != 0) WEPinScroll(0, dy, gActiveTE);
        }
    } else {
        double desired = ((double)cur * (double)total) / 32767.0;
        long   wLen    = gWindowEnd - gWindowStart;
        long   maxStart = total - WINDOW_SIZE;
        if (maxStart < 0) maxStart = 0;

        Boolean inside = (desired >= (double)gWindowStart && wLen > 0 &&
                         (desired <= (double)gWindowEnd ||
                          gWindowStart >= maxStart || gWindowEnd >= total));
        if (inside) {
            double frac = (desired - (double)gWindowStart) / (double)wLen;
            if (frac < 0.0) frac = 0.0;
            if (frac > 1.0) frac = 1.0;
            long maxScroll = GetMaxScrollPixels();
            long target    = (long)(frac * (double)maxScroll);
            long current   = CurrentScrollOffset(gActiveTE);
            long dy        = current - target;
            if (dy != 0) WEPinScroll(0, dy, gActiveTE);
        } else {
            long newStart = (long)desired - (WINDOW_SIZE / 2);
            if (newStart < 0) newStart = 0;
            if (newStart > maxStart) newStart = maxStart;
            gScrollbarDriven = true;
            SyncWindowToBacking();
            LoadTextWindow(newStart);
            wLen = gWindowEnd - gWindowStart;
            if (wLen > 0) {
                double frac = (desired - (double)gWindowStart) / (double)wLen;
                if (frac < 0.0) frac = 0.0;
                if (frac > 1.0) frac = 1.0;
                long maxScroll = GetMaxScrollPixels();
                long target    = (long)(frac * (double)maxScroll);
                if (target > 0) WEPinScroll(0, -target, gActiveTE);
            }
        }
    }
}

static pascal void ScrollAction(ControlRef control, short part)
{
    short max   = GetControlMaximum(control);
    short cur   = GetControlValue(control);
    long  total = TotalLength();
    long  delta = 0;

    if (part == 0 || total <= 0) return;

    if (total <= WINDOW_SIZE) {
        LongRect viewRect;
        long     viewHeight;
        WEGetViewRect(&viewRect, gActiveTE);
        viewHeight = viewRect.bottom - viewRect.top;
        switch (part) {
        case kControlUpButtonPart:   delta = -20;              break;
        case kControlDownButtonPart: delta =  20;              break;
        case kControlPageUpPart:     delta = -(viewHeight-20); break;
        case kControlPageDownPart:   delta =   viewHeight-20;  break;
        default: delta = 0;
        }
        if (delta != 0) { SafeScroll(-delta); SyncScrollbarToOffset(); }
    } else {
        long lineDelta = (long)((80.0  * 32767.0) / (double)total); if (lineDelta < 1) lineDelta = 1;
        long pageDelta = (long)((1600.0* 32767.0) / (double)total); if (pageDelta < 5) pageDelta = 5;
        switch (part) {
        case kControlUpButtonPart:   delta = -lineDelta; break;
        case kControlDownButtonPart: delta =  lineDelta; break;
        case kControlPageUpPart:     delta = -pageDelta; break;
        case kControlPageDownPart:   delta =  pageDelta; break;
        default: delta = 0;
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
    ControlRef control;
    short      part, desired;

    part = FindControl(pt, gWindow, &control);
    if (part == 0 || control != gScrollBar) return;

    if (part == kControlIndicatorPart) {
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
    long total    = TotalLength();
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
        SafeScroll(pageScroll); SyncScrollbarToOffset();
    } else {
        short cur       = GetControlValue(gScrollBar);
        long  pageDelta = (long)((1600.0 * 32767.0) / (double)total);
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
        SafeScroll(-pageScroll); SyncScrollbarToOffset();
    } else {
        short cur       = GetControlValue(gScrollBar);
        short max       = GetControlMaximum(gScrollBar);
        long  pageDelta = (long)((1600.0 * 32767.0) / (double)total);
        if (pageDelta < 5) pageDelta = 5;
        cur += pageDelta;
        if (cur > max) cur = max;
        SetControlValue(gScrollBar, cur);
        gScrollbarDriven = true;
        ApplyScrollbarValue(cur);
    }
}

/* SuppressDrawing / RestoreDrawing: move TE view rect off-screen while
   rebuilding, so draws into the inactive TE don't appear on screen.
   Identical logic to classic; WESetRects is the shim to TXNSetFrameBounds. */
void SuppressDrawing(WEHandle te, Rect *saved)
{
    LongRect vrl;
    WEGetViewRect(&vrl, te);
    SetRect(saved, (short)vrl.left, (short)vrl.top, (short)vrl.right, (short)vrl.bottom);
    Rect hidden;
    SetRect(&hidden, -32000, -32000, -31900, -31900);
    WESetRects(&hidden, &hidden, te);
}

void RestoreDrawing(WEHandle te, Rect *saved)
{
    WESetRects(saved, saved, te);
}

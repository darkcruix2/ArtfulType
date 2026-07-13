#include "WASTE.h"
#include "../app.h"
#include <Memory.h>

OSErr WENew(const Rect *destRect, const Rect *viewRect, WEFlags flags, WEHandle *we)
{
    WEHandle hWE = (WEHandle) NewHandle(sizeof(WERecord));
    if (hWE == NULL) {
        return memFullErr;
    }
    (*hWE)->te = TEStyleNew(destRect, viewRect);
    if ((*hWE)->te == NULL) {
        DisposeHandle((Handle) hWE);
        return memFullErr;
    }
    *we = hWE;
    return noErr;
}

void WEDispose(WEHandle we)
{
    if (we != NULL) {
        if ((*we)->te != NULL) {
            TEDispose((*we)->te);
        }
        DisposeHandle((Handle) we);
    }
}

void WEActivate(WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TEActivate((*we)->te);
    }
}

void WEDeactivate(WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TEDeactivate((*we)->te);
    }
}

void WEUpdate(const Rect *rect, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TEHandle te = (*we)->te;
        GrafPtr oldPort;
        GetPort(&oldPort);
        SetPort((**te).inPort);
        TEUpdate(rect, te);

        /* --- Post-draw pass for special styles --- */
        /* Only needed in Writer (hide-markdown) mode */
        if (gHideMarkdown) {
            short fs = CurrentFontSize();
            short superSize = (short)(fs * 0.7);
            short subSize   = superSize - 1;

            /* TEGetPoint returns coordinates ALREADY in port (screen) space.
               No conversion needed — TEScroll has already moved destRect. */
            Rect vr = (**te).viewRect;

            TEStyleHandle teStyles = TEGetStyleHandle(te);
            if (teStyles == NULL) {
                SetPort(oldPort);
                return;
            }
            HLock((Handle)teStyles);

            short   nRuns    = (**teStyles).nRuns;
            STHandle styleTab = (**teStyles).styleTab;
            HLock((Handle)styleTab);

            /* Remember last HR baseline to only draw the line once per line */
            short lastHRScreenY = -32000;

            short r;
            for (r = 0; r < nRuns; r++) {
                short runStart = (**teStyles).runs[r].startChar;
                short runEnd   = (r + 1 < nRuns)
                                    ? (**teStyles).runs[r+1].startChar
                                    : (short)(**te).teLength;

                if (runEnd <= runStart) continue;

                short styleIdx = (**teStyles).runs[r].styleIndex;
                STElement st = (*styleTab)[styleIdx];

                /* --- Identify what kind of special style this run is --- */
                Boolean isSuper  = (st.stSize == superSize);
                Boolean isSub    = (st.stSize == subSize);
                Boolean isStrike = (st.stColor.green == 1);
                Boolean isHR     = ((st.stFace & bold) && st.stColor.blue == 1);

                if (!isSuper && !isSub && !isStrike && !isHR) continue;

                /* Set font so CharWidth/GetFontInfo are accurate */
                TextFont(st.stFont);
                TextFace(st.stFace);
                TextSize(st.stSize);

                FontInfo fi;
                GetFontInfo(&fi);

                /* ---- Walk characters in the run ---- */
                char c;
                short ci;
                for (ci = runStart; ci < runEnd; ci++) {
                    c = (*(**te).hText)[ci];
                    if (c == '\r' || c == '\n') continue;

                    /* TEGetPoint already returns port-space coordinates */
                    LONGINT ptVal = TEGetPoint(ci, te);
                    short screenV = (short)(ptVal >> 16);
                    short screenH = (short)(ptVal & 0xFFFF);

                    /* Skip characters outside the update rect */
                    if (screenV + fi.descent < rect->top)    continue;
                    if (screenV - fi.ascent  > rect->bottom) continue;

                    short cw = CharWidth(c);

                    if (isHR) {
                        /* Draw the HR line once per unique baseline */
                        if (screenV != lastHRScreenY) {
                            /* Erase the whole line's text row first */
                            Rect lineR;
                            lineR.left   = vr.left;
                            lineR.right  = vr.right;
                            lineR.top    = screenV - fi.ascent;
                            lineR.bottom = screenV + fi.descent;
                            EraseRect(&lineR);

                            /* Draw the rule at mid-height of the line */
                            short rulerY = screenV - fi.ascent / 2;
                            PenNormal();
                            PenSize(1, 1);
                            MoveTo(vr.left,  rulerY);
                            LineTo(vr.right - 1, rulerY);
                            lastHRScreenY = screenV;
                        }
                        /* Skip remaining chars in HR run — line already drawn */
                        break;

                    } else if (isSuper) {
                        /* Shift up 6px: erase must cover both original and new position */
                        short shift = 6;
                        Rect cellR;
                        cellR.left   = screenH;
                        cellR.right  = screenH + cw;
                        cellR.top    = screenV - fi.ascent - shift; /* covers new top */
                        cellR.bottom = screenV + fi.descent;         /* covers old bottom */
                        EraseRect(&cellR);
                        MoveTo(screenH, screenV - shift);
                        DrawChar(c);

                    } else if (isSub) {
                        /* Shift down 2px: erase must cover both original and new position */
                        short shift = 2;
                        Rect cellR;
                        cellR.left   = screenH;
                        cellR.right  = screenH + cw;
                        cellR.top    = screenV - fi.ascent;          /* covers old top */
                        cellR.bottom = screenV + fi.descent + shift; /* covers new bottom */
                        EraseRect(&cellR);
                        MoveTo(screenH, screenV + shift);
                        DrawChar(c);

                    } else if (isStrike) {
                        /* Draw a horizontal strikethrough line through the char */
                        short strikeY = screenV - (fi.ascent * 2) / 3;
                        MoveTo(screenH,      strikeY);
                        LineTo(screenH + cw, strikeY);
                    }
                }
            }

            HUnlock((Handle)styleTab);
            HUnlock((Handle)teStyles);

            /* Reset QuickDraw pen state */
            PenNormal();
            RGBColor black = {0, 0, 0};
            RGBForeColor(&black);
        }

        SetPort(oldPort);
    }
}


void WEClick(Point pt, Boolean shift, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TEClick(pt, shift, (*we)->te);
    }
}

void WEKey(short charCode, short keyCode, short modifiers, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TEKey(charCode, (*we)->te);
    }
}

void WEIdle(WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TEIdle((*we)->te);
    }
}

void WESetSelect(long selStart, long selEnd, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TESetSelect((short) selStart, (short) selEnd, (*we)->te);
    }
}

void WEGetSelection(long *selStart, long *selEnd, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        *selStart = (**((*we)->te)).selStart;
        *selEnd = (**((*we)->te)).selEnd;
    } else {
        *selStart = 0;
        *selEnd = 0;
    }
}

long WEOffsetToLine(long offset, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TEHandle te = (*we)->te;
        short low = 0;
        short high = (**te).nLines - 1;

        if (high < 0) return 0;

        while (low < high) {
            short mid = low + (high - low + 1) / 2;

            if ((**te).lineStarts[mid] <= offset) {
                low = mid;
            } else {
                high = mid - 1;
            }
        }
        return low;
    }
    return 0;
}

long WEGetHeight(long startLine, long endLine, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        return TEGetHeight((short) endLine, (short) startLine, (*we)->te);
    }
    return 0;
}

OSErr WESetStyle(short mode, const WETextStyle *style, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TextStyle ts;
        ts.tsFont = style->tsFont;
        ts.tsFace = style->tsFace;
        ts.tsSize = style->tsSize;
        ts.tsColor = style->tsColor;
        
        short teMode = mode & ~weDoVerticalOffset;
        TESetStyle(teMode, &ts, false, (*we)->te);
    }
    return noErr;
}

OSErr WEGetStyle(long offset, WETextStyle *style, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TextStyle ts;
        short dlh, dfa;
        TEGetStyle((short) offset, &ts, &dlh, &dfa, (*we)->te);
        style->tsFont = ts.tsFont;
        style->tsFace = ts.tsFace;
        style->tsSize = ts.tsSize;
        style->tsColor = ts.tsColor;
        style->verticalShift = 0; // Default baseline offset

        {
            short fs = CurrentFontSize();
            short superSize = (short)(fs * 0.7);
            short subSize = superSize - 1;

            if (ts.tsSize == superSize) {
                style->verticalShift = -4;
            } else if (ts.tsSize == subSize) {
                style->verticalShift = 3;
            }
        }
    }
    return noErr;
}

void WEGetDestRect(LongRect *destRect, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        Rect r = (**((*we)->te)).destRect;
        destRect->top = r.top;
        destRect->left = r.left;
        destRect->bottom = r.bottom;
        destRect->right = r.right;
    } else {
        destRect->top = destRect->left = destRect->bottom = destRect->right = 0;
    }
}

void WEGetViewRect(LongRect *viewRect, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        Rect r = (**((*we)->te)).viewRect;
        viewRect->top = r.top;
        viewRect->left = r.left;
        viewRect->bottom = r.bottom;
        viewRect->right = r.right;
    } else {
        viewRect->top = viewRect->left = viewRect->bottom = viewRect->right = 0;
    }
}

void WEPinScroll(long dx, long dy, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TEScroll((short) dx, (short) dy, (*we)->te);
        /* Redraw overlay after scroll so special styles remain visible */
        Rect vr = (**((*we)->te)).viewRect;
        WEUpdate(&vr, we);
    }
}

void WEScroll(long dx, long dy, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TEScroll((short) dx, (short) dy, (*we)->te);
        /* Redraw overlay after scroll so special styles remain visible */
        Rect vr = (**((*we)->te)).viewRect;
        WEUpdate(&vr, we);
    }
}

Handle WEGetText(WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        return (**((*we)->te)).hText;
    }
    return NULL;
}

long WEGetTextLength(WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        return (**((*we)->te)).teLength;
    }
    return 0;
}

OSErr WEInsert(const void *textPtr, long textLength, const WETextStyle *style, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TEInsert((Ptr) textPtr, textLength, (*we)->te);
    }
    return noErr;
}

void WEDelete(WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TEDelete((*we)->te);
    }
}

long WEGetLineCount(WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        return (**((*we)->te)).nLines;
    }
    return 0;
}

OSErr WEGetLineRange(long lineIndex, long *lineStart, long *lineEnd, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TEHandle te = (*we)->te;
        if (lineIndex < 0 || lineIndex >= (**te).nLines) {
            return -1;
        }
        *lineStart = (**te).lineStarts[lineIndex];
        if (lineIndex + 1 < (**te).nLines) {
            *lineEnd = (**te).lineStarts[lineIndex + 1];
        } else {
            *lineEnd = (**te).teLength;
        }
        return noErr;
    }
    return -1;
}

void WECalText(WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        TECalText((*we)->te);
    }
}

void WESetRects(const Rect *destRect, const Rect *viewRect, WEHandle we)
{
    if (we != NULL && (*we)->te != NULL) {
        (**((*we)->te)).destRect = *destRect;
        (**((*we)->te)).viewRect = *viewRect;
    }
}

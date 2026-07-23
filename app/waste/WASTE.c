#include "WASTE.h"
#include "../app.h"
#include <Memory.h>
#include <stdio.h>
#include <string.h>

OSErr WENew(const Rect *destRect, const Rect *viewRect, WEFlags flags,
            WEHandle *we) {
  WEHandle hWE = (WEHandle)NewHandle(sizeof(WERecord));
  if (hWE == NULL) {
    return memFullErr;
  }
  (*hWE)->te = TEStyleNew(destRect, viewRect);
  if ((*hWE)->te == NULL) {
    DisposeHandle((Handle)hWE);
    return memFullErr;
  }
  *we = hWE;
  return noErr;
}

void WEDispose(WEHandle we) {
  if (we != NULL) {
    if ((*we)->te != NULL) {
      TEDispose((*we)->te);
    }
    DisposeHandle((Handle)we);
  }
}

void WEActivate(WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TEActivate((*we)->te);
  }
}

void WEDeactivate(WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TEDeactivate((*we)->te);
  }
}

void WEUpdate(const Rect *rect, WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TEHandle te = (*we)->te;
    GrafPtr oldPort;
    GetPort(&oldPort);
    SetPort((**te).inPort);

    TEUpdate(rect, te);

    /* --- Right-aligned line numbers in Markdown view --- */
    if (!gHideMarkdown) {
      short nLines = (**te).nLines;
      Rect viewRect = (**te).viewRect;
      Handle hText = (**te).hText;
      short destLeft = (**te).destRect.left;

      short numFont = 0;
      GetFNum("\pMonaco", &numFont);
      if (numFont == 0)
        GetFNum("\pCourier", &numFont);

      TextFont(numFont);
      TextSize(12);
      TextFace(normal);

      /* Get Monaco 12pt metrics for StringWidth only */
      FontInfo fi;
      GetFontInfo(&fi);

      /* Erase the entire gutter strip so stale numbers from deleted/added
         lines don't remain. Use white background explicitly. */
      RGBColor whiteColor = {0xFFFF, 0xFFFF, 0xFFFF};
      RGBBackColor(&whiteColor);
      Rect gutterRect;
      gutterRect.left = viewRect.left;
      gutterRect.right = destLeft - 4;
      gutterRect.top = rect->top;
      gutterRect.bottom = rect->bottom;
      EraseRect(&gutterRect);

      RGBColor grayColor = {0x6666, 0x6666, 0x6666};
      RGBForeColor(&grayColor);

      HLock(hText);
      long paragraphNum = gWindowStartLine;

      short l;
      for (l = 0; l < nLines; l++) {
        short lineStart = (**te).lineStarts[l];
        Boolean isNewParagraph = false;

        if (l == 0) {
          if (lineStart == 0) {
            isNewParagraph = true;
          } else {
            char prevC = (*hText)[lineStart - 1];
            isNewParagraph = (prevC == '\r' || prevC == '\n');
          }
          paragraphNum = gWindowStartLine;
        } else if (lineStart > 0) {
          char prevC = (*hText)[lineStart - 1];
          if (prevC == '\r' || prevC == '\n') {
            isNewParagraph = true;
            paragraphNum++;
          }
        }

        if (isNewParagraph) {
          /* TEGetPoint returns the pen baseline Y (port coordinates) of
             the character at lineStart. This is the correct Y to use
             so the number baseline matches the text baseline exactly. */
          LONGINT ptVal = TEGetPoint(lineStart, te);
          short screenV = (short)(ptVal >> 16) - 3;

          /* Only draw if line is visible in portRect */
          if (screenV >= rect->top - 16 && screenV <= rect->bottom + 4) {
            char numStr[16];
            sprintf(numStr, "%ld", paragraphNum);
            Str255 pNumStr;
            pNumStr[0] = strlen(numStr);
            BlockMove(numStr, pNumStr + 1, pNumStr[0]);

            short strW = StringWidth(pNumStr);
            short numX = destLeft - 8 - strW;
            MoveTo(numX, screenV);
            DrawString(pNumStr);
          }
        }
      }
      HUnlock(hText);

      /* Draw vertical separator line between line numbers gutter and text */
      PenNormal();
      PenSize(1, 1);
      RGBColor sepColor = {0xCCCC, 0xCCCC, 0xCCCC};
      RGBForeColor(&sepColor);
      MoveTo((**te).destRect.left - 5, viewRect.top);
      LineTo((**te).destRect.left - 5, viewRect.bottom);

      RGBColor blackColor = {0, 0, 0};
      RGBForeColor(&blackColor);
      SetPort(oldPort);
      return;
    }

    /* --- Post-draw pass for special styles --- */
    /* Only needed in Writer (hide-markdown) mode */
    if (gHideMarkdown) {
      short fs = CurrentFontSize();
      short superSize = (short)(fs * 0.7);
      short subSize = superSize - 1;

      /* TEGetPoint returns coordinates ALREADY in port (screen) space.
         No conversion needed — TEScroll has already moved destRect. */
      Rect vr = (**te).viewRect;

      TEStyleHandle teStyles = TEGetStyleHandle(te);
      if (teStyles == NULL) {
        SetPort(oldPort);
        return;
      }
      HLock((Handle)teStyles);

      short nRuns = (**teStyles).nRuns;
      STHandle styleTab = (**teStyles).styleTab;
      HLock((Handle)styleTab);

      /* --- Draw line backgrounds / side-lines after TEUpdate using patOr ---
       */
      short l;
      for (l = 0; l < (**te).nLines; l++) {
        short lineStart = (**te).lineStarts[l];
        short lineEnd = (l + 1 < (**te).nLines) ? (**te).lineStarts[l + 1]
                                                : (**te).teLength;
        if (lineEnd > lineStart) {
          short styleIndex = -1;
          short rr;
          for (rr = 0; rr < nRuns; rr++) {
            if ((**teStyles).runs[rr].startChar <= lineStart &&
                (rr + 1 == nRuns ||
                 (**teStyles).runs[rr + 1].startChar > lineStart)) {
              styleIndex = (**teStyles).runs[rr].styleIndex;
              break;
            }
          }
          if (styleIndex >= 0) {
            STElement st = (*styleTab)[styleIndex];
            // Removed block code and blockquote graphical line rendering as
            // requested
          }
        }
      }

      /* Remember last HR baseline to only draw the line once per line */
      short lastHRScreenY = -32000;

      short r;
      for (r = 0; r < nRuns; r++) {
        short runStart = (**teStyles).runs[r].startChar;
        short runEnd = (r + 1 < nRuns) ? (**teStyles).runs[r + 1].startChar
                                       : (short)(**te).teLength;

        if (runEnd <= runStart)
          continue;

        short styleIdx = (**teStyles).runs[r].styleIndex;
        STElement st = (*styleTab)[styleIdx];

        /* --- Identify what kind of special style this run is --- */
        Boolean isSuper = (st.stSize == superSize);
        Boolean isSub = (st.stSize == subSize);
        Boolean isStrike = (st.stColor.green == 1);
        Boolean isHR = ((st.stFace & bold) && st.stColor.blue == 1);
        Boolean isTaskBox = (st.stColor.red == 255);

        if (!isSuper && !isSub && !isStrike && !isHR && !isTaskBox)
          continue;

        /* Set font so CharWidth/GetFontInfo are accurate */
        TextFont(st.stFont);
        TextFace(st.stFace);
        TextSize(st.stSize);

        FontInfo fi;
        GetFontInfo(&fi);

        if (isTaskBox) {
          LONGINT ptVal = TEGetPoint(runStart, te);
          short screenV = (short)(ptVal >> 16);
          short screenH = (short)(ptVal & 0xFFFF);
          if (screenV + fi.descent >= rect->top &&
              screenV - fi.ascent <= rect->bottom) {
            short midY = screenV - fi.ascent / 2;
            Rect boxR;
            boxR.left = (**te).destRect.left + 4;
            boxR.right = boxR.left + 10;
            boxR.top = midY - 5;
            boxR.bottom = boxR.top + 10;

            PenNormal();
            PenSize(1, 1);
            RGBColor black = {0, 0, 0};
            RGBForeColor(&black);
            FrameRect(&boxR);

            if (st.stColor.green == 1) {
              /* Checked tick mark */
              MoveTo(boxR.left + 2, boxR.top + 4);
              LineTo(boxR.left + 4, boxR.top + 7);
              LineTo(boxR.left + 9, boxR.top + 1);
              MoveTo(boxR.left + 2, boxR.top + 5);
              LineTo(boxR.left + 4, boxR.top + 8);
              LineTo(boxR.left + 9, boxR.top + 2);
            }
          }
          continue;
        }

        /* ---- Walk characters in the run ---- */
        char c;
        short ci;
        for (ci = runStart; ci < runEnd; ci++) {
          c = (*(**te).hText)[ci];
          if (c == '\r' || c == '\n')
            continue;

          /* TEGetPoint already returns port-space coordinates */
          LONGINT ptVal = TEGetPoint(ci, te);
          short screenV = (short)(ptVal >> 16);
          short screenH = (short)(ptVal & 0xFFFF);

          /* Skip characters outside the update rect */
          if (screenV + fi.descent < rect->top)
            continue;
          if (screenV - fi.ascent > rect->bottom)
            continue;

          short cw = CharWidth(c);

          if (isHR) {
            /* Draw the HR line once per unique baseline */
            if (screenV != lastHRScreenY) {
              /* Erase the whole line's text row first */
              Rect lineR;
              lineR.left = vr.left;
              lineR.right = vr.right;
              lineR.top = screenV - fi.ascent;
              lineR.bottom = screenV + fi.descent;
              EraseRect(&lineR);

              /* Draw the rule at mid-height of the line */
              short rulerY = screenV - fi.ascent / 2;
              PenNormal();
              PenSize(1, 1);
              MoveTo(vr.left, rulerY);
              LineTo(vr.right - 1, rulerY);
              lastHRScreenY = screenV;
            }
            /* Skip remaining chars in HR run — line already drawn */
            break;

          } else if (isSuper) {
            /* Shift up 6px: erase must cover both original and new position */
            short shift = 6;
            Rect cellR;
            cellR.left = screenH;
            cellR.right = screenH + cw;
            cellR.top = screenV - fi.ascent - shift; /* covers new top */
            cellR.bottom = screenV + fi.descent;     /* covers old bottom */
            EraseRect(&cellR);
            MoveTo(screenH, screenV - shift);
            DrawChar(c);

          } else if (isSub) {
            /* Shift down 2px: erase must cover both original and new position
             */
            short shift = 2;
            Rect cellR;
            cellR.left = screenH;
            cellR.right = screenH + cw;
            cellR.top = screenV - fi.ascent;             /* covers old top */
            cellR.bottom = screenV + fi.descent + shift; /* covers new bottom */
            EraseRect(&cellR);
            MoveTo(screenH, screenV + shift);
            DrawChar(c);

          } else if (isStrike) {
            /* Draw a horizontal strikethrough line through the char */
            short strikeY = screenV - (fi.ascent * 2) / 3;
            MoveTo(screenH, strikeY);
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

void WEClick(Point pt, Boolean shift, WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TEClick(pt, shift, (*we)->te);
  }
}

void WEKey(short charCode, short keyCode, short modifiers, WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TEHandle te = (*we)->te;
    TEKey(charCode, te);
    WEFixLineHeights(we);
  }
}

void WEIdle(WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TEIdle((*we)->te);
  }
}

void WESetSelect(long selStart, long selEnd, WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TESetSelect((short)selStart, (short)selEnd, (*we)->te);
  }
}

void WEGetSelection(long *selStart, long *selEnd, WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    *selStart = (**((*we)->te)).selStart;
    *selEnd = (**((*we)->te)).selEnd;
  } else {
    *selStart = 0;
    *selEnd = 0;
  }
}

long WEOffsetToLine(long offset, WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TEHandle te = (*we)->te;
    short low = 0;
    short high = (**te).nLines - 1;

    if (high < 0)
      return 0;

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

long WEGetHeight(long startLine, long endLine, WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    return TEGetHeight((short)endLine, (short)startLine, (*we)->te);
  }
  return 0;
}

OSErr WESetStyle(short mode, const WETextStyle *style, WEHandle we) {
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

OSErr WEGetStyle(long offset, WETextStyle *style, WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TextStyle ts;
    short dlh, dfa;
    TEGetStyle((short)offset, &ts, &dlh, &dfa, (*we)->te);
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

void WEGetDestRect(LongRect *destRect, WEHandle we) {
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

void WEGetViewRect(LongRect *viewRect, WEHandle we) {
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

void WEPinScroll(long dx, long dy, WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TEHandle te = (*we)->te;
    LongRect viewRect;
    WEGetViewRect(&viewRect, we);
    long viewHeight = viewRect.bottom - viewRect.top;
    long totalHeight = WEGetHeight(0, WEGetLineCount(we), we);

    (**te).destRect.bottom = (**te).destRect.top + (short)(totalHeight + viewHeight / 2);

    TEPinScroll((short)dx, (short)dy, te);
    if (gHideMarkdown) {
      Rect vr = (**te).viewRect;
      WEUpdate(&vr, we);
    }
  }
}

void WEScroll(long dx, long dy, WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TEScroll((short)dx, (short)dy, (*we)->te);
  }
}

Handle WEGetText(WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    return (**((*we)->te)).hText;
  }
  return NULL;
}

long WEGetTextLength(WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    return (**((*we)->te)).teLength;
  }
  return 0;
}

OSErr WEInsert(const void *textPtr, long textLength, const WETextStyle *style,
               WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TEInsert((Ptr)textPtr, textLength, (*we)->te);
  }
  return noErr;
}

void WEDelete(WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TEDelete((*we)->te);
  }
}

long WEGetLineCount(WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    return (**((*we)->te)).nLines;
  }
  return 0;
}

OSErr WEGetLineRange(long lineIndex, long *lineStart, long *lineEnd,
                     WEHandle we) {
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

Boolean WEFixLineHeights(WEHandle we) {
  /* Line height adjustments removed — headers use natural font metrics */
  return false;
}

void WECalText(WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    TECalText((*we)->te);
    WEFixLineHeights(we);
  }
}

void WESetRects(const Rect *destRect, const Rect *viewRect, WEHandle we) {
  if (we != NULL && (*we)->te != NULL) {
    (**((*we)->te)).destRect = *destRect;
    (**((*we)->te)).viewRect = *viewRect;
  }
}

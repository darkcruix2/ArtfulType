#include "app.h"
#include <string.h>

short AddLinkURL(const unsigned char *url)
{
    if (gLinkCount >= MAX_LINKS)
        return 0;
    gLinkCount++;
    BlockMove((Ptr) url, (Ptr) gLinkURLs[gLinkCount], url[0] + 1);
    return gLinkCount;
}

/*
    Markdown mode shows raw syntax with no visual styling at all -- just
    plain uniform text at the current zoom size. Selection is preserved
    since this gets called after Style-menu edits that already placed
    the caret somewhere meaningful.
*/
void ClearStyles(void)
{
    TextStyle ts;
    short fontNum;
    FontInfo info;
    short savedStart = (**gActiveTE).selStart;
    short savedEnd = (**gActiveTE).selEnd;

    fontNum = GetDefaultFontNum();
        
    ts.tsFont = fontNum;
    ts.tsFace = normal;
    ts.tsSize = CurrentFontSize();
    ts.tsColor.red = ts.tsColor.green = ts.tsColor.blue = 0;

    TESetSelect(0, 32767, gActiveTE);
    TESetStyle(doFont + doFace + doSize + doColor, &ts, false, gActiveTE);
    TECalText(gActiveTE);

    TESetSelect(savedStart, savedEnd, gActiveTE);
}




/*
    Builds gHiddenTE from gTE's canonical markdown text, stripping the
    delimiter characters themselves (**, *, `, [](), leading #s) and
    recording where the surviving text landed so styling can be applied
    afterward, in the stripped buffer's own coordinates.
*/
/*
    gTE and gHiddenTE are both bound to gWindow (a TE record draws into
    whatever GrafPort was current at TEStyleNew time, for its whole
    lifetime, regardless of which one is "active" later) -- so editing
    the *inactive* record still paints onto the window. Moving its
    viewRect off-screen for the duration of a rebuild makes those calls
    draw nothing, since drawing is clipped to viewRect every time.
*/
#define OFFSCREEN_COORD (-32000)

void SuppressDrawing(TEHandle te, Rect *saved)
{
    *saved = (**te).viewRect;
    SetRect(&(**te).viewRect, OFFSCREEN_COORD, OFFSCREEN_COORD,
            OFFSCREEN_COORD + 100, OFFSCREEN_COORD + 100);
}

void RestoreDrawing(TEHandle te, Rect *saved)
{
    (**te).viewRect = *saved;
}

void BuildHiddenView(void)
{
    Handle srcH;
    long len;
    Handle outH;
    long outLen;
    long i;
    StyleOp *ops;
    
    SetCursor(*GetCursor(watchCursor));

    gWriterOpCount = 0;
    gLinkCount = 0;
    srcH = gMarkdownText;
    len = gMarkdownLen;
    
    if (srcH == NULL) {
        if (gWriterText) DisposeHandle(gWriterText);
        gWriterText = NULL;
        gWriterLen = 0;
        LoadTextWindow(0);
        return;
    }
    
    if (gWriterOpsH == NULL) {
        gWriterOpsH = NewHandle(MAX_STYLE_OPS * sizeof(StyleOp));
    }
    
    HLock(srcH);
    {
        long maxOutLen = len * 8 + 1;
        long j;
        for (j = 0; j < len - 2; j++) {
            if ((j == 0 || (*srcH)[j - 1] == '\r') && (*srcH)[j] == '-' && (*srcH)[j+1] == '-' && (*srcH)[j+2] == '-') {
                maxOutLen += 17;
            }
        }
        outH = NewHandle(maxOutLen);
    }
    outLen = 0;

    HLock(outH);
    HLock(gWriterOpsH);
    ops = (StyleOp *) *gWriterOpsH;

    i = 0;
    while (i < len) {
        if (i + 1 < len && (*srcH)[i] == '\\' && ((*srcH)[i+1] == '*' || (*srcH)[i+1] == '_' || (*srcH)[i+1] == '#' || (*srcH)[i+1] == '>' || (*srcH)[i+1] == '[' || (*srcH)[i+1] == '`' || (*srcH)[i+1] == '\\')) {
            i += 2;
            continue;
        }


        if (i + 1 < len && (*srcH)[i] == '\\' && ((*srcH)[i+1] == '*' || (*srcH)[i+1] == '_' || (*srcH)[i+1] == '#' || (*srcH)[i+1] == '>' || (*srcH)[i+1] == '[' || (*srcH)[i+1] == '`' || (*srcH)[i+1] == '\\')) {
            (*outH)[outLen++] = (*srcH)[i+1];
            i += 2;
            continue;
        }

        if (i == 0 || (*srcH)[i - 1] == '\r') {
            long p = i;
            while (p < len && ((*srcH)[p] == ' ' || (*srcH)[p] == '\t')) p++;
            
            if (p + 2 < len && (*srcH)[p] == '`' && (*srcH)[p + 1] == '`' && (*srcH)[p + 2] == '`') {
                long j = p + 3;
                while (j < len && (*srcH)[j] != '\r') j++;
                long codeStart = j < len ? j + 1 : j;
                long codeEnd = len;
                long closingFenceStart = len;
                long search = codeStart;
                while (search < len) {
                    long lineBegin = search;
                    while (search < len && (*srcH)[search] != '\r') search++;
                    long lineEnd = search;
                    long sScan = lineBegin;
                    while (sScan < lineEnd && ((*srcH)[sScan] == ' ' || (*srcH)[sScan] == '\t')) sScan++;
                    if (sScan + 2 < lineEnd && (*srcH)[sScan] == '`' && (*srcH)[sScan + 1] == '`' && (*srcH)[sScan + 2] == '`') {
                        long remain = sScan + 3;
                        while (remain < lineEnd && ((*srcH)[remain] == ' ' || (*srcH)[remain] == '\t')) remain++;
                        if (remain == lineEnd) {
                            codeEnd = lineBegin > codeStart ? lineBegin - 1 : codeStart;
                            closingFenceStart = lineBegin;
                            break;
                        }
                    }
                    if (search < len) search++;
                }
                
                if (closingFenceStart < len) {
                    long outStart = outLen;
                    long m;
                    for (m = codeStart; m < codeEnd; m++) {
                        (*outH)[outLen++] = (*srcH)[m];
                    }
                    if (codeEnd < closingFenceStart) {
                        (*outH)[outLen++] = '\r';
                    }
                    
                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = outStart;
                        ops[gWriterOpCount].end = outLen;
                        ops[gWriterOpCount].kind = 'C';
                        ops[gWriterOpCount].level = 0;
                        ops[gWriterOpCount].linkID = 0;
                        gWriterOpCount++;
                    }
                    
                    j = closingFenceStart + 3;
                    while (j < len && (*srcH)[j] != '\r') j++;
                    if (j < len && (*srcH)[j] == '\r') j++;
                    i = j;
                    continue;
                }
            }
            
            short level = 0;
            while (level < 6 && p + level < len && (*srcH)[p + level] == '#')
                level++;
            if (level > 0 && p + level < len && (*srcH)[p + level] == ' ') {
                long lineStart = p + level + 1;
                long lineEnd = lineStart;
                long outStart = outLen;

                while (lineEnd < len && (*srcH)[lineEnd] != '\r') {
                    (*outH)[outLen++] = (*srcH)[lineEnd];
                    lineEnd++;
                }
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'H';
                    ops[gWriterOpCount].level = level;
                    gWriterOpCount++;
                }
                if (lineEnd < len && (*srcH)[lineEnd] == '\r') {
                    (*outH)[outLen++] = '\r';
                    i = lineEnd + 1;
                    if (i < len && (*srcH)[i] == '\r') {
                        i++;
                    }
                } else {
                    i = lineEnd;
                }
                continue;
            }


            {
                long blockquoteDepth = 0;
                long q = p;
                while (q < len) {
                    if ((*srcH)[q] == ' ' || (*srcH)[q] == '\t') {
                        q++;
                    } else if ((*srcH)[q] == '>') {
                        blockquoteDepth++;
                        q++;
                    } else {
                        break;
                    }
                }
                if (blockquoteDepth > 0) {
                    long lineStart = q;
                    long lineEnd = lineStart;
                    long outStart = outLen;

                    while (lineEnd < len && (*srcH)[lineEnd] != '\r') {
                        (*outH)[outLen++] = (*srcH)[lineEnd];
                        lineEnd++;
                    }
                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = outStart;
                        ops[gWriterOpCount].end = outLen;
                        ops[gWriterOpCount].kind = 'Q';
                        ops[gWriterOpCount].level = blockquoteDepth;
                        gWriterOpCount++;
                    }
                    if (lineEnd < len && (*srcH)[lineEnd] == '\r') {
                        (*outH)[outLen++] = '\r';
                        i = lineEnd + 1;
                        if (i < len && (*srcH)[i] == '\r') {
                            i++;
                        }
                    } else {
                        i = lineEnd;
                    }
                    continue;
                }
            }

            if (p + 2 < len && (((*srcH)[p] == '-' && (*srcH)[p+1] == '-' && (*srcH)[p+2] == '-') ||
                                ((*srcH)[p] == '*' && (*srcH)[p+1] == '*' && (*srcH)[p+2] == '*') ||
                                ((*srcH)[p] == '_' && (*srcH)[p+1] == '_' && (*srcH)[p+2] == '_'))) {
                long end = p + 3;
                while (end < len && (*srcH)[end] == ' ') end++;
                if (end == len || (*srcH)[end] == '\r') {
                    long outStart = outLen;
                    short s;
                    for (s = 0; s < 20; s++) (*outH)[outLen++] = '-';
                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = outStart;
                        ops[gWriterOpCount].end = outLen;
                        ops[gWriterOpCount].kind = 'R';
                        gWriterOpCount++;
                    }
                    if (end < len && (*srcH)[end] == '\r') {
                        (*outH)[outLen++] = '\r';
                        end++;
                    }
                    i = end;
                    continue;
                }
            }

            if (p + 1 < len && ((*srcH)[p] == '-' || (*srcH)[p] == '+' || (*srcH)[p] == '*') && (*srcH)[p + 1] == ' ') {
                long lineStart = p + 2;
                long lineEnd = lineStart;
                long outStart = outLen;
                short spaceCount = p - i;
                short nestingLevel = spaceCount / 2;
                
                Boolean isTask = false;
                Boolean isChecked = false;
                if (p + 5 < len && (*srcH)[p + 2] == '[' && ((*srcH)[p + 3] == ' ' || (*srcH)[p + 3] == 'x' || (*srcH)[p + 3] == 'X') && (*srcH)[p + 4] == ']' && (*srcH)[p + 5] == ' ') {
                    isTask = true;
                    isChecked = ((*srcH)[p + 3] == 'x' || (*srcH)[p + 3] == 'X');
                }

                long s;
                for (s = i; s < p; s++) {
                    (*outH)[outLen++] = (*srcH)[s];
                }

                if (isTask) {
                    (*outH)[outLen++] = '[';
                    (*outH)[outLen++] = isChecked ? 'x' : ' ';
                    (*outH)[outLen++] = ']';
                    (*outH)[outLen++] = ' ';
                    lineStart = p + 6;
                } else {
                    char bulletChar = '\245';
                    if (nestingLevel == 1) {
                        bulletChar = 'o';
                    } else if (nestingLevel >= 2) {
                        bulletChar = '-';
                    }
                    (*outH)[outLen++] = bulletChar;
                    (*outH)[outLen++] = ' ';
                }

                lineEnd = lineStart;
                while (lineEnd < len && (*srcH)[lineEnd] != '\r') {
                    (*outH)[outLen++] = (*srcH)[lineEnd];
                    lineEnd++;
                }
                if (lineEnd < len && (*srcH)[lineEnd] == '\r') {
                    (*outH)[outLen++] = '\r';
                    i = lineEnd + 1;
                    if (i < len && (*srcH)[i] == '\r') {
                        i++;
                    }
                } else {
                    i = lineEnd;
                }
                continue;
            }
        }

        if (i + 2 < len && ((*srcH)[i] == '*' || (*srcH)[i] == '_') && (*srcH)[i] == (*srcH)[i + 1] && (*srcH)[i] == (*srcH)[i + 2]) {
            char delim = (*srcH)[i];
            long j = i + 3;
            while (j + 2 < len && ((*srcH)[j] != delim || (*srcH)[j + 1] != delim || (*srcH)[j + 2] != delim))
                j++;
            if (j + 2 < len) {
                long outStart = outLen, m;
                for (m = i + 3; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'X';
                    gWriterOpCount++;
                }
                i = j + 3;
                continue;
            }
        }

        if (i + 1 < len && ((*srcH)[i] == '*' || (*srcH)[i] == '_') && (*srcH)[i] == (*srcH)[i + 1]) {
            char delim = (*srcH)[i];
            long j = i + 2;
            while (j + 1 < len && ((*srcH)[j] != delim || (*srcH)[j + 1] != delim))
                j++;
            if (j + 1 < len) {
                long outStart = outLen, m;
                for (m = i + 2; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'B';
                    gWriterOpCount++;
                }
                i = j + 2;
                continue;
            }
        }
        if ((*srcH)[i] == '*' || (*srcH)[i] == '_') {
            char delim = (*srcH)[i];
            long j = i + 1;
            while (j < len && (*srcH)[j] != delim)
                j++;
            if (j < len) {
                long outStart = outLen, m;
                for (m = i + 1; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'I';
                    gWriterOpCount++;
                }
                i = j + 1;
                continue;
            }
        }
        if (i + 1 < len && (*srcH)[i] == '~' && (*srcH)[i + 1] == '~') {
            long j = i + 2;
            while (j + 1 < len && ((*srcH)[j] != '~' || (*srcH)[j + 1] != '~'))
                j++;
            if (j + 1 < len) {
                long outStart = outLen, m;
                for (m = i + 2; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'S';
                    gWriterOpCount++;
                }
                i = j + 2;
                continue;
            }
        }
        if (i + 1 < len && (*srcH)[i] == '=' && (*srcH)[i + 1] == '=') {
            long j = i + 2;
            while (j + 1 < len && ((*srcH)[j] != '=' || (*srcH)[j + 1] != '='))
                j++;
            if (j + 1 < len) {
                long outStart = outLen, m;
                for (m = i + 2; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'E';
                    gWriterOpCount++;
                }
                i = j + 2;
                continue;
            }
        }
        if ((*srcH)[i] == '`') {
            long j = i + 1;
            while (j < len && (*srcH)[j] != '`')
                j++;
            if (j < len) {
                long outStart = outLen, m;
                for (m = i + 1; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'C';
                    gWriterOpCount++;
                }
                i = j + 1;
                continue;
            }
        }
        if ((*srcH)[i] == '<') {
            long closeAngle = i + 1;
            while (closeAngle < len && (*srcH)[closeAngle] != '>')
                closeAngle++;
            if (closeAngle < len) {
                long urlLen = closeAngle - (i + 1);
                if (urlLen > 7 && urlLen < 255) {
                    // check if starts with http
                    if (((*srcH)[i+1] == 'h' && (*srcH)[i+2] == 't' && (*srcH)[i+3] == 't' && (*srcH)[i+4] == 'p') ||
                        ((*srcH)[i+1] == 'm' && (*srcH)[i+2] == 'a' && (*srcH)[i+3] == 'i' && (*srcH)[i+4] == 'l')) {
                        long outStart = outLen, m;
                        Str255 url;
                        for (m = i + 1; m < closeAngle; m++)
                            (*outH)[outLen++] = (*srcH)[m];
                        url[0] = (unsigned char) urlLen;
                        BlockMove(*srcH + i + 1, url + 1, urlLen);
                        if (gWriterOpCount < MAX_STYLE_OPS) {
                            ops[gWriterOpCount].start = outStart;
                            ops[gWriterOpCount].end = outLen;
                            ops[gWriterOpCount].kind = 'L';
                            ops[gWriterOpCount].linkID = AddLinkURL(url);
                            gWriterOpCount++;
                        }
                        i = closeAngle + 1;
                        continue;
                    }
                }
            }
        }

        if ((*srcH)[i] == '[' || ((*srcH)[i] == '!' && i + 1 < len && (*srcH)[i+1] == '[')) {
            Boolean isImage = ((*srcH)[i] == '!');
            long openBracket = isImage ? i + 1 : i;
            long closeBracket = openBracket + 1;
            while (closeBracket < len && (*srcH)[closeBracket] != ']')
                closeBracket++;
            if (closeBracket < len && closeBracket + 1 < len && (*srcH)[closeBracket + 1] == '(') {
                long closeParen = closeBracket + 2;
                while (closeParen < len && (*srcH)[closeParen] != ')')
                    closeParen++;
                if (closeParen < len) {
                    long outStart = outLen, m;
                    Str255 url;
                    long urlLen = closeParen - (closeBracket + 2);
                    if (isImage) {
                        (*outH)[outLen++] = '!';
                    }
                    for (m = openBracket + 1; m < closeBracket; m++)
                        (*outH)[outLen++] = (*srcH)[m];
                    if (urlLen > 255) urlLen = 255;
                    url[0] = (unsigned char) urlLen;
                    BlockMove(*srcH + closeBracket + 2, url + 1, urlLen);
                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = outStart;
                        ops[gWriterOpCount].end = outLen;
                        ops[gWriterOpCount].kind = isImage ? 'M' : 'L';
                        ops[gWriterOpCount].linkID = AddLinkURL(url);
                        gWriterOpCount++;
                    }
                    i = closeParen + 1;
                    continue;
                }
            }
        }

        (*outH)[outLen++] = (*srcH)[i];
        i++;
    }

    HUnlock(srcH);
    HUnlock(gWriterOpsH);
    HUnlock(outH);
    SetHandleSize(outH, outLen);

    if (gWriterText != NULL) {
        DisposeHandle(gWriterText);
    }
    gWriterText = outH;
    gWriterLen = outLen;

    InitCursor();
    LoadTextWindow(0);
}

void SyncHiddenToCanonical(void)
{
    Handle srcH = gWriterText;
    long len = gWriterLen;
    Handle outH;
    long outCap;
    long outLen;
    long lineStart, lineEnd;
    short li;
    long urlSpace;
    
    if (srcH == NULL) return;

    urlSpace = 0;
    for (li = 1; li <= gLinkCount; li++)
        urlSpace += gLinkURLs[li][0];
    outCap = len * 5 + 1024 + urlSpace;
    outH = NewHandle(outCap);
    outLen = 0;

    HLock(srcH);
    HLock(outH);

    Boolean prevWasBlockquote = false;
    short prevBlockquoteDepth = 0;
    Boolean prevWasListItem = false;
    Boolean prevWasHeading = false;
    Boolean prevWasTaskItem = false;

    lineStart = 0;
    while (lineStart < len) {
        Boolean isHR = false;
        TextStyle firstStyle;
        long textOffset = lineStart;
        
        lineEnd = lineStart;
        while (lineEnd < len && (*srcH)[lineEnd] != '\r')
            lineEnd++;

        Boolean isBlockquote = false;
        short blockquoteDepth = 0;
        if (lineEnd > lineStart) {
            short k;
            StyleOp *ops = (StyleOp *) *gWriterOpsH;
            firstStyle.tsFace = 0;
            firstStyle.tsColor.blue = 0;
            if (gWriterOpsH) {
                HLock(gWriterOpsH);
                for (k = 0; k < gWriterOpCount; k++) {
                    if (ops[k].start <= lineStart && ops[k].end > lineStart) {
                        if (ops[k].kind == 'H') {
                            firstStyle.tsFace |= bold;
                            firstStyle.tsSize = CurrentFontSize() + (7 - ops[k].level) * 2;
                        } else if (ops[k].kind == 'R') {
                            firstStyle.tsFace |= bold;
                            firstStyle.tsColor.blue = 1;
                        } else if (ops[k].kind == 'Q') {
                            isBlockquote = true;
                            blockquoteDepth = ops[k].level;
                        }
                    }
                }
                HUnlock(gWriterOpsH);
            }

            if (firstStyle.tsColor.blue == 1) {
                Boolean onlyDashes = true;
                long d;
                for (d = lineStart; d < lineEnd; d++) {
                    if ((*srcH)[d] != '-') {
                        onlyDashes = false;
                        break;
                    }
                }
                if (onlyDashes && lineEnd > lineStart) {
                    isHR = true;
                }
            }
        }

        Boolean isHeading = false;
        Boolean isListItem = false;
        
        if (!isHR && (firstStyle.tsFace & bold)) {
            short lvl;
            for (lvl = 1; lvl <= 6; lvl++) {
                if (firstStyle.tsSize == CurrentFontSize() + (7 - lvl) * 2) {
                    isHeading = true;
                    break;
                }
            }
        }
        
        Boolean isTaskItem = false;
        Boolean isCheckedTask = false;
        if (!isHR && !isHeading && !isBlockquote) {
            long p = lineStart;
            while (p < lineEnd && ((*srcH)[p] == ' ' || (*srcH)[p] == '\t')) p++;
            if (p + 3 < lineEnd && (*srcH)[p] == '[' && ((*srcH)[p + 1] == ' ' || (*srcH)[p + 1] == 'x' || (*srcH)[p + 1] == 'X') && (*srcH)[p + 2] == ']' && (*srcH)[p + 3] == ' ') {
                isTaskItem = true;
                isCheckedTask = ((*srcH)[p + 1] == 'x' || (*srcH)[p + 1] == 'X');
            } else if (p < lineEnd && ((unsigned char)(*srcH)[p] == 0xA5 || (*srcH)[p] == 'o' || (*srcH)[p] == '-') && p + 1 < lineEnd && (*srcH)[p + 1] == ' ') {
                isListItem = true;
            } else {
                textOffset = p;
            }
        }
        Boolean isAnyList = isListItem || isTaskItem;
        Boolean prevWasAnyList = prevWasListItem || prevWasTaskItem;
        if (isHeading || isHR ||
            (isBlockquote && !prevWasBlockquote) ||
            (!isBlockquote && prevWasBlockquote) || 
            (isBlockquote && prevWasBlockquote && blockquoteDepth != prevBlockquoteDepth) ||
            (isAnyList && !prevWasAnyList) ||
            (!isAnyList && prevWasAnyList)) {
            
            if (outLen >= 1 && (*outH)[outLen - 1] == '\r') {
                if (outLen < 2 || (*outH)[outLen - 2] != '\r') {
                    (*outH)[outLen++] = '\r';
                }
            }
        }

        if (isHR) {
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '\r';
        } else if (isHeading) {
            short lvl;
            for (lvl = 1; lvl <= 6; lvl++) {
                if (firstStyle.tsSize == CurrentFontSize() + (7 - lvl) * 2) {
                    short s;
                    for (s = 0; s < lvl; s++)
                        (*outH)[outLen++] = '#';
                    (*outH)[outLen++] = ' ';
                    break;
                }
            }
        } else if (isBlockquote) {
            short depth;
            for (depth = 0; depth < blockquoteDepth; depth++) {
                (*outH)[outLen++] = '>';
            }
            (*outH)[outLen++] = ' ';
        } else if (isTaskItem) {
            long p = lineStart;
            while (p < lineEnd && ((*srcH)[p] == ' ' || (*srcH)[p] == '\t')) {
                (*outH)[outLen++] = (*srcH)[p];
                p++;
            }
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = ' ';
            (*outH)[outLen++] = '[';
            (*outH)[outLen++] = isCheckedTask ? 'x' : ' ';
            (*outH)[outLen++] = ']';
            (*outH)[outLen++] = ' ';
            textOffset = p + 4;
        } else if (isListItem) {
            long p = lineStart;
            while (p < lineEnd && ((*srcH)[p] == ' ' || (*srcH)[p] == '\t')) {
                (*outH)[outLen++] = (*srcH)[p];
                p++;
            }
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = ' ';
            textOffset = p + 2;
        }

        if (!isHR) {
            long runStart = textOffset;
            while (runStart < lineEnd) {
                long runEnd = runStart + 1;
                short linkID = 0;
                Boolean isBold = false, isItalic = false, isCode = false, isImageLink = false, isBoldItalic = false, isStrike = false, isHighlight = false, isMultilineCode = false;
                
                if (gWriterOpsH) {
                    short k;
                    StyleOp *ops;
                    HLock(gWriterOpsH);
                    ops = (StyleOp *) *gWriterOpsH;
                    
                    for (k = 0; k < gWriterOpCount; k++) {
                        if (ops[k].start <= runStart && ops[k].end > runStart) {
                            if (ops[k].kind == 'X') isBoldItalic = true;
                            if (ops[k].kind == 'B') isBold = true;
                            if (ops[k].kind == 'I') isItalic = true;
                            if (ops[k].kind == 'C') isCode = true;
                            if (ops[k].kind == 'S') isStrike = true;
                            if (ops[k].kind == 'E') isHighlight = true;
                            if (ops[k].kind == 'L') { linkID = ops[k].linkID; isImageLink = false; }
                            if (ops[k].kind == 'M') { linkID = ops[k].linkID; isImageLink = true; }
                            if (ops[k].end < runEnd || runEnd == runStart + 1) {
                                runEnd = ops[k].end;
                                if (runEnd > lineEnd) runEnd = lineEnd;
                            }
                        }
                    }
                    
                    for (k = 0; k < gWriterOpCount; k++) {
                        if (ops[k].start > runStart && ops[k].start < runEnd) {
                            runEnd = ops[k].start;
                        }
                    }
                    HUnlock(gWriterOpsH);
                } else {
                    runEnd = lineEnd;
                }

                if (isCode) {
                    long s;
                    for (s = runStart; s < runEnd; s++) {
                        if ((*srcH)[s] == '\r') {
                            isMultilineCode = true;
                            break;
                        }
                    }
                }
                if (isBoldItalic) { (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; }
                if (isBold) { (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; }
                if (isItalic) (*outH)[outLen++] = '*';
                if (isStrike) { (*outH)[outLen++] = '~'; (*outH)[outLen++] = '~'; }
                if (isHighlight) { (*outH)[outLen++] = '='; (*outH)[outLen++] = '='; }
                if (isCode) {
                    if (isMultilineCode) {
                        if (outLen > 0 && (*outH)[outLen - 1] != '\r') {
                            (*outH)[outLen++] = '\r';
                        }
                        (*outH)[outLen++] = '`';
                        (*outH)[outLen++] = '`';
                        (*outH)[outLen++] = '`';
                        (*outH)[outLen++] = '\r';
                    } else {
                        (*outH)[outLen++] = '`';
                    }
                }
                if (linkID > 0) {
                    if (isImageLink) (*outH)[outLen++] = '!';
                    (*outH)[outLen++] = '[';
                }

                while (runStart < runEnd) {
                    (*outH)[outLen++] = (*srcH)[runStart++];
                }

                if (linkID > 0) {
                    (*outH)[outLen++] = ']';
                    (*outH)[outLen++] = '(';
                    if (linkID <= gLinkCount) {
                        long uLen = gLinkURLs[linkID][0];
                        long u;
                        for (u = 1; u <= uLen; u++)
                            (*outH)[outLen++] = gLinkURLs[linkID][u];
                    }
                    (*outH)[outLen++] = ')';
                }
                if (isCode) {
                    if (isMultilineCode) {
                        if (outLen > 0 && (*outH)[outLen - 1] != '\r') {
                            (*outH)[outLen++] = '\r';
                        }
                        (*outH)[outLen++] = '`';
                        (*outH)[outLen++] = '`';
                        (*outH)[outLen++] = '`';
                        (*outH)[outLen++] = '\r';
                    } else {
                        (*outH)[outLen++] = '`';
                    }
                }
                if (isHighlight) { (*outH)[outLen++] = '='; (*outH)[outLen++] = '='; }
                if (isStrike) { (*outH)[outLen++] = '~'; (*outH)[outLen++] = '~'; }
                if (isItalic) (*outH)[outLen++] = '*';
                if (isBold) { (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; }
                if (isBoldItalic) { (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; }
            }
        }

        if (lineEnd < len) {
            Boolean isSingleReturn = true;
            if (lineEnd + 1 < len && (*srcH)[lineEnd + 1] == '\r') {
                isSingleReturn = false;
            }
            if (lineStart > 0 && (*srcH)[lineStart - 1] == '\r' && lineEnd == lineStart) {
                isSingleReturn = false;
            }
            if (isHR || isHeading || isBlockquote || isListItem || isTaskItem) {
                isSingleReturn = false;
            }
            if (isSingleReturn) {
                if (outLen >= 1 && (*outH)[outLen - 1] != ' ') {
                    (*outH)[outLen++] = ' ';
                    (*outH)[outLen++] = ' ';
                } else if (outLen >= 1 && (*outH)[outLen - 1] == ' ') {
                    if (outLen < 2 || (*outH)[outLen - 2] != ' ') {
                        (*outH)[outLen++] = ' ';
                    }
                }
            }
            (*outH)[outLen++] = '\r';
        }

        if (isHeading && outLen >= 1 && (*outH)[outLen - 1] == '\r') {
            if (outLen < 2 || (*outH)[outLen - 2] != '\r') {
                (*outH)[outLen++] = '\r';
            }
        }

        prevWasBlockquote = isBlockquote;
        prevBlockquoteDepth = blockquoteDepth;
        prevWasListItem = isListItem;
        prevWasHeading = isHeading;
        prevWasTaskItem = isTaskItem;

        lineStart = lineEnd + 1;
    }

    HUnlock(srcH);
    HUnlock(outH);

    if (gMarkdownText != NULL)
        DisposeHandle(gMarkdownText);
    gMarkdownText = outH;
    gMarkdownLen = outLen;
    SetHandleSize(gMarkdownText, gMarkdownLen);

    InitCursor();
}

Handle EncodeSelectionAsMarkdown(short start, short end, TEHandle te)
{
    Handle srcH;
    Handle outH;
    long outCap;
    long outLen;
    long urlSpace;
    short li;
    short monacoFont;
    long i;
    Boolean inBold = false, inItalic = false, inCode = false, inLink = false, isImageLink = false, inBoldItalic = false, inStrike = false, inHighlight = false;
    Str255 curLinkURL;

    srcH = (**te).hText;
    urlSpace = 0;
    for (li = 1; li <= gLinkCount; li++)
        urlSpace += gLinkURLs[li][0];
    outCap = (long) (end - start) * 2 + 64 + urlSpace;
    outH = NewHandle(outCap);
    outLen = 0;

    GetFNum("\pMonaco", &monacoFont);

    HLock(srcH);
    HLock(outH);

    i = start;
    while (i <= end) {
        Boolean wantBold = false, wantItalic = false, wantCode = false, wantLink = false, wantBoldItalic = false, wantStrike = false, wantHighlight = false;
        short linkID = 0;

        if (i < end) {
            TextStyle st;
            short dlh, dfa;

            TEGetStyle((short) i, &st, &dlh, &dfa, te);
            if ((st.tsFace & bold) != 0 && (st.tsFace & italic) != 0) {
                wantBoldItalic = true;
            } else {
                wantBold = (st.tsFace & bold) != 0;
                wantItalic = (st.tsFace & italic) != 0;
            }
            wantCode = (st.tsFont == monacoFont);
            wantLink = (st.tsFace & underline) != 0;
            linkID = st.tsColor.red;
            isImageLink = (st.tsColor.green == 1);
            wantHighlight = (st.tsFace & outline) != 0;
        } else {
            isImageLink = false;
        }

        if (inCode && !wantCode) { (*outH)[outLen++] = '`'; inCode = false; }
        if (inHighlight && !wantHighlight) {
            (*outH)[outLen++] = '='; (*outH)[outLen++] = '=';
            inHighlight = false;
        }
        if (inBoldItalic && !wantBoldItalic) { 
            (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; 
            inBoldItalic = false; 
        }
        if (inItalic && !wantItalic) { (*outH)[outLen++] = '*'; inItalic = false; }
        if (inBold && !wantBold) {
            (*outH)[outLen++] = '*';
            (*outH)[outLen++] = '*';
            inBold = false;
        }
        if (inLink && !wantLink) {
            (*outH)[outLen++] = ']';
            (*outH)[outLen++] = '(';
            BlockMove(curLinkURL + 1, *outH + outLen, curLinkURL[0]);
            outLen += curLinkURL[0];
            (*outH)[outLen++] = ')';
            inLink = false;
        }

        if (!inLink && wantLink) {
            if (isImageLink) (*outH)[outLen++] = '!';
            (*outH)[outLen++] = '[';
            inLink = true;
            if (linkID >= 1 && linkID <= gLinkCount)
                BlockMove(gLinkURLs[linkID], curLinkURL, gLinkURLs[linkID][0] + 1);
            else
                curLinkURL[0] = 0;
        }
        if (!inHighlight && wantHighlight) {
            (*outH)[outLen++] = '='; (*outH)[outLen++] = '=';
            inHighlight = true;
        }
        if (!inBoldItalic && wantBoldItalic) {
            (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*';
            inBoldItalic = true;
        }
        if (!inBold && wantBold) {
            (*outH)[outLen++] = '*';
            (*outH)[outLen++] = '*';
            inBold = true;
        }
        if (!inItalic && wantItalic) { (*outH)[outLen++] = '*'; inItalic = true; }
        if (!inCode && wantCode) { (*outH)[outLen++] = '`'; inCode = true; }

        if (i < end) {
            if ((*srcH)[i] == '\r') {
                Boolean isSingleReturn = true;
                if (i + 1 < end && (*srcH)[i + 1] == '\r') isSingleReturn = false;
                if (i > start && (*srcH)[i - 1] == '\r') isSingleReturn = false;
                
                if (isSingleReturn) {
                    if (outLen >= 1 && (*outH)[outLen - 1] != ' ') {
                        (*outH)[outLen++] = ' ';
                        (*outH)[outLen++] = ' ';
                    } else if (outLen >= 1 && (*outH)[outLen - 1] == ' ') {
                        if (outLen < 2 || (*outH)[outLen - 2] != ' ') {
                            (*outH)[outLen++] = ' ';
                        }
                    }
                }
            }
            (*outH)[outLen++] = (*srcH)[i];
        }
        i++;
    }

    HUnlock(srcH);
    HUnlock(outH);
    SetHandleSize(outH, outLen);

    return outH;
}

void InsertMarkdownAsStyled(Handle srcH, long srcLen, TEHandle te)
{
    Handle outH;
    long outLen;
    long i;
    static StyleOp ops[MAX_STYLE_OPS];
    short opCount = 0;
    short insertStart;
    short k;
    TextStyle baseStyle;
    short fontNum;

    outH = NewHandle(srcLen + 1);
    outLen = 0;

    HLock(srcH);
    HLock(outH);

    i = 0;
    while (i < srcLen) {
        if (i + 1 < srcLen && (*srcH)[i] == '\\' && ((*srcH)[i+1] == '*' || (*srcH)[i+1] == '_' || (*srcH)[i+1] == '#' || (*srcH)[i+1] == '>' || (*srcH)[i+1] == '[' || (*srcH)[i+1] == '`' || (*srcH)[i+1] == '\\')) {
            (*outH)[outLen++] = (*srcH)[i+1];
            i += 2;
            continue;
        }

        if (i + 2 < srcLen && ((*srcH)[i] == '*' || (*srcH)[i] == '_') && (*srcH)[i] == (*srcH)[i + 1] && (*srcH)[i] == (*srcH)[i + 2]) {
            char delim = (*srcH)[i];
            long j = i + 3;

            while (j + 2 < srcLen && !((*srcH)[j] == delim && (*srcH)[j + 1] == delim && (*srcH)[j + 2] == delim))
                j++;
            if (j + 2 < srcLen) {
                long outStart = outLen, m;

                for (m = i + 3; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (opCount < MAX_STYLE_OPS) {
                    ops[opCount].start = (short) outStart;
                    ops[opCount].end = (short) outLen;
                    ops[opCount].kind = 'X';
                    opCount++;
                }
                i = j + 3;
                continue;
            }
        }

        if (i + 1 < srcLen && ((*srcH)[i] == '*' || (*srcH)[i] == '_') && (*srcH)[i] == (*srcH)[i + 1]) {
            char delim = (*srcH)[i];
            long j = i + 2;

            while (j + 1 < srcLen && !((*srcH)[j] == delim && (*srcH)[j + 1] == delim))
                j++;
            if (j + 1 < srcLen) {
                long outStart = outLen, m;

                for (m = i + 2; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (opCount < MAX_STYLE_OPS) {
                    ops[opCount].start = (short) outStart;
                    ops[opCount].end = (short) outLen;
                    ops[opCount].kind = 'B';
                    opCount++;
                }
                i = j + 2;
                continue;
            }
        }
        if ((*srcH)[i] == '*' || (*srcH)[i] == '_') {
            char delim = (*srcH)[i];
            long j = i + 1;

            while (j < srcLen && (*srcH)[j] != delim)
                j++;
            if (j < srcLen) {
                long outStart = outLen, m;

                for (m = i + 1; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (opCount < MAX_STYLE_OPS) {
                    ops[opCount].start = (short) outStart;
                    ops[opCount].end = (short) outLen;
                    ops[opCount].kind = 'I';
                    opCount++;
                }
                i = j + 1;
                continue;
            }
        }
        if ((*srcH)[i] == '`') {
            long j = i + 1;

            while (j < srcLen && (*srcH)[j] != '`')
                j++;
            if (j < srcLen) {
                long outStart = outLen, m;

                for (m = i + 1; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (opCount < MAX_STYLE_OPS) {
                    ops[opCount].start = (short) outStart;
                    ops[opCount].end = (short) outLen;
                    ops[opCount].kind = 'C';
                    opCount++;
                }
                i = j + 1;
                continue;
            }
        }
        if ((*srcH)[i] == '<') {
            long closeAngle = i + 1;
            while (closeAngle < srcLen && (*srcH)[closeAngle] != '>')
                closeAngle++;
            if (closeAngle < srcLen) {
                long urlLen = closeAngle - (i + 1);
                if (urlLen > 7 && urlLen < 255) {
                    if (((*srcH)[i+1] == 'h' && (*srcH)[i+2] == 't' && (*srcH)[i+3] == 't' && (*srcH)[i+4] == 'p') ||
                        ((*srcH)[i+1] == 'm' && (*srcH)[i+2] == 'a' && (*srcH)[i+3] == 'i' && (*srcH)[i+4] == 'l')) {
                        long outStart = outLen, m;
                        Str255 url;
                        for (m = i + 1; m < closeAngle; m++)
                            (*outH)[outLen++] = (*srcH)[m];
                        url[0] = (unsigned char) urlLen;
                        BlockMove(*srcH + i + 1, url + 1, urlLen);
                        if (opCount < MAX_STYLE_OPS) {
                            ops[opCount].start = (short) outStart;
                            ops[opCount].end = (short) outLen;
                            ops[opCount].kind = 'L';
                            ops[opCount].linkID = AddLinkURL(url);
                            opCount++;
                        }
                        i = closeAngle + 1;
                        continue;
                    }
                }
            }
        }

        if ((*srcH)[i] == '[' || ((*srcH)[i] == '!' && i + 1 < srcLen && (*srcH)[i+1] == '[')) {
            Boolean isImage = ((*srcH)[i] == '!');
            long openBracket = isImage ? i + 1 : i;
            long closeBracket = openBracket + 1;

            while (closeBracket < srcLen && (*srcH)[closeBracket] != ']')
                closeBracket++;
            if (closeBracket < srcLen && closeBracket + 1 < srcLen && (*srcH)[closeBracket + 1] == '(') {
                long closeParen = closeBracket + 2;

                while (closeParen < srcLen && (*srcH)[closeParen] != ')')
                    closeParen++;
                if (closeParen < srcLen) {
                    long outStart = outLen, m;
                    Str255 url;
                    long urlLen = closeParen - (closeBracket + 2);

                    if (isImage) {
                        (*outH)[outLen++] = '!';
                    }
                    for (m = openBracket + 1; m < closeBracket; m++)
                        (*outH)[outLen++] = (*srcH)[m];
                    if (urlLen > 255) urlLen = 255;
                    url[0] = (unsigned char) urlLen;
                    BlockMove(*srcH + closeBracket + 2, url + 1, urlLen);
                    if (opCount < MAX_STYLE_OPS) {
                        ops[opCount].start = (short) outStart;
                        ops[opCount].end = (short) outLen;
                        ops[opCount].kind = isImage ? 'M' : 'L';
                        ops[opCount].linkID = AddLinkURL(url);
                        opCount++;
                    }
                    i = closeParen + 1;
                    continue;
                }
            }
        }

        (*outH)[outLen++] = (*srcH)[i];
        i++;
    }

    HUnlock(srcH);

    insertStart = (**te).selStart;
    TEInsert(*outH, outLen, te);
    HUnlock(outH);
    DisposeHandle(outH);

    /* TEInsert's new text inherits whatever style was at the
       insertion point -- normalize the whole pasted range to plain
       before applying the specific ops parsed above, the same order
       BuildHiddenView uses for the same reason. */
    fontNum = GetDefaultFontNum();
    baseStyle.tsFont = fontNum;
    baseStyle.tsFace = normal;
    baseStyle.tsSize = CurrentFontSize();
    baseStyle.tsColor.red = baseStyle.tsColor.green = baseStyle.tsColor.blue = 0;
    TESetSelect(insertStart, (short) (insertStart + outLen), te);
    TESetStyle(doFont + doFace + doSize + doColor, &baseStyle, false, te);

    for (k = 0; k < opCount; k++) {
        TextStyle opStyle;

        TESetSelect((short) (insertStart + ops[k].start), (short) (insertStart + ops[k].end), te);
        switch (ops[k].kind) {
            case 'X':
                opStyle.tsFace = bold | italic;
                TESetStyle(doFace, &opStyle, false, te);
                break;
            case 'B':
                opStyle.tsFace = bold;
                TESetStyle(doFace, &opStyle, false, te);
                break;
            case 'I':
                opStyle.tsFace = italic;
                TESetStyle(doFace, &opStyle, false, te);
                break;
            case 'C':
                GetFNum("\pMonaco", &opStyle.tsFont);
                TESetStyle(doFont, &opStyle, false, te);
                break;
            case 'L':
            case 'M':
                opStyle.tsFace = underline;
                opStyle.tsColor.red = ops[k].linkID;
                opStyle.tsColor.green = (ops[k].kind == 'M') ? 1 : 0;
                opStyle.tsColor.blue = 0;
                TESetStyle(doFace + doColor, &opStyle, false, te);
                break;
            case 'Q':
                opStyle.tsFace = italic;
                opStyle.tsColor.red = 0;
                opStyle.tsColor.green = 0;
                opStyle.tsColor.blue = 1 + ops[k].level;
                TESetStyle(doFace + doColor, &opStyle, false, te);
                break;
        }
    }

    TESetSelect((short) (insertStart + outLen), (short) (insertStart + outLen), te);
    TECalText(te);
}

void WrapSelection(char *prefix, char *suffix)
{
    short selStart, selEnd;
    long selLen, totalLen, textLen;
    short prefixLen, suffixLen;
    Handle textH;
    Handle newH;
    Boolean outerWrapped, innerWrapped;

    selStart = (**gTE).selStart;
    selEnd = (**gTE).selEnd;
    selLen = selEnd - selStart;
    textH = (**gTE).hText;
    textLen = (**gTE).teLength;

    gDirty = true;

    prefixLen = strlen(prefix);
    suffixLen = strlen(suffix);

    HLock(textH);
    outerWrapped =
        (selStart >= prefixLen) &&
        (selEnd + suffixLen <= textLen) &&
        (memcmp(*textH + selStart - prefixLen, prefix, prefixLen) == 0) &&
        (memcmp(*textH + selEnd, suffix, suffixLen) == 0);
    innerWrapped = !outerWrapped &&
        (selLen >= prefixLen + suffixLen) &&
        (memcmp(*textH + selStart, prefix, prefixLen) == 0) &&
        (memcmp(*textH + selEnd - suffixLen, suffix, suffixLen) == 0);
    HUnlock(textH);

    if (outerWrapped) {
        /* markers sit just outside the selection -- strip them (toggle off) */
        newH = NewHandle(selLen);
        HLock(newH);
        HLock(textH);
        BlockMove(*textH + selStart, *newH, selLen);
        HUnlock(textH);

        TESetSelect(selStart - prefixLen, selEnd + suffixLen, gTE);
        TEDelete(gTE);
        TEInsert(*newH, selLen, gTE);
        HUnlock(newH);
        DisposeHandle(newH);

        TESetSelect(selStart - prefixLen, selStart - prefixLen + selLen, gTE);
        return;
    }

    if (innerWrapped) {
        /* markers are part of the selection itself -- strip them (toggle off) */
        long innerLen = selLen - prefixLen - suffixLen;

        newH = NewHandle(innerLen);
        HLock(newH);
        HLock(textH);
        BlockMove(*textH + selStart + prefixLen, *newH, innerLen);
        HUnlock(textH);

        TEDelete(gTE);
        TEInsert(*newH, innerLen, gTE);
        HUnlock(newH);
        DisposeHandle(newH);

        TESetSelect(selStart, selStart + innerLen, gTE);
        return;
    }

    totalLen = prefixLen + selLen + suffixLen;
    newH = NewHandle(totalLen);
    HLock(newH);
    HLock(textH);
    BlockMove(prefix, *newH, prefixLen);
    BlockMove(*textH + selStart, *newH + prefixLen, selLen);
    BlockMove(suffix, *newH + prefixLen + selLen, suffixLen);
    HUnlock(textH);

    TEDelete(gTE);
    TEInsert(*newH, totalLen, gTE);
    HUnlock(newH);
    DisposeHandle(newH);

    TESetSelect(selStart + prefixLen, selStart + prefixLen + selLen, gTE);
}

void ApplyHeading(short level)
{
    short selStart;
    short lineStart;
    long textLen;
    Handle textH;
    char prefix[8];
    short i;
    Boolean alreadyHeading;

    gDirty = true;

    selStart = (**gTE).selStart;
    textH = (**gTE).hText;
    textLen = (**gTE).teLength;

    lineStart = selStart;
    HLock(textH);
    while (lineStart > 0 && (*textH)[lineStart - 1] != '\r')
        lineStart--;
    HUnlock(textH);

    for (i = 0; i < level; i++)
        prefix[i] = '#';
    prefix[level] = ' ';

    HLock(textH);
    alreadyHeading =
        (lineStart + level + 1 <= textLen) &&
        (memcmp(*textH + lineStart, prefix, level + 1) == 0);
    HUnlock(textH);

    if (alreadyHeading) {
        TESetSelect(lineStart, lineStart + level + 1, gTE);
        TEDelete(gTE);
        return;
    }

    TESetSelect(lineStart, lineStart, gTE);
    TEInsert(prefix, level + 1, gTE);
}

void DoLink(void)
{
    short selStart, selEnd;
    long selLen, totalLen;
    Handle textH;
    Handle newH;
    static char mid[] = "]()";
    short midLen = 3;
    short cursorPos;

    gDirty = true;

    selStart = (**gTE).selStart;
    selEnd = (**gTE).selEnd;
    selLen = selEnd - selStart;
    textH = (**gTE).hText;

    totalLen = 1 + selLen + midLen;
    newH = NewHandle(totalLen);
    HLock(newH);
    HLock(textH);
    (*newH)[0] = '[';
    BlockMove(*textH + selStart, *newH + 1, selLen);
    BlockMove(mid, *newH + 1 + selLen, midLen);
    HUnlock(textH);

    TEDelete(gTE);
    TEInsert(*newH, totalLen, gTE);
    HUnlock(newH);
    DisposeHandle(newH);

    cursorPos = selStart + selLen + 3;
    TESetSelect(cursorPos, cursorPos, gTE);
}

/*
    Style commands while in Hide Markdown mode apply real TextStyle
    directly to gHiddenTE instead of inserting delimiter text -- there's
    no visible syntax to insert. Toggle state is read back from the
    style at the selection start.
*/
static Boolean SelectionHasFace(Style face)
{
    TextStyle ts;
    short lh, fa;

    TEGetStyle((**gHiddenTE).selStart, &ts, &lh, &fa, gHiddenTE);
    return (ts.tsFace & face) != 0;
}

void ToggleFace(Style face)
{
    TextStyle ts;

    ts.tsFace = SelectionHasFace(face) ? normal : face;
    TESetStyle(doFace, &ts, true, gHiddenTE);
}

/* Prompts for a URL; returns true and fills in `url` if OK was clicked. */
static Boolean ShowLinkURLDialog(unsigned char *url)
{
    DialogPtr dlg;
    short item;
    DialogItemType type;
    Handle itemH;
    Rect box;
    Boolean result;

    dlg = GetNewDialog(kLinkDialog, NULL, (WindowPtr) -1L);
    if (dlg == NULL)
        return false;

    SelectDialogItemText(dlg, iLinkField, 0, 32767);

    do {
        ModalDialog(NULL, &item);
    } while (item != iLinkOK && item != iLinkCancel);

    result = (item == iLinkOK);
    if (result) {
        GetDialogItem(dlg, iLinkField, &type, &itemH, &box);
        GetDialogItemText(itemH, url);
    }

    DisposeDialog(dlg);
    SetPort(gWindow);
    UpdateMenuBarLook();
    return result;
}

/*
    "Link" in Writer mode: prompts for a URL, then applies underline +
    a link ID (see AddLinkURL) to the current selection.
*/
void DoLinkHidden(void)
{
    Str255 url;

    if ((**gHiddenTE).selStart == (**gHiddenTE).selEnd)
        return;

    if (ShowLinkURLDialog(url)) {
        TextStyle ts;

        ts.tsFace = underline;
        ts.tsColor.red = AddLinkURL(url);
        ts.tsColor.green = 0;
        ts.tsColor.blue = 0;
        TESetStyle(doFace + doColor, &ts, true, gHiddenTE);
    }
}

void ToggleCode(void)
{
    TextStyle ts;
    short lh, fa;
    short monacoFont, defaultFont;

    GetFNum("\pMonaco", &monacoFont);
    defaultFont = GetDefaultFontNum();

    TEGetStyle((**gHiddenTE).selStart, &ts, &lh, &fa, gHiddenTE);
    ts.tsFont = (ts.tsFont == monacoFont) ? defaultFont : monacoFont;
    TESetStyle(doFont, &ts, true, gHiddenTE);
}

void ToggleHeadingHidden(short level)
{
    short selStart;
    long lineStart, lineEnd;
    Handle textH;
    long len;
    TextStyle ts;
    short lh, fa;
    Boolean isThisLevel;

    selStart = (**gHiddenTE).selStart;
    textH = (**gHiddenTE).hText;
    len = (**gHiddenTE).teLength;

    HLock(textH);
    lineStart = selStart;
    while (lineStart > 0 && (*textH)[lineStart - 1] != '\r')
        lineStart--;
    lineEnd = lineStart;
    while (lineEnd < len && (*textH)[lineEnd] != '\r')
        lineEnd++;
    HUnlock(textH);

    TEGetStyle((short) lineStart, &ts, &lh, &fa, gHiddenTE);
    isThisLevel = (ts.tsFace & bold) && (ts.tsSize == CurrentFontSize() + (7 - level) * 2);

    TESetSelect((short) lineStart, (short) lineEnd, gHiddenTE);
    if (isThisLevel) {
        ts.tsFace = normal;
        ts.tsSize = CurrentFontSize();
    } else {
        ts.tsFace = bold;
        ts.tsSize = CurrentFontSize() + (7 - level) * 2;
    }
    TESetStyle(doFace + doSize, &ts, true, gHiddenTE);
}

/*
    Sets the style at a zero-length selection (the insertion point) --
    Style TextEdit uses this as the style for whatever gets typed next,
    which is exactly what's needed after closing a live-converted span
    so typing doesn't keep inheriting bold/italic/code indefinitely.
*/
static void SetTypingStyleNormal(short pos)
{
    TextStyle ts;
    short fontNum;

    fontNum = GetDefaultFontNum();
    ts.tsFont = fontNum;
    ts.tsFace = normal;
    ts.tsSize = CurrentFontSize();
    TESetSelect(pos, pos, gHiddenTE);
    TESetStyle(doFont + doFace + doSize, &ts, true, gHiddenTE);
}

/*
    Live "type the markdown, get the formatting" for Writer mode: called
    after every keystroke. Looks backward from the caret for a delimiter
    pair that the just-typed character completed, and if found, strips
    both delimiters and applies the corresponding style in place.
    Strikethrough has no native classic Mac text style, so it stays
    menu-only; everything else, including links, converts live.
*/
void DetectInlineMarkdown(char justTyped)
{
    Handle textH;
    long len;
    long caret;
    long lineStart;
    long lineEnd;

    if (justTyped == '\r') {
        SetTypingStyleNormal((**gHiddenTE).selEnd);
        return;
    }

    textH = (**gHiddenTE).hText;
    len = (**gHiddenTE).teLength;
    caret = (**gHiddenTE).selEnd;
    
    if (caret >= 2 && (*textH)[caret - 2] == '\\') {
        return; // justTyped is escaped, do not process
    }

    HLock(textH);

    lineStart = caret;
    while (lineStart > 0 && (*textH)[lineStart - 1] != '\r')
        lineStart--;

    lineEnd = caret;
    while (lineEnd < len && (*textH)[lineEnd] != '\r')
        lineEnd++;

    if (justTyped == ' ') {
        short level = 0;
        long p = lineStart;

        while (level < 6 && p < caret - 1 && (*textH)[p] == '#') {
            level++;
            p++;
        }
        if (level > 0 && p == caret - 1) {
            TextStyle ts;

            HUnlock(textH);
            TESetSelect((short) lineStart, (short) caret, gHiddenTE);
            TEDelete(gHiddenTE);
            TESetSelect((short) lineStart, (short) lineStart, gHiddenTE);
            ts.tsFace = bold;
            ts.tsSize = CurrentFontSize() + (7 - level) * 2;
            TESetStyle(doFace + doSize, &ts, true, gHiddenTE);
            InvalidateHeightCache();
            return;
        }

        if (caret >= 2) {
            long p2 = caret - 2;
            long bqCount = 0;
            while (p2 >= lineStart && (*textH)[p2] == '>') {
                bqCount++;
                p2--;
            }
            while (p2 >= lineStart && ((*textH)[p2] == ' ' || (*textH)[p2] == '\t')) {
                p2--;
            }
            
            if (p2 < lineStart && bqCount > 0) {
                TextStyle ts;
                HUnlock(textH);
                
                TESetSelect((short) lineStart, (short) caret, gHiddenTE);
                TEDelete(gHiddenTE);
                
                ts.tsFace = italic;
                ts.tsColor.red = 0;
                ts.tsColor.green = 0;
                ts.tsColor.blue = 1 + bqCount;
                
                TESetSelect((short) lineStart, (short) lineStart, gHiddenTE);
                TESetStyle(doFace + doColor, &ts, true, gHiddenTE);
                SetTypingStyleNormal((short) lineStart);
                InvalidateHeightCache();
                return;
            }
        }
        
        short scan = lineStart;
        while (scan < caret && ((*textH)[scan] == ' ' || (*textH)[scan] == '\t')) scan++;
        
        if (caret - 1 == scan + 5 && ((*textH)[scan] == '-' || (*textH)[scan] == '+' || (*textH)[scan] == '*') && (*textH)[scan + 1] == ' ' &&
            (*textH)[scan + 2] == '[' && ((*textH)[scan + 3] == ' ' || (*textH)[scan + 3] == 'x' || (*textH)[scan + 3] == 'X') && (*textH)[scan + 4] == ']' && (*textH)[scan + 5] == ' ') {
            
            Boolean isChecked = ((*textH)[scan + 3] == 'x' || (*textH)[scan + 3] == 'X');
            HUnlock(textH);
            
            TESetSelect((short) scan, (short) caret, gHiddenTE);
            TEDelete(gHiddenTE);
            
            char taskStr[5];
            taskStr[0] = '[';
            taskStr[1] = isChecked ? 'x' : ' ';
            taskStr[2] = ']';
            taskStr[3] = ' ';
            taskStr[4] = 0;
            
            TESetSelect((short) scan, (short) scan, gHiddenTE);
            TEInsert(taskStr, 4, gHiddenTE);
            InvalidateHeightCache();
            return;
        }
        if (caret - 1 == scan + 1 && ((*textH)[scan] == '-' || (*textH)[scan] == '+' || (*textH)[scan] == '*')) {
            short spaceCount = scan - lineStart;
            short nestingLevel = spaceCount / 2;
            char bulletChar = '\245';
            if (nestingLevel == 1) {
                bulletChar = 'o';
            } else if (nestingLevel >= 2) {
                bulletChar = '-';
            }
            
            HUnlock(textH);
            TESetSelect((short) scan, (short) caret, gHiddenTE);
            TEDelete(gHiddenTE);
            
            char bulletStr[3];
            bulletStr[0] = bulletChar;
            bulletStr[1] = ' ';
            bulletStr[2] = 0;
            
            TESetSelect((short) scan, (short) scan, gHiddenTE);
            TEInsert(bulletStr, 2, gHiddenTE);
            InvalidateHeightCache();
            return;
        }
    } else if (justTyped == '*' || justTyped == '_') {
        if (caret >= 4 && (*textH)[caret - 2] == justTyped && (*textH)[caret - 1] == justTyped) {
            long p = caret - 4;

            while (p >= lineStart) {
                if ((*textH)[p] == justTyped && (*textH)[p + 1] == justTyped && p + 2 < caret - 2) {
                    long innerStart = p + 2;
                    long innerEnd = caret - 2;
                    TextStyle ts;

                    HUnlock(textH);
                    TESetSelect((short) innerEnd, (short) caret, gHiddenTE);
                    TEDelete(gHiddenTE);
                    TESetSelect((short) p, (short) innerStart, gHiddenTE);
                    TEDelete(gHiddenTE);

                    ts.tsFace = bold;
                    TESetSelect((short) p, (short) (innerEnd - 2), gHiddenTE);
                    TESetStyle(doFace, &ts, true, gHiddenTE);
                    SetTypingStyleNormal((short) (innerEnd - 2));
                    InvalidateHeightCache();
                    return;
                }
                p--;
            }

            /* No opening ** behind the caret -- the just-typed ** may
               instead be an OPENING delimiter for a closing ** that's
               already sitting later in the line (going back to bold
               text that was typed earlier, closing delimiter first). */
            {
                long q = caret + 1;

                while (q + 1 < lineEnd) {
                    if ((*textH)[q] == justTyped && (*textH)[q + 1] == justTyped) {
                        long innerEnd = q;
                        TextStyle ts;

                        HUnlock(textH);
                        TESetSelect((short) innerEnd, (short) (innerEnd + 2), gHiddenTE);
                        TEDelete(gHiddenTE);
                        TESetSelect((short) (caret - 2), (short) caret, gHiddenTE);
                        TEDelete(gHiddenTE);

                        ts.tsFace = bold;
                        TESetSelect((short) (caret - 2), (short) (innerEnd - 2), gHiddenTE);
                        TESetStyle(doFace, &ts, true, gHiddenTE);
                        SetTypingStyleNormal((short) (caret - 2));
                        InvalidateHeightCache();
                        return;
                    }
                    q++;
                }
            }
        } else if (caret >= 3 && (*textH)[caret - 2] != justTyped) {
            long p = caret - 2;

            while (p >= lineStart) {
                if ((*textH)[p] == justTyped &&
                    (p == lineStart || (*textH)[p - 1] != justTyped) &&
                    (*textH)[p + 1] != justTyped && p + 1 < caret - 1) {
                    long innerStart = p + 1;
                    long innerEnd = caret - 1;
                    TextStyle ts;

                    HUnlock(textH);
                    TESetSelect((short) innerEnd, (short) caret, gHiddenTE);
                    TEDelete(gHiddenTE);
                    TESetSelect((short) p, (short) innerStart, gHiddenTE);
                    TEDelete(gHiddenTE);

                    ts.tsFace = italic;
                    TESetSelect((short) p, (short) (innerEnd - 1), gHiddenTE);
                    TESetStyle(doFace, &ts, true, gHiddenTE);
                    SetTypingStyleNormal((short) (innerEnd - 1));
                    InvalidateHeightCache();
                    return;
                }
                p--;
            }

            /* No opening * behind the caret -- the just-typed * may
               instead be an OPENING italic delimiter for a closing *
               that's already sitting later in the line. */
            {
                long q = caret;

                while (q < lineEnd) {
                    if ((*textH)[q] == justTyped &&
                        (*textH)[q - 1] != justTyped &&
                        (q + 1 == lineEnd || (*textH)[q + 1] != justTyped) &&
                        q > caret) {
                        long innerEnd = q;
                        TextStyle ts;

                        HUnlock(textH);
                        TESetSelect((short) innerEnd, (short) (innerEnd + 1), gHiddenTE);
                        TEDelete(gHiddenTE);
                        TESetSelect((short) (caret - 1), (short) caret, gHiddenTE);
                        TEDelete(gHiddenTE);

                        ts.tsFace = italic;
                        TESetSelect((short) (caret - 1), (short) (innerEnd - 1), gHiddenTE);
                        TESetStyle(doFace, &ts, true, gHiddenTE);
                        SetTypingStyleNormal((short) (caret - 1));
                        InvalidateHeightCache();
                        return;
                    }
                    q++;
                }
            }
        }
    } else if (justTyped == '`') {
        long p = caret - 2;

        while (p >= lineStart) {
            if ((*textH)[p] == '`' && p + 1 < caret - 1) {
                long innerStart = p + 1;
                long innerEnd = caret - 1;
                TextStyle ts;

                HUnlock(textH);
                TESetSelect((short) innerEnd, (short) caret, gHiddenTE);
                TEDelete(gHiddenTE);
                TESetSelect((short) p, (short) innerStart, gHiddenTE);
                TEDelete(gHiddenTE);

                GetFNum("\pMonaco", &ts.tsFont);
                TESetSelect((short) p, (short) (innerEnd - 1), gHiddenTE);
                TESetStyle(doFont, &ts, true, gHiddenTE);
                SetTypingStyleNormal((short) (innerEnd - 1));
                InvalidateHeightCache();
                return;
            }
            p--;
        }

        /* No opening ` behind the caret -- the just-typed ` may instead
           be an OPENING code delimiter for a closing ` already sitting
           later in the line. */
        {
            long q = caret;

            while (q < lineEnd) {
                if ((*textH)[q] == '`' && q > caret) {
                    long innerEnd = q;
                    TextStyle ts;

                    HUnlock(textH);
                    TESetSelect((short) innerEnd, (short) (innerEnd + 1), gHiddenTE);
                    TEDelete(gHiddenTE);
                    TESetSelect((short) (caret - 1), (short) caret, gHiddenTE);
                    TEDelete(gHiddenTE);

                    GetFNum("\pMonaco", &ts.tsFont);
                    TESetSelect((short) (caret - 1), (short) (innerEnd - 1), gHiddenTE);
                    TESetStyle(doFont, &ts, true, gHiddenTE);
                    SetTypingStyleNormal((short) (caret - 1));
                    InvalidateHeightCache();
                    return;
                }
                q++;
            }
        }
    } else if (justTyped == ')') {
        long closeParenPos = caret - 1;
        long p = closeParenPos - 1;

        while (p >= lineStart && (*textH)[p] != '(')
            p--;

        if (p >= lineStart && p > lineStart && (*textH)[p - 1] == ']') {
            long openParenPos = p;
            long closeBracketPos = openParenPos - 1;
            long urlStart = openParenPos + 1;
            long urlLen = closeParenPos - urlStart;
            long q = closeBracketPos - 1;

            while (q >= lineStart && (*textH)[q] != '[')
                q--;

            if (q >= lineStart) {
                long openBracketPos = q;
                Str255 url;
                short linkID;
                TextStyle ts;

                if (urlLen < 0) urlLen = 0;
                if (urlLen > 255) urlLen = 255;
                url[0] = (unsigned char) urlLen;
                BlockMove(*textH + urlStart, url + 1, urlLen);

                HUnlock(textH);

                TESetSelect((short) closeBracketPos, (short) caret, gHiddenTE);
                TEDelete(gHiddenTE);
                TESetSelect((short) openBracketPos, (short) (openBracketPos + 1), gHiddenTE);
                TEDelete(gHiddenTE);

                linkID = AddLinkURL(url);

                ts.tsFace = underline;
                ts.tsColor.red = linkID;
                ts.tsColor.green = 0;
                ts.tsColor.blue = 0;
                TESetSelect((short) openBracketPos, (short) (closeBracketPos - 1), gHiddenTE);
                TESetStyle(doFace + doColor, &ts, true, gHiddenTE);
                SetTypingStyleNormal((short) (closeBracketPos - 1));
                InvalidateHeightCache();
                return;
            }
        }
    } else if (justTyped == '>') {
        if (caret >= 2) {
            long p = caret - 2;
            while (p >= lineStart && (*textH)[p] != '<') p--;
            if (p >= lineStart) {
                long urlLen = caret - 1 - (p + 1);
                if (urlLen > 7 && urlLen < 255) {
                    if (((*textH)[p+1] == 'h' && (*textH)[p+2] == 't' && (*textH)[p+3] == 't' && (*textH)[p+4] == 'p') ||
                        ((*textH)[p+1] == 'm' && (*textH)[p+2] == 'a' && (*textH)[p+3] == 'i' && (*textH)[p+4] == 'l')) {
                        
                        Str255 url;
                        TextStyle ts;
                        short linkID;
                        
                        url[0] = (unsigned char) urlLen;
                        BlockMove(*textH + p + 1, url + 1, urlLen);
                        linkID = AddLinkURL(url);

                        HUnlock(textH);
                        TESetSelect((short) (caret - 1), (short) caret, gHiddenTE);
                        TEDelete(gHiddenTE);
                        TESetSelect((short) p, (short) (p + 1), gHiddenTE);
                        TEDelete(gHiddenTE);

                        ts.tsFace = underline;
                        ts.tsColor.red = linkID;
                        ts.tsColor.green = 0;
                        ts.tsColor.blue = 0;

                        TESetSelect((short) p, (short) (caret - 2), gHiddenTE);
                        TESetStyle(doFace + doColor, &ts, false, gHiddenTE);
                        SetTypingStyleNormal((short) (caret - 2));
                        InvalidateHeightCache();
                        return;
                    }
                }
            }
        }
    } else if (justTyped == '~') {
        if (caret >= 4 && (*textH)[caret - 2] == '~' && (*textH)[caret - 1] == '~') {
            long p = caret - 4;
            while (p >= lineStart) {
                if ((*textH)[p] == '~' && (*textH)[p + 1] == '~' && p + 2 < caret - 2) {
                    long innerStart = p + 2;
                    long innerEnd = caret - 2;
                    TextStyle ts;

                    HUnlock(textH);
                    TESetSelect((short) innerEnd, (short) caret, gHiddenTE);
                    TEDelete(gHiddenTE);
                    TESetSelect((short) p, (short) innerStart, gHiddenTE);
                    TEDelete(gHiddenTE);

                    ts.tsFace = condense;
                    TESetSelect((short) p, (short) (innerEnd - 2), gHiddenTE);
                    TESetStyle(doFace, &ts, true, gHiddenTE);
                    SetTypingStyleNormal((short) (innerEnd - 2));
                    InvalidateHeightCache();
                    return;
                }
                p--;
            }
        }
    } else if (justTyped == '=') {
        if (caret >= 4 && (*textH)[caret - 2] == '=' && (*textH)[caret - 1] == '=') {
            long p = caret - 4;
            while (p >= lineStart) {
                if ((*textH)[p] == '=' && (*textH)[p + 1] == '=' && p + 2 < caret - 2) {
                    long innerStart = p + 2;
                    long innerEnd = caret - 2;
                    TextStyle ts;

                    HUnlock(textH);
                    TESetSelect((short) innerEnd, (short) caret, gHiddenTE);
                    TEDelete(gHiddenTE);
                    TESetSelect((short) p, (short) innerStart, gHiddenTE);
                    TEDelete(gHiddenTE);

                    ts.tsFace = outline;
                    TESetSelect((short) p, (short) (innerEnd - 2), gHiddenTE);
                    TESetStyle(doFace, &ts, true, gHiddenTE);
                    SetTypingStyleNormal((short) (innerEnd - 2));
                    InvalidateHeightCache();
                    return;
                }
                p--;
            }
        }
    }

    HUnlock(textH);
}

/* "None" in Writer mode: just clear the applied style on the selection. */
void ClearSelectionStyleHidden(void)
{
    TextStyle ts;
    short fontNum;

    if ((**gHiddenTE).selStart == (**gHiddenTE).selEnd)
        return;

    fontNum = GetDefaultFontNum();
    ts.tsFont = fontNum;
    ts.tsFace = normal;
    ts.tsSize = CurrentFontSize();
    ts.tsColor.red = ts.tsColor.green = ts.tsColor.blue = 0;
    TESetStyle(doFont + doFace + doSize + doColor, &ts, true, gHiddenTE);
}

/*
    "None" in Markdown mode: strips any matched markdown delimiter pairs
    that fall entirely within the selection. Delimiters that extend
    outside the selection are left alone -- to clear those,
    extend the selection to include them, or toggle the specific Style
    menu item that applied them.
*/
void ClearMarkdownInSelection(void)
{
    Handle textH;
    short selStart, selEnd;
    Handle outH;
    long outLen;
    long i;

    selStart = (**gTE).selStart;
    selEnd = (**gTE).selEnd;
    if (selStart == selEnd)
        return;

    textH = (**gTE).hText;
    outH = NewHandle(selEnd - selStart + 1);
    outLen = 0;

    HLock(textH);
    HLock(outH);

    i = selStart;
    while (i < selEnd) {
        if (i == 0 || (*textH)[i - 1] == '\r') {
            short level = 0;
            long p = i;

            while (level < 6 && p < selEnd && (*textH)[p] == '#') {
                level++;
                p++;
            }
            if (level > 0 && p < selEnd && (*textH)[p] == ' ') {
                i = p + 1;
                continue;
            }
        }

        if (i + 1 < selEnd && ((*textH)[i] == '*' || (*textH)[i] == '_') && (*textH)[i] == (*textH)[i + 1]) {
            char delim = (*textH)[i];
            long j = i + 2;

            while (j + 1 < selEnd && !((*textH)[j] == delim && (*textH)[j + 1] == delim))
                j++;
            if (j + 1 < selEnd) {
                long k;

                for (k = i + 2; k < j; k++)
                    (*outH)[outLen++] = (*textH)[k];
                i = j + 2;
                continue;
            }
        }
        if ((*textH)[i] == '*' || (*textH)[i] == '_') {
            char delim = (*textH)[i];
            long j = i + 1;

            while (j < selEnd && (*textH)[j] != delim)
                j++;
            if (j < selEnd) {
                long k;

                for (k = i + 1; k < j; k++)
                    (*outH)[outLen++] = (*textH)[k];
                i = j + 1;
                continue;
            }
        }
        if ((*textH)[i] == '`') {
            long j = i + 1;

            while (j < selEnd && (*textH)[j] != '`')
                j++;
            if (j < selEnd) {
                long k;

                for (k = i + 1; k < j; k++)
                    (*outH)[outLen++] = (*textH)[k];
                i = j + 1;
                continue;
            }
        }
        if ((*textH)[i] == '[') {
            long closeBracket = i + 1;

            while (closeBracket < selEnd && (*textH)[closeBracket] != ']')
                closeBracket++;
            if (closeBracket < selEnd && closeBracket + 1 < selEnd && (*textH)[closeBracket + 1] == '(') {
                long closeParen = closeBracket + 2;

                while (closeParen < selEnd && (*textH)[closeParen] != ')')
                    closeParen++;
                if (closeParen < selEnd) {
                    long k;

                    for (k = i + 1; k < closeBracket; k++)
                        (*outH)[outLen++] = (*textH)[k];
                    i = closeParen + 1;
                    continue;
                }
            }
        }

        (*outH)[outLen++] = (*textH)[i];
        i++;
    }

    HUnlock(textH);
    HUnlock(outH);

    TESetSelect(selStart, selEnd, gTE);
    TEDelete(gTE);
    TEInsert(*outH, outLen, gTE);
    DisposeHandle(outH);

    TESetSelect(selStart, (short) (selStart + outLen), gTE);
}



void LoadTextWindow(long startOffset)
{
    long len, copyLen;
    Handle srcH;
    TEHandle te = gActiveTE;
    Rect savedViewRect;
    Rect hiddenRect;
    TextStyle baseStyle;
    short fontNum;
    
    if (gHideMarkdown) {
        srcH = gWriterText;
        len = gWriterLen;
    } else {
        srcH = gMarkdownText;
        len = gMarkdownLen;
    }
    
    if (srcH == NULL) return;
    
    if (startOffset < 0) startOffset = 0;
    if (startOffset > len) startOffset = len;
    
    if (startOffset > 0 && startOffset < len) {
        long scan = startOffset;
        HLock(srcH);
        while (scan > 0 && (*srcH)[scan - 1] != '\r') {
            scan--;
        }
        HUnlock(srcH);
        startOffset = scan;
    }
    
    copyLen = len - startOffset;
    if (copyLen > WINDOW_SIZE) copyLen = WINDOW_SIZE;
    
    /* Snap the end to a line boundary so we never cut in the middle of a line */
    if (startOffset + copyLen < len) {
        long scan = startOffset + copyLen;
        HLock(srcH);
        while (scan > startOffset && (*srcH)[scan] != '\r') {
            scan--;
        }
        HUnlock(srcH);
        if (scan > startOffset) {
            copyLen = scan - startOffset + 1;
        }
    }
    
    gWindowStart = startOffset;
    gWindowEnd = startOffset + copyLen;
    
    /* Compute the 1-based global line number at the top of this window.
       We count '\r' characters in the backing store from byte 0 up to startOffset.
       This is O(startOffset) but only runs at window-load time (not every keystroke). */
    {
        long i;
        long lineCount = 1;
        HLock(srcH);
        for (i = 0; i < startOffset; i++) {
            if ((*srcH)[i] == '\r') lineCount++;
        }
        HUnlock(srcH);
        gWindowStartLine = lineCount;
    }
    /* Move the viewRect completely off-screen so that NO drawing happens
       during the delete/insert/style phase. Every TESetStyle, TEDelete,
       TEInsert, TECalText call clips to viewRect – with it off-screen the
       window stays perfectly still until we restore it at the very end. */
    savedViewRect = (**te).viewRect;
    SetRect(&hiddenRect, OFFSCREEN_COORD, OFFSCREEN_COORD,
            OFFSCREEN_COORD + (savedViewRect.right - savedViewRect.left),
            OFFSCREEN_COORD + (savedViewRect.bottom - savedViewRect.top));
    (**te).viewRect = hiddenRect;
    /* destRect must also follow so TECalText doesn't miscalculate line widths */
    (**te).destRect = hiddenRect;
    
    /* 1. Delete all existing text */
    TESetSelect(0, (**te).teLength, te);
    TEDelete(te);
    
    /* 2. Insert the new chunk – text arrives unstyled (inherits last run) */
    HLock(srcH);
    TEInsert(*srcH + startOffset, copyLen, te);
    HUnlock(srcH);
    
    /* 3. Apply a uniform base style across everything so we start clean */
    fontNum = GetDefaultFontNum();
    baseStyle.tsFont  = fontNum;
    baseStyle.tsFace  = normal;
    baseStyle.tsSize  = CurrentFontSize();
    baseStyle.tsColor.red = baseStyle.tsColor.green = baseStyle.tsColor.blue = 0;
    TESetSelect(0, (**te).teLength, te);
    TESetStyle(doFont + doFace + doSize + doColor, &baseStyle, false, te);
    
    /* 4. Apply any Writer-mode syntax highlight ops */
    if (gHideMarkdown && gWriterOpsH != NULL) {
        short k;
        StyleOp *ops;
        
        HLock(gWriterOpsH);
        ops = (StyleOp *) *gWriterOpsH;
        for (k = 0; k < gWriterOpCount; k++) {
            long opStart, opEnd;
            TextStyle opStyle;
            
            if (ops[k].end <= startOffset || ops[k].start >= gWindowEnd)
                continue;
                
            opStart = ops[k].start - startOffset;
            opEnd   = ops[k].end   - startOffset;
            if (opStart < 0)        opStart = 0;
            if (opEnd > copyLen)    opEnd   = copyLen;
            
            TESetSelect((short)opStart, (short)opEnd, te);
            switch (ops[k].kind) {
                case 'X':
                    opStyle.tsFace = bold | italic;
                    TESetStyle(doFace, &opStyle, false, te);
                    break;
                case 'B':
                    opStyle.tsFace = bold;
                    TESetStyle(doFace, &opStyle, false, te);
                    break;
                case 'I':
                    opStyle.tsFace = italic;
                    TESetStyle(doFace, &opStyle, false, te);
                    break;
                case 'C':
                    GetFNum("\pMonaco", &opStyle.tsFont);
                    opStyle.tsFace = normal;
                    TESetStyle(doFont + doFace, &opStyle, false, te);
                    break;
                case 'L':
                    opStyle.tsFace = underline;
                    opStyle.tsColor.red = ops[k].linkID;
                    opStyle.tsColor.green = 0;
                    opStyle.tsColor.blue = 0;
                    TESetStyle(doFace + doColor, &opStyle, false, te);
                    break;
                case 'H':
                    opStyle.tsFace = bold;
                    opStyle.tsSize = CurrentFontSize() + (7 - ops[k].level) * 2;
                    TESetStyle(doFace + doSize, &opStyle, false, te);
                    break;
                case 'R':
                    opStyle.tsFace = bold;
                    opStyle.tsColor.red   = 0;
                    opStyle.tsColor.green = 0;
                    opStyle.tsColor.blue  = 1;
                    TESetStyle(doFace + doColor, &opStyle, false, te);
                    break;
                case 'Q':
                    opStyle.tsFace = italic;
                    opStyle.tsColor.red   = 0;
                    opStyle.tsColor.green = 0;
                    opStyle.tsColor.blue  = 1 + ops[k].level;
                    TESetStyle(doFace + doColor, &opStyle, false, te);
                    break;
                case 'S':
                    /* Strikethrough is parsed but rendered as normal characters */
                    break;
                case 'E':
                    opStyle.tsFace = outline;
                    TESetStyle(doFace, &opStyle, false, te);
                    break;
            }
        }
        HUnlock(gWriterOpsH);
    }
    
    /* 5. Place caret at start, reflow text */
    TESetSelect(0, 0, te);
    TECalText(te);
    
    /* 6. Restore the real viewRect and destRect (top-aligned, no scroll offset) */
    (**te).viewRect = savedViewRect;
    (**te).destRect = savedViewRect;   /* text starts at very top of view */
    
    /* 7. Erase the window area and let TEUpdate paint the freshly laid-out text */
    EraseRect(&savedViewRect);
    TEUpdate(&savedViewRect, te);
}

void SyncWindowToBacking(void)
{
    long newLen = (**gActiveTE).teLength;
    long oldLen = gWindowEnd - gWindowStart;
    long diff = newLen - oldLen;
    Handle targetH;
    long *targetLenPtr;
    
    if (gHideMarkdown) {
        targetH = gWriterText;
        targetLenPtr = &gWriterLen;
    } else {
        targetH = gMarkdownText;
        targetLenPtr = &gMarkdownLen;
    }
    
    if (targetH == NULL) return;
    
    if (diff != 0) {
        SetHandleSize(targetH, *targetLenPtr + diff);
        HLock(targetH);
        if (gWindowEnd < *targetLenPtr) {
            BlockMove(*targetH + gWindowEnd, *targetH + gWindowEnd + diff, *targetLenPtr - gWindowEnd);
        }
        HUnlock(targetH);
        *targetLenPtr += diff;
    }
    
    HLock(targetH);
    BlockMove(*(**gActiveTE).hText, *targetH + gWindowStart, newLen);
    HUnlock(targetH);
    
    if (gHideMarkdown && gWriterOpsH != NULL) {
        /* 1. Remove old style ops in this window range and adjust later ones */
        short k;
        short newCount = 0;
        StyleOp *ops;
        long oldWindowEnd = gWindowEnd;
        
        HLock(gWriterOpsH);
        ops = (StyleOp *) *gWriterOpsH;
        for (k = 0; k < gWriterOpCount; k++) {
            if (ops[k].start < oldWindowEnd && ops[k].end > gWindowStart) {
                continue;
            }
            if (ops[k].start >= oldWindowEnd) {
                ops[k].start += diff;
                ops[k].end += diff;
            } else if (ops[k].end >= oldWindowEnd) {
                ops[k].end += diff;
            }
            if (newCount != k) {
                ops[newCount] = ops[k];
            }
            newCount++;
        }
        gWriterOpCount = newCount;
        HUnlock(gWriterOpsH);
        
        /* 2. Extract current active TE style runs and add them to gWriterOpsH */
        TEStyleHandle teStyles = TEGetStyleHandle(gActiveTE);
        if (teStyles != NULL) {
            short nRuns;
            STHandle styleTab;
            
            HLock((Handle)teStyles);
            nRuns = (**teStyles).nRuns;
            styleTab = (**teStyles).styleTab;
            HLock((Handle)styleTab);
            
            short r;
            for (r = 0; r < nRuns; r++) {
                short runStart = (**teStyles).runs[r].startChar;
                short runEnd = (r + 1 < nRuns) ? (**teStyles).runs[r+1].startChar : (**gActiveTE).teLength;
                
                if (runEnd <= runStart) continue;
                
                short styleIdx = (**teStyles).runs[r].styleIndex;
                STElement style = (*styleTab)[styleIdx];

                
                Boolean isBold = (style.stFace & bold) != 0;
                Boolean isItalic = (style.stFace & italic) != 0;
                Boolean isUnderline = (style.stFace & underline) != 0;
                Boolean isHighlight = (style.stFace & outline) != 0;
                
                short headerLevel = 0;
                if (isBold && style.stSize > CurrentFontSize()) {
                    short lvl;
                    for (lvl = 1; lvl <= 6; lvl++) {
                        if (style.stSize == CurrentFontSize() + (7 - lvl) * 2) {
                            headerLevel = lvl;
                            break;
                        }
                    }
                }
                
                short monacoFontNum;
                GetFNum("\pMonaco", &monacoFontNum);
                Boolean isCode = (style.stFont == monacoFontNum);
                
                Boolean isHR = isBold && (style.stColor.blue == 1);
                
                Boolean isBlockquote = (style.stColor.blue >= 2);
                short bqDepth = isBlockquote ? (style.stColor.blue - 1) : 0;
                
                short linkID = 0;
                if (isUnderline && style.stColor.red > 0) {
                    linkID = style.stColor.red;
                }
                
                long globalStart = gWindowStart + runStart;
                long globalEnd = gWindowStart + runEnd;
                
                HLock(gWriterOpsH);
                ops = (StyleOp *) *gWriterOpsH;
                
                if (isHR) {
                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = globalStart;
                        ops[gWriterOpCount].end = globalEnd;
                        ops[gWriterOpCount].kind = 'R';
                        ops[gWriterOpCount].level = 0;
                        ops[gWriterOpCount].linkID = 0;
                        gWriterOpCount++;
                    }
                } else if (isBlockquote) {
                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = globalStart;
                        ops[gWriterOpCount].end = globalEnd;
                        ops[gWriterOpCount].kind = 'Q';
                        ops[gWriterOpCount].level = bqDepth;
                        ops[gWriterOpCount].linkID = 0;
                        gWriterOpCount++;
                    }
                } else if (headerLevel > 0) {
                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = globalStart;
                        ops[gWriterOpCount].end = globalEnd;
                        ops[gWriterOpCount].kind = 'H';
                        ops[gWriterOpCount].level = headerLevel;
                        ops[gWriterOpCount].linkID = 0;
                        gWriterOpCount++;
                    }
                } else {
                    if (isBold && isItalic) {
                        if (gWriterOpCount < MAX_STYLE_OPS) {
                            ops[gWriterOpCount].start = globalStart;
                            ops[gWriterOpCount].end = globalEnd;
                            ops[gWriterOpCount].kind = 'X';
                            ops[gWriterOpCount].level = 0;
                            ops[gWriterOpCount].linkID = 0;
                            gWriterOpCount++;
                        }
                    } else {
                        if (isBold) {
                            if (gWriterOpCount < MAX_STYLE_OPS) {
                                ops[gWriterOpCount].start = globalStart;
                                ops[gWriterOpCount].end = globalEnd;
                                ops[gWriterOpCount].kind = 'B';
                                ops[gWriterOpCount].level = 0;
                                ops[gWriterOpCount].linkID = 0;
                                gWriterOpCount++;
                            }
                        }
                        if (isItalic) {
                            if (gWriterOpCount < MAX_STYLE_OPS) {
                                ops[gWriterOpCount].start = globalStart;
                                ops[gWriterOpCount].end = globalEnd;
                                ops[gWriterOpCount].kind = 'I';
                                ops[gWriterOpCount].level = 0;
                                ops[gWriterOpCount].linkID = 0;
                                gWriterOpCount++;
                            }
                        }
                        if (isHighlight) {
                            if (gWriterOpCount < MAX_STYLE_OPS) {
                                ops[gWriterOpCount].start = globalStart;
                                ops[gWriterOpCount].end = globalEnd;
                                ops[gWriterOpCount].kind = 'E';
                                ops[gWriterOpCount].level = 0;
                                ops[gWriterOpCount].linkID = 0;
                                gWriterOpCount++;
                            }
                        }
                    }
                    if (isCode) {
                        if (gWriterOpCount < MAX_STYLE_OPS) {
                            ops[gWriterOpCount].start = globalStart;
                            ops[gWriterOpCount].end = globalEnd;
                            ops[gWriterOpCount].kind = 'C';
                            ops[gWriterOpCount].level = 0;
                            ops[gWriterOpCount].linkID = 0;
                            gWriterOpCount++;
                        }
                    }
                    if (linkID > 0) {
                        if (gWriterOpCount < MAX_STYLE_OPS) {
                            ops[gWriterOpCount].start = globalStart;
                            ops[gWriterOpCount].end = globalEnd;
                            ops[gWriterOpCount].kind = 'L';
                            ops[gWriterOpCount].level = 0;
                            ops[gWriterOpCount].linkID = linkID;
                            gWriterOpCount++;
                        }
                    }
                }
                HUnlock(gWriterOpsH);
            }
            HUnlock((Handle)styleTab);
            HUnlock((Handle)teStyles);
        }
    }
    
    gWindowEnd += diff;
}


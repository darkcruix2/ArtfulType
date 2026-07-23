#include <Types.h>
#include <Memory.h>
#include <OSUtils.h>
#include <ToolUtils.h>
#include <string.h>
#include <stdio.h>

#include "app.h"

static long GetTESelStart(WEHandle we) { long s, e; WEGetSelection(&s, &e, we); return s; }
static long GetTESelEnd(WEHandle we) { long s, e; WEGetSelection(&s, &e, we); return e; }

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
    WETextStyle ts;
    short fontNum;
    short savedStart = GetTESelStart(gActiveTE);
    short savedEnd = GetTESelEnd(gActiveTE);

    if (!gHideMarkdown) {
        short monoFont = 0;
        GetFNum("\pMonaco", &monoFont);
        if (monoFont == 0) GetFNum("\pCourier", &monoFont);
        fontNum = (monoFont != 0) ? monoFont : GetDefaultFontNum();
        ts.tsSize = 12;
    } else {
        fontNum = GetDefaultFontNum();
        ts.tsSize = CurrentFontSize();
    }
        
    ts.tsFont = fontNum;
    ts.tsFace = normal;
    ts.tsColor.red = ts.tsColor.green = ts.tsColor.blue = 0;

    WESetSelect(0, 32767, gActiveTE);
    WESetStyle(weDoFont + weDoFace + weDoSize + weDoColor, &ts, gActiveTE);
    WECalText(gActiveTE);

    WESetSelect(savedStart, savedEnd, gActiveTE);
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

void SuppressDrawing(WEHandle te, Rect *saved)
{
    LongRect viewRectLong;
    WEGetViewRect(&viewRectLong, te);
    saved->left = (short)viewRectLong.left;
    saved->top = (short)viewRectLong.top;
    saved->right = (short)viewRectLong.right;
    saved->bottom = (short)viewRectLong.bottom;
    
    Rect hiddenRect;
    SetRect(&hiddenRect, OFFSCREEN_COORD, OFFSCREEN_COORD,
            OFFSCREEN_COORD + 100, OFFSCREEN_COORD + 100);
    WESetRects(&hiddenRect, &hiddenRect, te);
}

void RestoreDrawing(WEHandle te, Rect *saved)
{
    WESetRects(saved, saved, te);
}

/*
    ParseInlineContent: parse inline markdown formatting (bold, italic, code,
    links, etc.) from srcH[start..end) and append stripped output to outH at
    *outLenPtr.  Style ops are appended to the ops array via gWriterOpCount.
    srcH and outH must be locked by the caller.
*/
static void ParseInlineContent(Handle srcH, long start, long end, Handle outH, long *outLenPtr, StyleOp *ops)
{
    long i = start;
    long outLen = *outLenPtr;

    while (i < end) {
        /* Escape sequences */
        if (i + 1 < end && (*srcH)[i] == '\\' && ((*srcH)[i+1] == '*' || (*srcH)[i+1] == '_' || (*srcH)[i+1] == '#' || (*srcH)[i+1] == '>' || (*srcH)[i+1] == '[' || (*srcH)[i+1] == '`' || (*srcH)[i+1] == '\\')) {
            (*outH)[outLen++] = (*srcH)[i+1];
            i += 2;
            continue;
        }

        /* ***bold+italic*** */
        if (i + 2 < end && ((*srcH)[i] == '*' || (*srcH)[i] == '_') && (*srcH)[i] == (*srcH)[i + 1] && (*srcH)[i] == (*srcH)[i + 2]) {
            char delim = (*srcH)[i];
            long j = i + 3;
            while (j + 2 < end && ((*srcH)[j] != delim || (*srcH)[j + 1] != delim || (*srcH)[j + 2] != delim))
                j++;
            if (j + 2 < end) {
                long outStart = outLen, m;
                for (m = i + 3; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'X';
                    ops[gWriterOpCount].level = 0;
                    ops[gWriterOpCount].linkID = 0;
                    gWriterOpCount++;
                }
                i = j + 3;
                continue;
            }
        }

        /* **bold** */
        if (i + 1 < end && ((*srcH)[i] == '*' || (*srcH)[i] == '_') && (*srcH)[i] == (*srcH)[i + 1]) {
            char delim = (*srcH)[i];
            long j = i + 2;
            while (j + 1 < end && ((*srcH)[j] != delim || (*srcH)[j + 1] != delim))
                j++;
            if (j + 1 < end) {
                long outStart = outLen, m;
                for (m = i + 2; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'B';
                    ops[gWriterOpCount].level = 0;
                    ops[gWriterOpCount].linkID = 0;
                    gWriterOpCount++;
                }
                i = j + 2;
                continue;
            }
        }
        /* *italic* */
        if ((*srcH)[i] == '*' || (*srcH)[i] == '_') {
            char delim = (*srcH)[i];
            long j = i + 1;
            while (j < end && (*srcH)[j] != delim)
                j++;
            if (j < end) {
                long outStart = outLen, m;
                for (m = i + 1; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'I';
                    ops[gWriterOpCount].level = 0;
                    ops[gWriterOpCount].linkID = 0;
                    gWriterOpCount++;
                }
                i = j + 1;
                continue;
            }
        }
        /* ~~strikethrough~~ */
        if (i + 1 < end && (*srcH)[i] == '~' && (*srcH)[i + 1] == '~') {
            long j = i + 2;
            while (j + 1 < end && ((*srcH)[j] != '~' || (*srcH)[j + 1] != '~'))
                j++;
            if (j + 1 < end) {
                long outStart = outLen, m;
                for (m = i + 2; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'S';
                    ops[gWriterOpCount].level = 0;
                    ops[gWriterOpCount].linkID = 0;
                    gWriterOpCount++;
                }
                i = j + 2;
                continue;
            }
        }
        /* ~subscript~ */
        if ((*srcH)[i] == '~' && i + 1 < end && (*srcH)[i + 1] != '~') {
            long j = i + 1;
            while (j < end && (*srcH)[j] != '~' && (*srcH)[j] != '\r')
                j++;
            if (j < end && (*srcH)[j] == '~' && j > i + 1) {
                long outStart = outLen, m;
                for (m = i + 1; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'D';
                    ops[gWriterOpCount].level = 0;
                    ops[gWriterOpCount].linkID = 0;
                    gWriterOpCount++;
                }
                i = j + 1;
                continue;
            }
        }
        /* ^superscript^ */
        if ((*srcH)[i] == '^') {
            long j = i + 1;
            while (j < end && (*srcH)[j] != '^' && (*srcH)[j] != '\r')
                j++;
            if (j < end && (*srcH)[j] == '^' && j > i + 1) {
                long outStart = outLen, m;
                for (m = i + 1; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'P';
                    ops[gWriterOpCount].level = 0;
                    ops[gWriterOpCount].linkID = 0;
                    gWriterOpCount++;
                }
                i = j + 1;
                continue;
            }
        }
        /* ==highlight== */
        if (i + 1 < end && (*srcH)[i] == '=' && (*srcH)[i + 1] == '=') {
            long j = i + 2;
            while (j + 1 < end && ((*srcH)[j] != '=' || (*srcH)[j + 1] != '='))
                j++;
            if (j + 1 < end) {
                long outStart = outLen, m;
                for (m = i + 2; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'E';
                    ops[gWriterOpCount].level = 0;
                    ops[gWriterOpCount].linkID = 0;
                    gWriterOpCount++;
                }
                i = j + 2;
                continue;
            }
        }
        /* `inline code` */
        if ((*srcH)[i] == '`') {
            long j = i + 1;
            while (j < end && (*srcH)[j] != '`')
                j++;
            if (j < end) {
                long outStart = outLen, m;
                for (m = i + 1; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'C';
                    ops[gWriterOpCount].level = 0;
                    ops[gWriterOpCount].linkID = 0;
                    gWriterOpCount++;
                }
                i = j + 1;
                continue;
            }
        }
        /* <auto-link> */
        if ((*srcH)[i] == '<') {
            long closeAngle = i + 1;
            while (closeAngle < end && (*srcH)[closeAngle] != '>')
                closeAngle++;
            if (closeAngle < end) {
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
                        if (gWriterOpCount < MAX_STYLE_OPS) {
                            ops[gWriterOpCount].start = outStart;
                            ops[gWriterOpCount].end = outLen;
                            ops[gWriterOpCount].kind = 'L';
                            ops[gWriterOpCount].level = 0;
                            ops[gWriterOpCount].linkID = AddLinkURL(url);
                            gWriterOpCount++;
                        }
                        i = closeAngle + 1;
                        continue;
                    }
                }
            }
        }
        /* [link](url) and ![image](url) */
        if ((*srcH)[i] == '[' || ((*srcH)[i] == '!' && i + 1 < end && (*srcH)[i+1] == '[')) {
            Boolean isImage = ((*srcH)[i] == '!');
            long openBracket = isImage ? i + 1 : i;
            long closeBracket = openBracket + 1;
            while (closeBracket < end && (*srcH)[closeBracket] != ']')
                closeBracket++;
            if (closeBracket < end && closeBracket + 1 < end && (*srcH)[closeBracket + 1] == '(') {
                long closeParen = closeBracket + 2;
                while (closeParen < end && (*srcH)[closeParen] != ')')
                    closeParen++;
                if (closeParen < end) {
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
                        ops[gWriterOpCount].level = 0;
                        ops[gWriterOpCount].linkID = AddLinkURL(url);
                        gWriterOpCount++;
                    }
                    i = closeParen + 1;
                    continue;
                }
            }
        }

        /* Plain character -- pass through */
        (*outH)[outLen++] = (*srcH)[i];
        i++;
    }

    *outLenPtr = outLen;
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
                    if (outLen > 0 && (*outH)[outLen - 1] == '\r') {
                        if (outLen < 2 || (*outH)[outLen - 2] != '\r') {
                            (*outH)[outLen++] = '\r';
                        }
                    }
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
                        ops[gWriterOpCount].level = 1;
                        ops[gWriterOpCount].linkID = 0;
                        gWriterOpCount++;
                    }

                    if (outLen > 0 && (*outH)[outLen - 1] == '\r') {
                        if (outLen < 2 || (*outH)[outLen - 2] != '\r') {
                            (*outH)[outLen++] = '\r';
                        }
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
                    lineEnd++;
                }
                ParseInlineContent(srcH, lineStart, lineEnd, outH, &outLen, ops);
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

                    short d;
                    for (d = 0; d < blockquoteDepth; d++) {
                        (*outH)[outLen++] = '"';
                    }
                    (*outH)[outLen++] = ' ';

                    /* Skip leading whitespace after > markers */
                    while (lineStart < len && ((*srcH)[lineStart] == ' ' || (*srcH)[lineStart] == '\t'))
                        lineStart++;

                    while (lineEnd < len && (*srcH)[lineEnd] != '\r') {
                        lineEnd++;
                    }

                    long contentStart = outLen;
                    ParseInlineContent(srcH, lineStart, lineEnd, outH, &outLen, ops);
                    
                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = outStart;
                        ops[gWriterOpCount].end = outStart + blockquoteDepth;
                        ops[gWriterOpCount].kind = 'q'; /* Quote prefix */
                        ops[gWriterOpCount].level = blockquoteDepth;
                        gWriterOpCount++;
                    }
                    
                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = contentStart;
                        ops[gWriterOpCount].end = outLen;
                        ops[gWriterOpCount].kind = 'Q'; /* Quote content */
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
                    
                    (*outH)[outLen++] = '-';
                    (*outH)[outLen++] = '-';
                    (*outH)[outLen++] = '-';
                    
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

            /* Indented code block: 4+ spaces at start of line */
            if (p - i >= 4 && (*srcH)[i] == ' ' && (*srcH)[i+1] == ' ' && (*srcH)[i+2] == ' ' && (*srcH)[i+3] == ' ') {
                long lineStart = i + 4;
                long lineEnd = lineStart;
                long outStart = outLen;

                while (lineEnd < len && (*srcH)[lineEnd] != '\r') {
                    lineEnd++;
                }
                /* Copy content with indent stripped */
                {
                    long m;
                    for (m = lineStart; m < lineEnd; m++)
                        (*outH)[outLen++] = (*srcH)[m];
                }
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'C';
                    ops[gWriterOpCount].level = 0;
                    ops[gWriterOpCount].linkID = 0;
                    gWriterOpCount++;
                }
                if (lineEnd < len && (*srcH)[lineEnd] == '\r') {
                    (*outH)[outLen++] = '\r';
                    i = lineEnd + 1;
                } else {
                    i = lineEnd;
                }
                continue;
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
                    long taskBoxStart = outLen;
                    (*outH)[outLen++] = '[';
                    (*outH)[outLen++] = isChecked ? 'X' : ' ';
                    (*outH)[outLen++] = ']';
                    (*outH)[outLen++] = ' ';
                    
                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = taskBoxStart;
                        ops[gWriterOpCount].end = taskBoxStart + 4;
                        ops[gWriterOpCount].kind = 'K'; /* Monospaced font / Task box */
                        ops[gWriterOpCount].level = isChecked ? 1 : 0;
                        ops[gWriterOpCount].linkID = 0;
                        gWriterOpCount++;
                    }
                    lineStart = p + 6;
                } else {
                    char bulletChar = '\245';
                    if (nestingLevel == 1) {
                        bulletChar = 'o';
                    } else if (nestingLevel == 2) {
                        bulletChar = 's';
                    } else if (nestingLevel == 3) {
                        bulletChar = 'o';
                    } else if (nestingLevel >= 4) {
                        bulletChar = '-';
                    }
                    long bulletStart = outLen;
                    (*outH)[outLen++] = bulletChar;
                    (*outH)[outLen++] = ' ';

                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = bulletStart;
                        ops[gWriterOpCount].end = bulletStart + 2;
                        ops[gWriterOpCount].kind = 'U'; /* Unordered bullet list */
                        ops[gWriterOpCount].level = nestingLevel;
                        ops[gWriterOpCount].linkID = 0;
                        gWriterOpCount++;
                    }
                }

                lineEnd = lineStart;
                while (lineEnd < len && (*srcH)[lineEnd] != '\r') {
                    lineEnd++;
                }
                ParseInlineContent(srcH, lineStart, lineEnd, outH, &outLen, ops);
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
        /* Single ~subscript~ */
        if ((*srcH)[i] == '~' && i + 1 < len && (*srcH)[i + 1] != '~') {
            long j = i + 1;
            while (j < len && (*srcH)[j] != '~' && (*srcH)[j] != '\r')
                j++;
            if (j < len && (*srcH)[j] == '~' && j > i + 1) {
                long outStart = outLen, m;
                for (m = i + 1; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'D';
                    gWriterOpCount++;
                }
                i = j + 1;
                continue;
            }
        }
        /* ^superscript^ */
        if ((*srcH)[i] == '^') {
            long j = i + 1;
            while (j < len && (*srcH)[j] != '^' && (*srcH)[j] != '\r')
                j++;
            if (j < len && (*srcH)[j] == '^' && j > i + 1) {
                long outStart = outLen, m;
                for (m = i + 1; m < j; m++)
                    (*outH)[outLen++] = (*srcH)[m];
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'P';
                    gWriterOpCount++;
                }
                i = j + 1;
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
        WETextStyle firstStyle;
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
            } else if (p < lineEnd && ((unsigned char)(*srcH)[p] == 0xA5 || (*srcH)[p] == 'o' || (*srcH)[p] == 's' || (*srcH)[p] == '-') && p + 1 < lineEnd && (*srcH)[p + 1] == ' ') {
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
            long p = lineStart;
            while (p < lineEnd && ((*srcH)[p] == '"' || (*srcH)[p] == '\t' || (*srcH)[p] == ' ')) {
                p++;
            }
            textOffset = p;
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
                Boolean isSuper = false, isSub = false;
                
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
                            if (ops[k].kind == 'P') isSuper = true;
                            if (ops[k].kind == 'D') isSub = true;
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
                if (isSuper) (*outH)[outLen++] = '^';
                if (isSub)   (*outH)[outLen++] = '~';
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
                if (isSub)   (*outH)[outLen++] = '~';
                if (isSuper) (*outH)[outLen++] = '^';
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

Handle EncodeSelectionAsMarkdown(long start, long end, WEHandle te)
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

    srcH = WEGetText(te);
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
            WETextStyle st;
            short dlh, dfa;

            WEGetStyle((short) i, &st, te);
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

void InsertMarkdownAsStyled(Handle srcH, long srcLen, WEHandle te)
{
    Handle outH;
    long outLen;
    long i;
    static StyleOp ops[MAX_STYLE_OPS];
    short opCount = 0;
    short insertStart;
    short k;
    WETextStyle baseStyle;
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

    insertStart = GetTESelStart(te);
    WEInsert(*outH, outLen, NULL, te);
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
    WESetSelect(insertStart, (short) (insertStart + outLen), te);
    WESetStyle(weDoFont + weDoFace + weDoSize + weDoColor, &baseStyle, te);

    for (k = 0; k < opCount; k++) {
        WETextStyle opStyle;

        WESetSelect((short) (insertStart + ops[k].start), (short) (insertStart + ops[k].end), te);
        switch (ops[k].kind) {
            case 'X':
                opStyle.tsFace = bold | italic;
                WESetStyle(weDoFace, &opStyle, te);
                break;
            case 'B':
                opStyle.tsFace = bold;
                WESetStyle(weDoFace, &opStyle, te);
                break;
            case 'I':
                opStyle.tsFace = italic;
                WESetStyle(weDoFace, &opStyle, te);
                break;
            case 'C':
                GetFNum("\pMonaco", &opStyle.tsFont);
                WESetStyle(weDoFont, &opStyle, te);
                break;
            case 'L':
            case 'M':
                opStyle.tsFace = underline;
                opStyle.tsColor.red = ops[k].linkID;
                opStyle.tsColor.green = (ops[k].kind == 'M') ? 1 : 0;
                opStyle.tsColor.blue = 0;
                WESetStyle(weDoFace + weDoColor, &opStyle, te);
                break;
            case 'Q':
                opStyle.tsFace = italic;
                opStyle.tsColor.red = 0;
                opStyle.tsColor.green = 0;
                opStyle.tsColor.blue = 1 + ops[k].level;
                WESetStyle(weDoFace + weDoColor, &opStyle, te);
                break;
        }
    }

    WESetSelect((short) (insertStart + outLen), (short) (insertStart + outLen), te);
    WECalText(te);
}

void WrapSelection(char *prefix, char *suffix)
{
    short selStart, selEnd;
    long selLen, totalLen, textLen;
    short prefixLen, suffixLen;
    Handle textH;
    Handle newH;
    Boolean outerWrapped, innerWrapped;

    selStart = GetTESelStart(gTE);
    selEnd = GetTESelEnd(gTE);
    selLen = selEnd - selStart;
    textH = WEGetText(gTE);
    textLen = WEGetTextLength(gTE);

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

        WESetSelect(selStart - prefixLen, selEnd + suffixLen, gTE);
        WEDelete(gTE);
        WEInsert(*newH, selLen, NULL, gTE);
        HUnlock(newH);
        DisposeHandle(newH);

        WESetSelect(selStart - prefixLen, selStart - prefixLen + selLen, gTE);
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

        WEDelete(gTE);
        WEInsert(*newH, innerLen, NULL, gTE);
        HUnlock(newH);
        DisposeHandle(newH);

        WESetSelect(selStart, selStart + innerLen, gTE);
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

    WEDelete(gTE);
    WEInsert(*newH, totalLen, NULL, gTE);
    HUnlock(newH);
    DisposeHandle(newH);

    WESetSelect(selStart + prefixLen, selStart + prefixLen + selLen, gTE);
}

static void ApplyLinePrefixToTE(WEHandle te, const char *prefix);

void ApplyHeading(short level)
{
    char prefix[8];
    short i;
    for (i = 0; i < level && i < 6; i++) prefix[i] = '#';
    prefix[i] = ' ';
    prefix[i+1] = '\0';
    ApplyLinePrefixToTE(gTE, prefix);
}

static Boolean LineStartsWithNumber(const char *buf, long len, long *prefixBytes)
{
    long i = 0;
    while (i < len && buf[i] >= '0' && buf[i] <= '9') i++;
    if (i > 0 && i + 1 < len && buf[i] == '.' && buf[i+1] == ' ') {
        if (prefixBytes) *prefixBytes = i + 2;
        return true;
    }
    return false;
}

static Boolean GetExistingListMarkerLen(const char *buf, long len, long *markerLen)
{
    if (len >= 6 && buf[0] == '-' && buf[1] == ' ' && buf[2] == '[' && (buf[3] == ' ' || buf[3] == 'x' || buf[3] == 'X') && buf[4] == ']' && buf[5] == ' ') {
        if (markerLen) *markerLen = 6;
        return true;
    }
    if (len >= 4 && buf[0] == '[' && (buf[1] == ' ' || buf[1] == 'x' || buf[1] == 'X') && buf[2] == ']' && buf[3] == ' ') {
        if (markerLen) *markerLen = 4;
        return true;
    }
    if (len >= 2 && (buf[0] == '-' || buf[0] == '*' || buf[0] == '+' || buf[0] == (char)'\245') && buf[1] == ' ') {
        if (markerLen) *markerLen = 2;
        return true;
    }
    long numBytes = 0;
    if (LineStartsWithNumber(buf, len, &numBytes)) {
        if (markerLen) *markerLen = numBytes;
        return true;
    }
    return false;
}

static void ApplyLinePrefixToTE(WEHandle te, const char *prefix)
{
    long selStart, selEnd;
    long textLen;
    Handle textH;
    long start, end;
    short prefixLen;
    Boolean isNumbered;

    if (!te || !prefix) return;
    prefixLen = strlen(prefix);
    isNumbered = (strcmp(prefix, "1. ") == 0);

    WEGetSelection(&selStart, &selEnd, te);
    if (selEnd < selStart) {
        long tmp = selStart; selStart = selEnd; selEnd = tmp;
    }

    textH = WEGetText(te);
    textLen = WEGetTextLength(te);
    if (textLen == 0) {
        if (isNumbered) {
            WEInsert("1. ", 3, NULL, te);
        } else {
            WEInsert((Ptr)prefix, prefixLen, NULL, te);
        }
        gDirty = true;
        return;
    }

    start = selStart;
    HLock(textH);
    while (start > 0 && (*textH)[start - 1] != '\r')
        start--;

    end = selEnd;
    if (end > selStart && end > start && (*textH)[end - 1] == '\r') {
        end--;
    }
    while (end < textLen && (*textH)[end] != '\r')
        end++;

    long lineStart = start;
    long nonEmptyCount = 0;
    long prefixedCount = 0;

    while (lineStart <= end) {
        long lineEnd = lineStart;
        while (lineEnd < end && (*textH)[lineEnd] != '\r')
            lineEnd++;
        
        long lineLen = lineEnd - lineStart;
        if (lineLen > 0) {
            nonEmptyCount++;
            if (isNumbered) {
                long mLen = 0;
                if (LineStartsWithNumber(*textH + lineStart, lineLen, &mLen)) {
                    prefixedCount++;
                }
            } else {
                if (lineLen >= prefixLen && memcmp(*textH + lineStart, prefix, prefixLen) == 0) {
                    prefixedCount++;
                }
            }
        }
        lineStart = lineEnd + 1;
    }

    Boolean removing = (nonEmptyCount > 0 && prefixedCount == nonEmptyCount);

    long maxNewLen = (end - start) + (nonEmptyCount + 2) * (prefixLen + 12) + 64;
    Handle newH = NewHandle(maxNewLen);
    if (!newH) {
        HUnlock(textH);
        return;
    }

    HLock(newH);
    long newLen = 0;
    long itemNumber = 1;
    lineStart = start;

    while (lineStart <= end) {
        long lineEnd = lineStart;
        while (lineEnd < end && (*textH)[lineEnd] != '\r')
            lineEnd++;
        
        long lineLen = lineEnd - lineStart;
        const char *linePtr = *textH + lineStart;

        if (removing) {
            long skipBytes = 0;
            if (isNumbered) {
                LineStartsWithNumber(linePtr, lineLen, &skipBytes);
            } else {
                if (lineLen >= prefixLen && memcmp(linePtr, prefix, prefixLen) == 0) {
                    skipBytes = prefixLen;
                }
            }
            long copyLen = lineLen - skipBytes;
            if (copyLen > 0) {
                BlockMove(linePtr + skipBytes, *newH + newLen, copyLen);
                newLen += copyLen;
            }
        } else {
            if (lineLen > 0 || (start == end)) {
                long existingMarkerLen = 0;
                if (isNumbered || prefix[0] == '-' || prefix[0] == '*' || prefix[0] == (char)'\245') {
                    GetExistingListMarkerLen(linePtr, lineLen, &existingMarkerLen);
                }

                if (isNumbered) {
                    char numBuf[16];
                    sprintf(numBuf, "%ld. ", itemNumber++);
                    short nLen = strlen(numBuf);
                    BlockMove(numBuf, *newH + newLen, nLen);
                    newLen += nLen;
                } else {
                    if (existingMarkerLen > 0 && lineLen >= prefixLen && memcmp(linePtr, prefix, prefixLen) == 0) {
                        existingMarkerLen = 0;
                    } else {
                        BlockMove((Ptr)prefix, *newH + newLen, prefixLen);
                        newLen += prefixLen;
                    }
                }

                long copyLen = lineLen - existingMarkerLen;
                if (copyLen > 0) {
                    BlockMove(linePtr + existingMarkerLen, *newH + newLen, copyLen);
                    newLen += copyLen;
                }
            }
        }

        if (lineEnd < end || (lineEnd < textLen && (*textH)[lineEnd] == '\r')) {
            (*newH)[newLen++] = '\r';
        }

        lineStart = lineEnd + 1;
    }

    HUnlock(textH);
    HUnlock(newH);

    WESetSelect(start, end, te);
    WEDelete(te);
    WEInsert(*newH, newLen, NULL, te);
    DisposeHandle(newH);

    WESetSelect(start, start + newLen, te);
    gDirty = true;
}

void ApplyLinePrefix(const char *prefix)
{
    ApplyLinePrefixToTE(gTE, prefix);
    SyncWindowToBacking();
    if (gWindow != NULL) {
        InvalRect(&gWindow->portRect);
    }
}

void ApplyLinePrefixHidden(const char *prefix)
{
    ApplyLinePrefixToTE(gHiddenTE, prefix);
    SyncWindowToBacking();
    SyncHiddenToCanonical();
    BuildHiddenView();
    if (gWindow != NULL) {
        InvalRect(&gWindow->portRect);
    }
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

    selStart = GetTESelStart(gTE);
    selEnd = GetTESelEnd(gTE);
    selLen = selEnd - selStart;
    textH = WEGetText(gTE);

    totalLen = 1 + selLen + midLen;
    newH = NewHandle(totalLen);
    HLock(newH);
    HLock(textH);
    (*newH)[0] = '[';
    BlockMove(*textH + selStart, *newH + 1, selLen);
    BlockMove(mid, *newH + 1 + selLen, midLen);
    HUnlock(textH);

    WEDelete(gTE);
    WEInsert(*newH, totalLen, NULL, gTE);
    HUnlock(newH);
    DisposeHandle(newH);

    cursorPos = selStart + selLen + 3;
    WESetSelect(cursorPos, cursorPos, gTE);
}

/*
    Style commands while in Hide Markdown mode apply real WETextStyle
    directly to gHiddenTE instead of inserting delimiter text -- there's
    no visible syntax to insert. Toggle state is read back from the
    style at the selection start.
*/
static Boolean SelectionHasFace(Style face)
{
    WETextStyle ts;
    short lh, fa;

    WEGetStyle(GetTESelStart(gHiddenTE), &ts, gHiddenTE);
    return (ts.tsFace & face) != 0;
}

void ToggleFace(Style face)
{
    WETextStyle ts;

    ts.tsFace = SelectionHasFace(face) ? normal : face;
    WESetStyle(weDoFace, &ts, gHiddenTE);
}

void ToggleStrike(void)
{
    WETextStyle ts;
    short fs = CurrentFontSize();
    WEGetStyle(GetTESelStart(gHiddenTE), &ts, gHiddenTE);
    ts.tsSize = (ts.tsSize == fs - 1) ? fs : fs - 1;
    WESetStyle(weDoSize, &ts, gHiddenTE);
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

    if (GetTESelStart(gHiddenTE) == GetTESelEnd(gHiddenTE))
        return;

    if (ShowLinkURLDialog(url)) {
        WETextStyle ts;

        ts.tsFace = underline;
        ts.tsColor.red = AddLinkURL(url);
        ts.tsColor.green = 0;
        ts.tsColor.blue = 0;
        WESetStyle(weDoFace + weDoColor, &ts, gHiddenTE);
    }
}

void ToggleCode(void)
{
    WETextStyle ts;
    short lh, fa;
    short monacoFont, defaultFont;

    GetFNum("\pMonaco", &monacoFont);
    defaultFont = GetDefaultFontNum();

    WEGetStyle(GetTESelStart(gHiddenTE), &ts, gHiddenTE);
    ts.tsFont = (ts.tsFont == monacoFont) ? defaultFont : monacoFont;
    WESetStyle(weDoFont, &ts, gHiddenTE);
}

void ToggleHeadingHidden(short level)
{
    short selStart;
    long lineStart, lineEnd;
    Handle textH;
    long len;
    WETextStyle ts;
    short lh, fa;
    Boolean isThisLevel;

    selStart = GetTESelStart(gHiddenTE);
    textH = WEGetText(gHiddenTE);
    len = WEGetTextLength(gHiddenTE);

    HLock(textH);
    lineStart = selStart;
    while (lineStart > 0 && (*textH)[lineStart - 1] != '\r')
        lineStart--;
    lineEnd = lineStart;
    while (lineEnd < len && (*textH)[lineEnd] != '\r')
        lineEnd++;
    HUnlock(textH);

    WEGetStyle((short) lineStart, &ts, gHiddenTE);
    isThisLevel = (ts.tsFace & bold) && (ts.tsSize == CurrentFontSize() + (7 - level) * 2);

    WESetSelect((short) lineStart, (short) lineEnd, gHiddenTE);
    if (isThisLevel) {
        ts.tsFace = normal;
        ts.tsSize = CurrentFontSize();
    } else {
        ts.tsFace = bold;
        ts.tsSize = CurrentFontSize() + (7 - level) * 2;
    }
    WESetStyle(weDoFace + weDoSize, &ts, gHiddenTE);
}

/*
    Sets the style at a zero-length selection (the insertion point) --
    Style TextEdit uses this as the style for whatever gets typed next,
    which is exactly what's needed after closing a live-converted span
    so typing doesn't keep inheriting bold/italic/code indefinitely.
*/
static void SetTypingStyleNormal(short pos)
{
    WETextStyle ts;
    short fontNum;

    fontNum = GetDefaultFontNum();
    ts.tsFont = fontNum;
    ts.tsFace = normal;
    ts.tsSize = CurrentFontSize();
    WESetSelect(pos, pos, gHiddenTE);
    WESetStyle(weDoFont + weDoFace + weDoSize, &ts, gHiddenTE);
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
        SetTypingStyleNormal(GetTESelEnd(gHiddenTE));
        return;
    }

    textH = WEGetText(gHiddenTE);
    len = WEGetTextLength(gHiddenTE);
    caret = GetTESelEnd(gHiddenTE);
    
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

        // Optimized header detection - only scan up to 6 # characters
        while (level < 6 && p < caret - 1 && (*textH)[p] == '#') {
            level++;
            p++;
        }
        if (level > 0 && p == caret - 1) {
            WETextStyle ts;

            HUnlock(textH);
            long deletedLen = caret - lineStart;
            long innerEnd = lineEnd - deletedLen;
            WESetSelect((short) lineStart, (short) caret, gHiddenTE);
            WEDelete(gHiddenTE);
            WESetSelect((short) lineStart, (short) innerEnd, gHiddenTE);
            ts.tsFace = bold;
            ts.tsSize = CurrentFontSize() + (7 - level) * 2;
            if (innerEnd > lineStart) {
                WESetStyle(weDoFace + weDoSize, &ts, gHiddenTE);
            }
            WESetSelect((short) lineStart, (short) lineStart, gHiddenTE);
            WESetStyle(weDoFace + weDoSize, &ts, gHiddenTE);
            InvalidateHeightCache();
            if (gHiddenTE != NULL && (*gHiddenTE)->te != NULL) {
                InvalRect(&(**((*gHiddenTE)->te)).viewRect);
            }
            return;
        }

        if (caret >= 2) {
            long p2 = caret - 2;
            long bqCount = 0;
            
            // Optimized blockquote detection - count > characters
            while (p2 >= lineStart && (*textH)[p2] == '>') {
                bqCount++;
                p2--;
            }
            // Skip trailing whitespace
            while (p2 >= lineStart && ((*textH)[p2] == ' ' || (*textH)[p2] == '\t')) {
                p2--;
            }
            
            if (p2 < lineStart && bqCount > 0) {
                WETextStyle ts;
                HUnlock(textH);
                
                WESetSelect((short) lineStart, (short) caret, gHiddenTE);
                WEDelete(gHiddenTE);

                char tabs[16];
                short i;
                for (i = 0; i < bqCount && i < 15; i++) tabs[i] = '\t';
                tabs[i] = 0;
                
                WESetSelect((short) lineStart, (short) lineStart, gHiddenTE);
                WEInsert(tabs, i, NULL, gHiddenTE);
                
                ts.tsColor.red   = 0;
                ts.tsColor.green = 0;
                ts.tsColor.blue  = 10 + bqCount;
                
                WESetSelect((short) lineStart, (short) (lineStart + i), gHiddenTE);
                WESetStyle(weDoColor, &ts, gHiddenTE);
                
                WESetSelect((short) (lineStart + i), (short) (lineStart + i), gHiddenTE);
                WESetStyle(weDoColor, &ts, gHiddenTE);

                SetTypingStyleNormal((short) (lineStart + i));
                
                WESetSelect((short) (lineStart + i), (short) (lineStart + i), gHiddenTE);
                WESetStyle(weDoColor, &ts, gHiddenTE);

                InvalidateHeightCache();
                return;
            }
        }
        
        short scan = lineStart;
        // Skip leading whitespace
        while (scan < caret && ((*textH)[scan] == ' ' || (*textH)[scan] == '\t')) scan++;
        
        // Optimized task list detection - check exact pattern
        if (caret - 1 == scan + 5 && ((*textH)[scan] == '-' || (*textH)[scan] == '+' || (*textH)[scan] == '*') && (*textH)[scan + 1] == ' ' &&
            (*textH)[scan + 2] == '[' && ((*textH)[scan + 3] == ' ' || (*textH)[scan + 3] == 'x' || (*textH)[scan + 3] == 'X') && (*textH)[scan + 4] == ']' && (*textH)[scan + 5] == ' ') {
            
            Boolean isChecked = ((*textH)[scan + 3] == 'x' || (*textH)[scan + 3] == 'X');
            HUnlock(textH);
            
            WESetSelect((short) scan, (short) caret, gHiddenTE);
            WEDelete(gHiddenTE);
            
            char taskStr[5];
            taskStr[0] = '[';
            taskStr[1] = isChecked ? 'x' : ' ';
            taskStr[2] = ']';
            taskStr[3] = ' ';
            taskStr[4] = 0;
            
            WESetSelect((short) scan, (short) scan, gHiddenTE);
            WEInsert(taskStr, 4, NULL, gHiddenTE);
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
            WESetSelect((short) scan, (short) caret, gHiddenTE);
            WEDelete(gHiddenTE);
            
            char bulletStr[3];
            bulletStr[0] = bulletChar;
            bulletStr[1] = ' ';
            bulletStr[2] = 0;
            
            WESetSelect((short) scan, (short) scan, gHiddenTE);
            WEInsert(bulletStr, 2, NULL, gHiddenTE);
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
                    WETextStyle ts;

                    HUnlock(textH);
                    WESetSelect((short) innerEnd, (short) caret, gHiddenTE);
                    WEDelete(gHiddenTE);
                    WESetSelect((short) p, (short) innerStart, gHiddenTE);
                    WEDelete(gHiddenTE);

                    ts.tsFace = bold;
                    WESetSelect((short) p, (short) (innerEnd - 2), gHiddenTE);
                    WESetStyle(weDoFace, &ts, gHiddenTE);
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
                        WETextStyle ts;

                        HUnlock(textH);
                        WESetSelect((short) innerEnd, (short) (innerEnd + 2), gHiddenTE);
                        WEDelete(gHiddenTE);
                        WESetSelect((short) (caret - 2), (short) caret, gHiddenTE);
                        WEDelete(gHiddenTE);

                        ts.tsFace = bold;
                        WESetSelect((short) (caret - 2), (short) (innerEnd - 2), gHiddenTE);
                        WESetStyle(weDoFace, &ts, gHiddenTE);
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
                    WETextStyle ts;

                    HUnlock(textH);
                    WESetSelect((short) innerEnd, (short) caret, gHiddenTE);
                    WEDelete(gHiddenTE);
                    WESetSelect((short) p, (short) innerStart, gHiddenTE);
                    WEDelete(gHiddenTE);

                    ts.tsFace = italic;
                    WESetSelect((short) p, (short) (innerEnd - 1), gHiddenTE);
                    WESetStyle(weDoFace, &ts, gHiddenTE);
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
                        WETextStyle ts;

                        HUnlock(textH);
                        WESetSelect((short) innerEnd, (short) (innerEnd + 1), gHiddenTE);
                        WEDelete(gHiddenTE);
                        WESetSelect((short) (caret - 1), (short) caret, gHiddenTE);
                        WEDelete(gHiddenTE);

                        ts.tsFace = italic;
                        WESetSelect((short) (caret - 1), (short) (innerEnd - 1), gHiddenTE);
                        WESetStyle(weDoFace, &ts, gHiddenTE);
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
                WETextStyle ts;

                HUnlock(textH);
                WESetSelect((short) innerEnd, (short) caret, gHiddenTE);
                WEDelete(gHiddenTE);
                WESetSelect((short) p, (short) innerStart, gHiddenTE);
                WEDelete(gHiddenTE);

                GetFNum("\pMonaco", &ts.tsFont);
                WESetSelect((short) p, (short) (innerEnd - 1), gHiddenTE);
                WESetStyle(weDoFont, &ts, gHiddenTE);
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
                    WETextStyle ts;

                    HUnlock(textH);
                    WESetSelect((short) innerEnd, (short) (innerEnd + 1), gHiddenTE);
                    WEDelete(gHiddenTE);
                    WESetSelect((short) (caret - 1), (short) caret, gHiddenTE);
                    WEDelete(gHiddenTE);

                    GetFNum("\pMonaco", &ts.tsFont);
                    WESetSelect((short) (caret - 1), (short) (innerEnd - 1), gHiddenTE);
                    WESetStyle(weDoFont, &ts, gHiddenTE);
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
                WETextStyle ts;

                if (urlLen < 0) urlLen = 0;
                if (urlLen > 255) urlLen = 255;
                url[0] = (unsigned char) urlLen;
                BlockMove(*textH + urlStart, url + 1, urlLen);

                HUnlock(textH);

                WESetSelect((short) closeBracketPos, (short) caret, gHiddenTE);
                WEDelete(gHiddenTE);
                WESetSelect((short) openBracketPos, (short) (openBracketPos + 1), gHiddenTE);
                WEDelete(gHiddenTE);

                linkID = AddLinkURL(url);

                ts.tsFace = underline;
                ts.tsColor.red = linkID;
                ts.tsColor.green = 0;
                ts.tsColor.blue = 0;
                WESetSelect((short) openBracketPos, (short) (closeBracketPos - 1), gHiddenTE);
                WESetStyle(weDoFace + weDoColor, &ts, gHiddenTE);
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
                        WETextStyle ts;
                        short linkID;
                        
                        url[0] = (unsigned char) urlLen;
                        BlockMove(*textH + p + 1, url + 1, urlLen);
                        linkID = AddLinkURL(url);

                        HUnlock(textH);
                        WESetSelect((short) (caret - 1), (short) caret, gHiddenTE);
                        WEDelete(gHiddenTE);
                        WESetSelect((short) p, (short) (p + 1), gHiddenTE);
                        WEDelete(gHiddenTE);

                        ts.tsFace = underline;
                        ts.tsColor.red = linkID;
                        ts.tsColor.green = 0;
                        ts.tsColor.blue = 0;

                        WESetSelect((short) p, (short) (caret - 2), gHiddenTE);
                        WESetStyle(weDoFace + weDoColor, &ts, gHiddenTE);
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
                    WETextStyle ts;

                    HUnlock(textH);
                    WESetSelect((short) innerEnd, (short) caret, gHiddenTE);
                    WEDelete(gHiddenTE);
                    WESetSelect((short) p, (short) innerStart, gHiddenTE);
                    WEDelete(gHiddenTE);

                    ts.tsFace = condense;
                    WESetSelect((short) p, (short) (innerEnd - 2), gHiddenTE);
                    WESetStyle(weDoFace, &ts, gHiddenTE);
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
                    WETextStyle ts;

                    HUnlock(textH);
                    WESetSelect((short) innerEnd, (short) caret, gHiddenTE);
                    WEDelete(gHiddenTE);
                    WESetSelect((short) p, (short) innerStart, gHiddenTE);
                    WEDelete(gHiddenTE);

                    ts.tsFace = outline;
                    WESetSelect((short) p, (short) (innerEnd - 2), gHiddenTE);
                    WESetStyle(weDoFace, &ts, gHiddenTE);
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
    WETextStyle ts;
    short fontNum;
    long selStart, selEnd;

    WEGetSelection(&selStart, &selEnd, gHiddenTE);
    if (selStart == selEnd)
        return;

    fontNum = GetDefaultFontNum();
    ts.tsFont = fontNum;
    ts.tsFace = normal;
    ts.tsSize = CurrentFontSize();
    ts.tsColor.red = ts.tsColor.green = ts.tsColor.blue = 0;
    WESetStyle(weDoFont + weDoFace + weDoSize + weDoColor, &ts, gHiddenTE);
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

    long selStartLong, selEndLong;
    WEGetSelection(&selStartLong, &selEndLong, gTE);
    selStart = (short)selStartLong;
    selEnd = (short)selEndLong;
    if (selStart == selEnd)
        return;

    textH = WEGetText(gTE);
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

    WESetSelect(selStart, selEnd, gTE);
    WEDelete(gTE);
    WEInsert(*outH, outLen, NULL, gTE);
    DisposeHandle(outH);

    WESetSelect(selStart, (short) (selStart + outLen), gTE);
}



void LoadTextWindow(long startOffset)
{
    long len, copyLen;
    Handle srcH;
    WEHandle te = gActiveTE;
    Rect savedViewRect;
    Rect hiddenRect;
    WETextStyle baseStyle;
    short fontNum;
    
    if (gWindow != NULL) {
        SetPort(gWindow);
    }
    
    if (!gHideMarkdown && (gMarkdownText == NULL || gMarkdownLen == 0)) {
        if (gWriterText == NULL && gHiddenTE != NULL) {
            SyncWindowToBacking();
        }
        SyncHiddenToCanonical();
    }
    
    if (gHideMarkdown) {
        srcH = gWriterText;
        len = gWriterLen;
    } else {
        srcH = gMarkdownText;
        len = gMarkdownLen;
    }
    
    if (srcH == NULL) return;
    
    if (len <= WINDOW_SIZE) {
        startOffset = 0;
    } else {
        long maxStart = len - (WINDOW_SIZE / 2);
        if (maxStart < 0) maxStart = 0;
        if (startOffset < 0) startOffset = 0;
        if (startOffset > maxStart) startOffset = maxStart;
    }
    
    if (startOffset > 0 && startOffset < len) {
        long scan = startOffset;
        HLock(srcH);
        while (scan > 0 && (*srcH)[scan - 1] != '\r' && (*srcH)[scan - 1] != '\n') {
            scan--;
        }
        HUnlock(srcH);
        startOffset = scan;
    }
    
    copyLen = len - startOffset;
    if (copyLen > WINDOW_SIZE) copyLen = WINDOW_SIZE;
    
    /* Snap the end to a line boundary so we never cut in the middle of a line */
    if (startOffset + copyLen < len) {
        long scan = startOffset + copyLen - 1;
        HLock(srcH);
        while (scan > startOffset && (*srcH)[scan] != '\r' && (*srcH)[scan] != '\n') {
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
            char c = (*srcH)[i];
            if (c == '\r') {
                lineCount++;
            } else if (c == '\n') {
                if (i == 0 || (*srcH)[i - 1] != '\r') {
                    lineCount++;
                }
            }
        }
        HUnlock(srcH);
        gWindowStartLine = lineCount;
    }
    /* Move the viewRect completely off-screen so that NO drawing happens
       during the delete/insert/style phase. Every TESetStyle, TEDelete,
       TEInsert, TECalText call clips to viewRect – with it off-screen the
       window stays perfectly still until we restore it at the very end. */
    savedViewRect = gWindow->portRect;
    savedViewRect.left += MARGIN_H;
    savedViewRect.right -= MARGIN_H;
    savedViewRect.top += MARGIN_TOP;
    savedViewRect.bottom -= MARGIN_BOTTOM;
    SetRect(&hiddenRect, OFFSCREEN_COORD, OFFSCREEN_COORD,
            OFFSCREEN_COORD + (savedViewRect.right - savedViewRect.left),
            OFFSCREEN_COORD + (savedViewRect.bottom - savedViewRect.top));
    WESetRects(&hiddenRect, &hiddenRect, te);
    
    /* 1. Delete all existing text */
    WESetSelect(0, WEGetTextLength(te), te);
    WEDelete(te);
    
    /* 2. Insert the new chunk – text arrives unstyled (inherits last run) */
    HLock(srcH);
    WEInsert(*srcH + startOffset, copyLen, NULL, te);
    HUnlock(srcH);
    
    /* 3. Apply a uniform base style across everything so we start clean */
    if (!gHideMarkdown) {
        short monoFont = 0;
        GetFNum("\pMonaco", &monoFont);
        if (monoFont == 0) GetFNum("\pCourier", &monoFont);
        fontNum = (monoFont != 0) ? monoFont : GetDefaultFontNum();
        baseStyle.tsSize = 12;
    } else {
        fontNum = GetDefaultFontNum();
        baseStyle.tsSize = CurrentFontSize();
    }
    baseStyle.tsFont  = fontNum;
    baseStyle.tsFace  = normal;
    baseStyle.tsColor.red = baseStyle.tsColor.green = baseStyle.tsColor.blue = 0;
    WESetSelect(0, WEGetTextLength(te), te);
    WESetStyle(weDoFont + weDoFace + weDoSize + weDoColor, &baseStyle, te);
    
    /* 4. Apply any Writer-mode syntax highlight ops */
    if (gHideMarkdown && gWriterOpsH != NULL) {
        short k;
        StyleOp *ops;
        
        HLock(gWriterOpsH);
        ops = (StyleOp *) *gWriterOpsH;
        for (k = 0; k < gWriterOpCount; k++) {
            long opStart, opEnd;
            WETextStyle opStyle;
            
            if (ops[k].end <= startOffset || ops[k].start >= gWindowEnd)
                continue;
                
            opStart = ops[k].start - startOffset;
            opEnd   = ops[k].end   - startOffset;
            if (opStart < 0)        opStart = 0;
            if (opEnd > copyLen)    opEnd   = copyLen;
            
            WESetSelect(opStart, opEnd, te);
            WEGetStyle(opStart, &opStyle, te);
            switch (ops[k].kind) {
                case 'X':
                    opStyle.tsFace |= (bold | italic);
                    WESetStyle(weDoFace, &opStyle, te);
                    break;
                case 'B':
                    opStyle.tsFace |= bold;
                    WESetStyle(weDoFace, &opStyle, te);
                    break;
                case 'I':
                    opStyle.tsFace |= italic;
                    WESetStyle(weDoFace, &opStyle, te);
                    break;
                case 'C':
                    GetFNum("\pMonaco", &opStyle.tsFont);
                    opStyle.tsFace = normal;
                    opStyle.tsSize = CurrentFontSize() - 1;
                    opStyle.tsColor.red = 0;
                    opStyle.tsColor.green = 0;
                    if (ops[k].level > 0) {
                        opStyle.tsColor.blue = 3;
                    } else {
                        opStyle.tsColor.blue = 0;
                    }
                    WESetStyle(weDoFont + weDoFace + weDoSize + weDoColor, &opStyle, te);
                    break;
                case 'L':
                    opStyle.tsFace |= underline;
                    opStyle.tsColor.red = ops[k].linkID;
                    opStyle.tsColor.green = 0;
                    opStyle.tsColor.blue = 0;
                    WESetStyle(weDoFace + weDoColor, &opStyle, te);
                    break;
                case 'H':
                    opStyle.tsFace |= bold;
                    opStyle.tsSize = CurrentFontSize() + (7 - ops[k].level) * 2;
                    WESetStyle(weDoFace + weDoSize, &opStyle, te);
                    break;
                case 'R':
                    opStyle.tsFace |= bold;
                    opStyle.tsColor.red   = 0;
                    opStyle.tsColor.green = 0;
                    opStyle.tsColor.blue  = 1;
                    WESetStyle(weDoFace + weDoColor, &opStyle, te);
                    break;
                case 'q':
                    opStyle.tsFace = bold;
                    opStyle.tsSize = CurrentFontSize() + 3;
                    opStyle.tsColor.red   = 253;
                    opStyle.tsColor.green = ops[k].level;
                    opStyle.tsColor.blue  = 0;
                    WESetStyle(weDoFont + weDoFace + weDoSize + weDoColor, &opStyle, te);
                    break;
                case 'Q':
                    opStyle.tsFace = italic;
                    opStyle.tsColor.red   = 0;
                    opStyle.tsColor.green = 0;
                    opStyle.tsColor.blue  = 10 + ops[k].level;
                    WESetStyle(weDoFace + weDoColor, &opStyle, te);
                    break;
                case 'T':
                    GetFNum("\pMonaco", &opStyle.tsFont);
                    opStyle.tsFace = normal;
                    opStyle.tsColor.red = 0;
                    opStyle.tsColor.green = 0;
                    opStyle.tsColor.blue = 2;
                    WESetStyle(weDoFont + weDoFace + weDoColor, &opStyle, te);
                    break;
                case 'S':
                    opStyle.tsColor.red   = 0;
                    opStyle.tsColor.green = 1;
                    opStyle.tsColor.blue  = 0;
                    WESetStyle(weDoColor, &opStyle, te);
                    break;
                case 'P':
                    opStyle.tsSize = (short)(CurrentFontSize() * 0.7);
                    WESetStyle(weDoSize, &opStyle, te);
                    break;
                case 'D':
                    opStyle.tsSize = (short)(CurrentFontSize() * 0.7) - 1;
                    WESetStyle(weDoSize, &opStyle, te);
                    break;
                case 'K':
                    GetFNum("\pMonaco", &opStyle.tsFont);
                    if (opStyle.tsFont == 0) GetFNum("\pCourier", &opStyle.tsFont);
                    opStyle.tsFace = normal;
                    opStyle.tsSize = CurrentFontSize();
                    opStyle.tsColor.red = 255;
                    opStyle.tsColor.green = ops[k].level;
                    opStyle.tsColor.blue = 0;
                    WESetStyle(weDoFont + weDoFace + weDoSize + weDoColor, &opStyle, te);
                    break;
                case 'U':
                    GetFNum("\pMonaco", &opStyle.tsFont);
                    if (opStyle.tsFont == 0) GetFNum("\pCourier", &opStyle.tsFont);
                    opStyle.tsFace = normal;
                    opStyle.tsSize = CurrentFontSize();
                    opStyle.tsColor.red = 254;
                    opStyle.tsColor.green = ops[k].level;
                    opStyle.tsColor.blue = 0;
                    WESetStyle(weDoFont + weDoFace + weDoSize + weDoColor, &opStyle, te);
                    break;
                case 'E':
                    opStyle.tsFace = outline;
                    WESetStyle(weDoFace, &opStyle, te);
                    break;
            }
        }
        HUnlock(gWriterOpsH);
    }
    
    /* 5. Place caret at start */
    WESetSelect(0, 0, te);
    
    /* 6. Restore the real viewRect and destRect.
       Set the gutter margin HERE (not inside WESetRects) so offscreen rects
       are never corrupted by the +48 adjustment. */
    WESetRects(&savedViewRect, &savedViewRect, te);
    if (!gHideMarkdown) {
        /* Markdown view: inset destRect left by 48px for the line-numbers gutter */
        (*(*te)->te)->destRect.left = savedViewRect.left + 48;
    }
    WECalText(te);
    
    /* 7. Erase the window area and let WEUpdate paint the freshly laid-out text */
    EraseRect(&savedViewRect);
    WEUpdate(&savedViewRect, te);
}

void SyncWindowToBacking(void)
{
    long newLen = WEGetTextLength(gActiveTE);
    long oldLen = gWindowEnd - gWindowStart;
    long diff = newLen - oldLen;
    Handle *targetHPtr;
    long *targetLenPtr;
    
    if (gHideMarkdown) {
        targetHPtr = &gWriterText;
        targetLenPtr = &gWriterLen;
    } else {
        targetHPtr = &gMarkdownText;
        targetLenPtr = &gMarkdownLen;
    }
    
    if (*targetHPtr == NULL) {
        *targetHPtr = NewHandle(0);
        *targetLenPtr = 0;
        oldLen = 0;
        diff = newLen;
        gWindowStart = 0;
        gWindowEnd = 0;
    }
    
    Handle targetH = *targetHPtr;
    
    if (diff != 0) {
        SetHandleSize(targetH, *targetLenPtr + diff);
        HLock(targetH);
        if (gWindowEnd < *targetLenPtr) {
            BlockMove(*targetH + gWindowEnd, *targetH + gWindowEnd + diff, *targetLenPtr - gWindowEnd);
        }
        HUnlock(targetH);
        *targetLenPtr += diff;
    }
    
    if (newLen > 0) {
        HLock(targetH);
        BlockMove(*(WEGetText(gActiveTE)), *targetH + gWindowStart, newLen);
        HUnlock(targetH);
    }
    
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
        TEStyleHandle teStyles = TEGetStyleHandle((*gActiveTE)->te);
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
                short runEnd = (r + 1 < nRuns) ? (**teStyles).runs[r+1].startChar : WEGetTextLength(gActiveTE);
                
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
                
                Boolean isBlockquote = (style.stColor.blue >= 10);
                short bqDepth = isBlockquote ? (style.stColor.blue - 10) : 0;
                
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
                    {
                        short fs = CurrentFontSize();
                        short superSize = (short)(fs * 0.7);
                        short subSize = superSize - 1;

                        Boolean isSuper = (style.stSize == superSize);
                        Boolean isSub = (style.stSize == subSize);
                        Boolean isStrike = (style.stColor.green == 1);

                        if (isSuper) {
                            if (gWriterOpCount < MAX_STYLE_OPS) {
                                ops[gWriterOpCount].start = globalStart;
                                ops[gWriterOpCount].end = globalEnd;
                                ops[gWriterOpCount].kind = 'P';
                                ops[gWriterOpCount].level = 0;
                                ops[gWriterOpCount].linkID = 0;
                                gWriterOpCount++;
                            }
                        }
                        if (isSub) {
                            if (gWriterOpCount < MAX_STYLE_OPS) {
                                ops[gWriterOpCount].start = globalStart;
                                ops[gWriterOpCount].end = globalEnd;
                                ops[gWriterOpCount].kind = 'D';
                                ops[gWriterOpCount].level = 0;
                                ops[gWriterOpCount].linkID = 0;
                                gWriterOpCount++;
                            }
                        }
                        if (isStrike) {
                            if (gWriterOpCount < MAX_STYLE_OPS) {
                                ops[gWriterOpCount].start = globalStart;
                                ops[gWriterOpCount].end = globalEnd;
                                ops[gWriterOpCount].kind = 'S';
                                ops[gWriterOpCount].level = 0;
                                ops[gWriterOpCount].linkID = 0;
                                gWriterOpCount++;
                            }
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


void InsertDateHeading(short level)
{
    DateTimeRec date;
    unsigned long secs;
    char buf[64];
    int i;
    
    GetDateTime(&secs);
    SecondsToDate(secs, &date);
    
    if (gHideMarkdown) {
        WETextStyle ts;
        ts.tsFace = bold;
        ts.tsSize = CurrentFontSize() + (7 - level) * 2;
        WESetStyle(weDoFace + weDoSize, &ts, gActiveTE);
        
        sprintf(buf, "%04d-%02d-%02d\r", date.year, date.month, date.day);
        WEInsert(buf, strlen(buf), NULL, gActiveTE);
        
        ts.tsFace = normal;
        ts.tsSize = CurrentFontSize();
        WESetStyle(weDoFace + weDoSize, &ts, gActiveTE);
        
        InvalidateHeightCache();
        if (gActiveTE != NULL && (*gActiveTE)->te != NULL) {
            InvalRect(&(**((*gActiveTE)->te)).viewRect);
        }
    } else {
        for (i = 0; i < level; i++) buf[i] = '#';
        buf[level] = ' ';
        sprintf(buf + level + 1, "%04d-%02d-%02d\r", date.year, date.month, date.day);
        
        WEInsert(buf, strlen(buf), NULL, gActiveTE);
    }
}

void InsertTimeHeading(short level)
{
    DateTimeRec date;
    unsigned long secs;
    char buf[64];
    int i;
    
    GetDateTime(&secs);
    SecondsToDate(secs, &date);
    
    if (gHideMarkdown) {
        WETextStyle ts;
        ts.tsFace = bold;
        ts.tsSize = CurrentFontSize() + (7 - level) * 2;
        WESetStyle(weDoFace + weDoSize, &ts, gActiveTE);
        
        sprintf(buf, "%02d:%02d\r", date.hour, date.minute);
        WEInsert(buf, strlen(buf), NULL, gActiveTE);
        
        ts.tsFace = normal;
        ts.tsSize = CurrentFontSize();
        WESetStyle(weDoFace + weDoSize, &ts, gActiveTE);
        
        InvalidateHeightCache();
        if (gActiveTE != NULL && (*gActiveTE)->te != NULL) {
            InvalRect(&(**((*gActiveTE)->te)).viewRect);
        }
    } else {
        for (i = 0; i < level; i++) buf[i] = '#';
        buf[level] = ' ';
        sprintf(buf + level + 1, "%02d:%02d\r", date.hour, date.minute);
        
        WEInsert(buf, strlen(buf), NULL, gActiveTE);
    }
}

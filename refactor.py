import os

with open("app/markdown.c", "r") as f:
    content = f.read()

build_hidden_start = content.find("void BuildHiddenView(void)")
build_hidden_end = content.find("\nvoid SyncHiddenToCanonical(void)")

new_build_hidden = r"""void BuildHiddenView(void)
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
        if (i == 0 || (*srcH)[i - 1] == '\r') {
            short level = 0;
            while (level < 3 && i + level < len && (*srcH)[i + level] == '#')
                level++;
            if (level > 0 && i + level < len && (*srcH)[i + level] == ' ') {
                long lineStart = i + level + 1;
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

            if (i + 2 < len && (*srcH)[i] == '-' && (*srcH)[i+1] == '-' && (*srcH)[i+2] == '-') {
                long end = i + 3;
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

            if (i + 1 < len && (*srcH)[i] == '-' && (*srcH)[i + 1] == ' ') {
                long lineStart = i + 2;
                long lineEnd = lineStart;
                long outStart = outLen;

                (*outH)[outLen++] = '\245';
                (*outH)[outLen++] = ' ';

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

        if (i + 1 < len && (*srcH)[i] == '*' && (*srcH)[i + 1] == '*') {
            long j = i + 2;
            while (j + 1 < len && ((*srcH)[j] != '*' || (*srcH)[j + 1] != '*'))
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
        if ((*srcH)[i] == '*') {
            long j = i + 1;
            while (j < len && (*srcH)[j] != '*')
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
        if ((*srcH)[i] == '[') {
            long closeBracket = i + 1;
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
                    for (m = i + 1; m < closeBracket; m++)
                        (*outH)[outLen++] = (*srcH)[m];
                    if (urlLen > 255) urlLen = 255;
                    url[0] = (unsigned char) urlLen;
                    BlockMove(*srcH + closeBracket + 2, url + 1, urlLen);
                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = outStart;
                        ops[gWriterOpCount].end = outLen;
                        ops[gWriterOpCount].kind = 'L';
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
"""

content = content[:build_hidden_start] + new_build_hidden + content[build_hidden_end:]

extras = r"""
#define WINDOW_SIZE 4000

void LoadTextWindow(long startOffset)
{
    long len, copyLen;
    Handle srcH;
    TEHandle te = gActiveTE;
    
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
    
    copyLen = len - startOffset;
    if (copyLen > WINDOW_SIZE) copyLen = WINDOW_SIZE;
    
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
    
    TESetSelect(0, 32767, te);
    TEDelete(te);
    
    HLock(srcH);
    TEInsert(*srcH + startOffset, copyLen, te);
    HUnlock(srcH);
    
    if (gHideMarkdown && gWriterOpsH != NULL) {
        short k;
        StyleOp *ops;
        
        ClearStyles();
        
        HLock(gWriterOpsH);
        ops = (StyleOp *) *gWriterOpsH;
        for (k = 0; k < gWriterOpCount; k++) {
            if (ops[k].end <= startOffset || ops[k].start >= gWindowEnd)
                continue;
                
            long opStart = ops[k].start - startOffset;
            long opEnd = ops[k].end - startOffset;
            if (opStart < 0) opStart = 0;
            if (opEnd > copyLen) opEnd = copyLen;
            
            TextStyle opStyle;
            TESetSelect((short)opStart, (short)opEnd, te);
            switch (ops[k].kind) {
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
                    opStyle.tsFace = underline;
                    opStyle.tsColor.red = ops[k].linkID;
                    opStyle.tsColor.green = 0;
                    opStyle.tsColor.blue = 0;
                    TESetStyle(doFace + doColor, &opStyle, false, te);
                    break;
                case 'H':
                    opStyle.tsFace = bold;
                    opStyle.tsSize = CurrentFontSize() + (4 - ops[k].level) * 4;
                    TESetStyle(doFace + doSize, &opStyle, false, te);
                    break;
                case 'R':
                    opStyle.tsFace = bold;
                    opStyle.tsColor.blue = 1; 
                    TESetStyle(doFace + doColor, &opStyle, false, te);
                    break;
            }
        }
        HUnlock(gWriterOpsH);
        TESetSelect(0, 0, te);
        TECalText(te);
    }
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
        
        if (gHideMarkdown && gWriterOpsH != NULL) {
            short k;
            StyleOp *ops;
            HLock(gWriterOpsH);
            ops = (StyleOp *) *gWriterOpsH;
            for (k = 0; k < gWriterOpCount; k++) {
                if (ops[k].start >= gWindowEnd) {
                    ops[k].start += diff;
                    ops[k].end += diff;
                } else if (ops[k].end >= gWindowEnd) {
                    ops[k].end += diff;
                }
            }
            HUnlock(gWriterOpsH);
        }
    }
    
    HLock(targetH);
    BlockMove(*(**gActiveTE).hText, *targetH + gWindowStart, newLen);
    HUnlock(targetH);
    
    gWindowEnd += diff;
}
"""

content += extras

sync_start = content.find("void SyncHiddenToCanonical(void)")
sync_end = content.find("\nHandle EncodeSelectionAsMarkdown(short start, short end, TEHandle te)", sync_start)

new_sync = r"""void SyncHiddenToCanonical(void)
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

    lineStart = 0;
    while (lineStart < len) {
        Boolean isHR = false;
        TextStyle firstStyle;
        long textOffset = lineStart;
        
        lineEnd = lineStart;
        while (lineEnd < len && (*srcH)[lineEnd] != '\r')
            lineEnd++;

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
                            firstStyle.tsSize = CurrentFontSize() + (4 - ops[k].level) * 4;
                        } else if (ops[k].kind == 'R') {
                            firstStyle.tsFace |= bold;
                            firstStyle.tsColor.blue = 1;
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
            
            if (!isHR && (firstStyle.tsFace & bold)) {
                short lvl;
                for (lvl = 1; lvl <= 3; lvl++) {
                    if (firstStyle.tsSize == CurrentFontSize() + (4 - lvl) * 4) {
                        short s;
                        for (s = 0; s < lvl; s++)
                            (*outH)[outLen++] = '#';
                        (*outH)[outLen++] = ' ';
                        break;
                    }
                }
            }
        }

        if (isHR) {
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '-';
        } else if (lineEnd > lineStart && (*srcH)[lineStart] == '\245' && (*srcH)[lineStart + 1] == ' ') {
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = ' ';
            textOffset = lineStart + 2;
        }

        if (!isHR) {
            long runStart = textOffset;
            while (runStart < lineEnd) {
                long runEnd = runStart + 1;
                short linkID = 0;
                Boolean isBold = false, isItalic = false, isCode = false;
                
                if (gWriterOpsH) {
                    short k;
                    StyleOp *ops;
                    HLock(gWriterOpsH);
                    ops = (StyleOp *) *gWriterOpsH;
                    
                    for (k = 0; k < gWriterOpCount; k++) {
                        if (ops[k].start <= runStart && ops[k].end > runStart) {
                            if (ops[k].kind == 'B') isBold = true;
                            if (ops[k].kind == 'I') isItalic = true;
                            if (ops[k].kind == 'C') isCode = true;
                            if (ops[k].kind == 'L') linkID = ops[k].linkID;
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

                if (isBold) { (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; }
                if (isItalic) (*outH)[outLen++] = '*';
                if (isCode) (*outH)[outLen++] = '`';
                if (linkID > 0) (*outH)[outLen++] = '[';

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
                if (isCode) (*outH)[outLen++] = '`';
                if (isItalic) (*outH)[outLen++] = '*';
                if (isBold) { (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; }
            }
        }

        if (!isHR) {
            if (lineEnd < len)
                (*outH)[outLen++] = '\r';
        }
        
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
"""

content = content[:sync_start] + new_sync + content[sync_end:]

content = content.replace("    short start, end, kind, level;", "    long start, end;\n    short kind, level;")
content = content.replace("#define MAX_STYLE_OPS 1000", "#define MAX_STYLE_OPS 16384")

with open("app/markdown.c", "w") as f:
    f.write(content)

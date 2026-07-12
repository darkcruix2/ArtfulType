import re

with open('app/markdown.c', 'r') as f:
    text = f.read()

# 1. Backslash Escaping and Bold+Italic
search_loop = """
    i = 0;
    while (i < len) {
        if (i == 0 || (*srcH)[i - 1] == '\\r') {
"""
replace_loop = """
    i = 0;
    while (i < len) {
        if (i + 1 < len && (*srcH)[i] == '\\\\' && ((*srcH)[i+1] == '*' || (*srcH)[i+1] == '_' || (*srcH)[i+1] == '#' || (*srcH)[i+1] == '>' || (*srcH)[i+1] == '[' || (*srcH)[i+1] == '`' || (*srcH)[i+1] == '\\\\')) {
            (*outH)[outLen++] = (*srcH)[i+1];
            i += 2;
            continue;
        }

        if (i == 0 || (*srcH)[i - 1] == '\\r') {
            long p = i;
            while (p < len && ((*srcH)[p] == ' ' || (*srcH)[p] == '\\t')) p++;
"""
text = text.replace(search_loop, replace_loop)


# 2. Update Headings parsing to use `p`
search_headings = """
            short level = 0;
            while (level < 6 && i + level < len && (*srcH)[i + level] == '#')
                level++;
            if (level > 0 && i + level < len && (*srcH)[i + level] == ' ') {
                long lineStart = i + level + 1;
"""
replace_headings = """
            short level = 0;
            while (level < 6 && p + level < len && (*srcH)[p + level] == '#')
                level++;
            if (level > 0 && p + level < len && (*srcH)[p + level] == ' ') {
                long lineStart = p + level + 1;
"""
text = text.replace(search_headings, replace_headings)

search_headings_end = """
                if (lineEnd < len && (*srcH)[lineEnd] == '\\r') {
                    (*outH)[outLen++] = '\\r';
                    i = lineEnd + 1;
                    if (i < len && (*srcH)[i] == '\\r') {
                        i++;
                    }
                } else {
                    i = lineEnd;
                }
                continue;
            }

            if (i + 1 < len && (*srcH)[i] == '>' && (*srcH)[i + 1] == ' ') {
"""
replace_headings_end = """
                if (lineEnd < len && (*srcH)[lineEnd] == '\\r') {
                    (*outH)[outLen++] = '\\r';
                    i = lineEnd + 1;
                    if (i < len && (*srcH)[i] == '\\r') {
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
                    if ((*srcH)[q] == ' ' || (*srcH)[q] == '\\t') {
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

                    while (lineEnd < len && (*srcH)[lineEnd] != '\\r') {
                        (*outH)[outLen++] = (*srcH)[lineEnd];
                        lineEnd++;
                    }
                    if (gWriterOpCount < MAX_STYLE_OPS) {
                        ops[gWriterOpCount].start = outStart;
                        ops[gWriterOpCount].end = outLen;
                        ops[gWriterOpCount].kind = 'Q';
                        gWriterOpCount++;
                    }
                    if (lineEnd < len && (*srcH)[lineEnd] == '\\r') {
                        (*outH)[outLen++] = '\\r';
                        i = lineEnd + 1;
                        if (i < len && (*srcH)[i] == '\\r') {
                            i++;
                        }
                    } else {
                        i = lineEnd;
                    }
                    continue;
                }
            }

            if (p + 2 < len && (((*srcH)[p] == '-' && (*srcH)[p+1] == '-' && (*srcH)[p+2] == '-') ||
"""
# Replace blockquotes and prepare for HRs
search_blockquotes = """
            if (i + 1 < len && (*srcH)[i] == '>' && (*srcH)[i + 1] == ' ') {
                long lineStart = i + 2;
                long lineEnd = lineStart;
                long outStart = outLen;

                while (lineEnd < len && (*srcH)[lineEnd] != '\\r') {
                    (*outH)[outLen++] = (*srcH)[lineEnd];
                    lineEnd++;
                }
                if (gWriterOpCount < MAX_STYLE_OPS) {
                    ops[gWriterOpCount].start = outStart;
                    ops[gWriterOpCount].end = outLen;
                    ops[gWriterOpCount].kind = 'Q';
                    gWriterOpCount++;
                }
                if (lineEnd < len && (*srcH)[lineEnd] == '\\r') {
                    (*outH)[outLen++] = '\\r';
                    i = lineEnd + 1;
                    if (i < len && (*srcH)[i] == '\\r') {
                        i++;
                    }
                } else {
                    i = lineEnd;
                }
                continue;
            }

            if (i + 2 < len && (((*srcH)[i] == '-' && (*srcH)[i+1] == '-' && (*srcH)[i+2] == '-') ||
"""
text = text.replace(search_blockquotes, replace_headings_end)


# HRs
search_hr = """
                                ((*srcH)[i] == '*' && (*srcH)[i+1] == '*' && (*srcH)[i+2] == '*') ||
                                ((*srcH)[i] == '_' && (*srcH)[i+1] == '_' && (*srcH)[i+2] == '_'))) {
                long end = i + 3;
                while (end < len && (*srcH)[end] == ' ') end++;
                if (end == len || (*srcH)[end] == '\\r') {
"""
replace_hr = """
                                ((*srcH)[p] == '*' && (*srcH)[p+1] == '*' && (*srcH)[p+2] == '*') ||
                                ((*srcH)[p] == '_' && (*srcH)[p+1] == '_' && (*srcH)[p+2] == '_'))) {
                long end = p + 3;
                while (end < len && (*srcH)[end] == ' ') end++;
                if (end == len || (*srcH)[end] == '\\r') {
"""
text = text.replace(search_hr, replace_hr)


# Unordered Lists
search_lists = """
            if (i + 1 < len && ((*srcH)[i] == '-' || (*srcH)[i] == '+' || (*srcH)[i] == '*') && (*srcH)[i + 1] == ' ') {
                long lineStart = i + 2;
                long lineEnd = lineStart;
                long outStart = outLen;

                (*outH)[outLen++] = '\\245';
                (*outH)[outLen++] = ' ';
"""
replace_lists = """
            if (p + 1 < len && ((*srcH)[p] == '-' || (*srcH)[p] == '+' || (*srcH)[p] == '*') && (*srcH)[p + 1] == ' ') {
                long lineStart = p + 2;
                long lineEnd = lineStart;
                long outStart = outLen;

                long s;
                for (s = i; s < p; s++) {
                    (*outH)[outLen++] = (*srcH)[s];
                }

                (*outH)[outLen++] = '\\245';
                (*outH)[outLen++] = ' ';
"""
text = text.replace(search_lists, replace_lists)


# 3. Bold+Italic (X)
search_bold = """
        if (i + 1 < len && ((*srcH)[i] == '*' || (*srcH)[i] == '_') && (*srcH)[i] == (*srcH)[i + 1]) {
            char delim = (*srcH)[i];
"""
replace_bold = """
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
"""
text = text.replace(search_bold, replace_bold)

# 4. Angle Bracket URLs
search_angle = """
        if ((*srcH)[i] == '[' || ((*srcH)[i] == '!' && i + 1 < len && (*srcH)[i+1] == '[')) {
"""
replace_angle = """
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
"""
text = text.replace(search_angle, replace_angle)

with open('app/markdown.c', 'w') as f:
    f.write(text)

import re

with open('app/markdown.c', 'r') as f:
    text = f.read()

# 2. Blockquotes, Horizontal Rules, Unordered Lists in BuildHiddenView
search_block = """
            if (i + 2 < len && (*srcH)[i] == '-' && (*srcH)[i+1] == '-' && (*srcH)[i+2] == '-') {
"""

replace_block = """
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
                                ((*srcH)[i] == '*' && (*srcH)[i+1] == '*' && (*srcH)[i+2] == '*') ||
                                ((*srcH)[i] == '_' && (*srcH)[i+1] == '_' && (*srcH)[i+2] == '_'))) {
"""

text = text.replace(search_block, replace_block)

search_ul = """
            if (i + 1 < len && (*srcH)[i] == '-' && (*srcH)[i + 1] == ' ') {
"""
replace_ul = """
            if (i + 1 < len && ((*srcH)[i] == '-' || (*srcH)[i] == '+' || (*srcH)[i] == '*') && (*srcH)[i + 1] == ' ') {
"""
text = text.replace(search_ul, replace_ul)


# 3. Bold, Italic, Images
search_bold = """
        if (i + 1 < len && (*srcH)[i] == '*' && (*srcH)[i + 1] == '*') {
            long j = i + 2;
            while (j + 1 < len && ((*srcH)[j] != '*' || (*srcH)[j + 1] != '*'))
                j++;
            if (j + 1 < len) {
"""
replace_bold = """
        if (i + 1 < len && ((*srcH)[i] == '*' || (*srcH)[i] == '_') && (*srcH)[i] == (*srcH)[i + 1]) {
            char delim = (*srcH)[i];
            long j = i + 2;
            while (j + 1 < len && ((*srcH)[j] != delim || (*srcH)[j + 1] != delim))
                j++;
            if (j + 1 < len) {
"""
text = text.replace(search_bold, replace_bold)

search_italic = """
        if ((*srcH)[i] == '*') {
            long j = i + 1;
            while (j < len && (*srcH)[j] != '*')
                j++;
            if (j < len) {
"""
replace_italic = """
        if ((*srcH)[i] == '*' || (*srcH)[i] == '_') {
            char delim = (*srcH)[i];
            long j = i + 1;
            while (j < len && (*srcH)[j] != delim)
                j++;
            if (j < len) {
"""
text = text.replace(search_italic, replace_italic)

search_link = """
        if ((*srcH)[i] == '[') {
            long closeBracket = i + 1;
"""
replace_link = """
        if ((*srcH)[i] == '[' || ((*srcH)[i] == '!' && i + 1 < len && (*srcH)[i+1] == '[')) {
            Boolean isImage = ((*srcH)[i] == '!');
            long openBracket = isImage ? i + 1 : i;
            long closeBracket = openBracket + 1;
"""
text = text.replace(search_link, replace_link)

search_link_inner = """
                    long urlLen = closeParen - (closeBracket + 2);
                    for (m = i + 1; m < closeBracket; m++)
                        (*outH)[outLen++] = (*srcH)[m];
"""
replace_link_inner = """
                    long urlLen = closeParen - (closeBracket + 2);
                    if (isImage) {
                        (*outH)[outLen++] = '!';
                    }
                    for (m = openBracket + 1; m < closeBracket; m++)
                        (*outH)[outLen++] = (*srcH)[m];
"""
text = text.replace(search_link_inner, replace_link_inner)

search_link_kind = """
                        ops[gWriterOpCount].kind = 'L';
"""
replace_link_kind = """
                        ops[gWriterOpCount].kind = isImage ? 'M' : 'L';
"""
text = text.replace(search_link_kind, replace_link_kind)

with open('app/markdown.c', 'w') as f:
    f.write(text)

print("Done")

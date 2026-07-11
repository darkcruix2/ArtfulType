import re

with open('app/markdown.c', 'r') as f:
    text = f.read()

# 1. InsertMarkdownAsStyled: Bold
search_bold_ins = """
        if (i + 1 < srcLen && (*srcH)[i] == '*' && (*srcH)[i + 1] == '*') {
            long j = i + 2;

            while (j + 1 < srcLen && !((*srcH)[j] == '*' && (*srcH)[j + 1] == '*'))
                j++;
            if (j + 1 < srcLen) {
"""
replace_bold_ins = """
        if (i + 1 < srcLen && ((*srcH)[i] == '*' || (*srcH)[i] == '_') && (*srcH)[i] == (*srcH)[i + 1]) {
            char delim = (*srcH)[i];
            long j = i + 2;

            while (j + 1 < srcLen && !((*srcH)[j] == delim && (*srcH)[j + 1] == delim))
                j++;
            if (j + 1 < srcLen) {
"""
text = text.replace(search_bold_ins, replace_bold_ins)

# 2. InsertMarkdownAsStyled: Italic
search_italic_ins = """
        if ((*srcH)[i] == '*') {
            long j = i + 1;

            while (j < srcLen && (*srcH)[j] != '*')
                j++;
            if (j < srcLen) {
"""
replace_italic_ins = """
        if ((*srcH)[i] == '*' || (*srcH)[i] == '_') {
            char delim = (*srcH)[i];
            long j = i + 1;

            while (j < srcLen && (*srcH)[j] != delim)
                j++;
            if (j < srcLen) {
"""
text = text.replace(search_italic_ins, replace_italic_ins)


# 3. InsertMarkdownAsStyled: Link/Image
search_link_ins = """
        if ((*srcH)[i] == '[' || ((*srcH)[i] == '!' && i + 1 < len && (*srcH)[i+1] == '[')) {
            Boolean isImage = ((*srcH)[i] == '!');
            long openBracket = isImage ? i + 1 : i;
            long closeBracket = openBracket + 1;

            while (closeBracket < srcLen && (*srcH)[closeBracket] != ']')
"""
# Wait, my previous search_link replacement for BuildHiddenView accidentally touched InsertMarkdownAsStyled but it failed because 'len' was used instead of 'srcLen'.
# Let's check what InsertMarkdownAsStyled actually has right now.

import re

with open('app/markdown.c', 'r') as f:
    text = f.read()

# ClearMarkdownInSelection: Bold
search_clear_bold = """
        if (i + 1 < selEnd && (*textH)[i] == '*' && (*textH)[i + 1] == '*') {
            long j = i + 2;

            while (j + 1 < selEnd && !((*textH)[j] == '*' && (*textH)[j + 1] == '*'))
                j++;
"""
replace_clear_bold = """
        if (i + 1 < selEnd && ((*textH)[i] == '*' || (*textH)[i] == '_') && (*textH)[i] == (*textH)[i + 1]) {
            char delim = (*textH)[i];
            long j = i + 2;

            while (j + 1 < selEnd && !((*textH)[j] == delim && (*textH)[j + 1] == delim))
                j++;
"""
text = text.replace(search_clear_bold, replace_clear_bold)

# ClearMarkdownInSelection: Italic
search_clear_italic = """
        if ((*textH)[i] == '*') {
            long j = i + 1;

            while (j < selEnd && (*textH)[j] != '*')
                j++;
"""
replace_clear_italic = """
        if ((*textH)[i] == '*' || (*textH)[i] == '_') {
            char delim = (*textH)[i];
            long j = i + 1;

            while (j < selEnd && (*textH)[j] != delim)
                j++;
"""
text = text.replace(search_clear_italic, replace_clear_italic)


# DetectInlineMarkdown: Bold/Italic
search_detect_start = """
    } else if (justTyped == '*') {
"""
replace_detect_start = """
    } else if (justTyped == '*' || justTyped == '_') {
"""
text = text.replace(search_detect_start, replace_detect_start)

# Inside DetectInlineMarkdown, replace '*' with justTyped
# We have to be careful. Let's just replace exact strings for the bold/italic blocks.
search_detect_bold1 = """
        if (caret >= 4 && (*textH)[caret - 2] == '*' && (*textH)[caret - 1] == '*') {
            long p = caret - 4;

            while (p >= lineStart) {
                if ((*textH)[p] == '*' && (*textH)[p + 1] == '*' && p + 2 < caret - 2) {
"""
replace_detect_bold1 = """
        if (caret >= 4 && (*textH)[caret - 2] == justTyped && (*textH)[caret - 1] == justTyped) {
            long p = caret - 4;

            while (p >= lineStart) {
                if ((*textH)[p] == justTyped && (*textH)[p + 1] == justTyped && p + 2 < caret - 2) {
"""
text = text.replace(search_detect_bold1, replace_detect_bold1)

search_detect_bold2 = """
                    if ((*textH)[q] == '*' && (*textH)[q + 1] == '*') {
"""
replace_detect_bold2 = """
                    if ((*textH)[q] == justTyped && (*textH)[q + 1] == justTyped) {
"""
text = text.replace(search_detect_bold2, replace_detect_bold2)

search_detect_italic1 = """
        } else if (caret >= 3 && (*textH)[caret - 2] != '*') {
            long p = caret - 2;

            while (p >= lineStart) {
                if ((*textH)[p] == '*' &&
                    (p == lineStart || (*textH)[p - 1] != '*') &&
                    (*textH)[p + 1] != '*' && p + 1 < caret - 1) {
"""
replace_detect_italic1 = """
        } else if (caret >= 3 && (*textH)[caret - 2] != justTyped) {
            long p = caret - 2;

            while (p >= lineStart) {
                if ((*textH)[p] == justTyped &&
                    (p == lineStart || (*textH)[p - 1] != justTyped) &&
                    (*textH)[p + 1] != justTyped && p + 1 < caret - 1) {
"""
text = text.replace(search_detect_italic1, replace_detect_italic1)

search_detect_italic2 = """
                    if ((*textH)[q] == '*' &&
                        (*textH)[q - 1] != '*' &&
                        (q + 1 == lineEnd || (*textH)[q + 1] != '*') &&
"""
replace_detect_italic2 = """
                    if ((*textH)[q] == justTyped &&
                        (*textH)[q - 1] != justTyped &&
                        (q + 1 == lineEnd || (*textH)[q + 1] != justTyped) &&
"""
text = text.replace(search_detect_italic2, replace_detect_italic2)

with open('app/markdown.c', 'w') as f:
    f.write(text)

print("Done")

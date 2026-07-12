import re

with open('app/markdown.c', 'r') as f:
    text = f.read()

search = """
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
                for (lvl = 1; lvl <= 6; lvl++) {
                    if (firstStyle.tsSize == CurrentFontSize() + (7 - lvl) * 2) {
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
            if (outLen >= 1 && (*outH)[outLen - 1] == '\r') {
                if (outLen < 2 || (*outH)[outLen - 2] != '\r') {
                    (*outH)[outLen++] = '\r';
                }
            }
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '\r';
        } else if (isBlockquote) {
            (*outH)[outLen++] = '>';
            (*outH)[outLen++] = ' ';
        } else if (lineEnd > lineStart && (*srcH)[lineStart] == '\\245' && (*srcH)[lineStart + 1] == ' ') {
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = ' ';
            textOffset = lineStart + 2;
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
        
        if (!isHR && !isHeading && !isBlockquote) {
            long p = lineStart;
            while (p < lineEnd && ((*srcH)[p] == ' ' || (*srcH)[p] == '\\t')) p++;
            if (p < lineEnd && (*srcH)[p] == '\\245' && p + 1 < lineEnd && (*srcH)[p + 1] == ' ') {
                isListItem = true;
            } else {
                textOffset = p;
            }
        }
"""

replace = """
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

        if (!isHR && !isHeading && !isBlockquote) {
            long p = lineStart;
            while (p < lineEnd && ((*srcH)[p] == ' ' || (*srcH)[p] == '\\t')) p++;
            if (p < lineEnd && (*srcH)[p] == '\\245' && p + 1 < lineEnd && (*srcH)[p + 1] == ' ') {
                isListItem = true;
            } else {
                textOffset = p;
            }
        }

        // Apply pre-padding for Headings, Blockquotes, and Lists
        if (isHeading || isHR ||
            (isBlockquote && !prevWasBlockquote) ||
            (!isBlockquote && prevWasBlockquote) ||
            (isListItem && !prevWasListItem) ||
            (!isListItem && prevWasListItem)) {
            if (outLen >= 1 && (*outH)[outLen - 1] == '\\r') {
                if (outLen < 2 || (*outH)[outLen - 2] != '\\r') {
                    (*outH)[outLen++] = '\\r';
                }
            }
        }

        if (isHR) {
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '\\r';
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
            (*outH)[outLen++] = '>';
            (*outH)[outLen++] = ' ';
        } else if (isListItem) {
            long p = lineStart;
            while (p < lineEnd && ((*srcH)[p] == ' ' || (*srcH)[p] == '\\t')) {
                (*outH)[outLen++] = (*srcH)[p];
                p++;
            }
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = ' ';
            textOffset = p + 2;
        }

"""
text = text.replace(search, replace)

search_var = """
    lineStart = 0;
    while (lineStart < len) {
        Boolean isHR = false;
"""
replace_var = """
    Boolean prevWasBlockquote = false;
    Boolean prevWasListItem = false;
    Boolean prevWasHeading = false;

    lineStart = 0;
    while (lineStart < len) {
        Boolean isHR = false;
"""
text = text.replace(search_var, replace_var)

search_end = """
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
            (*outH)[outLen++] = '\\r';
        }

        
        lineStart = lineEnd + 1;
    }
"""
replace_end = """
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
            (*outH)[outLen++] = '\\r';
        }

        if (isHeading && outLen >= 1 && (*outH)[outLen - 1] == '\\r') {
            if (outLen < 2 || (*outH)[outLen - 2] != '\\r') {
                (*outH)[outLen++] = '\\r';
            }
        }

        prevWasBlockquote = isBlockquote;
        prevWasListItem = isListItem;
        prevWasHeading = isHeading;

        lineStart = lineEnd + 1;
    }
"""
text = text.replace(search_end, replace_end)


with open('app/markdown.c', 'w') as f:
    f.write(text)

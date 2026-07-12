import re

with open('app/markdown.c', 'r') as f:
    text = f.read()

# DetectInlineMarkdown: Angle brackets, Backslash escaping, and BoldItalic 'X'
search_detect = """
    while (i < len) {
"""
replace_detect = """
    while (i < len) {
        if (i + 1 < len && (*srcH)[i] == '\\\\' && ((*srcH)[i+1] == '*' || (*srcH)[i+1] == '_' || (*srcH)[i+1] == '#' || (*srcH)[i+1] == '>' || (*srcH)[i+1] == '[' || (*srcH)[i+1] == '`' || (*srcH)[i+1] == '\\\\')) {
            i += 2;
            continue;
        }

        if ((*srcH)[i] == '<') {
            long closeAngle = i + 1;
            while (closeAngle < len && (*srcH)[closeAngle] != '>')
                closeAngle++;
            if (closeAngle < len) {
                long urlLen = closeAngle - (i + 1);
                if (urlLen > 7 && urlLen < 255) {
                    if (((*srcH)[i+1] == 'h' && (*srcH)[i+2] == 't' && (*srcH)[i+3] == 't' && (*srcH)[i+4] == 'p') ||
                        ((*srcH)[i+1] == 'm' && (*srcH)[i+2] == 'a' && (*srcH)[i+3] == 'i' && (*srcH)[i+4] == 'l')) {
                        return true;
                    }
                }
            }
        }

        if (i + 2 < len && ((*srcH)[i] == '*' || (*srcH)[i] == '_') && (*srcH)[i] == (*srcH)[i + 1] && (*srcH)[i] == (*srcH)[i + 2]) {
            char delim = (*srcH)[i];
            long j = i + 3;
            while (j + 2 < len && !((*srcH)[j] == delim && (*srcH)[j + 1] == delim && (*srcH)[j + 2] == delim))
                j++;
            if (j + 2 < len) return true;
        }
"""
text = text.replace(search_detect, replace_detect)

with open('app/markdown.c', 'w') as f:
    f.write(text)

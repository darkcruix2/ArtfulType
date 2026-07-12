import re

with open('app/markdown.c', 'r') as f:
    text = f.read()

# 1. InsertMarkdownAsStyled: Backslash Escaping
search_insert_loop = """
    i = 0;
    while (i < srcLen) {
        if (i + 1 < srcLen && ((*srcH)[i] == '*' || (*srcH)[i] == '_') && (*srcH)[i] == (*srcH)[i + 1]) {
"""
replace_insert_loop = """
    i = 0;
    while (i < srcLen) {
        if (i + 1 < srcLen && (*srcH)[i] == '\\\\' && ((*srcH)[i+1] == '*' || (*srcH)[i+1] == '_' || (*srcH)[i+1] == '#' || (*srcH)[i+1] == '>' || (*srcH)[i+1] == '[' || (*srcH)[i+1] == '`' || (*srcH)[i+1] == '\\\\')) {
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
"""
text = text.replace(search_insert_loop, replace_insert_loop)

# 2. InsertMarkdownAsStyled: Apply Style 'X'
search_apply_x = """
            case 'B':
                opStyle.tsFace = bold;
                TESetStyle(doFace, &opStyle, false, te);
                break;
"""
replace_apply_x = """
            case 'X':
                opStyle.tsFace = bold | italic;
                TESetStyle(doFace, &opStyle, false, te);
                break;
            case 'B':
                opStyle.tsFace = bold;
                TESetStyle(doFace, &opStyle, false, te);
                break;
"""
text = text.replace(search_apply_x, replace_apply_x)


# 3. InsertMarkdownAsStyled: Angle Bracket URLs
search_angle_ins = """
        if ((*srcH)[i] == '[' || ((*srcH)[i] == '!' && i + 1 < srcLen && (*srcH)[i+1] == '[')) {
"""
replace_angle_ins = """
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
"""
text = text.replace(search_angle_ins, replace_angle_ins)

with open('app/markdown.c', 'w') as f:
    f.write(text)


import re

with open('app/markdown.c', 'r') as f:
    text = f.read()

# 1. SyncHiddenToCanonical: Fix space stripping for nested lists
search_strip = """
        if (!isHR && !isHeading && !isBlockquote) {
            if (lineEnd > lineStart && (*srcH)[lineStart] == '\\245' && (*srcH)[lineStart + 1] == ' ') {
                isListItem = true;
            } else {
                while (textOffset < lineEnd && ((*srcH)[textOffset] == ' ' || (*srcH)[textOffset] == '\\t')) {
                    textOffset++;
                }
            }
        }
"""
replace_strip = """
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
text = text.replace(search_strip, replace_strip)

# 2. SyncHiddenToCanonical: Handle Bold+Italic ('X')
search_vars = """
                Boolean isBold = false, isItalic = false, isCode = false, isImageLink = false;
"""
replace_vars = """
                Boolean isBold = false, isItalic = false, isCode = false, isImageLink = false, isBoldItalic = false;
"""
text = text.replace(search_vars, replace_vars)

search_kinds = """
                            if (ops[k].kind == 'B') isBold = true;
                            if (ops[k].kind == 'I') isItalic = true;
"""
replace_kinds = """
                            if (ops[k].kind == 'X') isBoldItalic = true;
                            if (ops[k].kind == 'B') isBold = true;
                            if (ops[k].kind == 'I') isItalic = true;
"""
text = text.replace(search_kinds, replace_kinds)

search_emit = """
                if (isBold) { (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; }
                if (isItalic) (*outH)[outLen++] = '*';
"""
replace_emit = """
                if (isBoldItalic) { (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; }
                if (isBold) { (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; }
                if (isItalic) (*outH)[outLen++] = '*';
"""
text = text.replace(search_emit, replace_emit)

search_emit2 = """
                if (isItalic) (*outH)[outLen++] = '*';
                if (isBold) { (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; }
"""
replace_emit2 = """
                if (isItalic) (*outH)[outLen++] = '*';
                if (isBold) { (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; }
                if (isBoldItalic) { (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; (*outH)[outLen++] = '*'; }
"""
text = text.replace(search_emit2, replace_emit2)


# 3. EncodeSelectionAsMarkdown: Handle Bold+Italic ('X')
search_enc_vars = """
    Boolean inBold = false, inItalic = false, inCode = false, inLink = false, isImageLink = false;
"""
replace_enc_vars = """
    Boolean inBold = false, inItalic = false, inCode = false, inLink = false, isImageLink = false, inBoldItalic = false;
"""
text = text.replace(search_enc_vars, replace_enc_vars)

search_enc_want = """
        Boolean wantBold = false, wantItalic = false, wantCode = false, wantLink = false;
"""
replace_enc_want = """
        Boolean wantBold = false, wantItalic = false, wantCode = false, wantLink = false, wantBoldItalic = false;
"""
text = text.replace(search_enc_want, replace_enc_want)

search_enc_st = """
            TEGetStyle((short) i, &st, &dlh, &dfa, te);
            wantBold = (st.tsFace & bold) != 0;
            wantItalic = (st.tsFace & italic) != 0;
"""
replace_enc_st = """
            TEGetStyle((short) i, &st, &dlh, &dfa, te);
            if ((st.tsFace & bold) != 0 && (st.tsFace & italic) != 0) {
                wantBoldItalic = true;
            } else {
                wantBold = (st.tsFace & bold) != 0;
                wantItalic = (st.tsFace & italic) != 0;
            }
"""
text = text.replace(search_enc_st, replace_enc_st)

search_enc_off = """
        if (inCode && !wantCode) { (*outH)[outLen++] = '`'; inCode = false; }
        if (inItalic && !wantItalic) { (*outH)[outLen++] = '*'; inItalic = false; }
        if (inBold && !wantBold) {
            (*outH)[outLen++] = '*';
            (*outH)[outLen++] = '*';
            inBold = false;
        }
"""
replace_enc_off = """
        if (inCode && !wantCode) { (*outH)[outLen++] = '`'; inCode = false; }
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
"""
text = text.replace(search_enc_off, replace_enc_off)

search_enc_on = """
        if (!inBold && wantBold) {
            (*outH)[outLen++] = '*';
            (*outH)[outLen++] = '*';
            inBold = true;
        }
        if (!inItalic && wantItalic) { (*outH)[outLen++] = '*'; inItalic = true; }
"""
replace_enc_on = """
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
"""
text = text.replace(search_enc_on, replace_enc_on)


with open('app/markdown.c', 'w') as f:
    f.write(text)


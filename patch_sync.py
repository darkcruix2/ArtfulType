import re

with open('app/markdown.c', 'r') as f:
    text = f.read()

search_ops = """
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
                        }
                    }
                }
                HUnlock(gWriterOpsH);
            }
"""
replace_ops = """
        Boolean isBlockquote = false;
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
                        }
                    }
                }
                HUnlock(gWriterOpsH);
            }
"""
text = text.replace(search_ops, replace_ops)


search_hr = """
        if (isHR) {
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '-';
        } else if (lineEnd > lineStart && (*srcH)[lineStart] == '\\245' && (*srcH)[lineStart + 1] == ' ') {
"""
replace_hr = """
        if (isHR) {
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '-';
            (*outH)[outLen++] = '-';
        } else if (isBlockquote) {
            (*outH)[outLen++] = '>';
            (*outH)[outLen++] = ' ';
        } else if (lineEnd > lineStart && (*srcH)[lineStart] == '\\245' && (*srcH)[lineStart + 1] == ' ') {
"""
text = text.replace(search_hr, replace_hr)


search_vars = """
                short linkID = 0;
                Boolean isBold = false, isItalic = false, isCode = false;
"""
replace_vars = """
                short linkID = 0;
                Boolean isBold = false, isItalic = false, isCode = false, isImageLink = false;
"""
text = text.replace(search_vars, replace_vars)

search_kinds = """
                            if (ops[k].kind == 'B') isBold = true;
                            if (ops[k].kind == 'I') isItalic = true;
                            if (ops[k].kind == 'C') isCode = true;
                            if (ops[k].kind == 'L') linkID = ops[k].linkID;
"""
replace_kinds = """
                            if (ops[k].kind == 'B') isBold = true;
                            if (ops[k].kind == 'I') isItalic = true;
                            if (ops[k].kind == 'C') isCode = true;
                            if (ops[k].kind == 'L') { linkID = ops[k].linkID; isImageLink = false; }
                            if (ops[k].kind == 'M') { linkID = ops[k].linkID; isImageLink = true; }
"""
text = text.replace(search_kinds, replace_kinds)


search_bracket = """
                if (linkID > 0) (*outH)[outLen++] = '[';
"""
replace_bracket = """
                if (linkID > 0) {
                    if (isImageLink) (*outH)[outLen++] = '!';
                    (*outH)[outLen++] = '[';
                }
"""
text = text.replace(search_bracket, replace_bracket)


with open('app/markdown.c', 'w') as f:
    f.write(text)

print("Done")

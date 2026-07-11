import re

with open('app/markdown.c', 'r') as f:
    text = f.read()

# Fix the compile error (len -> srcLen)
search_bad = """if ((*srcH)[i] == '[' || ((*srcH)[i] == '!' && i + 1 < len && (*srcH)[i+1] == '[')) {"""
replace_good = """if ((*srcH)[i] == '[' || ((*srcH)[i] == '!' && i + 1 < srcLen && (*srcH)[i+1] == '[')) {"""
text = text.replace(search_bad, replace_good)

# Add image ! rendering and M kind
search_link_inner = """
                    for (m = i + 1; m < closeBracket; m++)
                        (*outH)[outLen++] = (*srcH)[m];
"""
replace_link_inner = """
                    if (isImage) {
                        (*outH)[outLen++] = '!';
                    }
                    for (m = openBracket + 1; m < closeBracket; m++)
                        (*outH)[outLen++] = (*srcH)[m];
"""
text = text.replace(search_link_inner, replace_link_inner)

search_link_kind = """
                        ops[opCount].kind = 'L';
"""
replace_link_kind = """
                        ops[opCount].kind = isImage ? 'M' : 'L';
"""
text = text.replace(search_link_kind, replace_link_kind)

search_apply_m = """
            case 'L':
                opStyle.tsFace = underline;
                opStyle.tsColor.red = ops[k].linkID;
                opStyle.tsColor.green = 0;
                opStyle.tsColor.blue = 0;
                TESetStyle(doFace + doColor, &opStyle, false, te);
                break;
"""
replace_apply_m = """
            case 'L':
            case 'M':
                opStyle.tsFace = underline;
                opStyle.tsColor.red = ops[k].linkID;
                opStyle.tsColor.green = (ops[k].kind == 'M') ? 1 : 0;
                opStyle.tsColor.blue = 0;
                TESetStyle(doFace + doColor, &opStyle, false, te);
                break;
"""
text = text.replace(search_apply_m, replace_apply_m)

with open('app/markdown.c', 'w') as f:
    f.write(text)

print("Done")

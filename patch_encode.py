import re

with open('app/markdown.c', 'r') as f:
    text = f.read()

search_vars = """
    Boolean inBold = false, inItalic = false, inCode = false, inLink = false;
"""
replace_vars = """
    Boolean inBold = false, inItalic = false, inCode = false, inLink = false, isImageLink = false;
"""
text = text.replace(search_vars, replace_vars)


search_st = """
            wantLink = (st.tsFace & underline) != 0;
            linkID = st.tsColor.red;
        }
"""
replace_st = """
            wantLink = (st.tsFace & underline) != 0;
            linkID = st.tsColor.red;
            isImageLink = (st.tsColor.green == 1);
        } else {
            isImageLink = false;
        }
"""
text = text.replace(search_st, replace_st)

search_bracket = """
        if (!inLink && wantLink) {
            (*outH)[outLen++] = '[';
            inLink = true;
"""
replace_bracket = """
        if (!inLink && wantLink) {
            if (isImageLink) (*outH)[outLen++] = '!';
            (*outH)[outLen++] = '[';
            inLink = true;
"""
text = text.replace(search_bracket, replace_bracket)

with open('app/markdown.c', 'w') as f:
    f.write(text)

print("Done")

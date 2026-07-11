import re

with open('app/markdown.c', 'r') as f:
    text = f.read()

search = """
        if (caret - 1 == lineStart + 1 && (*textH)[lineStart] == '-') {
"""
replace = """
        if (caret - 1 == lineStart + 1 && ((*textH)[lineStart] == '-' || (*textH)[lineStart] == '+' || (*textH)[lineStart] == '*')) {
"""
text = text.replace(search, replace)

with open('app/markdown.c', 'w') as f:
    f.write(text)

print("Done")

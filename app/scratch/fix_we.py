import os
import re

file_path = "/mnt/volume1/MyCode/workspace/ArtfulType/app/markdown.c"

with open(file_path, "r") as f:
    content = f.read()

# Helper to add helper functions if not present
helpers = """
static long GetTESelStart(WEHandle we) { long s, e; WEGetSelection(&s, &e, we); return s; }
static long GetTESelEnd(WEHandle we) { long s, e; WEGetSelection(&s, &e, we); return e; }
"""
if "GetTESelStart" not in content:
    # Insert after #include lines
    content = re.sub(r'(#include "app\.h"\n)', r'\1' + helpers + '\n', content, count=1)

# Basic replacements
content = re.sub(r'\bTESetSelect\(', 'WESetSelect(', content)
content = re.sub(r'\bTEDelete\(', 'WEDelete(', content)
content = re.sub(r'\bTECalText\(', 'WECalText(', content)
content = re.sub(r'\bTEUpdate\(', 'WEUpdate(', content)

# TextStyle -> WETextStyle (exclude ones already replaced to WEWETextStyle)
content = re.sub(r'\bTextStyle\b', 'WETextStyle', content)
content = re.sub(r'\bWEWETextStyle\b', 'WETextStyle', content)

# doFace, doFont, etc -> weDoFace, weDoFont, etc
for word in ['doFont', 'doFace', 'doSize', 'doColor']:
    content = re.sub(r'\b' + word + r'\b', 'we' + word.capitalize(), content)
content = re.sub(r'\bweWeDo', 'weDo', content, flags=re.IGNORECASE)

# TEInsert(ptr, len, te) -> WEInsert(ptr, len, NULL, te)
content = re.sub(r'\bTEInsert\(([^,]+),\s*([^,]+),\s*([^)]+)\)', r'WEInsert(\1, \2, NULL, \3)', content)

# TESetStyle(mode, style, redraw, te) -> WESetStyle(mode, style, te)
content = re.sub(r'\bTESetStyle\(([^,]+),\s*([^,]+),\s*([^,]+),\s*([^)]+)\)', r'WESetStyle(\1, \2, \4)', content)

# TEGetStyle(offset, style, lh, fa, te) -> WEGetStyle(offset, style, te)
content = re.sub(r'\bTEGetStyle\(([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^)]+)\)', r'WEGetStyle(\1, \2, \5)', content)

# TEStyleHandle -> remains TEStyleHandle. WETextStyleHandle -> TEStyleHandle
content = re.sub(r'\bWETextStyleHandle\b', 'TEStyleHandle', content)

# (**gTE).selStart -> GetTESelStart(gTE)
content = re.sub(r'\(\*\*([a-zA-Z0-9_]+)\)\.selStart', r'GetTESelStart(\1)', content)
# (**gTE).selEnd -> GetTESelEnd(gTE)
content = re.sub(r'\(\*\*([a-zA-Z0-9_]+)\)\.selEnd', r'GetTESelEnd(\1)', content)

# (**gTE).hText -> WEGetText(gTE)
content = re.sub(r'\(\*\*([a-zA-Z0-9_]+)\)\.hText', r'WEGetText(\1)', content)

# (**gTE).teLength -> WEGetTextLength(gTE)
content = re.sub(r'\(\*\*([a-zA-Z0-9_]+)\)\.teLength', r'WEGetTextLength(\1)', content)

# One specific fix: SuppressDrawing and RestoreDrawing take WEHandle now
content = re.sub(r'\bTEHandle\s+te\b', 'WEHandle te', content)

# EncodeSelectionAsMarkdown taking TEHandle te
content = re.sub(r'EncodeSelectionAsMarkdown\(short start, short end, TEHandle', 'EncodeSelectionAsMarkdown(short start, short end, WEHandle', content)
content = re.sub(r'InsertMarkdownAsStyled\(Handle srcH, long srcLen, TEHandle', 'InsertMarkdownAsStyled(Handle srcH, long srcLen, WEHandle', content)

with open(file_path, "w") as f:
    f.write(content)

print("Replacement complete.")

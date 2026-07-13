import os
import re

file_path = "/mnt/volume1/MyCode/workspace/ArtfulType/app/markdown.c"

with open(file_path, "r") as f:
    content = f.read()

# Fix capitalization of weDo constants
content = content.replace("weDofont", "weDoFont")
content = content.replace("weDoface", "weDoFace")
content = content.replace("weDosize", "weDoSize")
content = content.replace("weDocolor", "weDoColor")

# Fix EncodeSelectionAsMarkdown signature to use long
content = content.replace("Handle EncodeSelectionAsMarkdown(short start, short end, WEHandle te)", "Handle EncodeSelectionAsMarkdown(long start, long end, WEHandle te)")

# Fix SuppressDrawing
suppress_target = """void SuppressDrawing(WEHandle te, Rect *saved)
{
    *saved = (**te).viewRect;
    SetRect(&(**te).viewRect, OFFSCREEN_COORD, OFFSCREEN_COORD,
            OFFSCREEN_COORD + (saved->right - saved->left),
            OFFSCREEN_COORD + (saved->bottom - saved->top));
}"""

suppress_replacement = """void SuppressDrawing(WEHandle te, Rect *saved)
{
    LongRect viewRectLong;
    WEGetViewRect(&viewRectLong, te);
    saved->left = (short)viewRectLong.left;
    saved->top = (short)viewRectLong.top;
    saved->right = (short)viewRectLong.right;
    saved->bottom = (short)viewRectLong.bottom;
    
    Rect hiddenRect;
    SetRect(&hiddenRect, OFFSCREEN_COORD, OFFSCREEN_COORD,
            OFFSCREEN_COORD + (saved->right - saved->left),
            OFFSCREEN_COORD + (saved->bottom - saved->top));
    WESetRects(&hiddenRect, &hiddenRect, te);
}"""
content = content.replace(suppress_target, suppress_replacement)

# Fix RestoreDrawing
restore_target = """void RestoreDrawing(WEHandle te, Rect *saved)
{
    (**te).viewRect = *saved;
}"""

restore_replacement = """void RestoreDrawing(WEHandle te, Rect *saved)
{
    WESetRects(saved, saved, te);
}"""
content = content.replace(restore_target, restore_replacement)

with open(file_path, "w") as f:
    f.write(content)

print("Fixes applied.")

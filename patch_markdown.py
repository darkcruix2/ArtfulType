import re

with open('app/markdown.c', 'r') as f:
    content = f.read()

# 1. Headings (levels 1-6)
content = content.replace('level < 3', 'level < 6')
content = content.replace('lvl <= 3', 'lvl <= 6')
content = content.replace('(4 - ops[k].level) * 4', '(7 - ops[k].level) * 2')
content = content.replace('(4 - lvl) * 4', '(7 - lvl) * 2')
content = content.replace('(4 - level) * 4', '(7 - level) * 2')

# 2. Blockquotes ('Q') in SyncHiddenToCanonical
sync_hidden = """
            if (firstStyle.tsColor.blue == 1) {
"""
sync_hidden_repl = """
            if (firstStyle.tsFace & italic) {
                Boolean onlyQuote = true;
                // Check if this run is a blockquote
                // We'll rely on a flag if needed, but for now let's say italic + no bold is blockquote?
                // Wait, users can type italic text. Blockquotes are block level.
                // We shouldn't guess blockquotes based on firstStyle.tsFace & italic alone.
"""
# Actually, let's use a patch file instead of a python script. It's much cleaner and less prone to regex errors.

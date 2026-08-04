import sys

def main():
    # Update index.html
    with open('src/index.html', 'r') as f:
        content = f.read()

    content = content.replace('<option value="irix-cde">IRIX CDE</option>', 
                              '<option value="solaris-cde">Solaris CDE</option>\n            <option value="beos">BeOS</option>')

    with open('src/index.html', 'w') as f:
        f.write(content)

    # Update styles.css
    with open('src/styles.css', 'r') as f:
        content = f.read()

    # Replace irix-cde with solaris-cde globally
    content = content.replace('irix-cde', 'solaris-cde')
    content = content.replace('IRIX CDE', 'Solaris CDE')

    # Fix Solaris CDE Theme Palette
    old_solaris = """/* ── Solaris CDE Theme ─────────────────────────────────────────────────────────── */
[data-theme="solaris-cde"] {
  --bg:           #242636;
  --bg-sidebar:   #34374a;
  --bg-header:    #3b3e58;
  --bg-elevated:  #464a62;
  --bg-editor:    #1c1d28;
  --selection:    #d48800;
  --fg:           #e6e6f0;
  --comment:      #8a90ac;
  --cyan:         #56b6c2;
  --green:        #7bc27b;
  --orange:       #f09838;
  --pink:         #e06c9f;
  --purple:       #ae81ff;
  --red:          #e05252;
  --yellow:       #e5c07b;
  --border:       #585e7d;
  --glass-bg:     #34374a;

  --accent:       #d48800;
  --accent-hover: #f09838;
  --text-main:    #e6e6f0;
  --text-muted:   #8a90ac;

  --font-sans: 'Liberation Sans', 'DejaVu Sans', 'Helvetica', sans-serif;
  --font-mono: 'Liberation Mono', 'DejaVu Sans Mono', 'Courier New', monospace;

  --active-file-bg: #d48800;
  --active-file-fg: #111118;
}"""

    new_solaris = """/* ── Solaris CDE Theme ─────────────────────────────────────────────────────────── */
[data-theme="solaris-cde"] {
  --bg:           #485D75;
  --bg-sidebar:   #657989;
  --bg-header:    #657989;
  --bg-elevated:  #8B9EAE;
  --bg-editor:    #A7ADBA;
  --selection:    #3A4B59;
  --fg:           #000000;
  --comment:      #444444;
  --cyan:         #3A4B59;
  --green:        #485D75;
  --orange:       #8B9EAE;
  --pink:         #657989;
  --purple:       #3A4B59;
  --red:          #B32026;
  --yellow:       #A7ADBA;
  --border:       #3A4B59;
  --glass-bg:     #657989;

  --accent:       #B32026;
  --accent-hover: #D42830;
  --text-main:    #000000;
  --text-muted:   #222222;

  --font-sans: 'Liberation Sans', 'DejaVu Sans', 'Helvetica', sans-serif;
  --font-mono: 'Liberation Mono', 'DejaVu Sans Mono', 'Courier New', monospace;

  --active-file-bg: #B32026;
  --active-file-fg: #FFFFFF;
}"""

    content = content.replace(old_solaris, new_solaris)

    # Fix Solaris CDE Specific Visuals
    old_solaris_visuals = """/* ── Solaris CDE Specific Visuals ──────────────────────────────────────────────── */
[data-theme="solaris-cde"] body {
  background: #242636;
}

[data-theme="solaris-cde"] #app-header {
  background: #3b3e58;
  border-bottom: 2px solid;
  border-color: #5c6282 #1b1c28 #1b1c28 #5c6282;
}

[data-theme="solaris-cde"] .app-title {
  color: #f09838;
  font-weight: 700;
  letter-spacing: 0.05em;
}

[data-theme="solaris-cde"] .app-version {
  background: #1b1c28;
  color: #f09838;
  border: 1px solid #5c6282;
  border-radius: 0;
}

[data-theme="solaris-cde"] #sidebar {
  background: #34374a;
  border-right: 2px solid;
  border-color: #5c6282 #1b1c28 #1b1c28 #5c6282;
}

[data-theme="solaris-cde"] .sidebar-header {
  border-bottom: 2px solid #1b1c28;
}

[data-theme="solaris-cde"] .sidebar-header h2 {
  color: #f09838;
}

[data-theme="solaris-cde"] .file-item {
  border-radius: 0;
  color: #e6e6f0;
}

[data-theme="solaris-cde"] .file-item:hover {
  background: #464a62;
  color: #f09838;
}

[data-theme="solaris-cde"] .file-item.active {
  background: #d48800;
  color: #111118;
  font-weight: bold;
}

[data-theme="solaris-cde"] .file-item.active::before {
  color: #111118;
}

[data-theme="solaris-cde"] #toolbar {
  background: #34374a;
  border-bottom: 2px solid;
  border-color: #5c6282 #1b1c28 #1b1c28 #5c6282;
}

[data-theme="solaris-cde"] .icon-btn,
[data-theme="solaris-cde"] .btn {
  background: #464a62;
  color: #e6e6f0;
  border-radius: 0;
  border: 2px solid;
  border-color: #687094 #202230 #202230 #687094;
}

[data-theme="solaris-cde"] .icon-btn:hover,
[data-theme="solaris-cde"] .btn:hover {
  background: #565b78;
  color: #ffffff;
}

[data-theme="solaris-cde"] .fmt-btn.active {
  background: #d48800;
  color: #111118;
  border-color: #202230 #687094 #687094 #202230;
}

[data-theme="solaris-cde"] .primary-btn {
  background: #d48800;
  color: #111118;
  font-weight: bold;
  border-color: #f0a020 #8a5800 #8a5800 #f0a020;
}

[data-theme="solaris-cde"] .primary-btn:hover {
  background: #f09838;
}

[data-theme="solaris-cde"] #editor-area {
  background: #1c1d28;
  border: 2px solid;
  border-color: #1b1c28 #5c6282 #5c6282 #1b1c28;
}

[data-theme="solaris-cde"] #writer-view,
[data-theme="solaris-cde"] #markdown-input {
  background: #1c1d28;
  color: #e6e6f0;
  caret-color: #f09838;
}"""

    new_solaris_visuals = """/* ── Solaris CDE Specific Visuals ──────────────────────────────────────────────── */
[data-theme="solaris-cde"] body {
  background: #485D75;
}

[data-theme="solaris-cde"] #app-header {
  background: #657989;
  border-bottom: 2px solid;
  border-color: #8B9EAE #3A4B59 #3A4B59 #8B9EAE;
}

[data-theme="solaris-cde"] .app-title {
  color: #000000;
  font-weight: 700;
}

[data-theme="solaris-cde"] .app-version {
  background: #8B9EAE;
  color: #000000;
  border: 1px solid #3A4B59;
  border-radius: 0;
}

[data-theme="solaris-cde"] #sidebar {
  background: #657989;
  border-right: 2px solid;
  border-color: #8B9EAE #3A4B59 #3A4B59 #8B9EAE;
}

[data-theme="solaris-cde"] .sidebar-header {
  border-bottom: 2px solid #3A4B59;
}

[data-theme="solaris-cde"] .sidebar-header h2 {
  color: #000000;
}

[data-theme="solaris-cde"] .file-item {
  border-radius: 0;
  color: #000000;
}

[data-theme="solaris-cde"] .file-item:hover {
  background: #8B9EAE;
  color: #000000;
}

[data-theme="solaris-cde"] .file-item.active {
  background: #B32026;
  color: #FFFFFF;
  font-weight: bold;
}

[data-theme="solaris-cde"] .file-item.active::before {
  color: #FFFFFF;
}

[data-theme="solaris-cde"] #toolbar {
  background: #657989;
  border-bottom: 2px solid;
  border-color: #8B9EAE #3A4B59 #3A4B59 #8B9EAE;
}

[data-theme="solaris-cde"] .icon-btn,
[data-theme="solaris-cde"] .btn {
  background: #8B9EAE;
  color: #000000;
  border-radius: 0;
  border: 2px solid;
  border-color: #A7ADBA #3A4B59 #3A4B59 #A7ADBA;
}

[data-theme="solaris-cde"] .icon-btn:hover,
[data-theme="solaris-cde"] .btn:hover {
  background: #A7ADBA;
  color: #000000;
}

[data-theme="solaris-cde"] .fmt-btn.active {
  background: #B32026;
  color: #FFFFFF;
  border-color: #3A4B59 #A7ADBA #A7ADBA #3A4B59;
}

[data-theme="solaris-cde"] .primary-btn {
  background: #B32026;
  color: #FFFFFF;
  font-weight: bold;
  border-color: #D42830 #5A0A10 #5A0A10 #D42830;
}

[data-theme="solaris-cde"] .primary-btn:hover {
  background: #D42830;
}

[data-theme="solaris-cde"] #editor-area {
  background: #A7ADBA;
  border: 2px solid;
  border-color: #3A4B59 #FFFFFF #FFFFFF #3A4B59;
}

[data-theme="solaris-cde"] #writer-view,
[data-theme="solaris-cde"] #markdown-input {
  background: #A7ADBA;
  color: #000000;
  caret-color: #B32026;
}"""

    content = content.replace(old_solaris_visuals, new_solaris_visuals)

    beos_theme = """
/* ── BeOS Theme ─────────────────────────────────────────────────────────────── */
[data-theme="beos"] {
  --bg:           #336699;
  --bg-sidebar:   #E8E8E8;
  --bg-header:    #FFCC00;
  --bg-elevated:  #FFFFFF;
  --bg-editor:    #FFFFFF;
  --selection:    #183f93;
  --fg:           #000000;
  --comment:      #555555;
  --cyan:         #183f93;
  --green:        #008000;
  --orange:       #ff9900;
  --pink:         #cc00cc;
  --purple:       #183f93;
  --red:          #CC0000;
  --yellow:       #FFCC00;
  --border:       #8C8C8C;
  --glass-bg:     #E8E8E8;

  --accent:       #183f93;
  --accent-hover: #336699;
  --text-main:    #000000;
  --text-muted:   #666666;

  --font-sans: 'Swis721 BT', 'Helvetica Neue', Helvetica, Arial, sans-serif;
  --font-mono: 'Courier New', Courier, monospace;

  --active-file-bg: #FFCC00;
  --active-file-fg: #000000;
}

/* ── BeOS Specific Visuals ─────────────────────────────────────────────────── */
[data-theme="beos"] body {
  background: #336699;
}

[data-theme="beos"] #app-header {
  background: #FFCC00;
  border-bottom: 2px solid #000000;
}

[data-theme="beos"] .app-title {
  color: #000000;
  font-weight: bold;
}

[data-theme="beos"] .app-version {
  background: #FFFFFF;
  color: #000000;
  border: 1px solid #8C8C8C;
  border-radius: 2px;
}

[data-theme="beos"] #sidebar {
  background: #E8E8E8;
  border-right: 2px solid #8C8C8C;
}

[data-theme="beos"] .sidebar-header {
  border-bottom: 2px solid #8C8C8C;
}

[data-theme="beos"] .sidebar-header h2 {
  color: #183f93;
}

[data-theme="beos"] .file-item {
  border-radius: 0;
  color: #000000;
}

[data-theme="beos"] .file-item:hover {
  background: #D8D8D8;
}

[data-theme="beos"] .file-item.active {
  background: #FFCC00;
  color: #000000;
  font-weight: bold;
}

[data-theme="beos"] #toolbar {
  background: #E8E8E8;
  border-bottom: 2px solid #8C8C8C;
}

[data-theme="beos"] .icon-btn,
[data-theme="beos"] .btn {
  background: #E8E8E8;
  color: #000000;
  border-radius: 2px;
  border: 1px solid #8C8C8C;
  box-shadow: inset 1px 1px 0px #FFFFFF, 1px 1px 0px #A0A0A0;
}

[data-theme="beos"] .icon-btn:hover,
[data-theme="beos"] .btn:hover {
  background: #D8D8D8;
}

[data-theme="beos"] .icon-btn:active,
[data-theme="beos"] .btn:active {
  box-shadow: inset 1px 1px 0px #A0A0A0, 1px 1px 0px #FFFFFF;
}

[data-theme="beos"] .primary-btn {
  background: #FFCC00;
  font-weight: bold;
}

[data-theme="beos"] .primary-btn:hover {
  background: #F3C000;
}

[data-theme="beos"] #editor-area {
  background: #FFFFFF;
  border: 2px solid #8C8C8C;
  border-left: none;
}

[data-theme="beos"] #writer-view,
[data-theme="beos"] #markdown-input {
  background: #FFFFFF;
  color: #000000;
  caret-color: #CC0000;
}

"""

    content = content.replace('/* ── Reset ─────────────────────────────────────────────────────────────────── */', beos_theme + '/* ── Reset ─────────────────────────────────────────────────────────────────── */')

    with open('src/styles.css', 'w') as f:
        f.write(content)

if __name__ == '__main__':
    main()

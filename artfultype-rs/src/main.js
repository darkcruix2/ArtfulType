const { invoke } = window.__TAURI__.core;

// ─── State ────────────────────────────────────────────────────────────────────
let markdownInputEl;   // hidden textarea — stores raw markdown as source cache
let writerViewEl;      // contenteditable div — primary editing surface
let toggleModeBtn;
let modeIndicatorEl;
let isMarkdownMode = false; // false = Writer (default), true = raw Markdown
let statusMessageEl;
let wordCountEl;
let charCountEl;
let currentFilePath = null;
let platform = "linux";

// ─── Debounce ─────────────────────────────────────────────────────────────────
function debounce(fn, wait) {
  let timer = null;
  return function (...args) {
    clearTimeout(timer);
    timer = setTimeout(() => fn.apply(this, args), wait);
  };
}

// ─── Platform ─────────────────────────────────────────────────────────────────
function isPrimaryMod(e) {
  if (platform === "macos") return e.metaKey;
  return e.ctrlKey && !e.metaKey;
}

// ─── Stats ────────────────────────────────────────────────────────────────────
function updateStats(text) {
  const trimmed = text.trim();
  const words = trimmed === "" ? 0 : trimmed.split(/\s+/).length;
  wordCountEl.textContent = `${words} words`;
  charCountEl.textContent = `${text.length} chars`;
}

const debouncedStats = debounce(() => {
  // In Writer mode get text from the contenteditable
  const text = isMarkdownMode
    ? markdownInputEl.value
    : (writerViewEl.innerText || writerViewEl.textContent || "");
  updateStats(text);
}, 150);

// ─── HTML → Reduced Markdown Serializer ──────────────────────────────────────
// Converts the contenteditable HTML back to a readable Markdown subset.
// Does not aim for perfect round-trip fidelity — it produces clean,
// readable Markdown that covers the features available in the toolbar.

function nodeToMd(node) {
  if (node.nodeType === Node.TEXT_NODE) {
    return node.textContent;
  }
  if (node.nodeType !== Node.ELEMENT_NODE) return "";

  const tag = node.tagName.toLowerCase();
  const children = () => Array.from(node.childNodes).map(nodeToMd).join("");

  switch (tag) {
    case "h1": return `# ${children().trim()}\n\n`;
    case "h2": return `## ${children().trim()}\n\n`;
    case "h3": return `### ${children().trim()}\n\n`;
    case "h4": return `#### ${children().trim()}\n\n`;
    case "h5": return `##### ${children().trim()}\n\n`;
    case "h6": return `###### ${children().trim()}\n\n`;
    case "p":  return `${children()}\n\n`;
    case "br": return "  \n"; // markdown hard line break
    case "strong":
    case "b":  return `**${children()}**`;
    case "em":
    case "i":  return `*${children()}*`;
    case "del":
    case "s":  return `~~${children()}~~`;
    case "code": {
      // Inside a <pre> block — let the pre case handle it
      if (node.parentElement && node.parentElement.tagName.toLowerCase() === "pre") {
        return node.textContent;
      }
      return `\`${node.textContent}\``;
    }
    case "pre": {
      const codeEl = node.querySelector("code");
      const lang = codeEl
        ? (codeEl.className.replace(/language-/, "").trim())
        : "";
      const code = codeEl ? codeEl.textContent : node.textContent;
      return `\`\`\`${lang}\n${code}\n\`\`\`\n\n`;
    }
    case "a": {
      const href = node.getAttribute("href") || "";
      return `[${children()}](${href})`;
    }
    case "ul": {
      const items = Array.from(node.children)
        .map((li) => `- ${liToMd(li)}`)
        .join("\n");
      return `${items}\n\n`;
    }
    case "ol": {
      const items = Array.from(node.children)
        .map((li, i) => `${i + 1}. ${liToMd(li)}`)
        .join("\n");
      return `${items}\n\n`;
    }
    case "li": return liToMd(node);
    case "blockquote": {
      const inner = children()
        .trim()
        .split("\n")
        .map((l) => `> ${l}`)
        .join("\n");
      return `${inner}\n\n`;
    }
    case "hr": return `---\n\n`;
    case "table": {
      const rows = Array.from(node.querySelectorAll("tr"));
      if (rows.length === 0) return children();
      const headers = Array.from(rows[0].querySelectorAll("th,td")).map(
        (c) => c.textContent.trim()
      );
      const sep = headers.map(() => "---").join(" | ");
      const body = rows
        .slice(1)
        .map((r) =>
          Array.from(r.querySelectorAll("td,th"))
            .map((c) => c.textContent.trim())
            .join(" | ")
        )
        .filter(Boolean);
      return [headers.join(" | "), sep, ...body].join("\n") + "\n\n";
    }
    case "div":
    case "section":
    case "article": {
      const inner = children();
      // Add a trailing newline only if the div doesn't already end with one
      return inner.endsWith("\n") ? inner : `${inner}\n`;
    }
    case "span": return children();
    // Ignore purely structural / style elements
    case "script":
    case "style": return "";
    default: return children();
  }
}

function liToMd(li) {
  // Handle nested lists inside <li>
  let text = "";
  for (const child of li.childNodes) {
    if (child.nodeType === Node.TEXT_NODE) {
      text += child.textContent;
    } else if (child.nodeType === Node.ELEMENT_NODE) {
      const t = child.tagName.toLowerCase();
      if (t === "ul" || t === "ol") {
        // Indent nested list by 2 spaces
        const nested = nodeToMd(child).trimEnd();
        text +=
          "\n" +
          nested
            .split("\n")
            .map((l) => `  ${l}`)
            .join("\n");
      } else {
        text += nodeToMd(child);
      }
    }
  }
  return text.trim();
}

function htmlToMarkdown(el) {
  return Array.from(el.childNodes)
    .map(nodeToMd)
    .join("")
    .replace(/\n{3,}/g, "\n\n") // collapse excess blank lines
    .trim();
}

// ─── Markdown Render (Rust → HTML) ───────────────────────────────────────────
async function renderMarkdownToWriter(markdownText) {
  try {
    const html = await invoke("parse_markdown", { text: markdownText });
    writerViewEl.innerHTML = html;
  } catch (e) {
    console.error("parse_markdown failed", e);
    writerViewEl.innerHTML = `<p style="color:#f87171">Render error: ${e}</p>`;
  }
}

// ─── Mode Toggle ──────────────────────────────────────────────────────────────
async function toggleMode() {
  if (isMarkdownMode) {
    // Markdown → Writer: parse textarea content, render into writer view
    const md = markdownInputEl.value;
    isMarkdownMode = false;
    markdownInputEl.classList.add("hidden");
    writerViewEl.classList.remove("hidden");
    toggleModeBtn.textContent = "Markdown Mode";
    modeIndicatorEl.textContent = "Writer Mode";
    await renderMarkdownToWriter(md);
    writerViewEl.focus();
  } else {
    // Writer → Markdown: serialize HTML to markdown, show in textarea
    const md = htmlToMarkdown(writerViewEl);
    isMarkdownMode = true;
    writerViewEl.classList.add("hidden");
    markdownInputEl.classList.remove("hidden");
    markdownInputEl.value = md;
    toggleModeBtn.textContent = "Writer Mode";
    modeIndicatorEl.textContent = "Markdown Mode";
    markdownInputEl.focus();
  }
}

// ─── Formatting — Writer Mode ─────────────────────────────────────────────────

/**
 * Apply rich-text formatting in Writer mode using execCommand,
 * or wrap markdown syntax in Markdown mode.
 */
function applyRichFormat(execCmd, mdPrefix, mdSuffix = mdPrefix) {
  if (!isMarkdownMode) {
    document.execCommand(execCmd);
    writerViewEl.focus();
  } else {
    wrapMarkdownSelection(mdPrefix, mdSuffix);
  }
}

/**
 * Apply a block-level heading in Writer mode.
 * Uses execCommand('formatBlock') which works reliably in WebKit.
 */
function applyHeading(level) {
  if (!isMarkdownMode) {
    // execCommand formatBlock expects angle-bracketed tag names
    document.execCommand("formatBlock", false, `h${level}`);
    writerViewEl.focus();
  } else {
    setMarkdownHeading(level);
  }
}

/**
 * Insert inline code around selection.
 * execCommand has no 'code' command so we use the Range API.
 */
function applyCode() {
  if (!isMarkdownMode) {
    const sel = window.getSelection();
    if (!sel || sel.rangeCount === 0) return;
    const range = sel.getRangeAt(0);
    const selected = range.toString();
    range.deleteContents();
    const code = document.createElement("code");
    code.textContent = selected || " ";
    range.insertNode(code);
    // Place cursor after the element
    range.setStartAfter(code);
    range.setEndAfter(code);
    sel.removeAllRanges();
    sel.addRange(range);
    writerViewEl.focus();
  } else {
    wrapMarkdownSelection("`", "`");
  }
}

// ─── Formatting — Markdown Mode ───────────────────────────────────────────────

function wrapMarkdownSelection(prefix, suffix = prefix) {
  const ta = markdownInputEl;
  const s = ta.selectionStart;
  const e = ta.selectionEnd;
  const before = ta.value.slice(0, s);
  const sel = ta.value.slice(s, e);
  const after = ta.value.slice(e);

  if (sel.startsWith(prefix) && sel.endsWith(suffix)) {
    // Toggle off
    const inner = sel.slice(prefix.length, sel.length - suffix.length);
    ta.value = before + inner + after;
    ta.selectionStart = s;
    ta.selectionEnd = s + inner.length;
  } else {
    ta.value = before + prefix + sel + suffix + after;
    ta.selectionStart = s + prefix.length;
    ta.selectionEnd = s + prefix.length + sel.length;
  }
  ta.focus();
  updateStats(ta.value);
}

function setMarkdownHeading(level) {
  const ta = markdownInputEl;
  const pos = ta.selectionStart;
  const lineStart = ta.value.lastIndexOf("\n", pos - 1) + 1;
  const lineEnd = ta.value.indexOf("\n", pos);
  const end = lineEnd === -1 ? ta.value.length : lineEnd;
  const line = ta.value.slice(lineStart, end);
  const stripped = line.replace(/^#{1,6}\s*/, "");
  const prefix = "#".repeat(level) + " ";
  ta.value = ta.value.slice(0, lineStart) + prefix + stripped + ta.value.slice(end);
  ta.selectionStart = ta.selectionEnd = lineStart + prefix.length + stripped.length;
  ta.focus();
  updateStats(ta.value);
}

// ─── Insert at Cursor ─────────────────────────────────────────────────────────
function formatTime(d) {
  return d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" });
}
function formatDate(d) {
  return d.toLocaleDateString([], { year: "numeric", month: "long", day: "numeric" });
}

function insertAtCursor(text) {
  if (!isMarkdownMode) {
    // Insert into contenteditable via Selection API
    const sel = window.getSelection();
    if (sel && sel.rangeCount > 0) {
      const range = sel.getRangeAt(0);
      range.deleteContents();
      const textNode = document.createTextNode(text);
      range.insertNode(textNode);
      range.setStartAfter(textNode);
      range.setEndAfter(textNode);
      sel.removeAllRanges();
      sel.addRange(range);
    }
    writerViewEl.focus();
  } else {
    const ta = markdownInputEl;
    const s = ta.selectionStart;
    ta.value = ta.value.slice(0, s) + text + ta.value.slice(ta.selectionEnd);
    ta.selectionStart = ta.selectionEnd = s + text.length;
    ta.focus();
    updateStats(ta.value);
  }
}

// ─── File Operations ──────────────────────────────────────────────────────────

/** Gets the current markdown content regardless of active mode. */
function getCurrentMarkdown() {
  if (isMarkdownMode) {
    return markdownInputEl.value;
  }
  // Serialize Writer mode HTML → Markdown
  return htmlToMarkdown(writerViewEl);
}

async function openFile() {
  try {
    statusMessageEl.textContent = "Opening…";
    const fileData = await invoke("open_file_dialog");
    if (fileData) {
      currentFilePath = fileData.path;
      document.querySelector(".file-item.active").textContent = fileData.name;
      statusMessageEl.textContent = `Opened: ${fileData.name}`;

      if (isMarkdownMode) {
        // In Markdown mode just load raw text into textarea
        markdownInputEl.value = fileData.content;
        updateStats(fileData.content);
      } else {
        // In Writer mode render the file into the contenteditable
        await renderMarkdownToWriter(fileData.content);
        // Keep raw markdown cached for mode-switching
        markdownInputEl.value = fileData.content;
        updateStats(fileData.content);
      }
    } else {
      statusMessageEl.textContent = "Ready";
    }
  } catch (e) {
    console.error(e);
    statusMessageEl.textContent = "Error opening file";
  }
}

async function saveFile() {
  const content = getCurrentMarkdown();
  try {
    if (currentFilePath) {
      await invoke("save_file", { path: currentFilePath, content });
      statusMessageEl.textContent = "Saved.";
    } else {
      statusMessageEl.textContent = "Saving…";
      const savedPath = await invoke("save_file_dialog", { content });
      if (savedPath) {
        currentFilePath = savedPath;
        const filename = savedPath.split(/[/\\]/).pop();
        document.querySelector(".file-item.active").textContent = filename;
        statusMessageEl.textContent = `Saved: ${filename}`;
      } else {
        statusMessageEl.textContent = "Save cancelled.";
      }
    }
  } catch (e) {
    console.error(e);
    statusMessageEl.textContent = "Error saving file";
  }
}

function newFile() {
  currentFilePath = null;
  markdownInputEl.value = "";
  writerViewEl.innerHTML = "";
  document.querySelector(".file-item.active").textContent = "untitled.md";
  updateStats("");
  statusMessageEl.textContent = "New file";
  if (!isMarkdownMode) writerViewEl.focus();
  else markdownInputEl.focus();
}

// ─── Keyboard Shortcuts ───────────────────────────────────────────────────────
function handleKeydown(e) {
  const mod = isPrimaryMod(e);

  if (mod && !e.altKey) {
    switch (e.key.toLowerCase()) {
      case "s": e.preventDefault(); saveFile(); return;
      case "o": e.preventDefault(); openFile(); return;
      case "n": e.preventDefault(); newFile(); return;
      case "m": e.preventDefault(); toggleMode(); return;
      case "b": e.preventDefault(); applyRichFormat("bold",   "**"); return;
      case "i": e.preventDefault(); applyRichFormat("italic", "*");  return;
      case "k": e.preventDefault(); applyCode(); return;
      case "1": e.preventDefault(); applyHeading(1); return;
      case "2": e.preventDefault(); applyHeading(2); return;
      case "3": e.preventDefault(); applyHeading(3); return;
    }
  }

  // Ctrl+Alt+T — insert current time
  if (mod && e.altKey && (e.key === "t" || e.key === "T")) {
    e.preventDefault();
    insertAtCursor(formatTime(new Date()));
    return;
  }

  // Ctrl+Alt+D — insert current date
  if (mod && e.altKey && (e.key === "d" || e.key === "D")) {
    e.preventDefault();
    insertAtCursor(formatDate(new Date()));
    return;
  }
}

// ─── Initialisation ───────────────────────────────────────────────────────────
window.addEventListener("DOMContentLoaded", async () => {
  markdownInputEl = document.getElementById("markdown-input");
  writerViewEl    = document.getElementById("writer-view");
  toggleModeBtn   = document.getElementById("toggle-mode-btn");
  modeIndicatorEl = document.getElementById("mode-indicator");
  statusMessageEl = document.getElementById("status-message");
  wordCountEl     = document.getElementById("word-count");
  charCountEl     = document.getElementById("char-count");

  // Detect platform from Rust
  try { platform = await invoke("get_platform"); } catch (_) { platform = "linux"; }

  // ── Stats update on input ──
  writerViewEl.addEventListener("input", debouncedStats);
  markdownInputEl.addEventListener("input", debouncedStats);

  // ── Buttons ──
  document.getElementById("toggle-mode-btn").addEventListener("click", toggleMode);
  document.getElementById("open-file-btn").addEventListener("click",   openFile);
  document.getElementById("save-file-btn").addEventListener("click",   saveFile);
  document.getElementById("new-file-btn").addEventListener("click",    newFile);

  // ── Format toolbar ──
  document.getElementById("bold-btn").addEventListener("click",   () => applyRichFormat("bold",   "**"));
  document.getElementById("italic-btn").addEventListener("click", () => applyRichFormat("italic", "*"));
  document.getElementById("code-btn").addEventListener("click",   () => applyCode());
  document.getElementById("h1-btn").addEventListener("click",     () => applyHeading(1));
  document.getElementById("h2-btn").addEventListener("click",     () => applyHeading(2));
  document.getElementById("h3-btn").addEventListener("click",     () => applyHeading(3));
  document.getElementById("time-btn").addEventListener("click",   () => insertAtCursor(formatTime(new Date())));
  document.getElementById("date-btn").addEventListener("click",   () => insertAtCursor(formatDate(new Date())));

  // ── Keyboard shortcuts ──
  window.addEventListener("keydown", handleKeydown);

  // ── Default content ──
  const defaultMd =
    "# Welcome to ArtfulType Pro\n\n" +
    "Start writing your next masterpiece.\n\n" +
    "## Features\n\n" +
    "- Modern cross-platform architecture\n" +
    "- Beautiful typography\n" +
    "- Uncompromised performance\n\n" +
    "## Keyboard Shortcuts\n\n" +
    "| Action | Shortcut |\n" +
    "| --- | --- |\n" +
    "| **Bold** | Ctrl+B |\n" +
    "| *Italic* | Ctrl+I |\n" +
    "| `Code` | Ctrl+K |\n" +
    "| Heading 1–3 | Ctrl+1 / 2 / 3 |\n" +
    "| Insert Time | Ctrl+Alt+T |\n" +
    "| Insert Date | Ctrl+Alt+D |\n" +
    "| Toggle Mode | Ctrl+M |\n" +
    "| Save | Ctrl+S |\n" +
    "| Open | Ctrl+O |\n";

  markdownInputEl.value = defaultMd;
  markdownInputEl.classList.add("hidden");

  // Start in Writer Mode — render the default content
  isMarkdownMode = false;
  writerViewEl.classList.remove("hidden");
  modeIndicatorEl.textContent = "Writer Mode";
  toggleModeBtn.textContent = "Markdown Mode";
  await renderMarkdownToWriter(defaultMd);
  updateStats(defaultMd);
  writerViewEl.focus();
});

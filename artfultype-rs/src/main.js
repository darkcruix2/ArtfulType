const { invoke, convertFileSrc } = window.__TAURI__.core;

// ─── State ────────────────────────────────────────────────────────────────────
let markdownInputEl;
let writerViewEl;
let toggleModeBtn;
let modeIndicatorEl;
let isMarkdownMode = false;
let statusMessageEl;
let wordCountEl;
let charCountEl;
let currentFilePath = null;
let currentFileDir  = null; // directory of the open file, for resolving relative image paths
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
  const text = isMarkdownMode
    ? markdownInputEl.value
    : (writerViewEl.innerText || writerViewEl.textContent || "");
  updateStats(text);
}, 150);

// ─── Image URL Helpers ────────────────────────────────────────────────────────

/**
 * Returns true if a URL is a local filesystem path (not http/https/data/blob).
 */
function isLocalPath(src) {
  if (!src) return false;
  return !/^(https?:|data:|blob:|asset:|tauri:)/i.test(src);
}

/**
 * Resolves a potentially-relative src to an absolute path using the
 * directory of the currently open file, then converts it to a tauri
 * asset:// URL that the webview can display.
 */
function resolveImageSrc(src) {
  if (!isLocalPath(src)) return src; // already an absolute URL

  // If it looks like an absolute filesystem path (/home/...) use it directly
  if (src.startsWith("/")) {
    return convertFileSrc(src);
  }

  // Relative path — resolve against the directory of the open file
  if (currentFileDir) {
    const sep = "/";
    const abs = currentFileDir.replace(/\\/g, "/").replace(/\/$/, "") + sep + src.replace(/\\/g, "/");
    return convertFileSrc(abs);
  }

  return src; // can't resolve without a base dir — leave as-is
}

/**
 * After rendering HTML into the writer view, walk all <img> tags and convert
 * local src paths to asset:// URLs so the webview can display them.
 */
function fixImageSrcs(el) {
  el.querySelectorAll("img").forEach((img) => {
    const src = img.getAttribute("src");
    if (src) {
      img.setAttribute("src", resolveImageSrc(src));
    }
  });
}

// ─── HTML → Reduced Markdown Serializer ──────────────────────────────────────

function nodeToMd(node) {
  if (node.nodeType === Node.TEXT_NODE) return node.textContent;
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
    case "br": return "  \n";
    case "strong":
    case "b":  return `**${children()}**`;
    case "em":
    case "i":  return `*${children()}*`;
    case "del":
    case "s":  return `~~${children()}~~`;
    case "code": {
      if (node.parentElement && node.parentElement.tagName.toLowerCase() === "pre") {
        return node.textContent;
      }
      return `\`${node.textContent}\``;
    }
    case "pre": {
      const codeEl = node.querySelector("code");
      const lang = codeEl ? codeEl.className.replace(/language-/, "").trim() : "";
      const code = codeEl ? codeEl.textContent : node.textContent;
      return `\`\`\`${lang}\n${code}\n\`\`\`\n\n`;
    }
    case "a": {
      const href = node.getAttribute("href") || "";
      return `[${children()}](${href})`;
    }
    case "img": {
      const alt = node.getAttribute("alt") || "";
      // Recover the original src, not the resolved asset:// URL
      const src = node.dataset.originalSrc || node.getAttribute("src") || "";
      return `![${alt}](${src})`;
    }
    case "ul": {
      const items = Array.from(node.children)
        .map((li) => `- ${liToMd(li)}`).join("\n");
      return `${items}\n\n`;
    }
    case "ol": {
      const items = Array.from(node.children)
        .map((li, i) => `${i + 1}. ${liToMd(li)}`).join("\n");
      return `${items}\n\n`;
    }
    case "li": return liToMd(node);
    case "blockquote": {
      const inner = children().trim().split("\n").map((l) => `> ${l}`).join("\n");
      return `${inner}\n\n`;
    }
    case "hr": return `---\n\n`;
    case "table": {
      const rows = Array.from(node.querySelectorAll("tr"));
      if (rows.length === 0) return children();
      const headers = Array.from(rows[0].querySelectorAll("th,td")).map((c) => c.textContent.trim());
      const sep = headers.map(() => "---").join(" | ");
      const body = rows.slice(1)
        .map((r) => Array.from(r.querySelectorAll("td,th")).map((c) => c.textContent.trim()).join(" | "))
        .filter(Boolean);
      return [headers.join(" | "), sep, ...body].join("\n") + "\n\n";
    }
    case "div":
    case "section":
    case "article": {
      const inner = children();
      return inner.endsWith("\n") ? inner : `${inner}\n`;
    }
    case "span": return children();
    case "script":
    case "style": return "";
    default: return children();
  }
}

function liToMd(li) {
  let text = "";
  for (const child of li.childNodes) {
    if (child.nodeType === Node.TEXT_NODE) {
      text += child.textContent;
    } else if (child.nodeType === Node.ELEMENT_NODE) {
      const t = child.tagName.toLowerCase();
      if (t === "ul" || t === "ol") {
        const nested = nodeToMd(child).trimEnd();
        text += "\n" + nested.split("\n").map((l) => `  ${l}`).join("\n");
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
    .replace(/\n{3,}/g, "\n\n")
    .trim();
}

// ─── Markdown Render (Rust → HTML) ───────────────────────────────────────────

async function renderMarkdownToWriter(markdownText) {
  try {
    const html = await invoke("parse_markdown", { text: markdownText });
    writerViewEl.innerHTML = html;
    // Tag each img with its original src before we replace it, so the
    // serializer can write back the original path (not the asset:// URL)
    writerViewEl.querySelectorAll("img").forEach((img) => {
      const src = img.getAttribute("src");
      if (src) img.dataset.originalSrc = src;
    });
    fixImageSrcs(writerViewEl);
  } catch (e) {
    console.error("parse_markdown failed", e);
    writerViewEl.innerHTML = `<p style="color:#f87171">Render error: ${e}</p>`;
  }
}

// ─── Live Markdown Syntax Detection ──────────────────────────────────────────
// When typing in Writer mode, detect markdown syntax and apply formatting
// in real time (Typora-style):
//   # /## /### at line start → heading
//   **text** → bold, *text* → italic, `text` → code
//   ![alt](path) → rendered image

const BLOCK_TAGS = new Set(["P","DIV","H1","H2","H3","H4","H5","H6",
                             "LI","BLOCKQUOTE","PRE","SECTION","ARTICLE"]);

function isBlockEl(el) {
  return el && BLOCK_TAGS.has(el.tagName);
}

/** Returns the nearest block-level ancestor of a node within writerViewEl. */
function getBlockAncestor(node) {
  let el = node.nodeType === Node.TEXT_NODE ? node.parentElement : node;
  while (el && el !== writerViewEl) {
    if (isBlockEl(el)) return el;
    el = el.parentElement;
  }
  return null;
}

/**
 * Saves cursor position as {node, offset} for later restoration.
 * We use the text-node + character-offset approach so we survive
 * innerHTML replacements that keep the same DOM structure.
 */
function saveCursor() {
  const sel = window.getSelection();
  if (!sel || sel.rangeCount === 0) return null;
  const range = sel.getRangeAt(0);
  return { node: range.startContainer, offset: range.startOffset };
}

function restoreCursor(saved) {
  if (!saved) return;
  try {
    const sel = window.getSelection();
    const range = document.createRange();
    range.setStart(saved.node, Math.min(saved.offset, saved.node.textContent?.length ?? 0));
    range.collapse(true);
    sel.removeAllRanges();
    sel.addRange(range);
  } catch (_) {/* ignore stale nodes */}
}

// ── Heading detection ──

/**
 * If the block containing the cursor starts with ^#{1,3} $,
 * convert it to the corresponding heading element and strip the prefix.
 * Called on every space input while in a non-heading block.
 */
function tryApplyHeading() {
  const sel = window.getSelection();
  if (!sel || sel.rangeCount === 0) return;
  const range = sel.getRangeAt(0);
  const block = getBlockAncestor(range.startContainer);
  if (!block) return;

  // Only fire when block is a plain paragraph (not already a heading)
  if (/^H[1-6]$/.test(block.tagName)) return;

  const text = block.textContent;
  const m = text.match(/^(#{1,3}) /);
  if (!m) return;

  const level = m[1].length;
  const tag = `h${level}`;
  const content = text.slice(m[0].length); // strip "# "

  // Apply the heading via execCommand (keeps undo history)
  document.execCommand("formatBlock", false, tag);

  // Now strip the leading "# " from the new block's text content
  const newSel = window.getSelection();
  if (!newSel || newSel.rangeCount === 0) return;
  const newBlock = getBlockAncestor(newSel.getRangeAt(0).startContainer);
  if (!newBlock) return;

  if (newBlock.textContent.startsWith(m[0])) {
    // Preserve child nodes but strip the prefix from the first text node
    const firstText = newBlock.firstChild;
    if (firstText && firstText.nodeType === Node.TEXT_NODE) {
      firstText.textContent = firstText.textContent.slice(m[0].length);
    } else {
      // Fallback: set plain text content
      newBlock.textContent = content;
    }
    // Place cursor at start of heading content
    const r = document.createRange();
    const textNode = newBlock.firstChild || newBlock;
    r.setStart(textNode, 0);
    r.collapse(true);
    newSel.removeAllRanges();
    newSel.addRange(r);
  }
}

// ── Inline formatting detection ──

/**
 * Scans a text node for a complete inline markdown pattern.
 * When found (and the cursor is past the closing delimiter), replaces
 * the raw text with the formatted element and returns true.
 */
function tryInlinePattern(textNode, cursorOffset, regex, createElement) {
  const text = textNode.textContent;
  let match;
  // Reset lastIndex to scan from start
  regex.lastIndex = 0;
  while ((match = regex.exec(text)) !== null) {
    const matchEnd = match.index + match[0].length;
    // Only transform patterns that are fully before the cursor
    if (matchEnd > cursorOffset) continue;

    const before = text.slice(0, match.index);
    const after  = text.slice(matchEnd);

    const parent = textNode.parentNode;
    if (!parent) return false;

    // Build the replacement fragment
    const frag = document.createDocumentFragment();
    if (before) frag.appendChild(document.createTextNode(before));
    frag.appendChild(createElement(match[1]));
    const afterNode = document.createTextNode(after);
    frag.appendChild(afterNode);

    parent.replaceChild(frag, textNode);

    // Restore cursor right after the newly inserted element
    const sel = window.getSelection();
    const r = document.createRange();
    r.setStart(afterNode, 0);
    r.collapse(true);
    sel.removeAllRanges();
    sel.addRange(r);

    return true; // transformed — stop scanning this node
  }
  return false;
}

/**
 * Scans the text node at the cursor for completed inline markdown patterns.
 * Order matters: check bold (**) before italic (*) to avoid false positives.
 */
function tryApplyInlineFormats(range) {
  const node = range.startContainer;
  if (node.nodeType !== Node.TEXT_NODE) return;
  const offset = range.startOffset;

  // Bold: **content**
  if (tryInlinePattern(node, offset, /\*\*([^*\n]+)\*\*/g, (c) => {
    const el = document.createElement("strong"); el.textContent = c; return el;
  })) return;

  // Italic: *content* (not preceded/followed by another *)
  if (tryInlinePattern(node, offset, /(?<!\*)\*([^*\n]+)\*(?!\*)/g, (c) => {
    const el = document.createElement("em"); el.textContent = c; return el;
  })) return;

  // Inline code: `content`
  if (tryInlinePattern(node, offset, /`([^`\n]+)`/g, (c) => {
    const el = document.createElement("code"); el.textContent = c; return el;
  })) return;

  // Strikethrough: ~~content~~
  if (tryInlinePattern(node, offset, /~~([^~\n]+)~~/g, (c) => {
    const el = document.createElement("del"); el.textContent = c; return el;
  })) return;
}

/**
 * Detects a complete ![alt](path) image syntax in the current text node,
 * replaces it with a live <img> element, and converts local paths to asset URLs.
 */
function tryApplyImageSyntax(range) {
  const node = range.startContainer;
  if (node.nodeType !== Node.TEXT_NODE) return;
  const text = node.textContent;
  const offset = range.startOffset;

  const imgRx = /!\[([^\]]*)\]\(([^)]+)\)/g;
  let match;
  while ((match = imgRx.exec(text)) !== null) {
    if (match.index + match[0].length > offset) continue;

    const alt  = match[1];
    const src  = match[2];
    const before = text.slice(0, match.index);
    const after  = text.slice(match.index + match[0].length);

    const parent = node.parentNode;
    if (!parent) return;

    const frag = document.createDocumentFragment();
    if (before) frag.appendChild(document.createTextNode(before));

    // Wrap in a paragraph-ish element so the image isn't inline in a text run
    const img = document.createElement("img");
    img.setAttribute("alt", alt);
    img.dataset.originalSrc = src;
    img.setAttribute("src", resolveImageSrc(src));
    img.className = "writer-image";
    frag.appendChild(img);

    const afterNode = document.createTextNode(after);
    frag.appendChild(afterNode);
    parent.replaceChild(frag, node);

    const sel = window.getSelection();
    const r = document.createRange();
    r.setStart(afterNode, 0);
    r.collapse(true);
    sel.removeAllRanges();
    sel.addRange(r);
    return;
  }
}

/**
 * Master handler wired to the 'input' event on writerViewEl.
 * Dispatches to the appropriate live-markdown transformation.
 */
function handleLiveMarkdown(e) {
  if (isMarkdownMode) return;

  const sel = window.getSelection();
  if (!sel || sel.rangeCount === 0) return;
  const range = sel.getRangeAt(0);

  // ── Heading: triggered by typing a space ──
  if (e.inputType === "insertText" && e.data === " ") {
    tryApplyHeading();
    return; // heading check takes priority; don't also do inline on same space
  }

  // ── Closing delimiter typed — check for complete inline patterns ──
  // We check after *, `, ~, and ) (for images)
  const trigger = e.data;
  if (trigger === "*" || trigger === "`" || trigger === "~" || trigger === ")") {
    tryApplyInlineFormats(range);
    if (trigger === ")") tryApplyImageSyntax(range);
  }
}

// ─── Mode Toggle ──────────────────────────────────────────────────────────────

async function toggleMode() {
  if (isMarkdownMode) {
    const md = markdownInputEl.value;
    isMarkdownMode = false;
    markdownInputEl.classList.add("hidden");
    writerViewEl.classList.remove("hidden");
    toggleModeBtn.textContent = "Markdown Mode";
    modeIndicatorEl.textContent = "Writer Mode";
    await renderMarkdownToWriter(md);
    writerViewEl.focus();
  } else {
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

function applyRichFormat(execCmd, mdPrefix, mdSuffix = mdPrefix) {
  if (!isMarkdownMode) {
    document.execCommand(execCmd);
    writerViewEl.focus();
  } else {
    wrapMarkdownSelection(mdPrefix, mdSuffix);
  }
}

function applyHeading(level) {
  if (!isMarkdownMode) {
    document.execCommand("formatBlock", false, `h${level}`);
    writerViewEl.focus();
  } else {
    setMarkdownHeading(level);
  }
}

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
  const sel    = ta.value.slice(s, e);
  const after  = ta.value.slice(e);
  if (sel.startsWith(prefix) && sel.endsWith(suffix)) {
    const inner = sel.slice(prefix.length, sel.length - suffix.length);
    ta.value = before + inner + after;
    ta.selectionStart = s;
    ta.selectionEnd   = s + inner.length;
  } else {
    ta.value = before + prefix + sel + suffix + after;
    ta.selectionStart = s + prefix.length;
    ta.selectionEnd   = s + prefix.length + sel.length;
  }
  ta.focus();
  updateStats(ta.value);
}

function setMarkdownHeading(level) {
  const ta = markdownInputEl;
  const pos = ta.selectionStart;
  const lineStart = ta.value.lastIndexOf("\n", pos - 1) + 1;
  const lineEnd   = ta.value.indexOf("\n", pos);
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

function getCurrentMarkdown() {
  return isMarkdownMode ? markdownInputEl.value : htmlToMarkdown(writerViewEl);
}

async function openFile() {
  try {
    statusMessageEl.textContent = "Opening…";
    const fileData = await invoke("open_file_dialog");
    if (fileData) {
      currentFilePath = fileData.path;
      // Derive the directory from the full path
      currentFileDir = fileData.path.replace(/[/\\][^/\\]+$/, "") || null;

      document.querySelector(".file-item.active").textContent = fileData.name;
      statusMessageEl.textContent = `Opened: ${fileData.name}`;
      markdownInputEl.value = fileData.content;

      if (isMarkdownMode) {
        updateStats(fileData.content);
      } else {
        await renderMarkdownToWriter(fileData.content);
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
        currentFileDir  = savedPath.replace(/[/\\][^/\\]+$/, "") || null;
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
  currentFileDir  = null;
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
      case "s": e.preventDefault(); saveFile();      return;
      case "o": e.preventDefault(); openFile();      return;
      case "n": e.preventDefault(); newFile();       return;
      case "m": e.preventDefault(); toggleMode();    return;
      case "b": e.preventDefault(); applyRichFormat("bold",   "**"); return;
      case "i": e.preventDefault(); applyRichFormat("italic", "*");  return;
      case "k": e.preventDefault(); applyCode();     return;
      case "1": e.preventDefault(); applyHeading(1); return;
      case "2": e.preventDefault(); applyHeading(2); return;
      case "3": e.preventDefault(); applyHeading(3); return;
    }
  }
  if (mod && e.altKey) {
    if (e.key === "t" || e.key === "T") { e.preventDefault(); insertAtCursor(formatTime(new Date())); return; }
    if (e.key === "d" || e.key === "D") { e.preventDefault(); insertAtCursor(formatDate(new Date())); return; }
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

  try { platform = await invoke("get_platform"); } catch (_) { platform = "linux"; }

  // ── Input listeners ──
  writerViewEl.addEventListener("input", (e) => {
    handleLiveMarkdown(e);
    debouncedStats();
  });
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

  // ── Global keyboard shortcuts ──
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

  isMarkdownMode = false;
  writerViewEl.classList.remove("hidden");
  modeIndicatorEl.textContent = "Writer Mode";
  toggleModeBtn.textContent = "Markdown Mode";
  await renderMarkdownToWriter(defaultMd);
  updateStats(defaultMd);
  writerViewEl.focus();
});

const { invoke } = window.__TAURI__.core;

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
let currentFileDir  = null;
let platform = "linux";
let isDirty = false;
let autoSaveTimer = null;

const RECENT_KEY    = "artfultype-recent-v1";
const RECENT_MAX    = 10;
const SETTINGS_KEY  = "artfultype-settings-v1";

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

// ─── Settings ─────────────────────────────────────────────────────────────────
function loadSettings() {
  try { return JSON.parse(localStorage.getItem(SETTINGS_KEY) || "{}"); }
  catch { return {}; }
}
function saveSettings(obj) {
  const current = loadSettings();
  localStorage.setItem(SETTINGS_KEY, JSON.stringify({ ...current, ...obj }));
}

// ─── Dirty / Unsaved indicator ────────────────────────────────────────────────
function setDirty(dirty) {
  isDirty = dirty;
  const btn = document.getElementById("save-file-btn");
  if (!btn) return;
  if (dirty) {
    btn.classList.add("dirty");
    btn.title = "Unsaved changes – Save (Ctrl+S)";
  } else {
    btn.classList.remove("dirty");
    btn.title = "Save File (Ctrl+S)";
  }
}

// ─── Auto-save ────────────────────────────────────────────────────────────────
function startAutoSave(intervalMinutes) {
  clearInterval(autoSaveTimer);
  if (intervalMinutes > 0) {
    autoSaveTimer = setInterval(() => {
      if (isDirty && currentFilePath) {
        saveFile(true);
      }
    }, intervalMinutes * 60 * 1000);
  }
}

function applyAutoSaveSetting(intervalMinutes) {
  saveSettings({ autoSaveMinutes: intervalMinutes });
  startAutoSave(intervalMinutes);
  updateAutoSaveUI(intervalMinutes);
}

function updateAutoSaveUI(intervalMinutes) {
  const items = document.querySelectorAll(".autosave-option");
  items.forEach(el => {
    el.classList.toggle("active", parseInt(el.dataset.minutes) === intervalMinutes);
  });
  const label = document.getElementById("autosave-label");
  if (label) {
    label.textContent = intervalMinutes === 0 ? "Auto-save: Off" : `Auto-save: ${intervalMinutes}m`;
  }
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
  setDirty(true);
}, 150);

// ─── Saved Selection (for toolbar buttons that steal focus) ───────────────────
// When a toolbar button is clicked, the browser moves focus away from the
// contenteditable and the selection is lost. We save the last known range
// and restore it before any format operation so it targets the right place.
let _savedRange = null;

function saveSelection() {
  if (isMarkdownMode) return;
  const sel = window.getSelection();
  if (sel && sel.rangeCount > 0) {
    _savedRange = sel.getRangeAt(0).cloneRange();
  }
}

function restoreSelection() {
  if (!_savedRange || isMarkdownMode) return;
  const sel = window.getSelection();
  sel.removeAllRanges();
  sel.addRange(_savedRange);
}

// ─── Recent Files ─────────────────────────────────────────────────────────────
function loadRecentFiles() {
  try { return JSON.parse(localStorage.getItem(RECENT_KEY) || "[]"); }
  catch { return []; }
}
function saveRecentFiles(list) {
  localStorage.setItem(RECENT_KEY, JSON.stringify(list));
}
function addToRecentFiles(path, name) {
  let recent = loadRecentFiles().filter(f => f.path !== path);
  recent.unshift({ path, name });
  recent = recent.slice(0, RECENT_MAX);
  saveRecentFiles(recent);
  renderFileList();
}
function removeFromRecentFiles(path) {
  saveRecentFiles(loadRecentFiles().filter(f => f.path !== path));
  renderFileList();
}
function renderFileList() {
  const list = document.getElementById("file-list");
  list.innerHTML = "";
  const currentName = currentFilePath ? currentFilePath.split(/[/\\]/).pop() : "untitled.md";
  const activeItem = document.createElement("li");
  activeItem.className = "file-item active";
  activeItem.textContent = currentName;
  activeItem.title = currentFilePath || "(new file)";
  list.appendChild(activeItem);
  for (const f of loadRecentFiles()) {
    if (f.path === currentFilePath) continue;
    const li = document.createElement("li");
    li.className = "file-item";
    li.textContent = f.name;
    li.title = f.path;
    li.addEventListener("click", () => openRecentFile(f));
    list.appendChild(li);
  }
}

async function openRecentFile(item) {
  try {
    statusMessageEl.textContent = `Opening ${item.name}…`;
    const fileData = await invoke("read_file", { path: item.path });
    await applyOpenedFile(fileData);
    statusMessageEl.textContent = `Opened: ${fileData.name}`;
  } catch (e) {
    console.error(e);
    statusMessageEl.textContent = `Cannot open: ${item.name}`;
    removeFromRecentFiles(item.path);
  }
}

// ─── Image Helpers ────────────────────────────────────────────────────────────
function isLocalPath(src) {
  if (!src) return false;
  return !/^(https?:|data:|blob:|asset:|tauri:)/i.test(src);
}
function normalizePath(path) {
  const parts = path.split("/");
  const out = [];
  for (const p of parts) {
    if (p === ".." && out.length > 0) out.pop();
    else if (p !== ".") out.push(p);
  }
  return out.join("/");
}
function resolveToAbsolute(src) {
  if (!isLocalPath(src)) return null;
  if (src.startsWith("/")) return normalizePath(src);
  if (currentFileDir) return normalizePath(currentFileDir.replace(/\\/g, "/") + "/" + src);
  return null;
}
async function loadLocalImage(img, src) {
  const absPath = resolveToAbsolute(src);
  if (!absPath) return;
  try {
    const dataUrl = await invoke("read_image_base64", { path: absPath });
    img.setAttribute("src", dataUrl);
  } catch (err) {
    console.warn(`Image not found: ${absPath}`, err);
    img.setAttribute("alt", (img.getAttribute("alt") || "") + " [not found]");
  }
}
async function fixImageSrcs(el) {
  const imgs = Array.from(el.querySelectorAll("img"));
  await Promise.all(imgs.map(async (img) => {
    const src = img.getAttribute("src");
    if (!src || !isLocalPath(src)) return;
    img.dataset.originalSrc = src;
    await loadLocalImage(img, src);
  }));
}

// ─── HTML → Markdown Serializer ──────────────────────────────────────────────
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
    case "strong": case "b": return `**${children()}**`;
    case "em": case "i": return `*${children()}*`;
    case "del": case "s": return `~~${children()}~~`;
    case "code": {
      if (node.parentElement?.tagName.toLowerCase() === "pre") return node.textContent;
      return `\`${node.textContent}\``;
    }
    case "pre": {
      const codeEl = node.querySelector("code");
      const lang = codeEl ? codeEl.className.replace(/language-/, "").trim() : "";
      return `\`\`\`${lang}\n${(codeEl || node).textContent}\n\`\`\`\n\n`;
    }
    case "a": return `[${children()}](${node.getAttribute("href") || ""})`;
    case "img": {
      const src = node.dataset.originalSrc || node.getAttribute("src") || "";
      return `![${node.getAttribute("alt") || ""}](${src})`;
    }
    case "ul": {
      const items = Array.from(node.children).map((li) => {
        const cb = li.querySelector("input[type=checkbox]");
        if (cb) return `- [${cb.checked ? "x" : " "}] ${liToMd(li)}`;
        return `- ${liToMd(li)}`;
      }).join("\n");
      return `${items}\n\n`;
    }
    case "ol": {
      const items = Array.from(node.children)
        .map((li, i) => `${i + 1}. ${liToMd(li)}`).join("\n");
      return `${items}\n\n`;
    }
    case "li": return liToMd(node);
    case "blockquote": {
      const inner = children().trim().split("\n").map(l => `> ${l}`).join("\n");
      return `${inner}\n\n`;
    }
    case "hr": return `---\n\n`;
    case "table": {
      const rows = Array.from(node.querySelectorAll("tr"));
      if (!rows.length) return children();
      const headers = Array.from(rows[0].querySelectorAll("th,td")).map(c => c.textContent.trim());
      const body = rows.slice(1).map(r =>
        Array.from(r.querySelectorAll("td,th")).map(c => c.textContent.trim()).join(" | ")
      ).filter(Boolean);
      return [headers.join(" | "), headers.map(() => "---").join(" | "), ...body].join("\n") + "\n\n";
    }
    case "input": return "";
    case "div": case "section": case "article": {
      const inner = children();
      return inner.endsWith("\n") ? inner : inner + "\n";
    }
    case "span": return children();
    case "script": case "style": return "";
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
      if (t === "input") continue;
      if (t === "ul" || t === "ol") {
        const nested = nodeToMd(child).trimEnd();
        text += "\n" + nested.split("\n").map(l => `  ${l}`).join("\n");
      } else {
        text += nodeToMd(child);
      }
    }
  }
  return text.trim();
}
function htmlToMarkdown(el) {
  return Array.from(el.childNodes)
    .map(nodeToMd).join("")
    .replace(/\n{3,}/g, "\n\n").trim();
}

// ─── Markdown Render ──────────────────────────────────────────────────────────
async function renderMarkdownToWriter(markdownText) {
  try {
    const html = await invoke("parse_markdown", { text: markdownText });
    writerViewEl.innerHTML = html;
    await fixImageSrcs(writerViewEl);
  } catch (e) {
    writerViewEl.innerHTML = `<p style="color:var(--red)">Render error: ${e}</p>`;
  }
}

// ─── Mode Toggle ──────────────────────────────────────────────────────────────
async function toggleMode() {
  if (isMarkdownMode) {
    isMarkdownMode = false;
    markdownInputEl.classList.add("hidden");
    writerViewEl.classList.remove("hidden");
    toggleModeBtn.textContent = "Markdown Mode";
    modeIndicatorEl.textContent = "Writer Mode";
    await renderMarkdownToWriter(markdownInputEl.value);
    writerViewEl.focus();
  } else {
    isMarkdownMode = true;
    const md = htmlToMarkdown(writerViewEl);
    writerViewEl.classList.add("hidden");
    markdownInputEl.classList.remove("hidden");
    markdownInputEl.value = md;
    toggleModeBtn.textContent = "Writer Mode";
    modeIndicatorEl.textContent = "Markdown Mode";
    markdownInputEl.focus();
  }
}

// ─── DOM Helpers ──────────────────────────────────────────────────────────────
const BLOCK_TAGS = new Set([
  "P","DIV","H1","H2","H3","H4","H5","H6",
  "LI","BLOCKQUOTE","PRE","SECTION","ARTICLE"
]);
function isBlockEl(el) { return el && BLOCK_TAGS.has(el.tagName); }
function getBlockAncestor(node) {
  let el = node.nodeType === Node.TEXT_NODE ? node.parentElement : node;
  while (el && el !== writerViewEl) {
    if (isBlockEl(el)) return el;
    el = el.parentElement;
  }
  return null;
}
function placeCaret(node, offset) {
  const sel = window.getSelection();
  const r = document.createRange();
  r.setStart(node, offset);
  r.collapse(true);
  sel.removeAllRanges();
  sel.addRange(r);
  _savedRange = r.cloneRange();
}

// ─── Direct DOM List / Block Builders ────────────────────────────────────────
function buildUL(replaceTarget, remainingText) {
  const ul = document.createElement("ul");
  const li = document.createElement("li");
  const tn = document.createTextNode(remainingText);
  li.appendChild(tn); ul.appendChild(li);
  replaceTarget.parentNode.replaceChild(ul, replaceTarget);
  placeCaret(tn, 0);
}
function buildOL(replaceTarget, remainingText) {
  const ol = document.createElement("ol");
  const li = document.createElement("li");
  const tn = document.createTextNode(remainingText);
  li.appendChild(tn); ol.appendChild(li);
  replaceTarget.parentNode.replaceChild(ol, replaceTarget);
  placeCaret(tn, 0);
}
function buildTaskItem(replaceTarget, checked, remainingText) {
  const ul = document.createElement("ul");
  ul.className = "contains-task-list";
  const li = document.createElement("li");
  li.className = "task-list-item";
  const cb = document.createElement("input");
  cb.type = "checkbox"; cb.checked = !!checked;
  const tn = document.createTextNode(remainingText || "");
  li.appendChild(cb); li.appendChild(tn); ul.appendChild(li);
  replaceTarget.parentNode.replaceChild(ul, replaceTarget);
  placeCaret(tn, 0);
}
function buildBlockquote(replaceTarget, remainingText) {
  const bq = document.createElement("blockquote");
  const tn = document.createTextNode(remainingText);
  bq.appendChild(tn);
  replaceTarget.parentNode.replaceChild(bq, replaceTarget);
  placeCaret(tn, 0);
}

// Creates a new task-list <li> (with checkbox) and appends it after afterLi.
function newTaskListItem(afterLi, parentUl) {
  const li = document.createElement("li");
  li.className = "task-list-item";
  const cb = document.createElement("input");
  cb.type = "checkbox";
  const tn = document.createTextNode("");
  li.appendChild(cb); li.appendChild(tn);
  parentUl.insertBefore(li, afterLi.nextSibling);
  placeCaret(tn, 0);
}

// Exits (or unindents) a task list when Enter is pressed on an empty item.
function exitTaskList(li, parentUl) {
  const grandParentLi = parentUl.parentElement;
  if (grandParentLi && grandParentLi.tagName === "LI") {
    // We are in a nested list — outdent instead of fully exiting
    document.execCommand("outdent");
    return;
  }
  // Top-level task list → insert paragraph after and remove empty item
  const p = document.createElement("p");
  const tn = document.createTextNode("");
  p.appendChild(tn);
  parentUl.parentNode.insertBefore(p, parentUl.nextSibling);
  if (parentUl.children.length === 1) {
    parentUl.parentNode.removeChild(parentUl);
  } else {
    parentUl.removeChild(li);
  }
  placeCaret(tn, 0);
}

// ─── Space Key: Block Syntax Trigger ─────────────────────────────────────────
function handleSpaceInWriter() {
  const sel = window.getSelection();
  if (!sel || !sel.rangeCount) return false;
  const range = sel.getRangeAt(0);
  if (!range.collapsed) return false;
  const container = range.startContainer;
  if (container.nodeType !== Node.TEXT_NODE) return false;
  const textBefore = container.textContent.slice(0, range.startOffset);
  const textAfter  = container.textContent.slice(range.startOffset);
  const block = getBlockAncestor(container);
  if (block) {
    if (/^H[1-6]|LI|BLOCKQUOTE|PRE/.test(block.tagName)) return false;
    if (block.textContent !== textBefore) return false;
  } else {
    if (container.parentNode !== writerViewEl) return false;
  }
  const target = block || container;
  switch (textBefore) {
    case "-":
    case "*": buildUL(target, textAfter); return true;
    case "1.": buildOL(target, textAfter); return true;
    case ">": buildBlockquote(target, textAfter); return true;
  }
  if (/^- \[[ x]\]$/.test(textBefore)) {
    buildTaskItem(target, textBefore.includes("[x]"), textAfter);
    return true;
  }
  return false;
}

// ─── Formatting — Writer Mode ─────────────────────────────────────────────────
function writerExec(cmd) {
  restoreSelection();
  document.execCommand(cmd);
  writerViewEl.focus();
}

function applyRichFormat(execCmd, mdPrefix, mdSuffix = mdPrefix) {
  if (!isMarkdownMode) {
    writerExec(execCmd);
  } else {
    wrapMarkdownSelection(mdPrefix, mdSuffix);
  }
}

function applyHeading(level) {
  if (!isMarkdownMode) {
    restoreSelection();
    document.execCommand("formatBlock", false, `h${level}`);
    writerViewEl.focus();
  } else {
    setMarkdownHeading(level);
  }
}

function applyCode() {
  if (!isMarkdownMode) {
    restoreSelection();
    const sel = window.getSelection();
    if (!sel || !sel.rangeCount) return;
    const range = sel.getRangeAt(0);
    const selected = range.toString();
    range.deleteContents();
    const code = document.createElement("code");
    code.textContent = selected || " ";
    range.insertNode(code);
    range.setStartAfter(code); range.setEndAfter(code);
    sel.removeAllRanges(); sel.addRange(range);
    writerViewEl.focus();
  } else { wrapMarkdownSelection("`", "`"); }
}

function applyBlockquote() {
  if (!isMarkdownMode) {
    restoreSelection();
    document.execCommand("formatBlock", false, "blockquote");
    writerViewEl.focus();
  } else { wrapMarkdownLines("> "); }
}

function applyUnorderedList() {
  if (!isMarkdownMode) {
    restoreSelection();
    document.execCommand("insertUnorderedList");
    writerViewEl.focus();
  } else {
    const ta = markdownInputEl;
    const start = ta.value.lastIndexOf("\n", ta.selectionStart - 1) + 1;
    const line = ta.value.slice(start, ta.value.indexOf("\n", ta.selectionStart));
    if (!/^- /.test(line)) {
      const ins = "- ";
      ta.value = ta.value.slice(0, start) + ins + ta.value.slice(start);
      ta.selectionStart = ta.selectionEnd = ta.selectionStart + ins.length;
    }
    ta.focus();
  }
}

function applyOrderedList() {
  if (!isMarkdownMode) {
    restoreSelection();
    document.execCommand("insertOrderedList");
    writerViewEl.focus();
  } else {
    const ta = markdownInputEl;
    const start = ta.value.lastIndexOf("\n", ta.selectionStart - 1) + 1;
    const ins = "1. ";
    ta.value = ta.value.slice(0, start) + ins + ta.value.slice(start);
    ta.selectionStart = ta.selectionEnd = ta.selectionStart + ins.length;
    ta.focus();
  }
}

function applyCheckboxList() {
  if (!isMarkdownMode) {
    // Restore focus + selection that was lost when toolbar button was clicked
    restoreSelection();
    const sel = window.getSelection();
    if (!sel || !sel.rangeCount) {
      // No known selection — append at end of editor
      const ul = makeTaskListUL();
      writerViewEl.appendChild(ul);
      const tn = ul.querySelector("li").lastChild;
      placeCaret(tn, 0);
      writerViewEl.focus();
      return;
    }

    const range = sel.getRangeAt(0);
    const block = getBlockAncestor(range.startContainer);
    const ul = makeTaskListUL();

    if (block && block !== writerViewEl) {
      // If block is empty (only whitespace / <br>), replace it
      if (block.textContent.trim() === "" && block.tagName !== "LI") {
        block.parentNode.replaceChild(ul, block);
      } else {
        // Insert after current block
        block.parentNode.insertBefore(ul, block.nextSibling);
      }
    } else {
      writerViewEl.appendChild(ul);
    }

    const tn = ul.querySelector("li").lastChild;
    placeCaret(tn, 0);
    writerViewEl.focus();
  } else {
    const ta = markdownInputEl;
    const s = ta.selectionStart;
    const ins = "- [ ] ";
    ta.value = ta.value.slice(0, s) + ins + ta.value.slice(s);
    ta.selectionStart = ta.selectionEnd = s + ins.length;
    ta.focus();
  }
}

function makeTaskListUL() {
  const ul = document.createElement("ul");
  ul.className = "contains-task-list";
  const li = document.createElement("li");
  li.className = "task-list-item";
  const cb = document.createElement("input");
  cb.type = "checkbox";
  const tn = document.createTextNode("");
  li.appendChild(cb); li.appendChild(tn); ul.appendChild(li);
  return ul;
}

function applyHorizontalRule() {
  if (!isMarkdownMode) {
    restoreSelection();
    document.execCommand("insertHorizontalRule");
    writerViewEl.focus();
  } else {
    const ta = markdownInputEl;
    const s = ta.selectionStart;
    const ins = "\n---\n\n";
    ta.value = ta.value.slice(0, s) + ins + ta.value.slice(s);
    ta.selectionStart = ta.selectionEnd = s + ins.length;
    ta.focus();
  }
}

// ─── Formatting — Markdown Mode ───────────────────────────────────────────────
function wrapMarkdownSelection(prefix, suffix = prefix) {
  const ta = markdownInputEl;
  const s = ta.selectionStart, e = ta.selectionEnd;
  const before = ta.value.slice(0, s), sel = ta.value.slice(s, e), after = ta.value.slice(e);
  if (sel.startsWith(prefix) && sel.endsWith(suffix)) {
    const inner = sel.slice(prefix.length, sel.length - suffix.length);
    ta.value = before + inner + after;
    ta.selectionStart = s; ta.selectionEnd = s + inner.length;
  } else {
    ta.value = before + prefix + sel + suffix + after;
    ta.selectionStart = s + prefix.length; ta.selectionEnd = s + prefix.length + sel.length;
  }
  ta.focus(); debouncedStats();
}
function wrapMarkdownLines(prefix) {
  const ta = markdownInputEl;
  const s = ta.selectionStart;
  const lineStart = ta.value.lastIndexOf("\n", s - 1) + 1;
  ta.value = ta.value.slice(0, lineStart) + prefix + ta.value.slice(lineStart);
  ta.selectionStart = ta.selectionEnd = s + prefix.length;
  ta.focus();
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
    restoreSelection();
    const sel = window.getSelection();
    if (sel && sel.rangeCount > 0) {
      const range = sel.getRangeAt(0);
      range.deleteContents();
      const tn = document.createTextNode(text);
      range.insertNode(tn);
      range.setStartAfter(tn); range.setEndAfter(tn);
      sel.removeAllRanges(); sel.addRange(range);
    }
    writerViewEl.focus();
  } else {
    const ta = markdownInputEl;
    const s = ta.selectionStart;
    ta.value = ta.value.slice(0, s) + text + ta.value.slice(ta.selectionEnd);
    ta.selectionStart = ta.selectionEnd = s + text.length;
    ta.focus(); debouncedStats();
  }
}

// ─── Live Markdown Syntax ─────────────────────────────────────────────────────
function tryApplyHeading() {
  const sel = window.getSelection();
  if (!sel || !sel.rangeCount) return;
  const block = getBlockAncestor(sel.getRangeAt(0).startContainer);
  if (!block || /^H[1-6]$/.test(block.tagName)) return;
  const text = block.textContent;
  const m = text.match(/^(#{1,6}) /);
  if (!m) return;
  const level = m[1].length;
  const content = text.slice(m[0].length);
  document.execCommand("formatBlock", false, `h${level}`);
  const newSel = window.getSelection();
  if (!newSel || !newSel.rangeCount) return;
  const newBlock = getBlockAncestor(newSel.getRangeAt(0).startContainer);
  if (!newBlock) return;
  if (newBlock.textContent.startsWith(m[0])) {
    const first = newBlock.firstChild;
    if (first?.nodeType === Node.TEXT_NODE) {
      first.textContent = first.textContent.slice(m[0].length);
    } else {
      newBlock.textContent = content;
    }
    const r = document.createRange();
    const tn = newBlock.firstChild || newBlock;
    r.setStart(tn, 0); r.collapse(true);
    newSel.removeAllRanges(); newSel.addRange(r);
  }
}

function tryInlinePattern(textNode, cursorOffset, regex, createElement) {
  const text = textNode.textContent;
  regex.lastIndex = 0;
  let match;
  while ((match = regex.exec(text)) !== null) {
    const matchEnd = match.index + match[0].length;
    if (matchEnd > cursorOffset) continue;
    const before = text.slice(0, match.index);
    const after  = text.slice(matchEnd);
    const parent = textNode.parentNode;
    if (!parent) return false;
    const frag = document.createDocumentFragment();
    if (before) frag.appendChild(document.createTextNode(before));
    const el = createElement(match[1]);
    el.classList.add("just-formatted");
    frag.appendChild(el);
    const afterNode = document.createTextNode(after);
    frag.appendChild(afterNode);
    parent.replaceChild(frag, textNode);
    const sel = window.getSelection();
    const r = document.createRange();
    r.setStart(afterNode, 0); r.collapse(true);
    sel.removeAllRanges(); sel.addRange(r);
    return true;
  }
  return false;
}

function tryApplyInlineFormats(range) {
  const node = range.startContainer;
  if (node.nodeType !== Node.TEXT_NODE) return;
  const offset = range.startOffset;
  if (tryInlinePattern(node, offset, /\*\*([^*\n]+)\*\*/g, c => {
    const el = document.createElement("strong"); el.textContent = c; return el;
  })) return;
  if (tryInlinePattern(node, offset, /(?<!\*)\*([^*\n]+)\*(?!\*)/g, c => {
    const el = document.createElement("em"); el.textContent = c; return el;
  })) return;
  if (tryInlinePattern(node, offset, /`([^`\n]+)`/g, c => {
    const el = document.createElement("code"); el.textContent = c; return el;
  })) return;
  if (tryInlinePattern(node, offset, /~~([^~\n]+)~~/g, c => {
    const el = document.createElement("del"); el.textContent = c; return el;
  })) return;
}

async function tryApplyImageSyntax(range) {
  const node = range.startContainer;
  if (node.nodeType !== Node.TEXT_NODE) return;
  const text = node.textContent;
  const offset = range.startOffset;
  const imgRx = /!\[([^\]]*)\]\(([^)]+)\)/g;
  let match;
  while ((match = imgRx.exec(text)) !== null) {
    if (match.index + match[0].length > offset) continue;
    const alt = match[1], src = match[2];
    const before = text.slice(0, match.index);
    const after  = text.slice(match.index + match[0].length);
    const parent = node.parentNode;
    if (!parent) return;
    const frag = document.createDocumentFragment();
    if (before) frag.appendChild(document.createTextNode(before));
    const img = document.createElement("img");
    img.setAttribute("alt", alt); img.dataset.originalSrc = src;
    img.className = "writer-image"; img.setAttribute("src", "");
    frag.appendChild(img);
    const afterNode = document.createTextNode(after);
    frag.appendChild(afterNode);
    parent.replaceChild(frag, node);
    const sel = window.getSelection();
    const r = document.createRange();
    r.setStart(afterNode, 0); r.collapse(true);
    sel.removeAllRanges(); sel.addRange(r);
    loadLocalImage(img, src);
    return;
  }
}

function handleLiveMarkdown(e) {
  if (isMarkdownMode) return;
  const sel = window.getSelection();
  if (!sel || !sel.rangeCount) return;
  const range = sel.getRangeAt(0);
  if (e.inputType === "insertText" && e.data === " ") {
    tryApplyHeading();
    return;
  }
  const trigger = e.data;
  if (trigger === "*" || trigger === "`" || trigger === "~") {
    tryApplyInlineFormats(range);
  }
  if (trigger === ")") {
    tryApplyInlineFormats(range);
    tryApplyImageSyntax(range);
  }
}

// ─── Enter Key in Writer Mode ─────────────────────────────────────────────────
function handleWriterEnter(e) {
  const sel = window.getSelection();
  if (!sel || !sel.rangeCount) return false;
  const range = sel.getRangeAt(0);
  const block = getBlockAncestor(range.startContainer);
  if (!block) return false;

  // ── Task-list item Enter ──
  // Detect by class on the <li> itself so indented items (nested UL without
  // the contains-task-list class added by indent) are handled correctly.
  if (block.tagName === "LI") {
    const isTaskItem = block.classList.contains("task-list-item");
    if (isTaskItem) {
      e.preventDefault();
      if (!block.textContent.trim()) {
        exitTaskList(block, block.parentElement);
      } else {
        newTaskListItem(block, block.parentElement);
      }
      return true;
    }
    return false; // regular list: let browser handle
  }

  if (/^H[1-6]$/.test(block.tagName)) return false;
  if (block.tagName === "BLOCKQUOTE") return false;

  const text = block.textContent;

  // ── HR trigger: --- ──
  if (/^(-{3,}|\*{3,}|_{3,})$/.test(text.trim())) {
    e.preventDefault();
    block.textContent = "";
    document.execCommand("insertHorizontalRule");
    return true;
  }

  // ── Heading trigger: "#{1,6} <content>" ──
  const hm = text.match(/^(#{1,6}) (.+)$/);
  if (hm) {
    e.preventDefault();
    const level = hm[1].length;
    const content = hm[2];
    document.execCommand("formatBlock", false, `h${level}`);
    const updSel = window.getSelection();
    if (updSel && updSel.rangeCount > 0) {
      const hBlock = getBlockAncestor(updSel.getRangeAt(0).startContainer);
      if (hBlock) {
        hBlock.textContent = content;
        const r = document.createRange();
        r.selectNodeContents(hBlock); r.collapse(false);
        updSel.removeAllRanges(); updSel.addRange(r);
      }
    }
    document.execCommand("insertParagraph");
    document.execCommand("formatBlock", false, "p");
    return true;
  }
  return false;
}

// ─── File Operations ──────────────────────────────────────────────────────────
function getCurrentMarkdown() {
  return isMarkdownMode ? markdownInputEl.value : htmlToMarkdown(writerViewEl);
}

async function applyOpenedFile(fileData) {
  currentFilePath = fileData.path;
  currentFileDir  = fileData.path.replace(/[/\\][^/\\]+$/, "") || null;
  markdownInputEl.value = fileData.content;
  addToRecentFiles(fileData.path, fileData.name);
  if (isMarkdownMode) {
    updateStats(fileData.content);
  } else {
    await renderMarkdownToWriter(fileData.content);
    updateStats(fileData.content);
  }
  setDirty(false);
}

async function openFile() {
  try {
    statusMessageEl.textContent = "Opening…";
    const fileData = await invoke("open_file_dialog");
    if (fileData) {
      await applyOpenedFile(fileData);
      statusMessageEl.textContent = `Opened: ${fileData.name}`;
    } else {
      statusMessageEl.textContent = "Ready";
    }
  } catch (e) {
    console.error(e);
    statusMessageEl.textContent = "Error opening file";
  }
}

async function saveFile(silent = false) {
  const content = getCurrentMarkdown();
  try {
    if (currentFilePath) {
      await invoke("save_file", { path: currentFilePath, content });
      setDirty(false);
      if (!silent) statusMessageEl.textContent = "Saved.";
    } else {
      if (!silent) statusMessageEl.textContent = "Saving…";
      const savedPath = await invoke("save_file_dialog", { content });
      if (savedPath) {
        currentFilePath = savedPath;
        currentFileDir  = savedPath.replace(/[/\\][^/\\]+$/, "") || null;
        const filename = savedPath.split(/[/\\]/).pop();
        addToRecentFiles(savedPath, filename);
        setDirty(false);
        if (!silent) statusMessageEl.textContent = `Saved: ${filename}`;
      } else {
        if (!silent) statusMessageEl.textContent = "Save cancelled.";
      }
    }
  } catch (e) {
    console.error(e);
    if (!silent) statusMessageEl.textContent = "Error saving file";
  }
}

function newFile() {
  currentFilePath = null;
  currentFileDir  = null;
  markdownInputEl.value = "";
  writerViewEl.innerHTML = "";
  renderFileList();
  updateStats("");
  setDirty(false);
  statusMessageEl.textContent = "New file";
  if (!isMarkdownMode) writerViewEl.focus();
  else markdownInputEl.focus();
}

// ─── Keyboard Shortcuts ───────────────────────────────────────────────────────
function handleKeydown(e) {
  const mod = isPrimaryMod(e);

  if (!isMarkdownMode && e.key === "Enter" && !e.shiftKey && !mod) {
    if (handleWriterEnter(e)) return;
  }

  if (!isMarkdownMode && e.key === " " && !mod && !e.altKey && !e.shiftKey) {
    if (handleSpaceInWriter()) { e.preventDefault(); return; }
  }

  if (!isMarkdownMode && e.key === "Tab" && !mod) {
    const sel = window.getSelection();
    if (sel && sel.rangeCount > 0) {
      let node = sel.getRangeAt(0).startContainer;
      let li = null;
      while (node && node !== writerViewEl) {
        if (node.nodeType === Node.ELEMENT_NODE && node.tagName === "LI") { li = node; break; }
        node = node.parentNode;
      }
      if (li) {
        e.preventDefault();
        document.execCommand(e.shiftKey ? "outdent" : "indent");
        return;
      }
    }
  }

  // Undo / Redo
  if (mod && !e.altKey) {
    const k = e.key.toLowerCase();
    if (k === "z") {
      if (!isMarkdownMode) { e.preventDefault(); document.execCommand(e.shiftKey ? "redo" : "undo"); }
      return;
    }
    if (k === "y") {
      if (!isMarkdownMode) { e.preventDefault(); document.execCommand("redo"); }
      return;
    }
  }

  if (mod && !e.altKey) {
    switch (e.key.toLowerCase()) {
      case "s": e.preventDefault(); saveFile();          return;
      case "o": e.preventDefault(); openFile();          return;
      case "n": e.preventDefault(); newFile();           return;
      case "m": e.preventDefault(); toggleMode();        return;
      case "b": e.preventDefault(); applyRichFormat("bold",   "**"); return;
      case "i": e.preventDefault(); applyRichFormat("italic", "*");  return;
      case "k": e.preventDefault(); applyCode();         return;
      case "q": e.preventDefault(); applyBlockquote();   return;
      case "1": e.preventDefault(); applyHeading(1);     return;
      case "2": e.preventDefault(); applyHeading(2);     return;
      case "3": e.preventDefault(); applyHeading(3);     return;
      case "4": e.preventDefault(); applyHeading(4);     return;
      case "5": e.preventDefault(); applyHeading(5);     return;
      case "6": e.preventDefault(); applyHeading(6);     return;
    }
  }
  if (mod && e.altKey) {
    if (e.key === "t" || e.key === "T") { e.preventDefault(); insertAtCursor(formatTime(new Date())); return; }
    if (e.key === "d" || e.key === "D") { e.preventDefault(); insertAtCursor(formatDate(new Date())); return; }
  }
}

// ─── Undo / Redo toolbar ──────────────────────────────────────────────────────
function doUndo() {
  if (!isMarkdownMode) { document.execCommand("undo"); writerViewEl.focus(); }
  else markdownInputEl.focus();
}
function doRedo() {
  if (!isMarkdownMode) { document.execCommand("redo"); writerViewEl.focus(); }
  else markdownInputEl.focus();
}

// ─── Auto-save dropdown ───────────────────────────────────────────────────────
function toggleAutoSaveMenu(e) {
  e.stopPropagation();
  const menu = document.getElementById("autosave-menu");
  menu.classList.toggle("open");
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

  // ── Save selection whenever it changes in writer view ──
  // This ensures toolbar buttons (which steal focus) still operate
  // on the correct cursor position.
  document.addEventListener("selectionchange", () => {
    if (!isMarkdownMode && document.activeElement === writerViewEl) {
      saveSelection();
    }
  });
  writerViewEl.addEventListener("blur", saveSelection);

  // ── Input events ──
  writerViewEl.addEventListener("input", (e) => { handleLiveMarkdown(e); debouncedStats(); });
  markdownInputEl.addEventListener("input", debouncedStats);

  // ── File buttons ──
  document.getElementById("toggle-mode-btn").addEventListener("click", toggleMode);
  document.getElementById("open-file-btn").addEventListener("click",   openFile);
  document.getElementById("save-file-btn").addEventListener("click",   () => saveFile());
  document.getElementById("new-file-btn").addEventListener("click",    newFile);

  // ── Format toolbar ──
  document.getElementById("bold-btn").addEventListener("click",   () => applyRichFormat("bold",   "**"));
  document.getElementById("italic-btn").addEventListener("click", () => applyRichFormat("italic", "*"));
  document.getElementById("code-btn").addEventListener("click",   () => applyCode());
  document.getElementById("undo-btn").addEventListener("click",   doUndo);
  document.getElementById("redo-btn").addEventListener("click",   doRedo);
  for (let i = 1; i <= 6; i++) {
    document.getElementById(`h${i}-btn`).addEventListener("click", () => applyHeading(i));
  }
  document.getElementById("ul-btn").addEventListener("click",    () => applyUnorderedList());
  document.getElementById("ol-btn").addEventListener("click",    () => applyOrderedList());
  document.getElementById("cb-btn").addEventListener("click",    () => applyCheckboxList());
  document.getElementById("quote-btn").addEventListener("click", () => applyBlockquote());
  document.getElementById("hr-btn").addEventListener("click",    () => applyHorizontalRule());
  document.getElementById("time-btn").addEventListener("click",  () => insertAtCursor(formatTime(new Date())));
  document.getElementById("date-btn").addEventListener("click",  () => insertAtCursor(formatDate(new Date())));

  // ── Auto-save menu ──
  const autoSaveBtn = document.getElementById("autosave-btn");
  if (autoSaveBtn) {
    autoSaveBtn.addEventListener("click", toggleAutoSaveMenu);
  }
  document.querySelectorAll(".autosave-option").forEach(el => {
    el.addEventListener("click", (e) => {
      e.stopPropagation();
      applyAutoSaveSetting(parseInt(el.dataset.minutes));
      document.getElementById("autosave-menu").classList.remove("open");
    });
  });
  document.addEventListener("click", () => {
    document.getElementById("autosave-menu")?.classList.remove("open");
  });

  // ── Global keyboard shortcuts ──
  window.addEventListener("keydown", handleKeydown);

  // ── Restore settings ──
  const settings = loadSettings();
  const autoSaveMinutes = settings.autoSaveMinutes ?? 0;
  updateAutoSaveUI(autoSaveMinutes);
  startAutoSave(autoSaveMinutes);

  // ── File list ──
  renderFileList();

  // ── Default content ──
  const defaultMd =
    "# Welcome to ArtfulType Pro\n\n" +
    "Start writing your next masterpiece.\n\n" +
    "## Features\n\n" +
    "- **Writer mode** — live Markdown editing\n" +
    "- **Dracula theme** — beautiful dark palette\n" +
    "- Native file I/O with recent files\n" +
    "- Auto-save support\n\n" +
    "### Keyboard Shortcuts\n\n" +
    "| Action | Key |\n" +
    "| --- | --- |\n" +
    "| Bold | Ctrl+B |\n" +
    "| Italic | Ctrl+I |\n" +
    "| Code | Ctrl+K |\n" +
    "| H1–H6 | Ctrl+1–6 |\n" +
    "| Blockquote | Ctrl+Q |\n" +
    "| Undo | Ctrl+Z |\n" +
    "| Redo | Ctrl+Y |\n" +
    "| Toggle Mode | Ctrl+M |\n\n" +
    "#### Live triggers in Writer Mode\n\n" +
    "> Type `- ` → bullet list · `1. ` → numbered · `> ` → blockquote\n" +
    "> Type `# ` / `## ` / `### ` → headings\n\n" +
    "##### Task list example\n\n" +
    "- [x] Fix file dialog freeze\n" +
    "- [x] Dracula theme\n" +
    "- [x] Bullet list auto-convert\n" +
    "- [ ] More features\n\n" +
    "---\n\n";

  markdownInputEl.value = defaultMd;
  markdownInputEl.classList.add("hidden");
  writerViewEl.classList.remove("hidden");
  isMarkdownMode = false;
  modeIndicatorEl.textContent = "Writer Mode";
  toggleModeBtn.textContent = "Markdown Mode";
  await renderMarkdownToWriter(defaultMd);
  updateStats(defaultMd);
  setDirty(false);
  writerViewEl.focus();
});

const { invoke } = window.__TAURI__.core;

let markdownInputEl;
let writerViewEl;
let toggleModeBtn;
let isMarkdownMode = false;
let statusMessageEl;
let wordCountEl;
let charCountEl;
let openFileBtn;
let saveFileBtn;
let currentFilePath = null;

async function updatePreview() {
  const markdownText = markdownInputEl.value;
  updateStats(markdownText);
  if (!isMarkdownMode) {
    try {
      // Call Rust backend to parse markdown
      const htmlContent = await invoke("parse_markdown", { text: markdownText });
      writerViewEl.innerHTML = htmlContent;
    } catch (e) {
      console.error("Failed to parse markdown", e);
      writerViewEl.innerHTML = `<p style="color:red">Error parsing markdown: ${e}</p>`;
    }
  }
}

function updateStats(text) {
  const words = text.trim() === "" ? 0 : text.trim().split(/\s+/).length;
  const chars = text.length;
  wordCountEl.textContent = `${words} words`;
  charCountEl.textContent = `${chars} chars`;
}

function toggleMode() {
  isMarkdownMode = !isMarkdownMode;
  if (isMarkdownMode) {
    writerViewEl.classList.add("hidden");
    markdownInputEl.classList.remove("hidden");
    toggleModeBtn.textContent = "Writer Mode";
    markdownInputEl.focus();
  } else {
    markdownInputEl.classList.add("hidden");
    writerViewEl.classList.remove("hidden");
    toggleModeBtn.textContent = "Markdown Mode";
    updatePreview();
  }
}

async function openFile() {
  try {
    const fileData = await invoke("open_file_dialog");
    if (fileData) {
      currentFilePath = fileData.path;
      markdownInputEl.value = fileData.content;
      document.querySelector('.file-item.active').textContent = fileData.name;
      updatePreview();
      statusMessageEl.textContent = `Opened: ${fileData.name}`;
    }
  } catch (e) {
    console.error(e);
    statusMessageEl.textContent = "Error opening file";
  }
}

async function saveFile() {
  const content = markdownInputEl.value;
  try {
    if (currentFilePath) {
      await invoke("save_file", { path: currentFilePath, content });
      statusMessageEl.textContent = "File saved.";
    } else {
      const savedPath = await invoke("save_file_dialog", { content });
      if (savedPath) {
        currentFilePath = savedPath;
        // Extract filename from path
        const filename = savedPath.split(/[\\/]/).pop();
        document.querySelector('.file-item.active').textContent = filename;
        statusMessageEl.textContent = `Saved: ${filename}`;
      }
    }
  } catch (e) {
    console.error(e);
    statusMessageEl.textContent = "Error saving file";
  }
}

window.addEventListener("DOMContentLoaded", () => {
  markdownInputEl = document.getElementById("markdown-input");
  writerViewEl = document.getElementById("writer-view");
  toggleModeBtn = document.getElementById("toggle-mode-btn");
  statusMessageEl = document.getElementById("status-message");
  wordCountEl = document.getElementById("word-count");
  charCountEl = document.getElementById("char-count");
  openFileBtn = document.getElementById("open-file-btn");
  saveFileBtn = document.getElementById("save-file-btn");

  markdownInputEl.addEventListener("input", updatePreview);
  toggleModeBtn.addEventListener("click", toggleMode);
  openFileBtn.addEventListener("click", openFile);
  saveFileBtn.addEventListener("click", saveFile);

  window.addEventListener("keydown", (e) => {
    if (e.ctrlKey || e.metaKey) {
      if (e.key === "s") {
        e.preventDefault();
        saveFile();
      } else if (e.key === "o") {
        e.preventDefault();
        openFile();
      } else if (e.key === "n") {
        e.preventDefault();
        markdownInputEl.value = "";
        currentFilePath = null;
        document.querySelector('.file-item.active').textContent = "untitled.md";
        updatePreview();
      }
    }
  });

  // Set default content
  markdownInputEl.value = "# Welcome to ArtfulType Pro\\n\\nStart writing your next masterpiece.\\n\\n- Modern cross-platform architecture\\n- Beautiful typography\\n- Uncompromised performance";
  
  // Start in Writer Mode
  isMarkdownMode = false;
  markdownInputEl.classList.add("hidden");
  updatePreview();
});

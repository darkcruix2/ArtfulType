// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use pulldown_cmark::{Parser, Options, html};
use rfd::AsyncFileDialog;
use std::fs;
use std::path::Path;
use base64::{Engine as _, engine::general_purpose::STANDARD};
use serde::{Serialize, Deserialize};

#[tauri::command]
fn parse_markdown(text: &str) -> String {
    // Enable common markdown extensions for better rendering
    let mut opts = Options::empty();
    opts.insert(Options::ENABLE_STRIKETHROUGH);
    opts.insert(Options::ENABLE_TABLES);
    opts.insert(Options::ENABLE_FOOTNOTES);
    opts.insert(Options::ENABLE_TASKLISTS);

    let parser = Parser::new_ext(text, opts);
    let mut html_output = String::with_capacity(text.len() * 2);
    html::push_html(&mut html_output, parser);
    html_output
}

#[derive(Serialize, Deserialize)]
struct FileData {
    path: String,
    name: String,
    content: String,
}

/// Returns the current platform so the frontend can adjust keyboard shortcuts.
/// Values: "linux", "windows", "macos"
#[tauri::command]
fn get_platform() -> &'static str {
    if cfg!(target_os = "linux") {
        "linux"
    } else if cfg!(target_os = "windows") {
        "windows"
    } else {
        "macos"
    }
}

/// Opens a native file-picker dialog asynchronously.
/// Using AsyncFileDialog prevents the GTK dialog from blocking the main thread
/// on Linux, which was the cause of the application freeze.
#[tauri::command]
async fn open_file_dialog() -> Option<FileData> {
    let file = AsyncFileDialog::new()
        .add_filter("Markdown", &["md", "markdown", "txt"])
        .pick_file()
        .await;

    if let Some(handle) = file {
        let path = handle.path().to_path_buf();
        // Read file content asynchronously to avoid blocking
        let content = fs::read_to_string(&path).unwrap_or_default();
        let name = path
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_else(|| "untitled.md".to_string());
        return Some(FileData {
            path: path.to_string_lossy().into_owned(),
            name,
            content,
        });
    }
    None
}

/// Opens a native save dialog asynchronously.
#[tauri::command]
async fn save_file_dialog(content: String) -> Option<String> {
    let file = AsyncFileDialog::new()
        .add_filter("Markdown", &["md", "markdown", "txt"])
        .save_file()
        .await;

    if let Some(handle) = file {
        let path = handle.path().to_path_buf();
        let _ = fs::write(&path, content.as_bytes());
        return Some(path.to_string_lossy().into_owned());
    }
    None
}

#[tauri::command]
fn save_file(path: &str, content: &str) -> Result<(), String> {
    fs::write(path, content).map_err(|e| e.to_string())
}

/// Reads an image from an absolute local path and returns it as a
/// "data:image/TYPE;base64,DATA" URL ready to use as an <img src>.
/// This avoids all asset-protocol / CSP / scope issues.
#[tauri::command]
fn read_image_base64(path: String) -> Result<String, String> {
    let bytes = fs::read(&path).map_err(|e| format!("Cannot read {path}: {e}"))?;
    let ext = Path::new(&path)
        .extension()
        .and_then(|e| e.to_str())
        .unwrap_or("")
        .to_ascii_lowercase();
    let mime = match ext.as_str() {
        "png"  => "image/png",
        "jpg" | "jpeg" => "image/jpeg",
        "gif"  => "image/gif",
        "webp" => "image/webp",
        "svg"  => "image/svg+xml",
        "bmp"  => "image/bmp",
        "ico"  => "image/x-icon",
        "avif" => "image/avif",
        _      => "application/octet-stream",
    };
    let b64 = STANDARD.encode(&bytes);
    Ok(format!("data:{mime};base64,{b64}"))
}

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![
            parse_markdown,
            get_platform,
            open_file_dialog,
            save_file_dialog,
            save_file,
            read_image_base64
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

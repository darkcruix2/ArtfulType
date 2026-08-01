// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use pulldown_cmark::{Parser, html};
use rfd::FileDialog;
use std::fs;
use serde::{Serialize, Deserialize};

#[tauri::command]
fn parse_markdown(text: &str) -> String {
    let parser = Parser::new(text);
    let mut html_output = String::new();
    html::push_html(&mut html_output, parser);
    html_output
}

#[derive(Serialize, Deserialize)]
struct FileData {
    path: String,
    name: String,
    content: String,
}

#[tauri::command]
fn open_file_dialog() -> Option<FileData> {
    let file = FileDialog::new()
        .add_filter("Markdown", &["md", "markdown", "txt"])
        .pick_file();

    if let Some(path) = file {
        let content = fs::read_to_string(&path).unwrap_or_default();
        let name = path.file_name().unwrap().to_string_lossy().into_owned();
        return Some(FileData {
            path: path.to_string_lossy().into_owned(),
            name,
            content,
        });
    }
    None
}

#[tauri::command]
fn save_file_dialog(content: &str) -> Option<String> {
    let file = FileDialog::new()
        .add_filter("Markdown", &["md", "markdown", "txt"])
        .save_file();

    if let Some(path) = file {
        let _ = fs::write(&path, content);
        return Some(path.to_string_lossy().into_owned());
    }
    None
}

#[tauri::command]
fn save_file(path: &str, content: &str) -> Result<(), String> {
    fs::write(path, content).map_err(|e| e.to_string())
}

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![
            parse_markdown,
            open_file_dialog,
            save_file_dialog,
            save_file
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

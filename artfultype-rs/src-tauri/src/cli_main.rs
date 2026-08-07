use crossterm::{
    event::{self, Event, KeyCode, KeyModifiers},
    execute,
    terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen},
};
use ratatui::{
    backend::CrosstermBackend,
    layout::{Constraint, Direction, Layout, Rect},
    style::{Color, Modifier, Style},
    text::{Line, Span, Text},
    widgets::{Block, Borders, Clear, List, ListItem, Paragraph},
    Terminal,
};
use std::{
    env, fs,
    io::{self, stdout},
    path::PathBuf,
};

#[derive(Debug, Clone, Copy, PartialEq)]
enum ViewMode {
    Writer,
    Markdown,
    Split,
    PureText,
}

#[derive(Debug, Clone, PartialEq)]
enum PopupState {
    None,
    QuitConfirm,
    SaveAs {
        current_dir: String,
        entries: Vec<(String, bool)>,
        selected: usize,
        scroll: usize,
        input: String,
        input_focused: bool,
    },
    OpenFile {
        current_dir: String,
        entries: Vec<(String, bool)>,
        selected: usize,
        scroll: usize,
    },
    Search { input: String },
    SearchReplace { search: String, replace: String, step: u8 }, // step 0: search, step 1: replace
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum Theme {
    DarkAntigravity,
    RetroGreen,
    RetroAmber,
    Dracula,
    VT100,
}

impl Theme {
    fn colors(&self) -> ThemeColors {
        match self {
            Theme::DarkAntigravity => ThemeColors {
                bg: Color::Rgb(10, 14, 20),
                fg: Color::Rgb(0, 240, 255),
                accent: Color::Rgb(0, 240, 255),
                muted: Color::Rgb(92, 110, 140),
                border: Color::Rgb(27, 37, 54),
                header: Color::Rgb(112, 0, 255),
                quote: Color::Rgb(255, 0, 127),
                sel_bg: Color::Rgb(30, 60, 100),
                sel_fg: Color::Rgb(220, 240, 255),
            },
            Theme::RetroGreen => ThemeColors {
                bg: Color::Rgb(5, 14, 5),
                fg: Color::Rgb(51, 255, 51),
                accent: Color::Rgb(102, 255, 102),
                muted: Color::Rgb(31, 128, 31),
                border: Color::Rgb(25, 64, 25),
                header: Color::Rgb(51, 255, 51),
                quote: Color::Rgb(153, 255, 51),
                sel_bg: Color::Rgb(20, 80, 20),
                sel_fg: Color::Rgb(200, 255, 200),
            },
            Theme::RetroAmber => ThemeColors {
                bg: Color::Rgb(15, 11, 0),
                fg: Color::Rgb(255, 176, 0),
                accent: Color::Rgb(255, 208, 102),
                muted: Color::Rgb(153, 106, 0),
                border: Color::Rgb(77, 54, 0),
                header: Color::Rgb(255, 176, 0),
                quote: Color::Rgb(255, 128, 0),
                sel_bg: Color::Rgb(80, 55, 0),
                sel_fg: Color::Rgb(255, 230, 150),
            },
            Theme::Dracula => ThemeColors {
                bg: Color::Rgb(40, 42, 54),
                fg: Color::Rgb(248, 248, 242),
                accent: Color::Rgb(189, 147, 249),
                muted: Color::Rgb(98, 114, 164),
                border: Color::Rgb(98, 114, 164),
                header: Color::Rgb(255, 121, 198),
                quote: Color::Rgb(241, 250, 140),
                sel_bg: Color::Rgb(68, 71, 90),
                sel_fg: Color::Rgb(248, 248, 242),
            },
            Theme::VT100 => ThemeColors {
                bg: Color::Rgb(0, 0, 0),    // True-color black; bypasses ANSI palette detection with TERM=vt100
                fg: Color::White,
                accent: Color::Green,
                muted: Color::Gray,
                border: Color::Rgb(40, 40, 40),  // Dark grey for menubar/statusbar strip
                header: Color::Green,
                quote: Color::Yellow,
                sel_bg: Color::Blue,
                sel_fg: Color::White,
            },
        }
    }
}

struct ThemeColors {
    bg: Color,
    fg: Color,
    accent: Color,
    muted: Color,
    border: Color,
    header: Color,
    quote: Color,
    sel_bg: Color,
    sel_fg: Color,
}

fn read_dir_entries(dir: &str) -> Vec<(String, bool)> {
    let mut entries = Vec::new();
    if let Ok(path) = std::path::PathBuf::from(dir).canonicalize() {
        if path.parent().is_some() {
            entries.push(("..".to_string(), true));
        }
        if let Ok(read_dir) = std::fs::read_dir(&path) {
            let mut dirs = Vec::new();
            let mut files = Vec::new();
            for entry in read_dir.flatten() {
                if let Ok(metadata) = entry.metadata() {
                    let name = entry.file_name().to_string_lossy().to_string();
                    if metadata.is_dir() {
                        dirs.push((name, true));
                    } else {
                        files.push((name, false));
                    }
                }
            }
            dirs.sort_by(|a, b| a.0.to_lowercase().cmp(&b.0.to_lowercase()));
            files.sort_by(|a, b| a.0.to_lowercase().cmp(&b.0.to_lowercase()));
            entries.extend(dirs);
            entries.extend(files);
        }
    }
    entries
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum ActiveMenu {
    None,
    File,
    Edit,
    Format,
    Manipulation,
    View,
    Theme,
    Help,
}

#[derive(Debug, Clone, Copy)]
enum MenuAction {
    OpenFile,
    SaveFile,
    SaveAs,
    Quit,
    Heading1,
    Heading2,
    Heading3,
    Bold,
    Italic,
    Code,
    CalloutNote,
    TaskCheckbox,
    ViewWriter,
    ViewMarkdown,
    ViewSplit,
    ViewPureText,
    ThemeDarkAntigravity,
    ThemeRetroGreen,
    ThemeRetroAmber,
    ThemeDracula,
    ThemeVT100,
    Undo,
    Redo,
    Search,
    SearchReplace,
    ReplaceAll,
    Indent,
    Unindent,
    NoOp,
}

struct App {
    file_path: Option<String>,
    file_name: String,
    content: String,
    cursor_line: usize,
    cursor_col: usize,
    scroll_top: usize,
    scroll_left: usize,
    // Selection anchor: set when Shift-movement begins; None = no selection.
    selection_anchor: Option<(usize, usize)>,
    // Internal clipboard for cut/copy/paste.
    clipboard: String,
    history: Vec<String>,
    history_index: usize,
    view_mode: ViewMode,
    theme: Theme,
    active_menu: ActiveMenu,
    menu_selected: usize,
    popup: PopupState,
    dirty: bool,
    status_msg: String,
    should_quit: bool,
    snapshot_disabled: bool,
}

impl App {
    fn new(
        file_path: Option<String>,
        initial_mode: Option<ViewMode>,
        initial_theme: Option<Theme>,
    ) -> App {
        let mut content = String::new();
        let mut file_name = "untitled.md".to_string();

        if let Some(ref path) = file_path {
            let p = PathBuf::from(path);
            if let Ok(c) = fs::read_to_string(&p) {
                content = c;
                if let Some(n) = p.file_name() {
                    file_name = n.to_string_lossy().to_string();
                }
            }
        }

        if content.is_empty() {
            content = "# Welcome to ArtfulType CLI\n\n\
                       A distraction-free Markdown Writer & TUI Editor.\n\n\
                       ## Features\n\n\
                       - [x] **Writer mode** — live styled preview in terminal\n\
                       - [x] **Markdown mode** — raw syntax editor\n\
                       - [x] **Split mode** — side-by-side view\n\
                       - [x] VT100 / Pure ASCII Mode (--vt100)\n\n\
                       > [!NOTE]\n\
                       > Press Alt+F, Alt+O, Alt+V, Alt+T or use Arrow keys inside menus.\n\n\
                       ---"
                .to_string();
        }

        App {
            file_path,
            file_name,
            content: content.clone(),
            cursor_line: 0,
            cursor_col: 0,
            scroll_top: 0,
            scroll_left: 0,
            selection_anchor: None,
            clipboard: String::new(),
            history: vec![content.clone()],
            history_index: 0,
            view_mode: initial_mode.unwrap_or(ViewMode::Split),
            theme: initial_theme.unwrap_or(Theme::DarkAntigravity),
            active_menu: ActiveMenu::None,
            menu_selected: 0,
            popup: PopupState::None,
            dirty: false,
            status_msg: "Ready".to_string(),
            should_quit: false,
            snapshot_disabled: false,
        }
    }

    fn snapshot(&mut self) {
        if self.snapshot_disabled { return; }
        // truncate future redo history
        self.history.truncate(self.history_index + 1);
        self.history.push(self.content.clone());
        if self.history.len() > 11 { // Keep up to 10 changes + initial state
            self.history.remove(0);
        }
        self.history_index = self.history.len() - 1;
        self.dirty = true;
    }

    fn undo(&mut self) {
        if self.history_index > 0 {
            self.history_index -= 1;
            self.content = self.history[self.history_index].clone();
            self.dirty = true; // or check if history_index == saved_index, but for simplicity let's set it dirty
            self.ensure_cursor_valid();
            self.status_msg = "Undo".to_string();
        }
    }

    fn redo(&mut self) {
        if self.history_index + 1 < self.history.len() {
            self.history_index += 1;
            self.content = self.history[self.history_index].clone();
            self.dirty = true;
            self.ensure_cursor_valid();
            self.status_msg = "Redo".to_string();
        }
    }

    fn open_menu(&mut self, menu: ActiveMenu) {
        self.active_menu = menu;
        self.menu_selected = 0;
    }

    fn get_lines(&self) -> Vec<&str> {
        if self.content.is_empty() {
            return vec![""];
        }
        let v: Vec<&str> = self.content.split('\n').collect();
        if v.is_empty() { vec![""] } else { v }
    }

    fn ensure_cursor_valid(&mut self) {
        let lc = self.get_lines().len();
        if lc == 0 {
            self.cursor_line = 0;
            self.cursor_col = 0;
            return;
        }
        if self.cursor_line >= lc {
            self.cursor_line = lc - 1;
        }
        let lines = self.get_lines();
        let ll = lines[self.cursor_line].chars().count();
        if self.cursor_col > ll {
            self.cursor_col = ll;
        }
    }

    /// Keep scroll_top so that cursor_line is always visible inside inner_height rows.
    fn clamp_scroll(&mut self, inner_height: usize) {
        let h = inner_height.max(1);
        if self.cursor_line < self.scroll_top {
            self.scroll_top = self.cursor_line;
        } else if self.cursor_line >= self.scroll_top + h {
            self.scroll_top = self.cursor_line + 1 - h;
        }
    }

    /// Keep scroll_left so that cursor_col is always visible inside inner_width columns.
    fn clamp_scroll_x(&mut self, inner_width: usize) {
        let w = inner_width.max(1);
        // Add a small margin or just keep it tight
        if self.cursor_col < self.scroll_left {
            self.scroll_left = self.cursor_col;
        } else if self.cursor_col >= self.scroll_left + w {
            self.scroll_left = self.cursor_col + 1 - w;
        }
    }

    fn sync_content_from_lines(&mut self, lines: Vec<String>) {
        self.snapshot();
        self.content = lines.join("\n");
        self.dirty = true;
    }

    // ── Selection helpers ──────────────────────────────────────────────────

    /// If no anchor is set, drop one at the current cursor position.
    fn start_selection(&mut self) {
        if self.selection_anchor.is_none() {
            self.selection_anchor = Some((self.cursor_line, self.cursor_col));
        }
    }

    /// Clear the selection without moving the cursor.
    fn clear_selection(&mut self) {
        self.selection_anchor = None;
    }

    /// Returns (start, end) in document order where start <= end.
    fn selection_range(&self) -> Option<((usize, usize), (usize, usize))> {
        self.selection_anchor.map(|anchor| {
            let cursor = (self.cursor_line, self.cursor_col);
            if anchor <= cursor {
                (anchor, cursor)
            } else {
                (cursor, anchor)
            }
        })
    }

    /// Extract the currently selected text as a String.
    fn selected_text(&self) -> String {
        let range = match self.selection_range() {
            Some(r) => r,
            None => return String::new(),
        };
        let ((sl, sc), (el, ec)) = range;
        let lines = self.get_lines();
        if sl == el {
            let line = lines[sl];
            let chars: Vec<char> = line.chars().collect();
            let end = ec.min(chars.len());
            let start = sc.min(end);
            chars[start..end].iter().collect()
        } else {
            let mut out = String::new();
            for li in sl..=el {
                if li >= lines.len() { break; }
                let chars: Vec<char> = lines[li].chars().collect();
                if li == sl {
                    out.push_str(&chars[sc.min(chars.len())..].iter().collect::<String>());
                    out.push('\n');
                } else if li == el {
                    let end = ec.min(chars.len());
                    out.push_str(&chars[..end].iter().collect::<String>());
                } else {
                    out.push_str(&chars.iter().collect::<String>());
                    out.push('\n');
                }
            }
            out
        }
    }

    /// Delete selected text, move cursor to selection start, clear selection.
    fn delete_selection(&mut self) {
        let range = match self.selection_range() {
            Some(r) => r,
            None => return,
        };
        let ((sl, sc), (el, ec)) = range;
        let mut lines: Vec<String> = self.get_lines().iter().map(|s| s.to_string()).collect();

        if sl == el {
            // single-line deletion
            let chars: Vec<char> = lines[sl].chars().collect();
            let before: String = chars[..sc].iter().collect();
            let after: String = chars[ec.min(chars.len())..].iter().collect();
            lines[sl] = before + &after;
        } else {
            // multi-line deletion: merge sl tail and el head
            let sl_chars: Vec<char> = lines[sl].chars().collect();
            let el_chars: Vec<char> = lines[el].chars().collect();
            let before: String = sl_chars[..sc.min(sl_chars.len())].iter().collect();
            let after: String = el_chars[ec.min(el_chars.len())..].iter().collect();
            lines[sl] = before + &after;
            // remove lines sl+1 through el
            lines.drain((sl + 1)..=(el.min(lines.len() - 1)));
        }

        self.cursor_line = sl;
        self.cursor_col = sc;
        self.selection_anchor = None;
        self.sync_content_from_lines(lines);
    }

    // ── Cursor movement (no-selection variants) ────────────────────────────

    fn move_up(&mut self, inner_height: usize) {
        if self.cursor_line > 0 {
            self.cursor_line -= 1;
        }
        self.ensure_cursor_valid();
        self.clamp_scroll(inner_height);
    }

    fn move_down(&mut self, inner_height: usize) {
        let lc = self.get_lines().len();
        if self.cursor_line + 1 < lc {
            self.cursor_line += 1;
        }
        self.ensure_cursor_valid();
        self.clamp_scroll(inner_height);
    }

    fn move_left(&mut self, inner_height: usize) {
        if self.cursor_col > 0 {
            self.cursor_col -= 1;
        } else if self.cursor_line > 0 {
            self.cursor_line -= 1;
            let lines = self.get_lines();
            self.cursor_col = lines[self.cursor_line].chars().count();
        }
        self.ensure_cursor_valid();
        self.clamp_scroll(inner_height);
    }

    fn move_right(&mut self, inner_height: usize) {
        let lines = self.get_lines();
        if self.cursor_line < lines.len() {
            let ll = lines[self.cursor_line].chars().count();
            if self.cursor_col < ll {
                self.cursor_col += 1;
            } else if self.cursor_line + 1 < lines.len() {
                self.cursor_line += 1;
                self.cursor_col = 0;
            }
        }
        self.ensure_cursor_valid();
        self.clamp_scroll(inner_height);
    }

    fn move_home(&mut self) {
        self.cursor_col = 0;
    }

    fn move_end(&mut self) {
        let lines = self.get_lines();
        if self.cursor_line < lines.len() {
            self.cursor_col = lines[self.cursor_line].chars().count();
        }
    }

    fn move_to_file_start(&mut self) {
        self.cursor_line = 0;
        self.cursor_col = 0;
        self.scroll_top = 0;
    }

    fn move_to_file_end(&mut self, inner_height: usize) {
        let lc = self.get_lines().len();
        if lc > 0 {
            self.cursor_line = lc - 1;
            let lines = self.get_lines();
            self.cursor_col = lines[self.cursor_line].chars().count();
        }
        self.clamp_scroll(inner_height);
    }

    fn move_page_up(&mut self, inner_height: usize) {
        let page = inner_height.max(1);
        self.cursor_line = self.cursor_line.saturating_sub(page);
        self.ensure_cursor_valid();
        self.clamp_scroll(inner_height);
    }

    fn move_page_down(&mut self, inner_height: usize) {
        let lc = self.get_lines().len();
        let page = inner_height.max(1);
        self.cursor_line = (self.cursor_line + page).min(lc.saturating_sub(1));
        self.ensure_cursor_valid();
        self.clamp_scroll(inner_height);
    }

    // ── Editing ───────────────────────────────────────────────────────────

    fn insert_char(&mut self, c: char) {
        if self.selection_anchor.is_some() {
            self.delete_selection();
        }
        let mut lines: Vec<String> =
            self.get_lines().iter().map(|s| s.to_string()).collect();
        if self.cursor_line >= lines.len() {
            self.cursor_line = lines.len().saturating_sub(1);
        }
        let line = &mut lines[self.cursor_line];
        let bi = line
            .char_indices()
            .map(|(i, _)| i)
            .nth(self.cursor_col)
            .unwrap_or(line.len());
        line.insert(bi, c);
        self.cursor_col += 1;
        self.sync_content_from_lines(lines);
    }

    fn insert_newline(&mut self) {
        if self.selection_anchor.is_some() {
            self.delete_selection();
        }
        let mut lines: Vec<String> =
            self.get_lines().iter().map(|s| s.to_string()).collect();
        if self.cursor_line >= lines.len() {
            self.cursor_line = lines.len().saturating_sub(1);
        }
        let bi = lines[self.cursor_line]
            .char_indices()
            .map(|(i, _)| i)
            .nth(self.cursor_col)
            .unwrap_or(lines[self.cursor_line].len());
        let tail = lines[self.cursor_line][bi..].to_string();
        lines[self.cursor_line].truncate(bi);
        lines.insert(self.cursor_line + 1, tail);
        self.cursor_line += 1;
        self.cursor_col = 0;
        self.sync_content_from_lines(lines);
    }

    fn backspace(&mut self, inner_height: usize) {
        if self.selection_anchor.is_some() {
            self.delete_selection();
            self.clamp_scroll(inner_height);
            return;
        }
        let mut lines: Vec<String> =
            self.get_lines().iter().map(|s| s.to_string()).collect();
        if self.cursor_line >= lines.len() {
            self.cursor_line = lines.len().saturating_sub(1);
        }
        if self.cursor_col > 0 {
            let bi = lines[self.cursor_line]
                .char_indices()
                .map(|(i, _)| i)
                .nth(self.cursor_col - 1);
            if let Some(pos) = bi {
                lines[self.cursor_line].remove(pos);
                self.cursor_col -= 1;
            }
            self.sync_content_from_lines(lines);
        } else if self.cursor_line > 0 {
            let curr = lines.remove(self.cursor_line);
            self.cursor_line -= 1;
            self.cursor_col = lines[self.cursor_line].chars().count();
            lines[self.cursor_line].push_str(&curr);
            self.sync_content_from_lines(lines);
            self.clamp_scroll(inner_height);
        }
    }

    fn delete_forward(&mut self) {
        if self.selection_anchor.is_some() {
            self.delete_selection();
            return;
        }
        let mut lines: Vec<String> =
            self.get_lines().iter().map(|s| s.to_string()).collect();
        if self.cursor_line >= lines.len() {
            self.cursor_line = lines.len().saturating_sub(1);
        }
        let ll = lines[self.cursor_line].chars().count();
        if self.cursor_col < ll {
            let bi = lines[self.cursor_line]
                .char_indices()
                .map(|(i, _)| i)
                .nth(self.cursor_col);
            if let Some(pos) = bi {
                lines[self.cursor_line].remove(pos);
            }
            self.sync_content_from_lines(lines);
        } else if self.cursor_line + 1 < lines.len() {
            let next = lines.remove(self.cursor_line + 1);
            lines[self.cursor_line].push_str(&next);
            self.sync_content_from_lines(lines);
        }
    }

    fn copy_selection(&mut self) {
        let text = self.selected_text();
        if !text.is_empty() {
            self.clipboard = text.clone();
            copy_to_system_clipboard(&text);
            self.status_msg = "Copied".to_string();
        }
    }

    fn cut_selection(&mut self, inner_height: usize) {
        let text = self.selected_text();
        if !text.is_empty() {
            self.clipboard = text.clone();
            copy_to_system_clipboard(&text);
            self.delete_selection();
            self.clamp_scroll(inner_height);
            self.status_msg = "Cut".to_string();
        }
    }

    fn paste(&mut self) {
        if self.selection_anchor.is_some() {
            self.delete_selection();
        }
        self.snapshot();
        self.snapshot_disabled = true;
        let text = self.clipboard.clone();
        for c in text.chars() {
            if c == '\n' {
                self.insert_newline();
            } else {
                self.insert_char(c);
            }
        }
        self.snapshot_disabled = false;
        self.status_msg = "Pasted".to_string();
    }

    fn insert_str_at_cursor(&mut self, text: &str) {
        for c in text.chars() {
            if c == '\n' {
                self.insert_newline();
            } else {
                self.insert_char(c);
            }
        }
    }

    /// If text is selected, wraps it with `prefix` and `suffix` (e.g. ** and **).
    /// If nothing is selected, inserts `fallback` at the cursor.
    fn wrap_selection_or_insert(&mut self, prefix: &str, suffix: &str, fallback: &str) {
        if self.selection_anchor.is_some() {
            let selected = self.selected_text();
            if !selected.is_empty() {
                self.delete_selection();
                let wrapped = format!("{}{}{}", prefix, selected, suffix);
                self.insert_str_at_cursor(&wrapped);
                return;
            }
        }
        self.insert_str_at_cursor(fallback);
    }

    fn save_file(&mut self) {
        if let Some(ref path) = self.file_path {
            if std::fs::write(path, &self.content).is_ok() {
                self.dirty = false;
                self.status_msg = format!("Saved: {}", self.file_name);
            } else {
                self.status_msg = "Error saving file".to_string();
            }
        } else {
            let dir = std::env::current_dir().unwrap_or_else(|_| std::path::PathBuf::from(".")).to_string_lossy().to_string();
            let entries = read_dir_entries(&dir);
            self.popup = PopupState::SaveAs {
                current_dir: dir,
                entries,
                selected: 0,
                scroll: 0,
                input: String::new(),
                input_focused: false,
            };
        }
    }

    fn select_all(&mut self) {
        self.selection_anchor = Some((0, 0));
        let (len, last_col) = {
            let lines = self.get_lines();
            (lines.len(), lines.last().map_or(0, |l| l.chars().count()))
        };
        self.cursor_line = len.saturating_sub(1);
        self.cursor_col = last_col;
    }

    fn delete_to_end_of_line(&mut self) {
        self.snapshot();
        let mut lines = self.get_lines().iter().map(|s| s.to_string()).collect::<Vec<_>>();
        if self.cursor_line < lines.len() {
            let line = &lines[self.cursor_line];
            let bi = line.char_indices().map(|(i, _)| i).nth(self.cursor_col).unwrap_or(line.len());
            let deleted = line[bi..].to_string();
            if !deleted.is_empty() {
                self.clipboard = deleted.clone();
                copy_to_system_clipboard(&deleted);
            }
            lines[self.cursor_line] = line[..bi].to_string();
            self.sync_content_from_lines(lines);
        }
    }

    fn search_forward(&mut self, query: &str, inner_height: usize) {
        if query.is_empty() { return; }
        
        let target = {
            let lines = self.get_lines();
            let mut res = None;
            for (i, line) in lines.iter().enumerate().skip(self.cursor_line) {
                let start_col = if i == self.cursor_line { self.cursor_col + 1 } else { 0 };
                let bi = line.char_indices().map(|(idx, _)| idx).nth(start_col).unwrap_or(line.len());
                if bi < line.len() {
                    if let Some(pos) = line[bi..].find(query) {
                        let prefix = &line[..bi + pos];
                        res = Some((i, prefix.chars().count()));
                        break;
                    }
                }
            }
            res
        };
        
        if let Some((i, col)) = target {
            self.cursor_line = i;
            self.cursor_col = col;
            self.clamp_scroll(inner_height);
            self.start_selection();
            self.cursor_col += query.chars().count();
            self.clamp_scroll(inner_height);
            self.status_msg = format!("Found '{}'", query);
        } else {
            self.status_msg = format!("'{}' not found", query);
        }
    }

    fn replace_all(&mut self, search: &str, replace: &str) {
        if search.is_empty() { return; }
        self.snapshot();
        self.content = self.content.replace(search, replace);
        self.dirty = true;
        self.ensure_cursor_valid();
        self.status_msg = format!("Replaced all occurrences of '{}'", search);
    }

    fn indent_selection(&mut self) {
        self.snapshot();
        let mut lines = self.get_lines().iter().map(|s| s.to_string()).collect::<Vec<_>>();
        let (start_line, end_line) = match self.selection_range() {
            Some(((sl, _), (el, _))) => (sl, el),
            None => (self.cursor_line, self.cursor_line),
        };
        for i in start_line..=end_line {
            if i < lines.len() {
                lines[i].insert_str(0, "    ");
            }
        }
        self.sync_content_from_lines(lines);
        self.cursor_col += 4;
    }

    fn unindent_selection(&mut self) {
        self.snapshot();
        let mut lines = self.get_lines().iter().map(|s| s.to_string()).collect::<Vec<_>>();
        let (start_line, end_line) = match self.selection_range() {
            Some(((sl, _), (el, _))) => (sl, el),
            None => (self.cursor_line, self.cursor_line),
        };
        for i in start_line..=end_line {
            if i < lines.len() {
                if lines[i].starts_with("    ") {
                    lines[i] = lines[i][4..].to_string();
                } else if lines[i].starts_with('\t') {
                    lines[i] = lines[i][1..].to_string();
                }
            }
        }
        self.sync_content_from_lines(lines);
        self.cursor_col = self.cursor_col.saturating_sub(4);
    }

    fn execute_action(&mut self, action: MenuAction, inner_height: usize) {
        match action {
            MenuAction::OpenFile => {
                let dir = std::env::current_dir().unwrap_or_else(|_| std::path::PathBuf::from(".")).to_string_lossy().to_string();
                let entries = read_dir_entries(&dir);
                self.popup = PopupState::OpenFile {
                    current_dir: dir,
                    entries,
                    selected: 0,
                    scroll: 0,
                };
            }
            MenuAction::SaveFile => self.save_file(),
            MenuAction::SaveAs => {
                let dir = std::env::current_dir().unwrap_or_else(|_| std::path::PathBuf::from(".")).to_string_lossy().to_string();
                let entries = read_dir_entries(&dir);
                self.popup = PopupState::SaveAs {
                    current_dir: dir,
                    entries,
                    selected: 0,
                    scroll: 0,
                    input: String::new(),
                    input_focused: false,
                };
            }
            MenuAction::Quit => {
                if self.dirty {
                    self.popup = PopupState::QuitConfirm;
                } else {
                    self.should_quit = true;
                }
            }
            MenuAction::Heading1 => self.insert_str_at_cursor("# "),
            MenuAction::Heading2 => self.insert_str_at_cursor("## "),
            MenuAction::Heading3 => self.insert_str_at_cursor("### "),
            MenuAction::Bold => self.wrap_selection_or_insert("**", "**", "**bold**"),
            MenuAction::Italic => self.wrap_selection_or_insert("*", "*", "*italic*"),
            MenuAction::Code => self.wrap_selection_or_insert("`", "`", "`code`"),
            MenuAction::CalloutNote => self.insert_str_at_cursor("> [!NOTE]\n> "),
            MenuAction::TaskCheckbox => self.insert_str_at_cursor("- [ ] "),
            MenuAction::ViewWriter => {
                self.view_mode = ViewMode::Writer;
                if self.active_menu == ActiveMenu::Manipulation { self.active_menu = ActiveMenu::Format; }
            }
            MenuAction::ViewMarkdown => {
                self.view_mode = ViewMode::Markdown;
                if self.active_menu == ActiveMenu::Manipulation { self.active_menu = ActiveMenu::Format; }
            }
            MenuAction::ViewSplit => {
                self.view_mode = ViewMode::Split;
                if self.active_menu == ActiveMenu::Manipulation { self.active_menu = ActiveMenu::Format; }
            }
            MenuAction::ViewPureText => {
                self.view_mode = ViewMode::PureText;
                if self.active_menu == ActiveMenu::Format { self.active_menu = ActiveMenu::Manipulation; }
            }
            MenuAction::ThemeDarkAntigravity => self.theme = Theme::DarkAntigravity,
            MenuAction::ThemeRetroGreen => self.theme = Theme::RetroGreen,
            MenuAction::ThemeRetroAmber => self.theme = Theme::RetroAmber,
            MenuAction::ThemeDracula => self.theme = Theme::Dracula,
            MenuAction::ThemeVT100 => self.theme = Theme::VT100,
            MenuAction::Undo => self.undo(),
            MenuAction::Redo => self.redo(),
            MenuAction::Search => {
                self.popup = PopupState::Search { input: String::new() };
            }
            MenuAction::SearchReplace => {
                self.popup = PopupState::SearchReplace { search: String::new(), replace: String::new(), step: 0 };
            }
            MenuAction::ReplaceAll => {
                self.popup = PopupState::SearchReplace { search: String::new(), replace: String::new(), step: 0 };
            }
            MenuAction::Indent => self.indent_selection(),
            MenuAction::Unindent => self.unindent_selection(),
            MenuAction::NoOp => {}
        }
        self.clamp_scroll(inner_height);
        // If a popup was opened, don't close active menu until popup is done,
        // or close it now? Let's close the dropdown.
        if self.popup == PopupState::None {
            self.active_menu = ActiveMenu::None;
        } else {
            self.active_menu = ActiveMenu::None;
        }
    }
}

fn get_menu_items(menu: ActiveMenu) -> Vec<(&'static str, MenuAction)> {
    match menu {
        ActiveMenu::File => vec![
            ("[O] Open File...  (Ctrl+O)", MenuAction::OpenFile),
            ("[S] Save File     (Ctrl+S)", MenuAction::SaveFile),
            ("[A] Save As...", MenuAction::SaveAs),
            ("[Q] Quit          (Ctrl+Q)", MenuAction::Quit),
        ],
        ActiveMenu::Edit => vec![
            ("Undo          (Ctrl+Alt+Z)", MenuAction::Undo),
            ("Redo          (Ctrl+Alt+Y)", MenuAction::Redo),
            ("Copy          (Ctrl+Alt+C)", MenuAction::NoOp),
            ("Cut           (Ctrl+Alt+X)", MenuAction::NoOp),
            ("Paste         (Ctrl+Alt+V)", MenuAction::NoOp),
        ],
        ActiveMenu::Format => vec![
            ("H1 Heading 1  (Ctrl+1)", MenuAction::Heading1),
            ("H2 Heading 2  (Ctrl+2)", MenuAction::Heading2),
            ("H3 Heading 3  (Ctrl+3)", MenuAction::Heading3),
            ("Bold      (Ctrl+Alt+B)", MenuAction::Bold),
            ("Italic    (Ctrl+Alt+I)", MenuAction::Italic),
            ("Code      (Ctrl+Alt+K)", MenuAction::Code),
            ("Callout Note", MenuAction::CalloutNote),
            ("Task Checkbox", MenuAction::TaskCheckbox),
        ],
        ActiveMenu::Manipulation => vec![
            ("Search", MenuAction::Search),
            ("Search and Replace", MenuAction::SearchReplace),
            ("Replace All", MenuAction::ReplaceAll),
            ("Indent Selection", MenuAction::Indent),
            ("Unindent Selection", MenuAction::Unindent),
        ],
        ActiveMenu::View => vec![
            ("Writer Mode   (F2)", MenuAction::ViewWriter),
            ("Markdown Mode (F3)", MenuAction::ViewMarkdown),
            ("Split Mode    (F4)", MenuAction::ViewSplit),
            ("Pure Text Mode (Ctrl+F2)", MenuAction::ViewPureText),
        ],
        ActiveMenu::Theme => vec![
            ("Dark Antigravity", MenuAction::ThemeDarkAntigravity),
            ("Retro Green CRT", MenuAction::ThemeRetroGreen),
            ("Retro Amber CRT", MenuAction::ThemeRetroAmber),
            ("Dracula Standard", MenuAction::ThemeDracula),
            ("VT100 Pure ASCII", MenuAction::ThemeVT100),
        ],
        ActiveMenu::Help => vec![
            ("Shift+Arrows: Select text", MenuAction::NoOp),
            ("Ctrl+Alt+C/X/V: Copy/Cut/Paste", MenuAction::NoOp),
            ("Alt+F/E/O/V/T: Open Menus", MenuAction::NoOp),
            ("F2:Writer  F3:MD  F4:Split", MenuAction::NoOp),
            ("Ctrl+S:Save  Ctrl+Q:Quit", MenuAction::NoOp),
        ],
        ActiveMenu::None => vec![],
    }
}

fn next_menu(m: ActiveMenu, is_pure_text: bool) -> ActiveMenu {
    match m {
        ActiveMenu::File => ActiveMenu::Edit,
        ActiveMenu::Edit => if is_pure_text { ActiveMenu::Manipulation } else { ActiveMenu::Format },
        ActiveMenu::Format => ActiveMenu::View,
        ActiveMenu::Manipulation => ActiveMenu::View,
        ActiveMenu::View => ActiveMenu::Theme,
        ActiveMenu::Theme => ActiveMenu::Help,
        _ => ActiveMenu::File,
    }
}

fn prev_menu(m: ActiveMenu, is_pure_text: bool) -> ActiveMenu {
    match m {
        ActiveMenu::Edit => ActiveMenu::File,
        ActiveMenu::Format => ActiveMenu::Edit,
        ActiveMenu::Manipulation => ActiveMenu::Edit,
        ActiveMenu::View => if is_pure_text { ActiveMenu::Manipulation } else { ActiveMenu::Format },
        ActiveMenu::Theme => ActiveMenu::View,
        ActiveMenu::Help => ActiveMenu::Theme,
        _ => ActiveMenu::Help,
    }
}

fn copy_to_system_clipboard(text: &str) {
    use base64::prelude::*;
    use std::io::Write;
    let b64 = BASE64_STANDARD.encode(text);
    print!("\x1B]52;c;{}\x07", b64);
    let _ = std::io::stdout().flush();
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    let mut initial_file = None;
    let mut initial_mode = None;
    let mut initial_theme = None;

    let mut i = 1;
    while i < args.len() {
        let arg = &args[i];
        if arg == "-h" || arg == "--help" {
            println!("ArtfulType Terminal / TUI v0.26.1");
            println!("Usage: artfultype-cli [OPTIONS] [FILE]\n");
            println!("  --mode writer|markdown|split");
            println!("  --theme dark-antigravity|retro-green|retro-amber|dracula|vt100");
            println!("  --vt100, --ascii   Force VT100/ASCII mode");
            println!("  -h, --help         Help");
            println!("  -v, --version      Version");
            return Ok(());
        } else if arg == "-v" || arg == "--version" {
            println!("ArtfulType Terminal / TUI v0.26.1");
            return Ok(());
        } else if arg == "--vt100" || arg == "--ascii" {
            initial_theme = Some(Theme::VT100);
        } else if (arg == "--mode") && i + 1 < args.len() {
            match args[i + 1].to_lowercase().as_str() {
                "writer" => initial_mode = Some(ViewMode::Writer),
                "markdown" => initial_mode = Some(ViewMode::Markdown),
                "split" => initial_mode = Some(ViewMode::Split),
                _ => {}
            }
            i += 1;
        } else if let Some(m) = arg.strip_prefix("--mode=") {
            match m.to_lowercase().as_str() {
                "writer" => initial_mode = Some(ViewMode::Writer),
                "markdown" => initial_mode = Some(ViewMode::Markdown),
                "split" => initial_mode = Some(ViewMode::Split),
                _ => {}
            }
        } else if (arg == "--theme") && i + 1 < args.len() {
            match args[i + 1].to_lowercase().as_str() {
                "retro-green" => initial_theme = Some(Theme::RetroGreen),
                "retro-amber" => initial_theme = Some(Theme::RetroAmber),
                "dracula" => initial_theme = Some(Theme::Dracula),
                "vt100" | "ascii" => initial_theme = Some(Theme::VT100),
                _ => initial_theme = Some(Theme::DarkAntigravity),
            }
            i += 1;
        } else if let Some(t) = arg.strip_prefix("--theme=") {
            match t.to_lowercase().as_str() {
                "retro-green" => initial_theme = Some(Theme::RetroGreen),
                "retro-amber" => initial_theme = Some(Theme::RetroAmber),
                "dracula" => initial_theme = Some(Theme::Dracula),
                "vt100" | "ascii" => initial_theme = Some(Theme::VT100),
                _ => initial_theme = Some(Theme::DarkAntigravity),
            }
        } else if !arg.starts_with('-') && initial_file.is_none() {
            initial_file = Some(arg.clone());
        }
        i += 1;
    }

    enable_raw_mode()?;
    let mut stdout = stdout();
    execute!(stdout, EnterAlternateScreen)?;
    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;

    let mut app = App::new(initial_file, initial_mode, initial_theme);
    let res = run_app(&mut terminal, &mut app);

    disable_raw_mode()?;
    execute!(terminal.backend_mut(), LeaveAlternateScreen)?;
    terminal.show_cursor()?;

    if let Err(err) = res {
        println!("Error: {err:?}");
    }
    Ok(())
}

fn run_app<B: ratatui::backend::Backend>(
    terminal: &mut Terminal<B>,
    app: &mut App,
) -> io::Result<()> {
    loop {
        terminal.draw(|f| ui(f, app))?;

        if event::poll(std::time::Duration::from_millis(50))? {
            if let Event::Key(key) = event::read()? {
                // Compute inner height: full height minus menubar(1) + statusbar(1) + borders(2).
                let ts = terminal.size()?;
                let inner_h = ts.height.saturating_sub(4) as usize;
                let inner_w = match app.view_mode {
                    ViewMode::Split => (ts.width / 2).saturating_sub(8) as usize,
                    ViewMode::Writer => ts.width.saturating_sub(2) as usize,
                    _ => ts.width.saturating_sub(8) as usize,
                };

                let shift = key.modifiers.contains(KeyModifiers::SHIFT);
                let ctrl  = key.modifiers.contains(KeyModifiers::CONTROL);
                let alt   = key.modifiers.contains(KeyModifiers::ALT);

                // ── Popup handling ───────────────────────────────────────
                if app.popup != PopupState::None {
                    match app.popup.clone() {
                        PopupState::QuitConfirm => {
                            match key.code {
                                KeyCode::Char('y') | KeyCode::Char('Y') => {
                                    app.save_file();
                                    app.should_quit = true;
                                    app.popup = PopupState::None;
                                }
                                KeyCode::Char('n') | KeyCode::Char('N') => {
                                    app.should_quit = true;
                                    app.popup = PopupState::None;
                                }
                                KeyCode::Esc => {
                                    app.popup = PopupState::None;
                                    app.status_msg = "Quit cancelled".to_string();
                                }
                                _ => {}
                            }
                        }
                        PopupState::SaveAs { mut current_dir, mut entries, mut selected, mut scroll, mut input, mut input_focused } => {
                            match key.code {
                                KeyCode::Tab => {
                                    input_focused = !input_focused;
                                    app.popup = PopupState::SaveAs { current_dir, entries, selected, scroll, input, input_focused };
                                }
                                KeyCode::Up => {
                                    if !input_focused {
                                        if selected > 0 {
                                            selected -= 1;
                                            if selected < scroll {
                                                scroll = selected;
                                            }
                                            if !entries.is_empty() && selected < entries.len() {
                                                let (name, is_dir) = &entries[selected];
                                                if !*is_dir {
                                                    input = name.clone();
                                                }
                                            }
                                        }
                                    }
                                    app.popup = PopupState::SaveAs { current_dir, entries, selected, scroll, input, input_focused };
                                }
                                KeyCode::Down => {
                                    if !input_focused {
                                        if !entries.is_empty() && selected < entries.len() - 1 {
                                            selected += 1;
                                            if selected >= scroll + 15 {
                                                scroll = selected.saturating_sub(14);
                                            }
                                            if !entries.is_empty() && selected < entries.len() {
                                                let (name, is_dir) = &entries[selected];
                                                if !*is_dir {
                                                    input = name.clone();
                                                }
                                            }
                                        }
                                    }
                                    app.popup = PopupState::SaveAs { current_dir, entries, selected, scroll, input, input_focused };
                                }
                                KeyCode::Enter => {
                                    if !input_focused && input.is_empty() {
                                        if !entries.is_empty() && selected < entries.len() {
                                            let (name, is_dir) = &entries[selected];
                                            if *is_dir {
                                                let new_path = if name == ".." {
                                                    std::path::PathBuf::from(&current_dir).parent().unwrap_or_else(|| std::path::Path::new(&current_dir)).to_path_buf()
                                                } else {
                                                    std::path::PathBuf::from(&current_dir).join(name)
                                                };
                                                if let Ok(canon) = new_path.canonicalize() {
                                                    current_dir = canon.to_string_lossy().to_string();
                                                    entries = read_dir_entries(&current_dir);
                                                    selected = 0;
                                                    scroll = 0;
                                                    app.popup = PopupState::SaveAs { current_dir, entries, selected, scroll, input, input_focused };
                                                }
                                            }
                                        }
                                    } else {
                                        if !input.is_empty() {
                                            let file_path = std::path::PathBuf::from(&current_dir).join(&input);
                                            app.file_path = Some(file_path.to_string_lossy().to_string());
                                            app.file_name = input.clone();
                                            app.save_file();
                                            app.popup = PopupState::None;
                                        }
                                    }
                                }
                                KeyCode::Esc => app.popup = PopupState::None,
                                KeyCode::Char(c) => {
                                    if input_focused {
                                        input.push(c);
                                        app.popup = PopupState::SaveAs { current_dir, entries, selected, scroll, input, input_focused };
                                    }
                                }
                                KeyCode::Backspace => {
                                    if input_focused {
                                        input.pop();
                                        app.popup = PopupState::SaveAs { current_dir, entries, selected, scroll, input, input_focused };
                                    }
                                }
                                _ => {}
                            }
                        }
                        PopupState::OpenFile { mut current_dir, mut entries, mut selected, mut scroll } => {
                            match key.code {
                                KeyCode::Up => {
                                    if selected > 0 {
                                        selected -= 1;
                                        if selected < scroll {
                                            scroll = selected;
                                        }
                                    }
                                    app.popup = PopupState::OpenFile { current_dir, entries, selected, scroll };
                                }
                                KeyCode::Down => {
                                    if !entries.is_empty() && selected < entries.len() - 1 {
                                        selected += 1;
                                        // Assume height of 15 items in popup
                                        if selected >= scroll + 15 {
                                            scroll = selected.saturating_sub(14);
                                        }
                                    }
                                    app.popup = PopupState::OpenFile { current_dir, entries, selected, scroll };
                                }
                                KeyCode::Enter => {
                                    if !entries.is_empty() && selected < entries.len() {
                                        let (name, is_dir) = &entries[selected];
                                        if *is_dir {
                                            let new_path = if name == ".." {
                                                std::path::PathBuf::from(&current_dir).parent().unwrap_or_else(|| std::path::Path::new(&current_dir)).to_path_buf()
                                            } else {
                                                std::path::PathBuf::from(&current_dir).join(name)
                                            };
                                            if let Ok(canon) = new_path.canonicalize() {
                                                current_dir = canon.to_string_lossy().to_string();
                                                entries = read_dir_entries(&current_dir);
                                                selected = 0;
                                                scroll = 0;
                                                app.popup = PopupState::OpenFile { current_dir, entries, selected, scroll };
                                            }
                                        } else {
                                            let file_path = std::path::PathBuf::from(&current_dir).join(name);
                                            if let Ok(content) = std::fs::read_to_string(&file_path) {
                                                app.content = content;
                                                app.file_path = Some(file_path.to_string_lossy().to_string());
                                                app.file_name = name.clone();
                                                app.cursor_line = 0;
                                                app.cursor_col = 0;
                                                app.scroll_top = 0;
                                                app.scroll_left = 0;
                                                app.dirty = false;
                                                app.history.clear();
                                                app.history_index = 0;
                                                app.snapshot();
                                                app.clear_selection();
                                                app.status_msg = format!("Opened file {}", app.file_name);
                                            } else {
                                                app.status_msg = format!("Failed to read file: {}", file_path.to_string_lossy());
                                            }
                                            app.popup = PopupState::None;
                                        }
                                    }
                                }
                                KeyCode::Esc => app.popup = PopupState::None,
                                _ => {}
                            }
                        }
                        PopupState::Search { mut input } => {
                            match key.code {
                                KeyCode::Enter => {
                                    let query = input.clone();
                                    app.popup = PopupState::None;
                                    app.search_forward(&query, inner_h);
                                }
                                KeyCode::Esc => app.popup = PopupState::None,
                                KeyCode::Char(c) => {
                                    input.push(c);
                                    app.popup = PopupState::Search { input };
                                }
                                KeyCode::Backspace => {
                                    input.pop();
                                    app.popup = PopupState::Search { input };
                                }
                                _ => {}
                            }
                        }
                        PopupState::SearchReplace { mut search, mut replace, step } => {
                            match key.code {
                                KeyCode::Enter => {
                                    if step == 0 {
                                        app.popup = PopupState::SearchReplace { search, replace, step: 1 };
                                    } else {
                                        let s = search.clone();
                                        let r = replace.clone();
                                        app.popup = PopupState::None;
                                        app.replace_all(&s, &r);
                                    }
                                }
                                KeyCode::Esc => app.popup = PopupState::None,
                                KeyCode::Char(c) => {
                                    if step == 0 {
                                        search.push(c);
                                    } else {
                                        replace.push(c);
                                    }
                                    app.popup = PopupState::SearchReplace { search, replace, step };
                                }
                                KeyCode::Backspace => {
                                    if step == 0 {
                                        search.pop();
                                    } else {
                                        replace.pop();
                                    }
                                    app.popup = PopupState::SearchReplace { search, replace, step };
                                }
                                _ => {}
                            }
                        }
                        _ => {}
                    }
                    continue;
                }

                // ── Alt key: open menus ──────────────────────────────────
                if alt && !ctrl {
                    match key.code {
                        KeyCode::Char('f') | KeyCode::Char('F') => app.open_menu(ActiveMenu::File),
                        KeyCode::Char('e') | KeyCode::Char('E') => app.open_menu(ActiveMenu::Edit),
                        KeyCode::Char('o') | KeyCode::Char('O') => {
                            if app.view_mode == ViewMode::PureText {
                                app.open_menu(ActiveMenu::Manipulation)
                            } else {
                                app.open_menu(ActiveMenu::Format)
                            }
                        }
                        KeyCode::Char('v') | KeyCode::Char('V') => app.open_menu(ActiveMenu::View),
                        KeyCode::Char('t') | KeyCode::Char('T') => app.open_menu(ActiveMenu::Theme),
                        KeyCode::Char('h') | KeyCode::Char('H') => app.open_menu(ActiveMenu::Help),
                        KeyCode::Up => app.move_to_file_start(),
                        KeyCode::Down => app.move_to_file_end(inner_h),
                        _ => {}
                    }
                    continue;
                }

                // ── Dropdown menu navigation ─────────────────────────────
                if app.active_menu != ActiveMenu::None {
                    let items = get_menu_items(app.active_menu);
                    match key.code {
                        KeyCode::Esc => app.active_menu = ActiveMenu::None,
                        KeyCode::Up => {
                            if !items.is_empty() {
                                app.menu_selected = app.menu_selected.saturating_sub(1);
                            }
                        }
                        KeyCode::Down => {
                            if !items.is_empty() {
                                app.menu_selected =
                                    (app.menu_selected + 1).min(items.len() - 1);
                            }
                        }
                        KeyCode::Left => app.open_menu(prev_menu(app.active_menu, app.view_mode == ViewMode::PureText)),
                        KeyCode::Right => app.open_menu(next_menu(app.active_menu, app.view_mode == ViewMode::PureText)),
                        KeyCode::Enter | KeyCode::Char(' ') => {
                            if app.menu_selected < items.len() {
                                let action = items[app.menu_selected].1;
                                app.execute_action(action, inner_h);
                            } else {
                                app.active_menu = ActiveMenu::None;
                            }
                        }
                        _ => {}
                    }
                    continue;
                }

                // ── Ctrl shortcuts ───────────────────────────────────────
                if ctrl {
                    if alt {
                        match key.code {
                            KeyCode::Char('z') => app.undo(),
                            KeyCode::Char('y') => app.redo(),
                            KeyCode::Char('c') => app.copy_selection(),
                            KeyCode::Char('x') => app.cut_selection(inner_h),
                            KeyCode::Char('v') => app.paste(),
                            KeyCode::Char('b') => app.wrap_selection_or_insert("**", "**", "**bold**"),
                            KeyCode::Char('i') => app.wrap_selection_or_insert("*", "*", "*italic*"),
                            KeyCode::Char('k') => app.wrap_selection_or_insert("`", "`", "`code`"),
                            KeyCode::Char('a') => app.select_all(),
                            _ => {}
                        }
                    } else {
                        match key.code {
                            KeyCode::Char('a') => app.select_all(),
                            KeyCode::Char('k') => app.delete_to_end_of_line(),
                            KeyCode::Char('q') => {
                                if app.dirty {
                                    app.popup = PopupState::QuitConfirm;
                                } else {
                                    app.should_quit = true;
                                }
                            }
                            KeyCode::Char('s') => app.save_file(),
                            KeyCode::Char('o') => app.execute_action(MenuAction::OpenFile, inner_h),
                            KeyCode::Char('1') => app.insert_str_at_cursor("# "),
                            KeyCode::Char('2') => app.insert_str_at_cursor("## "),
                            KeyCode::Char('3') => app.insert_str_at_cursor("### "),
                            KeyCode::Home | KeyCode::Up => app.move_to_file_start(),
                            KeyCode::End | KeyCode::Down => app.move_to_file_end(inner_h),
                            KeyCode::F(2) => app.view_mode = ViewMode::PureText,
                            _ => {}
                        }
                    }
                    app.clamp_scroll(inner_h);
                    app.clamp_scroll_x(inner_w);
                    continue;
                }

                // ── Shift + movement: extend selection ───────────────────
                if shift {
                    match key.code {
                        KeyCode::Up => {
                            app.start_selection();
                            app.move_up(inner_h);
                        }
                        KeyCode::Down => {
                            app.start_selection();
                            app.move_down(inner_h);
                        }
                        KeyCode::Left => {
                            app.start_selection();
                            app.move_left(inner_h);
                        }
                        KeyCode::Right => {
                            app.start_selection();
                            app.move_right(inner_h);
                        }
                        KeyCode::Home => {
                            app.start_selection();
                            app.move_home();
                        }
                        KeyCode::End => {
                            app.start_selection();
                            app.move_end();
                        }
                        KeyCode::PageUp => {
                            app.start_selection();
                            app.move_page_up(inner_h);
                        }
                        KeyCode::PageDown => {
                            app.start_selection();
                            app.move_page_down(inner_h);
                        }
                        // Shift+Char: just insert the uppercase character normally
                        KeyCode::Char(c) => {
                            app.clear_selection();
                            app.insert_char(c);
                        }
                        _ => {}
                    }
                    app.clamp_scroll(inner_h);
                    app.clamp_scroll_x(inner_w);
                    continue;
                }

                // ── Regular (unmodified) keys ────────────────────────────
                match key.code {
                    KeyCode::F(2) => app.view_mode = ViewMode::Writer,
                    KeyCode::F(3) => app.view_mode = ViewMode::Markdown,
                    KeyCode::F(4) => app.view_mode = ViewMode::Split,
                    // Movement keys clear selection
                    KeyCode::Up => { app.clear_selection(); app.move_up(inner_h); }
                    KeyCode::Down => { app.clear_selection(); app.move_down(inner_h); }
                    KeyCode::Left => { app.clear_selection(); app.move_left(inner_h); }
                    KeyCode::Right => { app.clear_selection(); app.move_right(inner_h); }
                    KeyCode::Home => { app.clear_selection(); app.move_home(); }
                    KeyCode::End => { app.clear_selection(); app.move_end(); }
                    KeyCode::PageUp => { app.clear_selection(); app.move_page_up(inner_h); }
                    KeyCode::PageDown => { app.clear_selection(); app.move_page_down(inner_h); }
                    // Editing
                    KeyCode::Char(c) => app.insert_char(c),
                    KeyCode::Enter => app.insert_newline(),
                    KeyCode::Backspace => app.backspace(inner_h),
                    KeyCode::Delete => app.delete_forward(),
                    KeyCode::Esc => app.clear_selection(),
                    _ => {}
                }
                // Sync scroll after every keypress
                app.clamp_scroll(inner_h);
                app.clamp_scroll_x(inner_w);
            }
        }

        if app.should_quit {
            return Ok(());
        }
    }
}

fn ui(f: &mut ratatui::Frame, app: &App) {
    let colors = app.theme.colors();
    let size = f.area();

    let chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(1), // menubar
            Constraint::Min(1),    // editor area
            Constraint::Length(1), // statusbar
        ])
        .split(size);

    // ── Menubar ──
    let menu_spans = vec![
        Span::styled(
            " [File] ",
            if app.active_menu == ActiveMenu::File {
                Style::default().bg(colors.accent).fg(colors.bg).add_modifier(Modifier::BOLD)
            } else {
                Style::default().fg(colors.fg)
            },
        ),
        Span::styled(
            " [Edit] ",
            if app.active_menu == ActiveMenu::Edit {
                Style::default().bg(colors.accent).fg(colors.bg).add_modifier(Modifier::BOLD)
            } else {
                Style::default().fg(colors.fg)
            },
        ),
        Span::styled(
            if app.view_mode == ViewMode::PureText {
                " [Manipulation] "
            } else {
                " [Format] "
            },
            if app.active_menu == ActiveMenu::Format || app.active_menu == ActiveMenu::Manipulation {
                Style::default().bg(colors.accent).fg(colors.bg).add_modifier(Modifier::BOLD)
            } else {
                Style::default().fg(colors.fg)
            },
        ),
        Span::styled(
            " [View] ",
            if app.active_menu == ActiveMenu::View {
                Style::default().bg(colors.accent).fg(colors.bg).add_modifier(Modifier::BOLD)
            } else {
                Style::default().fg(colors.fg)
            },
        ),
        Span::styled(
            " [Theme] ",
            if app.active_menu == ActiveMenu::Theme {
                Style::default().bg(colors.accent).fg(colors.bg).add_modifier(Modifier::BOLD)
            } else {
                Style::default().fg(colors.fg)
            },
        ),
        Span::styled(
            " [Help] ",
            if app.active_menu == ActiveMenu::Help {
                Style::default().bg(colors.accent).fg(colors.bg).add_modifier(Modifier::BOLD)
            } else {
                Style::default().fg(colors.fg)
            },
        ),
        Span::styled("  | ArtfulType CLI v0.26.1", Style::default().fg(colors.muted)),
    ];
    f.render_widget(
        Paragraph::new(Line::from(menu_spans)).style(Style::default().bg(colors.border)),
        chunks[0],
    );

    // ── Editor area ──
    let editor_rect = chunks[1];
    let inner_x = editor_rect.x + 1;
    let inner_y = editor_rect.y + 1;
    let inner_h = editor_rect.height.saturating_sub(2) as usize;
    let cursor_row_in_view = app.cursor_line.saturating_sub(app.scroll_top);

    match app.view_mode {
        ViewMode::Markdown | ViewMode::PureText => {
            render_markdown_editor(f, editor_rect, app, &colors);
            if app.active_menu == ActiveMenu::None && cursor_row_in_view < inner_h {
                let col = app.cursor_col.saturating_sub(app.scroll_left) as u16;
                let cx = inner_x + 6 + col;
                let cy = inner_y + cursor_row_in_view as u16;
                f.set_cursor_position((cx, cy));
            }
        }
        ViewMode::Writer => {
            render_writer_view(f, editor_rect, app, &colors);
            if app.active_menu == ActiveMenu::None && cursor_row_in_view < inner_h {
                let col = app.cursor_col.saturating_sub(app.scroll_left) as u16;
                let cx = inner_x + col;
                let cy = inner_y + cursor_row_in_view as u16;
                f.set_cursor_position((cx, cy));
            }
        }
        ViewMode::Split => {
            let split = Layout::default()
                .direction(Direction::Horizontal)
                .constraints([Constraint::Percentage(50), Constraint::Percentage(50)])
                .split(editor_rect);
            render_markdown_editor(f, split[0], app, &colors);
            render_writer_view(f, split[1], app, &colors);
            if app.active_menu == ActiveMenu::None && cursor_row_in_view < inner_h {
                let col = app.cursor_col.saturating_sub(app.scroll_left) as u16;
                let left_inner_x = split[0].x + 1;
                let left_inner_y = split[0].y + 1;
                let cx = left_inner_x + 6 + col;
                let cy = left_inner_y + cursor_row_in_view as u16;
                f.set_cursor_position((cx, cy));
            }
        }
    }

    // ── Statusbar ──
    let words = app.content.split_whitespace().count();
    let total_lines = app.get_lines().len();
    let mode_str = match app.view_mode {
        ViewMode::Writer => "Writer",
        ViewMode::Markdown => "Markdown",
        ViewMode::Split => "Split",
        ViewMode::PureText => "PureText",
    };
    let dirty = if app.dirty { " *" } else { "" };
    let theme_tag = if app.theme == Theme::VT100 { " (VT100)" } else { "" };
    let sel_tag = if app.selection_anchor.is_some() {
        let text = app.selected_text();
        format!(" | SEL:{} chars", text.len())
    } else {
        String::new()
    };
    let status = Line::from(vec![
        Span::styled(format!(" {} ", app.status_msg), Style::default().fg(colors.fg)),
        Span::styled(
            format!(
                " | {mode_str}{theme_tag} | {} {dirty} | L:{}/{} C:{}{sel_tag} | W:{words} ",
                app.file_name,
                app.cursor_line + 1,
                total_lines,
                app.cursor_col + 1
            ),
            Style::default().fg(colors.muted),
        ),
    ]);
    f.render_widget(
        Paragraph::new(status).style(Style::default().bg(colors.border)),
        chunks[2],
    );

    // ── Dropdown menus ──
    if app.active_menu != ActiveMenu::None {
        render_dropdown_popup(f, app, &colors);
    }

    // ── Popups ──
    if app.popup != PopupState::None {
        render_popup(f, app, &colors);
    }
}

fn render_popup(f: &mut ratatui::Frame, app: &App, colors: &ThemeColors) {
    let size = f.area();
    let mut height = 5;
    let mut width = 40;
    if let PopupState::OpenFile { .. } | PopupState::SaveAs { .. } = &app.popup {
        height = 20;
        width = 60;
    }

    let area = Rect::new(
        (size.width.saturating_sub(width)) / 2,
        (size.height.saturating_sub(height)) / 2,
        width.min(size.width),
        height.min(size.height),
    );
    f.render_widget(Clear, area);

    let (title, content_lines) = match &app.popup {
        PopupState::QuitConfirm => {
            (
                " Quit ",
                vec![
                    "File has unsaved changes.".to_string(),
                    "Save before quitting? [Y/N/Esc]".to_string(),
                ]
            )
        }
        PopupState::SaveAs { current_dir, entries, selected, scroll, input, input_focused } => {
            let mut lines = vec![format!("Dir: {}", current_dir), "".to_string()];
            let display_count = height.saturating_sub(7) as usize; // account for borders (2), headers/footers (5)
            for (i, (name, is_dir)) in entries.iter().skip(*scroll).take(display_count).enumerate() {
                let actual_idx = i + scroll;
                let cursor = if !*input_focused && actual_idx == *selected { ">" } else { " " };
                let icon = if *is_dir { "📁" } else { "📄" };
                lines.push(format!("{} {} {}", cursor, icon, name));
            }
            if entries.len() > scroll + display_count {
                lines.push("   ...".to_string());
            }
            lines.push("".to_string());
            let input_cursor = if *input_focused { "_" } else { "" };
            lines.push(format!("Save as [Tab]: {}{}", input, input_cursor));
            (
                " Save As ",
                lines
            )
        }
        PopupState::OpenFile { current_dir, entries, selected, scroll } => {
            let mut lines = vec![format!("Dir: {}", current_dir), "".to_string()];
            let display_count = height.saturating_sub(5) as usize; // account for borders (2), headers/footers (3)
            for (i, (name, is_dir)) in entries.iter().skip(*scroll).take(display_count).enumerate() {
                let actual_idx = i + scroll;
                let cursor = if actual_idx == *selected { ">" } else { " " };
                let icon = if *is_dir { "📁" } else { "📄" };
                lines.push(format!("{} {} {}", cursor, icon, name));
            }
            if entries.len() > scroll + display_count {
                lines.push("   ...".to_string());
            }
            (
                " Open File ",
                lines
            )
        }
        PopupState::Search { input } => {
            (
                " Search ",
                vec![
                    "Search for:".to_string(),
                    format!("> {}", input),
                ]
            )
        }
        PopupState::SearchReplace { search, replace, step } => {
            if *step == 0 {
                (
                    " Search & Replace ",
                    vec![
                        "Search for:".to_string(),
                        format!("> {}", search),
                    ]
                )
            } else {
                (
                    " Search & Replace ",
                    vec![
                        format!("Replace '{}' with:", search),
                        format!("> {}", replace),
                    ]
                )
            }
        }
        PopupState::None => ("", vec![]),
    };

    let p = Paragraph::new(
        content_lines.into_iter()
            .map(|l| Line::from(Span::styled(l, Style::default().fg(colors.fg))))
            .collect::<Vec<_>>()
    )
    .block(
        Block::default()
            .borders(Borders::ALL)
            .title(title)
            .border_style(Style::default().fg(colors.accent))
    )
    .style(Style::default().bg(colors.bg));

    f.render_widget(p, area);
}

/// Styled writer preview — one rendered line per source line, no word-wrap.
fn render_writer_view(f: &mut ratatui::Frame, area: Rect, app: &App, colors: &ThemeColors) {
    let is_vt100 = app.theme == Theme::VT100;

    let check   = if is_vt100 { "[x] " } else { "☑ " };
    let uncheck = if is_vt100 { "[ ] " } else { "☐ " };
    let bullet  = if is_vt100 { "* "  } else { "• " };
    let ct = if is_vt100 { "+-- " } else { "┌─ " };
    let cb = if is_vt100 { "| "  } else { "│ " };
    let ce = if is_vt100 { " --------" } else { " ────────" };
    let hr = if is_vt100 { "----------------------------" } else { "────────────────────────────" };

    let sel = app.selection_range();
    let (sel_bg, sel_fg) = (colors.sel_bg, colors.sel_fg);

    let rendered: Vec<Line> = app
        .get_lines()
        .into_iter()
        .enumerate()
        .skip(app.scroll_top)
        .map(|(idx, line)| {
            let t = line.trim();

            // Compute which character range on this source line is selected.
            // source_len = char count of the raw source line (used to clamp columns).
            let source_len = line.chars().count();
            let sel_range: Option<(usize, usize)> = sel.and_then(|((sl, sc), (el, ec))| {
                if idx < sl || idx > el { return None; }
                let start = if idx == sl { sc.min(source_len) } else { 0 };
                let end   = if idx == el { ec.min(source_len) } else { source_len };
                if start == end { None } else { Some((start, end)) }
            });

            // Helper: given rendered text (owned String) and optional col_offset
            // (how many source chars were stripped from the front), return a Line
            // with proper partial selection spans.
            // col_offset: source chars stripped before the rendered text begins.
            // For regular lines col_offset=0, for "# Title" col_offset=2, etc.
            let make_line = |rendered_text: String, col_offset: usize, base_style: Style, sel_style: Style| -> Line {
                match sel_range {
                    None => Line::styled(rendered_text, base_style),
                    Some((src_start, src_end)) => {
                        // Map source selection columns into rendered-text columns.
                        let rlen = rendered_text.chars().count();
                        let r_start = src_start.saturating_sub(col_offset).min(rlen);
                        let r_end   = src_end.saturating_sub(col_offset).min(rlen);
                        if r_start == 0 && r_end == rlen {
                            return Line::styled(rendered_text, sel_style);
                        }
                        if r_start >= r_end {
                            return Line::styled(rendered_text, base_style);
                        }
                        let chars: Vec<char> = rendered_text.chars().collect();
                        let before:   String = chars[..r_start].iter().collect();
                        let selected: String = chars[r_start..r_end].iter().collect();
                        let after:    String = chars[r_end..].iter().collect();
                        let mut spans = vec![];
                        if !before.is_empty()   { spans.push(Span::styled(before,   base_style)); }
                        spans.push(Span::styled(selected, sel_style));
                        if !after.is_empty()    { spans.push(Span::styled(after,    base_style)); }
                        Line::from(spans)
                    }
                }
            };

            let sel_style_bold_ul = Style::default().fg(sel_fg).bg(sel_bg).add_modifier(Modifier::BOLD | Modifier::UNDERLINED);
            let sel_style_bold    = Style::default().fg(sel_fg).bg(sel_bg).add_modifier(Modifier::BOLD);
            let sel_style_plain   = Style::default().fg(sel_fg).bg(sel_bg);

            if t.starts_with("# ") {
                let text = t.trim_start_matches("# ").to_string();
                let base = Style::default().fg(colors.header).add_modifier(Modifier::BOLD | Modifier::UNDERLINED);
                make_line(text, 2, base, sel_style_bold_ul)
            } else if t.starts_with("## ") {
                let text = t.trim_start_matches("## ").to_string();
                let base = Style::default().fg(colors.accent).add_modifier(Modifier::BOLD);
                make_line(text, 3, base, sel_style_bold)
            } else if t.starts_with("### ") {
                let text = t.trim_start_matches("### ").to_string();
                let base = Style::default().fg(colors.quote).add_modifier(Modifier::BOLD);
                make_line(text, 4, base, sel_style_bold)
            } else if t.starts_with("> [!") {
                // Callout — keep full-line highlight (rendered text is transformed, col mapping unreliable).
                let text = format!("{ct}{}{ce}", t.trim_start_matches("> "));
                let base = Style::default().fg(colors.quote).add_modifier(Modifier::BOLD);
                make_line(text, 2, base, sel_style_bold)
            } else if t.starts_with("> ") {
                let text = format!("{cb}{}", t.trim_start_matches("> "));
                let base = Style::default().fg(colors.quote);
                make_line(text, 2, base, sel_style_plain)
            } else if t.starts_with("- [x]") || t.starts_with("- [X]") {
                // Prefix "- [x] " = 6 chars stripped, replaced by check glyph.
                let body = t.trim_start_matches("- [x]").trim_start_matches("- [X]").trim().to_string();
                let icon_sel = sel_range.is_some();
                let icon_s = if icon_sel { sel_style_bold    } else { Style::default().fg(colors.accent).add_modifier(Modifier::BOLD) };
                let text_s = if icon_sel { sel_style_plain   } else { Style::default().fg(colors.fg) };
                Line::from(vec![Span::styled(check, icon_s), Span::styled(body, text_s)])
            } else if t.starts_with("- [ ]") {
                let body = t.trim_start_matches("- [ ]").trim().to_string();
                let icon_sel = sel_range.is_some();
                let icon_s = if icon_sel { sel_style_plain   } else { Style::default().fg(colors.muted) };
                let text_s = if icon_sel { sel_style_plain   } else { Style::default().fg(colors.fg) };
                Line::from(vec![Span::styled(uncheck, icon_s), Span::styled(body, text_s)])
            } else if t.starts_with("- ") || t.starts_with("* ") {
                let body = t.trim_start_matches("- ").trim_start_matches("* ").to_string();
                let icon_sel = sel_range.is_some();
                let icon_s = if icon_sel { sel_style_plain   } else { Style::default().fg(colors.accent) };
                let text_s = if icon_sel { sel_style_plain   } else { Style::default().fg(colors.fg) };
                Line::from(vec![Span::styled(bullet, icon_s), Span::styled(body, text_s)])
            } else if t == "---" {
                let base = Style::default().fg(colors.muted);
                make_line(hr.to_string(), 0, base, sel_style_plain)
            } else {
                // Regular text: source columns map 1:1 to rendered columns.
                let base = Style::default().fg(colors.fg);
                make_line(line.to_string(), 0, base, sel_style_plain)
            }
        })
        .collect();

    let inner_width = area.width.saturating_sub(2) as usize;
    
    let right_scrolled = app.get_lines().iter().skip(app.scroll_top).take(area.height as usize).any(|l| l.chars().count() > app.scroll_left + inner_width);
    
    let mut block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(colors.accent))
        .title(ratatui::widgets::block::Title::from(" Writer Preview ").alignment(ratatui::layout::Alignment::Center));

    if app.scroll_left > 0 {
        block = block.title(ratatui::widgets::block::Title::from(" < ").alignment(ratatui::layout::Alignment::Left));
    }
    if right_scrolled {
        block = block.title(ratatui::widgets::block::Title::from(" > ").alignment(ratatui::layout::Alignment::Right));
    }

    let p = Paragraph::new(Text::from(rendered))
        .block(block)
        .scroll((0, app.scroll_left as u16))
        .style(Style::default().bg(colors.bg));
    f.render_widget(p, area);
}

/// Raw markdown editor with line numbers and selection highlighting.
fn render_markdown_editor(
    f: &mut ratatui::Frame,
    area: Rect,
    app: &App,
    colors: &ThemeColors,
) {
    let sep = if app.theme == Theme::VT100 { "|" } else { "│" };
    let sel = app.selection_range();
    let (sel_bg, sel_fg) = (colors.sel_bg, colors.sel_fg);

    let lines: Vec<Line> = app
        .get_lines()
        .into_iter()
        .enumerate()
        .skip(app.scroll_top)
        .map(|(idx, line)| {
            let chars: Vec<char> = line.chars().collect();
            let len = chars.len();

            let inner_width = area.width.saturating_sub(8) as usize;
            let mut line_sep = sep.to_string();
            let mut sep_style = Style::default().fg(colors.muted);

            if app.scroll_left > 0 && len > 0 {
                line_sep = "<".to_string();
                sep_style = Style::default().fg(colors.accent).add_modifier(Modifier::BOLD);
            } else if len > app.scroll_left + inner_width {
                line_sep = ">".to_string();
                sep_style = Style::default().fg(colors.accent).add_modifier(Modifier::BOLD);
            }

            let num_span = Span::styled(
                format!("{:3} {line_sep} ", idx + 1),
                sep_style,
            );

            let display_chars: Vec<char> = chars.into_iter().skip(app.scroll_left).collect();
            let display_len = display_chars.len();

            // Determine selection column range for this specific line.
            let sel_range: Option<(usize, usize)> = sel.and_then(|((sl, sc), (el, ec))| {
                if idx < sl || idx > el { return None; }
                let start = if idx == sl { sc.min(len) } else { 0 };
                let end   = if idx == el { ec.min(len) } else { len };
                
                let adj_start = start.saturating_sub(app.scroll_left).min(display_len);
                let adj_end = end.saturating_sub(app.scroll_left).min(display_len);
                
                if adj_start == adj_end { None } else { Some((adj_start, adj_end)) }
            });

            match sel_range {
                None => {
                    Line::from(vec![num_span,
                        Span::styled(display_chars.into_iter().collect::<String>(), Style::default().fg(colors.fg))])
                }
                Some((start, end)) if start == 0 && end == display_len => {
                    Line::from(vec![num_span,
                        Span::styled(display_chars.into_iter().collect::<String>(), Style::default().fg(sel_fg).bg(sel_bg))])
                }
                Some((start, end)) => {
                    let before:   String = display_chars[..start].iter().collect();
                    let selected: String = display_chars[start..end].iter().collect();
                    let after:    String = display_chars[end..].iter().collect();
                    let mut spans = vec![num_span];
                    if !before.is_empty() {
                        spans.push(Span::styled(before, Style::default().fg(colors.fg)));
                    }
                    spans.push(Span::styled(selected, Style::default().fg(sel_fg).bg(sel_bg)));
                    if !after.is_empty() {
                        spans.push(Span::styled(after, Style::default().fg(colors.fg)));
                    }
                    Line::from(spans)
                }
            }
        })
        .collect();

    let inner_width = area.width.saturating_sub(8) as usize;
    
    let base_title = if app.view_mode == ViewMode::PureText { " Pure Text Editor " } else { " Markdown Editor " };
    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(colors.border))
        .title(ratatui::widgets::block::Title::from(base_title).alignment(ratatui::layout::Alignment::Center));

    let p = Paragraph::new(Text::from(lines))
        .block(block)
        .style(Style::default().bg(colors.bg));
    f.render_widget(p, area);
}

fn render_dropdown_popup(f: &mut ratatui::Frame, app: &App, colors: &ThemeColors) {
    let (title, x_off): (&str, u16) = match app.active_menu {
        ActiveMenu::File => (" File ", 1),
        ActiveMenu::Edit => (" Edit ", 9),
        ActiveMenu::Format => (" Format ", 17),
        ActiveMenu::View => (" View ", 27),
        ActiveMenu::Theme => (" Theme ", 35),
        ActiveMenu::Help => (" Help ", 44),
        ActiveMenu::Manipulation => (" Manipulation ", 17),
        ActiveMenu::None => return,
    };

    let items = get_menu_items(app.active_menu);
    let h = (items.len() as u16 + 2).max(4);
    let w = 34_u16;
    let area = Rect::new(
        x_off,
        1,
        w.min(f.area().width.saturating_sub(x_off)),
        h.min(f.area().height.saturating_sub(1)),
    );
    f.render_widget(Clear, area);

    let prefix = if app.theme == Theme::VT100 { " > " } else { " ► " };
    let list_items: Vec<ListItem> = items
        .iter()
        .enumerate()
        .map(|(idx, (label, _))| {
            if idx == app.menu_selected {
                ListItem::new(Span::styled(
                    format!("{prefix}{label}"),
                    Style::default()
                        .bg(colors.accent)
                        .fg(colors.bg)
                        .add_modifier(Modifier::BOLD),
                ))
            } else {
                ListItem::new(Span::styled(
                    format!("   {label}"),
                    Style::default().fg(colors.fg),
                ))
            }
        })
        .collect();

    let list = List::new(list_items)
        .block(
            Block::default()
                .borders(Borders::ALL)
                .title(title)
                .border_style(Style::default().fg(colors.accent)),
        )
        .style(Style::default().bg(colors.bg));
    f.render_widget(list, area);
}

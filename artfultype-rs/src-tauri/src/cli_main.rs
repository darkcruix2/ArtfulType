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

#[derive(Debug, Clone, Copy, PartialEq)]
enum ActiveMenu {
    None,
    File,
    Edit,
    Format,
    View,
    Theme,
    Help,
}

#[derive(Debug, Clone, Copy)]
enum MenuAction {
    SaveFile,
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
    ThemeDarkAntigravity,
    ThemeRetroGreen,
    ThemeRetroAmber,
    ThemeDracula,
    ThemeVT100,
    NoOp,
}

struct App {
    file_path: Option<String>,
    file_name: String,
    content: String,
    cursor_line: usize,
    cursor_col: usize,
    scroll_top: usize,
    // Selection anchor: set when Shift-movement begins; None = no selection.
    selection_anchor: Option<(usize, usize)>,
    // Internal clipboard for cut/copy/paste.
    clipboard: String,
    view_mode: ViewMode,
    theme: Theme,
    active_menu: ActiveMenu,
    menu_selected: usize,
    dirty: bool,
    status_msg: String,
    should_quit: bool,
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
            content,
            cursor_line: 0,
            cursor_col: 0,
            scroll_top: 0,
            selection_anchor: None,
            clipboard: String::new(),
            view_mode: initial_mode.unwrap_or(ViewMode::Split),
            theme: initial_theme.unwrap_or(Theme::DarkAntigravity),
            active_menu: ActiveMenu::None,
            menu_selected: 0,
            dirty: false,
            status_msg: "Ready".to_string(),
            should_quit: false,
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

    fn sync_content_from_lines(&mut self, lines: Vec<String>) {
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
            self.clipboard = text;
            self.status_msg = "Copied".to_string();
        }
    }

    fn cut_selection(&mut self, inner_height: usize) {
        let text = self.selected_text();
        if !text.is_empty() {
            self.clipboard = text;
            self.delete_selection();
            self.clamp_scroll(inner_height);
            self.status_msg = "Cut".to_string();
        }
    }

    fn paste(&mut self) {
        if self.selection_anchor.is_some() {
            self.delete_selection();
        }
        let text = self.clipboard.clone();
        for c in text.chars() {
            if c == '\n' {
                self.insert_newline();
            } else {
                self.insert_char(c);
            }
        }
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
            if fs::write(path, &self.content).is_ok() {
                self.dirty = false;
                self.status_msg = format!("Saved: {}", self.file_name);
            } else {
                self.status_msg = "Error saving file".to_string();
            }
        } else {
            self.status_msg = "No file path. Specify path on CLI.".to_string();
        }
    }

    fn execute_action(&mut self, action: MenuAction, inner_height: usize) {
        match action {
            MenuAction::SaveFile => self.save_file(),
            MenuAction::Quit => self.should_quit = true,
            MenuAction::Heading1 => self.insert_str_at_cursor("# "),
            MenuAction::Heading2 => self.insert_str_at_cursor("## "),
            MenuAction::Heading3 => self.insert_str_at_cursor("### "),
            MenuAction::Bold => self.wrap_selection_or_insert("**", "**", "**bold**"),
            MenuAction::Italic => self.wrap_selection_or_insert("*", "*", "*italic*"),
            MenuAction::Code => self.wrap_selection_or_insert("`", "`", "`code`"),
            MenuAction::CalloutNote => self.insert_str_at_cursor("> [!NOTE]\n> "),
            MenuAction::TaskCheckbox => self.insert_str_at_cursor("- [ ] "),
            MenuAction::ViewWriter => self.view_mode = ViewMode::Writer,
            MenuAction::ViewMarkdown => self.view_mode = ViewMode::Markdown,
            MenuAction::ViewSplit => self.view_mode = ViewMode::Split,
            MenuAction::ThemeDarkAntigravity => self.theme = Theme::DarkAntigravity,
            MenuAction::ThemeRetroGreen => self.theme = Theme::RetroGreen,
            MenuAction::ThemeRetroAmber => self.theme = Theme::RetroAmber,
            MenuAction::ThemeDracula => self.theme = Theme::Dracula,
            MenuAction::ThemeVT100 => self.theme = Theme::VT100,
            MenuAction::NoOp => {}
        }
        self.clamp_scroll(inner_height);
        self.active_menu = ActiveMenu::None;
    }
}

fn get_menu_items(menu: ActiveMenu) -> Vec<(&'static str, MenuAction)> {
    match menu {
        ActiveMenu::File => vec![
            ("[S] Save File  (Ctrl+S)", MenuAction::SaveFile),
            ("[Q] Quit       (Ctrl+Q)", MenuAction::Quit),
        ],
        ActiveMenu::Edit => vec![
            ("Copy          (Ctrl+C)", MenuAction::NoOp),
            ("Cut           (Ctrl+X)", MenuAction::NoOp),
            ("Paste         (Ctrl+V)", MenuAction::NoOp),
            ("Bold          (Ctrl+B)", MenuAction::Bold),
            ("Italic        (Ctrl+I)", MenuAction::Italic),
            ("Code          (Ctrl+K)", MenuAction::Code),
        ],
        ActiveMenu::Format => vec![
            ("H1 Heading 1  (Ctrl+1)", MenuAction::Heading1),
            ("H2 Heading 2  (Ctrl+2)", MenuAction::Heading2),
            ("H3 Heading 3  (Ctrl+3)", MenuAction::Heading3),
            ("Bold          (Ctrl+B)", MenuAction::Bold),
            ("Italic        (Ctrl+I)", MenuAction::Italic),
            ("Code          (Ctrl+K)", MenuAction::Code),
            ("Callout Note", MenuAction::CalloutNote),
            ("Task Checkbox", MenuAction::TaskCheckbox),
        ],
        ActiveMenu::View => vec![
            ("Writer Mode   (F2)", MenuAction::ViewWriter),
            ("Markdown Mode (F3)", MenuAction::ViewMarkdown),
            ("Split Mode    (F4)", MenuAction::ViewSplit),
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
            ("Ctrl+C/X/V: Copy/Cut/Paste", MenuAction::NoOp),
            ("Alt+F/E/O/V/T: Open Menus", MenuAction::NoOp),
            ("F2:Writer  F3:MD  F4:Split", MenuAction::NoOp),
            ("Ctrl+S:Save  Ctrl+Q:Quit", MenuAction::NoOp),
        ],
        ActiveMenu::None => vec![],
    }
}

fn next_menu(m: ActiveMenu) -> ActiveMenu {
    match m {
        ActiveMenu::File => ActiveMenu::Edit,
        ActiveMenu::Edit => ActiveMenu::Format,
        ActiveMenu::Format => ActiveMenu::View,
        ActiveMenu::View => ActiveMenu::Theme,
        ActiveMenu::Theme => ActiveMenu::Help,
        _ => ActiveMenu::File,
    }
}

fn prev_menu(m: ActiveMenu) -> ActiveMenu {
    match m {
        ActiveMenu::Edit => ActiveMenu::File,
        ActiveMenu::Format => ActiveMenu::Edit,
        ActiveMenu::View => ActiveMenu::Format,
        ActiveMenu::Theme => ActiveMenu::View,
        ActiveMenu::Help => ActiveMenu::Theme,
        _ => ActiveMenu::Help,
    }
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

                let shift = key.modifiers.contains(KeyModifiers::SHIFT);
                let ctrl  = key.modifiers.contains(KeyModifiers::CONTROL);
                let alt   = key.modifiers.contains(KeyModifiers::ALT);

                // ── Alt key: open menus ──────────────────────────────────
                if alt {
                    match key.code {
                        KeyCode::Char('f') | KeyCode::Char('F') => app.open_menu(ActiveMenu::File),
                        KeyCode::Char('e') | KeyCode::Char('E') => app.open_menu(ActiveMenu::Edit),
                        KeyCode::Char('o') | KeyCode::Char('O') => app.open_menu(ActiveMenu::Format),
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
                        KeyCode::Left => app.open_menu(prev_menu(app.active_menu)),
                        KeyCode::Right => app.open_menu(next_menu(app.active_menu)),
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
                    match key.code {
                        KeyCode::Char('q') => app.should_quit = true,
                        KeyCode::Char('s') => app.save_file(),
                        KeyCode::Char('c') => app.copy_selection(),
                        KeyCode::Char('x') => app.cut_selection(inner_h),
                        KeyCode::Char('v') => app.paste(),
                        KeyCode::Char('b') => app.wrap_selection_or_insert("**", "**", "**bold**"),
                        KeyCode::Char('i') => app.wrap_selection_or_insert("*", "*", "*italic*"),
                        KeyCode::Char('k') => app.wrap_selection_or_insert("`", "`", "`code`"),
                        KeyCode::Char('1') => app.insert_str_at_cursor("# "),
                        KeyCode::Char('2') => app.insert_str_at_cursor("## "),
                        KeyCode::Char('3') => app.insert_str_at_cursor("### "),
                        KeyCode::Home | KeyCode::Up => app.move_to_file_start(),
                        KeyCode::End | KeyCode::Down => app.move_to_file_end(inner_h),
                        _ => {}
                    }
                    app.clamp_scroll(inner_h);
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
            " [Format] ",
            if app.active_menu == ActiveMenu::Format {
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
        ViewMode::Markdown => {
            render_markdown_editor(f, editor_rect, app, &colors);
            if app.active_menu == ActiveMenu::None && cursor_row_in_view < inner_h {
                let cx = inner_x + 6 + app.cursor_col as u16;
                let cy = inner_y + cursor_row_in_view as u16;
                f.set_cursor_position((cx, cy));
            }
        }
        ViewMode::Writer => {
            render_writer_view(f, editor_rect, app, &colors);
            if app.active_menu == ActiveMenu::None && cursor_row_in_view < inner_h {
                let cx = inner_x + app.cursor_col as u16;
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
                let left_inner_x = split[0].x + 1;
                let left_inner_y = split[0].y + 1;
                let cx = left_inner_x + 6 + app.cursor_col as u16;
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

    let p = Paragraph::new(Text::from(rendered))
        .block(
            Block::default()
                .borders(Borders::ALL)
                .title(" Writer Preview ")
                .border_style(Style::default().fg(colors.accent)),
        )
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
            let num_span = Span::styled(
                format!("{:3} {sep} ", idx + 1),
                Style::default().fg(colors.muted),
            );

            let chars: Vec<char> = line.chars().collect();
            let len = chars.len();

            // Determine selection column range for this specific line.
            let sel_range: Option<(usize, usize)> = sel.and_then(|((sl, sc), (el, ec))| {
                if idx < sl || idx > el { return None; }
                let start = if idx == sl { sc.min(len) } else { 0 };
                let end   = if idx == el { ec.min(len) } else { len };
                if start == end { None } else { Some((start, end)) }
            });

            match sel_range {
                None => {
                    // No selection on this line.
                    Line::from(vec![num_span,
                        Span::styled(line.to_string(), Style::default().fg(colors.fg))])
                }
                Some((start, end)) if start == 0 && end == len => {
                    // Entire line selected.
                    Line::from(vec![num_span,
                        Span::styled(line.to_string(), Style::default().fg(sel_fg).bg(sel_bg))])
                }
                Some((start, end)) => {
                    // Partial line selection: split into before / selected / after.
                    let before:   String = chars[..start].iter().collect();
                    let selected: String = chars[start..end].iter().collect();
                    let after:    String = chars[end..].iter().collect();
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

    let p = Paragraph::new(Text::from(lines))
        .block(
            Block::default()
                .borders(Borders::ALL)
                .title(" Markdown Editor ")
                .border_style(Style::default().fg(colors.border)),
        )
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

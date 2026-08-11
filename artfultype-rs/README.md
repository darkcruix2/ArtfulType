# ArtfulType Pro (Rust / Tauri Edition) v0.30.2

A distraction-free Markdown writing app built natively for desktop and terminal using Rust and Tauri. This is the modern, cross-platform successor to the classic Macintosh `ArtfulType`.

## Features

- **Writer mode** — live Markdown-to-rich-text preview
- **Markdown mode** — plain raw-syntax editing
- **Pure Text / Code mode** — distraction-free code and plain text editing with toggleable syntax highlighting (`F6` / `Ctrl+H`) and code auto-indentation
- **☁ Nextcloud Cloud Integration** — link Nextcloud storage via WebDAV, browse remote folder structures, edit and auto-save directly to Nextcloud in both GUI (`artfultype-rs`) and TUI (`art`)
- **Native File I/O** — Open/Save local plain Markdown & code files
- **Dark mode glassmorphism & Dracula themes**

## Building from Source

Since this application is built with Tauri and Rust, it can be built for Windows, macOS, and Linux natively. You do **not** need Node.js or `npm` installed, as this project uses a vanilla HTML/CSS frontend.

### 1. Prerequisites

First, ensure you have Rust and Cargo installed:

- [Install Rust](https://www.rust-lang.org/tools/install)

Next, you need to install the Tauri CLI via Cargo:

```bash
cargo install tauri-cli --version "^2.0.0" --locked
```

Depending on your client machine's operating system, you will need the following system dependencies:

#### Windows

- Install the [C++ Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) (included with Visual Studio or as a standalone).
- *WebView2* is pre-installed on Windows 11.

#### macOS

- Install the Xcode Command Line Tools:

  ```bash
  xcode-select --install
  ```

#### Linux

- Install `webkit2gtk` and related build tools. For Debian/Ubuntu:

  ```bash
  sudo apt update
  sudo apt install libwebkit2gtk-4.1-dev build-essential curl wget file libssl-dev libgtk-3-dev libayatana-appindicator3-dev librsvg2-dev
  ```

### 2. Developing & Running

To run the app locally with hot-reloading on your client desktop environment:

```bash
cargo tauri dev
```

### 3. Building a Release

To compile a highly optimized, native executable/installer (e.g. `.exe` on Windows, `.dmg`/`.app` on macOS, `.deb`/AppImage on Linux):

```bash
cargo tauri build
```

The resulting binaries will be placed in the `src-tauri/target/release/bundle/` directory.


## Pre-built Releases

Pre-compiled release binaries and packages are available in the `../releases/` directory:

- **macOS (ARM64)**:
  - GUI DMG Installer: `../releases/artfultype-rs_0.30.2_aarch64.dmg`
  - GUI App Zip: `../releases/artfultype-rs-mac.zip`
  - CLI Binary (`art`): `../releases/artfultype-cli-mac-arm64`
- **Linux (AMD64)**:
  - AppImage: `../releases/artfultype-rs_0.30.2_amd64.AppImage`
  - Debian Package: `../releases/artfultype-rs_0.30.2_amd64.deb`
  - RPM Package: `../releases/artfultype-rs-0.30.2-1.x86_64.rpm`
- **Windows (x64)**:
  - Installer: `../releases/artfultype-rs_0.30.2_x64-setup.exe`
  - Portable Executable: `../releases/artfultype-rs-windows-x64.exe`

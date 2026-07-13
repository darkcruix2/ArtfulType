# ArtfulType Pro Version Implementation

## Overview

The Pro version of ArtfulType implements multi-document support with enhanced window management capabilities. This document describes the architectural changes and implementation details that distinguish the Pro version from the standard version.

## Key Architectural Changes

### Document Record Structure

The Pro version introduces a `DocumentRecord` structure to manage multiple open documents:

```c
typedef struct DocumentRecord {
    WindowPtr window;
    WEHandle te;
    WEHandle hiddenTE;
    WEHandle activeTE;
    ControlHandle scrollBar;
    ControlHandle jumpToTopBtn;
    ControlHandle jumpToEndBtn;
    Boolean scrollBarVisible;
    
    Boolean haveFile;
    Boolean dirty;
    Str255 fileName;
    short vRefNum;
    Boolean hideMarkdown;
    Handle markdownText;
    long markdownLen;
    Handle writerText;
    long writerLen;
    Handle writerOpsH;
    short writerOpCount;
    long windowStart;
    long windowEnd;
    Handle lineOffsetsH;
    long numLines;
    long lastCharCount;
    short lastLine;
    short lastCol;
    Boolean showStatusBar;
    UndoSnapshot undoStack[MAX_UNDO_LEVELS];
    short undoCount;
    UndoSnapshot redoStack[MAX_UNDO_LEVELS];
    short redoCount;
    Boolean typingRunActive;
    Str255 linkURLs[MAX_LINKS + 1];
    short linkCount;
    Boolean shiftSelectionActive;
    short shiftAnchor;
    short zoomIndex;
    struct DocumentRecord *next;
} DocumentRecord;
```

### Global State Management

The Pro version replaces global variables with document-specific pointers:

- **Standard Version**: Uses global variables like `gWindow`, `gTE`, `gActiveTE`
- **Pro Version**: Uses document-specific pointers like `gActiveDoc->window`, `gActiveDoc->te`, `gActiveDoc->activeTE`

## Multi-Document Support Implementation

### Document List Management

The application maintains a linked list of open documents:

```c
extern DocumentRecord *gActiveDoc;
extern DocumentRecord *gDocumentList;
```

### Window Menu Integration

The Pro version implements a window menu (`mWindow`) that lists all open documents, allowing users to switch between different documents.

## Event Loop Modifications

The `EventLoop` function has been modified to handle multiple documents:

1. **Window Activation**: Properly sets the active document when clicking on different windows
2. **Document-Specific Operations**: All operations now reference the active document's state
3. **Menu Handling**: Enhanced menu processing for multi-document scenarios

## Conditional Compilation

The codebase uses `#ifdef ARTFUL_PRO` preprocessor directives to maintain both versions:

```c
#ifdef ARTFUL_PRO
    // Pro version specific code
#else
    // Standard version code
#endif
```

## Key Pro Features

### Multi-Window Support
- Multiple documents can be open simultaneously
- Each document maintains independent state (dirty flag, file name, etc.)
- Window switching through the Window menu

### Enhanced Window Management
- Custom window menu showing all open documents
- Proper port management for multiple windows
- Document-specific scrollbars and UI elements

### Context-Aware Operations
- All operations automatically target the active document
- Proper state tracking for each individual document
- Independent undo/redo stacks per document

## Implementation Details

### Document Activation
```c
void SetActiveDocument(DocumentRecord *doc);
DocumentRecord* GetDocumentForWindow(WindowPtr w);
```

### Window Menu Updates
```c
void UpdateWindowMenu(void);
```

### Event Handling
The event loop handles:
- Window activation and deactivation
- Mouse clicks on different document windows
- Menu selections for window switching
- Proper port restoration when switching between documents

## Differences from Standard Version

| Feature | Standard Version | Pro Version |
|-------|------------------|-----------|
| Document Support | Single document | Multiple documents |
| Window Management | Single window | Multiple windows |
| State Tracking | Global variables | Document-specific |
| Menu System | Simple menu | Enhanced with window menu |
| Undo/Redo | Global stack | Per-document stacks |

## Code Structure

The Pro version maintains backward compatibility while extending functionality through:
1. **Conditional compilation** for version-specific code
2. **Document-oriented architecture** 
3. **Enhanced state management** for multiple simultaneous documents
4. **Proper window handling** with multi-window support

This implementation allows users to work with multiple documents simultaneously while preserving the familiar ArtfulType interface and functionality.
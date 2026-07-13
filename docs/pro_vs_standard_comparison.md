# ArtfulType Pro vs Standard Version Comparison

## Overview

This document provides a detailed comparison between the Pro and Standard versions of ArtfulType, highlighting the key differences that distinguish the Pro version's enhanced capabilities.

## Core Architecture Differences

### Standard Version
- **Single Document Model**: Operates with one document at a time
- **Global State**: Uses global variables for all application state
  - `gWindow`, `gTE`, `gActiveTE` for window and text editor references
  - Global variables for file name, dirty flag, etc.
- **Simple Event Loop**: Basic event handling for single window
- **Monolithic Design**: All functionality tied to single document context

### Pro Version
- **Multi-Document Model**: Supports multiple documents simultaneously
- **Document-Oriented Architecture**: Uses `DocumentRecord` structure for state management
  - `gActiveDoc` pointer tracks current document
  - `gDocumentList` maintains linked list of all open documents
- **Enhanced Event Loop**: Complex event handling for multiple windows
- **Modular Design**: Each document maintains independent state

## Feature Comparison

| Feature | Standard Version | Pro Version |
|---------|--------------|-------------|
| **Document Support** | Single document only | Multiple simultaneous documents |
| **Window Management** | Single window | Multiple windows with window menu |
| **Undo/Redo** | Global stack | Per-document stacks |
| **State Tracking** | Global variables | Document-specific |
| **Memory Management** | Simple allocation | Handle pooling for efficiency |
| **UI Elements** | Shared UI components | Independent per document |
| **Menu System** | Basic menus | Enhanced with window menu |
| **Window Switching** | Not supported | Full switching capability |

## Technical Implementation Details

### State Management
**Standard Version**:
```c
// Global variables
extern WindowPtr gWindow;
extern WEHandle gTE;
extern Boolean gDirty;
extern Str255 gFileName;
```

**Pro Version**:
```c
// Document-specific pointers
extern DocumentRecord *gActiveDoc;
#define gWindow (gActiveDoc->window)
#define gTE (gActiveDoc->te)
#define gDirty (gActiveDoc->dirty)
#define gFileName (gActiveDoc->fileName)
```

### Event Handling
**Standard Version**: Simple event loop processing single window events

**Pro Version**: Enhanced event loop with:
- Window activation/deactivation handling
- Document switching based on window clicks
- Proper port management for multiple windows
- Context-aware operations targeting active document

### Memory Management
**Standard Version**: Direct memory allocation for all components

**Pro Version**: 
- Handle pooling system (`gMemoryPool`)
- Efficient allocation/deallocation for multiple documents
- Resource cleanup for closed documents

## User Experience Differences

### Standard Version
- Single window interface
- Limited to one document at a time
- Basic menu system
- Global undo/redo functionality
- No window switching capability

### Pro Version
- Multiple window interface
- Simultaneous document editing
- Enhanced window menu with document listing
- Independent undo/redo for each document
- Window switching and management capabilities
- Customizable view settings per document

## Performance Considerations

### Standard Version
- Lower memory overhead
- Simple state tracking
- Fast execution for single document operations

### Pro Version
- Higher memory usage due to multiple document handles
- More complex event handling
- Enhanced performance through handle pooling
- Efficient resource management for multiple documents

## Code Structure Differences

### Standard Version Files
- Single main.c file with global state
- Simple conditional compilation (`#ifndef ARTFUL_PRO`)
- Direct function calls without document context

### Pro Version Files
- Enhanced main.c with document management functions
- Complex event loop handling
- DocumentRecord structure and related functions
- Multiple conditional compilation blocks (`#ifdef ARTFUL_PRO`)
- Enhanced menu and window management code

## Limitations and Trade-offs

### Pro Version Advantages
- Multi-document support
- Enhanced productivity features
- Independent document state management
- Better workflow for complex projects

### Pro Version Limitations
- Higher memory consumption
- More complex implementation
- Increased code maintenance overhead
- Steeper learning curve for users

## Conclusion

The Pro version of ArtfulType represents a significant architectural enhancement over the standard version, introducing multi-document support while maintaining backward compatibility. The implementation demonstrates sophisticated state management through document-oriented design, enabling users to work with multiple documents simultaneously while preserving the familiar ArtfulType interface and functionality.

The key innovation lies in replacing global state with document-specific pointers, allowing for independent operation of multiple windows while sharing core application functionality. This approach provides substantial productivity benefits for users working on complex projects involving multiple related documents.
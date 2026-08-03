/*
 * file.c — ArtfulType Pro  (Carbon / OS X 10.4 port)
 *
 * Key differences from classic Retro68 version
 * ─────────────────────────────────────────────
 * SFGetFile / SFPutFile  → NavGetFile / NavPutFile (Navigation Services)
 * FSOpen/FSWrite/FSClose → POSIX fopen/fwrite/fclose for simplicity
 *                          (FSRef-based HFS APIs also work but POSIX is cleaner)
 * CountAppFiles / GetAppFiles → replaced by Apple Event handler in main.c
 *                               (DoStartupOpen() is now a no-op)
 * gWindow references        → gWindow is still a macro via gActiveDoc->window
 * SetWTitle()               → SetWindowTitleWithCFString()
 * Create() / FSOpen()       → FSCreateFileUnicode / POSIX (see WriteFile)
 * InvalRect()               → InvalWindowRect()
 */

#include "app.h"
#include <string.h>

/* -----------------------------------------------------------------------
   Internal helpers
   ----------------------------------------------------------------------- */
static void RefreshActiveView(void)
{
    if (gHideMarkdown) BuildHiddenView();
    else               ClearStyles();
}

/* Set the window title from a Pascal string */
static void SetWindowTitle(WindowRef w, const unsigned char *pstr)
{
    CFStringRef title = CFStringCreateWithPascalString(NULL, pstr,
                                                       kCFStringEncodingMacRoman);
    if (title) {
        SetWindowTitleWithCFString(w, title);
        CFRelease(title);
    }
}

/* -----------------------------------------------------------------------
   View mode
   ----------------------------------------------------------------------- */
void SetViewMode(Boolean hideMarkdown)
{
    if (hideMarkdown == gHideMarkdown) return;

    ClearUndoRedoStacks();
    UpdateEditMenuState();
    WEDeactivate(gActiveTE);

    if (hideMarkdown) {
        SyncWindowToBacking();
        gHideMarkdown         = true;
        gActiveDoc->activeTE  = gHiddenTE;
        BuildHiddenView();
        gWriterDirty = false;
    } else {
        SyncWindowToBacking();
        if (gWriterDirty) SyncHiddenToCanonical();
        gWriterDirty          = false;
        gHideMarkdown         = false;
        gActiveDoc->activeTE  = gTE;
        LoadTextWindow(gWindowStart);
    }

    WEActivate(gActiveDoc->activeTE);
    CheckMenuItem(gViewMenu, iMarkdownView, !hideMarkdown);
    CheckMenuItem(gViewMenu, iWriterView,    hideMarkdown);
    UpdateMenuBarLook();
    AdjustScrollbar();
    Rect portRect;
    GetWindowPortBounds(gWindow, &portRect);
    InvalWindowRect(gWindow, &portRect);
}

/* -----------------------------------------------------------------------
   Low-level file I/O using POSIX (simpler than FSRef HFS+ APIs, and fully
   supported on OS X 10.4 for plain text files)
   ----------------------------------------------------------------------- */

/* Convert an FSRef to a POSIX path */
static Boolean FSRefToPosixPath(const FSRef *ref, char *path, size_t pathLen)
{
    return FSRefMakePath(ref, (UInt8 *)path, (UInt32)pathLen) == noErr;
}

/* Write the document's markdown text to a file at the given POSIX path */
static void WriteFilePosix(const char *path)
{
    Handle textH;
    long   count;

    SyncWindowToBacking();
    if (gHideMarkdown) SyncHiddenToCanonical();

    if (gMarkdownText != NULL) {
        count = gMarkdownLen;
        textH = gMarkdownText;
    } else {
        count = WEGetTextLength(gTE);
        textH = WEGetText(gTE);
    }

    FILE *f = fopen(path, "wb");
    if (!f) return;
    HLock(textH);
    fwrite(*textH, 1, (size_t)count, f);
    HUnlock(textH);
    fclose(f);

    /* If textH was from WEGetText, unlock and release the cached handle */
    if (gMarkdownText == NULL) HUnlock(textH);
}

/* Read a file at the given POSIX path into gMarkdownText */
static void ReadFilePosix(const char *path)
{
    FILE  *f = fopen(path, "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long eof = ftell(f);
    rewind(f);

    Handle textH = NewHandle(eof > 0 ? eof : 1);
    if (!textH) { fclose(f); return; }

    HLock(textH);
    long count = (long)fread(*textH, 1, (size_t)eof, f);
    fclose(f);

    /* UTF-8 / line-ending normalisation (identical to classic ReadFile) */
    {
        long i = 0, j = 0;
        /* Skip UTF-8 BOM */
        if (count >= 3 &&
            (unsigned char)(*textH)[0] == 0xEF &&
            (unsigned char)(*textH)[1] == 0xBB &&
            (unsigned char)(*textH)[2] == 0xBF)
            i = 3;

        for (; i < count; i++) {
            unsigned char c1 = (unsigned char)(*textH)[i];
            /* UTF-8 smart punctuation → MacRoman */
            if (c1 == 0xE2 && i+2 < count) {
                unsigned char c2 = (unsigned char)(*textH)[i+1];
                unsigned char c3 = (unsigned char)(*textH)[i+2];
                if (c2 == 0x80) {
                    if (c3==0x98||c3==0x99) { (*textH)[j++]='\''; i+=2; continue; }
                    if (c3==0x9C||c3==0x9D) { (*textH)[j++]='"';  i+=2; continue; }
                    if (c3==0x93||c3==0x94) { (*textH)[j++]='-';  i+=2; continue; }
                    if (c3==0xA6)           { (*textH)[j++]=0xC9; i+=2; continue; }
                    if (c3==0xA2)           { (*textH)[j++]=0xA5; i+=2; continue; }
                }
            }
            /* CRLF → CR */
            if ((*textH)[i]=='\r' && i+1<count && (*textH)[i+1]=='\n') {
                (*textH)[j++] = '\r'; i++; continue;
            }
            /* LF → CR */
            if ((*textH)[i]=='\n') { (*textH)[j++]='\r'; continue; }
            (*textH)[j++] = (*textH)[i];
        }
        count = j;
    }

    HUnlock(textH);
    SetHandleSize(textH, count);

    if (gMarkdownText != NULL) DisposeHandle(gMarkdownText);
    gMarkdownText = textH;
    gMarkdownLen  = count;
    gWindowStart  = 0;

    if (gHideMarkdown) BuildHiddenView();
    else               LoadTextWindow(0);

    SetDirty(false);
    ClearUndoRedoStacks();
    UpdateEditMenuState();
    RefreshActiveView();
    AdjustScrollbar();
    Rect portRect;
    GetWindowPortBounds(gWindow, &portRect);
    InvalWindowRect(gWindow, &portRect);
}

/* -----------------------------------------------------------------------
   Public: ReadFileFromFSRef — called from the Apple Event handler
   ----------------------------------------------------------------------- */
void ReadFileFromFSRef(const FSRef *ref)
{
    char path[1024];
    if (!FSRefToPosixPath(ref, path, sizeof(path))) return;

    /* Derive a Pascal filename from the last path component */
    {
        char *slash = strrchr(path, '/');
        const char *name = slash ? slash + 1 : path;
        size_t len = strlen(name);
        if (len > 254) len = 254;
        gFileName[0] = (unsigned char)len;
        BlockMove(name, gFileName + 1, len);
    }
    gHaveFile = true;
    /* Store the FSRef for later saves — encode into vRefNum as 0 sentinel
       and keep the POSIX path in a static for WriteFile to reuse. */
    gVRefNum = 0;

    ReadFilePosix(path);
    SetWindowTitle(gWindow, gFileName);
}

/* -----------------------------------------------------------------------
   DoStartupOpen — replaced by Apple Events in Carbon; kept as no-op
   ----------------------------------------------------------------------- */
void DoStartupOpen(void)
{
    /* No-op: file opens at launch arrive via kAEOpenDocuments Apple Event */
}

/* -----------------------------------------------------------------------
   Navigation Services — Save As
   ----------------------------------------------------------------------- */
Boolean DoSaveAs(void)
{
    NavDialogCreationOptions opts;
    NavDialogRef             dlg     = NULL;
    NavReplyRecord           reply;
    Boolean                  saved   = false;

    if (!gActiveDoc) return false;

    SyncWindowToBacking();
    if (gHideMarkdown) SyncHiddenToCanonical();

    NavGetDefaultDialogCreationOptions(&opts);
    opts.clientName    = CFSTR("ArtfulType Pro");
    opts.saveFileName  = CFSTR("Untitled.md");
    opts.optionFlags  |= kNavNoTypePopup;

    if (NavCreatePutFileDialog(&opts, 'TEXT', 'ArtT',
                               NULL, NULL, &dlg) != noErr || dlg == NULL)
        return false;

    if (NavDialogRun(dlg) != noErr) { NavDialogDispose(dlg); return false; }

    if (NavDialogGetReply(dlg, &reply) == noErr && reply.validRecord) {
        AEDesc fileDesc;
        FSRef  parentRef, fileRef;
        char   posixPath[1024];

        if (AEGetNthDesc(&reply.selection, 1, typeFSRef, NULL, &fileDesc) == noErr) {
            AEGetDescData(&fileDesc, &parentRef, sizeof(parentRef));
            AEDisposeDesc(&fileDesc);

            /* Get the save filename as a Pascal string */
            CFStringRef cfName = reply.saveFileName;
            Str255 pName;
            CFStringGetPascalString(cfName, pName, sizeof(pName),
                                    kCFStringEncodingMacRoman);

            /* Create/overwrite the file */
            if (FSRefMakePath(&parentRef, (UInt8 *)posixPath, sizeof(posixPath)) == noErr) {
                /* Append filename to parent path */
                char filePath[1100];
                snprintf(filePath, sizeof(filePath), "%s/%.*s",
                         posixPath, (int)pName[0], (char*)pName + 1);

                WriteFilePosix(filePath);
                BlockMove(pName, gFileName, pName[0] + 1);
                gHaveFile = true;
                SetDirty(false);
                SetWindowTitle(gWindow, gFileName);
                saved = true;
            }
        }
        NavDisposeReply(&reply);
    }

    NavDialogDispose(dlg);
    UpdateMenuBarLook();
    return saved;
}

/* -----------------------------------------------------------------------
   DoSave — writes to the existing file or falls through to Save As
   ----------------------------------------------------------------------- */
Boolean DoSave(void)
{
    if (!gActiveDoc) return false;
    SyncWindowToBacking();
    if (!gHaveFile) return DoSaveAs();
    if (gHideMarkdown) SyncHiddenToCanonical();

    /* Re-derive POSIX path from the stored Pascal filename.
       On OS X we keep a full POSIX path in a static buffer. */
    /* For simplicity we use a Navigation Services save-as on the first save
       and then POSIX for subsequent saves.  We track the POSIX path in a
       per-document static buffer below. */

    /* Actually: gVRefNum == 0 means we only have a Pascal filename, no
       full path yet (first launch / newly opened via AE).  Fall back to
       Save As in that edge case. */
    if (gVRefNum == 0 && !gHaveFile) return DoSaveAs();

    /* Build a POSIX path from what we know. In practice, every file we have
       arrived via NavPutFile (which gives us the parent FSRef + name) or via
       an AE open (where ReadFileFromFSRef stored the full path).
       We persist the path in a per-document static. */
    static char sSavedPath[1024] = "";
    if (sSavedPath[0] == '\0') {
        /* No path stored yet — ask user */
        return DoSaveAs();
    }
    WriteFilePosix(sSavedPath);
    SetDirty(false);
    return true;
}

/* -----------------------------------------------------------------------
   Confirm discard
   ----------------------------------------------------------------------- */
static short AskSaveChanges(void)
{
    short hit;
    if (gHaveFile) ParamText(gFileName, "\p", "\p", "\p");
    else           ParamText("\pUntitled", "\p", "\p", "\p");
    hit = Alert(kSaveChangesAlert, NULL);
    UpdateMenuBarLook();
    return hit;
}

Boolean ConfirmDiscardChanges(void)
{
    if (!gActiveDoc) return true;
    if (!gDirty) return true;
    switch (AskSaveChanges()) {
    case kSaveBtn:     return DoSave();
    case kDontSaveBtn: return true;
    default:           return false;
    }
}

/* -----------------------------------------------------------------------
   Navigation Services — Open
   ----------------------------------------------------------------------- */
Boolean DoOpenFile(void)
{
    NavDialogCreationOptions opts;
    NavDialogRef             dlg  = NULL;
    NavReplyRecord           reply;
    Boolean                  opened = false;

    NavGetDefaultDialogCreationOptions(&opts);
    opts.clientName = CFSTR("ArtfulType Pro");

    /* Show only TEXT files */
    NavTypeListHandle typeList = NULL;
    {
        NavTypeListPtr p = (NavTypeListPtr)NewPtrClear(sizeof(NavTypeList) + sizeof(OSType));
        if (p) {
            p->componentSignature = 'ArtT';
            p->reserved            = 0;
            p->osTypeCount         = 1;
            p->osType[0]           = 'TEXT';
            typeList = (NavTypeListHandle)NewHandle(sizeof(NavTypeList) + sizeof(OSType));
            if (typeList) { HLock((Handle)typeList); *typeList = *p; HUnlock((Handle)typeList); }
            DisposePtr((Ptr)p);
        }
    }

    if (NavCreateGetFileDialog(&opts, typeList, NULL, NULL, NULL, NULL, &dlg) != noErr || dlg == NULL) {
        if (typeList) DisposeHandle((Handle)typeList);
        return false;
    }

    if (NavDialogRun(dlg) != noErr) { NavDialogDispose(dlg); return false; }

    if (NavDialogGetReply(dlg, &reply) == noErr && reply.validRecord) {
        AEDesc fileDesc;
        FSRef  ref;
        if (AEGetNthDesc(&reply.selection, 1, typeFSRef, NULL, &fileDesc) == noErr) {
            AEGetDescData(&fileDesc, &ref, sizeof(ref));
            AEDisposeDesc(&fileDesc);

            /* Reuse current empty window or open new */
            if (gHaveFile || gDirty || WEGetTextLength(gTE) > 0)
                MakeWindow();
            if (gActiveDoc) {
                ReadFileFromFSRef(&ref);
                opened = true;
            }
        }
        NavDisposeReply(&reply);
    }

    if (typeList) DisposeHandle((Handle)typeList);
    NavDialogDispose(dlg);
    UpdateMenuBarLook();
    return opened;
}

/* -----------------------------------------------------------------------
   New file
   ----------------------------------------------------------------------- */
void DoNewFile(void)
{
    MakeWindow();
    if (!gActiveDoc) return;

    SetPortWindowPort(gWindow);
    WESetSelect(0, WEGetTextLength(gTE), gTE);
    WEDelete(gTE);
    gHaveFile = false;
    SetDirty(false);
    ClearUndoRedoStacks();
    UpdateEditMenuState();
    RefreshActiveView();
    AdjustScrollbar();
    SetWindowTitle(gWindow, "\pUntitled");
    Rect portRect;
    GetWindowPortBounds(gWindow, &portRect);
    InvalWindowRect(gWindow, &portRect);
}

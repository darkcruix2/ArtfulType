#include "app.h"

static void RefreshActiveView(void)
{
    if (gHideMarkdown)
        BuildHiddenView();
    else
        ClearStyles();
}

void SetViewMode(Boolean hideMarkdown)
{
    if (hideMarkdown == gHideMarkdown)
        return;

    ClearUndoRedoStacks();
    UpdateEditMenuState();
    TEDeactivate(gActiveTE);

    if (hideMarkdown) {
        BuildHiddenView();
        gActiveTE = gHiddenTE;
    } else {
        SyncHiddenToCanonical();
        gActiveTE = gTE;
    }

    TEActivate(gActiveTE);
    gHideMarkdown = hideMarkdown;
    CheckItem(gViewMenu, iMarkdownView, !hideMarkdown);
    CheckItem(gViewMenu, iWriterView, hideMarkdown);
    UpdateMenuBarLook();
    AdjustScrollbar();
    InvalRect(&gWindow->portRect);
}

static void WriteFile(StringPtr name, short vRefNum)
{
    short refNum;
    long count;
    Handle textH = (**gTE).hText;
    OSErr err;

    Create(name, vRefNum, 'ArtT', 'TEXT');

    err = FSOpen(name, vRefNum, &refNum);
    if (err != noErr)
        return;

    SetEOF(refNum, 0);
    count = (**gTE).teLength;
    HLock(textH);
    FSWrite(refNum, &count, *textH);
    HUnlock(textH);
    FSClose(refNum);

    SetWTitle(gWindow, name);
}

static void ReadFile(StringPtr name, short vRefNum)
{
    short refNum;
    long count;
    long eof;
    Handle textH;
    OSErr err;

    SetPort(gWindow);

    err = FSOpen(name, vRefNum, &refNum);
    if (err != noErr)
        return;

    GetEOF(refNum, &eof);
    textH = NewHandle(eof);
    HLock(textH);
    count = eof;
    FSRead(refNum, &count, *textH);
    FSClose(refNum);

    /* Convert UTF-8, LF to CR, and handle CRLF to fix text corruption */
    {
        long i, j = 0;
        
        /* Skip UTF-8 BOM if present */
        if (count >= 3 && (unsigned char)(*textH)[0] == 0xEF && (unsigned char)(*textH)[1] == 0xBB && (unsigned char)(*textH)[2] == 0xBF) {
            i = 3;
        } else {
            i = 0;
        }

        for (; i < count; i++) {
            unsigned char c1 = (unsigned char)(*textH)[i];
            
            /* Handle UTF-8 common punctuation */
            if (c1 == 0xE2 && i + 2 < count) {
                unsigned char c2 = (unsigned char)(*textH)[i+1];
                unsigned char c3 = (unsigned char)(*textH)[i+2];
                if (c2 == 0x80) {
                    if (c3 == 0x98 || c3 == 0x99) { /* smart single quotes */
                        (*textH)[j++] = '\''; i += 2; continue;
                    } else if (c3 == 0x9C || c3 == 0x9D) { /* smart double quotes */
                        (*textH)[j++] = '"'; i += 2; continue;
                    } else if (c3 == 0x93 || c3 == 0x94) { /* en/em dash */
                        (*textH)[j++] = '-'; i += 2; continue;
                    } else if (c3 == 0xA6) { /* ellipsis */
                        (*textH)[j++] = 0xC9; i += 2; continue; /* MacRoman ellipsis */
                    } else if (c3 == 0xA2) { /* bullet */
                        (*textH)[j++] = 0xA5; i += 2; continue; /* MacRoman bullet */
                    }
                }
            }

            if ((*textH)[i] == '\r' && i + 1 < count && (*textH)[i + 1] == '\n') {
                (*textH)[j++] = '\r';
                i++; /* skip the LF */
            } else if ((*textH)[i] == '\n') {
                (*textH)[j++] = '\r';
            } else {
                (*textH)[j++] = (*textH)[i];
            }
        }
        count = j;
    }

    TESetSelect(0, 32767, gTE);
    TEDelete(gTE);
    TEInsert(*textH, count, gTE);
    HUnlock(textH);
    DisposeHandle(textH);

    gDirty = false;
    ClearUndoRedoStacks();
    UpdateEditMenuState();
    RefreshActiveView();
    AdjustScrollbar();
    SetWTitle(gWindow, name);
    InvalRect(&gWindow->portRect);
}

void DoStartupOpen(void)
{
    short message, count;
    AppFile theFile;

    CountAppFiles(&message, &count);
    if (count < 1 || message != appOpen)
        return;

#ifdef ARTFUL_PRO
    for (short i = 1; i <= count; i++) {
        GetAppFiles(i, &theFile);
        if (i > 1 || gHaveFile || gDirty || (**gTE).teLength > 0) {
            MakeWindow();
        }
        if (gActiveDoc) {
            BlockMove(theFile.fName, gFileName, theFile.fName[0] + 1);
            gVRefNum = theFile.vRefNum;
            gHaveFile = true;
            ReadFile(gFileName, gVRefNum);
        }
        ClrAppFiles(i);
    }
#else
    GetAppFiles(1, &theFile);
    BlockMove(theFile.fName, gFileName, theFile.fName[0] + 1);
    gVRefNum = theFile.vRefNum;
    gHaveFile = true;
    ReadFile(gFileName, gVRefNum);
    ClrAppFiles(1);
#endif
}

Boolean DoSaveAs(void)
{
    SFReply reply;
    Point where = {100, 100};

#ifdef ARTFUL_PRO
    if (!gActiveDoc) return false;
#endif

    if (gHideMarkdown)
        SyncHiddenToCanonical();

    SFPutFile(where, "\pSave document as:", "\pUntitled.md", NULL, &reply);
    UpdateMenuBarLook();
    if (!reply.good)
        return false;

    BlockMove(reply.fName, gFileName, reply.fName[0] + 1);
    gVRefNum = reply.vRefNum;
    gHaveFile = true;
    WriteFile(gFileName, gVRefNum);
    gDirty = false;
    return true;
}

Boolean DoSave(void)
{
#ifdef ARTFUL_PRO
    if (!gActiveDoc) return false;
#endif

    if (!gHaveFile)
        return DoSaveAs();

    if (gHideMarkdown)
        SyncHiddenToCanonical();

    WriteFile(gFileName, gVRefNum);
    gDirty = false;
    return true;
}

static short AskSaveChanges(void)
{
    short hit;

    if (gHaveFile)
        ParamText(gFileName, "\p", "\p", "\p");
    else
        ParamText("\pUntitled", "\p", "\p", "\p");

    hit = Alert(kSaveChangesAlert, NULL);
    UpdateMenuBarLook();
    return hit;
}

Boolean ConfirmDiscardChanges(void)
{
#ifdef ARTFUL_PRO
    if (!gActiveDoc) return true;
#endif

    if (!gDirty)
        return true;

    switch (AskSaveChanges()) {
        case kSaveBtn:     return DoSave();
        case kDontSaveBtn: return true;
        default:            return false;
    }
}

Boolean DoOpenFile(void)
{
    SFReply reply;
    Point where = {100, 100};
    SFTypeList types;

    types[0] = 'TEXT';

    SFGetFile(where, "\p", NULL, 1, types, NULL, &reply);
    UpdateMenuBarLook();
    if (!reply.good)
        return false;

#ifdef ARTFUL_PRO
    if (gHaveFile || gDirty || (**gTE).teLength > 0) {
        MakeWindow();
        if (!gActiveDoc) return false;
    }
#endif

    BlockMove(reply.fName, gFileName, reply.fName[0] + 1);
    gVRefNum = reply.vRefNum;
    gHaveFile = true;
    ReadFile(gFileName, gVRefNum);
    return true;
}

void DoNewFile(void)
{
#ifdef ARTFUL_PRO
    MakeWindow();
    if (!gActiveDoc) return;
#endif

    SetPort(gWindow);
    TESetSelect(0, 32767, gTE);
    TEDelete(gTE);
    gHaveFile = false;
    gDirty = false;
    ClearUndoRedoStacks();
    UpdateEditMenuState();
    RefreshActiveView();
    AdjustScrollbar();
    SetWTitle(gWindow, "\pUntitled");
    InvalRect(&gWindow->portRect);
}

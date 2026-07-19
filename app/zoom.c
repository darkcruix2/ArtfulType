#include "app.h"

/* Zoom levels (point deltas from FONT_SIZE). 12/14/18/24pt have a real
   Times bitmap -- confirmed by reading the FOND resource directly rather
   than assuming. The 30pt level has no native bitmap (24pt is the
   largest this font has) and renders as a scaled enlargement of the
   24pt bitmap instead -- a known, accepted tradeoff for going bigger. */
static short kZoomLevels[] = { -4, 0, 6 };

short CurrentFontSize(void)
{
    short size = FONT_SIZE + kZoomLevels[gZoomIndex];
    if (size < 9) size = 9;
    return size;
}

void LoadZoomPref(void)
{
    Handle prefH = GetResource(kZoomPrefType, kZoomPrefID);
    short loadedPref = kZoomDefaultIndex;

    if (prefH != NULL) {
        HLock(prefH);
        loadedPref = *(short *) *prefH;
        HUnlock(prefH);
        ReleaseResource(prefH);
        if (loadedPref < 0 || loadedPref >= kNumZoomLevels)
            loadedPref = kZoomDefaultIndex;
    }
    
    gDefaultZoomIndex = loadedPref;
#ifndef ARTFUL_PRO
    gZoomIndex = loadedPref;
#endif
}

static void SaveZoomPref(void)
{
    Handle prefH = GetResource(kZoomPrefType, kZoomPrefID);

    if (prefH != NULL) {
        HLock(prefH);
        *(short *) *prefH = gZoomIndex;
        HUnlock(prefH);
        ChangedResource(prefH);
        WriteResource(prefH);
        ReleaseResource(prefH);
    }
}

void ApplyZoomIndex(short newIndex)
{
    long selStart, selEnd;

    if (newIndex < 0 || newIndex >= kNumZoomLevels || newIndex == gZoomIndex)
        return;

    WEGetSelection(&selStart, &selEnd, gActiveTE);

    // Sync current changes to backing store BEFORE changing font size, 
    // so headers are properly identified based on the OLD font size.
    SyncWindowToBacking();

    gZoomIndex = newIndex;
    
    // Load window from backing store. This will automatically rebuild all styles
    // (headers, bold, etc.) based on the NEW font size!
    LoadTextWindow(gWindowStart);
    
    WESetSelect(selStart, selEnd, gActiveTE);
    ScrollCaretIntoView(false);

    SaveZoomPref();
    AdjustScrollbar();
    InvalRect(&gWindow->portRect);
}

void DoZoom(short direction)
{
    ApplyZoomIndex(gZoomIndex + direction);
}

void DoZoomReset(void)
{
    ApplyZoomIndex(kZoomDefaultIndex);
}

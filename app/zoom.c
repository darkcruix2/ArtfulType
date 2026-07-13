#include "app.h"

/* Zoom levels (point deltas from FONT_SIZE). 12/14/18/24pt have a real
   Times bitmap -- confirmed by reading the FOND resource directly rather
   than assuming. The 30pt level has no native bitmap (24pt is the
   largest this font has) and renders as a scaled enlargement of the
   24pt bitmap instead -- a known, accepted tradeoff for going bigger. */
static short kZoomLevels[] = { -6, -4, 0, 6, 12 };

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

/*
    Remaps any run whose size matches one of the OLD base/heading sizes
    to the corresponding NEW size, in place -- used for zoom, so it
    never re-parses markdown and can't clobber unsynced edits in
    whichever buffer isn't currently canonical.
*/
static void RescaleStyles(WEHandle te, short oldBase, short newBase)
{
    long len = WEGetTextLength(te);
    long i = 0;
    long selStart, selEnd;
    WEGetSelection(&selStart, &selEnd, te);
    long savedStart = selStart;
    long savedEnd = selEnd;

    while (i < len) {
        WETextStyle st;
        long runStart = i;
        short oldSize;
        short newSize;

        WEGetStyle(i, &st, te);
        oldSize = st.tsSize;

        while (i < len) {
            WETextStyle st2;

            WEGetStyle(i, &st2, te);
            if (st2.tsSize != oldSize)
                break;
            i++;
        }

        if (oldSize == oldBase) newSize = newBase;
        else if (oldSize == oldBase + 12) newSize = newBase + 12;
        else if (oldSize == oldBase + 8) newSize = newBase + 8;
        else if (oldSize == oldBase + 4) newSize = newBase + 4;
        else newSize = oldSize + (newBase - oldBase);

        if (newSize != oldSize) {
            WETextStyle ts;
            ts.tsSize = newSize;
            WESetSelect(runStart, i, te);
            WESetStyle(weDoSize, &ts, te);
        }
    }

    WESetSelect(savedStart, savedEnd, te);
}

static void ApplyZoomIndex(short newIndex)
{
    short oldBase;
    short newBase;

    if (newIndex < 0 || newIndex >= kNumZoomLevels || newIndex == gZoomIndex)
        return;

    oldBase = CurrentFontSize();
    gZoomIndex = newIndex;
    newBase = CurrentFontSize();

    ClearStyles();
    RescaleStyles(gHiddenTE, oldBase, newBase);
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

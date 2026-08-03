/*
 * zoom.c — ArtfulType Pro  (Carbon / OS X 10.4 port)
 *
 * Logic identical to classic version.  Changes:
 *   • GetResource / ChangedResource / WriteResource — still work in Carbon
 *     when the app has a resource fork.  On OS X we keep the resource fork
 *     inside the .app bundle (Contents/Resources/application.rsrc) by
 *     convention.  The ZLvl pref resource continues to work.
 *   • InvalRect → InvalWindowRect
 */

#include "app.h"

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
    short  loaded = kZoomDefaultIndex;

    if (prefH != NULL) {
        HLock(prefH);
        loaded = *(short *)*prefH;
        HUnlock(prefH);
        ReleaseResource(prefH);
        if (loaded < 0 || loaded >= kNumZoomLevels)
            loaded = kZoomDefaultIndex;
    }
    gDefaultZoomIndex = loaded;
}

static void SaveZoomPref(void)
{
    Handle prefH = GetResource(kZoomPrefType, kZoomPrefID);
    if (prefH != NULL) {
        HLock(prefH);
        *(short *)*prefH = gZoomIndex;
        HUnlock(prefH);
        ChangedResource(prefH);
        WriteResource(prefH);
        ReleaseResource(prefH);
    }
}

void ApplyZoomIndex(short newIndex)
{
    Rect portRect;
    long selStart, selEnd;

    if (!gHideMarkdown) return;
    if (newIndex < 0 || newIndex >= kNumZoomLevels || newIndex == gZoomIndex) return;

    WEGetSelection(&selStart, &selEnd, gActiveTE);
    SyncWindowToBacking();
    gZoomIndex = newIndex;
    LoadTextWindow(gWindowStart);
    WESetSelect(selStart, selEnd, gActiveTE);
    ScrollCaretIntoView(false);
    SaveZoomPref();
    AdjustScrollbar();
    GetWindowPortBounds(gWindow, &portRect);
    InvalWindowRect(gWindow, &portRect);
}

void DoZoom(short direction)     { ApplyZoomIndex(gZoomIndex + direction); }
void DoZoomReset(void)           { ApplyZoomIndex(kZoomDefaultIndex); }

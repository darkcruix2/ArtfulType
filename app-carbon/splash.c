/*
 * splash.c — ArtfulType Pro  (Carbon / OS X 10.4 port)
 *
 * Changes from classic version:
 *   • qd.screenBits → GetQDGlobalsScreenBits()
 *   • SetPort(dlg) / GetPort → GetPortWindowPort() etc.
 *   • CopyBits portBits access → GetPortBitMapForCopyBits()
 *   • SetDialogItem with UserItemUPP → same API, still valid in Carbon
 *   • GetNewDialog still valid in Carbon when resource fork present
 *   • ModalDialog still valid in Carbon
 */

#include "app.h"
#include "splash_image.h"

#define kSplashImageWidth    128
#define kSplashImageHeight   100
#define kSplashImageRowBytes (kSplashImageWidth / 8)

static const unsigned char kVersionString[] = "\pv0.25";
static const unsigned char kGitHubURL[]     = "\pgithub.com/ActionRetro";

static pascal void DrawSplashTitle(DialogRef dlg, short itemNo)
{
    DialogItemType type;
    Handle         itemH;
    Rect           box;
    short          textWidth;
    Str255         s;
    BitMap         image;
    Rect           imageRect;

    GetDialogItem(dlg, itemNo, &type, &itemH, &box);
    SetPortDialogPort(dlg);

    TextFont(0); TextSize(0); TextFace(bold);
    BlockMove("\pArtfulType Pro", s, 15);
    textWidth = StringWidth(s);
    MoveTo(box.left + (box.right - box.left - textWidth) / 2, box.top + 18);
    DrawString(s);

    image.baseAddr = (Ptr)kSplashImageBits;
    image.rowBytes = kSplashImageRowBytes;
    SetRect(&image.bounds, 0, 0, kSplashImageWidth, kSplashImageHeight);
    SetRect(&imageRect,    0, 0, kSplashImageWidth, kSplashImageHeight);
    OffsetRect(&imageRect,
               box.left + (box.right  - box.left - kSplashImageWidth)  / 2,
               box.top  + 28);

    /* Carbon: use GetPortBitMapForCopyBits to get the port's BitMap */
    CopyBits(&image,
             GetPortBitMapForCopyBits(GetDialogPort(dlg)),
             &image.bounds, &imageRect, srcCopy, NULL);

    TextFace(normal); TextSize(9);
    BlockMove("\pA Distraction-Free Writing Environment", s, 39);
    textWidth = StringWidth(s);
    MoveTo(box.left + (box.right - box.left - textWidth) / 2, box.top + 144);
    DrawString(s);

    BlockMove(kVersionString, s, kVersionString[0] + 1);
    textWidth = StringWidth(s);
    MoveTo(box.left + (box.right - box.left - textWidth) / 2, box.top + 158);
    DrawString(s);

    TextFace(bold);
    BlockMove(kGitHubURL, s, kGitHubURL[0] + 1);
    textWidth = StringWidth(s);
    MoveTo(box.left + (box.right - box.left - textWidth) / 2, box.top + 172);
    DrawString(s);
    TextFace(normal);
}

/* Centre the dialog on screen before showing it */
static void CenterAndShowDialog(DialogRef dlg)
{
    BitMap screenBits;
    Rect   screenBounds, dlgBounds;
    short  dlgWidth, dlgHeight, newLeft, newTop;

    GetQDGlobalsScreenBits(&screenBits);
    screenBounds      = screenBits.bounds;
    screenBounds.top += MENU_BAR_HEIGHT;

    GetWindowPortBounds(GetDialogWindow(dlg), &dlgBounds);
    dlgWidth  = dlgBounds.right  - dlgBounds.left;
    dlgHeight = dlgBounds.bottom - dlgBounds.top;
    newLeft   = screenBounds.left + (screenBounds.right  - screenBounds.left - dlgWidth)  / 2;
    newTop    = screenBounds.top  + (screenBounds.bottom - screenBounds.top  - dlgHeight) / 2;

    MoveWindow(GetDialogWindow(dlg), newLeft, newTop, false);
    ShowWindow(GetDialogWindow(dlg));
}

void ShowSplashScreen(void)
{
    DialogRef      dlg;
    short          item;
    DialogItemType type;
    Handle         itemH;
    Rect           box;
    Boolean        done = false;

    while (!done) {
        dlg = GetNewDialog(kSplashDialog, NULL, (WindowRef)-1L);
        if (!dlg) return;
        CenterAndShowDialog(dlg);

        GetDialogItem(dlg, iSplashTitle, &type, &itemH, &box);
        SetDialogItem(dlg, iSplashTitle, type,
                      (Handle)NewUserItemUPP(DrawSplashTitle), &box);

        do { ModalDialog(NULL, &item); }
        while (item != iSplashNew && item != iSplashOpen);

        DisposeDialog(dlg);
        SetPortWindowPort(gWindow);
        UpdateMenuBarLook();

        done = (item == iSplashNew) || DoOpenFile();
    }
}

void ShowAboutBox(void)
{
    DialogRef      dlg;
    short          item;
    DialogItemType type;
    Handle         itemH;
    Rect           box;

    dlg = GetNewDialog(kAboutDialog, NULL, (WindowRef)-1L);
    if (!dlg) return;
    CenterAndShowDialog(dlg);

    GetDialogItem(dlg, iAboutTitle, &type, &itemH, &box);
    SetDialogItem(dlg, iAboutTitle, type,
                  (Handle)NewUserItemUPP(DrawSplashTitle), &box);

    do { ModalDialog(NULL, &item); } while (item != iAboutOK);

    DisposeDialog(dlg);
    SetPortWindowPort(gWindow);
    UpdateMenuBarLook();
}

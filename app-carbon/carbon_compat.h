/*
 * carbon_compat.h
 * ArtfulType Carbon Port
 *
 * Compatibility bridge between the classic Retro68 world and Carbon/OS X 10.4.
 * Include this first in every source file INSTEAD of Retro68 system headers.
 */

#ifndef CARBON_COMPAT_H
#define CARBON_COMPAT_H

/* The single Carbon umbrella header provides everything we need:
   QuickDraw, Windows, Menus, TextEdit, Dialogs, Events, Memory,
   Files, Navigation Services, MLTE, Apple Events, etc. */
#include <Carbon/Carbon.h>

/* Retro68 / classic Mac OS used LONGINT as a synonym for long.
   Standard C has long; define it away. */
#ifndef LONGINT
# define LONGINT long
#endif

/* Classic Mac Pascal-string helpers that may not be in a modern SDK */
#ifndef BlockMove
# define BlockMove(src, dst, len)  memmove((dst), (src), (size_t)(len))
#endif

/* Retro68 GetFNum is still present in Carbon but deprecated.
   We keep using it because we want runtime font lookup by name. */

/* Classic SFGetFile / SFPutFile are gone in Carbon — we replace them
   with Navigation Services wrappers in file.c. Do NOT redeclare them. */

/* OpenDeskAcc is gone in Carbon — no replacement needed (desk accessories
   are not a thing on OS X). Remove all calls to it. */

/* CountAppFiles / GetAppFiles / ClrAppFiles are gone in Carbon.
   Startup file opens come via Apple Event kAEOpenDocuments instead. */

/* qd (the QuickDraw globals struct) is gone in Carbon.
   Use accessor functions: GetQDGlobalsScreenBits(), etc. */

/* TEInit, InitGraf, InitFonts, InitWindows, InitMenus, InitDialogs,
   InitCursor are all gone / no-ops in Carbon.  RegisterAppearanceClient()
   and TXNInitTextension() replace them. */

/* AppendResMenu with 'DRVR' is a no-op in Carbon — desk accessories
   don't exist on OS X. We just skip that call. */

/* verUS region constant for resource fork — define it if missing */
#ifndef verUS
# define verUS 0
#endif

#endif /* CARBON_COMPAT_H */

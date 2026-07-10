#include "Finder.r"
#include "Dialogs.r"

resource 'DITL' (130) {
    {
        {80, 204, 100, 284}, Button { enabled, "Save" },
        {80, 114, 100, 194}, Button { enabled, "Cancel" },
        {80, 14, 100, 104}, Button { enabled, "Don't Save" },
        {10, 14, 70, 284}, StaticText { disabled, "Save changes to \"^0\" before continuing?" }
    }
};

resource 'ALRT' (130) {
    {116, 106, 226, 406},
    130,
    {
        OK, visible, sound1,
        OK, visible, sound1,
        OK, visible, sound1,
        OK, visible, sound1
    },
    alertPositionMainScreen
};

data 'ZLvl' (128) {
    $"0002"
};

resource 'DITL' (131) {
    {
        {206, 60, 228, 180}, Button { enabled, "New Document" },
        {206, 200, 228, 320}, Button { enabled, "Open Document" },
        {12, 15, 196, 365}, UserItem { disabled }
    }
};

resource 'DLOG' (131) {
    {61, 66, 301, 446},
    dBoxProc,
    invisible,
    noGoAway,
    0,
    131,
    "",
    noAutoCenter
};

resource 'DITL' (132) {
    {
        {75, 165, 97, 245}, Button { enabled, "OK" },
        {75, 75, 97, 155}, Button { enabled, "Cancel" },
        {15, 20, 33, 300}, StaticText { disabled, "Link URL:" },
        {38, 20, 58, 300}, EditText { enabled, "" }
    }
};

resource 'DLOG' (132) {
    {126, 96, 236, 416},
    dBoxProc,
    visible,
    noGoAway,
    0,
    132,
    "",
    noAutoCenter
};

resource 'DITL' (133) {
    {
        {206, 140, 228, 240}, Button { enabled, "OK" },
        {12, 15, 196, 365}, UserItem { disabled }
    }
};

resource 'DLOG' (133) {
    {61, 66, 301, 446},
    dBoxProc,
    invisible,
    noGoAway,
    0,
    133,
    "",
    noAutoCenter
};

resource 'ICN#' (128) {
    {
        $"00000000000000000000000000000000"
        $"003FFF00004000800180008003FFFFC0"
        $"00200040002FFE8000200080004FFC80"
        $"004001000080010007FFFFE03FFFFFFC"
        $"2E0000742EFFFF743EFFFF7C07FFFFE0"
        $"044444201FFFFFF83FFFFFFC3000000C"
        $"3FFFFFFC1FFFFFF80000000000000000"
        $"00000000000000000000000000000000",
        $"00000000000000000000000000000000"
        $"003FFF00007FFF8001FFFF8003FFFFC0"
        $"003FFFC0003FFF80003FFF80007FFF80"
        $"007FFF0000FFFF000FFFFFF03FFFFFFC"
        $"3FFFFFFC3FFFFFFC3FFFFFFC0FFFFFF0"
        $"0FFFFFF03FFFFFFC3FFFFFFC3FFFFFFC"
        $"3FFFFFFC3FFFFFFC0000000000000000"
        $"00000000000000000000000000000000"
    }
};

resource 'FREF' (128) {
    'APPL', 0, ""
};

resource 'FREF' (129) {
    'TEXT', 0, ""
};

resource 'BNDL' (128) {
    'ArtT', 0,
    {
        'ICN#', {
            0, 128
        },
        'FREF', {
            0, 128,
            1, 129
        }
    }
};

resource 'DITL' (134) {
    {
        {75, 165, 97, 245}, Button { enabled, "Find" },
        {75, 75, 97, 155}, Button { enabled, "Cancel" },
        {15, 20, 33, 300}, StaticText { disabled, "Search for:" },
        {38, 20, 58, 300}, EditText { enabled, "" }
    }
};

resource 'DLOG' (134) {
    {126, 96, 236, 416},
    dBoxProc,
    visible,
    noGoAway,
    0,
    134,
    "",
    noAutoCenter
};

resource 'DITL' (135) {
    {
        {125, 110, 147, 190}, Button { enabled, "Replace" },
        {125, 15, 147, 95}, Button { enabled, "Cancel" },
        {15, 20, 33, 300}, StaticText { disabled, "Find:" },
        {38, 20, 58, 300}, EditText { enabled, "" },
        {65, 20, 83, 300}, StaticText { disabled, "Replace with:" },
        {88, 20, 108, 300}, EditText { enabled, "" },
        {125, 205, 147, 305}, Button { enabled, "Replace All" }
    }
};


resource 'DLOG' (135) {
    {126, 96, 286, 416},
    dBoxProc,
    visible,
    noGoAway,
    0,
    135,
    "",
    noAutoCenter
};


resource 'SIZE' (-1) {
    false, // reserved
    acceptSuspendResumeEvents,
    false, // reserved
    canBackground,
    multiFinderAware,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,
    isHighLevelEventAware,
    localAndRemoteHLEvents,
    isStationeryAware,
    useTextEditServices,
    notDisplayManagerAware,
    false, // reserved
    false, // reserved
    5000 * 1024,
    5000 * 1024
};

resource 'vers' (1, purgeable) {
    0x00, 0x15, release, 0x00,
    verUS,
    "0.15",
    "0.15, © 2026 Space Mil"
};

resource 'vers' (2, purgeable) {
    0x00, 0x15, release, 0x00,
    verUS,
    "0.15",
    "ArtfulType Pro 0.15"
};

// joystick.cpp — the heart of the shim.
//
// We expose the classic WinMM joystick API (the path UE1's WinDrv polls via
// joyGetPosEx). Joystick id 0 is our *virtual* device, synthesised from a real
// modern controller through SDL2. Any other id (1..15) is delegated to the
// genuine winmm so physical legacy joysticks keep working.
//
// Everything (deadzones, axis routing, button map, trigger handling, invert,
// right-stick sensitivity) is read from sdljoy.ini at first use.

#include "proxy.h"
#include "config.h"
#include <SDL.h>
#include <mutex>
#include <string>
#include <cstring>

// ---- lazy init state -------------------------------------------------------
static std::once_flag    g_once;
static bool              g_ready       = false;
static SDL_GameController* g_pad        = nullptr;
static int               g_padIndex    = 0;

// cached config
struct Settings {
    int  controllerIndex = 0;
    bool background       = true;
    int  leftDeadzone     = 6000;
    int  rightDeadzone    = 6000;
    bool invertLeftY      = false;
    bool invertRightY     = true;     // UE1 "look up" usually feels inverted
    int  rightSensPct     = 100;      // tame the famously twitchy right stick
    std::string triggerMode = "buttons"; // "buttons" | "axes"
    int  triggerThreshold = 8000;
    int  leftTrigBtn      = 7;        // Joy7
    int  rightTrigBtn     = 8;        // Joy8
    // SDL_CONTROLLER_BUTTON_* -> Joy index (1-based; 0 = unmapped)
    int  btn[SDL_CONTROLLER_BUTTON_MAX] = {0};
    // axis routing: which JOYINFOEX field each stick axis drives
    char leftX = 'X', leftY = 'Y', rightX = 'R', rightY = 'U';
} S;

// delegation pointers for non-virtual ids
typedef MMRESULT (WINAPI *PFN_joyGetPosEx)(UINT, LPJOYINFOEX);
typedef MMRESULT (WINAPI *PFN_joyGetPos)(UINT, LPJOYINFO);
typedef MMRESULT (WINAPI *PFN_joyGetDevCapsA)(UINT, LPJOYCAPSA, UINT);
typedef MMRESULT (WINAPI *PFN_joyGetDevCapsW)(UINT, LPJOYCAPSW, UINT);
typedef UINT     (WINAPI *PFN_joyGetNumDevs)(void);
static PFN_joyGetPosEx     real_joyGetPosEx     = nullptr;
static PFN_joyGetPos       real_joyGetPos       = nullptr;
static PFN_joyGetDevCapsA  real_joyGetDevCapsA  = nullptr;
static PFN_joyGetDevCapsW  real_joyGetDevCapsW  = nullptr;
static PFN_joyGetNumDevs   real_joyGetNumDevs   = nullptr;

static char routeChar(const std::string& s, char def) {
    if (s.empty()) return def;
    char c = (char)toupper((unsigned char)s[0]);
    return (c=='X'||c=='Y'||c=='Z'||c=='R'||c=='U'||c=='V') ? c : def;
}

static int btnNameToEnum(const std::string& n) {
    // map ini keys to SDL_CONTROLLER_BUTTON_* (SDL's own string table)
    SDL_GameControllerButton b = SDL_GameControllerGetButtonFromString(n.c_str());
    return (int)b; // -1 if invalid
}

static std::wstring thisDllDir() {
    wchar_t buf[MAX_PATH] = {0};
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&thisDllDir, &self);
    GetModuleFileNameW(self, buf, MAX_PATH);
    std::wstring p(buf);
    size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}

static void loadSettings() {
    std::wstring dir = thisDllDir();
    cfg::load(dir);
    S.controllerIndex = cfg::getInt("general", "controller_index", 0);
    S.background      = cfg::getBool("general", "poll_background", true);
    S.leftDeadzone    = cfg::getInt("sticks", "left_deadzone", 6000);
    S.rightDeadzone   = cfg::getInt("sticks", "right_deadzone", 6000);
    S.invertLeftY     = cfg::getBool("sticks", "invert_left_y", false);
    S.invertRightY    = cfg::getBool("sticks", "invert_right_y", true);
    S.rightSensPct    = cfg::getInt("sticks", "right_sensitivity", 100);
    S.triggerMode     = cfg::getStr("triggers", "mode", "buttons");
    S.triggerThreshold= cfg::getInt("triggers", "threshold", 8000);
    S.leftTrigBtn     = cfg::getInt("triggers", "left_button", 7);
    S.rightTrigBtn    = cfg::getInt("triggers", "right_button", 8);
    S.leftX  = routeChar(cfg::getStr("axes","left_x","X"), 'X');
    S.leftY  = routeChar(cfg::getStr("axes","left_y","Y"), 'Y');
    S.rightX = routeChar(cfg::getStr("axes","right_x","R"), 'R');
    S.rightY = routeChar(cfg::getStr("axes","right_y","U"), 'U');

    // default button map, then override from [buttons]
    static const char* defs[][2] = {
        {"a","1"},{"b","2"},{"x","3"},{"y","4"},
        {"leftshoulder","5"},{"rightshoulder","6"},
        {"back","9"},{"start","10"},{"leftstick","11"},{"rightstick","12"},
        {"dpup","0"},{"dpdown","0"},{"dpleft","0"},{"dpright","0"},{"guide","0"},
    };
    for (auto& d : defs) {
        int e = btnNameToEnum(d[0]);
        if (e >= 0 && e < SDL_CONTROLLER_BUTTON_MAX)
            S.btn[e] = cfg::getInt("buttons", d[0], atoi(d[1]));
    }
}

static void ensureInit() {
    std::call_once(g_once, []{
        loadSettings();
        if (S.background)
            SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) return;
        // optional community mapping DB dropped next to the dll
        std::wstring db = thisDllDir() + L"\\gamecontrollerdb.txt";
        char dbA[MAX_PATH]; size_t conv = 0;
        wcstombs_s(&conv, dbA, db.c_str(), _TRUNCATE);
        SDL_GameControllerAddMappingsFromFile(dbA);
        g_padIndex = S.controllerIndex;
        g_ready = true;
    });
}

// Open / re-open the controller (handles hot-plug each poll).
static SDL_GameController* pad() {
    if (g_pad && SDL_GameControllerGetAttached(g_pad)) return g_pad;
    if (g_pad) { SDL_GameControllerClose(g_pad); g_pad = nullptr; }
    int seen = 0;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) continue;
        if (seen++ == g_padIndex) { g_pad = SDL_GameControllerOpen(i); break; }
    }
    return g_pad;
}

// SDL axis (-32768..32767) -> WinMM field (0..65535) with deadzone + invert.
static DWORD toField(int v, int deadzone, bool invert, int sensPct) {
    if (v > -deadzone && v < deadzone) v = 0;
    if (sensPct != 100) v = (int)((long long)v * sensPct / 100);
    if (v < -32768) v = -32768; if (v > 32767) v = 32767;
    if (invert) v = -v - (v == -32768 ? 1 : 0);
    return (DWORD)(v + 32768); // 0..65535, centre 32768
}

static void writeAxis(LPJOYINFOEX p, char field, DWORD val) {
    switch (field) {
        case 'X': p->dwXpos = val; break;
        case 'Y': p->dwYpos = val; break;
        case 'Z': p->dwZpos = val; break;
        case 'R': p->dwRpos = val; break;
        case 'U': p->dwUpos = val; break;
        case 'V': p->dwVpos = val; break;
    }
}

static DWORD povFromDpad(SDL_GameController* c) {
    bool up    = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP);
    bool down  = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    bool left  = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    bool right = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    if (up && right) return 4500;
    if (right && down) return 13500;
    if (down && left) return 22500;
    if (left && up) return 31500;
    if (up) return 0;
    if (right) return 9000;
    if (down) return 18000;
    if (left) return 27000;
    return (DWORD)JOY_POVCENTERED; // 0xFFFF
}

// Fill all six axes + buttons + POV for our virtual device.
static MMRESULT fillVirtual(LPJOYINFOEX pji) {
    SDL_GameControllerUpdate();
    SDL_GameController* c = pad();
    if (!c) return JOYERR_UNPLUGGED;

    // centre every axis first
    pji->dwXpos = pji->dwYpos = pji->dwZpos = 32768;
    pji->dwRpos = pji->dwUpos = pji->dwVpos = 32768;

    writeAxis(pji, S.leftX,  toField(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX),  S.leftDeadzone,  false,          100));
    writeAxis(pji, S.leftY,  toField(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY),  S.leftDeadzone,  S.invertLeftY,  100));
    writeAxis(pji, S.rightX, toField(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX), S.rightDeadzone, false,          S.rightSensPct));
    writeAxis(pji, S.rightY, toField(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY), S.rightDeadzone, S.invertRightY, S.rightSensPct));

    DWORD buttons = 0;
    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b) {
        int joy = S.btn[b];
        if (joy >= 1 && SDL_GameControllerGetButton(c, (SDL_GameControllerButton)b))
            buttons |= (1u << (joy - 1));
    }

    // triggers
    int lt = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    int rt = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    if (S.triggerMode == "axes") {
        // triggers as the two leftover axes (Z / V) — 0..65535
        pji->dwZpos = (DWORD)(lt * 2);   // SDL trigger is 0..32767
        pji->dwVpos = (DWORD)(rt * 2);
    } else { // buttons (this is what makes LT/RT bindable, the whole point)
        if (lt > S.triggerThreshold && S.leftTrigBtn  >= 1) buttons |= (1u << (S.leftTrigBtn  - 1));
        if (rt > S.triggerThreshold && S.rightTrigBtn >= 1) buttons |= (1u << (S.rightTrigBtn - 1));
    }

    pji->dwButtons = buttons;
    pji->dwButtonNumber = 0;
    pji->dwPOV = povFromDpad(c);
    return JOYERR_NOERROR;
}

// --------------------------------------------------------------------------
//  Exported WinMM joystick API
// --------------------------------------------------------------------------
extern "C" {

MMRESULT WINAPI joyGetPosEx(UINT uJoyID, LPJOYINFOEX pji)
{
    ensureInit();
    if (!pji || pji->dwSize < sizeof(JOYINFOEX)) return JOYERR_PARMS;
    if (uJoyID == (UINT)g_padIndex || uJoyID == 0)
        return fillVirtual(pji);
    if (!real_joyGetPosEx) real_joyGetPosEx = (PFN_joyGetPosEx)GetProcAddress(g_realWinmm, "joyGetPosEx");
    return real_joyGetPosEx ? real_joyGetPosEx(uJoyID, pji) : JOYERR_UNPLUGGED;
}

MMRESULT WINAPI joyGetPos(UINT uJoyID, LPJOYINFO pji)
{
    ensureInit();
    if (!pji) return JOYERR_PARMS;
    if (uJoyID == (UINT)g_padIndex || uJoyID == 0) {
        JOYINFOEX ex; memset(&ex, 0, sizeof(ex)); ex.dwSize = sizeof(ex); ex.dwFlags = JOY_RETURNALL;
        MMRESULT r = fillVirtual(&ex);
        if (r != JOYERR_NOERROR) return r;
        pji->wXpos = (UINT)ex.dwXpos; pji->wYpos = (UINT)ex.dwYpos;
        pji->wZpos = (UINT)ex.dwZpos; pji->wButtons = ex.dwButtons;
        return JOYERR_NOERROR;
    }
    if (!real_joyGetPos) real_joyGetPos = (PFN_joyGetPos)GetProcAddress(g_realWinmm, "joyGetPos");
    return real_joyGetPos ? real_joyGetPos(uJoyID, pji) : JOYERR_UNPLUGGED;
}

UINT WINAPI joyGetNumDevs(void)
{
    // Report the real max (16) so games happily probe id 0 = our virtual pad.
    if (!real_joyGetNumDevs) real_joyGetNumDevs = (PFN_joyGetNumDevs)GetProcAddress(g_realWinmm, "joyGetNumDevs");
    UINT real = real_joyGetNumDevs ? real_joyGetNumDevs() : 0;
    return real ? real : 16;
}

static void fillCaps(JOYCAPSW* w) {
    memset(w, 0, sizeof(*w));
    wcsncpy_s(w->szPname, L"SDL Virtual Controller", _TRUNCATE);
    w->wXmin = w->wYmin = w->wZmin = w->wRmin = w->wUmin = w->wVmin = 0;
    w->wXmax = w->wYmax = w->wZmax = w->wRmax = w->wUmax = w->wVmax = 65535;
    w->wNumButtons = 16; w->wMaxButtons = 32;
    w->wNumAxes = 6; w->wMaxAxes = 6;
    w->wCaps = JOYCAPS_HASZ | JOYCAPS_HASR | JOYCAPS_HASU | JOYCAPS_HASV |
               JOYCAPS_HASPOV | JOYCAPS_POVCTS;
}

MMRESULT WINAPI joyGetDevCapsW(UINT id, LPJOYCAPSW pjc, UINT cb)
{
    ensureInit();
    if (id == (UINT)g_padIndex || id == 0) {
        if (!pjc || cb < sizeof(JOYCAPSW)) return JOYERR_PARMS;
        fillCaps(pjc); return JOYERR_NOERROR;
    }
    if (!real_joyGetDevCapsW) real_joyGetDevCapsW = (PFN_joyGetDevCapsW)GetProcAddress(g_realWinmm, "joyGetDevCapsW");
    return real_joyGetDevCapsW ? real_joyGetDevCapsW(id, pjc, cb) : JOYERR_UNPLUGGED;
}

MMRESULT WINAPI joyGetDevCapsA(UINT id, LPJOYCAPSA pjc, UINT cb)
{
    ensureInit();
    if (id == (UINT)g_padIndex || id == 0) {
        if (!pjc || cb < sizeof(JOYCAPSA)) return JOYERR_PARMS;
        JOYCAPSW w; fillCaps(&w);
        memset(pjc, 0, sizeof(*pjc));
        WideCharToMultiByte(CP_ACP, 0, w.szPname, -1, pjc->szPname, sizeof(pjc->szPname), 0, 0);
        pjc->wXmin=w.wXmin; pjc->wXmax=w.wXmax; pjc->wYmin=w.wYmin; pjc->wYmax=w.wYmax;
        pjc->wZmin=w.wZmin; pjc->wZmax=w.wZmax; pjc->wRmin=w.wRmin; pjc->wRmax=w.wRmax;
        pjc->wUmin=w.wUmin; pjc->wUmax=w.wUmax; pjc->wVmin=w.wVmin; pjc->wVmax=w.wVmax;
        pjc->wNumButtons=w.wNumButtons; pjc->wMaxButtons=w.wMaxButtons;
        pjc->wNumAxes=w.wNumAxes; pjc->wMaxAxes=w.wMaxAxes; pjc->wCaps=w.wCaps;
        return JOYERR_NOERROR;
    }
    if (!real_joyGetDevCapsA) real_joyGetDevCapsA = (PFN_joyGetDevCapsA)GetProcAddress(g_realWinmm, "joyGetDevCapsA");
    return real_joyGetDevCapsA ? real_joyGetDevCapsA(id, pjc, cb) : JOYERR_UNPLUGGED;
}

// Threshold / capture: harmless for our virtual pad, delegate for real ids.
MMRESULT WINAPI joyGetThreshold(UINT id, LPUINT t)  { if (t) *t = 0; return JOYERR_NOERROR; }
MMRESULT WINAPI joySetThreshold(UINT id, UINT t)    { return JOYERR_NOERROR; }
MMRESULT WINAPI joyConfigChanged(DWORD f)           { return JOYERR_NOERROR; }
MMRESULT WINAPI joySetCapture(HWND h, UINT id, UINT period, BOOL changed) { return JOYERR_NOERROR; }
MMRESULT WINAPI joyReleaseCapture(UINT id)          { return JOYERR_NOERROR; }

} // extern "C"

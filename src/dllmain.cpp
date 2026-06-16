// dllmain.cpp — proxy entry point.
//
// We must NOT do heavy work here (loader lock): SDL init, threads and config
// file IO are all deferred to the first joystick call (see joystick.cpp).
// DllMain only loads the genuine winmm and wires up the forwarder pointers.

#include "proxy.h"
#include <string>

HMODULE g_realWinmm = nullptr;

static HMODULE LoadRealWinmm()
{
    wchar_t path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    if (n == 0 || n > MAX_PATH - 12) return nullptr;
    std::wstring full(path);
    full += L"\\winmm.dll";
    return LoadLibraryW(full.c_str()); // full path -> never loads ourselves
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        g_realWinmm = LoadRealWinmm();
        if (!g_realWinmm) return FALSE;     // can't proxy without the real one
        resolve_forwarders(g_realWinmm);    // fill every non-joy export pointer
        break;
    case DLL_PROCESS_DETACH:
        // Process is going away; let the OS reclaim g_realWinmm and SDL.
        break;
    }
    return TRUE;
}

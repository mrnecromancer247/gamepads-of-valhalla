// config.h — minimal ini reader for sdljoy.ini (loaded next to the DLL).
#pragma once
#include <string>

namespace cfg {
    // Loads sdljoy.ini located beside this DLL. Safe to call once (lazy).
    void load(const std::wstring& dllDir);

    int    getInt(const char* section, const char* key, int def);
    bool   getBool(const char* section, const char* key, bool def);
    std::string getStr(const char* section, const char* key, const char* def);
}

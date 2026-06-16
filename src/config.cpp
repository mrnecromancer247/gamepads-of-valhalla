// config.cpp — dependency-free ini parser.
#include "config.h"
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace {
    std::map<std::string, std::string> g_kv; // "section.key" -> value (lowercased keys)

    std::string lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return (char)std::tolower(c); });
        return s;
    }
    std::string trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }
}

namespace cfg {

void load(const std::wstring& dllDir) {
    std::wstring path = dllDir + L"\\sdljoy.ini";
    std::ifstream f(path);
    if (!f) return; // no file -> all defaults
    std::string line, section;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = lower(trim(line.substr(1, line.size() - 2)));
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = lower(trim(line.substr(0, eq)));
        std::string val = trim(line.substr(eq + 1));
        // strip trailing inline comment
        size_t c = val.find_first_of(";#");
        if (c != std::string::npos) val = trim(val.substr(0, c));
        g_kv[section + "." + key] = val;
    }
}

std::string getStr(const char* section, const char* key, const char* def) {
    auto it = g_kv.find(lower(section) + "." + lower(key));
    return it == g_kv.end() ? std::string(def) : it->second;
}
int getInt(const char* section, const char* key, int def) {
    std::string v = getStr(section, key, "");
    if (v.empty()) return def;
    try { return std::stoi(v); } catch (...) { return def; }
}
bool getBool(const char* section, const char* key, bool def) {
    std::string v = lower(getStr(section, key, ""));
    if (v.empty()) return def;
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

} // namespace cfg

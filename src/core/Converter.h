#pragma once

#include <string>

class Converter {
public:
    static bool TrcToLua(const std::wstring& trcFile, const std::wstring& luaFile, double tolerancePx);
    static bool TrcToLuaFull(const std::wstring& trcFile, const std::wstring& luaFile);
};

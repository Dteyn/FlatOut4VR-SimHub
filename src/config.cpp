#include "config.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <iterator>

namespace
{
std::wstring Widen(const char* text)
{
    std::wstring result;
    while (*text)
        result.push_back(static_cast<wchar_t>(*text++));
    return result;
}
std::string Narrow(const wchar_t* text)
{
    std::string result;
    while (*text)
        result.push_back(static_cast<char>(*text++));
    return result;
}
std::string ReadString(const std::wstring& file, const char* key, const char* fallback)
{
    wchar_t value[256]{};
    const std::wstring wideKey = Widen(key);
    const std::wstring wideFallback = Widen(fallback);
    GetPrivateProfileStringW(
        L"Extractor", wideKey.c_str(), wideFallback.c_str(), value, static_cast<DWORD>(std::size(value)), file.c_str());
    return Narrow(value);
}
int ReadInt(const std::wstring& file, const char* key, int fallback)
{
    const std::wstring wideKey = Widen(key);
    return static_cast<int>(GetPrivateProfileIntW(L"Extractor", wideKey.c_str(), fallback, file.c_str()));
}
} // namespace

std::wstring ExecutableDirectory()
{
    wchar_t path[32768]{};
    const DWORD count = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    if (!count || count >= std::size(path))
        return L".";
    std::wstring result(path, count);
    const size_t slash = result.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : result.substr(0, slash);
}
Config LoadConfig(const std::wstring& directory)
{
    const std::wstring file = directory + L"\\extractor.ini";
    Config c;
    c.listenIp = ReadString(file, "ListenIp", c.listenIp.c_str());
    c.listenPort = Clamp(ReadInt(file, "ListenPort", c.listenPort), 1, 65535);
    c.simHubIp = ReadString(file, "SimHubIp", c.simHubIp.c_str());
    c.simHubPort = Clamp(ReadInt(file, "SimHubPort", c.simHubPort), 1, 65535);
    c.fixedRateOutput = ReadInt(file, "FixedRateOutput", 1) != 0;
    c.outputHz = Clamp(ReadInt(file, "OutputHz", c.outputHz), 10, 240);
    c.sessionTimeoutMs = Clamp(ReadInt(file, "SessionTimeoutMs", c.sessionTimeoutMs), 250, 10000);
    c.defaultMaxRpm =
        static_cast<float>(Clamp(ReadInt(file, "DefaultMaxRpm", static_cast<int>(c.defaultMaxRpm)), 1, 30000));
    c.maxGears = Clamp(ReadInt(file, "MaxGears", c.maxGears), 1, 20);
    c.debugLogging = ReadInt(file, "DebugLogging", 0) != 0;
    return c;
}

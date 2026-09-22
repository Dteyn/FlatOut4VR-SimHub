#pragma once
#include <string>

template <typename T>
constexpr T Clamp(T value, T minimum, T maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

// Settings shared by configuration loading, telemetry decoding, and packet creation.
// Keeping this in its own header prevents those consumers from depending on each other.
struct Config
{
    std::string listenIp = "127.0.0.1";
    int listenPort = 20777;
    std::string simHubIp = "127.0.0.1";
    int simHubPort = 30777;
    bool fixedRateOutput = true;
    int outputHz = 60;
    int sessionTimeoutMs = 2000;
    float defaultMaxRpm = 8000.0f;
    int maxGears = 6;
    bool debugLogging = false;
};

std::wstring ExecutableDirectory();
Config LoadConfig(const std::wstring& executableDirectory);

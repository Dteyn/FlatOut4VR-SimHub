#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mmsystem.h>
#include "config.h"
#include "simhub.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Winmm.lib")
namespace
{
using Clock = std::chrono::steady_clock;
constexpr char kApplicationTitle[] = "FlatOut 4 VR SimHub Extractor v0.4.0";

volatile bool gRunning = true;
std::ofstream gLog;
bool gDebug = false;
BOOL WINAPI ConsoleHandler(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT ||
        type == CTRL_SHUTDOWN_EVENT)
    {
        gRunning = false;
        return TRUE;
    }
    return FALSE;
}
bool HasArgument(const char* commandLine, const char* argument)
{
    return commandLine && std::strstr(commandLine, argument) != nullptr;
}
void PrintCommandLineHelp(bool versionOnly)
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    FILE* output = nullptr;
    freopen_s(&output, "CONOUT$", "w", stdout);
    if (versionOnly)
    {
        std::printf("%s\n", kApplicationTitle);
    }
    else
    {
        std::printf("%s\n\n", kApplicationTitle);
        std::printf("Receives FlatOut JSON telemetry and sends SimHub External Simulation UDP telemetry.\n\n");
        std::printf("Project: https://github.com/Dteyn/FlatOut4VR-SimHub\n\n");
        std::printf("Usage:\n");
        std::printf("  extractor.exe          Run normally (SimHub starts this).\n");
        std::printf("  extractor.exe --test   Send the 10-second test signal and exit.\n");
        std::printf("  extractor.exe --help   Show this help.\n");
        std::printf("  extractor.exe --version  Show version information.\n");
    }
    std::fflush(stdout);
}
void Log(const char* format, ...)
{
    char text[1024]{};
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    SYSTEMTIME now{};
    GetLocalTime(&now);
    char line[1200]{};
    snprintf(line,
             sizeof(line),
             "%04u-%02u-%02u %02u:%02u:%02u.%03u | %s",
             now.wYear,
             now.wMonth,
             now.wDay,
             now.wHour,
             now.wMinute,
             now.wSecond,
             now.wMilliseconds,
             text);
    if (gLog)
    {
        gLog << line << '\n';
        gLog.flush();
    }
}
std::uint64_t RandomU64()
{
    std::random_device random;
    const auto value =
        (static_cast<std::uint64_t>(random()) << 32) ^ random() ^ static_cast<std::uint64_t>(GetCurrentProcessId());
    return value ? value : 1;
}
void Close(SOCKET& socket)
{
    if (socket != INVALID_SOCKET)
    {
        closesocket(socket);
        socket = INVALID_SOCKET;
    }
}
SOCKET BindInput(const Config& c)
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(c.listenPort));
    if (InetPtonA(AF_INET, c.listenIp.c_str(), &address.sin_addr) != 1)
        return INVALID_SOCKET;
    SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == INVALID_SOCKET)
        return INVALID_SOCKET;
    BOOL exclusive = TRUE;
    setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
    u_long nonBlocking = 1;
    ioctlsocket(socket, FIONBIO, &nonBlocking);
    if (bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        Close(socket);
        return INVALID_SOCKET;
    }
    return socket;
}
SOCKET CreateOutput()
{
    return ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}
bool Destination(const Config& c, sockaddr_in& address)
{
    address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(c.simHubPort));
    return InetPtonA(AF_INET, c.simHubIp.c_str(), &address.sin_addr) == 1;
}

struct RunState
{
    SOCKET input = INVALID_SOCKET;
    SOCKET output = INVALID_SOCKET;
    std::uint64_t emitterId = 0;
    std::uint64_t sessionId = 0;
    std::uint64_t packetCounter = 0;
    Telemetry latest;
    bool haveTelemetry = false;
    bool raceActive = false;
    bool sentAny = false;
    bool sourceWasFresh = false;
    Clock::time_point start;
    Clock::time_point sessionStart;
    Clock::time_point nextBind;
    Clock::time_point nextOutput;
    Clock::time_point lastReceive;
    Clock::time_point lastHeartbeat;
    unsigned long long rx = 0;
    unsigned long long valid = 0;
    unsigned long long invalid = 0;
    unsigned long long tx = 0;
    unsigned long long bindFailures = 0;
    unsigned long long recoveries = 0;
    unsigned long long outputErrors = 0;
};

bool SendTelemetryPacket(const Telemetry& telemetry,
                         const Config& config,
                         const sockaddr_in& outputAddress,
                         RunState& state,
                         Clock::time_point now,
                         bool logFailure)
{
    if (state.output == INVALID_SOCKET)
        state.output = CreateOutput();
    if (state.output == INVALID_SOCKET)
        return false;

    const auto packet = MakePacket(telemetry,
                                   config,
                                   state.emitterId,
                                   state.sessionId,
                                   ++state.packetCounter,
                                   std::chrono::duration<double>(now - state.sessionStart).count());
    if (sendto(state.output,
               reinterpret_cast<const char*>(&packet),
               sizeof(packet),
               0,
               reinterpret_cast<const sockaddr*>(&outputAddress),
               sizeof(outputAddress)) == sizeof(packet))
    {
        state.sentAny = true;
        ++state.tx;
        return true;
    }

    Close(state.output);
    ++state.outputErrors;
    if (logFailure)
        Log("Output socket failed; it will be recreated.");
    return false;
}

void MaybeRebindInput(const Config& config, RunState& state, Clock::time_point now, bool testMode)
{
    if (testMode || state.input != INVALID_SOCKET || now < state.nextBind)
        return;

    state.input = BindInput(config);
    if (state.input == INVALID_SOCKET)
    {
        ++state.bindFailures;
        Log("UDP bind failed for %s:%d; retrying in 1 second.", config.listenIp.c_str(), config.listenPort);
        state.nextBind = now + std::chrono::seconds(1);
    }
    else
    {
        Log("Listening on %s:%d.", config.listenIp.c_str(), config.listenPort);
    }
}

void ReceiveAndProcessInput(const Config& config,
                            const sockaddr_in& outputAddress,
                            RunState& state,
                            Clock::time_point now)
{
    if (state.input == INVALID_SOCKET)
        return;

    for (;;)
    {
        char buffer[65536];
        const int received = recv(state.input, buffer, sizeof(buffer), 0);
        if (received == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
                break;
            Close(state.input);
            ++state.recoveries;
            state.nextBind = now + std::chrono::seconds(1);
            Log("Input socket failed; rebind scheduled.");
            break;
        }
        if (received <= 0)
            break;

        ++state.rx;
        std::string raw(buffer, received);
        const size_t first = raw.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || (raw[first] != '{' && raw[first] != '['))
        {
            ++state.invalid;
            continue;
        }
        const auto fields = ExtractJsonScalars(raw);
        if (fields.empty())
        {
            ++state.invalid;
            continue;
        }

        state.latest = DecodeTelemetry(fields, config);
        const bool wasActive = state.raceActive && state.haveTelemetry &&
                               now - state.lastReceive <= std::chrono::milliseconds(config.sessionTimeoutMs);
        state.raceActive = state.latest.active();
        state.haveTelemetry = true;
        state.lastReceive = now;
        ++state.valid;
        if (state.raceActive && !wasActive)
        {
            state.sessionId = RandomU64();
            state.sessionStart = now;
            Log("Telemetry live; new race detected.");
        }
        else if (!state.raceActive && wasActive)
        {
            Log(state.latest.raceEnded ? "Race ended; neutral output active."
                                       : "Telemetry became inactive; neutral output active.");
        }

        if (!config.fixedRateOutput)
        {
            SendTelemetryPacket(state.raceActive ? state.latest : InactiveTelemetry(config),
                                config,
                                outputAddress,
                                state,
                                now,
                                false);
        }
    }
}

void MaybeSendScheduledOutput(const Config& config,
                              const sockaddr_in& outputAddress,
                              RunState& state,
                              Clock::time_point now,
                              bool testMode)
{
    const bool testing = testMode && now - state.start < std::chrono::seconds(10);
    const bool fresh = state.haveTelemetry && state.raceActive &&
                       now - state.lastReceive <= std::chrono::milliseconds(config.sessionTimeoutMs);
    const bool due = testing || config.fixedRateOutput || (!config.fixedRateOutput && state.sourceWasFresh && !fresh);
    if (due && now >= state.nextOutput)
    {
        const auto interval = std::chrono::duration<double>(1.0 / config.outputHz);
        do
        {
            state.nextOutput += std::chrono::duration_cast<Clock::duration>(interval);
        } while (state.nextOutput < now - std::chrono::milliseconds(250));

        if (testing || state.haveTelemetry || state.sentAny)
        {
            const Telemetry outgoing =
                testing ? TestPattern(std::chrono::duration<double>(now - state.sessionStart).count())
                        : (fresh ? state.latest : InactiveTelemetry(config));
            SendTelemetryPacket(outgoing, config, outputAddress, state, now, true);
        }
    }
    state.sourceWasFresh = fresh;
    if (testMode && !testing)
        gRunning = false;
}

void LogHeartbeat(const RunState& state, Clock::time_point now)
{
    Log("Heartbeat: rx=%llu valid=%llu invalid=%llu tx=%llu ageMs=%lld bindFailures=%llu recoveries=%llu "
        "outputErrors=%llu",
        state.rx,
        state.valid,
        state.invalid,
        state.tx,
        state.haveTelemetry
            ? std::chrono::duration_cast<std::chrono::milliseconds>(now - state.lastReceive).count()
            : -1LL,
        state.bindFailures,
        state.recoveries,
        state.outputErrors);
}

void SendFinalNeutralPacket(const Config& config, const sockaddr_in& outputAddress, RunState& state)
{
    if (!state.sentAny)
        return;

    if (state.output == INVALID_SOCKET)
        state.output = CreateOutput();
    if (state.output == INVALID_SOCKET)
        return;

    const auto packet = MakePacket(InactiveTelemetry(config),
                                   config,
                                   state.emitterId,
                                   state.sessionId,
                                   ++state.packetCounter,
                                   std::chrono::duration<double>(Clock::now() - state.sessionStart).count());
    sendto(state.output,
           reinterpret_cast<const char*>(&packet),
           sizeof(packet),
           0,
           reinterpret_cast<const sockaddr*>(&outputAddress),
           sizeof(outputAddress));
    Log("Sent final neutral inactive SimHub packet.");
}
} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR commandLine, int)
{
    if (HasArgument(commandLine, "--help") || HasArgument(commandLine, "-h") || HasArgument(commandLine, "/?"))
    {
        PrintCommandLineHelp(false);
        return 0;
    }
    if (HasArgument(commandLine, "--version"))
    {
        PrintCommandLineHelp(true);
        return 0;
    }
    const std::wstring directory = ExecutableDirectory();
    const Config config = LoadConfig(directory);
    const bool testMode = HasArgument(commandLine, "--test");
    gDebug = config.debugLogging;
    if (gDebug)
    {
        const std::wstring logFile = directory + L"\\FlatOut4VR-SimHub-Extractor.log";
        gLog.open(logFile.c_str(), std::ios::trunc);
    }
    Log("%s", kApplicationTitle);
    WSADATA winsock{};
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0)
    {
        Log("Winsock initialization failed.");
        return 1;
    }
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    timeBeginPeriod(1);
    sockaddr_in outputAddress{};
    if (!Destination(config, outputAddress))
    {
        Log("Invalid SimHub IP address: %s", config.simHubIp.c_str());
        timeEndPeriod(1);
        WSACleanup();
        return 1;
    }
    RunState state;
    state.emitterId = RandomU64();
    state.sessionId = RandomU64();
    state.start = Clock::now();
    state.sessionStart = state.start;
    state.nextBind = state.start;
    state.nextOutput = state.start;
    state.lastReceive = state.start;
    state.lastHeartbeat = state.start;
    Log("Sending to %s:%d %s.",
        config.simHubIp.c_str(),
        config.simHubPort,
        testMode ? "for 10-second test" : (config.fixedRateOutput ? "at fixed rate" : "at source rate"));
    while (gRunning)
    {
        const auto now = Clock::now();
        MaybeRebindInput(config, state, now, testMode);
        ReceiveAndProcessInput(config, outputAddress, state, now);
        MaybeSendScheduledOutput(config, outputAddress, state, now, testMode);
        if (gDebug && now - state.lastHeartbeat >= std::chrono::seconds(5))
        {
            state.lastHeartbeat = now;
            LogHeartbeat(state, now);
        }
        Sleep(1);
    }
    SendFinalNeutralPacket(config, outputAddress, state);
    Close(state.input);
    Close(state.output);
    timeEndPeriod(1);
    WSACleanup();
    Log("Extractor stopped. RX=%llu valid=%llu invalid=%llu TX=%llu", state.rx, state.valid, state.invalid, state.tx);
    return 0;
}

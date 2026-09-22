#pragma once
#include "telemetry.h"
#include <cstdint>

constexpr std::uint32_t kGameSignature = 0x859D4B95u;
constexpr std::uint32_t kTelemetrySignature = 0x2C8F6B47u;
#pragma pack(push, 1)
struct SHTelemetryPacket
{
    std::uint32_t gameSignature, telemetrySignature;
    std::uint16_t layoutMajor, layoutMinor;
    std::uint64_t emitterId;
    std::uint8_t packetId;
    std::uint64_t packetCounter;
    std::uint8_t sessionRunning, sessionPaused;
    std::uint64_t sessionId;
    std::uint8_t replay, userInControl, aiInControl, spectator;
    double sessionTimeSeconds;
    std::uint32_t physicsDiscontinuityCounter;
    float yawDegrees, pitchDegrees, rollDegrees, yawRateDegreesPerSecond, localSurgeMs2, localSwayMs2, localHeaveMs2,
        localVelocityForwardMps, localVelocityLateralMps, localVelocityUpMps, speedKmh, groundSpeedKmh;
    std::uint8_t engineIgnitionOn, engineStarted;
    float engineRpm, engineMaxRpm, engineTorqueNm, throttle, brake, clutch, handbrake;
    char gear[8];
    std::int32_t maxGears;
    std::uint32_t completedLaps;
    std::int32_t totalLaps, racePosition;
    double currentLapTime, lastLapTimeSeconds, bestLapTimeSeconds;
    float wheelSlipFL, wheelSlipFR, wheelSlipRL, wheelSlipRR, suspensionVelocityFL, suspensionVelocityFR,
        suspensionVelocityRL, suspensionVelocityRR;
};
#pragma pack(pop)
static_assert(sizeof(SHTelemetryPacket) == 213, "SimHub packet layout changed");
SHTelemetryPacket MakePacket(const Telemetry& telemetry,
                             const Config& config,
                             std::uint64_t emitterId,
                             std::uint64_t sessionId,
                             std::uint64_t packetCounter,
                             double sessionSeconds);

#include "simhub.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace
{
float Slip(double v)
{
    return static_cast<float>(Clamp(std::abs(v), 0.0, 1.0));
}
} // namespace
SHTelemetryPacket MakePacket(const Telemetry& source,
                             const Config& config,
                             std::uint64_t emitterId,
                             std::uint64_t sessionId,
                             std::uint64_t packetCounter,
                             double sessionSeconds)
{
    const Telemetry inactive = InactiveTelemetry(config);
    const Telemetry& t = source.active() ? source : inactive;
    SHTelemetryPacket p{};
    p.gameSignature = kGameSignature;
    p.telemetrySignature = kTelemetrySignature;
    p.layoutMajor = 1;
    p.layoutMinor = 0;
    p.emitterId = emitterId;
    p.packetCounter = packetCounter;
    p.sessionRunning = source.active() ? 1 : 0;
    p.sessionPaused = t.paused ? 1 : 0;
    p.sessionId = sessionId;
    p.userInControl = p.sessionRunning;
    p.sessionTimeSeconds = sessionSeconds;
    p.yawDegrees = static_cast<float>(std::fmod(std::fmod(t.yawDegrees, 360.0) + 360.0, 360.0));
    p.pitchDegrees = static_cast<float>(t.pitchDegrees);
    p.rollDegrees = static_cast<float>(t.rollDegrees);
    p.yawRateDegreesPerSecond = static_cast<float>(t.yawSpeed * kRadiansToDegrees);
    p.localSurgeMs2 = static_cast<float>(t.accelerationZ);
    p.localSwayMs2 = static_cast<float>(t.accelerationX);
    p.localHeaveMs2 = static_cast<float>(t.accelerationY);
    p.localVelocityForwardMps = static_cast<float>(t.velocityZ);
    p.localVelocityLateralMps = static_cast<float>(t.velocityX);
    p.localVelocityUpMps = static_cast<float>(t.velocityY);
    p.speedKmh = static_cast<float>(std::abs(t.speedKmh));
    p.groundSpeedKmh = static_cast<float>(
        std::sqrt(t.velocityX * t.velocityX + t.velocityZ * t.velocityZ) * kKilometersPerHourPerMeterPerSecond);
    p.engineIgnitionOn = p.sessionRunning;
    p.engineStarted = p.sessionRunning && t.rpm > 0;
    p.engineRpm = static_cast<float>(std::max(0.0, t.rpm));
    p.engineMaxRpm = static_cast<float>(std::max(1.0, t.maxRpm));
    p.engineTorqueNm = static_cast<float>(t.engineTorqueNm);
    p.throttle = static_cast<float>(Clamp(t.throttle, 0.0, 1.0));
    p.brake = static_cast<float>(Clamp(t.brake, 0.0, 1.0));
    p.clutch = static_cast<float>(Clamp(t.clutch, 0.0, 1.0));
    p.handbrake = static_cast<float>(Clamp(t.handbrake, 0.0, 1.0));
    const size_t gearLength = std::min(t.gear.size(), sizeof(p.gear) - 1);
    std::memcpy(p.gear, t.gear.data(), gearLength);
    p.maxGears = config.maxGears;
    p.completedLaps = t.completedLaps;
    p.totalLaps = t.totalLaps;
    p.racePosition = t.racePosition;
    p.currentLapTime = t.currentLapTimeSeconds;
    p.lastLapTimeSeconds = t.lastLapTimeSeconds;
    p.bestLapTimeSeconds = t.bestLapTimeSeconds;
    p.wheelSlipFL = Slip(t.slipFL);
    p.wheelSlipFR = Slip(t.slipFR);
    p.wheelSlipRL = Slip(t.slipRL);
    p.wheelSlipRR = Slip(t.slipRR);
    p.suspensionVelocityFL = static_cast<float>(t.suspensionVelFL);
    p.suspensionVelocityFR = static_cast<float>(t.suspensionVelFR);
    p.suspensionVelocityRL = static_cast<float>(t.suspensionVelRL);
    p.suspensionVelocityRR = static_cast<float>(t.suspensionVelRR);
    return p;
}

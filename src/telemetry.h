#pragma once

#include "config.h"
#include <cstdint>
#include <string>
#include <vector>

constexpr double kRadiansToDegrees = 57.2957795130823208768;
constexpr double kKilometersPerHourPerMeterPerSecond = 3.6;

struct Field
{
    std::string key;
    std::string value;
};

struct Telemetry
{
    bool gameRunning = true, frameValid = true, outputActive = true, paused = false, raceEnded = false;
    double speedKmh = 0, rpm = 0, rpmRatio = 0, maxRpm = 8000;
    double throttle = 0, brake = 0, clutch = 0, handbrake = 0;
    std::string gear = "N";
    double accelerationX = 0, accelerationY = 0, accelerationZ = 0;
    double velocityX = 0, velocityY = 0, velocityZ = 0;
    double pitchDegrees = 0, rollDegrees = 0, yawDegrees = 0, yawSpeed = 0;
    double engineTorqueNm = 0;
    std::uint32_t completedLaps = 0;
    int totalLaps = -1, racePosition = 0;
    double currentLapTimeSeconds = 0, lastLapTimeSeconds = 0, bestLapTimeSeconds = 0;
    double slipFL = 0, slipFR = 0, slipRL = 0, slipRR = 0;
    double suspensionVelFL = 0, suspensionVelFR = 0, suspensionVelRL = 0, suspensionVelRR = 0;
    bool active() const
    {
        return gameRunning && frameValid && outputActive && !raceEnded;
    }
};

std::vector<Field> ExtractJsonScalars(const std::string& text);
Telemetry DecodeTelemetry(const std::vector<Field>& fields, const Config& config);
Telemetry InactiveTelemetry(const Config& config);
Telemetry TestPattern(double seconds);

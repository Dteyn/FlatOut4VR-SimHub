#include "telemetry.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace
{
bool ParseString(const std::string& text, size_t& pos, std::string& value)
{
    if (pos >= text.size() || text[pos++] != '"')
        return false;
    value.clear();
    while (pos < text.size())
    {
        char ch = text[pos++];
        if (ch == '"')
            return true;
        if (ch != '\\')
        {
            value.push_back(ch);
            continue;
        }
        if (pos >= text.size())
            return false;
        switch (text[pos++])
        {
        case '"':
            value.push_back('"');
            break;
        case '\\':
            value.push_back('\\');
            break;
        case '/':
            value.push_back('/');
            break;
        case 'b':
            value.push_back('\b');
            break;
        case 'f':
            value.push_back('\f');
            break;
        case 'n':
            value.push_back('\n');
            break;
        case 'r':
            value.push_back('\r');
            break;
        case 't':
            value.push_back('\t');
            break;
        case 'u':
            if (pos + 4 > text.size())
                return false;
            pos += 4;
            value.push_back('?');
            break;
        default:
            return false;
        }
    }
    return false;
}
void SkipSpace(const std::string& text, size_t& pos)
{
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
        ++pos;
}
std::string Lower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
class Lookup
{
  public:
    explicit Lookup(const std::vector<Field>& fields)
    {
        for (const auto& field : fields)
            if (!field.key.empty())
                values_.emplace(Lower(field.key), field.value);
    }
    bool Number(double& value, std::initializer_list<const char*> names) const
    {
        for (const char* name : names)
        {
            const auto it = values_.find(Lower(name));
            if (it == values_.end())
                continue;
            char* end = nullptr;
            const double parsed = std::strtod(it->second.c_str(), &end);
            if (end != it->second.c_str() && *end == '\0' && std::isfinite(parsed))
            {
                // .NET's numeric parser canonicalized capture values such as "-0.0000"
                // to positive zero. Keep the native packet stream byte-identical to the
                // accepted C# baseline rather than serializing a negative-zero float.
                value = parsed == 0.0 ? 0.0 : parsed;
                return true;
            }
        }
        return false;
    }
    bool Text(std::string& value, std::initializer_list<const char*> names) const
    {
        for (const char* name : names)
        {
            const auto it = values_.find(Lower(name));
            if (it != values_.end())
            {
                value = it->second;
                return true;
            }
        }
        return false;
    }

  private:
    std::unordered_map<std::string, std::string> values_;
};
bool Bool(const Lookup& fields, bool fallback, std::initializer_list<const char*> names)
{
    double value;
    return fields.Number(value, names) ? value != 0.0 : fallback;
}
double Control(const Lookup& fields, std::initializer_list<const char*> names)
{
    double value;
    if (!fields.Number(value, names))
        return 0;
    if (value > 1 && value <= 100)
        value /= 100;
    return Clamp(value, 0.0, 1.0);
}
std::string Gear(std::string value)
{
    char* end = nullptr;
    const double number = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0')
        return value;
    const int gear = static_cast<int>(std::round(number));
    return gear < 0 ? "R" : gear == 0 ? "N" : std::to_string(gear);
}
} // namespace

std::vector<Field> ExtractJsonScalars(const std::string& text)
{
    std::vector<Field> fields;
    size_t pos = 0;
    while (pos < text.size())
    {
        if (text[pos] != '"')
        {
            ++pos;
            continue;
        }
        const size_t start = pos;
        Field field;
        if (!ParseString(text, pos, field.key))
        {
            pos = start + 1;
            continue;
        }
        const size_t afterKey = pos;
        SkipSpace(text, pos);
        if (pos >= text.size() || text[pos] != ':')
        {
            pos = afterKey;
            continue;
        }
        SkipSpace(text, ++pos);
        if (pos >= text.size())
            break;
        if (text[pos] == '"')
        {
            if (ParseString(text, pos, field.value))
                fields.push_back(std::move(field));
            continue;
        }
        if (text[pos] == '{' || text[pos] == '[')
        {
            ++pos;
            continue;
        }
        const size_t valueStart = pos;
        while (pos < text.size() && text[pos] != ',' && text[pos] != '}' && text[pos] != ']')
            ++pos;
        field.value = text.substr(valueStart, pos - valueStart);
        const size_t first = field.value.find_first_not_of(" \t\r\n");
        const size_t last = field.value.find_last_not_of(" \t\r\n");
        if (first != std::string::npos)
        {
            field.value = field.value.substr(first, last - first + 1);
            fields.push_back(std::move(field));
        }
    }
    return fields;
}

Telemetry DecodeTelemetry(const std::vector<Field>& values, const Config& config)
{
    const Lookup f(values);
    Telemetry t;
    double v = 0;
    t.gameRunning = Bool(f, true, {"GameRunning"});
    t.frameValid = Bool(f, true, {"FrameValid"});
    t.outputActive = Bool(f, true, {"OutputActive"});
    t.paused = Bool(f, false, {"Paused"});
    t.raceEnded = Bool(f, false, {"RaceEnded"});
    if (f.Number(v, {"SpeedKmh", "speed_kmh"}))
        t.speedKmh = v;
    else if (f.Number(v, {"Speed", "SpeedMps", "speed_mps", "VehicleSpeed"}))
        t.speedKmh = v * kKilometersPerHourPerMeterPerSecond;
    bool haveRpm = f.Number(t.rpm, {"RPM", "Rpm", "EngineRPM", "EngineRpm", "engine_rpm"});
    f.Number(t.rpmRatio, {"RPMRatio", "RpmRatio", "rpm_ratio"});
    if (!f.Number(t.maxRpm, {"MaxRPM", "MaxRpm", "EngineMaxRPM", "EngineMaxRpm", "max_rpm"}))
    {
        const double derived = haveRpm && t.rpm > 0 && t.rpmRatio > .005 ? t.rpm / t.rpmRatio : 0;
        t.maxRpm = derived >= 1000 && derived <= 30000 ? derived : config.defaultMaxRpm;
    }
    t.throttle = Control(f, {"Throttle", "Gas", "Accelerator", "throttle_input"});
    t.brake = Control(f, {"Brake", "brake_input"});
    t.clutch = Control(f, {"Clutch", "clutch_input"});
    t.handbrake = Control(f, {"Handbrake", "ParkingBrake", "handbrake_input"});
    std::string text;
    if (f.Text(text, {"GearText", "Gear", "CurrentGear", "current_gear"}))
        t.gear = Gear(text);
    f.Number(t.accelerationX, {"LocalAccelerationX"});
    f.Number(t.accelerationY, {"LocalAccelerationY"});
    f.Number(t.accelerationZ, {"LocalAccelerationZ"});
    f.Number(t.velocityX, {"LocalVelocityX", "VelocityX"});
    f.Number(t.velocityY, {"LocalVelocityY", "VelocityY"});
    f.Number(t.velocityZ, {"LocalVelocityZ", "VelocityZ"});
    double radians = 0;
    if (f.Number(v, {"PitchDegrees"}))
        t.pitchDegrees = v;
    else if (f.Number(radians, {"Pitch"}))
        t.pitchDegrees = radians * kRadiansToDegrees;
    if (f.Number(v, {"RollDegrees"}))
        t.rollDegrees = v;
    else if (f.Number(radians, {"Roll"}))
        t.rollDegrees = radians * kRadiansToDegrees;
    if (f.Number(v, {"YawDegrees"}))
        t.yawDegrees = v;
    else if (f.Number(radians, {"Yaw"}))
        t.yawDegrees = radians * kRadiansToDegrees;
    f.Number(t.yawSpeed, {"YawSpeed"});
    f.Number(t.engineTorqueNm, {"EngineTorqueNm"});
    if (f.Number(v, {"LapCompleted"}) && v >= 0)
        t.completedLaps = static_cast<std::uint32_t>(v);
    if (f.Number(v, {"RaceLaps"}) && v > 0)
        t.totalLaps = static_cast<int>(v);
    if (f.Number(v, {"RaceRank"}) && v >= 0)
        t.racePosition = static_cast<int>(v);
    if (f.Number(v, {"CurrentLapTimeMS"}) && v >= 0)
        t.currentLapTimeSeconds = v / 1000;
    if (f.Number(v, {"LastLapTimeMS"}) && v >= 0)
        t.lastLapTimeSeconds = v / 1000;
    if (f.Number(v, {"BestLapTimeMS"}) && v >= 0)
        t.bestLapTimeSeconds = v / 1000;
    f.Number(t.slipFL, {"LongitudinalSlipFL"});
    f.Number(t.slipFR, {"LongitudinalSlipFR"});
    f.Number(t.slipRL, {"LongitudinalSlipRL"});
    f.Number(t.slipRR, {"LongitudinalSlipRR"});
    f.Number(t.suspensionVelFL, {"SuspensionVelocityFL", "suspen_vel_fl"});
    f.Number(t.suspensionVelFR, {"SuspensionVelocityFR", "suspen_vel_fr"});
    f.Number(t.suspensionVelRL, {"SuspensionVelocityRL", "suspen_vel_bl", "suspen_vel_rl"});
    f.Number(t.suspensionVelRR, {"SuspensionVelocityRR", "suspen_vel_br", "suspen_vel_rr"});
    return t;
}
Telemetry InactiveTelemetry(const Config& config)
{
    Telemetry t;
    t.gameRunning = t.frameValid = t.outputActive = false;
    t.maxRpm = config.defaultMaxRpm;
    return t;
}
Telemetry TestPattern(double seconds)
{
    Telemetry t;
    const double phase = std::fmod(seconds, 10.0) / 10.0;
    const double speed = 10 + phase * 40;
    t.speedKmh = speed;
    t.velocityZ = speed / kKilometersPerHourPerMeterPerSecond;
    t.velocityX = std::sin(seconds) * 1.5;
    t.accelerationZ = std::sin(seconds * 1.3) * 1.5;
    t.accelerationX = std::sin(seconds * .8);
    t.accelerationY = std::sin(seconds * 1.7) * .4;
    t.rpm = 1500 + phase * 4000;
    t.maxRpm = 8000;
    t.throttle = .15 + phase * .5;
    t.gear = phase < .2 ? "1" : phase < .4 ? "2" : phase < .6 ? "3" : phase < .8 ? "4" : "5";
    t.yawDegrees = std::fmod(seconds * 12, 360.0);
    t.yawSpeed = 12 / kRadiansToDegrees;
    t.suspensionVelFL = std::sin(seconds * 2) * .05;
    t.suspensionVelFR = std::sin(seconds * 2.1) * .05;
    t.suspensionVelRL = std::sin(seconds * 1.9) * .05;
    t.suspensionVelRR = std::sin(seconds * 2.2) * .05;
    return t;
}

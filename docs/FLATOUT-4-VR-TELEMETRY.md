# FlatOut 4 VR telemetry reference

This document describes the telemetry produced by **FlatOut 4: Total Insanity VR** and how this project makes it available to SimHub. It is intended both as a practical introduction and as a reference for people who want to inspect, capture, or extend the data path.

The source information comes from the accepted FlatOut capture and the extractor's current decoder. The capture confirms field names and representative values; it does not establish every field's complete operating range in every vehicle, track, or game mode.

This document describes details from **game version v1.87** (May 14 2026 release date).

## The data path

```text
FlatOut 4 VR
  JSON UDP datagrams, 127.0.0.1:20777
        |
        v
extractor.exe
  validates, decodes, normalizes, and schedules output
        |
        v
SimHub External Simulation
  packed binary UDP, 127.0.0.1:30777
```

FlatOut supplies rich JSON telemetry. SimHub does not accept JSON directly: the extractor translates the useful values into the binary field layout declared by `FlatOut4VR.simdef`, following the [External Sim Integration guide](https://manual.simhubdash.com/external-sim-integration) from SimHub. SimHub starts `extractor.exe` while the FlatOut profile is active and terminates it when the profile changes or SimHub exits. The extractor does not monitor the game process itself.

The default addresses and ports are configurable in `extractor.ini`; keeping the defaults is normally best because both endpoints are local.

| Connection | Default | Direction |
| --- | --- | --- |
| FlatOut input | `127.0.0.1:20777` | FlatOut -> extractor |
| SimHub output | `127.0.0.1:30777` | extractor -> SimHub |

## FlatOut JSON input

Each source datagram is JSON. The extractor accepts a top-level object or array, extracts scalar properties, and ignores malformed or non-JSON datagrams. Property lookup is case-insensitive. Numeric values must be finite; quoted text is accepted where a text field is expected.

The following abbreviated frame shows the general shape. It is not a complete packet and values are illustrative.

```json
{
  "GameRunning": 1,
  "FrameValid": 1,
  "OutputActive": 1,
  "Paused": 0,
  "SpeedKmh": 104.6,
  "RPM": 5210.0,
  "RPMRatio": 0.651,
  "GearText": "3",
  "Throttle": 0.82,
  "Brake": 0.0,
  "LocalVelocityX": 1.4,
  "LocalVelocityY": 0.1,
  "LocalVelocityZ": 28.9,
  "YawDegrees": -131.9,
  "RaceEnded": 0
}
```

### Lifecycle and timing

| Field | Meaning in the source frame | Extractor behavior |
| --- | --- | --- |
| `GameRunning` | Game is running. | Must be nonzero for live output; defaults to live if absent. |
| `FrameValid` | Frame is usable. | Must be nonzero for live output; defaults to live if absent. |
| `OutputActive` | Vehicle output is active. | Must be nonzero for live output; defaults to live if absent. |
| `Paused` | Game is paused. | Passed to SimHub's session-paused flag while the source remains active. |
| `RaceEnded` | Race/results state has begun. | Immediately makes output inactive, even if other vehicle values keep arriving. |
| `Sequence` | Source frame sequence number. | Observed but not sent to SimHub. |
| `TimestampMS`, `DeltaTime` | Source timing values. | Observed but not sent to SimHub; the extractor owns SimHub packet scheduling. |

A frame is live only when `GameRunning`, `FrameValid`, and `OutputActive` are nonzero and `RaceEnded` is zero. If no live source frame arrives for `SessionTimeoutMs` (2,000 ms by default), the extractor emits neutral output. A fresh live frame after inactivity begins a new SimHub session.

### Drivetrain and controls

| FlatOut field or alias | Units / form | SimHub use |
| --- | --- | --- |
| `SpeedKmh`, `speed_kmh` | km/h | Preferred speed source. |
| `Speed`, `SpeedMps`, `speed_mps`, `VehicleSpeed` | m/s | Fallback speed source; multiplied by 3.6. |
| `RPM`, `Rpm`, `EngineRPM`, `EngineRpm`, `engine_rpm` | rpm | `EngineRpm`. |
| `RPMRatio`, `RpmRatio`, `rpm_ratio` | normalized ratio | Used with RPM to derive maximum RPM when no valid direct maximum is supplied. |
| `MaxRPM`, `MaxRpm`, `EngineMaxRPM`, `EngineMaxRpm`, `max_rpm` | rpm | Direct maximum-RPM value, when present. |
| `GearText`, `Gear`, `CurrentGear`, `current_gear` | text or number | `Gear`; a negative number becomes `R`, zero becomes `N`. |
| `Throttle`, `Gas`, `Accelerator`, `throttle_input` | 0–1 or 0–100 | `Throttle`, normalized and clamped to 0–1. |
| `Brake`, `brake_input` | 0–1 or 0–100 | `Brake`, normalized and clamped to 0–1. |
| `Clutch`, `clutch_input` | 0–1 or 0–100 | `Clutch`, normalized and clamped to 0–1. |
| `Handbrake`, `ParkingBrake`, `handbrake_input` | 0–1 or 0–100 | `Handbrake`, normalized and clamped to 0–1. |
| `EngineTorqueNm` | N·m | `EngineTorqueNm`. |

When a maximum RPM is absent, the extractor uses `RPM / RPMRatio` only when the result is plausible (1,000–30,000 rpm). Otherwise it uses `DefaultMaxRpm` from `extractor.ini` (8,000 by default).

### Motion, local axes, and orientation

FlatOut's observed local coordinate system is:

```text
X = lateral     Y = up     Z = forward
```

This is why the SimHub motion mapping is not a same-name copy:

| FlatOut source | Units | SimHub field |
| --- | --- | --- |
| `LocalAccelerationZ` | m/s² | `LocalSurgeMs2` (forward/back) |
| `LocalAccelerationX` | m/s² | `LocalSwayMs2` (left/right) |
| `LocalAccelerationY` | m/s² | `LocalHeaveMs2` (up/down) |
| `LocalVelocityZ` | m/s | `LocalVelocityForwardMps` |
| `LocalVelocityX` or `VelocityX` | m/s | `LocalVelocityLateralMps` |
| `LocalVelocityY` or `VelocityY` | m/s | `LocalVelocityUpMps` |
| `PitchDegrees`, `RollDegrees`, `YawDegrees` | degrees | Same-named SimHub orientation fields; degree values take precedence. |
| `Pitch`, `Roll`, `Yaw` | radians | Fallback orientation source, converted to degrees. |
| `YawSpeed` | radians/s | `YawRateDegreesPerSecond`, converted to degrees/s. |

The extractor wraps yaw into `0..360` before sending it to SimHub. `GroundSpeedKmh` is calculated from the horizontal local X/Z velocity vector and excludes vertical motion.

### Race and lap data

| FlatOut field | SimHub field | Notes |
| --- | --- | --- |
| `LapCompleted` | `CompletedLaps` | Negative values are ignored. |
| `RaceLaps` | `TotalLaps` | Positive values are used. |
| `RaceRank` | `RacePosition` | Non-negative values are used. |
| `CurrentLapTimeMS` | `CurrentLapTime` | Converted from milliseconds to seconds. |
| `LastLapTimeMS` | `LastLapTimeSeconds` | Converted from milliseconds to seconds. |
| `BestLapTimeMS` | `BestLapTimeSeconds` | Converted from milliseconds to seconds. |
| `RaceTimeMS`, `LogicLap`, `Checkpoint`, `CheckpointCount`, `DistToEndOfRace` | — | Observed in FlatOut telemetry but not emitted by the current SimHub contract. |

### Tyres and suspension

FlatOut reports four-corner values using `FL`, `FR`, `RL`, and `RR` suffixes. The current SimHub definition carries longitudinal slip and suspension velocity only.

| FlatOut source | SimHub use |
| --- | --- |
| `LongitudinalSlipFL/FR/RL/RR` | Four `WheelSlip*` fields. The absolute value is clamped to 0–1 because SimHub receives a normalized magnitude. |
| `SuspensionVelocityFL/FR/RL/RR` | Four `SuspensionVelocity*Mps` fields. Legacy aliases such as `suspen_vel_fl` are also accepted. |
| `SuspensionTravel*`, `SuspensionMaxTravel*`, `SuspensionAcceleration*`, `SpringForce*` | Available from FlatOut but not emitted by this definition. |
| `LateralSlip*`, `WheelRotationSpeed*`, `SurfaceID*`, `WheelInAir*` | Available from FlatOut but not emitted by this definition. |

### Other observed FlatOut data

The accepted capture also contains useful information that is deliberately outside the current 37-field SimHub packet: `Steering`, boost state, over-rev state, engine power, G-force, world position, motor life, damage delta, airborne/landing events, angular acceleration, and per-wheel surface/contact information.

These fields are valuable for research or a future explicitly versioned extension, but they are not hidden aliases for existing SimHub fields. Adding them would require changing the `.simdef`, regenerating SimHub's official structure/constants, and updating the packet encoder together.

## What SimHub receives

The extractor sends a packed, little-endian 213-byte SimHub External Simulation packet. Its stable identity is:

| Property | Value |
| --- | --- |
| Definition ID | `5059abb9-d53b-4e70-abb7-23ecfec6d4be` |
| Game signature | `0x859D4B95` |
| Telemetry signature | `0x2C8F6B47` |
| Layout | 1.0, revision-2 data conversion |
| Payload fields | 37 standard SimHub fields |

In addition to the mapped data above, the packet carries a process-stable emitter ID, a packet counter, a fresh session ID when live telemetry resumes, session time, active/paused flags, engine state, and configured maximum gear count. It is sent at 60 Hz by default (`FixedRateOutput=1` and `OutputHz=60`), using the newest valid FlatOut frame.

### SimHub payload order

In SimHub, the 37 payload fields follow the 55-byte standard header in this exact order. Each group below uses `float` unless noted otherwise.

| Positions | Fields |
| --- | --- |
| 1–4 | `YawDegrees`, `PitchDegrees`, `RollDegrees`, `YawRateDegreesPerSecond` |
| 5–7 | `LocalSurgeMs2`, `LocalSwayMs2`, `LocalHeaveMs2` |
| 8–12 | `LocalVelocityForwardMps`, `LocalVelocityLateralMps`, `LocalVelocityUpMps`, `SpeedKmh`, `GroundSpeedKmh` |
| 13–17 | `EngineIgnitionOn` (byte), `EngineStarted` (byte), `EngineRpm`, `EngineMaxRpm`, `EngineTorqueNm` |
| 18–21 | `Throttle`, `Brake`, `Clutch`, `Handbrake` |
| 22–26 | `Gear` (8-byte text), `MaxGears` (int32), `CompletedLaps` (uint32), `TotalLaps` (int32), `RacePosition` (int32) |
| 27–29 | `CurrentLapTime`, `LastLapTimeSeconds`, `BestLapTimeSeconds` (double) |
| 30–33 | `WheelSlipFrontLeft`, `WheelSlipFrontRight`, `WheelSlipRearLeft`, `WheelSlipRearRight` |
| 34–37 | `SuspensionVelocityFrontLeftMps`, `SuspensionVelocityFrontRightMps`, `SuspensionVelocityRearLeftMps`, `SuspensionVelocityRearRightMps` |

The exact binary field order is the `.simdef` file. Do not change field order, packet size, signatures, or packing independently. If the `.simdef` must change, save the file through SimHub's definition editor and regenerate its C++ demo before changing the extractor.

## Inactive and safety behavior

Inactive output is intentional. On race end, inactive lifecycle flags, source timeout, or orderly shutdown, the extractor sends a neutral packet instead of repeating the last moving-car values:

- session-running and user-control flags are cleared;
- gear becomes `N`;
- engine maximum RPM returns to the configured fallback;
- motion, speed, controls, RPM, torque, race, slip, and suspension values are zeroed.

This prevents a stale frame from continuing to drive SimHub dashboards, motion, or haptic effects after a race has ended or source telemetry has stopped.

## Exploring or troubleshooting the source

- Enable `DebugLogging=1` in `extractor.ini` to create the adjacent extractor log. The five-second heartbeat reports received, valid, invalid, and transmitted frame counts.
- A rising receive count but no SimHub response usually points to the definition/registration or output side (`30777`), not the FlatOut listener.
- A bind-retry log entry means another process owns the configured FlatOut input port (`20777` by default).
- The private development capture contains 663 frames and is used to guard the established decoder and packet mapping. It is evidence for field names and behavior, not a promise that every game state has been exhaustively sampled.

For implementation details, see [Architecture](ARCHITECTURE.md).

## Complete observed FlatOut property list

The table below is the complete set of **108 scalar properties** present in a FlatOut telemetry capture. The example is a real captured value, rendered compactly (`-0` represents the source's negative-zero value). It demonstrates property presence and JSON shape, not a guaranteed range, type contract, or semantic definition beyond the field name. Values vary with vehicle, track, mode, and game state.

| Property | Example captured value |
| --- | --- |
| `GameRunning` | `1` |
| `FrameValid` | `1` |
| `OutputActive` | `1` |
| `Paused` | `0` |
| `Sequence` | `1` |
| `TimestampMS` | `401720437` |
| `DeltaTime` | `0.021987` |
| `SpeedMps` | `-0` |
| `SpeedKmh` | `-0` |
| `SpeedMph` | `-0` |
| `RPM` | `331` |
| `RPMRatio` | `0.0433` |
| `GearRaw` | `1` |
| `Gear` | `0` |
| `GearText` | `N` |
| `Throttle` | `0` |
| `Brake` | `0` |
| `Clutch` | `0` |
| `Handbrake` | `0` |
| `Steering` | `-0` |
| `BoostActive` | `0` |
| `BoostStarted` | `0` |
| `BoostEnded` | `0` |
| `OverRevving` | `0` |
| `EnginePowerW` | `0` |
| `EngineTorqueNm` | `0` |
| `LocalVelocityX` | `0` |
| `LocalVelocityY` | `-0.3694` |
| `LocalVelocityZ` | `-0` |
| `LocalAccelerationX` | `0` |
| `LocalAccelerationY` | `16.8014` |
| `LocalAccelerationZ` | `-0` |
| `GForceLateral` | `0` |
| `GForceVertical` | `1.7133` |
| `GForceLongitudinal` | `-0` |
| `Roll` | `0` |
| `Pitch` | `-0` |
| `Yaw` | `-2.303243` |
| `RollDegrees` | `0` |
| `PitchDegrees` | `-0` |
| `YawDegrees` | `-131.9661` |
| `RollSpeed` | `-0` |
| `PitchSpeed` | `0` |
| `YawSpeed` | `0` |
| `RollAcceleration` | `0` |
| `PitchAcceleration` | `0` |
| `YawAcceleration` | `0` |
| `PositionX` | `117.595` |
| `PositionY` | `-573.703` |
| `PositionZ` | `192.497` |
| `MotorLife` | `100` |
| `MotorMaxLife` | `100` |
| `MotorLifeRatio` | `1` |
| `DamageDelta` | `0` |
| `Airborne` | `1` |
| `LandingEvent` | `0` |
| `RaceRank` | `1` |
| `RaceTimeMS` | `0` |
| `CurrentLapTimeMS` | `0` |
| `LastLapTimeMS` | `0` |
| `BestLapTimeMS` | `-1` |
| `LapCompleted` | `0` |
| `LogicLap` | `0` |
| `RaceLaps` | `3` |
| `Checkpoint` | `0` |
| `CheckpointCount` | `8` |
| `DistToEndOfRace` | `13080.898` |
| `RaceEnded` | `0` |
| `SuspensionTravelFL` | `-0` |
| `SuspensionMaxTravelFL` | `0.2898` |
| `SuspensionVelocityFL` | `0` |
| `SuspensionAccelerationFL` | `0` |
| `SpringForceFL` | `0` |
| `LongitudinalSlipFL` | `0` |
| `LateralSlipFL` | `0` |
| `WheelRotationSpeedFL` | `0` |
| `SurfaceIDFL` | `-1` |
| `WheelInAirFL` | `1` |
| `SuspensionTravelFR` | `-0` |
| `SuspensionMaxTravelFR` | `0.2898` |
| `SuspensionVelocityFR` | `0` |
| `SuspensionAccelerationFR` | `0` |
| `SpringForceFR` | `0` |
| `LongitudinalSlipFR` | `0` |
| `LateralSlipFR` | `0` |
| `WheelRotationSpeedFR` | `0` |
| `SurfaceIDFR` | `-1` |
| `WheelInAirFR` | `1` |
| `SuspensionTravelRL` | `-0` |
| `SuspensionMaxTravelRL` | `0.2898` |
| `SuspensionVelocityRL` | `0` |
| `SuspensionAccelerationRL` | `0` |
| `SpringForceRL` | `0` |
| `LongitudinalSlipRL` | `0` |
| `LateralSlipRL` | `0` |
| `WheelRotationSpeedRL` | `0` |
| `SurfaceIDRL` | `-1` |
| `WheelInAirRL` | `1` |
| `SuspensionTravelRR` | `-0` |
| `SuspensionMaxTravelRR` | `0.2898` |
| `SuspensionVelocityRR` | `0` |
| `SuspensionAccelerationRR` | `0` |
| `SpringForceRR` | `0` |
| `LongitudinalSlipRR` | `0` |
| `LateralSlipRR` | `0` |
| `WheelRotationSpeedRR` | `0` |
| `SurfaceIDRR` | `-1` |
| `WheelInAirRR` | `1` |

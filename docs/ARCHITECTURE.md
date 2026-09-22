# Architecture

This is a developer map of the project: what each part owns, how a telemetry frame travels through the application, and what must remain compatible when making a change. For installation, see [INSTALL.txt](../INSTALL.txt). For FlatOut field names and mappings, see the [telemetry reference](FLATOUT-4-VR-TELEMETRY.md).

## System in one view

```text
FlatOut 4 VR JSON UDP
        |
        v
extractor.exe
  receive -> decode -> retain newest state -> schedule -> encode
        |
        v
SimHub External Simulation UDP
```

`extractor.exe` is a windowless x64 C++17 companion application. It uses only WinSock and `winmm`; there is no SimHub SDK, .NET runtime, plugin callback, worker thread, or user interface.

SimHub owns the process lifetime. Its definition starts the extractor when the FlatOut 4 VR profile is active and stops it when the profile changes or SimHub exits. The extractor deliberately does not launch, watch, or close the FlatOut process.

## Project map

| Area | Responsibility |
| --- | --- |
| `src/main.cpp` | Process startup/shutdown, sockets, single runtime loop, session state, output scheduling, logging, and command-line modes. |
| `src/config.*` | Reads `extractor.ini` from the executable directory and applies safe bounds to settings. |
| `src/telemetry.*` | Extracts scalar JSON values, decodes the supported FlatOut source fields, builds inactive state, and supplies the test pattern. |
| `src/simhub.*` | Defines the packed SimHub packet and converts the normalized internal telemetry state into that packet. |
| `simdef/FlatOut4VR.simdef` | SimHub's authoritative definition of the game identity, field order, packet signatures, and extractor command. |
| `simdef/extractor.ini` | Shipped defaults for the editable runtime settings. |
| `registration/*.shlink` | Manual registration template for SimHub's External Sims registry. |
| `install.*`, `package-release.*` | User installation and release-package helpers. |

The code is intentionally small. Keep responsibilities in these existing files unless a new boundary is truly necessary; a framework or socket abstraction would make the extractor harder to inspect without improving its job.

## Runtime model

`main.cpp` owns one `RunState` object. It holds the current sockets, latest decoded telemetry, lifecycle flags, IDs/counters, scheduling timestamps, and diagnostic counters. All of it is accessed by one loop on one thread.

At startup the extractor:

1. Handles `--help`, `--version`, or `--test` before starting normal telemetry work.
2. Reads `extractor.ini` beside the executable and opens the optional diagnostic log.
3. Starts WinSock, enables the 1 ms Windows timer resolution, validates the SimHub destination, and creates process-scoped emitter/session identifiers.

Each loop iteration then:

1. Retries the FlatOut UDP input bind when needed.
2. Drains all available input datagrams without blocking.
3. Rejects malformed data; for a valid frame, decodes it and retains it as the newest source state.
4. Detects transitions into and out of active gameplay, creating a fresh SimHub session when live telemetry returns.
5. Sends either source-rate output or a fixed-rate packet using the newest state.
6. Emits an optional diagnostic heartbeat and sleeps briefly.

On normal shutdown it sends one final neutral packet when output was active, closes sockets, restores the timer resolution, and cleans up WinSock.

This newest-state approach is deliberate: UDP can arrive faster or slower than the SimHub cadence, and processing an old queue of vehicle states would add avoidable latency.

## State boundaries

There are three useful representations of telemetry:

| Representation | Purpose |
| --- | --- |
| FlatOut JSON datagram | Untrusted source input. It can contain fields the project does not use. |
| `Telemetry` | Small, normalized internal snapshot. It contains only values needed for the current SimHub contract and lifecycle decisions. |
| `SHTelemetryPacket` | Packed binary output. Its layout is tied exactly to the `.simdef`. |

The decoder is the boundary between game-specific input and the rest of the program. The packet encoder is the boundary between project state and SimHub. Keeping those boundaries narrow prevents FlatOut-specific aliases and parsing decisions from leaking into socket/scheduling code.

## Lifecycle and failure handling

The extractor treats data freshness and race eligibility separately from mere UDP traffic. A source frame is active only when its lifecycle values allow it and `RaceEnded` is not set. If source data becomes inactive or stale, the outgoing state is rebuilt as neutral rather than reusing the last moving-car frame.

The following behaviors are intentional and should be preserved unless a change is explicitly planned:

- input bind failures are logged and retried after one second;
- input socket failures schedule a rebind instead of ending the process;
- send failures close the output socket so a later send recreates it;
- malformed datagrams increment diagnostics but do not disturb the last valid state;
- graceful shutdown sends a final neutral packet;
- `--test` uses the ordinary output path for ten seconds, then exits cleanly.

These choices make the extractor safe to leave under SimHub's profile management and prevent stale motion/haptic output during menus, results, disconnects, and shutdown.

## Compatibility contract

The SimHub output is a stable integration contract, not an implementation detail. The current definition uses:

| Property | Value |
| --- | --- |
| Definition ID | `5059abb9-d53b-4e70-abb7-23ecfec6d4be` |
| Game signature | `0x859D4B95` |
| Telemetry signature | `0x2C8F6B47` |
| Layout | 1.0, revision-2 conversion |
| Packet | 213-byte packed, little-endian structure with 37 fields |
| Default output | UDP port `30777`, 60 Hz fixed-rate |

`SHTelemetryPacket` is packed and protected by a size assertion. Its field order, primitive types, signatures, and packing must match the `.simdef` exactly. Do not edit one side without regenerating and reviewing the other.

The accepted decoder behavior also includes its supported source aliases, precedence rules, unit conversion, control normalization, fallback RPM calculation, and inactive defaults. Example capture data helps verify that this behavior remains stable.

## Making changes safely

| If you need to change... | Start here | Compatibility impact |
| --- | --- | --- |
| A default, port, timeout, or diagnostic option | `config.h`, `config.cpp`, `simdef/extractor.ini`, and README settings | Usually local; preserve validation bounds. |
| How a FlatOut value is interpreted | `telemetry.cpp` and the telemetry reference | Compare against example capture data; preserve aliases and precedence unless intentionally changing them. |
| How normalized data reaches SimHub | `simhub.cpp` | Packet values can change; compare against example capture data and review downstream effects. |
| The SimHub fields or binary layout | `.simdef` first, then SimHub's generated C++ demo and `simhub.*` | Breaking contract change: update signatures/layout and revalidate the entire integration. |
| Installation/package layout | `install.*`, `package-release.*`, `INSTALL.txt` | Keep `FlatOut4VR.simdef`, `extractor.exe`, and `extractor.ini` together. |

Avoid combining a protocol change with a broad refactor. Make the contract change small, regenerate from SimHub, and compare representative example data before and after the change.

## Deployment

The release keeps `FlatOut4VR.simdef`, `extractor.exe`, `extractor.ini`, and the banner in the same FlatOut definition folder. The definition's `IconPath` and extractor command are relative to that folder. `install.bat`/`install.ps1` place the bundle under SimHub's definitions directory and write the registration link; the `.shlink` template remains available for manual installation.

When a definition changes, save it through SimHub's definition editor and keep its generated C++ demo with the definition change. SimHub's generated constants and structure are the final source of truth for the external packet contract.

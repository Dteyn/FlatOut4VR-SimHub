<p align="center">
  <img src="assets/flatout-4-vr-banner.jpg" alt="FlatOut 4: Total Insanity VR SimHub Extractor banner" width="460">
</p>

# FlatOut 4 VR SimHub
![Latest release](https://img.shields.io/github/v/release/Dteyn/FlatOut4VR-SimHub?include_prereleases)
![Downloads](https://img.shields.io/github/downloads/Dteyn/FlatOut4VR-SimHub/total)

Bring **FlatOut 4: Total Insanity VR** telemetry into SimHub for dashboards, motion, haptics, and other SimHub devices.

This repo contains a `.simdef` for FlatOut 4 VR, along with a small extractor app to convert telemetry data to SimHub format.

## Download

### Current Version: v0.4.0

### [Download Here (.zip)](https://github.com/Dteyn/FlatOut4VR-SimHub/releases/download/v0.4.0/FlatOut-4-VR-SimHub-v0.4.0.zip)

[View all releases](https://github.com/Dteyn/FlatOut4VR-SimHub/releases)

## What you need

- Windows 10 or Windows 11
- [FlatOut 4: Total Insanity VR](https://store.steampowered.com/app/3844750/FlatOut_4_Total_Insanity_VR/) **(version 1.87 - May 14 2026 release supported)** - v1.92-develop branch should also work
- [SimHub](https://www.simhubdash.com/) version 9.11.5 or later

## Installation

1. Download and extract the release ZIP from the link above.
2. Close SimHub, then double-click `install.bat`. Alternatively, see [INSTALL.txt](INSTALL.txt) for manual install.
3. Start SimHub and select **FlatOut 4: Total Insanity VR**.

The installer copies the needed files to your SimHub definitions folder and creates the SimHub registration link so the profile is registered in SimHub.

## How it works

The design utilizes SimHub's [External Sim Integration](https://manual.simhubdash.com/external-sim-integration) feature, introduced in SimHub version 9.11.5.

> [!WARNING]
> SimHub describes External Sim Integration as a **beta feature**. Things may break as External Sim Integration continues to be developed.

The overall design uses a companion app ("extractor") which SimHub runs in the background, and which converts telemetry data from FlatOut 4 VR into the format the SimHub accepts.

```text
FlatOut 4 VR JSON telemetry -> extractor.exe -> SimHub External Simulation binary format
```
FlatOut 4 VR outputs telemetry in JSON format at around 39 Hz (see [FLATOUT-4-VR-TELEMETRY.md](docs/FLATOUT-4-VR-TELEMETRY.md)), which requires conversion to SimHub's external simulation format.

SimHub starts the companion app `extractor.exe` automatically when you select the FlatOut 4 VR profile, and closes it when you switch game profiles or exit SimHub.

The extractor receives FlatOut's JSON telemetry at 39 Hz and translates it to SimHub's External Simulation format at 60 Hz by default. It also handles race-end, menu state, and timeouts, so old motion or haptic data is not left running after gameplay stops.

## Settings

Most users will not need to change any settings. If you need to use different ports or enable a diagnostic log, edit `extractor.ini` while SimHub is closed.

| Setting | Default | What it controls |
| --- | --- | --- |
| `ListenIp` | `127.0.0.1` | Address used to receive FlatOut telemetry. Leave this unchanged for the normal local setup. |
| `ListenPort` | `20777` | Port used to receive FlatOut telemetry. Change only if your FlatOut telemetry setup uses a different port. |
| `SimHubIp` | `127.0.0.1` | Address used to send converted telemetry to SimHub. Leave this unchanged unless SimHub is running on another computer. |
| `SimHubPort` | `30777` | SimHub External Simulation UDP port. Change only if the matching SimHub definition is configured for another port. |
| `FixedRateOutput` | `1` | `1` sends a smooth fixed-rate stream; `0` sends updates as FlatOut provides them. The default is recommended. |
| `OutputHz` | `60` | Output frequency when fixed-rate mode is enabled. Accepted range: 10–240 Hz. |
| `SessionTimeoutMs` | `2000` | Time to wait for FlatOut telemetry before sending safe neutral output. Accepted range: 250–10,000 ms. |
| `DefaultMaxRpm` | `8000` | Fallback maximum engine RPM when FlatOut does not provide a usable value. Accepted range: 1–30,000. |
| `MaxGears` | `6` | Maximum gear count reported to SimHub. Accepted range: 1–20. |
| `DebugLogging` | `0` | Set to `1` to create a diagnostic log beside the extractor; keep `0` for normal use. |

Default `extractor.ini`:

```ini
[Extractor]
ListenIp=127.0.0.1
ListenPort=20777
SimHubIp=127.0.0.1
SimHubPort=30777
FixedRateOutput=1
OutputHz=60
SessionTimeoutMs=2000
DefaultMaxRpm=8000
MaxGears=6
DebugLogging=0
```

## Developer Notes

- [Installation guide](INSTALL.txt) — manual installation and troubleshooting.
- [FlatOut 4 VR telemetry reference](docs/FLATOUT-4-VR-TELEMETRY.md) — a breakdown of FlatOut 4 VR's telemetry fields, example values, and mappings to SimHub fields.
- [Architecture notes](docs/ARCHITECTURE.md) — detailed implementation and protocol details for developers.

## Troubleshooting

If something is not working, enable `DebugLogging=1` in `extractor.ini`, reproduce the issue, and open a report in the [issue tracker](https://github.com/Dteyn/FlatOut4VR-SimHub/issues). Please include the generated log where possible.

## Building from source

Building is optional. On a Windows PC with Visual Studio 2022 C++ x64 tools, run:

```bat
build-vs2022.bat Release
package-release.bat
```

The finished release ZIP is written under `out\Packages`.

## License

This project is released under the [MIT License](LICENSE).

## Support the Developer

If you found this project useful, you can buy me a coffee here: https://ko-fi.co/Dteyn

You are visitor: ![Page views](https://dteyn-rad-page.netlify.app/.netlify/functions/pageviews?repo=FlatOut4VR-SimHub)

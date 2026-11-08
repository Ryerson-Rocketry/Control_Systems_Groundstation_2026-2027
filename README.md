# MetRocketry Groundstation

Ground station software for the Mark Walker Memorial Ground Station Competition 2027. It receives telemetry from the rocket **STOP DROP N' ROLL** over a LoRa radio, logs every packet, and shows it live on a retro game-themed dashboard sized for a 7" 1024×600 screen.

The competition rules the software has to follow are in [requirements.md](requirements.md).

## Quick start

The dev container (`.devcontainer/`) has Qt 6, CMake and Ninja installed.

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/groundstation --sim
```

`--sim` flies a simulated rocket, so you don't need the radio. The window opens on the dev container's desktop at **http://localhost:6080**. Click Connect, and enter the password `vscode` if asked.

## Running

| Command | Telemetry comes from |
|---|---|
| `groundstation` | RFM9x LoRa radio on the Pi's SPI bus (flight day; autostart needs no options) |
| `groundstation --sim` | Simulated flight that loops about every 2 minutes (demo stage) |
| `groundstation --i2c` | The radio board's microcontroller on the Pi's I2C bus |
| `groundstation --serial <port>` | A radio behind a USB serial port |

Without `--sim` on a dev machine there's no radio, so the dashboard shows **NO LINK**.

Radio settings: 433 MHz, SF9, 125 kHz bandwidth, coding rate 4/5, sync word 0x12. The rocket uses Adafruit's RFM9x library, so the 4-byte RadioHead header is stripped from each packet. Settings are in [sx127x.h](src/backend/include/sx127x.h) and [lorasource.h](src/backend/include/lorasource.h).

## Tests

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

These are backend tests: the radio driver against a fake SX127x chip, the parser, the logger, the UDP link and the simulator. One test sends a whole simulated flight as raw LoRa packets and checks the values the dashboard receives.

## How it works

```
radio / sim ──> PacketParser ──> TelemetryLogger (CSV on disk)
   (worker thread)    │
                      └──> loopback UDP ──> TelemetryHub ──> MainWindow ──> widgets
                                             (UI thread)    telemetryFromFrame()
```

- **Backend** (`src/backend/`) runs on its own thread so radio input and logging keep going if the UI is busy. Each packet becomes a `TelemetryFrame` in the rocket's own units.
- **Loopback UDP** carries frames to the UI. It's bound to localhost only, because rule 14 bans network control.
- **`telemetryFromFrame()`** in [telemetry.h](src/backend/telemetry.h) is the one place rocket units are converted to display units (ms → s, mg → m/s², Pa → kPa, km/h → m/s). Widgets only ever see a `Telemetry`.
- **Logs**: each run writes `telemetry.csv` in real units to a new timestamped folder under the app data folder (`~/.local/share/MetRocketryControlSystems/Control Systems Groundstation/logs/`). `GroundstationBackend::exportLogs()` copies a run to a USB drive.

### Telemetry packet

One CSV line per LoRa packet, 27 columns of scaled integers, in this order:

`timestamp, flight_state, accel_x/y/z, gyro_x/y/z, mag_x/y/z, roll, pitch, yaw, tilt_angle, altitude, velocity, pressure, temperature, roll_error, pitch_error, pwm_angle, gps_has_fix, gps_latitude, gps_longitude, gps_altitude, gps_speed`

The columns, their units and scale factors are defined once in [telemetryframe.cpp](src/backend/src/telemetryframe.cpp). Change them there and the parser, log and UDP all follow.

### Flight states

| # | State | Dashboard |
|---|---|---|
| 0 | Idle | Kirby walks on the pad |
| 1 | Booster Burn | Kirby puffs up and flies |
| 2 | Canard Activation | Canard roll/pitch error and PWM are live |
| 3 | Apogee | "APOGEE!" |
| 4 | Descent | Kirby opens his parasol; TOUCHDOWN once at ground level and still |

These numbers must match the firmware's `FlightState_t`.

## The dashboard

| Panel | Shows |
|---|---|
| Header | Mission clock, link/error status, packets received and rejected, mode (S = sim, F = flight) |
| Phase track | One ghost per flight state; Pac-Man waits by the current one and eats them as the flight passes them |
| Game & Watch | Rocket photo, state, packets, radio signal strength, pressure, GPS fix, logging |
| Kirby scene | Altitude with a scrolling sky, trail and highest point |
| Live data | Altitude, temperature, pressure, speed, roll/pitch/yaw, tilt |
| Plots | Altitude, temperature, pressure, orientation over the last 3 minutes |
| G.P.S tracker | Rocket track on an offline street map of the URRG launch site near Penn Yan, NY |

Colours, fonts and the stylesheet are all in [theme.cpp](src/ui/src/theme.cpp).

### Sprites and maps

- **Sprites:** drop PNGs in `assets/sprites/` and rebuild. [assets/sprites/README.md](assets/sprites/README.md) lists every name and size. A missing sprite shows as a pink placeholder box.
- **Fonts:** a pixel font in `assets/fonts/` replaces the default font.
- **Street map:** `assets/maps/urrg.json` is OpenStreetMap data. To make one for another launch site, see [tools/maps/convert_osm.py](tools/maps/convert_osm.py).

Everything in `assets/` is compiled into the binary, so the app runs from anywhere, including autostart on the Pi.

## Project layout

```
src/app/            main.cpp: command line, wiring
src/backend/        radio drivers, parser, logger, UDP, simulator, telemetry.h
src/ui/             dashboard widgets (include/ and src/)
tests/              backend tests (ctest)
assets/             sprites, fonts, offline maps
tools/maps/         map builder
requirements.md     competition software rules
```

Build in `build/`, which is git-ignored. Don't build in the repo root.

## Still to do

- [ ] Confirm the flight state numbers against the firmware
- [ ] Test on the Pi with the real radio
- [ ] Wire the physical buttons to USB log export and the tracker's map views (`GpsMap::setView`)
- [ ] Autostart in fullscreen on boot (rules 1–2)
- [ ] Detect lost packets: the packet has no sequence number, so only corrupt packets are counted

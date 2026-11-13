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

Radio wiring (Adafruit RFM96 breakout to the Pi 4B header): VIN to pin 1 (3.3V), GND to pin 6, SCK to pin 23, MISO to pin 21, MOSI to pin 19, CS to pin 24 (CE0) or 26 (CE1), and **G0 to pin 15 (GPIO22)**. Packets are read the moment G0 rises. If G0 isn't connected, the link line says so and the radio is polled instead. RST (pin 22, GPIO25) is optional and off by default (`resetLine` in [lorasource.h](src/backend/include/lorasource.h)).

Radio settings: 433 MHz, SF9, 125 kHz bandwidth, coding rate 4/5, sync word 0x12. The rocket's firmware puts a 4-byte RadioHead-style header on each packet, which is stripped. Settings are in [sx127x.h](src/backend/include/sx127x.h) and [lorasource.h](src/backend/include/lorasource.h).

Reading the dashboard when nothing shows up:

| What you see | What it means |
|---|---|
| **NO LINK**, status line says "Radio not found" | The Pi can't talk to its own radio: check wiring and that SPI is enabled |
| **LIVE**, RX 0, LOST 0, no SG number | Nothing is reaching the Pi: the rocket isn't transmitting, or frequency, antenna or range |
| **LIVE**, RX 0, LOST counting up | Packets arrive but can't be read: the rocket's packet layout doesn't match (see below) |

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

One CSV line of scaled integers per packet. The parser accepts three layouts and tells them apart by the number of fields; any of them may start with the station's callsign (`VE3HIV,`). They are defined in [telemetryframe.cpp](src/backend/src/telemetryframe.cpp) and must match the firmware field for field.

- **Radio packet** (14 fields, what the firmware's `Radio_BuildPacket()` sends once a second): `timestamp, flight_state, accel_x/y/z, gyro_x/y/z, pwm_angle, gps_has_fix, gps_latitude, gps_longitude, gps_altitude, gps_speed`
- **Extended radio packet** (22 fields, not sent by the firmware yet): the same plus `roll, pitch, yaw, tilt_angle, altitude, velocity, pressure, temperature`, in full-row order
- **Full row** (27 fields, the firmware's SD-card row; also the simulator and the wired sources): `timestamp, flight_state, accel_x/y/z, gyro_x/y/z, mag_x/y/z, roll, pitch, yaw, tilt_angle, altitude, velocity, pressure, temperature, roll_error, pitch_error, pwm_angle, gps_has_fix, gps_latitude, gps_longitude, gps_altitude, gps_speed`

A value the packet doesn't carry is never shown as zero: tiles read `--`, plots say NOT IN PACKET, and the log leaves the cell empty. With the 14-field radio packet that is altitude, speed, pressure, temperature and orientation.

The columns, their units and scale factors are defined once in [telemetryframe.cpp](src/backend/src/telemetryframe.cpp). Change them there and the parser, log and UDP all follow.

### Flight states

| # | State | Dashboard |
|---|---|---|
| 0 | Idle (`STATE_PAD`) | Kirby walks on the pad |
| 1 | Booster Burn (`STATE_BOOSTER_BURNOUT`) | Kirby puffs up and flies |
| 2 | Separation (`STATE_SEPARATION`) | |
| 3 | Sustainer Ignition (`STATE_SUSTAINER_IGNITION`) | |
| 4 | Canard Activation (`STATE_CANARDS_ACTIVATE`) | Canard PWM angle is live |
| 5 | Descent (`STATE_DESCENT`) | Kirby opens his parasol; TOUCHDOWN once at ground level and still |

These numbers match `FlightState_t` in the firmware's `Core/Inc/Defs/states.h` and must change with it.

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

- [x] Confirm the flight state numbers against the firmware (matched to the `radio-protsenko` branch)
- [ ] Test on the Pi with the real radio
- [ ] Wire the physical buttons to USB log export and the tracker's map views (`GpsMap::setView`)
- [ ] Autostart in fullscreen on boot (rules 1–2)
- [ ] Detect lost packets: the packet has no sequence number, so only corrupt packets are counted

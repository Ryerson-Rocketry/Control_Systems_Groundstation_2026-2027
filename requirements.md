# Groundstation Software Rules
Source: Mark Walker Memorial Ground Station Competition 2027, Rev 1.0.
All software must follow these. If a feature breaks one, it doesn't ship.

## Startup
1. Software starts automatically on power-up, no operator action.
2. It reaches the operational dashboard on its own.
3. It is ready to receive within a reasonable startup time.

## Input
4. No keyboard or mouse. Every function works from the built-in controls
   (touchscreen or physical buttons).
5. Nothing may require typing. Use presets, pickers, and buttons.
6. Controls are labeled so an unfamiliar operator can run it.

## Telemetry
7. Receive Container and PocketQube telemetry simultaneously.
8. Display both at the same time (GS1).
9. Show all required plots live, each clearly identified.
10. Log all telemetry at the same time.
11. USB memory capture is simple and obvious to use.

## Commanding
12. Transmit every command the mission guide requires, including in flight.
13. The command interface is well organized.

## Isolation
14. No remote control or remote operation in the shipped build
    (no VNC, SSH, or network control). Dev tools only.

## Demo and flight
15. A simulated/test telemetry mode exists for the demo stage.
16. The same software must run on flight day.

## Look
17. Legible and consistent theme, matching the build's style.

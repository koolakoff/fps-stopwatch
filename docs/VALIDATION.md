# FPS Stopwatch — validation record

Date: 2026-10-28. These checks were performed during development from the initial 0.1.0 implementation through v1.0. They are a record of completed checks, not certification on every supported Windows configuration.

## Environment

- Windows build environment: 10.0.22631.
- Microsoft Visual Studio Build Tools 2026, MSVC 19.51, Windows SDK 10.0.26100.0.
- Release x64, statically linked MSVC runtime (`/MT`).
- Primary display: 2560×1600, initial rate 240 Hz; enumerated modes: 60 and 240 Hz.
- Supported platform: Windows 10/11 x64 only. Linux and macOS are outside the project scope.

## Completed checks

- Build completed without compiler warnings.
- CTest clock tests passed: initial zero state, millisecond truncation, long gaps without frames, pause/resume, second/minute rollover, and expansion beyond 99 minutes.
- Fullscreen DXGI/Direct2D application ran on physical hardware.
- Automated checks exercised Space, key-repeat suppression, pause/resume, the mouse theme button, I, and Q.
- Actual back-buffer captures of both themes were inspected: digits were not clipped, milliseconds were larger, the right margin was retained, and rows included Hz, ms, and `(d)` / `(m)` markers.
- Temporary switching from 240 to 60 Hz and restoration to 240 Hz through D succeeded. The implementation verifies resolution when applying a mode.
- Separately checked selection of 60 Hz through the row-click handler and restoration to 240 Hz when exiting with 60 Hz active. The restored frequency was read back through the Windows API.
- Background-click start/pause/resume passed; theme and refresh-row clicks did not toggle the stopwatch.
- Reset clock tests and graphics smoke tests passed for keyboard reset while running and mouse reset while paused, preserving the running/paused state.
- `dumpbin /dependents` reported only system libraries: `d3d11.dll`, `dxgi.dll`, `d2d1.dll`, `DWrite.dll`, `USER32.dll`, `ole32.dll`, and `KERNEL32.dll`. There was no Visual C++ Redistributable dependency. WIC is accessed through system COM.

## Verification limits

- Not yet tested on another computer, a clean Windows 10 installation, fractional refresh rates, a rotated display, HDR, or multi-monitor configurations.
- Frame interval is calculated from the selected mode. Actual displayed-frame cadence has not been measured with an external camera or PresentMon. Frames may be dropped under load.
- Physical FPV measurement accuracy depends on the hardware and is not established by application tests.
- On graphics-device loss, the app exits with an error and attempts to restore the display mode. Automatic graphics-device recreation is not implemented.
- Restoration after forced process termination or an OS crash is not guaranteed.
- Theme selection is not persisted; every launch starts dark.

See [BUILD.md](BUILD.md) for commands and diagnostic output locations.
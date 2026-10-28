# FPS Timer — requirements

Status: requirements for v1.0, consolidated from the discussion on 2026-10-28. The first implementation has been built; verification results and limits are recorded in [VALIDATION.md](VALIDATION.md).

## Purpose

A fullscreen Windows stopwatch for measuring FPV video-system latency. The FPV camera films the computer screen. The user photographs the original screen and the receiver screen together, then subtracts the displayed readings. The result covers the observed camera, transmission, and receiver-display path.

## Platform and technology

- **Windows only:** Windows 10/11 x64. Linux and macOS are outside the supported scope.
- Portable application: one executable, no installation or administrator rights. The C++ runtime is linked statically; graphics and text rendering use built-in Windows components. Development tools are required only for building.
- Stack: C++, Win32, Direct3D 11, DXGI flip model, Direct2D/DirectWrite.
- Borderless fullscreen window; no separate windowed mode.
- Open on the primary monitor using its current resolution and refresh rate.
- Do not change the refresh rate automatically at startup.

## Stopwatch

- Format: `mm:ss:uuu`, where `uuu` is milliseconds with leading zeros, for example `02:17:483`.
- Start at `00:00:000`, paused.
- Large, heavy digits with fixed-width cells. Milliseconds are approximately 1.5 times the size of minutes and seconds.
- Use a shared baseline; digit positions must remain stable as readings change.
- Give the stopwatch as much space as possible without overlapping controls.
- Space starts, pauses, or resumes timing. A left-click outside active controls does the same.
- Use the high-resolution monotonic Windows `QueryPerformanceCounter` clock. Compute readings from elapsed time, not rendered frame count.
- Sample current time when preparing every frame. Dropped frames must not slow the stopwatch.
- After 99 minutes, expand the minute field and adapt the text size without resetting time.
- R and the reset button set time to zero while preserving the running or paused state. Ignore key auto-repeat for reset.

## Display refresh rate

- Show supported refresh rates for the monitor's current resolution in a vertical list on the left.
- Each row shows the nominal frame interval `1000 / refresh_rate_hz` to two decimal places, calculated from the exact mode frequency. Example: `144 Hz · 6.94 ms (d)`; maximum-rate example: `240 Hz · 4.17 ms (m)`. Markers follow the interval. Secondary text may be smaller while remaining legible.
- Outline the active rate. Highlight the initial Windows rate and mark it `(d)`. Mark the highest rate `(m)`. If both are the same, show `(d) (m)`.
- Select the initial rate by default.
- Keep the list compact: typography and row spacing are approximately 20% smaller than the first mockup, with usable mouse targets.
- Up/Down select and apply the adjacent rate without wrapping at the ends.
- Clicking a rate applies it; the mouse wheel scrolls a long list.
- M applies the maximum listed rate; D restores the initial rate.
- Preserve fractional modes such as 59.94 Hz; do not merge distinct modes solely because their displayed integers match.
- Changing rate must not reset or pause the stopwatch.
- Reflect the actually applied mode. If switching fails, retain or restore the previous mode and show a short English error.
- Restore the original display configuration on normal exit and handled failures. Restoration after forced termination or an OS crash cannot be guaranteed.
- Disable rate switching for duplicated displays.

## Frame presentation and measurement limits

- Target a stable fresh reading on every screen refresh with a minimal presentation queue.
- Synchronize presentation with the screen and avoid tearing. Unlimited off-screen rendering FPS is not the goal.
- Do not drive rendering with an ordinary fixed-interval GUI timer.
- In the lower-right corner, show `1000 / refresh_rate_hz` in milliseconds to two decimal places, for example `6.94 ms` at 144 Hz.
- Label this value `frame interval`. It is the nominal interval of the selected mode, not measured FPS or a guaranteed FPV-system error bound.
- Millisecond formatting does not imply 1 ms physical accuracy. Display scanout/response, camera exposure/readout, and variable pipeline latency affect the result.

## Controls and appearance

- All interface text, hints, errors, and project documentation are in English.
- List heading: `REFRESH RATE`. Units: `Hz` and `ms`.
- Show `v1.0` at the left of the bottom keyboard-hint line.
- Keep the stopwatch's right edge approximately 8% of screen width from the screen edge. Keep top-right buttons and the lower-right interval at least 3% from the right edge.
- Refresh-rate panel width: 285 design units at a 1920×1080 reference resolution.
- Reset is a bold circular-arrow icon at the bottom left below the frequency panel. Its button occupies half the panel width. Use a filled, wide triangular arrowhead that remains distinct from the thick arc.
- Adapt the timer size while preserving the relative digit sizes.

| Action | Keyboard | Mouse |
|---|---|---|
| Start / pause / resume | Space | Left-click outside exit, theme, reset, and refresh-rate controls |
| Reset to zero | R | Circular-arrow button below the refresh-rate panel |
| Exit | Esc, Q, or Alt+F4 | Close button at the top right |
| Toggle theme | I | Half-circle button beside close |
| Select refresh rate | Up / Down | Click a rate; scroll a long list with the wheel |
| Maximum refresh rate | M | Select the maximum-rate row |
| Initial refresh rate | D | Select the initial-rate row |

- Clicking an active control must not also toggle the stopwatch.
- Ignore key auto-repeat for start/pause, reset, and theme changes.
- Two themes with immediate transitions; start in the dark theme on each launch.
- Scale to screen resolution and DPI without clipping digits or overlapping controls.
- Buttons have usable hit areas and hover feedback.
- Keyboard hints are small and secondary.
- Theme persistence is not included in v1.0.

| Element | Dark theme | Light theme |
|---|---|---|
| Background | `#101418` | `#F2F0E8` |
| Main digits and selection frame | `#F2F0E8` | `#101418` |
| Initial-rate highlight | `#80CBC4` | `#00695C` |

A strict `#000000` / `#FFFFFF` pair was discussed as a possible extension and is not included in v1.0.

## Design references

The mockups illustrate 144 Hz and `02:17:483`. Their sample modes do not imply support on any particular monitor. They are historical design references; the implemented layout includes the later changes specified above.

- Revised dark mockup: [mockups/dark-v2.png](mockups/dark-v2.png).
- Revised light mockup: [mockups/light-v2.png](mockups/light-v2.png).
- Original mockups: [mockups/dark.png](mockups/dark.png) and [mockups/light.png](mockups/light.png).
- Generation notes: [mockups/PROMPTS.md](mockups/PROMPTS.md).
- Application screenshot: [screenshots/fps-timer-dark.png](screenshots/fps-timer-dark.png).

## Acceptance criteria

1. Open fullscreen in the initial display mode with a zeroed, paused stopwatch.
2. Space and background clicks correctly start, pause, and resume timing independently of frame count.
3. Preserve the required format, baseline, stable digit cells, larger milliseconds, and screen margins.
4. Make actual refresh modes selectable by keyboard and mouse, with an active frame, initial highlight, per-row intervals, and appropriate `(d)` / `(m)` markers.
5. M and D apply their respective modes and update the interval readout.
6. I and the theme button change palette. Esc, Q, Alt+F4, and close exit with an attempt to restore the initial configuration.
7. R and reset zero the reading without changing the running/paused state.
8. Dropped frames do not produce stopwatch drift. Clock accuracy and display presentation are verified separately.
9. Verify display switching, layout/DPI, focus behavior, and normal-exit restoration on available hardware, recording untested cases in the validation report.
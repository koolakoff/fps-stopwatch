# Mockup generation notes

Created with the built-in image_gen tool, using the `ui-mockup` category. The light mockup uses the dark mockup as a visual reference.

These are historical design references, not screenshots of the current application. The earliest images contain Russian labels. The descriptions below translate those labels into English; the original image files are preserved. Current requirements are in [SPECIFICATION.md](../SPECIFICATION.md).

## Version 2 refinements

Edit the first mockup using an English interface (`REFRESH RATE`, `frame interval`, and English keyboard hints). Reduce frequency-list font size and row spacing by about 20%. Use the exact entries `144 Hz (d)` and `240 Hz (m)`. Set the stopwatch's right margin to 5% of screen width and the buttons' and interval's right margin to 3%.

Preserve the dark palette. The light version uses the same geometry with background `#F2F0E8`, text `#101418`, and accent `#00695C`. A follow-up edit emphasized that the markers must be visible and reduced the stopwatch by 7%, anchored at the left edge, to increase the right margin.

The implemented app later increased the right margin to 8%, narrowed the frequency panel, added per-row intervals, a bottom-left reset icon, and a version label.

## Dark mockup — translated prompt

```text
Use case: ui-mockup.
Create a precise flat fullscreen Windows stopwatch application screenshot, landscape 16:9, no monitor bezel, desktop, gradients, or shadows. Minimal functional measurement instrument.

Huge heavy tabular sans-serif stopwatch centered in the area to the right of a narrow left frequency list: exact text "02:17:483". The "483" milliseconds are 1.5 times the font size of "02:17:", all on one baseline. The full time occupies most of the screen width without overlapping the sidebar.

Left-aligned narrow frequency list, small heading "REFRESH RATE", entries "60 Hz", "120 Hz", "144 Hz", "165 Hz", "240 Hz". Select 144 Hz with a thin rectangular border and turquoise text plus a small "DEFAULT" label.

Top right: two small unfilled mouse buttons, a half-filled circle theme icon and close ×.
Bottom right: "6.94 ms" with small label "frame interval".
Bottom left: discreet shortcut hint "Space — start / pause    ↑↓ — refresh rate    M — max    D — default    I — theme".

Dark theme: uniform graphite background #101418, warm off-white digits and text #F2F0E8, turquoise original frequency #80CBC4. Crisp typography. No other decoration.
```

## Light mockup — reconstructed description

The original saved light-prompt entry was missing and contained only `undefined`. The following describes the requested palette change; it is not a verbatim recovered prompt.

```text
Use the dark mockup as the visual reference. Preserve its layout, stopwatch typography, frequency list, controls, and labels. Change the palette to a uniform warm off-white background #F2F0E8, graphite digits and text #101418, and dark turquoise original-frequency highlight #00695C. Crisp typography; no texture.
```
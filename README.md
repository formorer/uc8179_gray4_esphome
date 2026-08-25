# uc8179_gray4 — 4-level grayscale driver for Seeed 7.5" UC8179 e-paper

ESPHome external component that drives 800x480 7.5" e-paper panels with a
UC8179 controller in **4 grayscale levels** (white, light gray, dark gray,
black) instead of plain black/white. Tested targets:

- **Seeed XIAO ePaper Driver Board + 7.5" panel** (XIAO ESP32-C3,
  Seeed_GFX setup 502) — see [test-gray4.yaml](test-gray4.yaml)
- **Seeed reTerminal E1001** (ESP32-S3, GDEY075T7 panel, Seeed_GFX setup 520) —
  example: [example-e1001.yaml](example-e1001.yaml); German step-by-step
  guide: [ANLEITUNG-E1001.md](ANLEITUNG-E1001.md)

Both devices use the same component — only the pins and board type in the
YAML differ.

**⚠️ Check your controller variant:** the XIAO 7.5" panel shipped in two
batches — UC8179 **or** JD79686B (Fitipower). This component only works with
the UC8179 variant; the JD79686B has no grayscale support in Seeed's own
reference (Seeed_GFX). The marking on the panel's flex cable tells you which
one you have — or simply run the stripe test: if you don't get a clean
4-level image (in either `lut_mode`), it is probably the JD79686B batch.

The UC8179 command sequences (gray init, LUTs, two-plane transmission) are
ported verbatim from [Seeed_GFX](https://github.com/Seeed-Studio/Seeed_GFX)
(`TFT_Drivers/UC8179_Defines.h`); the component skeleton follows
[parkghost/esphome-epaper](https://github.com/parkghost/esphome-epaper).

## Usage

```yaml
external_components:
  - source: github://formorer/uc8179_gray4_esphome
    components: [ uc8179_gray4 ]

# XIAO ESP32-C3 + ePaper Driver Board (see above for reTerminal E1001)
spi:
  clk_pin: GPIO8
  mosi_pin: GPIO10

display:
  - platform: uc8179_gray4
    cs_pin: GPIO3
    dc_pin: GPIO5
    reset_pin: GPIO2
    busy_pin:
      number: GPIO4
      mode:
        input: true
        pullup: true
    lut_mode: otp         # or "custom", see below
    update_interval: never
    lambda: |-
      it.filled_rectangle(0,   0, 800, 120, Color(0, 0, 0));
      it.filled_rectangle(0, 120, 800, 120, Color(85, 85, 85));
      it.filled_rectangle(0, 240, 800, 120, Color(170, 170, 170));
      it.filled_rectangle(0, 360, 800, 120, Color(255, 255, 255));
```

Full examples: [test-gray4.yaml](test-gray4.yaml) (XIAO),
[example-e1001.yaml](example-e1001.yaml) (reTerminal E1001).

## Color mapping

`Color(r,g,b)` is quantized to 4 levels by luminance (Rec. 709 weights,
thresholds at 64/128/192):

| Luminance | Level | Rendered as |
|---|---|---|
| 0–63 | 0 | black |
| 64–127 | 1 | dark gray |
| 128–191 | 2 | light gray |
| 192–255 | 3 | white |

There is no on-device dithering — pre-dither images (`image:` /
`online_image:`) with [tools/dither4.py](tools/dither4.py) or accept the
plain quantization:

```sh
python3 tools/dither4.py photo.jpg images/photo.png
```

## `lut_mode`: custom vs. otp

The panel shipped in two batches with different waveform sources. Seeed_GFX
detects the batch at runtime by reading back the panel OTP — that requires
bidirectional SPI over the MOSI pin and is intentionally not ported here.
Pick it via config instead:

- `custom` (default): uploads Seeed's grayscale LUTs into the registers
  (original/older panels).
- `otp`: uses the panel-internal OTP waveform (newer batches).

**If gray levels look washed out or the shades come out in the wrong order,
try the other mode.** A fully inverted image is a different problem — see
`invert_colors` below.

## `invert_colors`

Some panels drive the two data planes with the opposite polarity and render
everything as a photographic negative: white text on a black page. Set

```yaml
    invert_colors: true
```

and the driver complements both planes, mapping every level `v` to `3 - v`,
which cancels the panel out. Observed on a reTerminal E1001 whose panel
inverts in **both** `lut_mode` settings — so if switching `lut_mode` changes
nothing, this is the knob you want.

Fix it here rather than by inverting whatever generates the image: the
polarity is a property of the individual panel, so a server-side or
image-side workaround inverts it for every other device too.

Field test (2026-07, XIAO ePaper Driver Board, UC8179 panel batch): both
modes render clean 4-level output; `otp` has slightly better contrast
(deeper black) and is the recommendation on this device.

## Behavior & limitations (v1)

- **Full refresh only.** The grayscale waveform takes several seconds and
  flashes. No partial update in gray mode (the Arduino reference cannot do it
  either). For frequent updates choose a generous `update_interval`, or use
  `never` plus manual `component.update`.
- The panel enters deep sleep automatically after every refresh (battery
  friendly); each update cycle starts with reset + re-init.
- Framebuffer: 2 bits/pixel = 96 kB RAM.
- `it.fill(...)` / auto-clear: ESPHome's auto-clear fills with `COLOR_OFF`
  (= black in luminance semantics). For a white background, start the lambda
  with `it.fill(Color(255,255,255));` — or paint the whole area anyway, as in
  the example.
- The busy pin is active low and handled by the driver. Do **not** set
  `inverted: true` in YAML.

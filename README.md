# musicalFlower

A real-time music-reactive flower field visualization built with openFrameworks and Essentia. Audio is captured from system playback (e.g. a browser playing music), analyzed for pitch, volume, and spectral content, and used to drive a generative field of flowers that bloom, sway, rotate, and shed petals in response to the music.

## Dependencies

- **openFrameworks** (tested with oF 0.12.x) — must be installed at `../../..` relative to this project (standard oF app layout: `apps/myApps/musicalFlower/`)
- **Essentia** (v2.1-beta6-dev) — audio analysis library, with headers and shared library accessible
- **PipeWire / PulseAudio** — for automatic audio routing from system playback
- **FFTW3** (`libfftw3f`), **libyaml** — Essentia runtime dependencies

## Building

```bash
# From the project directory
make -j$(nproc)

# Run
cd bin && ./musicalFlower
# or
make RunRelease
```

The build system is the standard oF Makefile workflow. Essentia include/library paths are configured in `config.make`.

## How It Works

### Audio Pipeline

The app captures mono audio at 44100 Hz via oF's sound stream. On startup, it automatically routes audio from an active PulseAudio/PipeWire playback sink (e.g. Firefox playing music) into its input using `pactl`.

Each frame, a 2048-sample window is processed through Essentia:

- **Windowing** (Hann) -> **Spectrum** -> **PitchYinFFT** for pitch and confidence
- **RMS volume** computed directly from the input buffer
- **Spectral fullness** derived from the spectrum energy distribution

### Flower Field

300 flowers (default) are rendered with full lifecycle animation:

1. **Growing** (0-15%) — stem and head scale up
2. **Blooming** (15-60%) — full music reactivity: volume pulses petal length, pitch modulates pointiness
3. **Losing petals** (60-80%) — petals detach one by one and fall with physics
4. **Wilting** (80-95%) — stem droops, head shrinks
5. **Dead** (95-100%) — fade out, then respawn as a new flower

Each flower has randomized properties: position, petal shape, head type, stem structure, tendrils, colors, and music-reactivity personality.

### Five Flower Head Types

| Type | Description |
|------|-------------|
| **Radial** | Classic even-spaced petals (like a daisy) |
| **Phyllotaxis** | Golden angle spiral (like a sunflower) |
| **Rose Curve** | Polar `r = cos(k*theta)` modulation creating lobed patterns |
| **Superformula** | Gielis superformula for exotic organic envelopes |
| **Layered Whorls** | Concentric petal rings with phase shifts (like a zinnia) |

All types support Perlin noise distortion for organic wobble.

### Four Center Types

| Type | Description |
|------|-------------|
| **Simple Disc** | Solid circle |
| **Stamens** | Radiating lines with pollen tips |
| **Pollen Grid** | Mini phyllotaxis pattern of dots |
| **Geometric Star** | Star polygon |

Center type is assigned based on head type (e.g. phyllotaxis flowers get pollen grid centers).

### Music Reactivity

- **Volume** drives petal length pulsing and rotation speed scaling
- **Pitch** modulates petal pointiness (per-flower random direction)
- **Spectral fullness** controls lifecycle speed (busy spectrum = faster bloom/decay)
- **Beat detection** (volume onset) triggers rotation direction flips

### Falling Petals

When petals detach during the losing-petals phase, they become independent physics objects with gravity, horizontal wavering, tumbling rotation, and fade-out.

## Controls

### Keyboard

| Key | Action |
|-----|--------|
| `D` | Toggle debug mode (spectrum + pitch visualization) |
| `Space` | Toggle reactive mode (dynamic flower count driven by music activity) |
| `0` | Color mode: cycling (default) — rotates through all 8 schemes sequentially |
| `1`-`8` | Color mode: lock to a specific color scheme |
| `9` | Color mode: random — each new flower picks a random scheme |

### Reactive Mode

When activated with `Space`, the flower count scales dynamically with musical activity (30-1500 flowers). Activity is a composite score of beat density, volume, and spectral fullness over a sliding 5-second window.

- **High activity**: many flowers spawn with fast rotation
- **Low activity**: flowers thin out via dramatic fast-death animation (petal burst + stem collapse)
- **Toggle off**: field gradually returns to the base count of 300

### Color Schemes

Eight palettes spaced around the color wheel:

| Key | Name | Palette |
|-----|------|---------|
| 1 | Sunset | Warm orange-reds |
| 2 | Golden | Amber-yellows |
| 3 | Emerald | Greens |
| 4 | Ocean | Teal-cyans |
| 5 | Arctic | Ice-blues |
| 6 | Twilight | Indigo-purples |
| 7 | Orchid | Violet-magentas |
| 8 | Rose | Pinks |

All schemes use complementary colors for the flower center (hue + 180 degrees). Stems are tinted green with a subtle shift toward the scheme's hue.

## Project Structure

```
src/
  main.cpp        Window setup (1024x768)
  ofApp.h/.cpp    Application loop, Essentia pipeline, audio routing, mode switching
  Flower.h/.cpp   All flower types, stem, falling petals, flower field, color schemes
```

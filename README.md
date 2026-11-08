# After Burner Climax Recomp (Recharged)

A native Windows port of **After Burner Climax** (SEGA AM2, Xbox 360 XBLA, 2010), made by static recompilation: the
game's PowerPC code is translated to C++ and compiled for x86-64, and its Direct3D calls are drawn with Vulkan by
[NativeXRenderer](https://github.com/ElGuslml/NativeXRenderer) instead of an emulated GPU.

**No game data is included.** You need your own After Burner Climax XBLA package, World release (the Xbox 360 LIVE file
`58410A06\000D0000\C0B8A17E2A438BBE7D63573A04652DA71EE5DE5558`, default.xex 0.0.1.2).

## Features
- Native code (ReXGlue SDK v0.10.1) and native drawing (NativeXRenderer, Vulkan) at the game's own 60 FPS.
- Any resolution: render scale up to 3x (2560x1440 and 3840x2160 from the 1280x720 original), SMAA or FXAA,
  texture filtering up to 16x, screen sync choices (fast without tearing, V-Sync, or immediate for G-Sync/FreeSync).
- Shader cache (`shaders.cache` next to the exe): every shader the game ships is translated on the first start,
  behind a progress screen; later starts load it in a fraction of a second.
- Settings menu on **F1 / Esc** (pauses the game), playable with the controller, keyboard or mouse.
- Performance overlay: FPS, CPU and GPU use, frame-time graph.
- Keyboard play with rebindable keys; Xbox and other controllers through SDL.
- Windowed, borderless fullscreen or exclusive fullscreen (F11 toggles).
- Clean exit (a watchdog ends the process if the SDK's shutdown stalls) and a crash report (`crash.txt`).

## Playing
1. Download `after_burner_climax-win-amd64` from the newest successful run of **Build Windows** in the Actions tab
   (or a release), and unzip it.
2. Put your package file, or the whole `58410A06` folder copied from the console, next to `after_burner_climax.exe`.
3. Run `after_burner_climax.exe`. The first start extracts the game into an `assets` folder (the package is not
   needed after that) and builds the shader cache.

Requirements: 64-bit Windows 10/11, a GPU with Vulkan 1.3, and the
[Microsoft Visual C++ Redistributable 2015-2022 (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).

### Settings (F1 or Esc)
LB/RB (keyboard Q/E) switch category, Up/Down pick a row, Left/Right or A/Enter change it, B/Backspace close.
Saved to `after_burner_climax.toml` next to the exe (editable by hand while the game is closed).

| Category | Settings |
|---|---|
| Display | Display mode (Windowed / Borderless fullscreen / Fullscreen), window size, render resolution, screen sync |
| Graphics | Anti-aliasing (SMAA / FXAA / off), texture filtering, new shaders (how long a frame may wait for a first-time shader compile) |
| Audio | Audio buffer size |
| Controls | Keyboard on/off, edit keyboard keys |
| System | Performance overlay, log file, close menu, quit |

### Default keys
| Action | Controller | Key |
|---|---|---|
| Fly (flick left/right = barrel roll) | Left stick | Arrow keys |
| Machine gun | A | Z |
| Missiles | B | X |
| Climax mode | LB / RB | C / V |
| Brake / accelerate | LT / RT | A / S |
| X / Y | X / Y | Q / W |
| Start / Back | Start / Back | Enter / Backspace |

F1 / Esc: settings. F11: fullscreen.

## Building
GitHub Actions builds on every push to `main` or `nativex` (`.github/workflows/build-windows.yml`, about 10 minutes):
the ReXGlue SDK from source at v0.10.1 with `patches/rexglue-sdk` and `patches/ffmpeg`, NativeXRenderer (submodule)
with `patches/native_x_renderer`, then the game with clang and Ninja. See [docs/architecture.md](docs/architecture.md)
for how the pieces fit, and [docs/development-log.md](docs/development-log.md) for the history.

## Documentation
| File | What |
|---|---|
| [docs/architecture.md](docs/architecture.md) | How the port works: start-up, every source file and its functions, settings, CI |
| [docs/native-x-renderer.md](docs/native-x-renderer.md) | The game's Direct3D map for NativeX: hooks, device layout, verification, what is left |
| [docs/development-log.md](docs/development-log.md) | What was done, why, and the evidence |
| [docs/HANDOFF.md](docs/HANDOFF.md) | Current state and the next steps |

## Credits
- [darkuni](https://github.com/darkuni/afterburner-climax-recomp): the original After Burner Climax recompilation
  this project grew from (codegen set-up, overrides, the first menu and audio fixes).
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) (BSD-3-Clause, portions from [Xenia](https://xenia.jp)).
- [NativeXRenderer](https://github.com/ElGuslml/NativeXRenderer) by Gustavo (ElGuslml), with DiligentCore.
- The shader cache, settings menu and performance overlay come from this author's
  [Mad Riders recomp](https://github.com/Gandalf4212/mad-riders-recomp).

## Disclaimer
Unofficial fan project, not affiliated with or endorsed by SEGA or Microsoft. No game data (graphics, sound, levels)
is in this repository or its builds: it comes from your own package. `generated/` and the exe contain code
machine-translated from the game's executable. The project's own code is under the [MIT License](LICENSE); the
licenses of the SDK, NativeXRenderer and their third-party parts ship in the build's `licenses` folder.

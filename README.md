# After Burner Climax Recompiled

Static recompilation of **After Burner Climax** (Xbox 360 XBLA, SEGA AM2, 2010; Title ID 58410A06, default.xex 1.0.0.0 built 2009-12-21) to native Windows, using the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) v0.9.0.

Two releases are supported, each recompiled separately (their code sits at different addresses):

| Release | default.xex | Exe |
|---|---|---|
| World (retail XBLA package) | 0.0.1.2, 2,293,760 bytes, SHA-1 `a066b2d6…` | `after_burner_climax_world.exe` (code in `world/generated`) |
| Original | 1.0.0.0, 2,297,856 bytes, SHA-1 `6f8ba4d7…` | `after_burner_climax.exe` (code in `generated`) |

Run `after_burner_climax.exe` either way: it checks your `default.xex` and starts the World exe if that's what you have.
Any other version gets a message naming the supported ones.

**No game data is included.** You need your own After Burner Climax XBLA package (the Xbox 360 LIVE file from
your console's storage).

## Download
Get the latest `AfterBurnerClimax-<version>-win64.zip` from [Releases](https://github.com/darkuni/afterburner-climax-recomp/releases),
unzip it, put your package file (or the `58410A06` folder copied from the console) in the same folder and run `after_burner_climax.exe`: the first start makes the
`assets` folder from it. Nothing else to install. Requirements: 64-bit Windows 10/11 and the
[Microsoft Visual C++ Redistributable 2015-2022 (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).

## Building
GitHub Actions builds on every push to `main` (or run **Build Windows** by hand from the Actions tab); publishing a
release attaches the zip to it. Unzip a build and put your extracted game files in an `assets` folder next to the exe
(`tools/stfs_extract.py <package> assets` extracts the package):

```
after_burner_climax.exe
*.dll
assets\
  default.xex
  media\...
```

## In-game settings (F1 or Esc)
Same menu as the Poker Smash / ReBirth recomps: display mode (also F11), window size, render resolution, VSync, aspect ratio, texture filtering, audio buffer, keyboard on/off, per-button key rebinding, quit. Saved to `after_burner_climax.toml` next to the exe. F3 = performance overlay. Runs as the full game.

### Default keyboard layout
| Action | Controller | Key |
|---|---|---|
| Fly (flick left/right = barrel roll) | Left stick | Arrow keys |
| Machine gun | A | Z |
| Missiles | B | X |
| Climax mode | LB / RB | C / V |
| Brake / accelerate | LT / RT | A / S |
| X / Y | X / Y | Q / W |
| Start / Back | Start / Back | Enter / Backspace |

## Layout
| Path | What |
|---|---|
| `after_burner_climax_manifest.toml` | ReXGlue project manifest (codegen input) |
| `after_burner_climax_overrides.toml` | Missed branch targets + pointer-only functions |
| `generated/` | Recompiled C++ output of `rexglue codegen` |
| `src/after_burner_climax_app.h` | App class - game folder default, Xenos GPU plugin, full-game license, first-launch defaults, menu/F11 binds |
| `src/game_menu.*`, `src/game_keys.h` | F1/Esc settings menu and this game's key layout |
| `tools/` | Package/XEX extraction (STFS, XEX2 basic + LZX), pointer-function scan, label check |
| `.github/workflows/build-windows.yml` | CI: LLVM + prebuilt SDK (darkuni/rexglue-sdk-builds, v0.9.0-p23) + game; uploads the package, attaches it to releases |

## Codegen fixes so far
- `0x824A7C68`, `0x824D9DD8`, `0x82441BA8`, `0x824D92B0`: small thunks reached only by `b` from other functions.
- 58 functions reachable only through vtables/callbacks (`tools/scan_pointer_funcs.py`).

## Disclaimer
Unofficial fan project, not affiliated with or endorsed by SEGA or Microsoft. This repository and its releases contain
no game data (graphics, sound, levels): those come from your own package. `generated/` and the release exe contain code
machine-translated from the game's executable. The project's own code is under the [MIT License](LICENSE); the ReXGlue
SDK is BSD-3-Clause (portions derived from Xenia), and its licenses ship in the release zip.

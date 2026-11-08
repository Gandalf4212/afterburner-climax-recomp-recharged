# Handoff (2026-11-08)

State of branch `nativex` at the end of session 1, and what to do next. Update this file at the end of each session.

## Done
| Commit | What |
|---|---|
| 07102ea | ReXGlue v0.10.1 from source (was v0.9.0-p23 prebuilt); World release only (Original 1.0.0.0 exe and `world/` removed); code regenerated (0 errors, the same 24 unresolved-branch traps as before); XMA start/end skip ported (SDK patch 0010 + FFmpeg patch); SDK glue-name fix (0011); exit watchdog; CI. **CI passed (9.5 min).** |
| 1940c8f | F1 menu: Mad Riders layout + Afterburner's flight keys/rebinding page; Renderer row (`recomp_renderer`: xenia default / nativex); render res & VSync follow the renderer; AA / New shaders rows (NativeX only); Log file row; `config_file.h` writes all settings under the ASCII banner. |
| 099e5c7 | `docs/native-x-renderer.md`: D3D map of XDK 9328. |
| 9f958c5 | NativeXRenderer submodule (bf965e9) built in; `src/native_x_renderer_abc.cpp` hooks + profile; CI build was running at handoff. |

## Key files
- `src/after_burner_climax_app.h`: app hooks (paths, xex check, renderer switch `ApplyRenderer`, log switch, NativeX `Initialize`, menu, exit watchdog arming).
- `src/game_menu.cpp/.h`, `src/game_keys.h`, `src/config_file.h`: F1 menu, key layout, config writer.
- `src/exit_watchdog.h`: forced exit 3 s after a close request (see why in the file / log).
- `src/native_x_renderer_abc.cpp`: NativeX entry points and `DeviceLayout`/`ObjectLayout`.
- `CMakeLists.txt`: `ABC_SKIP_CODEGEN` (builds from committed code when assets/default.xex is absent), needs `REXSDK_DIR`.
- `.github/workflows/build-windows.yml`: artifact `after_burner_climax-win-amd64`.

## Waiting on the owner (tests on Windows)
1. Xenia path (default): boots, plays, audio OK (XMA skip port: listen for clicks on looping sounds), quitting
   ends the process. Log: "Exit watchdog armed", then "Title terminated; hard-exiting process." or
   "Exit watchdog: shutdown did not finish" (tells which stage stalls).
2. F1 > Log file On, restart with `--native_x_renderer_verify=true`, fly into a stage, press F10; send the log
   (report ~10 frames later). Optional: `--native_x_renderer_trace=true` + F9 dumps a frame.
3. Optional: F1 > Renderer = NativeX, restart: what draws.

## Next steps
1. Read the verify report: fix wrong offsets in the profile, add what it says is missing. Not mapped yet: VS/PS
   constant setters (no plain function writes `+1920 + 16*start`; likely inlined in the engine: scan for
   `ld/or/std` on device `+0`/`+8`), Begin/EndVertices (draws touch `+13436/+13440`), buffer Lock/Unlock,
   TextureUnlock, NewSegment, SetShaderGPRAllocation, occlusion queries (EVENT_WRITE_ZPD in engine code
   `82298838`, `8232AC70`), the two other draw functions `82249308`/`822496E0` (UP draws?), resource object
   layout (assumed 7978's). Candidates to confirm: SetViewport `8223F300`, BeginTiling `8224DC78`,
   EndTiling `82248DB0`. Method: NativeX `docs/integrating-a-game.md`; Mad Riders' table is a good model.
2. When NativeX draws correctly: make it the default renderer.
3. Shader cache (Mad Riders `patches/native_x_renderer/0001-0004`, `src/shader_scan.cpp`): tested against
   bf965e9, none apply as-is. 0001 (scene size/AA) overlaps upstream D126 (merge by hand); 0002 (cache core,
   ~2500 lines) only 1 code hunk + docs; 0003/0004 after those. Rebase on a test branch: apply on 5aa5a05,
   `git rebase bf965e9`. A shader scanner for this game's data format must be written (Mad Riders' reads
   `.rpack`). Check that frame generation / DLSS (upstream D116-D124) don't conflict with pre-launch preparation.
4. Rewrite README.md and all docs for this build (credit darkuni's original work); README still describes the
   old two-exe build.
5. Before major features: check rexglue/rexglue-sdk tags (`git ls-remote`) and NativeXRenderer main.
6. Roadmap (last): launcher that finds/asks for the package (file dialog), shows an attractive import screen with
   progress, extracts assets, runs codegen and builds the game locally so the repo holds no game-derived code;
   on update, recompile against an existing assets folder when no package is present.

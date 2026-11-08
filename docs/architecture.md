# Architecture

How After Burner Climax Recomp (Recharged) is put together: what runs at start-up, what each source file does, and
how the build is made. For the Direct3D map see [native-x-renderer.md](native-x-renderer.md).

## The pieces
| Piece | Where | Role |
|---|---|---|
| Recompiled game code | `generated/` (from `rexglue codegen`, committed) | The game's PowerPC code as C++: one function per guest function (`sub_XXXXXXXX`) |
| Codegen input | `after_burner_climax_manifest.toml`, `after_burner_climax_overrides.toml` | Project manifest; missed branch targets and functions reachable only through pointers |
| ReXGlue SDK v0.10.1 | built from source in CI + `patches/rexglue-sdk` | The Xbox 360 runtime: kernel, memory, threads, audio (XMA via FFmpeg + `patches/ffmpeg`), input (SDL), file system, window, ImGui |
| NativeXRenderer | `thirdparty/native_x_renderer` (upstream `bf965e9`) + `patches/native_x_renderer` | Draws the game's D3D calls with Vulkan (DiligentCore) |
| The app | `src/` | Start-up, settings, menu, hooks for NativeX, shader cache source, pause, overlay, exit and crash handling |

The SDK's Xenos GPU plugin is still loaded: its command processor executes the game's GPU commands, which NativeX
observes; it draws only if NativeX cannot start on the GPU.

## Start-up (in order)
1. `main.cpp`: the SDK's entry point creates `AfterBurnerClimaxApp`.
2. `OnConfigurePaths`: installs the crash handler; applies the log switch (`ApplyLogSwitch`); defaults the game data to
   `assets` next to the exe and, on the first start, extracts it from the package found next to the exe
   (`package_setup_first_run`); checks that `assets\default.xex` is the World 0.0.1.2 executable (`xex_version_check`,
   SHA-1 and size) or stops with a message.
3. `OnPreSetup`: logs the build; `ApplyRenderer` (NativeX on, the SDK's shader storage off); `frame_rate::apply` (1 ms
   timer resolution, guest refresh 60 Hz); the Xenos plugin; full-game license; first-launch and versioned defaults
   (`recomp_defaults_version`), each written to the config once.
4. `OnCreateDialogs`: the performance overlay, the shader cache screen, and the F1 / Esc / F11 binds.
5. `OnPostSetup`: the crash handler again (set-up may replace it); `SetUpShaderCache`; `native_x_renderer::Initialize`
   with the game's profile; the input callback.
6. `OnPostLaunchModule`: the game's main thread is held while NativeX prepares the shader cache, then resumed.

## Source files
| File | Contents |
|---|---|
| `main.cpp` | Entry point (SDK macro) |
| `after_burner_climax_app.h` | `AfterBurnerClimaxApp`: the start-up hooks above; `ToggleMenu` (opens/closes the menu, blocks the game's input with `BlockGameInput`, pauses it with `game_pause::set`); `SetUpShaderCache`; `OnWindowCloseRequested` (unpauses, arms the exit watchdog) |
| `native_x_renderer_abc.cpp` | NativeX hooks (`NATIVE_X_RENDERER_HOOK(address, Call)`) and `NativeXRendererProfile()` (device layout, object layout, output size). The Swap hook (`82252730`) is written out: it waits while paused, counts the frame for the overlay, and reports Present to NativeX |
| `shader_scan.cpp/.h` | `shader_scan::scan` reads `media\x360\shader_hlsl.farc` (SEGA FArC: entry table, gzip per entry), inflates each entry with stb_image's zlib decoder and collects the XDK shader objects; `shader_scan::fingerprint` (file names and sizes) tells NativeX when to scan again |
| `game_menu.cpp/.h` | `recomp_ui::GameMenu`: the F1 menu (ImGui draw list, blue theme, five categories, controller via `MenuHost::pad_buttons`). `Change(row, dir)` edits one setting and saves; `DrawMain`, `DrawKeys` (key rebinding); `ApplyDefaultKeys` |
| `game_keys.h` | This game's keyboard layout (`kKeyBinds`) and the SDK binds it leaves unbound |
| `config_file.h` | `recomp_ui::save_config`: writes `after_burner_climax.toml` with the banner and every menu setting, by section, with comments |
| `game_pause.h` | `game_pause::set` / `wait_while_paused`: the game's main thread waits at its next frame start while the menu is open; NativeX re-presents the last frame (its D132) |
| `perf_overlay.cpp/.h` | `recomp_ui::PerfOverlay` and `perf_on_frame`: FPS from the game's own frames, CPU and GPU use (Windows performance counters, sampled once a second), frame-time graph |
| `frame_rate.cpp/.h` | `frame_rate::apply`: 1 ms timer resolution; the guest refresh is held at 60 Hz because the game steps its logic once per frame |
| `package_setup.cpp/.h` | STFS (LIVE/CON) package extraction with block hash checks (`package_extract`, `package_setup_auto`, `package_setup_first_run`) and `xex_version_check`. Also builds as a command-line tool (`--auto`) |
| `exit_watchdog.h` | `exit_watchdog::arm`: ends the process 3 s after a quit if the SDK's shutdown stalls; logs which stage it reached |
| `crash_handler.h` | Unhandled-exception filter: exception, faulting module and offset, the stack (x64 unwind), to the log and `crash.txt` |

## Settings (`after_burner_climax.toml`)
Every setting is an SDK or NativeX cvar, saved by the menu on each change; command-line `--name=value` overrides it.

| Setting | Cvar | Values |
|---|---|---|
| Display mode | `fullscreen`, `fullscreen_exclusive` | windowed / borderless / exclusive |
| Window size | `window_width`, `window_height` | 1280x720 ... 2560x1440 |
| Render resolution | `native_x_renderer_resolution_scale` | 0 = fit the screen, 1, 1.5, 2, 3 |
| Screen sync | `native_x_renderer_present_mode` | "" fast, "vsync", "immediate" |
| Anti-aliasing | `native_x_renderer_antialiasing` | smaa / fxaa / off |
| Texture filtering | `anisotropic_override` | -1 game default, 0 off, 2..5 = 2x..16x |
| New shaders | `native_x_renderer_scene_wait_ms` | 0, 8 (default), 33 |
| Audio buffer | `audio_maxqframes` | 8, 16, 32, 64 |
| Keyboard | `mnk_mode`, `keybind_*` | on/off, key names |
| Performance overlay | `recomp_perf_overlay` | 0 off, 1 FPS, 2 + CPU/GPU, 3 + graph |
| Log file | `recomp_log` | writes `logs\after_burner_climax_NNN.log` |

Useful on the command line: `--recomp_log=true`, `--native_x_renderer_verify=true` (F10 or
`--native_x_renderer_verify_at=N` checks a frame's draws against the GPU, see native-x-renderer.md),
`--native_x_renderer_trace=true` (F9 dumps a frame).

## Files next to the exe at run time
| File | Made by |
|---|---|
| `assets\` | The first start, from the package |
| `after_burner_climax.toml` | The first start and the menu |
| `shaders.cache` | NativeX (first start, then grows while playing) |
| `logs\after_burner_climax_NNN.log` | With Log file on |
| `crash.txt` | The crash handler, after a crash |

## Build (CI)
`.github/workflows/build-windows.yml` on Windows: LLVM + Ninja; the SDK checked out at `REXGLUE_SDK_REF` (v0.10.1),
`git am` of `patches/rexglue-sdk`, `git apply` of `patches/ffmpeg`; `git am` of `patches/native_x_renderer` in the
submodule; CMake (`ABC_SKIP_CODEGEN=ON`: the committed `generated/` is compiled, no game file is needed) with sccache;
the exe, the SDK's DLLs, `packaging/README.txt` and the licenses are uploaded as `after_burner_climax-win-amd64`.

Regenerating `generated/` (only when the overrides change) needs `assets\default.xex` and the SDK's `rexglue codegen`;
on Linux, clang 18 needs `-D__cpp_concepts=202002L -Wno-builtin-macro-redefined` (see the development log).

## Tools
| Tool | Use |
|---|---|
| `tools/stfs_extract.py <package> <dir>` | Extract a package (what `package_setup.cpp` does at the first start) |
| `tools/scan_pointer_funcs.py`, `tools/scan_code_addr_funcs.py` | Find functions the codegen missed (reached only through pointers or addresses built in code); their results are in the overrides |
| `thirdparty/native_x_renderer/tools/ppc_index.py` | Index `generated/` by guest address and search it (`build`, `dis`, `callers`, `grep`) |

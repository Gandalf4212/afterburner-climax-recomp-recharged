# Development log (read this first in a new session)

Working notes for After Burner Climax Recharged: what was done, why, and what is next.
Newest entries at the bottom. Keep it factual: addresses, versions, file names, results.

## Goals (from the project owner)
1. Move from Xenia's emulated GPU (the SDK's Xenos plugin) to **NativeXRenderer** (Vulkan, draws the
   game's Direct3D calls on the host), as done in [mad-riders-recomp](https://github.com/Gandalf4212/mad-riders-recomp)
   (reference only, never modified from here). Both renderers stay while NativeX is brought up; **Xenia is the
   default** until NativeX is fully working.
2. Port Mad Riders' options menu (F1) with a **Renderer** row (Xenia / NativeX). ASCII header in the config toml.
3. Fix: process stays alive after quitting.
4. After NativeX works: port Mad Riders' **shader cache** (`shaders.cache`, see Mad Riders `docs/native-x-renderer.md`).
5. Rewrite README and all docs for this build (credit darkuni's original work).
6. Later (roadmap): a launcher that imports the package, extracts assets, recompiles and builds `default.xex`,
   so the repo holds no code derived from the game.
7. Check ReXGlue / NativeX for updates before major features.

Constraints: tight usage budget, no subagents, tell the owner before big builds/feature work. Builds run on
GitHub Actions (branch `nativex`); the owner tests on his PC (game files in `F:\claude2\abc`).

## Starting point (2026-11-08)
- Fork of darkuni/afterburner-climax-recomp at `9eeeb46`: ReXGlue **v0.9.0** + darkuni's prebuilt SDK
  package `v0.9.0-p23` (XMA stream start/end skip patch), Xenos GPU plugin.
- Two exes: Original release (1.0.0.0, `generated/`) and World release (0.0.1.2, `world/generated/`),
  handing off to each other by default.xex fingerprint.
- Owner's package: `58410A06\000D0000\C0B8A17E2A438BBE7D63573A04652DA71EE5DE5558` (518,696,960 bytes) =
  **World release**, default.xex 2,293,760 bytes, SHA-1 `a066b2d60eefb2d5982c07e862d5a9ea8b626eca`.
- default.xex imports only xam.xex and xboxkrnl.exe; D3D is linked in. **XDK 2.0.9328** (library version in the
  import table). NativeXRenderer has mapped 2099, 2417, 5426, 7978 and (Mad Riders) 21076, so 9328 is new:
  start from 7978's device layout.

## Versions checked (2026-11-08)
| Component | Was | Latest | Chosen |
|---|---|---|---|
| ReXGlue SDK | v0.9.0-p23 (darkuni prebuilt) | v0.10.1 (bd833a2, 2026-10-01) | **v0.10.1**, built from source |
| NativeXRenderer | (none) | main bf965e9 (2026-11-07, D126) | **upstream main** (Mad Riders pins 5aa5a05, D114) |
| darkuni/rexglue-sdk-builds | p23 | p23 (v0.9.0 only) | XMA patch ported to v0.10.1 |

Upstream NativeX after Mad Riders' pin adds FSR 1 / NIS scaling (D115), FSR 3.1 and DLSS (D116-D122), frame
generation (D123-D124), minimized-window fix (D125), D126. Note: upstream's D115 is the scaler; Mad Riders'
local patch 0002 also called its shader cache "D115". Mad Riders' NativeX patches (0001 scene size/AA,
0002-0004 shader cache) must be **rebased** onto upstream main when we need them.

## Decisions (owner, 2026-11-08)
- SDK v0.10.1, port the XMA skip patch. -> done, see below.
- **Drop the Original 1.0.0.0 build**: no xex to re-codegen it, and NativeX would need its D3D mapped twice.
  One exe, recompiled from the World release.
- NativeX from upstream main.
- Builds on GitHub Actions.

## Step 1: SDK v0.10.1 and re-codegen (2026-11-08)
- `rexglue` codegen tool built on Linux from v0.10.1 (clang 18 needs `-D__cpp_concepts=202002L` for
  `std::expected` in libstdc++; CI on Windows uses clang 20+ and needs nothing).
- Codegen of the World xex with the existing World overrides (now `after_burner_climax_overrides.toml` at the
  root): 0 errors, 135 files, 6 s. It logs 31 "Unresolved" branches (4 targets: `0x82285728`, `0x82285798`,
  `0x82285930`, `0x824E7374`): the v0.9.0 code had exactly the same 24 `REX_FATAL("Unresolved branch ...")`
  sites and the game ran, so nothing changed there. If one ever fires, the target needs an override entry.
- `generated/rexglue.cmake` from v0.10.1 named things `afterburnerclimax_*` (SDK `parse_app_name` drops `_`)
  while the files are `after_burner_climax_*`: fixed in the SDK (patch 0011). The glue now also always re-runs
  codegen during the build; `CMakeLists.txt` strips that step when `assets/default.xex` is absent
  (`ABC_SKIP_CODEGEN`, same trick as Mad Riders).
- `patches/rexglue-sdk/` (applied with `git am` on v0.10.1):
  - 0001-0009: NativeXRenderer's SDK changes, as carried by Mad Riders (apply cleanly to v0.10.1). They are
    inert until a renderer uses them (draw observer, sync-only switch, ...), so the Xenos path is unchanged.
  - 0010: XMA stream start/end skip (see below).
  - 0011: glue name fix.
- `patches/ffmpeg/0001`: darkuni's wmaprodec change exporting `xma_start_skip`/`xma_end_skip` as frame metadata
  (unchanged, applies to v0.10.1's FFmpeg).

### XMA skip port (patch 0010)
darkuni's v0.9.0 patch (952 lines) rewrote the decoder output to be sample-exact, handled FFmpeg's 64-sample
delay, added declick and tracing. v0.10.1 already realigns the output by the decoder delay
(`kDecoderStartPadding`, carry frame) but still ignores the frame-header skips. The port adds only that: the
decoded frame's `xma_start_skip` / `xma_end_skip` become a start skip / output limit **in whole 128-sample
subframes (nearest)**, fed through v0.10.1's existing `pending_start_skip_` / `pending_output_limit_` (applied to
the realigned block on the next decode). cvar `xma_honor_stream_skip` (default true; false = v0.10.1 behaviour).
A skip of 512 (the usual encoder delay: one whole frame) is exact. If seams still click on looping sounds,
the next step is darkuni's sample-exact packing.

### Exit hang
SDK quit path (`ReXApp::OnClosing`, identical in v0.9.0 and v0.10.1): `KernelState::TerminateTitle()` then
`std::_Exit(0)`. TerminateTitle waits 200 ms for guest threads, leaves stragglers running, then re-acquires the
kernel's global critical region; a straggler holding it (or a lock needed later) blocks forever, so `_Exit` is
never reached. `_Exit` can also stall in DLL detach on Windows. `src/exit_watchdog.h`: armed by
`OnWindowCloseRequested` (window X, Alt+F4) and the menu's Quit; after 3 s it calls `TerminateProcess`.
To confirm which stage blocks: the log has "Exit watchdog armed", then either "Title terminated; hard-exiting
process." (SDK got to `_Exit`) or not, then "Exit watchdog: shutdown did not finish".

### Other changes in this step
- Removed the Original exe, `world/`, and the variant hand-off (`xex_variant_check`, relaunch); the app checks
  for the World xex with `xex_version_check`.
- CI (`.github/workflows/build-windows.yml`): SDK v0.10.1 from source + patches, game with sccache. Artifact
  `after_burner_climax-win-amd64`. Triggers: push to `main` / `nativex`, `v*` tags, manual.
- `src/` compiles against the patched v0.10.1 headers (checked on Linux).

## Next
- [x] First CI build on `nativex`: success (run 42487839031, 9.5 min).
- [x] NativeX built in (submodule at bf965e9), F1 > Renderer switches it; hooks + profile in src/native_x_renderer_abc.cpp. Mad Riders NativeX patches tested against bf965e9: none apply as-is; resolvable overlaps (0001 meets upstream D126; 0002 shader cache only 1 code hunk). Merge them in the shader-cache phase.
- [x] Options menu port (committed 1940c8f, not pushed yet: waits for the first CI result).
- [~] NativeX mapping: see docs/native-x-renderer.md (device layout = XDK 7978 up to the shadow, current objects 7978 + 168).
- [x] Tests on the owner's PC (build 9f958c5): Xenia boots, audio good, quit clean ("Title terminated" 0.2 s
      after Quit). F10 and `--native_x_renderer_verify_at` both work (frame end hook 82252730 fires, ~59 fps).
- [~] Verify report: layout right; 8.1 % of draws fully match. Missing: game-written constants (8224A530) and
      RunCommandBuffer (822496E0). Details in docs/native-x-renderer.md. Both need new NativeX calls.
- [x] NativeX patches 0001-0006 (`patches/native_x_renderer`, `git am` in CI on bf965e9). Rebase of Mad Riders'
      four: conflicts in native_x_renderer.h (both profile fields kept), renderer.cpp (scene size counts as the
      output's for D126's steady passes), draw.h/draw.cpp (D126 steady passes skip, never wait from the D128
      budget; both counters kept), device.cpp (D129's thread count replaces the fixed 4), decisions.md.
      Decisions renumbered D127-D129. Checked with clang -fsyntax-only against v0.10.1 + patches.
- [x] D130 (patch 0006): `BeginShaderConstantF4` call, flag `kFilledAfter`, `StateTracker::TakeFilledPackets`
      before the next device call; `RunCommandBuffer` tracked. Hooks `8224A530`, `822496E0`.
- [x] Shader cache: `src/shader_scan.*` reads FArC (`FArC`, table size, alignment; entries name/offset/
      compressed/size; gzip each, inflated with stb_image's zlib decoder). World: 101 entries -> 96 unique
      (41 VS, 55 PS), tested on Linux against the owner's package. Fingerprint: the .farc names/sizes, version 1.
- [x] F1 menu redesign (owner's request): blue theme (gradient panel, glowing border, accent line, pill values),
      five categories DISPLAY / GRAPHICS / AUDIO / CONTROLS / SYSTEM switched with Left/Right, LB/RB or the D-pad
      left/right; Up/Down or D-pad pick a row; A / Enter / Space next value, B / Backspace previous (B on an action
      row closes). Controller 1 read with `InputSystem::GetStateForUI` (host `pad_buttons`). Display mode now
      Windowed / Borderless fullscreen / Fullscreen (SDK cvar `fullscreen_exclusive`; live, RefreshFullscreen when
      only the kind changes). Renderer row stays until NativeX is the only renderer (owner: remove it then).
- [x] Second verify run (log 004, build 78968c0): 124 of 126 draws fully match (98.4 %, was 8.1 %). Left: one
      draw's VS c36-c47 read before the game filled them (it calls setters while filling): D130 now reads the filled
      packets before the next draw / clear / resolve / frame end / BeginShaderConstantF4 only.
- [x] Crash found with crash.txt (build 56d3649): StateTracker::Gather -> ApplyRecord memcpy, from
      TakeFilledPackets in the BeginShaderConstantF4 hook; only with NativeX drawing AND verify on (log 005 had
      "verify on"). Upstream latent bug: a value joined the already-applied kValues run, so the next ApplyRecord
      began inside it. Fix D131 (patch 0007): reset `values_at_` after applying locally.
- [x] (was) NativeX renderer crash (log 005): shader cache fine (96 shaders, 206 translations, 0.1 s, 5 MB), then an
      access violation in VCRUNTIME140+0x1c17b (memcpy/memmove) as the first tiled 1280x256 4x MSAA targets appear;
      no log line. Added `src/crash_handler.h` (unhandled-exception filter: code, address, stack unwound from the
      context, symbols from the shipped after_burner_climax.pdb; written to the log and crash.txt) and debug info
      (CodeView) for src/ and NativeX only.
- [x] Owner test of eb733a0: NativeX runs without crashing; title screen drawn only in its top 256 of 720 lines
      (one 1280x256 4x MSAA tile). Hooked BeginTiling 8224DC78 (count -> +12924, rects x1,y1,x2,y2 at r6) and the
      tiling end 82248DB0 (clears +12924 at 82249018; its r4 is a pointer, so not the XDK EndTiling argument
      layout: NativeX only uses it to end the pass). Menu got no controller input: the app's input active-callback
      returned false while the menu was open, and the SDL driver then reports an untouched pad to GetStateForUI
      too. Now the menu holds `InputSystem::AddUIInputBlocker` (game reads a neutral pad) and the callback stays
      true. Controls per the owner: Left/Right (D-pad) change the value, A/Enter change, LB/RB (Q/E, Tab,
      PageUp/Down) category, B/Backspace close.
- [ ] Open: SET_CONSTANT packets with bin predication (BeginShaderConstantF4 ORs in +12892) go to
      `predicated_writes`, which Gather does not apply; check if the game writes constants inside a tiled pass.
- [ ] NativeX: submodule at upstream main, map XDK 9328 D3D entry points (see NativeX
      `docs/integrating-a-game.md`, `docs/xdk-d3d-reference.md`; Mad Riders' table in its
      `docs/native-x-renderer.md`), trace/verify runs on the owner's PC.
- [ ] Shader cache port, docs rewrite, launcher (roadmap).

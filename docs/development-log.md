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
- [x] Performance overlay (from mad-riders-recomp, perf_overlay.*): F1 > System, Off / FPS / FPS CPU GPU /
      + frame-time graph (cvar recomp_perf_overlay 0-3), blue style; frames counted in the Swap hook 82252730
      (written out by hand). No frame-rate setting: owner's test with --video_mode_refresh_rate=120 (overlay
      118 FPS) doubled the gameplay speed, so the game steps its logic once per frame. The guest refresh is forced
      to 60 Hz (frame_rate.cpp) and the timer resolution raised to 1 ms. Above 60 would need render interpolation
      of game states (large) or frame generation (needs DLSS/FSR view + motion vectors mapped): later, if wanted.
- [x] Session 2 wrap-up (owner: "it's all buttoned up"): NativeX is the only renderer (Renderer row, the
      `recomp_renderer` cvar, Xenia-only rows and the ABC_NATIVE_X_RENDERER switch removed; the Xenos plugin stays as
      NativeX's command processor). Aspect row removed (present_letterbox is Xenia's; NativeX keeps 16:9 with bars).
      Debug info / pdb removed from the build (the crash handler still reports module+offset). Removed one-off tools
      (check_labels, dis.sh, lzxtool, xex_lzx, xex_unpack) and docs/reference (ported). Docs rewritten: README,
      architecture.md (new), native-x-renderer.md, HANDOFF.md (launcher plan), packaging README.
- [x] Pause while the F1 menu is open (owner: the game kept running). Cooperative: the main thread waits at its next
      frame start in the Swap hook (`game_pause.h`); suspending guest threads from outside was rejected (a thread
      stopped in a host lock such as the heap's would hang the menu). NativeX D132 (patch 0008): `SetPaused`, the
      queue's poller re-presents the last frame with the dialogs at up to 60 FPS.
- [ ] Jets invisible under NativeX (found when NativeX became the only renderer; earlier "working" builds ran Xenia
      by default). Menu widened (em*64, value column 56 %): long values overran it. See HANDOFF for the plan.
- [ ] Jets fix, NativeX D133 (patch 0009), to test: the tracker read `RunCommandBuffer` (822496E0) as one non-draw
      call, so the DRAW_INDX packets of the game's recorded command buffers (~570 a frame) were counted and never
      drawn; log 007 showed only direct draws (241-273 per frame, all drawn). Now each draw packet in the scan is a
      split point and the call is handed on draw by draw, each with the packet state before it; INDIRECT_BUFFERs
      followed two levels. Cvars: `native_x_renderer_recorded_draws` (true), `native_x_renderer_predicated_constants`
      (false; HANDOFF step 3, applies bin-predicated float SET_CONSTANTs). Checked with a syntax-only compile of
      state.cpp against stub headers. Build f8da8dc, log 008: no change; predicated_constants=true made the nozzles
      worse (stays off). F10 verify (frame 1305): 0 indirect buffers read in 2870 calls, 228 draw packets from
      buffers outside the ring.
- [ ] D134 (patch 0010): RunCommandBuffer writes its INDIRECT_BUFFERs bin-predicated (r27 = 1 at 822499C0, header
      0xC0013F01 at 82249B78); ScanPackets ignored predicated non-draw packets, so D133 never saw the recorded
      buffers. Predicated INDIRECT_BUFFERs are now followed. To test.
- [ ] Build 77e6b68 + toml, log 009: jets still invisible, other draws broken, then a crash: a vertex shader read from a
      followed buffer was not microcode ("Unknown ALU vector operation", 7D92E03DB516BD8F), then nvoglv64.dll access
      violation; shaders.cache kept it and later launches crashed. D135 (patch 0011): D134 behind
      `native_x_renderer_predicated_indirect` (default off), with logging of the followed buffers and their shader
      loads. A crash now leaves `shaders.cache.bad`; the next launch deletes the cache and builds it again.
      Correction: the verifier's 228 "outside the ring" packets are not matched to any call (observer.cpp
      OnGpuDraw); the jets come only from RunCommandBuffer replays.
- [ ] Log 010 (predicated_indirect on): more garbage, "Unknown ALU scalar operation" (vertex C102E4A6B7EFB781),
      "Unsupported primitive type 0", driver crash after ~30 s in game. RunCommandBuffer replays seen: predicated
      INDIRECT_BUFFERs to BA31B9E0.. (871-9180 dwords). D135's load log was used up by one prebuilt buffer. D136
      (patch 0012): new programs from indirect buffers and per-buffer summaries logged instead.
- [ ] Log 011 (predicated_indirect on, D136): the recorded buffers are real (C0006100 00000000 C0006000 00000003
      C0032D01..., read to their ends, 1-15 indexed tri-strip draws each, no bad shader); draws per frame up
      (1800: 335 -> 417, 2400: 167 -> 369), all drawn; no jets. Log 012 (+ predicated_constants on): grey thrust,
      intro effects wrong. Log 008's verifier: the same recorded packets run up to 12x a frame with different VS
      constants (4, 25, 26, 99, 108). D137 (patch 0013): the constants the game fills after BeginShaderConstantF4
      (each jet's matrices) are read before RunCommandBuffer, not after its draws.
- [ ] Log 013 + Snap1/Snap2 (D137, both switches on): the player jet draws as a huge grey shape over the lower
      screen with black nozzle discs (wrong transform, no texture); "Unknown ALU scalar operation" (vertex
      DCAD7A87DA7CAC0C, not from an indirect buffer) and "primitive type 0" again. D138 (patch 0014): the verifier
      matches and compares the replayed draws, to find which state is wrong.
- [ ] Log 014 + snap3 (D138 verify): 228 GPU executions of 76 replayed draws matched, 19 fully clean, the rest
      RB_COLOR_MASK cpu 0x7 / gpu 0xF; the jet now shows as smoke trails and black nozzles. D139 (patch 0015): a
      recorded buffer's bin-predicated SET_CONSTANTs are applied (not the window offset/scissor).
- [ ] Log 015 (D139): no jet; 46 of 76 replays clean, 30 with RB_COLOR_MASK 0xF vs GPU 0x7: predicated writes are
      conditional. RunCommandBuffer writes SET_BIN_SELECT_HI = predicationSelect (8224997C); 8223EFA0 sets mask
      and SELECT_LO = -1. D140 (patch 0016): predication evaluated inside recorded buffers.
- [ ] Log 016 (D140): 76/76 replayed draws and 236/236 calls fully match; no jet. Float constants were never
      compared (no shader analysis without Xenia: "0 compared", 598 unanalyzed). D141 (patch 0017): replays'
      constants compared over the whole windows.
- [ ] Log 017 (D141): constants compared: 0 of 178 replays clean; the tracker has command-stream dwords
      (C0012700, C0006000, ...) in VS c20-c41 where the GPU has 0. BeginShaderConstantF4 (8224A530) builds a correct
      SET_CONSTANT (count*4+5 dwords reserved, NOP padding to 16 bytes). Patch 0018 is DEBUG ONLY (marked
      DEBUG(ABC)): it logs where command-like data enters the constants and where command-like programs come from.
- [ ] Log 018 (DEBUG(ABC) build, crashed): the command data in the constants came mostly from Present's packets
      (type-0 "writes" of 4,100-16,308 registers: 1003C200, 1ACC4014, 3FB33333 = 1.4f). NewSegment (822538F8:
      called by the kick path 82254330, the reservation 822546E8 and Present 82252CFC; writes +48) was not hooked, so
      Present was read across its segment change through stale ring data. Now hooked; NativeX D142 (patch 0019): a
      type-0 write past the register file ends the scan.
- [ ] Log 019 (NewSegment hooked): VS constants of every replay match; only PS c40-c43 (per-tile offsets) differ.
      Log 020 (+ predicated_constants): those take the last tile's values (c40.y 480), title screen wrong: leave off.
      State right, nothing drawn: the renderer's OnEnter did not record RunCommandBuffer, so OnLeave dropped every
      replayed draw (g_call mismatch). D143 (patch 0020) fixes it.
- [x] **Jets fixed.** Log 022 (build 8f04d27, D143): jets on the selection screen, in stages and on the ranking
      screen, the water, enemies and islands draw; 60 FPS, 0 failed draws, no bad program. Final build: debug patch
      (DEBUG(ABC), log 018) removed; NativeX D144 (patch 0020): `native_x_renderer_predicated_indirect` defaults to
      true; `native_x_renderer_predicated_constants` stays false (per-tile PS c40-c43, log 020).
- [ ] Build after 2e3cec8 (to test): NativeX D145 (patch 0021): a shader's microcode is stored only after a
      translation succeeds (no more bad shaders kept in shaders.cache); D146 (patch 0022) + app: the shader cache
      screen uses the F1 menu's blue palette; DrawIndexedVertices (82241320) hand-written with Mad Riders' split of
      list draws over 65,532 indices; the F1 menu takes the left stick's up/down as the D-pad's (hysteresis).
- [ ] The gamepad's Select (Back) opens and closes the settings menu (PadWatcher in after_burner_climax_app.h: an
      always-drawn dialog that reads controller 1 for the UI each frame; the toggle is deferred to the UI thread).
      Owner: Select kept reopening instead of closing. Now the watcher only opens the menu; the open menu closes
      itself on Select like B (game_menu.cpp kPadBack). The left stick's left/right also act as the D-pad's.
- [ ] Launcher step 1 (llvm-mingw): SDK + NativeX + DiligentCore + FFmpeg compile for x86_64-w64-mingw32
      (local cross-compile). patches/rexglue-sdk/0012, patches/ffmpeg/0002, tools/mingw-cross-toolchain.cmake.
      Open bugs and next steps: HANDOFF "Session 3 end".
- [ ] **Session 4: EndTiling hook moved 82248DB0 -> 8224DFE8** (src/native_x_renderer_abc.cpp; to test). 82248DB0
      is not EndTiling: it copies tile rects into +12928.. and stores r9 to +12924 like BeginTiling's tail, takes a
      command buffer in r4 (clears +116/+120, stores it to +13724), and its one site (82167680, sub_821673E8) runs
      inside the tiled scene; r6 is a stack rect pointer, hence the "EndTiling with a resolve destination" warning
      on every run. The XDK EndTiling is 8224DFE8 (resolves each tile via 8224EC70 when rects or dest are given,
      resets predication +12884..+12900), called once at 821683C4 by the engine's pass end 821682F0 with all-zero
      arguments. Evidence, log 022: the first fake EndTiling (line 240) is followed at once by 1280x256 4x MSAA
      color/depth targets at base 0/1024 (241-242), i.e. the scene went on into tile-sized hosts; later 1x targets
      of pitch 80/320/640 got 1280x720 hosts (383-417) because tiling was never ended after the scene. Expected:
      stage 3 ground textured (it showed only the lower tiles' clear color), bloom/luminance chain at its real
      sizes, and the warning gone.
- [ ] Session 4, owner test of the EndTiling fix (log 023, Snap13): the warning is gone and the post-scene targets
      have their real sizes (32x32, 320x180, 640x360, 256x256) but stage 3's ground is still flat beige and
      1280x256 4x color/depth targets still appear: something else draws outside the full-size pass. Next: an F9
      trace (`--native_x_renderer_trace=true`) of a stage 3 frame.
- [ ] Session 4: CI checkout failed since 866d4e5 ("not our ref c5246649"): the submodule was committed at a local
      patched commit. Reset to upstream bf965e9 (CI applies the patches).
- [ ] Session 4, menu: Select reopened the menu it had just closed (the menu closed on Select, the watcher saw the
      same press in the same frame and reopened it). Now only the watcher handles Select, as a toggle, 250 ms
      guard; the menu no longer reads Select. Display mode -> borderless moved the selection to "Render
      resolution": the resize moved the cursor relative to the window, read as a mouse move (hover selects).
      Hover is now ignored for 30 frames after the display size changes.
- [x] The toml lists `native_x_renderer_recorded_draws` and `native_x_renderer_predicated_constants` under
      "Renderer (testing)" with comments, even at their defaults (`src/config_file.h`; owner's request).
- [ ] NativeX: submodule at upstream main, map XDK 9328 D3D entry points (see NativeX
      `docs/integrating-a-game.md`, `docs/xdk-d3d-reference.md`; Mad Riders' table in its
      `docs/native-x-renderer.md`), trace/verify runs on the owner's PC.
- [ ] Shader cache port, docs rewrite, launcher (roadmap).

# Handoff (2026-11-08, end of session 2)

## State
Branch `nativex`. The game runs natively with **NativeXRenderer as its only renderer**: boot to gameplay, full screen
with the HUD, 60 FPS, no crash in the owner's tests. Shader cache, F1 menu (blue theme, five categories, controller),
pause while the menu is open, performance overlay, exit watchdog and crash report are in. Details:
[architecture.md](architecture.md), [native-x-renderer.md](native-x-renderer.md), [development-log.md](development-log.md).

**Blocking bug (fix before the launcher): every jet is invisible under NativeX** (player jet on the selection screen
and in stages, enemy jets; engine exhaust, scenery and HUD draw). Not a regression: builds up to 42501639666 defaulted
to Xenia; build 42503819109 (bedf762) is the first that forces NativeX. Deleting `shaders.cache` and the toml does not
help. Log 007 (owner's F:\claude2\abc): every draw call drawn, 0 failed or skipped, so the jets' draws are either not
draw calls NativeX sees or drawn with wrong state.

## Session 3 (2026-11-09): jets fix written, to test
NativeX D133 (patch 0009): `RunCommandBuffer`'s recorded draws are handed to the renderer one by one (cvar
`native_x_renderer_recorded_draws`, default true). Option `native_x_renderer_predicated_constants` (default false)
applies bin-predicated float SET_CONSTANTs (step 3 below). Owner: test the jet selection screen and a stage; if jets are
still missing, set the predicated option to true in the toml; if anything breaks, set recorded_draws to false. Send the
log (and an F10 verify capture on the selection screen if jets are still missing).

## Original plan (jets)
1. Verify capture on the jet selection screen: `--recomp_log=true --native_x_renderer_verify=true`, F10 there
   (log 007 had no capture: F10 was not pressed). Look at "draw packets from buffers outside the command buffer
   ring" and the unmatched packets: if the jets' draws are in recorded command buffers, that confirms it.
2. Likely fix (biggest open NativeX job): draw the contents of recorded command buffers. `RunCommandBuffer`
   (`822496E0`, hooked, D130) is tracked as a non-draw call; `ScanPackets` already follows its INDIRECT_BUFFERs one
   level (state.cpp, opcode 0x3F) and counts their DRAW_INDX packets, but the renderer gets no draw for them. Needed:
   split the call's packets at each DRAW_INDX/DRAW_INDX_2, rebuild the state at each (register writes and SET_CONSTANT
   before it, the inherited dirty masks the call ANDs at its start), and hand each to the renderer as a draw
   (`renderer::CaptureDraw` path, like an inline draw's EndVertices). Index/vertex data: from the DRAW_INDX packet and
   the fetch constants in the buffer. Also check `82243950` (14 sites, 3 INDIRECT_BUFFERs from device +13992..+15028).
3. Other possibility to rule out with the capture: SET_CONSTANT with bin predication (BeginShaderConstantF4 ORs in the
   predication at +12892, set inside tiled passes) goes to `predicated_writes`, which `StateTracker::Gather` ignores:
   the jets' matrices would then be stale. If so, apply predicated float-constant writes for non-draw calls.
4. Until fixed, consider putting the Renderer choice back (NativeX default, Xenia fallback) so the game stays playable.
5. Owner to test once: the F1 pause, and the widened menu (commit after bedf762, not built yet: [skip ci]).

## Known gaps (NativeX)
- Draws inside recorded command buffers (RunCommandBuffer, ~570 draw packets a frame) are tracked, not drawn. The
  game looked complete in the owner's screenshots, so find out what they are before working on it.
- Predicated SET_CONSTANT packets are not applied by the tracker; the last verify mismatch is fixed but not re-verified.
- No DLSS / FSR 3.1 / frame generation (needs the 3D view and motion mapped). The game steps its logic once per
  frame: above 60 FPS would need render interpolation of game states.

## After the jets: the launcher (owner's plan)
Turn the repository into one that holds **no recompiled game code**: a launcher that builds the game on the user's PC
from their own package, like
[mushihimesama-futari-black-label-recomp v0.8.0](https://github.com/Gandalf4212/mushihimesama-futari-black-label-recomp/releases/tag/v0.8.0)
(its release: the game zip with a trimmed w64devkit compiler; `toolchain-sources-<ver>.zip` with that compiler's GCC,
binutils and Mingw-w64 sources under GPLv3; a launcher exe that checks the CRCs, builds once into `assets\`, then starts
the game; `assets\build.log` on failure).

1. **Launcher exe** with a selection screen in the menu's style (blue panel, controller and keyboard): if no package
   is next to it, ask for the Xbox 360 file (file dialog); verify it with **CRC32** as
   "After Burner Climax XBLA (World)" (compute and record the package's CRC32, and default.xex's: SHA-1
   `a066b2d6...`, 2,293,760 bytes); extract it (`package_setup.cpp`); show progress.
2. **Local build**: run `rexglue codegen` on the user's default.xex with the manifest and overrides, then compile and
   link the game against prebuilt SDK and NativeX libraries. Measure time, RAM and disk, and write `assets\build.log`.
3. **Minimal build environment in the package**: a compiler that can build `generated/` and link with the SDK
   (decide: the w64devkit GCC route as in Futari, or clang + lld with the MSVC-compatible runtime the SDK is built
   for; the SDK is built with clang for MSVC today, so mixing compilers needs checking first). Prebuilt SDK, NativeX
   and FFmpeg libraries and headers.
4. **Toolchain sources combined into one file** (`toolchain-sources-<ver>.zip`) attached to each release, for the
   compiler's licenses.
5. CI: build the launcher and the prebuilt libraries, package the release; remove `generated/` from the repo.
6. Then: README and docs for the launcher.

Before starting: check `rexglue/rexglue-sdk` tags and NativeXRenderer main for updates (`git ls-remote`).

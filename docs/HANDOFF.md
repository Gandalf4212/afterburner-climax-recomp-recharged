# Handoff (2026-11-10, session 4)

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

## Session 3 end (2026-11-09 23:20): open bugs, launcher status

### Open rendering bugs (track these down next)
0. **READ FIRST (session 4 end):** stage 2/3 terrain is not drawn by NativeX (the old Xenia renderer draws it). The
   development log's "Session 4 end: RenderDoc capture analysis" entry says what is proven correct and the next two
   steps (run the PS with tools/rdc/psrun.py; look for a later overwrite). Ask the owner before every build.
1. **Stage 3 desert ground: flat beige** (Snap12, log 022). **Session 4: cause found, fix committed, to test.** The
   "EndTiling" hook sat on 82248DB0, a command-buffer begin the engine calls inside the tiled scene: NativeX ended
   the tiled pass mid-scene, so the targets bound after it got 1280x256 hosts (the ground went there; the lower
   tiles kept the clear color), and the post-scene passes ran with tiling on (bloom/luminance targets enlarged to
   1280x720). The hook is now on the real EndTiling 8224DFE8 (development-log "Session 4"). The old plan (route an
   EndTiling resolve through Resolve) is void: the game's EndTiling never resolves. Owner to check: every stage
   for flat-colored or missing surfaces, glow/bloom, the log has no "EndTiling with a resolve destination" line and
   no 1280x256 targets, and the post-process targets are logged at 640x360/320x180/... on the host.
2. **PS c40-c43 per tile:** the GPU sets them per tile (screen offsets; values like 256.0, 480.0); NativeX draws
   the scene once and keeps whatever the packets left. Harmless so far; watch shaders that use screen position.
3. **"Texture fetch constant ... has invalid type"** (2-3 per run): from the SDK's texture path, never investigated.
4. Unverified: Select and the left stick in the F1 menu (build 7c2df02), the blue shader-compile screen (delete
   shaders.cache to see it), the F1 pause, the 65,532-index split (no draw that large seen yet).

### How to find such bugs
- Run with `--recomp_log=true --native_x_renderer_verify=true`; F10 verifies one frame per press, so press it on
  each screen to check. "D138: replayed draws ... N of M fully match" covers RunCommandBuffer's recorded draws; the
  mismatch lists below it say which state is wrong. All state matching but a wrong picture = a renderer gap.
- **Owner's idea: an unlimited-lives (armor) cheat** so every stage can be reached and checked. To do: find the
  armor variable (the HUD's ARMOR %), e.g. with ppc_index.py on the code that draws or decrements it, or by a memory
  diff between hits, then add a menu toggle (F1 > a Cheats tab) that holds it at 100 %.

### Launcher: status (owner chose: LLVM toolchain like Witch Time, one locally linked exe)
- Decision: ship a trimmed **llvm-mingw** (clang + lld + libc++ + mingw-w64 headers/libs; Apache-2.0 with LLVM
  exception, no GPL source bundle). CI builds the SDK, NativeX, DiligentCore and FFmpeg as static libraries with it;
  the player's PC compiles generated/ + src/ and links one exe with lld. Witch Time (github.com/Zexyen/Witch-Time,
  tools/wtbuild/README.md) does the same with LLVM's release + llvm-mingw builtins, but splits game.dll / exe; we
  do not (ReXGlue hooks override generated functions at link time).
- **Done:** the SDK, NativeX, DiligentCore and FFmpeg compile for x86_64-w64-mingw32 (cross-compiled on Linux with
  clang-18 + Ubuntu mingw-w64). Fixes: patches/rexglue-sdk/0012 (no SEH thread naming outside MSVC, FARPROC casts,
  no endian.h on Windows), patches/ffmpeg/0002 (HAVE_LIBC_MSVCRT 0 under mingw). Both keep the MSVC build unchanged.
  Configure with `-DREXGLUE_USE_D3D12=OFF` (mingw-w64 lacks DXProgrammableCapture.h; the game uses Vulkan).
- Local cross-compile recipe (2 cores, about 30 min): tools/mingw-cross-toolchain.cmake; configure as in
  build-windows.yml plus `-DCMAKE_TOOLCHAIN_FILE=... -DREXGLUE_USE_D3D12=OFF`; if googlesource is unreachable,
  `-DFETCHCONTENT_SOURCE_DIR_ABSEIL-CPP=<empty dir with empty CMakeLists.txt>` (abseil is only for Dawn/WebGPU,
  which NativeX turns off). Linux-only artefacts, not real problems: header case (Windows.h vs windows.h: add
  case-variant symlinks via -isystem), std::expected (Ubuntu libstdc++; llvm-mingw's libc++ has it),
  SetThreadDescription (old Ubuntu mingw-w64 headers).
- **Next steps, in order:**
  1. CI job on windows-latest (or Linux) with real llvm-mingw (mstorsjo/llvm-mingw ucrt release): build the libs,
     then the whole game once with it, to prove the exe links and runs (expect link issues: rexruntime is a DLL today).
  2. Package the prebuilt .a libraries + headers + trimmed llvm-mingw + rexglue.exe (codegen; can stay MSVC-built).
  3. Launcher exe: owner's spec: **blue theme, rounded corners, the settings menu's font; if the game is not found
     next to it, an Import button opens a file dialog**; controller + keyboard. Verify the package (CRC32) and
     default.xex (SHA-1 a066b2d6..., 2,293,760 bytes), extract (package_setup.cpp), run codegen, compile, link into
     assets\, write assets\build.log, show progress, then start the game.
  4. CI release: launcher + prebuilt libs + toolchain; remove generated/ from the repo; README.

## Session 3 (2026-11-09): jets fixed
The jets and the near water were drawn only by `RunCommandBuffer` (822496E0) replaying recorded command buffers.
NativeX D133-D144 (patches 0009-0020): the replays' draws are split and handed on (D133); their predicated
INDIRECT_BUFFERs followed (D134, default on since D144); BeginShaderConstantF4's filled constants read before them
(D137); predicated SET_CONSTANTs inside them applied when the bin predicate passes (D139, D140); the game's NewSegment
(822538F8) hooked so Present is no longer read through stale ring data (D142); the renderer captures the replays
(D143). The verifier checks replays (D138, D141). Log 022: everything draws, 60 FPS.
- Toml "Renderer (testing)": `native_x_renderer_recorded_draws` (true), `native_x_renderer_predicated_indirect` (true),
  `native_x_renderer_predicated_constants` (false: per-tile PS constants).
- Open, minor: "EndTiling with a resolve destination: not supported yet" (once per run); D135/D136 info lines
  (bounded, about 50 per run) could be removed; the Mad Riders 65,532-index split hook is ported (DrawIndexedVertices).
- Owner to test once: the F1 pause, the widened menu, the left stick in the menu, the blue shader screen
  (delete shaders.cache to see it).
- Next: the launcher (below).

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

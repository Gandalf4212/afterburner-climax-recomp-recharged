# Handoff (2026-11-08, end of session 2)

## State
Branch `nativex`. The game runs natively with **NativeXRenderer as its only renderer**: boot to gameplay, full screen
with the HUD, 60 FPS, no crash in the owner's tests. Shader cache, F1 menu (blue theme, five categories, controller),
pause while the menu is open, performance overlay, exit watchdog and crash report are in. Details:
[architecture.md](architecture.md), [native-x-renderer.md](native-x-renderer.md), [development-log.md](development-log.md).

Waiting on the owner: a test of the newest build (menu pause, NativeX after the Xenia selector was removed).

## Known gaps (NativeX)
- Draws inside recorded command buffers (RunCommandBuffer, ~570 draw packets a frame) are tracked, not drawn. The
  game looked complete in the owner's screenshots, so find out what they are before working on it.
- Predicated SET_CONSTANT packets are not applied by the tracker; the last verify mismatch is fixed but not re-verified.
- No DLSS / FSR 3.1 / frame generation (needs the 3D view and motion mapped). The game steps its logic once per
  frame: above 60 FPS would need render interpolation of game states.

## Next: the launcher (owner's plan, starting 2026-11-09)
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

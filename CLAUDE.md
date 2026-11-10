# CLAUDE.md: After Burner Climax Recomp (Recharged)

Read `docs/HANDOFF.md` first (current state, next steps), then `docs/architecture.md` (files and functions),
`docs/development-log.md` (history, decisions) and `docs/native-x-renderer.md` (the D3D map) as needed.

## Working rules (from the owner)
- Tight token budget: no subagents; read only the files/lines a step needs. Never dump `generated/` (66 MB of
  machine-translated C++); query it with NativeX's `tools/ppc_index.py` (index in `out/`, rebuild with
  `python3 thirdparty/native_x_renderer/tools/ppc_index.py build`).
- Tell the owner before big builds (every push to `main`/`nativex` builds on GitHub Actions) and before big features.
- Docs-only commits: put `[skip ci]` in the message.
- Work on branch `nativex`. Commit as Gandalf4212 <gandalf4212@gmail.com>.
- Mad Riders' shader cache, menu and overlay are ported (`patches/native_x_renderer/`, `src/`); its repo
  (github.com/Gandalf4212/mad-riders-recomp, public) may be read, never changed.
- Keep `docs/development-log.md` updated: what, why, evidence (addresses, versions, results).
- The owner tests on Windows; game files in `F:\claude2\abc` (World package `58410A06\000D0000\C0B8A17E2A438BBE7D63573A04652DA71EE5DE5558`).
  default.xex is not in the repo; extract with `tools/stfs_extract.py <package> assets`.

## Facts
- Game: After Burner Climax, World release, default.xex 0.0.1.2, SHA-1 a066b2d6..., XDK 2.0.9328.
- ReXGlue SDK v0.10.1 built from source + `patches/rexglue-sdk` (git am) + `patches/ffmpeg` (in thirdparty/FFmpeg).
- NativeXRenderer (the only renderer) submodule `thirdparty/native_x_renderer` at upstream bf965e9 +
  `patches/native_x_renderer` 0001-0022 (git am; shader cache D127-D129, D130-D146). Editing them: see the end of
  `docs/native-x-renderer.md`.
- Codegen (Linux): clang 18 needs `-D__cpp_concepts=202002L -Wno-builtin-macro-redefined`; see the log.

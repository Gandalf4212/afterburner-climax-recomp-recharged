# Missing terrain on stages 2 and 3 (NativeX): what was tried and ruled out

Read this before working on the missing terrain. Session 4 (2026-11-10) spent its whole budget on it; everything
below is settled unless a new fact contradicts it. Details and evidence: `development-log.md`, "Session 4" entries.

## The symptom
- Stage 1 terrain (low, near) draws. Stage 2 (mountains, valley, town) and stage 3 (canyon) terrain does not: the sky
  and fog colour show where it should be (Snap13 vs the owner's old-Xenia screenshots). The old Xenia renderer
  build (same SDK) draws it.
- A jet that is hit disappears while its fire stays: probably the same bug.
- Stage 1 terrain = VS `D9036E826C687B36`; stage 2/3 terrain = VS `4CB48051EDE053AA`. Same 192-dword program; only
  the vertex layout differs (4CB480: stride 8, UVs by vfetch_mini at offset 6; D9036E: stride 12). Both use PS
  `92AC7F14F7C95B62` (1 texture, has a kill). Buildings (VS `FC2946D666B65B20`) draw fine in the same pass.

## Ruled out (with the evidence)
| Idea | How it was tested | Result |
|---|---|---|
| EndTiling hook at the wrong place | Found 82248DB0 was a command-buffer begin; moved to 8224DFE8 (real fix, kept) | Post-process sizes fixed; terrain still missing |
| Bin predication drops draws | D148 counter (log 027) | 0 dropped. `native_x_renderer_all_bins` breaks the jet: keep off |
| Per-tile PS constants c40-c43 | F10 (log 029); engine loop 82168994 writes c40 = tile offset | c40 right for full-screen draws; c41-c43 stale on the GPU too |
| Game state wrong | F10 verify on stages 2/3 | 100 % of draws match the GPU (state, constants, fetches, programs) |
| "Invalid" texture fetch constants | `gpu_allow_invalid_fetch_constants = true` (log 031) | No change |
| Guest memory shortcuts (D97/D98/D110), object motion | All off (toml) | No change |
| Shader cache file | `pipeline_storage = false`, cache renamed | No change |
| Vertex fetch type field | D151 (patch 0027) | Types were kVertex; never triggered |
| Dynamic constant addressing c[24+a0] | SDK translator.cpp:188 sets all 256 bits | Uploaded correctly |
| D130/D137 filled constants | Code read | Reach the renderer through the tracker's record |
| Upstream NativeX fixes | bf965e9..b63f67f (20 commits) | Nothing about geometry |
| Depth test / cull / blend / alpha test | D153 force-visible (log 037) | Still no pixels |
| Stale GPU copy of guest memory | RenderDoc capture: mirror buffer 601 at the terrain addresses | Byte-for-byte correct |
| Wrong constants / fetch / system constants | Read from the capture's upload heap | All correct, same as buildings |
| Translated vertex shader wrong | `tools/rdc/spvrun.py` ran SPIR-V module 1619 on the capture's data | Matches the guest's math to 0.1 % |
| Viewport / scissor / render target | Capture | Same as the buildings; forced draws render into scene image 2501 |
| Terrain texture | Capture: image 19866, BC3 2048x2048 | Alpha 255 in 99 % of blocks; swizzle identity (0x688), unsigned |
| Fog hiding the terrain | Snap13 | Sky shows through where the canyon walls are: no pixels at all |

## Still open (do these next, in order)
1. Run the terrain PS (module 1621) on the real inputs: `tools/rdc/psrun.py` (BC3 decode, extra ops) with the VS
   outputs `spvrun.py` prints. Its kill: drop if `packed c8.w (0) > tex.a * interp2.w (1.0) - packed c1.x (0.5)`.
   Does it kill, or what colour does it write?
2. If it writes a colour: look for a later draw that overwrites the lower half of image 2501.
3. Take a new capture WITHOUT `native_x_renderer_force_visible_vs` in the toml (Stage2-NativeX.rdc was taken with it).
   Stage2-NativeX-new.rdc still shows the forced state on the terrain pipeline (1303: no depth format, depth off,
   cull none, rendered without a depth attachment, exactly as D153 forces it), so the setting was probably still
   active: confirm the log has no "D153:" line before trusting a capture as the normal case.

## Tools
- F8 draw dump (D149), `native_x_renderer_log_shaders` (D150), `native_x_renderer_inspect_vs` (D152),
  `native_x_renderer_force_visible_vs` (D153), F9 trace, F10 verify.
- RenderDoc without a GPU: clone github.com/baldurk/renderdoc; `cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DENABLE_QRENDERDOC=OFF -DENABLE_PYRENDERDOC=OFF -DENABLE_GL=OFF -DENABLE_GLES=OFF -DENABLE_XLIB=OFF -DENABLE_XCB=OFF`;
  `ninja renderdoccmd` (~10 min); `renderdoccmd convert -f X.rdc -o out.zip.xml -c zip.xml`. In the zip: the
  mirror's initial contents and the upload heap's mapped writes are the large files; `spirv-dis` (apt
  spirv-tools) for shader modules.

## Rules from the owner
- Ask before every build (every push to `nativex` builds). Prefer reasoning from code and captures over new
  logging builds.

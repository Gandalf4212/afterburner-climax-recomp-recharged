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
   **Stage2-NativeX-new2.rdc is the clean normal capture** (force_visible off): the terrain pipelines (e.g. 12517,
   19319; index counts 1322, 1303) have depth format D24_UNORM_S8 (129), back-face culling, depth test and write on,
   compare LESS, and render with the scene's depth attachment (view 2483) like the buildings. Use it for steps 1-2.

## Best guesses for the next session (ranked) and a one-session plan
The owner will scrap the project if this is not found in the next session. No new logging builds: work from
Stage2-NativeX-new2.rdc (clean) with renderdoccmd + tools/rdc, ask before any build.
1. **(~45 %) The terrain PS kills every pixel.** D153 turned off the fixed-function tests but not the shader's own
   kill (`kill if packed c8.w > tex.a * interp2.w - packed c1.x`). Check the PACKED mapping: the PS float buffer is
   144 bytes = 9 constants packed by the shader's bitmap; packed [8] read (0.114, 0.299, 0.587, 0) looks like
   luminance weights, not a kill threshold, so NativeX may upload the constants in a different order or set than
   the translation expects (D93's sized-constant substitution, or the PS constants written after
   BeginShaderConstantF4 PS start 0/0x1C, D130). Steps: get PS 92AC7F's register bitmap (log its disassembly with
   `native_x_renderer_log_shaders = "92AC7F14F7C95B62"`, no build needed), map packed index -> register, compare the
   capture's 144 bytes with the game's PS constants (F10 compares the register file, not the packed upload).
   Then run psrun.py on the real inputs.
2. **(~25 %) Interpolators reach the PS wrong.** NativeX passes `interpolator_mask` = VS writes & PS reads (draw.cpp
   ~line 820); a mismatch would leave interp2 (w = 1.0 in the VS) at 0 in the PS and kill everything. Compare the
   VS/PS modification values in the pipeline (the VS output locations vs the PS input locations in modules 1301/1302
   of the new2 capture).
3. **(~20 %) A later draw paints over the terrain.** In new2, list every draw after the terrain that writes scene
   colour image 2501 with a full-screen rect (prim 13) and no depth test; check the mid-scene Resolve (#1041 in
   log 026) and what samples its copy.
4. **(~10 %) Something else** (MSAA resolve of the tiled pass, fog plane depth); revisit only if 1-3 are clean.

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

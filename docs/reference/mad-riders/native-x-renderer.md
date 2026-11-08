# NativeXRenderer in Mad Riders (work in progress)

[NativeXRenderer](https://github.com/ElGuslml/NativeXRenderer) draws the game's
Direct3D calls on the host (Vulkan, DiligentCore) instead of emulating the Xenos GPU. It is the default
renderer since 2026-11-02 and the only one since 0.8.2 (the F1 > Renderer choice and the Xenos settings are gone;
`native_x_renderer` is forced on): the ReXGlue SDK stays v0.10.0 (built from source with NativeXRenderer's
changes). Its Xenos plugin still runs the command processor NativeXRenderer observes; its host rendering is off,
and draws only if NativeXRenderer cannot start (last resort, logged).

## Build
- `thirdparty/native_x_renderer`: submodule (pinned commit), with DiligentCore inside it.
- `patches/rexglue-sdk/`: NativeXRenderer's eight SDK changes (draw observer, sync-only command processor,
  queries, gamma, fractional scale, renderer thread signals, RequestRanges, swap count), taken from
  ElGuslml/rexglue-sdk's `recomp` branch (changes 26-33), plus change 19 (lazy resolves: the texture cache's
  `TryLoadTextureDataFromResolveCache`, which NativeXRenderer overrides; its D3D12 part is off by default). They
  apply cleanly to v0.10.0.
- CMake: `-DREXSDK_DIR=<patched SDK source tree>` (required).
- CI: `.github/workflows/build-windows-nxr.yml` (artifact `mad_riders-nxr-win-amd64`).

## The executable
XDK 2.0.21076 (newer than every game NativeXRenderer has mapped: 2099, 2417, 5426, 7978). The D3D library is at
`0x82E5B000`-`0x82E84000`.

| Entry point | Address | Evidence |
|---|---|---|
| DrawIndexedVertices | `82E6EBB8` | DRAW_INDX with index data; flush reads masks +0/+8/+16/+24/+32 and constants +1920/+6016, fetch +1152, bool/loop +10112 |
| DrawVertices | `82E6E7A0` | same flush, auto-index DRAW_INDX |
| BeginVertices | `82E6E2E0` | (prim, vertexCount, stride) -> pointer; packets after the write pointer, end saved at +13844 (the first trace's "DrawVerticesUP data=0x1c stride=0xa8" were this call's arguments) |
| EndVertices | `82E6E2D0` | `lwz r11,13844(r3); stw r11,48(r3); blr`; called by the engine (`sub_82724780`) |
| SetRenderTarget | `82E66AE0` | `addi r11,r4,3204` (+12816 + 4i), RB_COLOR_INFO +10372, surface info helper 82E66A50; engine alias `82E67148` |
| SetDepthStencilSurface | `82E66E70` | +12832, RB_DEPTH_INFO shadow |
| Clear / ClearF | `82E6E210` / `82E6E0D8` | Clear unpacks the color (`vupkd3d128`) and calls ClearF per rect; ClearF sizes a NULL rect from `[surface+36]`; body 82E6DA58 (IM_LOAD_IMMEDIATE, SQ_PROGRAM_CNTL) |
| Resolve | `82E72558` | forwards to 82E71400 (RB_COPY_* shadow +10776..+10788, EVENT_WRITE, WAIT_REG_MEM); engine caller `sub_82761D58` |
| SetPredication | `82E70860` | +13084, SET_BIN_MASK |
| SetViewport / SetScissorRect | `82E669B0` / `82E660F8` | integer viewport -> floats +13024 (82E66748); scissor +13052..+13064 |
| SetPixelShader / SetVertexShader | `82E5D518` / `82E5D720` | write +13068 / +13072 |
| SetVertexDeclaration | `82E5D938` | `stw r4,12240(r3)`, `+16 |= 0x80000` |
| SetStreamSource | `82E661F0` | vertex fetch 95 - s at +1912 - 8s from `[vb+24]`/`[vb+28]`; +12836 + 4s |
| SetIndices | `82E66398` | writes +12812 |
| SetTexture | `82E69DB8` | texture+28..+48 -> +1152 + 24i; +12920 + 4i |
| Set{Vertex,Pixel}ShaderConstantF | `82E5D170` / `82E5D248` | +1920/+6016 + 16*start; mask from r7 |
| VB Lock/Unlock, IB Lock/Unlock | `82E63AB0`/`82E62998`, `82E63B00`/`82E62A58` | common lock 82E63740 kinds 10/12; common unlock 82E62588 |
| TextureUnlock, FlushCpuRange | `82E67E30`, `82E74B28` | fetch dwords 1/5 & ~0xFFF; dcbf loop skipping 0x7F100000-0x86FFFFFF |
| KickSegment / NewSegment | `82E73FB8` / `82E72ED8` | 114 library callers, tail 82E73C78 with the segment fence 82E73528 (EVENT_WRITE_SHD); NewSegment sets +48/+52/+56 |
| SetShaderGPRAllocation | `82E5DAC0` | +11048, defaults 64/64 |
| QueryIssue / QueryGetData | `82E6B090` / `82E6B340` | [q+148] tiles, EVENT_WRITE_ZPD helper 82E78BF0 |
| Swap | `82E6FFE0` | calls VdSwap; reported as Present (frame end), also feeds the FPS overlay |

Device layout: XDK 7978's up to the register shadow; fence counter +11036, write-back block +11024, flag bytes
+11068 (7978's + 128); vertex declaration +12240; the current-object block 7978's + 368 (index buffer, targets,
streams, textures, viewport, shaders, predication confirmed by their setters). BeginTiling/EndTiling exist
(82E76A30/82E77198 likely) but the engine never calls them.

## Verify results
- 2026-11-02, first run with Swap as the frame end (only draws and shaders hooked): 96.8 % of 588 matched
  draws; 205 calls never executed (the BeginVertices taken for DrawVerticesUP, calls whose packets crossed an
  unhooked segment kick); 15 packets written outside hooked calls (clears); mismatches in SQ_PROGRAM_CNTL /
  SQ_INTERPOLATOR_CNTL and the program hashes right after them. Then everything above was hooked.
- 2026-11-02, all entry points hooked, in a race (frame 2566): **458 of 458 matched draws fully match (100 %)**,
  0 calls read incompletely, no register/constant/fetch/program mismatches. The 14 unmatched packets are the
  library's own Clear (9) and Resolve (5) draws. 226 of 684 CPU draw calls had no GPU execution in the window
  (to look at). No tiling, no memexport, no queries in the frame.
- Every Resolve in a race goes through the engine's wrapper (return address 0x82761F30, its callers 0x82762690
  and 0x82762744): `scene_resolve_caller` needs the outer caller (D65) to pick the scene copy.

## First frames (2026-11-02)
- `--native_x_renderer=true`: menus and races draw as on the Xenos path; 70 -> 100+ FPS at the same spots (1080p,
  1.5x of 1280x720). First-run hitches were pipeline compilation: the second run loaded the stored 456 pipelines
  and its worst frame over 600 was 11 ms.
- The garage's rider showed only helmet, gloves and boots. Its body (decl 0x405abfa0) is one draw of 71322
  indices: the library splits it into DRAW_INDX packets of 65532 + 5790 indices inside one call (garage verify:
  VGT_DMA_BASE/SIZE/DRAW_INITIATOR differ for the call executed twice), and NativeXRenderer draws one packet per
  call, the last piece. Fix: the DrawIndexedVertices hook issues list draws over 65532 indices as several calls
  while NativeXRenderer is on.

## First-run hitches (2026-11-02)
New players reported bad hitching. The scene is drawn into 1280x672 targets, not the 1280x720 output, so
NativeXRenderer's D101 rule (offscreen passes wait for their pipelines instead of being skipped) applied to every
scene draw: each new pipeline stalled the frame on a first run. `patches/native_x_renderer/0001` adds
`Profile::scene_width/scene_height`; the profile sets 1280x672, so scene draws are skipped for the frame or two
their pipeline takes, as in other games, and only the true offscreen passes wait.

## Antialiasing (2026-11-02)
NativeXRenderer applies SMAA/FXAA to the copy of the finished scene, found by Resolve's return address and the
output's size (D41/D65). Here every resolve comes through the engine's wrapper (0x82761F30, outer 0x82762690), and
the finished scene is a 1280x672 copy (texture 0x40451CE0 in the garage: the raw scene at the first resolve, the
post-processed scene at the last) drawn as a quad into the cleared 1280x720 frame before the HUD; the frame is then
resolved to the front buffer (outer caller 0x82762744). The patch adds `Profile::scene_resolve_before_output`: the
newest scene-size color copy from the scene caller is antialiased when the first draw into an output-size target
comes. The menu's Anti-aliasing row sets `native_x_renderer_antialiasing` (SMAA default) under NativeX.

## Testing (needs the player's PC)
1. `mad_riders.exe --native_x_renderer_trace=true`: play a race; F9 dumps a frame into the log.
2. `mad_riders.exe --native_x_renderer_verify=true`: F10 verifies a frame; the report lands in the log about
   10 frames later.
Send the log.
3. `mad_riders.exe --native_x_renderer=true`: the renderer itself (first frames, 2026-11-02).

## Shader cache (`shaders.cache`, 0.8.2)
`patches/native_x_renderer/0002` (NativeXRenderer D115) replaces the per-title pipeline storage and the separate
`.vkcache` in the user's cache folder with one indexed file, `shaders.cache` next to the exe
(`native_x_renderer::SetShaderCache`, `src/mad_riders_app.h`). It holds the guest microcode, the SPIR-V of every
translation, the pipeline records and the driver's pipeline cache. Before the game's first command is drawn the
renderer prepares all of it while the game waits, and NativeXRenderer's `ShaderCacheScreen` shows the progress. How it works and how to add it to another recomp: `thirdparty/native_x_renderer/docs/shader-cache.md` (in the patch).

On the first launch (no `shaders.cache`, or other game data: names and sizes of the `.rpack` files) the game hands
over every shader it ships. `src/shader_scan.cpp` reads them from the `.rpack` packs: section 0x30 holds the XDK
shader objects (`0x102A1101` vertex, `0x102A1100` pixel) with the microcode cut out, section 0x31 the microcode in
the same order on 32-byte boundaries. 19 packs give 885 unique shaders (495 pixel, 390 vertex) in about 2 s;
every pixel shader in a Xenia dump of a play session (204) is among them byte for byte. Their vertex shaders
differ from the shipped ones only in the vertex fetches, which the game's D3D patches for each vertex layout, so
vertex translations (and pipelines) still come from play. Pixel shaders are translated for each interpolator set
the shipped vertex shaders write.

The old caches (`<user folder>\cache\native_x_renderer` and the Xenos `cache\shaders`) are deleted at start.

# NativeXRenderer in After Burner Climax

[NativeXRenderer](https://github.com/ElGuslml/NativeXRenderer) ("NativeX") draws the game's Direct3D calls on the
host with Vulkan (DiligentCore) instead of emulating the Xbox 360's Xenos GPU. It is the game's only renderer. The
SDK's Xenos plugin stays loaded: its command processor is what NativeX observes, and it draws only if NativeX cannot
start on the GPU (logged as such).

This file is the map between the game's executable and NativeX: which functions are hooked, where the D3D device
keeps its state, and what is not done yet. The hooks and the profile are in `src/native_x_renderer_abc.cpp`. NativeX
itself is the submodule `thirdparty/native_x_renderer` (upstream `bf965e9`) plus `patches/native_x_renderer/`.

## The executable
World release, default.xex 0.0.1.2. **XDK 2.0.9328** (import table, library version `0x20247000`).
The D3D library is linked in at about `0x82230000`-`0x82262000`. Tools: NativeX `tools/ppc_index.py build`,
`pm4_consts.py 82000000 82600000`, `fn_summary.py range 82230000 82262000` (outputs in `out/`, not committed).

## Device layout (confirmed by the verifier: render state, fetch, programs, bool/loop constants all match)
**Up to the register shadow it is XDK 7978's (= Mad Riders' 21076):** read in DrawIndexedVertices' flush
(`82241320`):

| Field | Offset | Evidence |
|---|---|---|
| Dirty masks | `+0` VS consts, `+8` PS consts, `+16` shadow groups, `+24` fetch (low half, slot 0 = bit 31) / `0x2280`, `+32` `0x2300`/`0x2380`/…, `+40` | `ld` order and the flush calls |
| Command buffer write / end | `+48` / `+52`, `+56` | flush helpers `822561B0`, `82255E18` |
| Fetch constants | `+1152` | fetch flush `82256058`: `addi r10,r3,1152` |
| VS / PS float constants | `+1920` / `+6016` | `addi r6,r3,1920; li r5,0x4000` / `+6016, 0x4400` |
| Bool/loop constants | `+10112` | `addi r6,r31,10112; li r5,0x4900` |
| Register shadow groups | `10368/0x2000/16, 10444/0x2100/21, 10528/0x2180/5, 10548/0x2200/12, 10596/0x2280/21, 10680/0x2300/38, 10832/0x2380/8` | `addi r6,r31,N; li r5,REG` + mask widths |
| Fence counter | `+10908` | program flush, segment fence `82253E40` writes it |
| Write-back block | `+10896` | KickSegment candidate `822544B8` |
| Flag bytes | `+10940`..`+10943` | flush `lbz` |

**The block of current objects is XDK 7978's + 168** (Mad Riders: + 368):

| Field | Offset | Setter / evidence |
|---|---|---|
| Vertex declaration | `+11992` | `822437C8` (5 instructions, 8 callers) |
| Index buffer | `+12612` | `8223EC90` |
| Render targets | `+12616 + 4i` | SetRenderTarget `8223ED20` (`addi r11,r4,3154`) |
| Depth/stencil | `+12632` | `8223F780` (`stw r4,12632(r3)`) |
| Vertex buffers | `+12636 + 4s` | SetStreamSource `8223EB70` (`addi r11,r29,3159`) |
| Textures | `+12720 + 4i` | SetTexture `8224C410` (`addi r11,r4,3180`); also `8223E658`, `8223E6D8`, `8224C5A0` |
| Viewport (floats) | `+12824` | read by ClearF body `8224D0F0` (`lfs +12824..+12836`) |
| Scissor | `+12852`..`+12864` | `8223EA78`, `8223F3A0` |
| Pixel / vertex shader | `+12868` / `+12872` | `82243438` / `822435F8`; read by the program flush `82256FD8` |
| Predication | `+12884`..`+12892` | `82248518`, `8224DB08` |
| Tiling: saved targets, tile count | `+12900`..`+12920`, `+12924` | BeginTiling candidate `8224DC78` |

## Hooked entry points (`src/native_x_renderer_abc.cpp`)
| Call | Address | Notes |
|---|---|---|
| DrawVertices / DrawIndexedVertices | `82240F30` / `82241320` | flush + DRAW_INDX; (prim r4, base r5, start r6, count r7) |
| SetRenderTarget, SetDepthStencilSurface | `8223ED20`, `8223F780` | `+12616 + 4i`, `+12632` |
| Clear, ClearF | `8224D8C8`, `8224D7A0` | ClearF body `8224D0F0` |
| Resolve | `8224EC70` | RB_COPY_* shadow `+10776..+10788`; 8 sites |
| SetPredication, SetViewport, SetScissorRect | `82248518`, `8223F300`, `8223EA78` | |
| SetPixelShader, SetVertexShader, SetVertexDeclaration | `82243438`, `822435F8`, `822437C8` | |
| SetStreamSource, SetIndices, SetTexture | `8223EB70`, `8223EC90`, `8224C410` | |
| BeginShaderConstantF4 | `8224A530` | NativeX D130: the engine writes its float constants itself (12 sites) |
| RunCommandBuffer | `822496E0` | NativeX D130: recorded command buffers (1 engine site) |
| BeginTiling / EndTiling | `8224DC78` / `82248DB0` | 4x MSAA scene in three 1280x256 tiles; the end's r4 is a pointer (not the XDK argument layout), NativeX only uses it to end the pass |
| FlushCpuRange, KickSegment | `82255350`, `822544B8` | |
| Swap (reported as Present) | `82252730` | frame end; written out by hand: the performance overlay counts frames here |

## State (2026-11-08)
- The game is drawn by NativeX from boot to gameplay, full screen with the HUD, at 60 FPS.
- Verifier (`--native_x_renderer_verify=true`, F10 or `--native_x_renderer_verify_at=N`): **124 of 126 draws** of a
  stage frame match the GPU state exactly (was 8 of 99 before D130). The last mismatch (constants read before the
  game finished filling them) is addressed by reading them before the next draw; not re-verified yet.
- Not done:
  - Draws **inside recorded command buffers** (`RunCommandBuffer`, ~570 draw packets per frame) are tracked but not
    drawn by NativeX's renderer. Find what they draw (likely static scenery or effects) before deciding.
  - SET_CONSTANT packets with bin predication go to `predicated_writes`, which the tracker does not apply.
  - Not hooked: Begin/EndVertices (inline draws, `+13436/+13440`), buffer Lock/Unlock, TextureUnlock, NewSegment,
    SetShaderGPRAllocation, occlusion queries (EVENT_WRITE_ZPD at `82298838`, `8232AC70`), the other draw function
    `82249308`, the resource object layout (assumed XDK 7978's).
  - No DLSS / FSR 3.1 / frame generation: they need the main 3D view (view hooks or `upscale_resolve_caller`) and
    the camera's motion. The game steps its logic once per frame, so it stays at 60 FPS.

## Shader cache
`shaders.cache` next to the exe: one compressed, indexed file (NativeX D127-D129, from Mad Riders). On the first start
the game's main thread is held while NativeX translates every shader the game ships (`src/shader_scan.cpp`: the 101
gzip entries of `media\x360\shader_hlsl.farc`, 96 unique: 41 vertex, 55 pixel) and guesses 206 pixel-shader
translations (about 0.1 s); vertex shader variants (the D3D patches fetches per vertex declaration) and pipelines are
compiled when first drawn and kept. Draws wait up to 8 ms per frame for those (F1 > Graphics > New shaders).

## NativeX patches (`patches/native_x_renderer/`, applied by CI with `git am`)
| Patch | What |
|---|---|
| 0001 | Scene size and scene antialiasing (Mad Riders) |
| 0002-0004 | Shader cache: one indexed file, prepared before the game draws; scene wait budget; compressed records, more pipeline threads (D127-D129) |
| 0005 | Decision numbers of 0002-0004 moved to D127-D129 (upstream uses D115-D117) |
| 0006 | D130: BeginShaderConstantF4 (constants the game writes after the call) and RunCommandBuffer |
| 0007 | D131: the tracker's record is not extended across an applied run (crash with the renderer and verifier both on) |
| 0008 | D132: SetPaused, the last frame is presented again while the game is paused |
| 0009 | D133: the draws inside recorded command buffers (RunCommandBuffer) are drawn; optional predicated float constants |
| 0010 | D134: a bin-predicated INDIRECT_BUFFER is followed (RunCommandBuffer predicates every one) |
| 0011 | D135: D134 behind `native_x_renderer_predicated_indirect` (default off); indirect buffers and their shader loads logged |
| 0012 | D136: diagnostics: new programs from indirect buffers, a summary of each predicated indirect buffer |
| 0013 | D137: constants filled after BeginShaderConstantF4 are read before RunCommandBuffer |
| 0014 | D138: the verifier checks draws replayed from recorded command buffers |
| 0015 | D139: a recorded buffer's predicated SET_CONSTANTs are applied (but the per-tile window registers) |
| 0016 | D140: bin predication (mask & select) evaluated inside recorded buffers |
| 0017 | D141: the verifier compares replayed draws' float constants without shader analysis |

To change NativeX: `git -C thirdparty/native_x_renderer checkout bf965e9 && git am ../../patches/native_x_renderer/*`,
commit, then `git format-patch bf965e9..HEAD -o ../../patches/native_x_renderer` (replace the old files).

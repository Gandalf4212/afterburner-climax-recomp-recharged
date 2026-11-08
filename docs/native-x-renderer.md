# NativeXRenderer in After Burner Climax (mapping in progress)

[NativeXRenderer](https://github.com/ElGuslml/NativeXRenderer) draws the game's Direct3D calls on the host
(Vulkan, DiligentCore) instead of emulating the Xenos GPU. Method: NativeX `docs/integrating-a-game.md`; the same
work for Mad Riders is in mad-riders-recomp `docs/native-x-renderer.md` and `src/native_x_renderer_mad_riders.cpp`.

## The executable
World release, default.xex 0.0.1.2. **XDK 2.0.9328** (import table, library version `0x20247000`).
The D3D library is linked in at about `0x82230000`-`0x82262000`. Tools: NativeX `tools/ppc_index.py build`,
`pm4_consts.py 82000000 82600000`, `fn_summary.py range 82230000 82262000` (outputs in `out/`, not committed).

## Device layout (2026-11-08, from the code; to be confirmed by the observer, F10)
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

## Entry points found so far
| Entry point | Address | Evidence |
|---|---|---|
| DrawVertices | `82240F30` | flush + DRAW_INDX (auto index), 2 engine sites |
| DrawIndexedVertices | `82241320` | flush + DRAW_INDX with index data, (prim r4, base r5, start r6, count r7) |
| (UP draws?) | `82249308`, `822496E0` | same flush; to identify (one engine site each) |
| Resolve | `8224EC70` | RB_COPY_* shadow `+10776..+10788`, EVENT_WRITE, WAIT_REG_MEM; 8 sites |
| ClearF path | `8224D7A0` -> body `8224D0F0` | IM_LOAD_IMM, viewport/scissor reads |
| Swap (frame end) | `82252730` | calls VdSwap (`82501B3C`); engine `82170048`/`82170368`, library Present `8225D718` |
| KickSegment (candidate) | `822544B8` | 112 library callers; write-back/fence reads |
| Segment fence | `82253E40` | EVENT_WRITE_SHD, writes `+10908` |
| BeginTiling / EndTiling (candidates) | `8224DC78` / `82248DB0` | save targets, tile count / restore + WAIT_REG_MEM |
| SetRenderTarget, SetDepthStencilSurface, SetStreamSource, SetTexture, SetIndices, SetVertexDeclaration, SetPixelShader, SetVertexShader, SetScissorRect, SetPredication | see the table above | |

## First verify run (2026-11-08, owner's PC, Xenia renderer, frames 819 and 2400)
Render state, vertex fetch, texture fetch, shader programs, bool/loop constants: 0 mismatches (layout confirmed).
Frame 2400: 99 draw calls, 8 fully match (8.1 %). Two causes, both outside what NativeX hooks today:
- **Game-written float constants.** `8224A530(dev, pixel?, start, vec4Count)` reserves `4*count+5` dwords
  (`822546E8`), writes a SET_CONSTANT header (`0xC0002D00 | count<<16 | pred +12892`) and returns a pointer the
  engine then fills (BeginShaderConstantF4-style; no End call, it is inlined). 12 engine sites, e.g.
  `8216D824`: VS start 108, 8 vec4. The shadow at +1920/+6016 is bypassed, so the tracker's values are stale
  (VS c72..c75, c0, c16..c24; PS shadow holds non-floats). `8224A5B8` (3 sites in `821673E8`) writes one
  SET_CONSTANT dword (`0x00040104`). Needs a NativeX call that reads the data at the next tracked call.
- **Recorded command buffers.** `822496E0(dev, cmdbuf, pred)` = RunCommandBuffer: ANDs the device dirty masks
  with the buffer's (+24..+56), flushes, then emits INDIRECT_BUFFER (`0xC0013F00`) per segment from the list at
  cmdbuf+116. 1 engine site (`82166F78`). 576 draw packets came from such buffers. `82243950` (14 sites) emits
  3 INDIRECT_BUFFERs from device +13992..+15028 (library-owned; to identify).

## Still to find
VS/PS constant setters proper (no plain function writes `+1920 + 16*start`), viewport setter, Clear, Begin/EndVertices, buffer Lock/Unlock,
TextureUnlock, FlushCpuRange, NewSegment, SetShaderGPRAllocation, queries, the resource object layout.

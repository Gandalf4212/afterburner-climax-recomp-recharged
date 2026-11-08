// NativeXRenderer hooks and profile for Mad Riders (XDK 2.0.21076).
//
// Work in progress (docs/native-x-renderer.md): the entry points below are the ones mapped so far, with
// the evidence in that document. Everything is inactive unless one of NativeXRenderer's modes is turned on
// (--native_x_renderer_trace, --native_x_renderer_verify, --native_x_renderer); the Xenos path draws.
//
// Layout: the device structure matches XDK 7978's (Guitar Hero: Metallica) up to the end of the register
// shadow (six dirty masks from +0, command buffer +48, VS/PS float constants +1920/+6016, shadow groups from
// +10368: read in the draw's flush); the block of current objects is 7978's + 368 (index buffer +12812,
// render targets +12816, depth +12832, shaders +13068/+13072: read and written by the setters below). Fields
// marked "unverified" follow from that shift and still need the observer (F10) to confirm them.
#include <native_x_renderer/hooks.h>

#include <native_x_renderer/native_x_renderer.h>

// Draws (PM4 scan: DRAW_INDX with an index buffer / auto-index). There is no DrawVerticesUP in this library:
// 82E6E2E0 is BeginVertices (dev, prim, vertexCount, stride) -> vertex pointer: it leaves its fetch constant and
// DRAW_INDX after the write pointer and saves their end at +13844 (data +13848, dwords +13856); the engine fills
// the vertices and calls EndVertices (82E6E2D0: [+48] = [+13844]) itself.
NATIVE_X_RENDERER_HOOK(82E6E7A0, DrawVertices)
NATIVE_X_RENDERER_HOOK(82E6E2E0, BeginVertices)
NATIVE_X_RENDERER_HOOK(82E6E2D0, EndVertices)
// Targets, clears, resolves, predication, viewport, scissor
NATIVE_X_RENDERER_HOOK(82E66AE0, SetRenderTarget)         // +12816 + 4*index; engine alias 82E67148 (b)
NATIVE_X_RENDERER_HOOK(82E66E70, SetDepthStencilSurface)  // +12832, RB_DEPTH_INFO shadow
NATIVE_X_RENDERER_HOOK(82E6E210, Clear)                   // unpacks the D3DCOLOR, calls ClearF per rect
NATIVE_X_RENDERER_HOOK(82E6E0D8, ClearF)                  // NULL rect -> [surface+36] size; body 82E6DA58
NATIVE_X_RENDERER_HOOK(82E72558, Resolve)                 // -> 82E71400: RB_COPY_* shadow +10776.., WAIT_REG_MEM
NATIVE_X_RENDERER_HOOK(82E70860, SetPredication)          // +13084, SET_BIN_MASK
NATIVE_X_RENDERER_HOOK(82E669B0, SetViewport)             // D3DVIEWPORT9 -> floats at +13024 (82E66748)
NATIVE_X_RENDERER_HOOK(82E660F8, SetScissorRect)          // +13052..+13064
// Shaders, declaration, streams, textures, constants
NATIVE_X_RENDERER_HOOK(82E5D518, SetPixelShader)          // +13068
NATIVE_X_RENDERER_HOOK(82E5D720, SetVertexShader)         // +13072
NATIVE_X_RENDERER_HOOK(82E5D938, SetVertexDeclaration)    // stw r4,12240(r3); +16 |= 0x80000
NATIVE_X_RENDERER_HOOK(82E661F0, SetStreamSource)         // vertex fetch 95 - s at +1912 - 8s; +12836 + 4s
NATIVE_X_RENDERER_HOOK(82E66398, SetIndices)              // +12812
NATIVE_X_RENDERER_HOOK(82E69DB8, SetTexture)              // texture+28 -> +1152 + 24i; +12920 + 4i
NATIVE_X_RENDERER_HOOK(82E5D170, SetVertexShaderConstantF)  // +1920 + 16*start; +0 |= r7
NATIVE_X_RENDERER_HOOK(82E5D248, SetPixelShaderConstantF)   // +6016 + 16*start; +8 |= r7
// Resources
NATIVE_X_RENDERER_HOOK(82E63AB0, VertexBufferLock)        // common lock 82E63740, kind 10
NATIVE_X_RENDERER_HOOK(82E62998, VertexBufferUnlock)      // b 82E62588([vb+24] & ~3, 0)
NATIVE_X_RENDERER_HOOK(82E63B00, IndexBufferLock)         // kind 12
NATIVE_X_RENDERER_HOOK(82E62A58, IndexBufferUnlock)
NATIVE_X_RENDERER_HOOK(82E67E30, TextureUnlock)           // fetch dwords 1 and 5 & ~0xFFF
NATIVE_X_RENDERER_HOOK(82E74B28, FlushCpuRange)           // dcbf loop, skips 0x7F100000-0x86FFFFFF
// Command buffer, GPRs, queries
NATIVE_X_RENDERER_HOOK(82E73FB8, KickSegment)             // 114 library callers; tail 82E73C78 (fence 82E73528)
NATIVE_X_RENDERER_HOOK(82E72ED8, NewSegment)              // (dwords + 16) * 4 bytes; +48/+52/+56
NATIVE_X_RENDERER_HOOK(82E5DAC0, SetShaderGPRAllocation)  // +11048, defaults 64/64
NATIVE_X_RENDERER_HOOK(82E6B090, QueryIssue)              // tiles -> [q+148], EVENT_WRITE_ZPD via 82E78BF0
NATIVE_X_RENDERER_HOOK(82E6B340, QueryGetData)
// DrawIndexedVertices (82E6EBB8), written out by hand. The library splits a draw of more than 65532 indices into
// several DRAW_INDX packets inside one call; NativeXRenderer takes one draw packet per call, so it drew only the
// last piece (the garage's rider, 71322 indices: only the helmet, gloves and boots showed). While NativeXRenderer
// is on, such a list draw is issued here as several calls of at most 65532 indices, the size the library uses
// itself: the same packets, one per call. Strips and fans (which would need overlapping pieces) pass through.
REX_EXTERN(__imp__sub_82E6EBB8);
namespace {
constexpr uint32_t kMaxIndicesPerCall = 65532;  // a multiple of 3 (triangles, rects) and 4 (quads)
bool IsListPrimitive(uint32_t prim) {
  return prim == 1 || prim == 2 || prim == 4 || prim == 8 || prim == 13;  // point, line, triangle, rect, quad lists
}
void DrawIndexedVerticesTracked(PPCContext& ctx, uint8_t* base) {
  ::native_x_renderer::detail::Enter(::native_x_renderer::Call::kDrawIndexedVertices, ctx, base);
  __imp__sub_82E6EBB8(ctx, base);
  ::native_x_renderer::detail::Leave(::native_x_renderer::Call::kDrawIndexedVertices, ctx, base);
}
}  // namespace
REX_HOOK_RAW(sub_82E6EBB8) {
  if (!::native_x_renderer::detail::g_hooks_active) [[likely]] {
    __imp__sub_82E6EBB8(ctx, base);
    return;
  }
  const uint32_t prim = ctx.r4.u32, count = ctx.r7.u32;
  if (count <= kMaxIndicesPerCall || !IsListPrimitive(prim)) {
    DrawIndexedVerticesTracked(ctx, base);
    return;
  }
  const uint64_t dev = ctx.r3.u64, base_vertex = ctx.r5.u64;
  uint32_t start = ctx.r6.u32, left = count;
  while (left) {
    const uint32_t n = left < kMaxIndicesPerCall ? left : kMaxIndicesPerCall;
    ctx.r3.u64 = dev;
    ctx.r4.u64 = prim;
    ctx.r5.u64 = base_vertex;
    ctx.r6.u64 = start;
    ctx.r7.u64 = n;
    DrawIndexedVerticesTracked(ctx, base);
    start += n;
    left -= n;
  }
}

// Frame end. The engine calls Swap (82E6FFE0 -> VdSwap) itself once a frame (from 8275FC00); the library's
// own Present wrapper (82E70748) never ran in a race (first trace: no frame summaries). Swap is reported as
// Present, the call that ends a frame. Written out by hand: the FPS overlay counts frames here too.
#include "perf_overlay.h"
extern "C" void __imp__sub_82E6FFE0(PPCContext& __restrict ctx, uint8_t* base);
REX_HOOK_RAW(sub_82E6FFE0) {
  recomp_ui::perf_on_frame();
  if (!::native_x_renderer::detail::g_hooks_active) [[likely]] {
    __imp__sub_82E6FFE0(ctx, base);
    return;
  }
  ::native_x_renderer::detail::Enter(::native_x_renderer::Call::kPresent, ctx, base);
  __imp__sub_82E6FFE0(ctx, base);
  ::native_x_renderer::detail::Leave(::native_x_renderer::Call::kPresent, ctx, base);
}

namespace mad_riders {

const native_x_renderer::Profile& NativeXRendererProfile() {
  static const native_x_renderer::Profile profile = [] {
    native_x_renderer::Profile p;
    p.game = "Mad Riders (58411240)";
    p.xdk_version = "2.0.21076";
    p.output_width = 1280;
    p.output_height = 720;
    // The scene, its post-processing and its depth are drawn into 1280x672 targets (resolved at 1280x672), the 2D layer
    // and the front buffer at 1280x720. Without this every scene draw counted as offscreen and waited for its pipeline
    // (D101): hitches of up to seconds on a first run (patches/native_x_renderer/0001).
    p.scene_width = 1280;
    p.scene_height = 672;
    // Antialiasing (SMAA/FXAA) on the finished scene: every resolve goes through the engine's wrapper (Resolve's return
    // address 0x82761F30), and the scene's last copy (1280x672, after the post-processing) is drawn into the 1280x720
    // frame before the HUD; the patch antialiases the newest scene-size copy at that first draw into the frame.
    p.scene_resolve_caller = 0x82761F30;
    p.scene_resolve_before_output = true;
    native_x_renderer::DeviceLayout& d = p.device;
    d.vs_constants_dirty = 0;       // the draw's flush: ld +0 -> constants at +1920 (0x4000)
    d.ps_constants_dirty = 8;       // ld +8 -> +6016 (0x4400)
    d.fetch_constants_dirty = 24;   // the draw's flush: ld +24, low half -> fetch flush 82E79638
    d.fetch_dirty_slot0_bit = 31;
    d.command_buffer_write = 48;
    d.fetch_constants = 1152;       // fetch flush: addi r10,r3,1152
    d.vs_float_constants = 1920;
    d.ps_float_constants = 6016;
    d.bool_loop_constants = 10112;  // flush: addi r6,r31,10112 with 0x4900
    d.register_groups = {{
        {10368, 0x2000, 16},
        {10444, 0x2100, 21},
        {10528, 0x2180, 5},
        {10548, 0x2200, 12},
        {10596, 0x2280, 21},
        {10680, 0x2300, 38},
        {10832, 0x2380, 8},
    }};
    d.fence_counter = 11036;        // setters fence the old object with [+11036]; the segment fence writes it
    d.write_back_block = 11024;     // KickSegment: lwz [+11024], then [wb+0]
    d.flags = 11068;                // ClearF/KickSegment: tiling 0x20, 0x10, recording 0x80 bytes at +11068
    d.vertex_declaration = 12240;   // SetVertexDeclaration; read by the program flush 82E7A528
    d.index_buffer = 12812;
    d.render_targets = 12816;
    d.depth_stencil = 12832;
    d.vertex_buffers = 12836;       // SetStreamSource (addi r11,r4,3209)
    d.textures = 12920;             // SetTexture (addi r11,r4,3230)
    d.viewport = 13024;             // floats; read by SetScissorRect and the clear
    d.pixel_shader = 13068;
    d.vertex_shader = 13072;
    d.predication_mask = 13084;     // SetPredication
    d.tile_count = 13124;           // read by Resolve; tiling is not used by the engine
    d.tile_rects = 13128;           // unverified (7978 + 368)
    native_x_renderer::ObjectLayout& o = p.objects;  // ClearF ([surface+36] bits 18/3), SetTexture (+28), streams (+24/+28)
    o.texture_fetch = 28;
    o.surface_info = 24;
    o.surface_color_depth_info = 28;
    o.surface_size = 36;
    o.surface_width_minus_one = {18, 14};
    o.surface_height_minus_one = {3, 15};
    o.buffer_address = 24;
    o.buffer_size = 28;
    return p;
  }();
  return profile;
}

}  // namespace mad_riders

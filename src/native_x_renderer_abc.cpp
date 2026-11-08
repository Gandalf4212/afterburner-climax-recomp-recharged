// NativeXRenderer hooks and profile for After Burner Climax (World, XDK 2.0.9328).
//
// Mapping in progress (docs/native-x-renderer.md has the evidence). Inactive unless a NativeXRenderer mode is on
// (F1 > Renderer = NativeX, --native_x_renderer_trace, --native_x_renderer_verify); the Xenos path draws otherwise.
//
// Layout: XDK 7978's (= Mad Riders') up to the end of the register shadow, read in DrawIndexedVertices' flush;
// the block of current objects is 7978's + 168, read in the setters below. Not mapped yet (the observer, F10,
// reports what is missing): the VS/PS constant setters (likely inlined into the engine), Begin/EndVertices,
// buffer Lock/Unlock, TextureUnlock, NewSegment, SetShaderGPRAllocation, queries.
#include <native_x_renderer/hooks.h>

#include <native_x_renderer/native_x_renderer.h>

// Draws
NATIVE_X_RENDERER_HOOK(82240F30, DrawVertices)          // flush + DRAW_INDX auto-index
NATIVE_X_RENDERER_HOOK(82241320, DrawIndexedVertices)   // flush + DRAW_INDX with index data
// Targets, clears, resolves, predication, viewport, scissor
NATIVE_X_RENDERER_HOOK(8223ED20, SetRenderTarget)       // addi r11,r4,3154: +12616 + 4*index
NATIVE_X_RENDERER_HOOK(8223F780, SetDepthStencilSurface)  // stw r4,12632(r3)
NATIVE_X_RENDERER_HOOK(8224D8C8, Clear)                 // calls ClearF per rect
NATIVE_X_RENDERER_HOOK(8224D7A0, ClearF)                // body 8224D0F0 (IM_LOAD_IMM, viewport floats)
NATIVE_X_RENDERER_HOOK(8224EC70, Resolve)               // RB_COPY_* shadow +10776..+10788, WAIT_REG_MEM
NATIVE_X_RENDERER_HOOK(82248518, SetPredication)        // +12884..+12892
NATIVE_X_RENDERER_HOOK(8223F300, SetViewport)           // -> 8223F098, floats at +12824 (unverified)
NATIVE_X_RENDERER_HOOK(8223EA78, SetScissorRect)        // +12852..+12864
// Shaders, declaration, streams, textures
NATIVE_X_RENDERER_HOOK(82243438, SetPixelShader)        // +12868
NATIVE_X_RENDERER_HOOK(822435F8, SetVertexShader)       // +12872
NATIVE_X_RENDERER_HOOK(822437C8, SetVertexDeclaration)  // +11992
NATIVE_X_RENDERER_HOOK(8223EB70, SetStreamSource)       // addi r11,r29,3159: +12636 + 4*stream
NATIVE_X_RENDERER_HOOK(8223EC90, SetIndices)            // +12612
NATIVE_X_RENDERER_HOOK(8224C410, SetTexture)            // addi r11,r4,3180: +12720 + 4*index
// Constants the engine writes itself: reserves 4*count+5 dwords, SET_CONSTANT header, returns the data pointer
// (12 engine sites, e.g. 8216D824: VS start 108, 8 vec4). The shadow at +1920/+6016 is bypassed (first verify run).
NATIVE_X_RENDERER_HOOK(8224A530, BeginShaderConstantF4)
// Recorded command buffers: flush of the inherited state, then INDIRECT_BUFFER per segment (cmdbuf+116); 1 site.
NATIVE_X_RENDERER_HOOK(822496E0, RunCommandBuffer)
// Memory, command buffer
NATIVE_X_RENDERER_HOOK(82255350, FlushCpuRange)         // dcbf loop, 18 library callers
NATIVE_X_RENDERER_HOOK(822544B8, KickSegment)           // 112 library callers; write-back +10896, fence +10908
// Frame end: Swap calls VdSwap (82501B3C); the engine calls it (82170048, 82170368), and the library's Present.
NATIVE_X_RENDERER_HOOK(82252730, Present)

namespace after_burner_climax {

const native_x_renderer::Profile& NativeXRendererProfile() {
  static const native_x_renderer::Profile profile = [] {
    native_x_renderer::Profile p;
    p.game = "After Burner Climax (58410A06)";
    p.xdk_version = "2.0.9328";
    p.output_width = 1280;
    p.output_height = 720;
    native_x_renderer::DeviceLayout& d = p.device;
    d.vs_constants_dirty = 0;      // flush: ld +0 -> +1920, 0x4000
    d.ps_constants_dirty = 8;      // ld +8 -> +6016, 0x4400
    d.fetch_constants_dirty = 24;  // ld +24, low half -> fetch flush 82256058
    d.fetch_dirty_slot0_bit = 31;
    d.command_buffer_write = 48;
    d.fetch_constants = 1152;
    d.vs_float_constants = 1920;
    d.ps_float_constants = 6016;
    d.bool_loop_constants = 10112;
    d.register_groups = {{
        {10368, 0x2000, 16},
        {10444, 0x2100, 21},
        {10528, 0x2180, 5},
        {10548, 0x2200, 12},
        {10596, 0x2280, 21},
        {10680, 0x2300, 38},
        {10832, 0x2380, 8},
    }};
    d.fence_counter = 10908;      // segment fence 82253E40 writes it
    d.write_back_block = 10896;   // KickSegment reads it
    d.flags = 10940;              // flag bytes read by the flush (7978's)
    d.vertex_declaration = 11992;
    d.index_buffer = 12612;
    d.render_targets = 12616;
    d.depth_stencil = 12632;
    d.vertex_buffers = 12636;
    d.textures = 12720;
    d.viewport = 12824;
    d.pixel_shader = 12868;
    d.vertex_shader = 12872;
    d.predication_mask = 12884;
    d.tile_count = 12924;
    d.tile_rects = 12928;         // unverified (7978 + 168)
    // Resource objects: 7978's = POTCAWE's (as in Mad Riders); to be confirmed by the observer.
    native_x_renderer::ObjectLayout& o = p.objects;
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

}  // namespace after_burner_climax

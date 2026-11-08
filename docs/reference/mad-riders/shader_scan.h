// Finds every compiled shader Mad Riders ships, for the first-launch build of shaders.cache
// (native_x_renderer::SetShaderCache; NativeXRenderer's docs/shader-cache.md).
//
// Mad Riders (Chrome Engine) keeps its shaders in the .rpack resource packs under Haste\Data: a pack's
// sections are zlib streams; section type 0x30 holds the XDK shader objects (0x102A1101 vertex,
// 0x102A1100 pixel) with their microcode cut out, and the matching 0x31 section holds that microcode,
// one shader after another in the same order, each starting on a 32-byte boundary
// (native_x_renderer::shader_sources::FindXdkShadersSplit).
// Checked against Xenia's shader dump of a play session: every pixel shader it saw is found byte for byte.
// Vertex shaders are found as compiled; the game's D3D patches their vertex fetches for each vertex
// layout when it binds them, so those variants are learned while playing (and kept in shaders.cache).
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

#include <native_x_renderer/native_x_renderer.h>

namespace shader_scan {

struct Result {
  std::vector<native_x_renderer::GameShader> shaders;  // unique by type and microcode
  uint32_t packs = 0;                                   // .rpack files read
};

// Names and sizes of the packs (and this scanner's version): quick, without reading them. Never 0.
uint64_t fingerprint(const std::filesystem::path& data_dir);

// Reads every *.rpack in `data_dir` (assets\Haste\Data). progress(done, total) counts packs.
Result scan(const std::filesystem::path& data_dir, const std::function<void(uint32_t, uint32_t)>& progress = {});

}  // namespace shader_scan

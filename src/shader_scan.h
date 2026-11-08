// Finds every compiled shader After Burner Climax ships, for the first-launch build of shaders.cache
// (native_x_renderer::SetShaderCache; NativeXRenderer's docs/shader-cache.md).
//
// The game keeps its shaders in assets\media\x360\shader_hlsl.farc, a SEGA "FArC" archive: "FArC", the
// big-endian size of the entry table that follows the next word, the alignment (1), then per entry a
// nul-terminated name (blinn_frag_cubemap.0000000000110.vp / .fp), the data's offset, its compressed size and
// its size. Each entry is a gzip stream holding one whole XDK shader object (0x102A1101 vertex, 0x102A1100
// pixel), microcode inline (native_x_renderer::shader_sources::FindXdkShaders). World release: 101 entries,
// 46 vertex and 55 pixel shaders (checked 2026-11-08).
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

#include <native_x_renderer/native_x_renderer.h>

namespace shader_scan {

struct Result {
  std::vector<native_x_renderer::GameShader> shaders;  // unique by type and microcode
  uint32_t entries = 0;                                 // archive entries read
};

// Name and size of every .farc in `x360_dir` (assets\media\x360) and this scanner's version. Never 0.
uint64_t fingerprint(const std::filesystem::path& x360_dir);

// Reads every .farc in `x360_dir`. progress(done, total) counts archive entries.
Result scan(const std::filesystem::path& x360_dir, const std::function<void(uint32_t, uint32_t)>& progress = {});

}  // namespace shader_scan

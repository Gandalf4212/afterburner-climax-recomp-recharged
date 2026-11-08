// See shader_scan.h.
#include "shader_scan.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include <native_x_renderer/shader_sources.h>

#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace shader_scan {
namespace {

namespace sources = native_x_renderer::shader_sources;

uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

constexpr uint32_t kSectionShaders = 0x30, kSectionMicrocode = 0x31;
constexpr size_t kMicrocodeAlignment = 32;
// Bump when the scan finds more (the next start builds the game's shaders again).
constexpr uint32_t kScanVersion = 1;

// A section's bytes: inflated (zlib), or as stored when its packed size is 0.
bool section_data(const std::vector<uint8_t>& pack, uint32_t offset, uint32_t size, uint32_t packed,
                  std::vector<uint8_t>& out) {
  out.clear();
  if (!size) return true;
  if (!packed) {
    if (uint64_t(offset) + size > pack.size()) return false;
    out.assign(pack.begin() + offset, pack.begin() + offset + size);
    return true;
  }
  if (uint64_t(offset) + packed > pack.size()) return false;
  out.resize(size);
  int n = stbi_zlib_decode_buffer(reinterpret_cast<char*>(out.data()), int(size),
                                  reinterpret_cast<const char*>(pack.data() + offset), int(packed));
  if (n < 0) return false;
  out.resize(size_t(n));
  return true;
}

}  // namespace

uint64_t fingerprint(const std::filesystem::path& data_dir) {
  return sources::FingerprintFiles(data_dir, sources::ListFiles(data_dir, {".rpack"}), kScanVersion);
}

Result scan(const std::filesystem::path& data_dir, const std::function<void(uint32_t, uint32_t)>& progress) {
  Result result;
  std::vector<std::filesystem::path> packs = sources::ListFiles(data_dir, {".rpack"});
  uint32_t done = 0;
  for (const auto& path : packs) {
    if (progress) progress(done, uint32_t(packs.size()));
    ++done;
    std::ifstream file(path, std::ios::binary);
    std::vector<uint8_t> pack((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    // RP5B header (big-endian): magic, version, ?, parts, sections, files, ... then 20-byte section entries:
    // u8 ?, u8 flags, u8 ?, u8 type, u32 offset, u32 size, u32 packed size, u32 count.
    if (pack.size() < 36 || std::memcmp(pack.data(), "RP5B", 4) != 0) continue;
    uint32_t section_count = be32(&pack[16]);
    if (36 + uint64_t(section_count) * 20 > pack.size()) continue;
    ++result.packs;
    // The k-th shader section goes with the k-th microcode section.
    std::vector<std::vector<uint8_t>> headers, microcode;
    for (uint32_t s = 0; s < section_count; ++s) {
      const uint8_t* e = &pack[36 + s * 20];
      uint32_t type = e[3];
      if (type != kSectionShaders && type != kSectionMicrocode) continue;
      std::vector<uint8_t> data;
      if (!section_data(pack, be32(e + 4), be32(e + 8), be32(e + 12), data)) data.clear();
      (type == kSectionShaders ? headers : microcode).push_back(std::move(data));
    }
    for (size_t k = 0; k < headers.size() && k < microcode.size(); ++k) {
      sources::FindXdkShadersSplit(headers[k].data(), headers[k].size(), microcode[k].data(), microcode[k].size(),
                                   kMicrocodeAlignment, result.shaders);
    }
  }
  if (progress) progress(done, uint32_t(packs.size()));
  sources::Deduplicate(result.shaders);
  return result;
}

}  // namespace shader_scan

// After Burner Climax's shader scan (shader_scan.h).
#include "shader_scan.h"

#include <cstring>
#include <fstream>
#include <string>

#include <native_x_renderer/shader_sources.h>
#include <rex/logging.h>

// Only stb_image's zlib decoder, with internal linkage (the SDK may build its own copy).
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_FAILURE_STRINGS
#include <stb_image.h>

namespace shader_scan {
namespace {

namespace sources = native_x_renderer::shader_sources;

constexpr uint32_t kScanVersion = 1;

uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

std::vector<uint8_t> ReadFile(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
}

// A gzip member (RFC 1952) inflated into exactly `size` bytes; false if it is not one or does not fit.
bool Gunzip(const uint8_t* data, size_t length, uint32_t size, std::vector<uint8_t>& out) {
  if (length < 18 || data[0] != 0x1F || data[1] != 0x8B || data[2] != 8) return false;
  const uint8_t flags = data[3];
  size_t p = 10;
  if (flags & 4) {  // FEXTRA
    if (p + 2 > length) return false;
    p += 2 + (data[p] | size_t(data[p + 1]) << 8);
  }
  for (uint8_t bit : {uint8_t(8), uint8_t(16)}) {  // FNAME, FCOMMENT: nul-terminated
    if (!(flags & bit)) continue;
    while (p < length && data[p]) ++p;
    ++p;
  }
  if (flags & 2) p += 2;  // FHCRC
  if (p >= length) return false;
  out.resize(size);
  int n = stbi_zlib_decode_noheader_buffer(reinterpret_cast<char*>(out.data()), int(size),
                                           reinterpret_cast<const char*>(data + p), int(length - p));
  return n == int(size);
}

struct Entry {
  std::string name;
  uint32_t offset, compressed, size;
};

// The entries of a FArC archive (shader_scan.h); empty if it is not one.
std::vector<Entry> ReadTable(const std::vector<uint8_t>& d) {
  std::vector<Entry> entries;
  if (d.size() < 12 || std::memcmp(d.data(), "FArC", 4) != 0) return entries;
  const size_t table_end = std::min<size_t>(d.size(), 8 + size_t(Be32(&d[4])));
  for (size_t p = 12; p < table_end;) {
    const void* nul = std::memchr(&d[p], 0, table_end - p);
    if (!nul) break;
    size_t name_end = static_cast<const uint8_t*>(nul) - d.data();
    if (name_end + 13 > table_end) break;
    Entry e{std::string(reinterpret_cast<const char*>(&d[p]), name_end - p), Be32(&d[name_end + 1]),
            Be32(&d[name_end + 5]), Be32(&d[name_end + 9])};
    p = name_end + 13;
    if (size_t(e.offset) + e.compressed > d.size() || e.size > (16u << 20)) continue;
    entries.push_back(std::move(e));
  }
  return entries;
}

}  // namespace

uint64_t fingerprint(const std::filesystem::path& x360_dir) {
  return sources::FingerprintFiles(x360_dir, sources::ListFiles(x360_dir, {".farc"}), kScanVersion);
}

Result scan(const std::filesystem::path& x360_dir, const std::function<void(uint32_t, uint32_t)>& progress) {
  Result result;
  std::vector<std::pair<std::vector<uint8_t>, std::vector<Entry>>> archives;
  uint32_t total = 0;
  for (const auto& path : sources::ListFiles(x360_dir, {".farc"})) {
    auto data = ReadFile(path);
    auto table = ReadTable(data);
    total += uint32_t(table.size());
    archives.emplace_back(std::move(data), std::move(table));
  }
  std::vector<uint8_t> inflated;
  for (const auto& [data, table] : archives) {
    for (const Entry& e : table) {
      if (Gunzip(&data[e.offset], e.compressed, e.size, inflated)) {
        sources::FindXdkShaders(inflated.data(), inflated.size(), result.shaders);
      } else {
        REXLOG_WARN("Shader scan: {} could not be inflated", e.name);
      }
      ++result.entries;
      if (progress) progress(result.entries, total);
    }
  }
  sources::Deduplicate(result.shaders);
  return result;
}

}  // namespace shader_scan

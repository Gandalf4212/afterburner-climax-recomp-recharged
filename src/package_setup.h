// First-run setup from the player's own XBLA package (see package_setup.cpp).
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
bool package_extract(const std::filesystem::path& package, const std::filesystem::path& out, std::string& err, int* bad_hashes);
// Finds a package for title_id next to the exe and extracts it into assets.
// 1 = extracted, 0 = no package found, -1 = failed (err says why)
int package_setup_auto(const std::filesystem::path& exe_dir, const std::filesystem::path& assets, uint32_t title_id,
                       std::string& err, std::string& used);
// Runs on start-up: makes the assets folder from a package next to the exe if it isn't there yet.
// Returns false (after telling the player why) when the game can't start.
bool package_setup_first_run(const std::filesystem::path& exe_dir, const std::filesystem::path& assets, uint32_t title_id,
                             const std::string& game);
// Checks that assets/default.xex is the exact executable this build was
// recompiled from (size + SHA-1). A different version (a title update, another
// release) can't run: tells the player why and returns false.
bool xex_version_check(const std::filesystem::path& assets, const char* expected_sha1_hex,
                       uint64_t expected_size, const std::string& game);

// A build of the game this package can run: the default.xex it was recompiled
// from, and the exe (next to this one) that runs it.
struct XexVariant {
  const char* name;       // shown to the player, e.g. "World release (0.0.1.2)"
  const char* sha1_hex;
  uint64_t size;
  const char* exe;        // file name of the exe for this variant
};
// Checks assets/default.xex against the variant this exe was built for
// (variants[self]). If it is another listed variant whose exe is next to this
// one, starts that exe with the same arguments and returns 2 (the caller
// exits). Returns 1 if this exe can run it, 0 after telling the player it is
// an unsupported version.
int xex_variant_check(const std::filesystem::path& assets, const std::filesystem::path& exe_dir,
                      const XexVariant* variants, int count, int self, const std::string& game);

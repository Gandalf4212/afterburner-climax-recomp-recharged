// Writes after_burner_climax.toml: the banner, every setting the F1 menu offers
// (always listed, with a comment, even at its default value), then any other
// setting changed from its default (key binds etc.).
//
// ReXGlue's own SaveConfig only writes values that differ from the default, so
// the menu settings would otherwise be missing from the file until changed.
// Adapted from mad-riders-recomp's config_file.h.
#pragma once

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <set>
#include <sstream>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>

namespace recomp_ui {

// Each line gets a "# " prefix when written.
inline constexpr const char* kConfigBanner = R"(     _____   _____  __                 __________
    /  _  \_/ ____\/  |_  ___________  \______   \__ _________  ____   ___________
   /  /_\  \   __\\   __\/ __ \_  __ \  |    |  _/  |  \_  __ \/    \_/ __ \_  __ \
  /    |    \  |   |  | \  ___/|  | \/  |    |   \  |  /|  | \/   |  \  ___/|  | \/
  \____|__  /__|   |__|  \___  >__|     |______  /____/ |__|  |___|  /\___  >__|
          \/                 \/                \/                  \/     \/
_________ .__  .__                        __________
\_   ___ \|  | |__| _____ _____  ___  ___ \______   \ ____   ____  ____   _____ ______
/    \  \/|  | |  |/     \\__  \ \  \/  /  |       _// __ \_/ ___\/  _ \ /     \\____ \
\     \___|  |_|  |  Y Y  \/ __ \_>    <   |    |   \  ___/\  \__(  <_> )  Y Y  \  |_> >
 \______  /____/__|__|_|  (____  /__/\_ \  |____|_  /\___  >\___  >____/|__|_|  /   __/
        \/              \/     \/      \/         \/     \/     \/            \/|__|

After Burner Climax Recomp (Recharged) settings. Changed by the F1 menu; safe to edit by
hand while the game is closed. Command-line options (--name=value) override it.)";

struct ConfigItem {
  const char* name;     // cvar
  const char* comment;  // shown above it; empty = none
};

struct ConfigSection {
  const char* title;
  std::initializer_list<ConfigItem> items;
};

// Settings that are always written. Missing cvars (NativeX ones in a build
// without it) are skipped.
inline const ConfigSection kConfigSections[] = {
    {"Renderer",
     {
         {"recomp_renderer", "\"xenia\" = emulated Xenos GPU (default), \"nativex\" = NativeXRenderer (Vulkan)"},
     }},
    {"Display",
     {
         {"fullscreen", "true = fullscreen, false = windowed (F11 switches)"},
         {"fullscreen_exclusive", "With fullscreen: true = exclusive (switches the display mode), false = borderless"},
         {"window_width", "Window size in windowed mode (0 = 1280)"},
         {"window_height", ""},
         {"present_letterbox", "true = keep 16:9 with black bars, false = stretch"},
     }},
    {"Graphics: Xenia renderer",
     {
         {"resolution_scale", "Render scale: 1 (1280x720), 2 or 3"},
         {"vsync", "true = wait for the monitor refresh"},
     }},
    {"Graphics: NativeX renderer",
     {
         {"native_x_renderer_resolution_scale",
          "Render scale: 0 = fit the screen, 1, 1.5, 2 or 3 (above the screen smooths edges)"},
         {"native_x_renderer_antialiasing", "Anti-aliasing: \"smaa\" (default), \"fxaa\" or \"off\""},
         {"native_x_renderer_scene_wait_ms",
          "New shaders: ms per frame draws may wait for a first-time compile (0 = pop in, 8, 33)"},
         {"native_x_renderer_present_mode",
          "Screen sync: \"\" = fast, no tearing; \"vsync\"; \"immediate\" (G-Sync/FreeSync)"},
     }},
    {"Graphics: both",
     {
         {"anisotropic_override", "Texture filtering: -1 game default, 0 off, 2 = 2x, 3 = 4x, 4 = 8x, 5 = 16x"},
     }},
    {"Audio",
     {
         {"audio_maxqframes", "Audio buffer frames: 8, 16, 32 or 64 (bigger fixes crackling)"},
         {"xma_honor_stream_skip", "true = drop XMA encoder delay / padding like the console (seamless loops)"},
     }},
    {"Controls",
     {
         {"mnk_mode", "true = keyboard plays as controller 1 (keys under Edit keyboard keys)"},
     }},
    {"Logging",
     {
         {"recomp_log", "true = write a log file in logs\\ (for bug reports), false = no log (default)"},
     }},
};

// True when this run turned logging off at start-up (log_level/log_file then hold values the app set).
inline bool g_log_forced_off = false;

inline std::string toml_value(const rex::cvar::FlagEntry& e, const std::string& v) {
  return e.type == rex::cvar::FlagType::String ? "\"" + v + "\"" : v;
}

inline void save_config(const std::filesystem::path& path) {
  if (path.empty()) return;
  std::set<std::string> written;
  if (g_log_forced_off) {
    written.insert("log_level");
    written.insert("log_file");
  }
  std::string out;
  {
    std::istringstream banner(kConfigBanner);
    for (std::string line; std::getline(banner, line);) out += line.empty() ? "#\n" : "# " + line + "\n";
    out += "\n";
  }
  auto& registry = rex::cvar::GetRegistry();
  auto find = [&](const std::string& name) -> const rex::cvar::FlagEntry* {
    for (const auto& e : registry)
      if (e.name == name) return &e;
    return nullptr;
  };
  for (const auto& section : kConfigSections) {
    std::string body;
    for (const auto& item : section.items) {
      const auto* e = find(item.name);
      if (!e) continue;
      if (item.comment && *item.comment) body += "# " + std::string(item.comment) + "\n";
      body += e->name + " = " + toml_value(*e, e->getter()) + "\n";
      written.insert(e->name);
    }
    if (!body.empty()) out += "# ---- " + std::string(section.title) + "\n" + body + "\n";
  }
  std::string other;
  for (const auto& e : registry) {
    if (written.count(e.name) || e.getter() == e.default_value) continue;
    other += e.name + " = " + toml_value(e, e.getter()) + "\n";
  }
  if (!other.empty()) out += "# ---- Other changed settings (key binds, internal)\n" + other;
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    REXLOG_ERROR("Could not write {}", path.string());
    return;
  }
  f << out;
}

}  // namespace recomp_ui

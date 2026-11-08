// mad_riders - ReXGlue Recompiled Project
//
// App hooks: first-run setup from the player's package, GPU plugin,
// full-game license, first-launch defaults, and the F1/Esc settings menu.
// Structure follows afterburner-climax-recomp (MIT, (c) 2026 darkuni).

#pragma once

#include <cstdlib>
#include <functional>
#include <system_error>
#include <vector>
#include <memory>
#include <mutex>
#include <filesystem>
#include <string>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/input/input_system.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <rex/system/xthread.h>
#include <rex/ui/keybinds.h>

#include <imgui.h>

#include "config_file.h"
#include "perf_overlay.h"
#include "crash_handler.h"
#include "debug_dump.h"
#include "dlc.h"
#include "frame_rate.h"
#include "game_keys.h"
#include "game_menu.h"
#include "package_setup.h"

#include "shader_scan.h"

#include <native_x_renderer/native_x_renderer.h>
#include <native_x_renderer/shader_cache_screen.h>
namespace mad_riders {
const native_x_renderer::Profile& NativeXRendererProfile();
}

#ifndef MADRIDERS_BUILD
#define MADRIDERS_BUILD "local"
#endif

REXCVAR_DEFINE_BOOL(recomp_log, false, "Recomp",
                    "Write a log file in logs\\ (for bug reports); off by default");
REXCVAR_DEFINE_BOOL(recomp_full_game, true, "Recomp",
                    "Run as the purchased game (false = trial, the console default)");

// The default.xex this build was recompiled from (Mad Riders XBLA, 0.0.0.2).
inline constexpr package_setup::GameInfo kMadRiders{
    "Mad Riders", 0x58411240u, "90349e4daf4812f27faf5c0a9e01edb5ff7d5219", 21446656u};

class MadRidersApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<MadRidersApp>(new MadRidersApp(ctx, "mad_riders",
        PPCImageConfig));
  }

  // Game data lives in "assets" next to the exe. On the first start it is made
  // from the player's own XBLA package (found automatically or picked in a
  // file dialog). --game_data_root still overrides it.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    const auto exe_dir = rex::filesystem::GetExecutableFolder();
    ApplyLogSwitch(paths.config_path);
    crash_handler::install(exe_dir);
    if (paths.game_data_root.empty()) {
      paths.game_data_root = exe_dir / "assets";
    }
    if (!package_setup::ensure_assets(exe_dir, paths.game_data_root, kMadRiders)) {
      std::exit(1);
    }
    config_path_ = paths.config_path;
    game_data_root_ = paths.game_data_root;
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    REXLOG_INFO("Mad Riders recomp, build {}", MADRIDERS_BUILD);
    // GPU emulation is opt-in in ReXGlue; default to the Xenos plugin.
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
    // XBLA titles run as the trial unless the license mask says purchased
    // (bit 0 = full game). Unlock unless the user set license_mask themselves.
    // --recomp_full_game=false keeps the console's default (trial), as Xenia shows it.
    // Decided on every launch (an older build saved license_mask=1 into the config).
    rex::cvar::SetFlagByName("license_mask", rex::cvar::Query<bool>("recomp_full_game") ? "1" : "0");
    frame_rate::apply();
    REXLOG_INFO("License: {} (license_mask={})",
                rex::cvar::Query<uint32_t>("license_mask") & 1 ? "full game" : "trial",
                rex::cvar::Query<uint32_t>("license_mask"));
    // First launch: windowed 1280x720, keyboard on, racing key layout.
    // After this everything comes from mad_riders.toml (edited by the F1 menu).
    if (!rex::cvar::Query<bool>("recomp_defaults_applied")) {
      rex::cvar::SetFlagByName("fullscreen", "false");
      rex::cvar::SetFlagByName("window_width", "1280");
      rex::cvar::SetFlagByName("window_height", "720");
      rex::cvar::SetFlagByName("mnk_mode", "true");
      rex::cvar::SetFlagByName("audio_maxqframes",
                               std::to_string(recomp_ui::game_keys::kDefaultAudioFrames));
      recomp_ui::GameMenu::ApplyDefaultKeys();
      rex::cvar::SetFlagByName("recomp_defaults_applied", "true");
      if (!config_path_.empty()) {
        recomp_ui::save_config(config_path_);
      }
      REXLOG_INFO("First launch: wrote default settings to {}",
                  config_path_.filename().string());
    }
    // Defaults added by later builds, applied once on top of existing settings.
    if (rex::cvar::Query<int32_t>("recomp_defaults_version") < 2) {
      // Write the log out every second so the lines before a crash aren't lost.
      rex::cvar::SetFlagByName("log_flush_interval", "1");
      rex::cvar::SetFlagByName("recomp_defaults_version", "2");
      if (!config_path_.empty()) recomp_ui::save_config(config_path_);
    }
    // Version 3: the config file lists every menu setting; rewrite it once.
    if (rex::cvar::Query<int32_t>("recomp_defaults_version") < 3) {
      rex::cvar::SetFlagByName("recomp_defaults_version", "3");
      recomp_ui::save_config(config_path_);
    }
    // NativeXRenderer is the renderer (since 0.8.2 the only one: the Xenos choice is gone). The SDK's own
    // shader storage is for the Xenos path, which draws nothing here: off.
    rex::cvar::SetFlagByName("native_x_renderer", "true");
    rex::cvar::SetFlagByName("store_shaders", "false");
    if (rex::cvar::Query<int32_t>("recomp_defaults_version") < 5) {
      rex::cvar::SetFlagByName("recomp_defaults_version", "5");
      recomp_ui::save_config(config_path_);
    }
    // Version 6: draws wait up to 8 ms per frame for a shader compiled for the first time (F1 > New shaders).
    if (rex::cvar::Query<int32_t>("recomp_defaults_version") < 6) {
      rex::cvar::SetFlagByName("native_x_renderer_scene_wait_ms", "8.0");
      rex::cvar::SetFlagByName("recomp_defaults_version", "6");
      recomp_ui::save_config(config_path_);
    }
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    perf_overlay_ = std::make_unique<recomp_ui::PerfOverlay>(drawer);
    native_x_renderer::ShaderCacheScreen::Style style;
    style.title = "MAD RIDERS";
    shader_cache_screen_ = std::make_unique<native_x_renderer::ShaderCacheScreen>(drawer, style);
    rex::ui::RegisterBind("bind_menu", "F1", "Open/close the settings menu",
                          [this] { ToggleMenu(); });
    rex::ui::RegisterBind("bind_menu_alt", "Escape", "Open/close the settings menu",
                          [this] { ToggleMenu(); });
    // Debug: F8 saves a memory snapshot (memdump_<time> next to the exe) for
    // offline analysis, e.g. finding decoded video frames.
    rex::ui::RegisterBind("bind_memdump", "F8", "Save a memory snapshot (debug)",
                          [this] { TakeSnapshot("F8"); });
    rex::ui::RegisterBind("bind_fullscreen", "F11", "Toggle fullscreen", [this] {
      if (menu_ && menu_->capturing()) return;
      bool fs = !window()->IsFullscreen();
      rex::cvar::SetFlagByName("fullscreen", fs ? "true" : "false");
      window()->SetFullscreen(fs);
      if (!config_path_.empty()) recomp_ui::save_config(config_path_);
    });
  }

  // DLC packages put next to the exe (e.g. 58411240\00000002\<file>) are
  // installed before the game starts, so its content scan sees them.
  void OnPreLaunchModule() override {
    dlc::install_found(runtime()->kernel_state(), runtime()->user_data_root(),
                       rex::filesystem::GetExecutableFolder(), kMadRiders.title_id);
    REXLOG_INFO("DLC installed: {}", [&] {
      auto s = dlc::installed_summary(runtime()->kernel_state(), kMadRiders.title_id);
      return s.empty() ? std::string("none") : s;
    }());
  }

  // The shader cache is prepared before the game's main thread runs at all (held here, resumed when done), so
  // its timers, intro video and audio never see the wait; the status screen shows meanwhile.
  void OnPostLaunchModule(rex::system::XThread* thread) override {
    if (!thread || !native_x_renderer::Renders()) return;
    thread->Suspend();
    native_x_renderer::PrepareShaderCacheBeforeLaunch([thread] { thread->Resume(); });
  }

  // The game must not see keys/buttons while the menu is open.
  void OnPostSetup() override {
    // Re-assert the crash filter in case SDL or a driver replaced it.
    crash_handler::install(rex::filesystem::GetExecutableFolder());
    SetUpShaderCache();
    // NativeXRenderer draws the game. (If it cannot start, on a GPU without the Vulkan features it needs, the
    // SDK's command processor still draws, as a last resort; the log says why.)
    native_x_renderer::Initialize(mad_riders::NativeXRendererProfile(),
                                  runtime() ? runtime()->graphics_system() : nullptr, window(),
                                  runtime() ? runtime()->cache_root() : std::filesystem::path());
    auto* input_sys = static_cast<rex::input::InputSystem*>(runtime()->input_system());
    if (input_sys && imgui_drawer()) {
      input_sys->SetActiveCallback([this]() {
        if (menu_) return false;
        return !imgui_drawer()->GetIO().WantCaptureMouse;
      });
    }
  }

  void OnShutdown() override {
    shader_cache_screen_.reset();
    perf_overlay_.reset();
  }

 private:
  // recomp_log (off by default; F1 menu, mad_riders.toml, --recomp_log=true): the log file in logs\.
  // The SDK sets its logging up right after this callback, from the config file, and always opens a
  // file: with logging off, every category is set to "off" (nothing is formatted or written) and the
  // file goes to the temp folder instead of a new logs\mad_riders_NNN.log each launch.
  // config_file.h leaves these two out of mad_riders.toml for the rest of the run.
  static void ApplyLogSwitch(const std::filesystem::path& config_path) {
    if (std::filesystem::exists(config_path)) rex::cvar::LoadConfig(config_path);
    if (rex::cvar::Query<bool>("recomp_log") || !rex::cvar::Query<std::string>("log_file").empty()) return;
    std::error_code ec;
    auto temp = std::filesystem::temp_directory_path(ec);
    if (ec) return;
    rex::cvar::SetFlagByName("log_level", "off");
    rex::cvar::SetFlagByName("log_file", (temp / "mad_riders_log_off.log").string());
    recomp_ui::g_log_forced_off = true;
  }

  // shaders.cache next to the exe: every shader and pipeline, prepared before the game draws. On the first
  // launch (or with other game data) all of the game's shaders, found in its .rpack files, go into it.
  // The caches of older builds (the user's cache folder) are removed.
  void SetUpShaderCache() {
    const auto exe_dir = rex::filesystem::GetExecutableFolder();
    if (runtime() && !runtime()->cache_root().empty()) {
      for (const char* old : {"native_x_renderer", "shaders"}) {
        std::error_code ec;
        auto dir = runtime()->cache_root() / old;
        if (std::filesystem::exists(dir, ec)) {
          std::filesystem::remove_all(dir, ec);
          REXLOG_INFO("Old shader cache {}: {}", dir.string(), ec ? "not removed: " + ec.message() : "removed");
        }
      }
    }
    native_x_renderer::ShaderCacheSource source;
    source.file = exe_dir / "shaders.cache";
    const auto data_dir = game_data_root_ / "Haste" / "Data";
    source.game_fingerprint = shader_scan::fingerprint(data_dir);
    source.scan = [data_dir](const std::function<void(uint32_t, uint32_t)>& progress) {
      shader_scan::Result found = shader_scan::scan(data_dir, progress);
      REXLOG_INFO("Shader scan: {} shaders in {} packs", found.shaders.size(), found.packs);
      return std::move(found.shaders);
    };
    native_x_renderer::SetShaderCache(std::move(source));
  }

  void TakeSnapshot(const char* why) {
    static std::mutex m;
    std::lock_guard<std::mutex> lock(m);
    REXLOG_INFO("{} memory snapshot: starting", why);
    auto dir = debug_dump::save_memory(runtime()->memory(), rex::filesystem::GetExecutableFolder());
    REXLOG_INFO("{} memory snapshot: {}", why, dir.empty() ? "failed" : dir.string());
    rex::FlushLogging();
  }

  void ToggleMenu() {
    if (menu_) {
      if (menu_->capturing()) return;  // Esc cancels the key capture instead
      delete menu_;                    // removes itself from the drawer
      menu_ = nullptr;
      return;
    }
    if (!imgui_drawer()) return;
    recomp_ui::MenuHost host;
    host.title = "MAD RIDERS";
    host.config_path = config_path_;
    host.set_fullscreen = [this](bool fs) {
      app_context().CallInUIThreadDeferred([this, fs] { window()->SetFullscreen(fs); });
    };
    host.quit = [this] {
      app_context().CallInUIThreadDeferred([this] { window()->RequestClose(); });
    };
    host.on_closed = [this] { menu_ = nullptr; };
    host.import_dlc = [this] {
      dlc::start_import(runtime()->kernel_state(), runtime()->user_data_root(),
                        rex::filesystem::GetExecutableFolder(), kMadRiders.title_id);
    };
    host.dlc_status = [] { return dlc::import_status(); };
    // Re-read the installed list only when an import finished (not every frame).
    dlc_summary_status_ = "\x01";
    host.dlc_installed = [this] {
      auto st = dlc::import_status();
      if (st != dlc_summary_status_) {
        dlc_summary_status_ = st;
        dlc_summary_ = dlc::installed_summary(runtime()->kernel_state(), kMadRiders.title_id);
      }
      return dlc_summary_;
    };
    menu_ = new recomp_ui::GameMenu(imgui_drawer(), std::move(host));
  }

  std::filesystem::path config_path_;
  std::filesystem::path game_data_root_;
  std::unique_ptr<native_x_renderer::ShaderCacheScreen> shader_cache_screen_;
  recomp_ui::GameMenu* menu_ = nullptr;  // owned by the ImGui drawer
  std::string dlc_summary_, dlc_summary_status_;
  std::unique_ptr<recomp_ui::PerfOverlay> perf_overlay_;
};

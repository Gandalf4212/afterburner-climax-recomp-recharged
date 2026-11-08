// after_burner_climax - ReXGlue Recompiled Project
//
// App hooks: default paths, GPU plugin, full-game license, first-launch
// defaults, and the F1/Esc settings menu (src/game_menu.*).

#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/filesystem/vfs.h>
#include <rex/input/input_system.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/ui/keybinds.h>

#include <imgui.h>

#include "config_file.h"
#include "exit_watchdog.h"
#include "game_keys.h"
#include "game_menu.h"
#include "package_setup.h"

#ifdef ABC_NATIVE_X_RENDERER
#include <native_x_renderer/native_x_renderer.h>
namespace after_burner_climax {
const native_x_renderer::Profile& NativeXRendererProfile();
}
#endif

class AfterBurnerClimaxApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<AfterBurnerClimaxApp>(new AfterBurnerClimaxApp(ctx, "after_burner_climax",
        PPCImageConfig));
  }

  // Default the game data folder to "assets" next to the exe, so the game
  // runs with a plain double-click. --game_data_root still overrides it.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    ApplyLogSwitch(paths.config_path);
    if (paths.game_data_root.empty()) {
      paths.game_data_root =
          rex::filesystem::GetExecutableFolder() / "assets";
      // First start: make "assets" from the player's own XBLA package next to the exe.
      if (!package_setup_first_run(rex::filesystem::GetExecutableFolder(),
                                   paths.game_data_root, 0x58410A06u,
                                   "After Burner Climax")) {
        std::exit(1);
      }
    }
    // Only the exact executable this build was recompiled from can run: the
    // World release (retail XBLA package), default.xex 0.0.1.2.
    if (!xex_version_check(paths.game_data_root, "a066b2d60eefb2d5982c07e862d5a9ea8b626eca", 2293760u,
                           "After Burner Climax")) {
      std::exit(1);
    }
    config_path_ = paths.config_path;
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
#ifdef ABC_BUILD
    REXLOG_INFO("After Burner Climax Recharged, build {}", ABC_BUILD);
#endif
    ApplyRenderer();
    // GPU emulation is opt-in in ReXGlue (gpu_plugin cvar). Default to the
    // Xenos plugin so the game actually renders; --gpu_plugin still overrides.
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
    // XBLA titles run as the trial ("locked") unless XamContentGetLicenseMask
    // reports a purchase; bit 0 = full game. ReXGlue's license_mask cvar
    // defaults to 0 (trial), so unlock unless the user set it explicitly.
    if (rex::cvar::Query<uint32_t>("license_mask") == 0) {
      rex::cvar::SetFlagByName("license_mask", "1");
      REXLOG_INFO("License: full game (license_mask=1)");
    }
    // First launch: windowed, keyboard on, flight key layout. After this
    // everything comes from after_burner_climax.toml (edited by the F1 menu).
    if (!rex::cvar::Query<bool>("recomp_defaults_applied")) {
      rex::cvar::SetFlagByName("fullscreen", "false");
      rex::cvar::SetFlagByName("window_width", "1280");
      rex::cvar::SetFlagByName("window_height", "720");
      rex::cvar::SetFlagByName("mnk_mode", "true");
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
      rex::cvar::SetFlagByName("audio_maxqframes",
                               std::to_string(recomp_ui::game_keys::kDefaultAudioFrames));
      rex::cvar::SetFlagByName("recomp_defaults_version", "2");
      if (!config_path_.empty()) recomp_ui::save_config(config_path_);
    }
    // Version 3: the config file lists every menu setting (config_file.h); rewrite it once.
    if (rex::cvar::Query<int32_t>("recomp_defaults_version") < 3) {
      rex::cvar::SetFlagByName("recomp_defaults_version", "3");
      recomp_ui::save_config(config_path_);
    }
    REXLOG_INFO("Audio buffer: {} frames", rex::cvar::Query<int32_t>("audio_maxqframes"));
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    (void)drawer;
    rex::ui::RegisterBind("bind_menu", "F1", "Open/close the settings menu",
                          [this] { ToggleMenu(); });
    rex::ui::RegisterBind("bind_menu_alt", "Escape", "Open/close the settings menu",
                          [this] { ToggleMenu(); });
    rex::ui::RegisterBind("bind_fullscreen", "F11", "Toggle fullscreen", [this] {
      if (menu_ && menu_->capturing()) return;
      bool fs = !window()->IsFullscreen();
      rex::cvar::SetFlagByName("fullscreen", fs ? "true" : "false");
      window()->SetFullscreen(fs);
      if (!config_path_.empty()) recomp_ui::save_config(config_path_);
    });
  }

  // Window X / Alt+F4 / menu Quit all come through here before the SDK's
  // TerminateTitle + _Exit; the watchdog ends the process if that stalls.
  bool OnWindowCloseRequested() override {
    exit_watchdog::arm("window close requested");
    return true;
  }

  // The game must not see keys/buttons while the menu is open.
  void OnPostSetup() override {
#ifdef ABC_NATIVE_X_RENDERER
    // Always initialized: with native_x_renderer off it only arms what --native_x_renderer_trace / _verify ask for.
    native_x_renderer::Initialize(after_burner_climax::NativeXRendererProfile(),
                                  runtime() ? runtime()->graphics_system() : nullptr, window(),
                                  runtime() ? runtime()->cache_root() : std::filesystem::path());
    REXLOG_INFO("Renderer in use: {}", native_x_renderer::Renders() ? "NativeX" : "Xenia");
#endif
    auto* input_sys = static_cast<rex::input::InputSystem*>(runtime()->input_system());
    if (input_sys && imgui_drawer()) {
      input_sys->SetActiveCallback([this]() {
        if (menu_) return false;
        return !imgui_drawer()->GetIO().WantCaptureMouse;
      });
    }
  }

 private:
  // recomp_renderer (F1 > Renderer): "xenia" = the SDK's Xenos plugin draws
  // (default); "nativex" = NativeXRenderer draws, the Xenos plugin only runs the
  // command processor it observes.
  static void ApplyRenderer() {
    const std::string r = rex::cvar::Query<std::string>("recomp_renderer");
    if (r != "xenia" && r != "nativex") {
      REXLOG_WARN("recomp_renderer \"{}\" unknown; using xenia", r);
    }
#ifdef ABC_NATIVE_X_RENDERER
    // NativeX draws; the Xenos plugin's host rendering goes off (it still runs the command processor, and draws
    // only if NativeXRenderer cannot start: logged). The SDK's shader storage is for the Xenos path.
    const bool nx = r == "nativex";
    rex::cvar::SetFlagByName("native_x_renderer", nx ? "true" : "false");
    if (nx) rex::cvar::SetFlagByName("store_shaders", "false");
    REXLOG_INFO("Renderer selected: {}", nx ? "NativeX (Vulkan)" : "Xenia (Xenos plugin)");
#else
    REXLOG_INFO("Renderer: Xenia (Xenos plugin)");
#endif
  }

  // recomp_log (off by default; F1 menu, after_burner_climax.toml, --recomp_log=true).
  // The SDK sets logging up right after this callback, from the config file, and
  // always opens a file: with logging off every category is "off" and the file goes
  // to the temp folder instead of a new logs\after_burner_climax_NNN.log per launch.
  // (From mad-riders-recomp.)
  static void ApplyLogSwitch(const std::filesystem::path& config_path) {
    if (std::filesystem::exists(config_path)) rex::cvar::LoadConfig(config_path);
    if (rex::cvar::Query<bool>("recomp_log") || !rex::cvar::Query<std::string>("log_file").empty()) return;
    std::error_code ec;
    auto temp = std::filesystem::temp_directory_path(ec);
    if (ec) return;
    rex::cvar::SetFlagByName("log_level", "off");
    rex::cvar::SetFlagByName("log_file", (temp / "after_burner_climax_log_off.log").string());
    recomp_ui::g_log_forced_off = true;
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
    host.title = "AFTER BURNER CLIMAX";
    host.config_path = config_path_;
    host.set_fullscreen = [this](bool fs) {
      app_context().CallInUIThreadDeferred([this, fs] { window()->SetFullscreen(fs); });
    };
    host.quit = [this] {
      exit_watchdog::arm("menu Quit");
      app_context().CallInUIThreadDeferred([this] { window()->RequestClose(); });
    };
    host.on_closed = [this] { menu_ = nullptr; };
    menu_ = new recomp_ui::GameMenu(imgui_drawer(), std::move(host));
  }

  std::filesystem::path config_path_;
  recomp_ui::GameMenu* menu_ = nullptr;  // owned by the ImGui drawer
};

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
#include "crash_handler.h"
#include "exit_watchdog.h"
#include "frame_rate.h"
#include "game_pause.h"
#include "perf_overlay.h"
#include "game_keys.h"
#include "game_menu.h"
#include "package_setup.h"

#include <native_x_renderer/native_x_renderer.h>
#include <native_x_renderer/shader_cache_screen.h>
#include <memory>
#include <rex/system/xthread.h>
#include "shader_scan.h"
namespace after_burner_climax {
const native_x_renderer::Profile& NativeXRendererProfile();
}

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
    crash_handler::install(rex::filesystem::GetExecutableFolder());
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
    game_data_root_ = paths.game_data_root;
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
#ifdef ABC_BUILD
    REXLOG_INFO("After Burner Climax Recomp (Recharged), build {}", ABC_BUILD);
#endif
    ApplyRenderer();
    frame_rate::apply();
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
    // Version 4: with NativeX, draws wait up to 8 ms per frame for a shader compiled for the first time
    // (F1 > New shaders; as Mad Riders).
    if (rex::cvar::Query<int32_t>("recomp_defaults_version") < 4) {
      rex::cvar::SetFlagByName("native_x_renderer_scene_wait_ms", "8.0");
      rex::cvar::SetFlagByName("recomp_defaults_version", "4");
      if (!config_path_.empty()) recomp_ui::save_config(config_path_);
    }
    REXLOG_INFO("Audio buffer: {} frames", rex::cvar::Query<int32_t>("audio_maxqframes"));
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    perf_overlay_ = std::make_unique<recomp_ui::PerfOverlay>(drawer);
    // The shader cache's status while it is prepared (draws nothing once ready).
    native_x_renderer::ShaderCacheScreen::Style style;
    style.title = "AFTER BURNER CLIMAX";
    // The F1 menu's blue theme (game_menu.cpp: kPanelTop/kPanelBottom, kBorder, kAccent, kWhite, kGrey, kGold).
    // ImU32 is ABGR: 0xAABBGGRR.
    style.backdrop = 0xFF180802;      // (2, 8, 24)
    style.panel_top = 0xF256240C;     // (12, 36, 86, 242)
    style.panel_bottom = 0xF21E0A03;  // (3, 10, 30, 242)
    style.edge = 0xBEFF9646;          // (70, 150, 255, 190)
    style.accent = 0xFFFFC85A;        // (90, 200, 255)
    style.bar_left = 0xFFEE7A2C;      // (44, 122, 238)
    style.bar_right = 0xFFFFC85A;     // (90, 200, 255)
    style.track = 0xE1341406;         // (6, 20, 52, 225)
    style.text = 0xFFFFFFFF;
    style.dim = 0xFFD2AC96;           // (150, 172, 210)
    style.done = 0xFF5AD2FF;          // gold (255, 210, 90)
    shader_cache_screen_ = std::make_unique<native_x_renderer::ShaderCacheScreen>(drawer, style);
    // The gamepad's Select (Back) opens and closes the settings menu, like F1 (owner's request).
    // Deferred: the watcher runs while the dialogs are drawn, and the toggle adds or removes the menu's dialog.
    pad_watcher_ = std::make_unique<PadWatcher>(drawer, [this] {
      // Only opens: while the menu is open, it reads the pad itself and closes on Select (game_menu.cpp).
      app_context().CallInUIThreadDeferred([this] {
        if (!menu_) ToggleMenu();
      });
    }, [this] { return ReadPadForUI(); });
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
    game_pause::set(false);
    exit_watchdog::arm("window close requested");
    return true;
  }

  // The game must not see keys/buttons while the menu is open.
  void OnPostSetup() override {
    // Again: SDL or a driver may have replaced the filter during set-up.
    crash_handler::install(rex::filesystem::GetExecutableFolder());
    SetUpShaderCache();
    native_x_renderer::Initialize(after_burner_climax::NativeXRendererProfile(),
                                  runtime() ? runtime()->graphics_system() : nullptr, window(),
                                  runtime() ? runtime()->cache_root() : std::filesystem::path());
    REXLOG_INFO("Renderer in use: {}", native_x_renderer::Renders() ? "NativeX" : "Xenia");
    auto* input_sys = static_cast<rex::input::InputSystem*>(runtime()->input_system());
    if (input_sys && imgui_drawer()) {
      input_sys->SetActiveCallback([this]() {
        if (menu_) return true;  // the menu reads the pad; the game is blocked (BlockGameInput)
        return !imgui_drawer()->GetIO().WantCaptureMouse;
      });
    }
  }

  // With NativeX, the shader cache is prepared before the game's main thread runs at all (held here, resumed
  // when done), so its timers, movies and audio never see the wait; the status screen shows meanwhile.
  void OnPostLaunchModule(rex::system::XThread* thread) override {
    if (!thread || !native_x_renderer::Renders()) return;
    thread->Suspend();
    native_x_renderer::PrepareShaderCacheBeforeLaunch([thread] { thread->Resume(); });
  }

  void OnShutdown() override {
    shader_cache_screen_.reset();
    perf_overlay_.reset();
  }

 private:
  static void ApplyRenderer() {
    // NativeXRenderer draws. The SDK's Xenos plugin still runs the command processor NativeX observes, and draws
    // only if NativeX cannot start on this GPU (logged); its shader storage is for its own drawing: off.
    rex::cvar::SetFlagByName("native_x_renderer", "true");
    rex::cvar::SetFlagByName("store_shaders", "false");
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

  // Controller 1's buttons for the UI, read even while the game sees a neutral pad (the menu, the Select toggle).
  uint16_t ReadPadForUI() {
      auto* input = runtime() ? static_cast<rex::input::InputSystem*>(runtime()->input_system()) : nullptr;
      if (!input) return 0;
      rex::input::X_INPUT_STATE state{};
      if (input->GetStateForUI(0, &state) != 0) return 0;
      uint16_t buttons = uint16_t(state.gamepad.buttons);
      // The left stick acts as the D-pad (owner's request: up/down move, left/right change a setting). Hysteresis:
      // pressed past half the throw, released under 30 %, so a resting stick does not chatter.
      static bool stick_up = false, stick_down = false, stick_left = false, stick_right = false;
      const int lx = int16_t(state.gamepad.thumb_lx), ly = int16_t(state.gamepad.thumb_ly);  // up/right positive
      stick_up = stick_up ? ly > 9830 : ly > 16384;
      stick_down = stick_down ? ly < -9830 : ly < -16384;
      stick_right = stick_right ? lx > 9830 : lx > 16384;
      stick_left = stick_left ? lx < -9830 : lx < -16384;
      if (stick_up) buttons |= 0x0001;     // X_INPUT_GAMEPAD_DPAD_UP
      if (stick_down) buttons |= 0x0002;   // X_INPUT_GAMEPAD_DPAD_DOWN
      if (stick_left) buttons |= 0x0004;   // X_INPUT_GAMEPAD_DPAD_LEFT
      if (stick_right) buttons |= 0x0008;  // X_INPUT_GAMEPAD_DPAD_RIGHT
      return buttons;
    }

  void ToggleMenu() {
    if (menu_) {
      if (menu_->capturing()) return;  // Esc cancels the key capture instead
      delete menu_;                    // removes itself from the drawer
      menu_ = nullptr;
      BlockGameInput(false);
      game_pause::set(false);
      return;
    }
    if (!imgui_drawer()) return;
    recomp_ui::MenuHost host;
    host.title = "AFTER BURNER CLIMAX";
    host.config_path = config_path_;
    host.set_fullscreen = [this](bool fs) {
      // fullscreen_exclusive is already set: re-apply when only borderless <-> exclusive changed.
      app_context().CallInUIThreadDeferred([this, fs] {
        if (window()->IsFullscreen() == fs) {
          window()->RefreshFullscreen();
        } else {
          window()->SetFullscreen(fs);
        }
      });
    };
    // Controller 1's buttons for the menu (D-pad, LB/RB, A/B), read for the UI while the game sees a neutral pad.
    host.pad_buttons = [this]() -> uint16_t { return ReadPadForUI(); };
    host.quit = [this] {
      game_pause::set(false);  // the game's thread must not be held while the title terminates
      exit_watchdog::arm("menu Quit");
      app_context().CallInUIThreadDeferred([this] { window()->RequestClose(); });
    };
    host.on_closed = [this] {
      menu_ = nullptr;
      BlockGameInput(false);
      game_pause::set(false);
    };
    BlockGameInput(true);
    game_pause::set(true);
    menu_ = new recomp_ui::GameMenu(imgui_drawer(), std::move(host));
  }

  // While the menu is open the game reads a neutral pad, but the input drivers stay active so the menu can read
  // the controller (GetStateForUI). Buttons still held when it closes stay hidden from the game until released.
  void BlockGameInput(bool on) {
    if (on == input_blocked_) return;
    auto* input = runtime() ? static_cast<rex::input::InputSystem*>(runtime()->input_system()) : nullptr;
    if (!input) return;
    input_blocked_ = on;
    if (on) {
      input->AddUIInputBlocker();
    } else {
      input->RemoveUIInputBlocker();
    }
  }
  bool input_blocked_ = false;
  std::unique_ptr<recomp_ui::PerfOverlay> perf_overlay_;
  // Watches controller 1's Select (Back) every frame, on the UI thread: a press toggles the settings menu.
  class PadWatcher : public rex::ui::ImGuiDialog {
   public:
    PadWatcher(rex::ui::ImGuiDrawer* drawer, std::function<void()> toggle, std::function<uint16_t()> read)
        : ImGuiDialog(drawer), toggle_(std::move(toggle)), read_(std::move(read)) {}
   protected:
    void OnDraw(ImGuiIO&) override {
      constexpr uint16_t kBack = 0x0020;  // X_INPUT_GAMEPAD_BACK
      const uint16_t pad = read_();
      const bool pressed = (pad & kBack) && !(prev_ & kBack);
      prev_ = pad;
      if (pressed) toggle_();
    }
   private:
    std::function<void()> toggle_;
    std::function<uint16_t()> read_;
    uint16_t prev_ = 0;
  };
  std::unique_ptr<PadWatcher> pad_watcher_;

  std::filesystem::path config_path_;
  std::filesystem::path game_data_root_;
  std::unique_ptr<native_x_renderer::ShaderCacheScreen> shader_cache_screen_;

  // NativeX's shader cache: shaders.cache next to the exe, one compressed indexed file (NativeX D127, D129). Its
  // first build translates every shader the game ships (shader_scan.h: media\x360\shader_hlsl.farc) and the
  // pipelines are learned while playing; a changed shader archive (fingerprint) builds them again.
  void SetUpShaderCache() {
    native_x_renderer::ShaderCacheSource source;
    source.file = rex::filesystem::GetExecutableFolder() / "shaders.cache";
    {
      // The last run crashed (crash_handler.h): its cache may hold what crashed it; built again.
      std::error_code ec;
      auto bad = source.file;
      bad += ".bad";
      if (std::filesystem::exists(bad, ec)) {
        std::filesystem::remove(source.file, ec);
        std::filesystem::remove(bad, ec);
        REXLOG_WARN("the last run crashed: shaders.cache deleted, built again");
      }
    }
    const auto x360_dir = game_data_root_ / "media" / "x360";
    source.game_fingerprint = shader_scan::fingerprint(x360_dir);
    source.scan = [x360_dir](const std::function<void(uint32_t, uint32_t)>& progress) {
      shader_scan::Result found = shader_scan::scan(x360_dir, progress);
      REXLOG_INFO("Shader scan: {} shaders in {} archive entries", found.shaders.size(), found.entries);
      return std::move(found.shaders);
    };
    native_x_renderer::SetShaderCache(std::move(source));
  }
  recomp_ui::GameMenu* menu_ = nullptr;  // owned by the ImGui drawer
};

// In-game settings menu (shared across the darkuni recomps).
//
// A blue panel with five categories (Display, Graphics, Audio, Controls, System): Left/Right or LB/RB switch
// category, Up/Down or the D-pad pick a row, A / Enter / Space step a value forward, B / Backspace step it back
// (on an action row B leaves the menu); the mouse works too (left click forward, right click back). A note for
// the row at the bottom; settings saved on every change. Drawn on ReXGlue's ImGui overlay.
//
// Open/close: F1 or Esc.  Everything is stored in <app>.toml next to the
// exe via ReXGlue's cvar system, so it is also editable by hand.

#pragma once

#include <filesystem>
#include <cstdint>
#include <functional>
#include <string>

#include <rex/ui/imgui_dialog.h>

namespace recomp_ui {

struct MenuHost {
  std::string title;                         // shown as "<TITLE>  -  SETTINGS"
  std::filesystem::path config_path;
  std::function<void(bool)> set_fullscreen;  // live display-mode switch
  std::function<void()> quit;                // close the game
  std::function<uint16_t()> pad_buttons;     // controller 1 (X_INPUT_GAMEPAD_* bits), 0 if none
  std::function<void()> on_closed;           // menu closed itself (deleted)
};

class GameMenu : public rex::ui::ImGuiDialog {
 public:
  GameMenu(rex::ui::ImGuiDrawer* drawer, MenuHost host);
  ~GameMenu() override;

  // True while waiting for a key to bind; the app's F1/Esc binds stay quiet.
  bool capturing() const { return capturing_ >= 0; }

  // Default keyboard layout (written on first launch, and by "Reset keys").
  static void ApplyDefaultKeys();

 protected:
  void OnDraw(ImGuiIO& io) override;
  void OnClose() override;

 private:
  enum class Page { kMain, kKeys };

  void DrawMain(float scale);
  void DrawKeys(float scale);
  void Change(int row, int dir);
  void Save();

  MenuHost host_;
  Page page_ = Page::kMain;
  int tab_ = 0;  // category
  int sel_ = 0;  // row within the category
  uint16_t pad_prev_ = 0, pad_pressed_ = 0;  // controller 1 buttons last frame / newly pressed this frame
  int keys_sel_ = 0;
  int capturing_ = -1;
  bool quit_armed_ = false;
  std::string status_;
};

}  // namespace recomp_ui

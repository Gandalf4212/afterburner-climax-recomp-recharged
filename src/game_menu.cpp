// In-game settings menu - see game_menu.h. Per-game key layout: game_keys.h.

#include "game_menu.h"

#include "config_file.h"
#include "game_keys.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/logging.h>

// Set once the first-launch defaults (windowed, keyboard on, key layout) are written.
REXCVAR_DEFINE_BOOL(recomp_defaults_applied, false, "Recomp",
                    "First-launch defaults have been applied");
// Bumped when a newer build adds a default (2 = per-game audio buffer).
REXCVAR_DEFINE_INT32(recomp_defaults_version, 0, "Recomp", "Last applied defaults version");
// Renderer for the next launch: "xenia" (the SDK's emulated Xenos GPU, default) or
// "nativex" (NativeXRenderer, Vulkan). Applied at start-up by the app.
REXCVAR_DEFINE_STRING(recomp_renderer, "xenia", "Recomp", "Renderer: xenia (default) or nativex");
// Log file in logs\ (off by default; the app applies it at start-up).
REXCVAR_DEFINE_BOOL(recomp_log, false, "Recomp", "Write a log file in logs\\ (for bug reports)");

namespace recomp_ui {
namespace {

namespace cvar = rex::cvar;

// ------------------------------------------------------------------ colours (blue theme)
const ImU32 kWhite = IM_COL32(255, 255, 255, 255);
const ImU32 kGrey = IM_COL32(150, 172, 210, 255);
const ImU32 kDim = IM_COL32(95, 115, 155, 255);
const ImU32 kGold = IM_COL32(255, 210, 90, 255);
const ImU32 kAccent = IM_COL32(90, 200, 255, 255);
const ImU32 kShade = IM_COL32(1, 6, 20, 175);
const ImU32 kPanelTop = IM_COL32(12, 36, 86, 242);
const ImU32 kPanelBottom = IM_COL32(3, 10, 30, 242);
const ImU32 kBorder = IM_COL32(70, 150, 255, 190);
const ImU32 kTabOn = IM_COL32(38, 108, 222, 240);
const ImU32 kTabOff = IM_COL32(16, 40, 88, 210);
const ImU32 kRowLeft = IM_COL32(44, 122, 238, 225);
const ImU32 kRowRight = IM_COL32(18, 56, 135, 90);
const ImU32 kPill = IM_COL32(6, 20, 52, 225);

// ------------------------------------------------------------------ cvar helpers
bool GetBool(const char* n) { return cvar::Query<bool>(n); }
int GetInt(const char* n) { return cvar::Query<int32_t>(n); }
std::string GetStr(const char* n) { return cvar::Query<std::string>(n); }
void SetBool(const char* n, bool v) { cvar::SetFlagByName(n, v ? "true" : "false"); }
void SetInt(const char* n, int v) { cvar::SetFlagByName(n, std::to_string(v)); }
void SetStr(const char* n, const std::string& v) { cvar::SetFlagByName(n, v); }
double GetDouble(const char* n) { return cvar::Query<double>(n); }
// NativeXRenderer's cvars exist only in builds that link it.
bool Has(const char* n) {
  for (const auto& e : cvar::GetRegistry())
    if (e.name == n) return true;
  return false;
}
// The renderer chosen for this run and the next (recomp_renderer, applied at start-up).
bool NativeX() { return GetStr("recomp_renderer") == "nativex"; }
bool NativeXRow(const char* n) { return NativeX() && Has(n); }

// ------------------------------------------------------------------ main page
enum MainRow {
  kRenderer,
  kDisplayMode,
  kWindowSize,
  kRenderRes,
  kAntiAlias,
  kCompileWait,
  kScreenSync,
  kAspect,
  kFiltering,
  kFrameRate,
  kPerfOverlay,
  kAudioBuffer,
  kKeyboard,
  kLogFile,
  kEditKeys,
  kCloseMenu,
  kQuit,
  kMainRowCount
};

struct WindowSize {
  int w, h;
  const char* label;
};
const WindowSize kWindowSizes[] = {
    {1280, 720, "1280 x 720"},
    {1600, 900, "1600 x 900"},
    {1920, 1080, "1920 x 1080"},
    {2560, 1440, "2560 x 1440"},
};
constexpr int kWindowSizeCount = 4;

// anisotropic_override values and their labels
const int kAnisoValues[] = {-1, 0, 2, 3, 4, 5};
const char* const kAnisoLabels[] = {"Game default", "Off", "2x", "4x", "8x", "16x"};
constexpr int kAnisoCount = 6;

// audio_maxqframes values and their labels
const int kAudioFrames[] = {8, 16, 32, 64};
const char* const kAudioLabels[] = {"Small (8)", "Medium (16)", "Large (32)", "Max (64)"};
constexpr int kAudioCount = 4;

int AudioIndex() {
  int v = GetInt("audio_maxqframes"), best = 0;
  for (int i = 0; i < kAudioCount; i++) {
    if (kAudioFrames[i] <= v) best = i;
  }
  return best;
}

// Xenia (Xenos plugin): resolution_scale 1..3
const char* const kRenderLabels[] = {"1x  1280x720 (original)", "2x  2560x1440", "3x  3840x2160"};

// ---- NativeXRenderer settings (rows ported from mad-riders-recomp)
const char* const kNxrAaValues[] = {"off", "fxaa", "smaa"};
const char* const kNxrAaLabels[] = {"Off", "FXAA", "SMAA"};
constexpr int kNxrAaCount = 3;
int NxrAaIndex() {
  std::string v = GetStr("native_x_renderer_antialiasing");
  for (int i = 0; i < kNxrAaCount; i++) if (v == kNxrAaValues[i]) return i;
  return 2;  // smaa, NativeXRenderer's default
}

const double kNxrScales[] = {0.0, 1.0, 1.5, 2.0, 3.0};
const char* const kNxrScaleLabels[] = {"Auto (fit the screen)", "1x  1280x720", "1.5x  1920x1080",
                                       "2x  2560x1440", "3x  3840x2160"};
constexpr int kNxrScaleCount = 5;
int NxrScaleIndex() {
  double v = GetDouble("native_x_renderer_resolution_scale");
  for (int i = 0; i < kNxrScaleCount; i++) if (std::abs(kNxrScales[i] - v) < 0.01) return i;
  return 0;
}

// native_x_renderer_scene_wait_ms: how long a frame may wait for a shader compiled for the first time
const double kWaitValues[] = {0.0, 8.0, 33.0};
const char* const kWaitLabels[] = {"Pop in (no stutter)", "Short wait (recommended)", "Long wait (fewest pop-ins)"};
constexpr int kWaitCount = 3;
int WaitIndex() {
  double v = GetDouble("native_x_renderer_scene_wait_ms"), best = 0;
  int index = 0;
  for (int i = 0; i < kWaitCount; i++) {
    if (kWaitValues[i] <= v + 0.01 && kWaitValues[i] >= best) {
      best = kWaitValues[i];
      index = i;
    }
  }
  return index;
}

const char* const kSyncValues[] = {"", "vsync", "immediate"};
const char* const kSyncLabels[] = {"Fast, no tearing", "V-Sync", "Immediate (G-Sync/FreeSync)"};
constexpr int kSyncCount = 3;
int SyncIndex() {
  std::string v = GetStr("native_x_renderer_present_mode");
  for (int i = 0; i < kSyncCount; i++) if (v == kSyncValues[i]) return i;
  return 0;
}

const int kFrameRates[] = {60, 120};
const char* const kFrameRateLabels[] = {"60 FPS (original)", "120 FPS (experimental)"};
const char* const kPerfLabels[] = {"Off", "FPS", "FPS, CPU, GPU", "FPS, CPU, GPU, frame times"};

constexpr const char* kNativeXOnly = "(NativeX only)";

const char* MainLabel(int row) {
  switch (row) {
    case kRenderer: return "Renderer";
    case kDisplayMode: return "Display mode";
    case kWindowSize: return "Window size";
    case kRenderRes: return "Render resolution";
    case kAntiAlias: return "Anti-aliasing";
    case kCompileWait: return "New shaders";
    case kScreenSync: return NativeX() ? "Screen sync" : "VSync";
    case kAspect: return "Aspect ratio";
    case kFiltering: return "Texture filtering";
    case kFrameRate: return "Frame rate";
    case kPerfOverlay: return "Performance overlay";
    case kAudioBuffer: return "Audio buffer";
    case kKeyboard: return "Keyboard controls";
    case kLogFile: return "Log file";
    case kEditKeys: return "Edit keyboard keys";
    case kCloseMenu: return "Close menu";
    case kQuit: return "Quit game";
  }
  return "";
}

const char* MainNote(int row) {
  switch (row) {
    case kRenderer:
      return "Xenia: emulated GPU (default). NativeX: Vulkan, draws the game natively (experimental). Next launch.";
    case kDisplayMode:
      return "Borderless: quick Alt+Tab. Fullscreen: exclusive, switches the display mode. F11 toggles windowed.";
    case kWindowSize: return "Size of the window in windowed mode. Applies next launch.";
    case kRenderRes:
      return NativeX() ? "Above your screen's size renders bigger and scales down: smoother edges. Next launch."
                       : "Internal rendering resolution. Applies next launch.";
    case kAntiAlias:
      return "SMAA: sharp, smooth edges (recommended). FXAA: cheaper, softer. NativeX only. Next launch.";
    case kCompileWait:
      return "Shaders seen for the first time: wait a little, or skip the object until ready. NativeX only.";
    case kScreenSync:
      return NativeX() ? "V-Sync if you see tearing; Immediate for G-Sync/FreeSync screens."
                       : "Wait for the monitor refresh. Off can tear.";
    case kAspect: return "Keep 16:9 adds black bars on other window shapes.";
    case kFiltering: return "Sharper textures seen at an angle.";
    case kFrameRate: return "120 is experimental: if the game speeds up, go back to 60. Next launch.";
    case kPerfOverlay: return "Frame rate, CPU and GPU use, and a frame-time graph in the top-right corner.";
    case kAudioBuffer: return "Bigger fixes crackles and dropouts, smaller has less delay. Next launch.";
    case kKeyboard: return "Play with the keyboard as controller 1.";
    case kLogFile: return "Writes logs\\after_burner_climax_NNN.log for bug reports. Next launch.";
    case kEditKeys: return "Choose which key presses each controller button.";
  }
  return nullptr;
}

// Display mode: windowed, borderless fullscreen (at the desktop's mode), exclusive fullscreen.
const char* const kDisplayLabels[] = {"Windowed", "Borderless fullscreen", "Fullscreen"};
constexpr int kDisplayCount = 3;
int DisplayIndex() {
  if (!GetBool("fullscreen")) return 0;
  return GetBool("fullscreen_exclusive") ? 2 : 1;
}

int WindowSizeIndex() {
  int w = GetInt("window_width"), h = GetInt("window_height");
  for (int i = 0; i < kWindowSizeCount; i++) {
    if (kWindowSizes[i].w == w && kWindowSizes[i].h == h) return i;
  }
  return 0;  // 0 x 0 = SDK default, which is 1280 x 720
}

int AnisoIndex() {
  int v = GetInt("anisotropic_override");
  for (int i = 0; i < kAnisoCount; i++) {
    if (kAnisoValues[i] == v) return i;
  }
  return 0;
}

// Value text for an option row, or nullptr for an action row.
std::string MainValue(int row) {
  switch (row) {
    case kRenderer: return NativeX() ? "NativeX (Vulkan)" : "Xenia (emulated GPU)";
    case kDisplayMode: return kDisplayLabels[DisplayIndex()];
    case kWindowSize: return kWindowSizes[WindowSizeIndex()].label;
    case kRenderRes:
      if (NativeX()) return NativeXRow("native_x_renderer_resolution_scale") ? kNxrScaleLabels[NxrScaleIndex()] : "-";
      return kRenderLabels[std::clamp(GetInt("resolution_scale"), 1, 3) - 1];
    case kAntiAlias:
      return NativeXRow("native_x_renderer_antialiasing") ? kNxrAaLabels[NxrAaIndex()] : kNativeXOnly;
    case kCompileWait:
      return NativeXRow("native_x_renderer_scene_wait_ms") ? kWaitLabels[WaitIndex()] : kNativeXOnly;
    case kScreenSync:
      if (NativeX()) return NativeXRow("native_x_renderer_present_mode") ? kSyncLabels[SyncIndex()] : "-";
      return GetBool("vsync") ? "On" : "Off";
    case kAspect: return GetBool("present_letterbox") ? "Keep 16:9" : "Stretch to window";
    case kFiltering: return kAnisoLabels[AnisoIndex()];
    case kFrameRate: return kFrameRateLabels[GetInt("recomp_frame_rate") == 120 ? 1 : 0];
    case kPerfOverlay: return kPerfLabels[std::clamp(GetInt("recomp_perf_overlay"), 0, 3)];
    case kAudioBuffer: return kAudioLabels[AudioIndex()];
    case kKeyboard: return GetBool("mnk_mode") ? "On" : "Off";
    case kLogFile: return GetBool("recomp_log") ? "On" : "Off";
  }
  return {};
}

bool IsOptionRow(int row) { return row <= kLogFile; }

// ------------------------------------------------------------------ keys page
using game_keys::KeyBind;
using game_keys::kKeyBinds;
constexpr int kKeyBindCount = sizeof(kKeyBinds) / sizeof(kKeyBinds[0]);
// Rows after the binds.
constexpr int kKeysReset = kKeyBindCount;
constexpr int kKeysBack = kKeyBindCount + 1;
constexpr int kKeysRowCount = kKeyBindCount + 2;

// Bindable keys: ReXGlue key name <-> ImGui key. F-keys, Esc and the backtick
// are left out because the menu and the SDK overlays use them.
struct KeyName {
  ImGuiKey key;
  const char* name;
};
const std::vector<KeyName>& BindableKeys() {
  static const std::vector<KeyName> keys = [] {
    std::vector<KeyName> v;
    static const char* const kLetters[] = {"A", "B", "C", "D", "E", "F", "G", "H", "I",
                                           "J", "K", "L", "M", "N", "O", "P", "Q", "R",
                                           "S", "T", "U", "V", "W", "X", "Y", "Z"};
    for (int i = 0; i < 26; i++) v.push_back({ImGuiKey(ImGuiKey_A + i), kLetters[i]});
    static const char* const kDigits[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    for (int i = 0; i < 10; i++) v.push_back({ImGuiKey(ImGuiKey_0 + i), kDigits[i]});
    v.insert(v.end(), {
                          {ImGuiKey_UpArrow, "Up"},
                          {ImGuiKey_DownArrow, "Down"},
                          {ImGuiKey_LeftArrow, "Left"},
                          {ImGuiKey_RightArrow, "Right"},
                          {ImGuiKey_Enter, "Return"},
                          {ImGuiKey_Space, "Space"},
                          {ImGuiKey_Tab, "Tab"},
                          {ImGuiKey_Backspace, "Backspace"},
                          {ImGuiKey_LeftShift, "Shift"},
                          {ImGuiKey_LeftCtrl, "Control"},
                          {ImGuiKey_LeftAlt, "Alt"},
                          {ImGuiKey_Insert, "Insert"},
                          {ImGuiKey_Delete, "Delete"},
                          {ImGuiKey_Home, "Home"},
                          {ImGuiKey_End, "End"},
                          {ImGuiKey_PageUp, "PageUp"},
                          {ImGuiKey_PageDown, "PageDown"},
                          {ImGuiKey_Minus, "Minus"},
                          {ImGuiKey_Equal, "Plus"},
                          {ImGuiKey_LeftBracket, "LBracket"},
                          {ImGuiKey_RightBracket, "RBracket"},
                      });
    return v;
  }();
  return keys;
}

std::string KeyDisplay(const std::string& name) {
  if (name.empty()) return "(none)";
  if (name == "Return") return "Enter";
  return name;
}

// ------------------------------------------------------------------ categories
struct Tab {
  const char* name;
  std::vector<int> rows;
};
const std::vector<Tab>& Tabs() {
  static const std::vector<Tab> tabs = {
      {"DISPLAY", {kDisplayMode, kWindowSize, kRenderRes, kAspect, kScreenSync, kFrameRate}},
      {"GRAPHICS", {kRenderer, kAntiAlias, kFiltering, kCompileWait}},
      {"AUDIO", {kAudioBuffer}},
      {"CONTROLS", {kKeyboard, kEditKeys}},
      {"SYSTEM", {kPerfOverlay, kLogFile, kCloseMenu, kQuit}},
  };
  return tabs;
}
constexpr int kMaxTabRows = 6;

// ------------------------------------------------------------------ gamepad (controller 1)
constexpr uint16_t kPadUp = 0x0001, kPadDown = 0x0002, kPadLeft = 0x0004, kPadRight = 0x0008;
constexpr uint16_t kPadLB = 0x0100, kPadRB = 0x0200, kPadA = 0x1000, kPadB = 0x2000;

// ------------------------------------------------------------------ drawing helpers
struct Painter {
  ImDrawList* dl;
  ImFont* font;
  float size;

  float TextWidth(const char* s, float scale = 1.0f) const {
    return font->CalcTextSizeA(size * scale, FLT_MAX, 0.0f, s).x;
  }
  void Text(float x, float y, const char* s, ImU32 col, float scale = 1.0f) const {
    dl->AddText(font, size * scale, ImVec2(x, y), col, s);
  }
  void TextWrapped(float x, float y, float w, const char* s, ImU32 col, float scale = 1.0f) const {
    dl->AddText(font, size * scale, ImVec2(x, y), col, s, nullptr, w);
  }
  void TextCentered(float cx, float y, const char* s, ImU32 col, float scale = 1.0f) const {
    Text(cx - TextWidth(s, scale) / 2, y, s, col, scale);
  }
  void Rect(float x, float y, float w, float h, ImU32 col, float rounding = 0.0f) const {
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), col, rounding);
  }
  void Frame(float x, float y, float w, float h, ImU32 col, float rounding, float thickness) const {
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), col, rounding, 0, thickness);
  }
  void Gradient(float x, float y, float w, float h, ImU32 top_left, ImU32 top_right, ImU32 bottom_right,
                ImU32 bottom_left) const {
    dl->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h), top_left, top_right, bottom_right,
                                bottom_left);
  }
};

bool Hovered(float x, float y, float w, float h) {
  return ImGui::IsMouseHoveringRect(ImVec2(x, y), ImVec2(x + w, y + h));
}
bool RowClicked(float x, float y, float w, float h, int mouse_button) {
  return Hovered(x, y, w, h) && ImGui::IsMouseClicked(mouse_button);
}

// The panel both pages share: shade, gradient panel with a glowing border, the title and an accent line.
// Returns the y below the header.
float DrawPanel(const Painter& p, float px, float py, float pw, float ph, float scale, const std::string& title,
                const char* subtitle) {
  ImGuiIO& io = ImGui::GetIO();
  p.Rect(0, 0, io.DisplaySize.x, io.DisplaySize.y, kShade);
  const float r = 14 * scale;
  p.Rect(px + 6 * scale, py + 8 * scale, pw, ph, IM_COL32(0, 0, 0, 90), r);  // drop shadow
  p.dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + ph), kPanelBottom, r);
  p.Gradient(px + r * 0.3f, py + r * 0.3f, pw - r * 0.6f, ph * 0.45f, kPanelTop, kPanelTop,
             IM_COL32(3, 10, 30, 0), IM_COL32(3, 10, 30, 0));
  p.Frame(px, py, pw, ph, IM_COL32(70, 150, 255, 60), r, 6 * scale);
  p.Frame(px, py, pw, ph, kBorder, r, 1.5f * scale);
  float x = px + 28 * scale, y = py + 20 * scale;
  p.Text(x, y, title.c_str(), kWhite, 1.25f);
  float tw = p.TextWidth(title.c_str(), 1.25f);
  p.Text(x + tw + 14 * scale, y + p.size * 0.38f, subtitle, kAccent, 0.8f);
  y += p.size * 1.25f + 10 * scale;
  p.Gradient(x, y, pw - 56 * scale, 2 * scale, kAccent, IM_COL32(90, 200, 255, 0), IM_COL32(90, 200, 255, 0),
             kAccent);
  return y + 14 * scale;
}

// A value as a pill with arrows: "<  value  >".
void DrawValue(const Painter& p, float x, float y, float w, float lh, const std::string& value, bool selected,
               bool inactive) {
  const float pad = 6.0f;
  p.Rect(x, y - pad * 0.5f, w, lh - pad, kPill, (lh - pad) / 2);
  if (selected && !inactive) p.Frame(x, y - pad * 0.5f, w, lh - pad, kAccent, (lh - pad) / 2, 1.5f);
  float ty = y + (lh - pad - p.size * 0.9f) / 2 - pad * 0.5f;
  p.TextCentered(x + w / 2, ty, value.c_str(), inactive ? kDim : kWhite, 0.9f);
  if (!inactive) {
    ImU32 arrow = selected ? kAccent : kDim;
    p.Text(x + 12, ty, "<", arrow, 0.9f);
    p.Text(x + w - 12 - p.TextWidth(">", 0.9f), ty, ">", arrow, 0.9f);
  }
}

}  // namespace

// ================================================================== GameMenu

GameMenu::GameMenu(rex::ui::ImGuiDrawer* drawer, MenuHost host)
    : ImGuiDialog(drawer), host_(std::move(host)) {
  // Buttons already held when the menu opens do not act.
  pad_prev_ = host_.pad_buttons ? host_.pad_buttons() : 0;
}

GameMenu::~GameMenu() = default;

void GameMenu::OnClose() {
  if (host_.on_closed) host_.on_closed();
}

void GameMenu::ApplyDefaultKeys() {
  for (const auto& b : kKeyBinds) SetStr(b.cvar, b.default_key);
  // Anything the game layout doesn't use is unbound so the SDK's FPS-style
  // defaults (WASD stick, mouse buttons) can't collide with it.
  for (const char* n : game_keys::kUnbound) SetStr(n, "");
}

void GameMenu::Save() {
  if (!host_.config_path.empty()) {
    recomp_ui::save_config(host_.config_path);
  }
}

void GameMenu::Change(int row, int dir) {
  switch (row) {
    case kRenderer:
      SetStr("recomp_renderer", NativeX() ? "xenia" : "nativex");
      status_ = "Renderer changes on the next launch.";
      break;
    case kDisplayMode: {
      int i = (DisplayIndex() + dir + kDisplayCount) % kDisplayCount;
      SetBool("fullscreen_exclusive", i == 2);
      SetBool("fullscreen", i != 0);
      if (host_.set_fullscreen) host_.set_fullscreen(i != 0);
      break;
    }
    case kWindowSize: {
      int i = (WindowSizeIndex() + dir + kWindowSizeCount) % kWindowSizeCount;
      SetInt("window_width", kWindowSizes[i].w);
      SetInt("window_height", kWindowSizes[i].h);
      status_ = "Window size changes on the next launch.";
      break;
    }
    case kRenderRes: {
      if (NativeX()) {
        if (!Has("native_x_renderer_resolution_scale")) return;
        int i = (NxrScaleIndex() + dir + kNxrScaleCount) % kNxrScaleCount;
        char buf[16];
        std::snprintf(buf, sizeof buf, "%.1f", kNxrScales[i]);
        SetStr("native_x_renderer_resolution_scale", buf);
        status_ = "Render resolution changes on the next launch.";
        break;
      }
      int v = std::clamp(GetInt("resolution_scale"), 1, 3) - 1;
      v = (v + dir + 3) % 3;
      SetInt("resolution_scale", v + 1);
      status_ = "Render resolution changes on the next launch.";
      break;
    }
    case kAntiAlias: {
      if (!NativeXRow("native_x_renderer_antialiasing")) return;
      int i = (NxrAaIndex() + dir + kNxrAaCount) % kNxrAaCount;
      SetStr("native_x_renderer_antialiasing", kNxrAaValues[i]);
      status_ = "Anti-aliasing changes on the next launch.";
      break;
    }
    case kCompileWait: {
      if (!NativeXRow("native_x_renderer_scene_wait_ms")) return;
      int i = (WaitIndex() + dir + kWaitCount) % kWaitCount;
      char buf[16];
      std::snprintf(buf, sizeof buf, "%.1f", kWaitValues[i]);
      SetStr("native_x_renderer_scene_wait_ms", buf);
      status_ = "New shaders changes on the next launch.";
      break;
    }
    case kScreenSync:
      if (NativeX()) {
        if (!Has("native_x_renderer_present_mode")) return;
        int i = (SyncIndex() + dir + kSyncCount) % kSyncCount;
        SetStr("native_x_renderer_present_mode", kSyncValues[i]);
        break;
      }
      SetBool("vsync", !GetBool("vsync"));
      break;
    case kAspect: SetBool("present_letterbox", !GetBool("present_letterbox")); break;
    case kFiltering: {
      int i = (AnisoIndex() + dir + kAnisoCount) % kAnisoCount;
      SetInt("anisotropic_override", kAnisoValues[i]);
      break;
    }
    case kAudioBuffer: {
      int i = (AudioIndex() + dir + kAudioCount) % kAudioCount;
      SetInt("audio_maxqframes", kAudioFrames[i]);
      status_ = "Audio buffer changes on the next launch.";
      break;
    }
    case kKeyboard: SetBool("mnk_mode", !GetBool("mnk_mode")); break;
    case kFrameRate:
      SetInt("recomp_frame_rate", GetInt("recomp_frame_rate") == 120 ? 60 : 120);
      status_ = "Frame rate changes on the next launch.";
      break;
    case kPerfOverlay: SetInt("recomp_perf_overlay", (std::clamp(GetInt("recomp_perf_overlay"), 0, 3) + dir + 4) % 4); break;
    case kLogFile:
      SetBool("recomp_log", !GetBool("recomp_log"));
      status_ = "Logging changes on the next launch.";
      break;
    default: return;
  }
  Save();
}

void GameMenu::OnDraw(ImGuiIO& io) {
  const ImVec2 display = io.DisplaySize;
  if (display.x <= 0 || display.y <= 0) return;

  // Controller 1: buttons pressed since the last frame.
  uint16_t pad = host_.pad_buttons ? host_.pad_buttons() : 0;
  pad_pressed_ = uint16_t(pad & ~pad_prev_);
  pad_prev_ = pad;

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(display);
  ImGui::Begin("##recomp_settings_menu", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
  float scale = std::max(0.6f, display.y / 900.0f);
  if (page_ == Page::kMain) {
    DrawMain(scale);
  } else {
    DrawKeys(scale);
  }
  ImGui::End();
}

void GameMenu::DrawMain(float scale) {
  const auto& tabs = Tabs();
  const int tab_count = int(tabs.size());
  auto pressed = [&](uint16_t bit) { return (pad_pressed_ & bit) != 0; };

  // ---- category: LB/RB; keyboard Q/E, Tab (Shift+Tab back), PageUp/PageDown
  int tab_dir = 0;
  const bool shift = ImGui::GetIO().KeyShift;
  if (pressed(kPadLB) || ImGui::IsKeyPressed(ImGuiKey_Q, false) || ImGui::IsKeyPressed(ImGuiKey_PageUp) ||
      (shift && ImGui::IsKeyPressed(ImGuiKey_Tab))) {
    tab_dir = -1;
  }
  if (pressed(kPadRB) || ImGui::IsKeyPressed(ImGuiKey_E, false) || ImGui::IsKeyPressed(ImGuiKey_PageDown) ||
      (!shift && ImGui::IsKeyPressed(ImGuiKey_Tab))) {
    tab_dir = 1;
  }
  if (tab_dir) {
    tab_ = (tab_ + tab_dir + tab_count) % tab_count;
    sel_ = 0;
    quit_armed_ = false;
  }
  const std::vector<int>& rows = tabs[tab_].rows;
  const int n = int(rows.size());
  sel_ = std::clamp(sel_, 0, n - 1);

  // ---- row: Up/Down, D-pad
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || pressed(kPadUp)) { sel_ = (sel_ + n - 1) % n; quit_armed_ = false; }
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || pressed(kPadDown)) { sel_ = (sel_ + 1) % n; quit_armed_ = false; }
  // ---- value: Left/Right (D-pad) back/forward, A / Enter / Space forward; B / Backspace leaves the menu
  int activate = 0;
  if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || pressed(kPadLeft)) activate = -1;
  if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || pressed(kPadRight) || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
      ImGui::IsKeyPressed(ImGuiKey_Space) || pressed(kPadA)) {
    activate = 1;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) || pressed(kPadB)) {
    Close();
    return;
  }

  // ---- layout
  ImGuiIO& io = ImGui::GetIO();
  Painter p{ImGui::GetWindowDrawList(), ImGui::GetFont(), 24.0f * scale};
  const float em = p.TextWidth("M");
  const float lh = p.size * 1.7f;
  const float pw = std::min(io.DisplaySize.x - 40 * scale, em * 46.0f);
  const float tab_h = p.size * 1.6f;
  const float ph = 20 * scale + p.size * 1.25f + 24 * scale + tab_h + 18 * scale + kMaxTabRows * lh +
                   p.size * 2.6f + p.size * 1.6f + 16 * scale;
  const float px = (io.DisplaySize.x - pw) / 2, py = (io.DisplaySize.y - ph) / 2;
  const float inner_x = px + 28 * scale, inner_w = pw - 56 * scale;

  float y = DrawPanel(p, px, py, pw, ph, scale, host_.title, "SETTINGS");

  // ---- tab bar
  {
    const float hint_w = p.TextWidth("LB", 0.7f) + 16 * scale;
    p.Text(inner_x, y + (tab_h - p.size * 0.7f) / 2, "LB", kDim, 0.7f);
    p.Text(inner_x + inner_w - p.TextWidth("RB", 0.7f), y + (tab_h - p.size * 0.7f) / 2, "RB", kDim, 0.7f);
    const float bar_x = inner_x + hint_w, bar_w = inner_w - 2 * hint_w;
    const float gap = 6 * scale, w = (bar_w - gap * (tab_count - 1)) / tab_count;
    for (int t = 0; t < tab_count; t++) {
      float tx = bar_x + t * (w + gap);
      if (RowClicked(tx, y, w, tab_h, ImGuiMouseButton_Left) && t != tab_) {
        tab_ = t;
        sel_ = 0;
        quit_armed_ = false;
      }
      const bool on = t == tab_;
      p.Rect(tx, y, w, tab_h, on ? kTabOn : kTabOff, 8 * scale);
      if (on) p.Rect(tx + 10 * scale, y + tab_h - 3 * scale, w - 20 * scale, 3 * scale, kAccent, 2 * scale);
      else if (Hovered(tx, y, w, tab_h)) p.Frame(tx, y, w, tab_h, kBorder, 8 * scale, 1.0f);
      p.TextCentered(tx + w / 2, y + (tab_h - p.size * 0.8f) / 2, tabs[t].name, on ? kWhite : kGrey, 0.8f);
    }
    y += tab_h + 18 * scale;
  }

  // ---- rows
  const std::vector<int>& cur = tabs[tab_].rows;
  const float value_w = inner_w * 0.5f, value_x = inner_x + inner_w - value_w - 8 * scale;
  for (int i = 0; i < int(cur.size()); i++) {
    const int row = cur[i];
    const float ry = y + i * lh;
    // mouse: hover selects, left click = next, right click = previous
    if (Hovered(inner_x, ry, inner_w, lh - 4 * scale) && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) {
      if (sel_ != i) quit_armed_ = false;
      sel_ = i;
    }
    if (RowClicked(inner_x, ry, inner_w, lh - 4 * scale, ImGuiMouseButton_Left)) { sel_ = i; activate = 1; }
    if (RowClicked(inner_x, ry, inner_w, lh - 4 * scale, ImGuiMouseButton_Right)) { sel_ = i; activate = -1; }

    const bool selected = i == sel_;
    if (selected) {
      p.dl->AddRectFilledMultiColor(ImVec2(inner_x, ry), ImVec2(inner_x + inner_w, ry + lh - 4 * scale), kRowLeft,
                                    kRowRight, kRowRight, kRowLeft);
      p.Rect(inner_x, ry, 4 * scale, lh - 4 * scale, kAccent);
    }
    const float ty = ry + (lh - 4 * scale - p.size) / 2;
    p.Text(inner_x + 18 * scale, ty, MainLabel(row), selected ? kWhite : kGrey);
    if (IsOptionRow(row)) {
      std::string value = MainValue(row);
      const bool inactive = value == kNativeXOnly || value == "-";
      DrawValue(p, value_x, ry + 3 * scale, value_w, lh - 4 * scale, value, selected, inactive);
    } else if (row == kEditKeys) {
      p.Text(value_x + value_w - p.TextWidth(">"), ty, ">", selected ? kAccent : kDim);
    } else if (row == kQuit && selected && quit_armed_) {
      p.Text(value_x, ty, "press A / Enter again to quit", kGold, 0.85f);
    }
  }
  y += kMaxTabRows * lh;

  // ---- act on the selection
  if (activate != 0) {
    const int row = cur[sel_];
    if (IsOptionRow(row)) {
      Change(row, activate);
    } else if (activate > 0) {
      switch (row) {
        case kEditKeys:
          page_ = Page::kKeys;
          keys_sel_ = 0;
          break;
        case kCloseMenu:
          Close();
          return;
        case kQuit:
          if (quit_armed_) {
            if (host_.quit) host_.quit();
          } else {
            quit_armed_ = true;
          }
          break;
      }
    }
  }

  // ---- note and button hints
  p.Gradient(inner_x, y, inner_w, 1 * scale, IM_COL32(70, 150, 255, 0), kBorder, kBorder, IM_COL32(70, 150, 255, 0));
  const char* note = MainNote(cur[sel_]);
  std::string bottom = !status_.empty() ? status_ : note ? note : "Saved automatically.";
  p.TextWrapped(inner_x, y + 10 * scale, inner_w, bottom.c_str(), status_.empty() ? kGrey : kGold, 0.75f);
  p.Text(inner_x, y + p.size * 2.6f,
         "Left Right / A: change    LB RB / Q E: category    B / Backspace / F1 / Esc: close", kDim,
         0.68f);
}

void GameMenu::DrawKeys(float scale) {
  int n = kKeysRowCount;
  auto pressed = [&](uint16_t bit) { return (pad_pressed_ & bit) != 0; };

  // ---- waiting for a key?
  if (capturing_ >= 0) {
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || pressed(kPadB)) {
      capturing_ = -1;
    } else {
      for (const auto& k : BindableKeys()) {
        if (ImGui::IsKeyPressed(k.key, false)) {
          SetStr(kKeyBinds[capturing_].cvar, k.name);
          Save();
          capturing_ = -1;
          break;
        }
      }
    }
  } else {
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || pressed(kPadUp)) keys_sel_ = (keys_sel_ + n - 1) % n;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || pressed(kPadDown)) keys_sel_ = (keys_sel_ + 1) % n;
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) ||
        pressed(kPadB)) {
      page_ = Page::kMain;
      return;
    }
  }
  bool activate = capturing_ < 0 && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                     ImGui::IsKeyPressed(ImGuiKey_Space, false) || pressed(kPadA));

  ImGuiIO& io = ImGui::GetIO();
  Painter p{ImGui::GetWindowDrawList(), ImGui::GetFont(), 22.0f * scale};
  const float lh = p.size * 1.45f;
  const float pw = std::min(io.DisplaySize.x - 40 * scale, p.TextWidth("M") * 44.0f);
  const float ph = 20 * scale + p.size * 1.25f + 24 * scale + n * lh + p.size * 3.2f;
  const float px = (io.DisplaySize.x - pw) / 2, py = (io.DisplaySize.y - ph) / 2;
  const float inner_x = px + 28 * scale, inner_w = pw - 56 * scale;
  const float value_x = inner_x + inner_w * 0.55f;

  float y = DrawPanel(p, px, py, pw, ph, scale, "KEYBOARD KEYS", "CONTROLS");

  for (int i = 0; i < n; i++) {
    const float ry = y + i * lh;
    if (capturing_ < 0) {
      if (Hovered(inner_x, ry, inner_w, lh - 3 * scale) && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) {
        keys_sel_ = i;
      }
      if (RowClicked(inner_x, ry, inner_w, lh - 3 * scale, ImGuiMouseButton_Left)) {
        keys_sel_ = i;
        activate = true;
      }
    }
    const bool selected = i == keys_sel_;
    if (selected) {
      p.dl->AddRectFilledMultiColor(ImVec2(inner_x, ry), ImVec2(inner_x + inner_w, ry + lh - 3 * scale), kRowLeft,
                                    kRowRight, kRowRight, kRowLeft);
      p.Rect(inner_x, ry, 4 * scale, lh - 3 * scale, kAccent);
    }
    const float ty = ry + (lh - 3 * scale - p.size) / 2;
    const ImU32 col = selected ? kWhite : kGrey;
    if (i < kKeyBindCount) {
      p.Text(inner_x + 18 * scale, ty, kKeyBinds[i].label, col);
      if (i == capturing_) {
        p.Text(value_x, ty, "press a key...", kGold);
      } else {
        p.Text(value_x, ty, KeyDisplay(GetStr(kKeyBinds[i].cvar)).c_str(), kAccent);
      }
    } else if (i == kKeysReset) {
      p.Text(inner_x + 18 * scale, ty, "Reset to defaults", col);
    } else {
      p.Text(inner_x + 18 * scale, ty, "Back", col);
    }
  }
  y += n * lh;

  if (activate) {
    if (keys_sel_ < kKeyBindCount) {
      capturing_ = keys_sel_;
    } else if (keys_sel_ == kKeysReset) {
      ApplyDefaultKeys();
      Save();
    } else {
      page_ = Page::kMain;
    }
  }

  const char* hint = capturing_ >= 0 ? "Press the new key. Esc / B cancels."
                                     : "A / Enter: change key    B / Backspace: back";
  p.Text(inner_x, y + 8 * scale, hint, capturing_ >= 0 ? kGold : kDim, 0.75f);
  p.Text(inner_x, y + 8 * scale + p.size * 1.1f, "Keys only work when Keyboard controls is On.", kDim, 0.75f);
}

}  // namespace recomp_ui

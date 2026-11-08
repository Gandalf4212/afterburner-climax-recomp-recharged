// Performance overlay in the top-right corner (F1 > System > Performance overlay, cvar recomp_perf_overlay):
// "FPS: 60", or with CPU and GPU use ("FPS: 60   CPU: 12%   GPU: 30%"), or that and a frame-time graph.
// Frames are counted at the game's own Swap (sub_82252730, which calls VdSwap), so it shows the game's frame
// rate, not the window's refresh rate. CPU: this process over all logical processors; GPU: the busiest GPU engine
// for this process (Task Manager's columns). Ported from mad-riders-recomp.
#pragma once

#include <rex/ui/imgui_dialog.h>

namespace recomp_ui {

// Counts one game frame (called from the game's Swap hook).
void perf_on_frame();

class PerfOverlay : public rex::ui::ImGuiDialog {
 public:
  explicit PerfOverlay(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnDraw(ImGuiIO& io) override;
};

}  // namespace recomp_ui

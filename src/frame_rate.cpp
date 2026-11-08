// See frame_rate.h.
#include "frame_rate.h"

#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_INT32(recomp_frame_rate, 60, "Recomp",
                     "Frame rate on screen: 60 (the game's) or 120 (NativeX frame generation; gameplay stays 60). "
                     "Applies at start-up");

namespace frame_rate {

void raise_timer_resolution() {
#ifdef _WIN32
  static bool done = false;
  if (!done) {
    done = true;
    MMRESULT r = timeBeginPeriod(1);
    REXLOG_INFO("Timer resolution 1 ms: {}", r == TIMERR_NOERROR ? "ok" : "failed");
  }
#endif
}

void apply() {
  raise_timer_resolution();
  int fps = REXCVAR_GET(recomp_frame_rate);
  if (fps != 60 && fps != 120) {
    fps = 60;
    rex::cvar::SetFlagByName("recomp_frame_rate", "60");
  }
  // Only the presentation: the guest's vblank (and so the game's speed) is left alone.
  for (const auto& e : rex::cvar::GetRegistry()) {
    if (e.name == "native_x_renderer_frame_generation") {
      rex::cvar::SetFlagByName("native_x_renderer_frame_generation", fps == 120 ? "true" : "false");
      break;
    }
  }
  REXLOG_INFO("Frame rate on screen: {} FPS (game 60)", fps);
}

}  // namespace frame_rate

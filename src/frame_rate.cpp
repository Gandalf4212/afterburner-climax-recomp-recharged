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
                     "Frame rate: 60 (original) or 120 (experimental). Applies at start-up");

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
  rex::cvar::SetFlagByName("video_mode_refresh_rate", std::to_string(fps));
  REXLOG_INFO("Frame rate: {} FPS (guest refresh {} Hz)", fps, fps);
}

}  // namespace frame_rate

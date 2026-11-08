// See frame_rate.h.
#include "frame_rate.h"


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

namespace frame_rate {

void apply() {
#ifdef _WIN32
  MMRESULT r = timeBeginPeriod(1);
  REXLOG_INFO("Timer resolution 1 ms: {}", r == TIMERR_NOERROR ? "ok" : "failed");
#endif
  // The game's logic runs once per frame: 60 Hz is its speed (a value left in the config is overridden).
  rex::cvar::SetFlagByName("video_mode_refresh_rate", "60");
}

}  // namespace frame_rate

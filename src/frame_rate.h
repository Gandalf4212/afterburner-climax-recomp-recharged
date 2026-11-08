// Frame rate (F1 > Display > Frame rate, cvar recomp_frame_rate: 60 or 120). After Burner Climax presents once
// per guest vblank (~59 FPS at the SDK's 60 Hz), so the setting sets the guest vblank rate
// (video_mode_refresh_rate). 120 is experimental: if the game steps its logic once per frame, it runs at double
// speed. Also raises the Windows timer resolution to 1 ms (the SDK's 1 ms sleeps otherwise last ~15.6 ms).
// Applied at start-up. Ported from mad-riders-recomp (which ran at 30).
#pragma once

namespace frame_rate {
void apply();
void raise_timer_resolution();
}  // namespace frame_rate

// Frame pacing. After Burner Climax steps its game logic once per frame (owner's test, 2026-11-08: a 120 Hz guest
// vblank doubled the game's speed), so the guest refresh stays at 60 Hz and there is no frame-rate setting. Raises
// the Windows timer resolution to 1 ms (the SDK's 1 ms sleeps otherwise last ~15.6 ms: late frames). At start-up.
#pragma once

namespace frame_rate {
void apply();
}  // namespace frame_rate

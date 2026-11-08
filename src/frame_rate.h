// Frame rate on screen (F1 > Display > Frame rate, cvar recomp_frame_rate: 60 or 120). The game always runs at its
// own 60 FPS; 120 turns on NativeXRenderer's frame generation (FSR 3.1, native_x_renderer_frame_generation, D123):
// a frame interpolated between each two of the game's, presented, never simulated. It needs NativeX with DLSS or
// FSR 3.1 (the game's 3D view and motion vectors), not mapped for this game yet. The guest vblank rate is never
// changed. Also raises the Windows timer resolution to 1 ms (steadier 60 FPS pacing). Applied at start-up.
#pragma once

namespace frame_rate {
void apply();
void raise_timer_resolution();
}  // namespace frame_rate

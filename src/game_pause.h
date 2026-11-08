// Pausing the game while the F1 menu is open. Cooperative: the game's main thread waits at the start of its next
// frame (the Swap hook in native_x_renderer_abc.cpp), a clean point between two frames, so its state is frozen and
// no lock is held. (Suspending the guest threads from outside could stop one inside a host lock, e.g. the heap's,
// and hang the menu.) NativeX shows the last frame with the menu meanwhile (its D132). The game's other threads
// (audio streams, loading) keep running.
#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include <native_x_renderer/native_x_renderer.h>

namespace game_pause {

inline std::mutex& Mutex() {
  static std::mutex m;
  return m;
}
inline std::condition_variable& Cv() {
  static std::condition_variable cv;
  return cv;
}
inline std::atomic<bool>& Flag() {
  static std::atomic<bool> paused{false};
  return paused;
}

inline void set(bool paused) {
  {
    std::lock_guard lock(Mutex());
    Flag().store(paused, std::memory_order_relaxed);
  }
  native_x_renderer::SetPaused(paused);
  Cv().notify_all();
}

// Called by the game's thread at each frame start: returns at once unless paused.
inline void wait_while_paused() {
  if (!Flag().load(std::memory_order_relaxed)) [[likely]] return;
  std::unique_lock lock(Mutex());
  Cv().wait(lock, [] { return !Flag().load(std::memory_order_relaxed); });
}

}  // namespace game_pause

// Exit watchdog: makes sure the process really ends when the player quits.
//
// Problem (inherited from the original recomp): after quitting, the window
// closed but after_burner_climax.exe stayed in Task Manager.
//
// ReXGlue's quit path (ReXApp::OnClosing, SDK v0.9.0 and v0.10.1 alike) is
//   KernelState::TerminateTitle()  -> std::_Exit(0)
// TerminateTitle() waits only 200 ms for guest threads to exit and leaves
// stragglers running, then takes the kernel's global critical region again to
// drop them from its thread map. A straggler that holds that lock (or any lock
// the teardown needs) blocks it forever, so _Exit is never reached. _Exit
// itself can also stall in DLL detach on Windows (ExitProcess runs every DLL's
// DLL_PROCESS_DETACH under the loader lock). Either way the UI is gone and the
// process lingers.
//
// Fix: when a close is requested (window X, Alt+F4, or the menu's Quit), arm a
// detached thread. If the process is still alive after the grace period it is
// ended with TerminateProcess, which skips all teardown (nothing is left to
// save at this point: settings are written when changed). The log records
// which stage was reached, so the real blocker can still be found.
#pragma once

#include <atomic>
#include <chrono>
#include <thread>

#include <rex/logging.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace exit_watchdog {

// Grace period between the close request and the forced end.
inline constexpr std::chrono::milliseconds kGrace{3000};

inline void arm(const char* why) {
  static std::atomic<bool> armed{false};
  if (armed.exchange(true)) return;
  REXLOG_INFO("Exit watchdog armed ({}): forcing exit in {} ms if shutdown stalls", why,
              kGrace.count());
  rex::FlushLogging();
  std::thread([] {
    std::this_thread::sleep_for(kGrace);
    REXLOG_WARN("Exit watchdog: shutdown did not finish in {} ms; terminating the process",
                kGrace.count());
    rex::FlushLogging();
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), 0);
#else
    std::_Exit(0);
#endif
  }).detach();
}

}  // namespace exit_watchdog

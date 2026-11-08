// See perf_overlay.h.
#include "perf_overlay.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#pragma comment(lib, "pdh.lib")
#endif

#include <rex/cvar.h>

#include <imgui.h>

REXCVAR_DEFINE_INT32(recomp_perf_overlay, 0, "Recomp",
                     "Performance overlay: 0 off, 1 FPS, 2 FPS + CPU/GPU, 3 FPS + CPU/GPU + frame time graph");

namespace {

constexpr int kHistory = 120;
std::array<std::atomic<float>, kHistory> g_frame_ms{};
std::atomic<uint32_t> g_frames{0};
std::atomic<int64_t> g_last_us{0};

int64_t now_us() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// ------------------------------------------------------------------ CPU and GPU use
// Sampled once a second on a thread of its own while the overlay shows them (modes 2 and 3).
// CPU: this process's CPU time over the wall time of all logical processors (Task Manager's "CPU" column).
// GPU: the busiest GPU engine's utilization for this process, from the "GPU Engine" performance counters
// (Task Manager's "GPU" column). -1 = not available yet.
std::atomic<float> g_cpu_percent{-1.0f};
std::atomic<float> g_gpu_percent{-1.0f};
std::atomic<int64_t> g_usage_wanted_us{0};  // last time the overlay asked; the sampler sleeps when stale
std::once_flag g_sampler_started;

#ifdef _WIN32
uint64_t FileTimeTo100ns(const FILETIME& t) { return (uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime; }

void SampleUsage() {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  const double cpus = double(std::max<DWORD>(si.dwNumberOfProcessors, 1));
  const std::wstring pid_tag = L"pid_" + std::to_wstring(GetCurrentProcessId()) + L"_";

  PDH_HQUERY query = nullptr;
  PDH_HCOUNTER counter = nullptr;
  bool gpu_ok = PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS &&
                PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter) ==
                    ERROR_SUCCESS;
  if (gpu_ok) PdhCollectQueryData(query);

  uint64_t last_cpu = 0;
  int64_t last_wall = 0;
  std::vector<uint8_t> buffer;
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (now_us() - g_usage_wanted_us.load(std::memory_order_relaxed) > 3'000'000) {
      last_wall = 0;  // overlay hidden: start over when it shows again
      continue;
    }
    FILETIME created, exited, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
      uint64_t cpu = FileTimeTo100ns(kernel) + FileTimeTo100ns(user);
      int64_t wall = now_us();
      if (last_wall) {
        double busy_us = double(cpu - last_cpu) / 10.0;
        double pct = 100.0 * busy_us / (double(wall - last_wall) * cpus);
        g_cpu_percent.store(float(std::clamp(pct, 0.0, 100.0)), std::memory_order_relaxed);
      }
      last_cpu = cpu;
      last_wall = wall;
    }
    if (!gpu_ok || PdhCollectQueryData(query) != ERROR_SUCCESS) continue;
    DWORD size = 0, count = 0;
    if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, &count, nullptr) !=
            PDH_MORE_DATA ||
        !size) {
      continue;
    }
    buffer.resize(size);
    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
    if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, &count, items) !=
        ERROR_SUCCESS) {
      continue;
    }
    // Instances: pid_<pid>_luid_<adapter>_phys_<n>_eng_<n>_engtype_<3D|Copy|...>. Sum per engine, take the busiest.
    std::vector<std::pair<std::wstring, double>> engines;
    for (DWORD i = 0; i < count; ++i) {
      std::wstring name = items[i].szName ? items[i].szName : L"";
      if (name.rfind(pid_tag, 0) != 0 || items[i].FmtValue.CStatus != ERROR_SUCCESS) continue;
      std::wstring engine = name.substr(name.find(L"_luid_"));
      auto it = std::find_if(engines.begin(), engines.end(), [&](auto& e) { return e.first == engine; });
      if (it == engines.end()) engines.emplace_back(engine, items[i].FmtValue.doubleValue);
      else it->second += items[i].FmtValue.doubleValue;
    }
    double busiest = 0;
    for (auto& e : engines) busiest = std::max(busiest, e.second);
    g_gpu_percent.store(float(std::clamp(busiest, 0.0, 100.0)), std::memory_order_relaxed);
  }
}
#endif

void WantUsage() {
  g_usage_wanted_us.store(now_us(), std::memory_order_relaxed);
#ifdef _WIN32
  std::call_once(g_sampler_started, [] { std::thread(SampleUsage).detach(); });
#endif
}

}  // namespace

namespace recomp_ui {
// Called once per game frame, at the start of the game's Swap (sub_82252730, which calls VdSwap).
void perf_on_frame() {
  int64_t t = now_us();
  int64_t last = g_last_us.exchange(t, std::memory_order_relaxed);
  if (last) {
    uint32_t n = g_frames.fetch_add(1, std::memory_order_relaxed);
    g_frame_ms[n % kHistory].store(float(t - last) / 1000.0f, std::memory_order_relaxed);
  }
}
}  // namespace recomp_ui

// The game's Swap is wrapped by NativeXRenderer's hook file (src/native_x_renderer_abc.cpp),
// which calls perf_on_frame().

namespace recomp_ui {

void PerfOverlay::OnDraw(ImGuiIO& io) {
  int mode = std::clamp(REXCVAR_GET(recomp_perf_overlay), 0, 3);
  if (mode <= 0) return;
  uint32_t n = g_frames.load(std::memory_order_relaxed);
  int count = int(std::min<uint32_t>(n, kHistory));
  if (!count) return;

  // Average over the most recent ~0.5 s of frames.
  float sum = 0, worst = 0;
  int used = 0;
  std::array<float, kHistory> hist{};
  for (int i = 0; i < count; i++) hist[i] = g_frame_ms[(n - count + i) % kHistory].load(std::memory_order_relaxed);
  for (int i = count - 1; i >= 0 && sum < 500.0f; i--) {
    sum += hist[i];
    worst = std::max(worst, hist[i]);
    used++;
  }
  float avg = used ? sum / used : 0;
  // A stalled game (loading) shows the time since the last frame.
  float since = float(now_us() - g_last_us.load()) / 1000.0f;
  if (since > 250.0f) avg = since;

  char line[128];
  std::snprintf(line, sizeof line, "FPS: %.0f", avg > 0 ? 1000.0f / avg : 0.0f);
  if (mode >= 2) {
    WantUsage();
    float cpu = g_cpu_percent.load(std::memory_order_relaxed), gpu = g_gpu_percent.load(std::memory_order_relaxed);
    char cpu_text[16] = "--", gpu_text[16] = "--";
    if (cpu >= 0) std::snprintf(cpu_text, sizeof cpu_text, "%.0f%%", cpu);
    if (gpu >= 0) std::snprintf(gpu_text, sizeof gpu_text, "%.0f%%", gpu);
    size_t len = std::strlen(line);
    std::snprintf(line + len, sizeof line - len, "   CPU: %s   GPU: %s", cpu_text, gpu_text);
  }

  // The menu's blue theme: a dark navy panel with a blue border, white text, a cyan graph.
  const float pad = 10.0f;
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - pad, pad), ImGuiCond_Always, ImVec2(1, 0));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(4, 14, 40, 190));
  ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(70, 150, 255, 170));
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(6, 20, 52, 200));
  ImGui::PushStyleColor(ImGuiCol_PlotLines, IM_COL32(90, 200, 255, 255));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::Begin("##recomp_perf", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
  ImGui::TextUnformatted(line);
  if (mode >= 3) {
    std::snprintf(line, sizeof line, "frame time, worst %.1f ms", worst);
    ImGui::PlotLines("##ft", hist.data(), count, 0, line, 0.0f, 50.0f,
                     ImVec2(std::max(240.0f, ImGui::GetItemRectSize().x), 56.0f));
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(5);
}

}  // namespace recomp_ui

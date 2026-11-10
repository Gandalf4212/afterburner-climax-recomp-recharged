// Crash report: on an unhandled exception (Windows), writes the exception, the faulting module and offset, and the
// faulting thread's stack (module+offset; function and line too when a .pdb is next to the exe, e.g. a local debug
// build) to the log and to crash.txt next to the exe. The SDK's own access
// violation handling (guest memory, write watches) runs first through vectored handlers; only what nobody handled
// arrives here.
#pragma once

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include <rex/logging.h>

#pragma comment(lib, "dbghelp.lib")

namespace crash_handler {

inline std::filesystem::path& Folder() {
  static std::filesystem::path folder;
  return folder;
}

// "module+0xoffset (function, file:line)" for an address.
inline std::string Describe(HANDLE process, DWORD64 address) {
  char text[768];
  HMODULE module = nullptr;
  char module_path[MAX_PATH] = "?";
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(address), &module)) {
    GetModuleFileNameA(module, module_path, MAX_PATH);
  }
  const char* name = std::strrchr(module_path, '\\');
  name = name ? name + 1 : module_path;
  int n = std::snprintf(text, sizeof text, "%s+0x%llx", name,
                        static_cast<unsigned long long>(address - reinterpret_cast<DWORD64>(module)));
  alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256] = {};
  auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = 255;
  DWORD64 displacement = 0;
  if (SymFromAddr(process, address, &displacement, symbol)) {
    n += std::snprintf(text + n, sizeof text - n, " %s+0x%llx", symbol->Name,
                       static_cast<unsigned long long>(displacement));
    IMAGEHLP_LINE64 line = {sizeof(IMAGEHLP_LINE64)};
    DWORD line_displacement = 0;
    if (SymGetLineFromAddr64(process, address, &line_displacement, &line)) {
      const char* file = std::strrchr(line.FileName, '\\');
      std::snprintf(text + n, sizeof text - n, " (%s:%lu)", file ? file + 1 : line.FileName, line.LineNumber);
    }
  }
  return text;
}

inline LONG WINAPI Filter(EXCEPTION_POINTERS* info) {
  static volatile LONG once = 0;
  if (InterlockedExchange(&once, 1)) return EXCEPTION_CONTINUE_SEARCH;
  HANDLE process = GetCurrentProcess();
  SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
  SymInitialize(process, Folder().string().c_str(), TRUE);

  std::string report;
  const EXCEPTION_RECORD* record = info->ExceptionRecord;
  char line[1024];
  std::snprintf(line, sizeof line, "Crash: exception 0x%08lx at %s, thread %lu", record->ExceptionCode,
                Describe(process, reinterpret_cast<DWORD64>(record->ExceptionAddress)).c_str(),
                GetCurrentThreadId());
  report += line;
  if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
    std::snprintf(line, sizeof line, "; %s address 0x%llx",
                  record->ExceptionInformation[0] == 0   ? "reading"
                  : record->ExceptionInformation[0] == 1 ? "writing"
                                                         : "executing",
                  static_cast<unsigned long long>(record->ExceptionInformation[1]));
    report += line;
  }
  report += "\n";

  // The faulting thread's stack, unwound from the exception's context (x64 unwind data).
  CONTEXT context = *info->ContextRecord;
  for (int frame = 0; frame < 48 && context.Rip; ++frame) {
    std::snprintf(line, sizeof line, "  #%02d %s\n", frame, Describe(process, context.Rip).c_str());
    report += line;
    DWORD64 image_base = 0;
    PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &image_base, nullptr);
    if (!function) {  // a leaf function: the return address is on top of the stack
      context.Rip = *reinterpret_cast<DWORD64*>(context.Rsp);
      context.Rsp += 8;
      continue;
    }
    void* handler_data = nullptr;
    DWORD64 establisher = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, context.Rip, function, &context, &handler_data, &establisher,
                     nullptr);
  }

  REXLOG_ERROR("{}", report);
  rex::FlushLogging();
  if (FILE* f = _wfopen((Folder() / "crash.txt").c_str(), L"w")) {
    std::fputs(report.c_str(), f);
    std::fclose(f);
  }
  // A crash may come from what the shader cache holds (a bad shader or pipeline from this run, log 009): the next
  // launch deletes shaders.cache before opening it (after_burner_climax_app.h, SetUpShaderCache).
  if (FILE* f = _wfopen((Folder() / "shaders.cache.bad").c_str(), L"w")) std::fclose(f);
  return EXCEPTION_CONTINUE_SEARCH;  // Windows Error Reporting still records it
}

// Call early, and again after the SDK's set-up (a library may replace the filter).
inline void install(const std::filesystem::path& exe_folder) {
  Folder() = exe_folder;
  SetUnhandledExceptionFilter(&Filter);
}

}  // namespace crash_handler

#else

#include <filesystem>
namespace crash_handler {
inline void install(const std::filesystem::path&) {}
}  // namespace crash_handler

#endif

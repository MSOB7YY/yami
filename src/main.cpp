// by claude

#include <windows.h>

#include <cwchar>

#include "app.h"

namespace {

constexpr wchar_t kMutexName[] = L"Local\\Yami";

}  // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR commandLine, _In_ int) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  const HANDLE mutex = CreateMutexW(nullptr, FALSE, kMutexName);
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    const HWND existing = FindWindowW(yami::App::kWindowClass, nullptr);
    if (existing) {
      DWORD processId = 0;
      GetWindowThreadProcessId(existing, &processId);
      AllowSetForegroundWindow(processId);
      PostMessageW(existing, yami::App::kShowMessage, 0, 0);
    }
    return 0;
  }
  const bool isStartHidden = wcsstr(commandLine, yami::App::kTrayArgument) != nullptr;
  yami::App app;
  const int exitCode = app.Run(instance, isStartHidden);
  if (mutex) CloseHandle(mutex);
  return exitCode;
}

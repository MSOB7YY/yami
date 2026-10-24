// by claude

#include "app.h"

#include <dwmapi.h>
#include <shellscalingapi.h>
#include <wtsapi32.h>

#include <algorithm>
#include <cstring>
#include <cwchar>

#include "gamma_ramp.h"

namespace yami {

int App::Run(HINSTANCE instance, bool isStartHidden) {
  taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
  isGammaRangeUnlocked_ = gamma_ramp::IsRangeUnlocked();
  dimCap_ = isGammaRangeUnlocked_ ? kMaxDim : kLockedDimCap;
  WNDCLASSEXW windowClass{};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.lpfnWndProc = WndProc;
  windowClass.hInstance = instance;
  windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  windowClass.lpszClassName = kWindowClass;
  RegisterClassExW(&windowClass);
  CreateWindowExW(kWindowExStyle, kWindowClass, kAppName, kWindowStyle, 0, 0, 0, 0, nullptr, nullptr, instance, this);
  if (!hwnd_) return 1;
  menuOwner_ = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", nullptr, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
  worker_ = std::make_unique<DdcWorker>(hwnd_, kProbedMessage);
  LoadAutoSource();
  StartAuto();
  EnumerateMonitors();
  AddTrayIcon();
  if (!isStartHidden) ShowAtCorner();
  MSG message;
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  worker_.reset();
  return 0;
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    auto* createdApp = static_cast<App*>(create->lpCreateParams);
    createdApp->hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createdApp));
  }
  auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (!app) return DefWindowProcW(hwnd, message, wParam, lParam);
  return app->HandleMessage(message, wParam, lParam);
}

LRESULT App::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE:
      OnCreate();
      return 0;
    case WM_NCHITTEST:
      return HitTest(lParam);
    case WM_PAINT: {
      PAINTSTRUCT paint;
      const HDC dc = BeginPaint(hwnd_, &paint);
      Paint(dc, paint.rcPaint);
      EndPaint(hwnd_, &paint);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_LBUTTONDOWN:
      OnMouseDown(lParam);
      return 0;
    case WM_MOUSEMOVE:
      OnMouseMove(lParam);
      return 0;
    case WM_LBUTTONUP:
      OnMouseUp(lParam);
      return 0;
    case WM_RBUTTONUP:
      OnRightClick(lParam);
      return 0;
    case WM_MOUSELEAVE:
      OnMouseLeave();
      return 0;
    case WM_CAPTURECHANGED:
      OnCaptureChanged();
      return 0;
    case WM_MOUSEWHEEL:
      OnMouseWheel(wParam, lParam);
      return 0;
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE) OnEscape();
      return 0;
    case WM_CLOSE:
      HideWindow();
      return 0;
    case WM_TIMER:
      OnTimer(wParam);
      return 0;
    case WM_DISPLAYCHANGE:
      OnDisplayChange();
      return 0;
    case WM_POWERBROADCAST:
      OnPowerBroadcast(wParam, lParam);
      return TRUE;
    case WM_WTSSESSION_CHANGE:
      if (wParam == WTS_SESSION_UNLOCK || wParam == WTS_CONSOLE_CONNECT) StartGammaRestore();
      return 0;
    case WM_DPICHANGED:
      OnDpiChanged(wParam);
      return 0;
    case WM_SETTINGCHANGE:
      OnSettingChange(lParam);
      return 0;
    case kTrayMessage:
      OnTrayMessage(wParam, lParam);
      return 0;
    case kProbedMessage:
      OnProbed(wParam, lParam);
      return 0;
    case kLightReadingMessage:
      OnLightReading(lParam);
      return 0;
    case kLightChangedMessage:
      OnLightChanged();
      return 0;
    case kShowMessage:
      ShowAtCorner();
      return 0;
    case WM_HOTKEY:
      if (wParam == kDimOffHotkeyId) TurnDimOff();
      return 0;
    case WM_DESTROY:
      OnDestroy();
      return 0;
  }
  if (message == taskbarCreatedMessage_) {
    AddTrayIcon();
    return 0;
  }
  return DefWindowProcW(hwnd_, message, wParam, lParam);
}

void App::OnCreate() {
  dpi_ = GetDpiForWindow(hwnd_);
  const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
  const COLORREF border = kColorBorder;
  DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
  DwmSetWindowAttribute(hwnd_, DWMWA_BORDER_COLOR, &border, sizeof(border));
  CreateFonts();
  isDimOffHotkeyRegistered_ = RegisterHotKey(hwnd_, kDimOffHotkeyId, kDimOffHotkeyModifiers, kDimOffHotkeyKey);
  displayStateNotification_ = RegisterPowerSettingNotification(hwnd_, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);
  WTSRegisterSessionNotification(hwnd_, NOTIFY_FOR_THIS_SESSION);
}

void App::OnDestroy() {
  if (isDimOffHotkeyRegistered_) UnregisterHotKey(hwnd_, kDimOffHotkeyId);
  if (displayStateNotification_) UnregisterPowerSettingNotification(displayStateNotification_);
  WTSUnRegisterSessionNotification(hwnd_);
  KillTimer(hwnd_, kSaveTimer);
  KillTimer(hwnd_, kGammaWatchTimer);
  KillTimer(hwnd_, kGammaRestoreTimer);
  StopAuto();
  SaveAll();
  RemoveTrayIcon();
  ResetGammaRamps();
  ReleaseMonitorDcs();
  ReleaseBackBuffer();
  DestroyFonts();
  if (trayIcon_) DestroyIcon(trayIcon_);
  if (menuOwner_) DestroyWindow(menuOwner_);
  PostQuitMessage(0);
}

void App::OnTimer(WPARAM timer) {
  if (timer == kSaveTimer) {
    KillTimer(hwnd_, kSaveTimer);
    SaveAll();
    UpdateTrayTip();
    return;
  }
  if (timer == kGammaWatchTimer) RestoreGammaRamps();
  if (timer == kGammaRestoreTimer) StepGammaRestore();
  if (timer == kLightTimer) StepLight();
  if (timer == kClockTimer) UpdateClock();
}

void App::OnDpiChanged(WPARAM wParam) {
  const UINT dpi = HIWORD(wParam);
  if (dpi == dpi_) return;
  dpi_ = dpi;
  CreateFonts();
  ResizeWindow();
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::OnSettingChange(LPARAM lParam) {
  const auto* area = reinterpret_cast<const wchar_t*>(lParam);
  if (!area || wcscmp(area, L"ImmersiveColorSet") != 0) return;
  RefreshTrayIcon();
}

void App::OnDisplayChange() {
  if (isSuspended_) {
    hasPendingEnumerate_ = true;
    return;
  }
  EnumerateMonitors();
}

void App::OnPowerBroadcast(WPARAM event, LPARAM lParam) {
  if (event == PBT_APMSUSPEND) {
    isSuspended_ = true;
    worker_->Pause();
    return;
  }
  if (event == PBT_APMRESUMEAUTOMATIC || event == PBT_APMRESUMESUSPEND) {
    OnResumed();
    return;
  }
  if (event != PBT_POWERSETTINGCHANGE) return;
  const auto* setting = reinterpret_cast<const POWERBROADCAST_SETTING*>(lParam);
  const bool isDisplayState = IsEqualGUID(setting->PowerSetting, GUID_CONSOLE_DISPLAY_STATE);
  if (!isDisplayState) return;
  DWORD displayState = 0;
  std::memcpy(&displayState, setting->Data, sizeof(displayState));
  if (displayState == kDisplayStateOn) StartGammaRestore();
}

void App::OnResumed() {
  isSuspended_ = false;
  worker_->Resume();
  if (hasPendingEnumerate_) {
    hasPendingEnumerate_ = false;
    EnumerateMonitors();
  } else {
    StartGammaRestore();
  }
  if (autoSource_ == AutoSource::time) {
    UpdateClock();
  } else {
    ApplyAuto();
  }
}

void App::ToggleWindow() {
  const bool isVisible = IsWindowVisible(hwnd_);
  if (isVisible && IsUncovered()) {
    HideWindow();
    return;
  }
  ShowAtCorner();
}

bool App::IsUncovered() const {
  RECT own{};
  GetWindowRect(hwnd_, &own);
  for (HWND above = GetWindow(hwnd_, GW_HWNDPREV); above; above = GetWindow(above, GW_HWNDPREV)) {
    if (!IsWindowVisible(above) || IsIconic(above)) continue;
    const LONG_PTR exStyle = GetWindowLongPtrW(above, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOPMOST) continue;
    DWORD cloaked = 0;
    DwmGetWindowAttribute(above, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) continue;
    RECT bounds{};
    const HRESULT boundsResult = DwmGetWindowAttribute(above, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds));
    if (FAILED(boundsResult)) GetWindowRect(above, &bounds);
    RECT overlap{};
    if (IntersectRect(&overlap, &own, &bounds)) return false;
  }
  return true;
}

void App::HideWindow() {
  ShowWindow(hwnd_, SW_HIDE);
  hotButton_ = TitleButton::none;
  isHelpVisible_ = false;
  ExitPreview();
}

void App::OnEscape() {
  if (isHelpVisible_) {
    ToggleHelp();
    return;
  }
  HideWindow();
}

void App::ToggleHelp() {
  isHelpVisible_ = !isHelpVisible_;
  helpScroll_ = 0;
  ResizeWindow();
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::ScrollHelp(int wheelDelta) {
  const int maxScroll = HelpMaxScroll();
  const int step = Scale(kHelpScrollStep);
  const int moved = helpScroll_ - MulDiv(wheelDelta, step, WHEEL_DELTA);
  const int scroll = std::clamp(moved, 0, maxScroll);
  if (scroll == helpScroll_) return;
  helpScroll_ = scroll;
  const int titleHeight = Scale(kTitleHeight);
  const int clientWidth = ClientWidth();
  const int clientHeight = ClientHeight();
  const RECT content{0, titleHeight, clientWidth, clientHeight};
  InvalidateRect(hwnd_, &content, FALSE);
}

void App::ShowAtCorner() {
  POINT cursor{};
  GetCursorPos(&cursor);
  const HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO info{};
  info.cbSize = sizeof(info);
  GetMonitorInfoW(monitor, &info);
  UINT dpiX = 0;
  UINT dpiY = 0;
  const HRESULT dpiResult = GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
  if (SUCCEEDED(dpiResult) && dpiX != dpi_) {
    dpi_ = dpiX;
    CreateFonts();
  }
  const int width = ClientWidth();
  const int height = ClientHeight();
  const int margin = Scale(kWindowMargin);
  const int x = info.rcWork.right - width - margin;
  const int y = info.rcWork.bottom - height - margin;
  SetWindowPos(hwnd_, nullptr, x, y, width, height, SWP_NOZORDER | SWP_SHOWWINDOW);
  SetForegroundWindow(hwnd_);
  worker_->Refresh();
}

void App::ResizeWindow() {
  const int width = ClientWidth();
  const int height = ClientHeight();
  RECT window{};
  GetWindowRect(hwnd_, &window);
  const int x = window.right - width;
  const int y = window.bottom - height;
  SetWindowPos(hwnd_, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace yami

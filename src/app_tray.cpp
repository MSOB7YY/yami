// by claude

#include <windowsx.h>

#include <cwchar>

#include "app.h"
#include "tray_icon.h"

namespace yami {

void App::OnTrayMessage(WPARAM wParam, LPARAM lParam) {
  const UINT event = LOWORD(lParam);
  if (event == NIN_SELECT || event == NIN_KEYSELECT) {
    ToggleWindow();
    return;
  }
  if (event != WM_CONTEXTMENU) return;
  const int x = GET_X_LPARAM(wParam);
  const int y = GET_Y_LPARAM(wParam);
  ShowTrayMenu(x, y);
}

void App::ShowTrayMenu(int x, int y) {
  const HMENU menu = CreatePopupMenu();
  const bool isAutorun = IsAutorunEnabled();
  const UINT autorunFlags = MF_STRING | (isAutorun ? MF_CHECKED : MF_UNCHECKED);
  const wchar_t* dimOffText = isDimOffHotkeyRegistered_ ? L"Turn dim off\tCtrl+Alt+Shift+D" : L"Turn dim off";
  AppendMenuW(menu, MF_STRING, kCommandDimOff, dimOffText);
  AppendMenuW(menu, autorunFlags, kCommandAutorun, L"Run at startup");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kCommandExit, L"Exit");
  SetForegroundWindow(hwnd_);
  const UINT command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, x, y, hwnd_, nullptr);
  DestroyMenu(menu);
  // documented tray menu requirement, the next menu closes instantly without it
  PostMessageW(hwnd_, WM_NULL, 0, 0);
  if (command == kCommandDimOff) TurnDimOff();
  if (command == kCommandAutorun) SetAutorun(!isAutorun);
  if (command == kCommandExit) DestroyWindow(hwnd_);
}

NOTIFYICONDATAW App::TrayData() const {
  NOTIFYICONDATAW data{};
  data.cbSize = sizeof(data);
  data.hWnd = hwnd_;
  data.uID = kTrayId;
  return data;
}

void App::AddTrayIcon() {
  if (!trayIcon_) trayIcon_ = tray_icon::Create();
  NOTIFYICONDATAW data = TrayData();
  data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
  data.uCallbackMessage = kTrayMessage;
  data.hIcon = trayIcon_;
  WriteTrayTip(data);
  Shell_NotifyIconW(NIM_ADD, &data);
  data.uVersion = NOTIFYICON_VERSION_4;
  Shell_NotifyIconW(NIM_SETVERSION, &data);
}

void App::RemoveTrayIcon() const {
  NOTIFYICONDATAW data = TrayData();
  Shell_NotifyIconW(NIM_DELETE, &data);
}

void App::UpdateTrayTip() const {
  NOTIFYICONDATAW data = TrayData();
  data.uFlags = NIF_TIP | NIF_SHOWTIP;
  WriteTrayTip(data);
  Shell_NotifyIconW(NIM_MODIFY, &data);
}

void App::RefreshTrayIcon() {
  const HICON previous = trayIcon_;
  trayIcon_ = tray_icon::Create();
  NOTIFYICONDATAW data = TrayData();
  data.uFlags = NIF_ICON;
  data.hIcon = trayIcon_;
  Shell_NotifyIconW(NIM_MODIFY, &data);
  if (previous) DestroyIcon(previous);
}

void App::WriteTrayTip(NOTIFYICONDATAW& data) const {
  std::wstring tip = kAppName;
  for (const Monitor& monitor : monitors_) {
    tip += L"\n";
    tip += monitor.name;
    if (monitor.ddc == DdcState::supported) {
      tip += L": brightness ";
      tip += std::to_wstring(monitor.brightness);
      tip += L", dim ";
    } else {
      tip += L": dim ";
    }
    tip += std::to_wstring(monitor.dim);
  }
  wcsncpy_s(data.szTip, tip.c_str(), _TRUNCATE);
}

std::wstring App::AutorunCommand() {
  wchar_t path[MAX_PATH];
  const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring command = L"\"";
  command.append(path, length);
  command += L"\" ";
  command += kTrayArgument;
  return command;
}

bool App::IsAutorunEnabled() {
  const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, kRunKey, kAppName, RRF_RT_REG_SZ, nullptr, nullptr, nullptr);
  return status == ERROR_SUCCESS;
}

void App::SetAutorun(bool isEnabled) {
  if (!isEnabled) {
    RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kAppName);
    return;
  }
  const std::wstring command = AutorunCommand();
  const DWORD size = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
  RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kAppName, REG_SZ, command.c_str(), size);
}

}  // namespace yami

// by claude

#include "gamma_ramp.h"

#include <shellapi.h>

#include <cmath>
#include <cstdlib>
#include <string>

namespace yami::gamma_ramp {

namespace {

constexpr wchar_t kRangeKey[] = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ICM";
constexpr wchar_t kRangeValue[] = L"GdiIcmGammaRange";
constexpr DWORD kRangeUnlocked = 256;
constexpr DWORD kUnlockTimeoutMS = 10000;

WORD RampValue(int step, double gamma) {
  const long value = std::lround(step * 257 * gamma);
  return static_cast<WORD>(value);
}

}  // namespace

double ReadLeftover(HDC dc) {
  if (!dc) return 1.0;
  WORD ramp[3][256];
  const bool didRead = GetDeviceGammaRamp(dc, ramp);
  if (!didRead) return 1.0;
  const double gamma = ramp[0][255] / 65535.0;
  for (int channel = 0; channel < 3; ++channel) {
    for (int i = 0; i < 256; ++i) {
      const int expected = RampValue(i, gamma);
      const int difference = std::abs(ramp[channel][i] - expected);
      if (difference > 1) return 1.0;
    }
  }
  return gamma;
}

bool IsApplied(HDC dc, double gamma) {
  WORD ramp[3][256];
  const bool didRead = GetDeviceGammaRamp(dc, ramp);
  const WORD expected = RampValue(255, gamma);
  return didRead && ramp[0][255] == expected;
}

bool Write(HDC dc, double gamma) {
  if (!dc) return false;
  WORD ramp[3][256];
  for (int i = 0; i < 256; ++i) {
    const WORD value = RampValue(i, gamma);
    ramp[0][i] = value;
    ramp[1][i] = value;
    ramp[2][i] = value;
  }
  return SetDeviceGammaRamp(dc, ramp);
}

bool IsRangeUnlocked() {
  DWORD value = 0;
  DWORD size = sizeof(value);
  const LSTATUS status = RegGetValueW(HKEY_LOCAL_MACHINE, kRangeKey, kRangeValue, RRF_RT_REG_DWORD, nullptr, &value, &size);
  return status == ERROR_SUCCESS && value >= kRangeUnlocked;
}

bool UnlockRange(HWND owner) {
  const std::wstring range = std::to_wstring(kRangeUnlocked);
  std::wstring parameters = L"add \"HKLM\\";
  parameters += kRangeKey;
  parameters += L"\" /v ";
  parameters += kRangeValue;
  parameters += L" /t REG_DWORD /d ";
  parameters += range;
  parameters += L" /f";
  SHELLEXECUTEINFOW execute{};
  execute.cbSize = sizeof(execute);
  execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
  execute.hwnd = owner;
  execute.lpVerb = L"runas";
  execute.lpFile = L"reg.exe";
  execute.lpParameters = parameters.c_str();
  execute.nShow = SW_HIDE;
  const bool didStart = ShellExecuteExW(&execute);
  if (!didStart || !execute.hProcess) return false;
  WaitForSingleObject(execute.hProcess, kUnlockTimeoutMS);
  CloseHandle(execute.hProcess);
  return IsRangeUnlocked();
}

}  // namespace yami::gamma_ramp

// by claude

#pragma once

#include <windows.h>

namespace yami::gamma_ramp {

double ReadLeftover(HDC dc);
bool IsApplied(HDC dc, double gamma);
bool Write(HDC dc, double gamma);
bool IsRangeUnlocked();
bool UnlockRange(HWND owner);

}  // namespace yami::gamma_ramp

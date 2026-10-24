// by claude

#pragma once

#include <windows.h>

namespace yami::gamma_ramp {

double ReadLeftover(HDC dc);
bool Write(HDC dc, double gamma, bool isNudged = false);
bool IsRangeUnlocked();
bool UnlockRange(HWND owner);

}  // namespace yami::gamma_ramp

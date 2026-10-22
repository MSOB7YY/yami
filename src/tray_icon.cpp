// by claude

#include "tray_icon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace yami::tray_icon {

namespace {

constexpr wchar_t kThemeKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

bool IsLightTaskbar() {
  DWORD value = 0;
  DWORD size = sizeof(value);
  const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, kThemeKey, L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
  return status == ERROR_SUCCESS && value != 0;
}

constexpr int kSamplesPerAxis = 16;
constexpr int kSamplesPerPixel = kSamplesPerAxis * kSamplesPerAxis;

HICON CreateGlyphIcon(int size, BYTE shade) {
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = size;
  info.bmiHeader.biHeight = -size;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  const HBITMAP color = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!color) return nullptr;
  auto* pixels = static_cast<uint32_t*>(bits);
  const double center = size / 2.0;
  const long strokeRounded = std::lround(size / 12.0);
  const long outerRounded = std::lround(size * 0.45);
  const int stroke = std::max(1, static_cast<int>(strokeRounded));
  const int outer = static_cast<int>(outerRounded);
  const int ringInner = outer - stroke;
  const int fillRadius = ringInner - stroke;
  const double outerSquared = static_cast<double>(outer) * outer;
  const double ringInnerSquared = static_cast<double>(ringInner) * ringInner;
  const double fillSquared = static_cast<double>(fillRadius) * fillRadius;
  double offsets[kSamplesPerAxis];
  for (int i = 0; i < kSamplesPerAxis; ++i) offsets[i] = (i + 0.5) / kSamplesPerAxis - center;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      uint32_t covered = 0;
      for (int sy = 0; sy < kSamplesPerAxis; ++sy) {
        const double dy = y + offsets[sy];
        const double dySquared = dy * dy;
        for (int sx = 0; sx < kSamplesPerAxis; ++sx) {
          const double dx = x + offsets[sx];
          const double distanceSquared = dx * dx + dySquared;
          const bool isOnRing = distanceSquared <= outerSquared && distanceSquared > ringInnerSquared;
          const bool isInHalf = dx < 0 && distanceSquared <= fillSquared;
          if (isOnRing || isInHalf) ++covered;
        }
      }
      const uint32_t a = (covered * 255 + kSamplesPerPixel / 2) / kSamplesPerPixel;
      const uint32_t c = a * shade / 255;
      pixels[y * size + x] = (a << 24) | (c << 16) | (c << 8) | c;
    }
  }
  const int maskStride = ((size + 15) / 16) * 2;
  const std::vector<BYTE> maskBits(static_cast<size_t>(maskStride) * size, 0);
  const HBITMAP mask = CreateBitmap(size, size, 1, 1, maskBits.data());
  ICONINFO iconInfo{TRUE, 0, 0, mask, color};
  const HICON icon = CreateIconIndirect(&iconInfo);
  DeleteObject(mask);
  DeleteObject(color);
  return icon;
}

}  // namespace

HICON Create() {
  const UINT dpi = GetDpiForSystem();
  const int size = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
  const bool isLightTaskbar = IsLightTaskbar();
  const BYTE shade = isLightTaskbar ? 24 : 255;
  return CreateGlyphIcon(size, shade);
}

}  // namespace yami::tray_icon

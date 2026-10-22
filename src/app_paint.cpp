// by claude

#include <algorithm>

#include "app.h"

namespace yami {

void App::Paint(HDC target, const RECT& dirty) {
  RECT client{};
  GetClientRect(hwnd_, &client);
  EnsureBackBuffer(target, client.right, client.bottom);
  FillColor(backDc_, dirty, kColorBackground);
  RECT overlap{};
  const RECT title = TitleRect();
  if (IntersectRect(&overlap, &title, &dirty)) PaintTitle(backDc_);
  for (int i = 0; i < static_cast<int>(monitors_.size()); ++i) {
    const ColumnLayout layout = LayoutColumn(i);
    if (!IntersectRect(&overlap, &layout.box, &dirty)) continue;
    PaintColumn(backDc_, monitors_[i], layout, i);
  }
  if (HasFooter()) {
    const RECT footer = FooterRect();
    if (IntersectRect(&overlap, &footer, &dirty)) PaintFooter(backDc_, footer);
  }
  SelectObject(backDc_, GetStockObject(SYSTEM_FONT));
  const int width = dirty.right - dirty.left;
  const int height = dirty.bottom - dirty.top;
  BitBlt(target, dirty.left, dirty.top, width, height, backDc_, dirty.left, dirty.top, SRCCOPY);
}

void App::PaintTitle(HDC dc) const {
  RECT text = TitleRect();
  text.left += Scale(kTitleInset);
  DrawLabel(dc, kAppName, text, titleFont_, kColorTextDim, DT_LEFT);
  const RECT close = CloseRect();
  const bool isCloseActive = isCloseHot_ || isClosePressed_;
  COLORREF closeBackground = kColorBackground;
  if (isCloseHot_) closeBackground = kColorCloseHot;
  if (isClosePressed_) closeBackground = kColorClosePressed;
  const COLORREF glyphColor = isCloseActive ? kColorCloseGlyphHot : kColorText;
  FillColor(dc, close, closeBackground);
  DrawLabel(dc, kCloseGlyph, close, iconFont_, glyphColor, DT_CENTER);
}

void App::PaintColumn(HDC dc, const Monitor& monitor, const ColumnLayout& layout, int index) const {
  const int boxRadius = Scale(kBoxRadius);
  FillRound(dc, layout.box, kColorColumn, kColorColumn, boxRadius);
  DrawLabel(dc, monitor.name, layout.name, nameFont_, kColorText, DT_CENTER);
  const bool isDraggingBrightness = dragMonitor_ == index && dragKind_ == SliderKind::brightness;
  const bool isDraggingDim = dragMonitor_ == index && dragKind_ == SliderKind::dim;
  PaintSlider(dc, monitor, SliderKind::brightness, layout.brightness, isDraggingBrightness);
  PaintSlider(dc, monitor, SliderKind::dim, layout.dim, isDraggingDim);
}

void App::PaintSlider(HDC dc, const Monitor& monitor, SliderKind kind, const SliderLayout& slider, bool isDragging) const {
  const bool isEnabled = IsSliderEnabled(monitor, kind);
  const int maximum = SliderMax(monitor, kind);
  const int cap = SliderCap(monitor, kind);
  const int value = SliderValue(monitor, kind);
  const int tickInner = Scale(kTickInner);
  const int tickOuter = Scale(kTickOuter);
  const int tickHeight = std::max(1, Scale(1));
  for (int tickValue = 0; tickValue <= maximum; tickValue += kClickStep) {
    const int y = ValueToY(slider, tickValue, maximum);
    const RECT leftTick{slider.centerX - tickOuter, y, slider.centerX - tickInner, y + tickHeight};
    const RECT rightTick{slider.centerX + tickInner, y, slider.centerX + tickOuter, y + tickHeight};
    const COLORREF tickColor = tickValue > cap ? kColorTickLocked : kColorTick;
    FillColor(dc, leftTick, tickColor);
    FillColor(dc, rightTick, tickColor);
  }
  const int trackHalfWidth = Scale(kTrackHalfWidth);
  const int capY = ValueToY(slider, cap, maximum);
  const RECT track{slider.centerX - trackHalfWidth, capY, slider.centerX + trackHalfWidth, slider.trackBottom};
  const RECT lockedTrack{track.left, slider.trackTop, track.right, capY};
  FillColor(dc, track, kColorTrack);
  FillColor(dc, lockedTrack, kColorTrackLocked);
  if (isEnabled) {
    const int thumbCenterY = ValueToY(slider, value, maximum);
    const RECT trackFill{track.left, thumbCenterY, track.right, track.bottom};
    const RECT thumb = ThumbRect(slider, value, maximum);
    const COLORREF thumbColor = isDragging ? kColorThumbActive : kColorThumb;
    const int thumbRadius = Scale(kThumbRadius);
    FillColor(dc, trackFill, kColorTrackFill);
    FillRound(dc, thumb, thumbColor, thumbColor, thumbRadius);
  }
  const int valueRadius = Scale(kValueRadius);
  FillRound(dc, slider.value, kColorValue, kColorValueBorder, valueRadius);
  std::wstring valueText = L"\x2013";
  if (isEnabled) valueText = std::to_wstring(value);
  const wchar_t* labelText = SliderLabel(monitor, kind);
  DrawLabel(dc, valueText, slider.value, valueFont_, kColorText, DT_CENTER);
  DrawLabel(dc, labelText, slider.label, labelFont_, kColorTextDim, DT_CENTER);
}

void App::PaintFooter(HDC dc, const RECT& footer) const {
  const int footerRadius = Scale(kFooterRadius);
  const wchar_t* text = isGammaRangeUnlocked_ ? L"Restart Windows to dim further" : L"Unlock stronger dimming";
  const COLORREF textColor = isGammaRangeUnlocked_ ? kColorTextDim : kColorText;
  FillRound(dc, footer, kColorFooter, kColorFooterBorder, footerRadius);
  DrawLabel(dc, text, footer, labelFont_, textColor, DT_CENTER);
}

const wchar_t* App::SliderLabel(const Monitor& monitor, SliderKind kind) {
  switch (kind) {
    case SliderKind::brightness:
      return monitor.ddc == DdcState::unsupported ? L"No DDC/CI" : L"Brightness";
    case SliderKind::dim:
      return L"Dim";
  }
  return L"";
}

void App::FillColor(HDC dc, const RECT& rect, COLORREF color) {
  SetDCBrushColor(dc, color);
  FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

void App::FillRound(HDC dc, const RECT& rect, COLORREF fill, COLORREF border, int radius) {
  SetDCBrushColor(dc, fill);
  SetDCPenColor(dc, border);
  RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
}

void App::DrawLabel(HDC dc, std::wstring_view text, RECT rect, HFONT font, COLORREF color, UINT alignment) {
  SelectObject(dc, font);
  SetTextColor(dc, color);
  const UINT format = alignment | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX;
  DrawTextW(dc, text.data(), static_cast<int>(text.size()), &rect, format);
}

void App::EnsureBackBuffer(HDC target, int width, int height) {
  if (backDc_ && backSize_.cx == width && backSize_.cy == height) return;
  ReleaseBackBuffer();
  backDc_ = CreateCompatibleDC(target);
  backBitmap_ = CreateCompatibleBitmap(target, width, height);
  backPreviousBitmap_ = SelectObject(backDc_, backBitmap_);
  SelectObject(backDc_, GetStockObject(DC_BRUSH));
  SelectObject(backDc_, GetStockObject(DC_PEN));
  SetBkMode(backDc_, TRANSPARENT);
  backSize_ = {width, height};
}

void App::ReleaseBackBuffer() {
  if (!backDc_) return;
  SelectObject(backDc_, backPreviousBitmap_);
  DeleteObject(backBitmap_);
  DeleteDC(backDc_);
  backDc_ = nullptr;
}

void App::CreateFonts() {
  DestroyFonts();
  titleFont_ = CreateUiFont(12, FW_NORMAL, L"Segoe UI");
  nameFont_ = CreateUiFont(12, FW_SEMIBOLD, L"Segoe UI");
  valueFont_ = CreateUiFont(13, FW_NORMAL, L"Segoe UI");
  labelFont_ = CreateUiFont(10, FW_NORMAL, L"Segoe UI");
  iconFont_ = CreateUiFont(10, FW_NORMAL, L"Segoe MDL2 Assets");
}

void App::DestroyFonts() {
  if (titleFont_) DeleteObject(titleFont_);
  if (nameFont_) DeleteObject(nameFont_);
  if (valueFont_) DeleteObject(valueFont_);
  if (labelFont_) DeleteObject(labelFont_);
  if (iconFont_) DeleteObject(iconFont_);
}

HFONT App::CreateUiFont(int size, int weight, const wchar_t* face) const {
  const int height = -Scale(size);
  return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}

}  // namespace yami

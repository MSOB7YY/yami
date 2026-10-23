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
  if (isHelpVisible_) {
    PaintHelp(backDc_);
  } else {
    for (int i = 0; i < static_cast<int>(monitors_.size()); ++i) {
      const ColumnLayout layout = LayoutColumn(i);
      if (!IntersectRect(&overlap, &layout.box, &dirty)) continue;
      PaintColumn(backDc_, monitors_[i], layout, i);
    }
    if (HasStrip()) {
      const StripLayout strip = LayoutStrip();
      if (IntersectRect(&overlap, &strip.box, &dirty)) PaintStrip(backDc_, strip);
    }
    if (HasFooter()) {
      const RECT footer = FooterRect();
      if (IntersectRect(&overlap, &footer, &dirty)) PaintFooter(backDc_, footer);
    }
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
  const RECT close = TitleButtonRect(TitleButton::close);
  const bool isCloseHot = hotButton_ == TitleButton::close;
  const bool isCloseActive = isCloseHot || isClosePressed_;
  COLORREF closeBackground = kColorBackground;
  if (isCloseHot) closeBackground = kColorCloseHot;
  if (isClosePressed_) closeBackground = kColorClosePressed;
  const COLORREF glyphColor = isCloseActive ? kColorCloseGlyphHot : kColorText;
  const int buttonRadius = Scale(kTitleButtonRadius);
  FillRound(dc, close, closeBackground, closeBackground, buttonRadius);
  DrawGlyph(dc, closeGlyph_, close, glyphColor);
  const bool isAutoOn = autoSource_ != AutoSource::off;
  const Glyph& autoGlyph = autoSource_ == AutoSource::time ? clockGlyph_ : lightGlyph_;
  PaintIconButton(dc, TitleButton::autoAdjust, autoGlyph, isAutoMenuOpen_, isAutoOn);
  PaintIconButton(dc, TitleButton::help, helpGlyph_, isHelpVisible_, isHelpVisible_);
}

void App::PaintIconButton(HDC dc, TitleButton button, const Glyph& glyph, bool isPressed, bool isLit) const {
  const RECT rect = TitleButtonRect(button);
  COLORREF background = kColorBackground;
  if (hotButton_ == button) background = kColorTitleButtonHot;
  if (isPressed) background = kColorTitleButtonPressed;
  const COLORREF glyphColor = isLit ? kColorText : kColorTextDim;
  const int buttonRadius = Scale(kTitleButtonRadius);
  FillRound(dc, rect, background, background, buttonRadius);
  DrawGlyph(dc, glyph, rect, glyphColor);
}

void App::PaintHelp(HDC dc) const {
  const int clientWidth = ClientWidth();
  const int clientHeight = ClientHeight();
  const int titleHeight = Scale(kTitleHeight);
  const int maxScroll = HelpMaxScroll();
  const int scroll = std::min(helpScroll_, maxScroll);
  const int contentTop = Scale(kBoxTop) - scroll;
  const int cardLeft = Scale(kPad);
  const int cardRight = clientWidth - cardLeft;
  const int headerLeft = cardLeft + Scale(kHelpHeaderInset);
  const int rowLeft = cardLeft + Scale(kHelpRowInset);
  const int rowRight = cardRight - Scale(kHelpRowInset);
  const int keysRight = rowLeft + Scale(kHelpKeysWidth);
  const int actionLeft = keysRight + Scale(kHelpGap);
  const int cardRadius = Scale(kBoxRadius);
  IntersectClipRect(dc, 0, titleHeight, clientWidth, clientHeight);
  int offset = 0;
  for (size_t i = 0; i < std::size(kHelpSections); ++i) {
    const HelpSection& section = kHelpSections[i];
    if (i > 0) offset += kHelpSectionGap;
    const int headerTop = contentTop + Scale(offset);
    const int headerBottom = contentTop + Scale(offset + kHelpHeaderHeight);
    const RECT header{headerLeft, headerTop, rowRight, headerBottom};
    DrawLabel(dc, section.title, header, nameFont_, kColorText, DT_LEFT);
    offset += kHelpHeaderHeight;
    const int rowsHeight = static_cast<int>(section.rows.size()) * kHelpRowHeight;
    const int cardHeight = 2 * kHelpCardPad + rowsHeight;
    const int cardTop = contentTop + Scale(offset);
    const int cardBottom = contentTop + Scale(offset + cardHeight);
    const RECT card{cardLeft, cardTop, cardRight, cardBottom};
    FillRound(dc, card, kColorColumn, kColorColumn, cardRadius);
    int rowOffset = offset + kHelpCardPad;
    for (const HelpRow& row : section.rows) {
      const int top = contentTop + Scale(rowOffset);
      const int bottom = contentTop + Scale(rowOffset + kHelpRowHeight);
      const RECT keys{rowLeft, top, keysRight, bottom};
      const RECT action{actionLeft, top, rowRight, bottom};
      DrawLabel(dc, row.keys, keys, smallFont_, kColorText, DT_LEFT);
      DrawLabel(dc, row.action, action, smallFont_, kColorTextDim, DT_LEFT);
      rowOffset += kHelpRowHeight;
    }
    offset += cardHeight;
  }
  offset += kHelpVersionGap;
  const int versionTop = contentTop + Scale(offset);
  const int versionBottom = contentTop + Scale(offset + kHelpVersionHeight);
  const RECT version{cardLeft, versionTop, cardRight, versionBottom};
  DrawLabel(dc, kVersionText, version, labelFont_, kColorTextDim, DT_CENTER);
  SelectClipRgn(dc, nullptr);
  if (maxScroll > 0) PaintHelpScrollbar(dc, scroll, maxScroll);
}

void App::PaintHelpScrollbar(HDC dc, int scroll, int maxScroll) const {
  const int clientWidth = ClientWidth();
  const int clientHeight = ClientHeight();
  const int titleHeight = Scale(kTitleHeight);
  const int inset = Scale(kHelpScrollbarInset);
  const int width = Scale(kHelpScrollbarWidth);
  const int viewHeight = clientHeight - titleHeight;
  const int trackTop = titleHeight + inset;
  const int trackSpan = clientHeight - inset - trackTop;
  const int thumbHeight = MulDiv(trackSpan, viewHeight, viewHeight + maxScroll);
  const int thumbTop = trackTop + MulDiv(trackSpan - thumbHeight, scroll, maxScroll);
  const int right = clientWidth - inset;
  const RECT thumb{right - width, thumbTop, right, thumbTop + thumbHeight};
  FillRound(dc, thumb, kColorTrack, kColorTrack, width);
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

void App::PaintStrip(HDC dc, const StripLayout& strip) const {
  const int boxRadius = Scale(kBoxRadius);
  FillRound(dc, strip.box, kColorColumn, kColorColumn, boxRadius);
  const bool isLight = autoSource_ == AutoSource::light;
  const Glyph& glyph = isLight ? lightGlyph_ : clockGlyph_;
  DrawGlyph(dc, glyph, strip.glyph, kColorTextDim);
  const float condition = AutoCondition();
  if (condition >= 0) {
    const std::wstring valueText = FormatCondition(condition);
    DrawLabel(dc, valueText, strip.value, smallBoldFont_, kColorText, DT_LEFT);
  } else {
    const bool hasSensor = lightSensor_ && lightSensor_->IsAvailable();
    const wchar_t* message = hasSensor ? L"Reading sensor" : L"No sensor found";
    const RECT messageRect{strip.value.left, strip.value.top, strip.now.right, strip.value.bottom};
    DrawLabel(dc, message, messageRect, labelFont_, kColorTextDim, DT_LEFT);
  }
  if (IsPreviewing()) {
    const int nowRadius = Scale(kValueRadius);
    FillRound(dc, strip.now, kColorValue, kColorValueBorder, nowRadius);
    DrawLabel(dc, L"Now", strip.now, labelFont_, kColorText, DT_CENTER);
  }
  PaintStripTrack(dc, strip, condition);
}

void App::PaintStripTrack(HDC dc, const StripLayout& strip, float condition) const {
  const bool hasCondition = condition >= 0;
  const int trackHalfWidth = Scale(kTrackHalfWidth);
  const RECT track{strip.trackLeft, strip.trackY - trackHalfWidth, strip.trackRight, strip.trackY + trackHalfWidth};
  const COLORREF trackColor = hasCondition ? kColorTrack : kColorTrackLocked;
  FillColor(dc, track, trackColor);
  const int tickCount = IsCircularAxis() ? kTimeTickCount : kLightTickCount;
  const int tickTop = strip.trackY + Scale(kStripTickTop);
  const int tickBottom = tickTop + Scale(kStripTickHeight);
  const int tickWidth = std::max(1, Scale(1));
  for (int i = 0; i <= tickCount; ++i) {
    const float tickCondition = static_cast<float>(i) / tickCount;
    const int x = ConditionToX(strip, tickCondition);
    const RECT tick{x, tickTop, x + tickWidth, tickBottom};
    FillColor(dc, tick, kColorTick);
  }
  if (!hasCondition) return;
  const size_t curveIndex = CurveIndex();
  const int markerWidth = Scale(kStripMarkerWidth);
  const int markerHalfHeight = Scale(kStripMarkerHeight) / 2;
  for (const Monitor& monitor : monitors_) {
    const CalibrationCurve& curve = monitor.curves[curveIndex];
    for (size_t i = 0; i < curve.Count(); ++i) {
      const float markerCondition = curve.ConditionAt(i);
      const int x = ConditionToX(strip, markerCondition);
      const int markerLeft = x - markerWidth / 2;
      const RECT marker{markerLeft, strip.trackY - markerHalfHeight, markerLeft + markerWidth, strip.trackY + markerHalfHeight};
      FillColor(dc, marker, kColorTextDim);
    }
  }
  if (IsPreviewing() && liveCondition_ >= 0) {
    const int liveX = ConditionToX(strip, liveCondition_);
    const int dotHalf = Scale(kStripDotSize) / 2;
    const RECT dot{liveX - dotHalf, strip.trackY - dotHalf, liveX + dotHalf, strip.trackY + dotHalf};
    FillRound(dc, dot, kColorText, kColorText, dotHalf * 2);
  }
  const int thumbX = ConditionToX(strip, condition);
  const int thumbHalfWidth = Scale(kStripThumbWidth) / 2;
  const int thumbHalfHeight = Scale(kStripThumbHeight) / 2;
  const RECT thumb{thumbX - thumbHalfWidth, strip.trackY - thumbHalfHeight, thumbX + thumbHalfWidth, strip.trackY + thumbHalfHeight};
  const COLORREF thumbColor = isDraggingPreview_ ? kColorThumbActive : kColorThumb;
  const int thumbRadius = Scale(kThumbRadius);
  FillRound(dc, thumb, thumbColor, thumbColor, thumbRadius);
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

void App::DrawGlyph(HDC dc, const Glyph& glyph, const RECT& rect, COLORREF color) {
  if (!glyph.font) return;
  SelectObject(dc, glyph.font);
  SetTextColor(dc, color);
  const int centerX = (rect.left + rect.right) * (kGlyphSubpixels / 2);
  const int centerY = (rect.top + rect.bottom) * (kGlyphSubpixels / 2);
  const int x = (centerX - glyph.inkCenterInSubpixels.x + kGlyphSubpixels / 2) >> kGlyphSubpixelShift;
  const int y = (centerY - glyph.inkCenterInSubpixels.y + kGlyphSubpixels / 2) >> kGlyphSubpixelShift;
  ExtTextOutW(dc, x, y, 0, nullptr, &glyph.code, 1, nullptr);
}

App::Glyph App::MeasureGlyph(HDC dc, wchar_t code, HFONT font) {
  SelectObject(dc, font);
  const MAT2 identity{{0, 1}, {0, 0}, {0, 0}, {0, 1}};
  GLYPHMETRICS metrics{};
  const DWORD size = GetGlyphOutlineW(dc, code, GGO_GRAY8_BITMAP, &metrics, 0, nullptr, &identity);
  if (size == GDI_ERROR || size == 0) return {};
  std::vector<BYTE> coverage(size);
  GetGlyphOutlineW(dc, code, GGO_GRAY8_BITMAP, &metrics, size, coverage.data(), &identity);
  TEXTMETRICW textMetrics{};
  GetTextMetricsW(dc, &textMetrics);
  const int width = static_cast<int>(metrics.gmBlackBoxX);
  const int height = static_cast<int>(metrics.gmBlackBoxY);
  const int pitch = (width + 3) & ~3;
  const int lastRow = (height - 1) * pitch;
  int topCoverage = 0;
  int bottomCoverage = 0;
  for (int x = 0; x < width; ++x) {
    topCoverage = std::max<int>(topCoverage, coverage[x]);
    bottomCoverage = std::max<int>(bottomCoverage, coverage[lastRow + x]);
  }
  int leftCoverage = 0;
  int rightCoverage = 0;
  for (int row = 0; row <= lastRow; row += pitch) {
    leftCoverage = std::max<int>(leftCoverage, coverage[row]);
    rightCoverage = std::max<int>(rightCoverage, coverage[row + width - 1]);
  }
  const int inkLeft = kGlyphSubpixels - leftCoverage;
  const int inkRight = (width - 1) * kGlyphSubpixels + rightCoverage;
  const int inkTop = kGlyphSubpixels - topCoverage;
  const int inkBottom = (height - 1) * kGlyphSubpixels + bottomCoverage;
  const int boxLeft = metrics.gmptGlyphOrigin.x * kGlyphSubpixels;
  const int boxTop = (textMetrics.tmAscent - metrics.gmptGlyphOrigin.y) * kGlyphSubpixels;
  const int inkCenterX = boxLeft + (inkLeft + inkRight) / 2;
  const int inkCenterY = boxTop + (inkTop + inkBottom) / 2;
  return {font, code, {inkCenterX, inkCenterY}};
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
  nameFont_ = CreateUiFont(12, FW_SEMIBOLD, L"Segoe UI Semibold");
  valueFont_ = CreateUiFont(13, FW_NORMAL, L"Segoe UI");
  labelFont_ = CreateUiFont(10, FW_NORMAL, L"Segoe UI");
  iconFont_ = CreateUiFont(10, FW_NORMAL, L"Segoe MDL2 Assets");
  glyphFont_ = CreateUiFont(12, FW_NORMAL, L"Segoe MDL2 Assets");
  smallFont_ = CreateUiFont(11, FW_NORMAL, L"Segoe UI");
  smallBoldFont_ = CreateUiFont(11, FW_SEMIBOLD, L"Segoe UI Semibold");
  const HDC measureDc = CreateCompatibleDC(nullptr);
  closeGlyph_ = MeasureGlyph(measureDc, kCloseGlyph, iconFont_);
  lightGlyph_ = MeasureGlyph(measureDc, kLightGlyph, glyphFont_);
  clockGlyph_ = MeasureGlyph(measureDc, kClockGlyph, glyphFont_);
  helpGlyph_ = MeasureGlyph(measureDc, kHelpGlyph, glyphFont_);
  DeleteDC(measureDc);
}

void App::DestroyFonts() {
  if (titleFont_) DeleteObject(titleFont_);
  if (nameFont_) DeleteObject(nameFont_);
  if (valueFont_) DeleteObject(valueFont_);
  if (labelFont_) DeleteObject(labelFont_);
  if (iconFont_) DeleteObject(iconFont_);
  if (glyphFont_) DeleteObject(glyphFont_);
  if (smallFont_) DeleteObject(smallFont_);
  if (smallBoldFont_) DeleteObject(smallBoldFont_);
}

HFONT App::CreateUiFont(int size, int weight, const wchar_t* face) const {
  const int height = -Scale(size);
  return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}

}  // namespace yami

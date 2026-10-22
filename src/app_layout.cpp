// by claude

#include <algorithm>
#include <cmath>

#include "app.h"

namespace yami {

int App::Scale(int value) const { return MulDiv(value, static_cast<int>(dpi_), 96); }

bool App::HasFooter() const { return dimCap_ < kMaxDim; }

int App::ColumnsWidth() const {
  const int count = static_cast<int>(monitors_.size());
  if (count == 0) return 0;
  const int columnWidth = Scale(kColumnWidth);
  const int columnGap = Scale(kColumnGap);
  return count * columnWidth + (count - 1) * columnGap;
}

int App::ClientWidth() const {
  const int minWidth = Scale(kMinClientWidth);
  const int columnsWidth = ColumnsWidth() + 2 * Scale(kPad);
  return std::max(minWidth, columnsWidth);
}

int App::ClientHeight() const {
  int contentHeight = kBoxTop + kBoxHeight;
  if (HasFooter()) contentHeight += kFooterGap + kFooterHeight;
  return Scale(contentHeight + kPad);
}

RECT App::TitleRect() const {
  const int clientWidth = ClientWidth();
  const int titleHeight = Scale(kTitleHeight);
  return {0, 0, clientWidth, titleHeight};
}

RECT App::CloseRect() const {
  const int clientWidth = ClientWidth();
  const int closeWidth = Scale(kCloseWidth);
  const int titleHeight = Scale(kTitleHeight);
  return {clientWidth - closeWidth, 0, clientWidth, titleHeight};
}

RECT App::FooterRect() const {
  const int pad = Scale(kPad);
  const int clientWidth = ClientWidth();
  const int top = Scale(kBoxTop + kBoxHeight + kFooterGap);
  const int bottom = top + Scale(kFooterHeight);
  return {pad, top, clientWidth - pad, bottom};
}

App::ColumnLayout App::LayoutColumn(int index) const {
  const int columnWidth = Scale(kColumnWidth);
  const int columnGap = Scale(kColumnGap);
  const int clientWidth = ClientWidth();
  const int columnsWidth = ColumnsWidth();
  const int columnsLeft = (clientWidth - columnsWidth) / 2;
  const int left = columnsLeft + index * (columnWidth + columnGap);
  const int right = left + columnWidth;
  const int top = Scale(kBoxTop);
  const int bottom = top + Scale(kBoxHeight);
  const int nameInset = Scale(kNameInset);
  const int nameTop = top + Scale(kNameTop);
  const int nameBottom = nameTop + Scale(kNameHeight);
  const int sliderWidth = Scale(kSliderWidth);
  const int brightnessCenterX = left + Scale(kColumnInset) + sliderWidth / 2;
  const int dimCenterX = brightnessCenterX + sliderWidth;
  ColumnLayout layout{};
  layout.box = {left, top, right, bottom};
  layout.name = {left + nameInset, nameTop, right - nameInset, nameBottom};
  layout.brightness = LayoutSlider(top, brightnessCenterX);
  layout.dim = LayoutSlider(top, dimCenterX);
  return layout;
}

App::SliderLayout App::LayoutSlider(int boxTop, int centerX) const {
  const int trackTop = boxTop + Scale(kTrackOffset);
  const int trackBottom = trackTop + Scale(kTrackHeight);
  const int valueTop = trackBottom + Scale(kThumbHeight / 2 + kValueGap);
  const int valueBottom = valueTop + Scale(kValueHeight);
  const int valueHalfWidth = Scale(kValueWidth) / 2;
  const int labelHalfWidth = Scale(kSliderWidth) / 2;
  const int labelTop = valueBottom + Scale(kLabelGap);
  const int labelBottom = labelTop + Scale(kLabelHeight);
  SliderLayout slider{};
  slider.centerX = centerX;
  slider.trackTop = trackTop;
  slider.trackBottom = trackBottom;
  slider.value = {centerX - valueHalfWidth, valueTop, centerX + valueHalfWidth, valueBottom};
  slider.label = {centerX - labelHalfWidth, labelTop, centerX + labelHalfWidth, labelBottom};
  return slider;
}

const App::SliderLayout& App::SliderOf(const ColumnLayout& layout, SliderKind kind) {
  switch (kind) {
    case SliderKind::brightness:
      return layout.brightness;
    case SliderKind::dim:
      return layout.dim;
  }
  return layout.dim;
}

int App::SliderValue(const Monitor& monitor, SliderKind kind) {
  switch (kind) {
    case SliderKind::brightness:
      return monitor.brightness;
    case SliderKind::dim:
      return monitor.dim;
  }
  return 0;
}

int App::SliderMax(const Monitor& monitor, SliderKind kind) {
  switch (kind) {
    case SliderKind::brightness:
      return monitor.maxBrightness > 0 ? monitor.maxBrightness : kDefaultMaxBrightness;
    case SliderKind::dim:
      return kMaxDim;
  }
  return kMaxDim;
}

int App::SliderCap(const Monitor& monitor, SliderKind kind) const {
  switch (kind) {
    case SliderKind::brightness:
      return SliderMax(monitor, kind);
    case SliderKind::dim:
      return dimCap_;
  }
  return 0;
}

bool App::IsSliderEnabled(const Monitor& monitor, SliderKind kind) {
  switch (kind) {
    case SliderKind::brightness:
      return monitor.ddc == DdcState::supported;
    case SliderKind::dim:
      return monitor.dc != nullptr;
  }
  return false;
}

int App::ValueToY(const SliderLayout& slider, int value, int maximum) {
  const int span = slider.trackBottom - slider.trackTop;
  return slider.trackBottom - MulDiv(value, span, maximum);
}

int App::ValueFromY(const SliderLayout& slider, int y, int maximum) {
  const int span = slider.trackBottom - slider.trackTop;
  const int fromBottom = slider.trackBottom - y;
  const long value = std::lround(static_cast<double>(fromBottom) * maximum / span);
  return static_cast<int>(value);
}

RECT App::ThumbRect(const SliderLayout& slider, int value, int maximum) const {
  const int centerY = ValueToY(slider, value, maximum);
  const int halfWidth = Scale(kThumbWidth) / 2;
  const int halfHeight = Scale(kThumbHeight) / 2;
  return {slider.centerX - halfWidth, centerY - halfHeight, slider.centerX + halfWidth, centerY + halfHeight};
}

App::SliderHit App::HitSlider(POINT point) const {
  const int halfWidth = Scale(kSliderWidth) / 2;
  for (int i = 0; i < static_cast<int>(monitors_.size()); ++i) {
    const ColumnLayout layout = LayoutColumn(i);
    if (!PtInRect(&layout.box, point)) continue;
    const bool isOnBrightness = point.x < layout.brightness.centerX + halfWidth;
    const SliderKind kind = isOnBrightness ? SliderKind::brightness : SliderKind::dim;
    return {i, kind};
  }
  return {-1, SliderKind::dim};
}

}  // namespace yami

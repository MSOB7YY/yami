// by claude

#include <windowsx.h>

#include "app.h"

namespace yami {

POINT App::PointFromLParam(LPARAM lParam) { return {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}; }

LRESULT App::HitTest(LPARAM lParam) const {
  POINT point = PointFromLParam(lParam);
  ScreenToClient(hwnd_, &point);
  const TitleButton button = HitTitleButton(point);
  if (button != TitleButton::none) return HTCLIENT;
  const RECT title = TitleRect();
  if (PtInRect(&title, point)) return HTCAPTION;
  return HTCLIENT;
}

void App::OnMouseDown(LPARAM lParam) {
  const POINT point = PointFromLParam(lParam);
  const TitleButton button = HitTitleButton(point);
  switch (button) {
    case TitleButton::help:
      ToggleHelp();
      return;
    case TitleButton::autoAdjust:
      ShowAutoMenu();
      return;
    case TitleButton::close:
      isClosePressed_ = true;
      SetCapture(hwnd_);
      InvalidateTitle();
      return;
    case TitleButton::none:
      break;
  }
  if (isHelpVisible_) return;
  if (HasFooter()) {
    const RECT footer = FooterRect();
    if (PtInRect(&footer, point)) {
      if (!isGammaRangeUnlocked_) StartGammaUnlock();
      return;
    }
  }
  if (HasStrip()) {
    const StripLayout strip = LayoutStrip();
    if (PtInRect(&strip.box, point)) {
      OnStripMouseDown(strip, point);
      return;
    }
  }
  const SliderHit hit = HitSlider(point);
  if (hit.monitor < 0) return;
  const Monitor& monitor = monitors_[hit.monitor];
  if (!IsSliderEnabled(monitor, hit.kind)) return;
  const ColumnLayout layout = LayoutColumn(hit.monitor);
  const SliderLayout& slider = SliderOf(layout, hit.kind);
  const int maximum = SliderMax(monitor, hit.kind);
  const int value = SliderValue(monitor, hit.kind);
  const RECT thumb = ThumbRect(slider, value, maximum);
  const int thumbCenterY = ValueToY(slider, value, maximum);
  if (PtInRect(&thumb, point)) {
    dragMonitor_ = hit.monitor;
    dragKind_ = hit.kind;
    dragOffset_ = point.y - thumbCenterY;
    SetCapture(hwnd_);
    InvalidateColumn(hit.monitor);
    return;
  }
  const int reach = Scale(kThumbHeight);
  const bool isOnTrack = point.y >= slider.trackTop - reach && point.y <= slider.trackBottom + reach;
  if (!isOnTrack) return;
  const int step = point.y < thumbCenterY ? kClickStep : -kClickStep;
  SetSliderValue(hit.monitor, hit.kind, value + step);
}

void App::OnMouseMove(LPARAM lParam) {
  const POINT point = PointFromLParam(lParam);
  UpdateTitleHover(point);
  if (isDraggingPreview_) {
    const StripLayout strip = LayoutStrip();
    const float condition = ConditionAtX(strip, point.x);
    const float snapped = SnapCondition(condition);
    SetPreview(snapped);
    return;
  }
  if (dragMonitor_ < 0) return;
  const Monitor& monitor = monitors_[dragMonitor_];
  const ColumnLayout layout = LayoutColumn(dragMonitor_);
  const SliderLayout& slider = SliderOf(layout, dragKind_);
  const int maximum = SliderMax(monitor, dragKind_);
  const int value = ValueFromY(slider, point.y - dragOffset_, maximum);
  SetSliderValue(dragMonitor_, dragKind_, value);
}

void App::OnMouseUp(LPARAM lParam) {
  if (isClosePressed_) {
    const POINT point = PointFromLParam(lParam);
    const TitleButton button = HitTitleButton(point);
    const bool shouldHide = button == TitleButton::close;
    ReleaseCapture();
    if (shouldHide) HideWindow();
    return;
  }
  if (dragMonitor_ >= 0 || isDraggingPreview_) ReleaseCapture();
}

void App::OnMouseLeave() {
  isTrackingLeave_ = false;
  if (hotButton_ == TitleButton::none) return;
  hotButton_ = TitleButton::none;
  InvalidateTitle();
}

void App::OnCaptureChanged() {
  if (isClosePressed_) {
    isClosePressed_ = false;
    InvalidateTitle();
  }
  if (isDraggingPreview_) {
    isDraggingPreview_ = false;
    InvalidateStrip();
  }
  if (dragMonitor_ < 0) return;
  const int index = dragMonitor_;
  dragMonitor_ = -1;
  InvalidateColumn(index);
  if (!didDragChange_) return;
  didDragChange_ = false;
  RecordLevels(index);
}

void App::UpdateTitleHover(POINT point) {
  const TitleButton hot = HitTitleButton(point);
  if (hot != TitleButton::none && !isTrackingLeave_) {
    TRACKMOUSEEVENT track{};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = hwnd_;
    isTrackingLeave_ = TrackMouseEvent(&track);
  }
  if (hot == hotButton_) return;
  hotButton_ = hot;
  InvalidateTitle();
}

void App::OnMouseWheel(WPARAM wParam, LPARAM lParam) {
  if (isHelpVisible_) {
    const int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    ScrollHelp(wheelDelta);
    return;
  }
  POINT point = PointFromLParam(lParam);
  ScreenToClient(hwnd_, &point);
  if (HasStrip()) {
    const StripLayout strip = LayoutStrip();
    if (PtInRect(&strip.box, point)) {
      const int steps = TakeWheelSteps(wParam);
      if (steps != 0) StepPreview(steps);
      return;
    }
  }
  const SliderHit hit = HitSlider(point);
  if (hit.monitor < 0) return;
  const Monitor& monitor = monitors_[hit.monitor];
  if (!IsSliderEnabled(monitor, hit.kind)) return;
  const int steps = TakeWheelSteps(wParam);
  if (steps == 0) return;
  const int value = SliderValue(monitor, hit.kind) + steps;
  SetSliderValue(hit.monitor, hit.kind, value);
}

int App::TakeWheelSteps(WPARAM wParam) {
  wheelRemainder_ += GET_WHEEL_DELTA_WPARAM(wParam);
  const int steps = wheelRemainder_ / WHEEL_DELTA;
  wheelRemainder_ -= steps * WHEEL_DELTA;
  return steps;
}

void App::OnStripMouseDown(const StripLayout& strip, POINT point) {
  if (AutoCondition() < 0) return;
  if (IsPreviewing() && PtInRect(&strip.now, point)) {
    ExitPreview();
    return;
  }
  if (!IsOnStripTrack(strip, point)) return;
  isDraggingPreview_ = true;
  SetCapture(hwnd_);
  const float condition = ConditionAtX(strip, point.x);
  const float snapped = SnapCondition(condition);
  SetPreview(snapped);
  InvalidateStrip();
}

void App::OnRightClick(LPARAM lParam) {
  if (isHelpVisible_ || !HasStrip() || AutoCondition() < 0) return;
  const POINT point = PointFromLParam(lParam);
  const StripLayout strip = LayoutStrip();
  if (!IsOnStripTrack(strip, point)) return;
  const float condition = ConditionAtX(strip, point.x);
  const int span = strip.trackRight - strip.trackLeft;
  const int reach = Scale(kStripMarkerReach);
  const float radius = static_cast<float>(reach) / span;
  RemoveCalibrationNear(condition, radius);
}

void App::InvalidateColumn(int index) const {
  const RECT box = LayoutColumn(index).box;
  InvalidateRect(hwnd_, &box, FALSE);
}

void App::InvalidateTitle() const {
  const RECT title = TitleRect();
  InvalidateRect(hwnd_, &title, FALSE);
}

void App::InvalidateStrip() const {
  if (!HasStrip()) return;
  const RECT box = LayoutStrip().box;
  InvalidateRect(hwnd_, &box, FALSE);
}

}  // namespace yami

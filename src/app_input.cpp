// by claude

#include <windowsx.h>

#include "app.h"

namespace yami {

POINT App::PointFromLParam(LPARAM lParam) { return {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}; }

LRESULT App::HitTest(LPARAM lParam) const {
  POINT point = PointFromLParam(lParam);
  ScreenToClient(hwnd_, &point);
  const RECT close = CloseRect();
  if (PtInRect(&close, point)) return HTCLIENT;
  const RECT title = TitleRect();
  if (PtInRect(&title, point)) return HTCAPTION;
  return HTCLIENT;
}

void App::OnMouseDown(LPARAM lParam) {
  const POINT point = PointFromLParam(lParam);
  const RECT close = CloseRect();
  if (PtInRect(&close, point)) {
    isClosePressed_ = true;
    SetCapture(hwnd_);
    InvalidateClose();
    return;
  }
  if (HasFooter()) {
    const RECT footer = FooterRect();
    if (PtInRect(&footer, point)) {
      if (!isGammaRangeUnlocked_) StartGammaUnlock();
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
  UpdateCloseHover(point);
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
    const RECT close = CloseRect();
    const bool shouldHide = PtInRect(&close, point);
    ReleaseCapture();
    if (shouldHide) HideWindow();
    return;
  }
  if (dragMonitor_ >= 0) ReleaseCapture();
}

void App::OnMouseLeave() {
  isTrackingLeave_ = false;
  if (!isCloseHot_) return;
  isCloseHot_ = false;
  InvalidateClose();
}

void App::OnCaptureChanged() {
  if (isClosePressed_) {
    isClosePressed_ = false;
    InvalidateClose();
  }
  if (dragMonitor_ < 0) return;
  const int index = dragMonitor_;
  dragMonitor_ = -1;
  InvalidateColumn(index);
}

void App::UpdateCloseHover(POINT point) {
  const RECT close = CloseRect();
  const bool isHot = PtInRect(&close, point);
  if (isHot && !isTrackingLeave_) {
    TRACKMOUSEEVENT track{};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = hwnd_;
    isTrackingLeave_ = TrackMouseEvent(&track);
  }
  if (isHot == isCloseHot_) return;
  isCloseHot_ = isHot;
  InvalidateClose();
}

void App::OnMouseWheel(WPARAM wParam, LPARAM lParam) {
  POINT point = PointFromLParam(lParam);
  ScreenToClient(hwnd_, &point);
  const SliderHit hit = HitSlider(point);
  if (hit.monitor < 0) return;
  const Monitor& monitor = monitors_[hit.monitor];
  if (!IsSliderEnabled(monitor, hit.kind)) return;
  wheelRemainder_ += GET_WHEEL_DELTA_WPARAM(wParam);
  const int steps = wheelRemainder_ / WHEEL_DELTA;
  wheelRemainder_ -= steps * WHEEL_DELTA;
  if (steps == 0) return;
  const int value = SliderValue(monitor, hit.kind) + steps;
  SetSliderValue(hit.monitor, hit.kind, value);
}

void App::InvalidateColumn(int index) const {
  const RECT box = LayoutColumn(index).box;
  InvalidateRect(hwnd_, &box, FALSE);
}

void App::InvalidateClose() const {
  const RECT close = CloseRect();
  InvalidateRect(hwnd_, &close, FALSE);
}

}  // namespace yami

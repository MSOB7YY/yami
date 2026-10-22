// by claude

#include <algorithm>
#include <cmath>
#include <cwchar>

#include "app.h"

namespace yami {

void App::LoadAutoSource() {
  DWORD value = 0;
  DWORD size = sizeof(value);
  const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, kAppKey, kAutoValue, RRF_RT_REG_DWORD, nullptr, &value, &size);
  const bool isKnown = status == ERROR_SUCCESS && value < kCurveCount;
  autoSource_ = isKnown ? static_cast<AutoSource>(value) : AutoSource::off;
}

void App::SetAutoSource(AutoSource source) {
  if (source == autoSource_) return;
  StopAuto();
  autoSource_ = source;
  const DWORD value = static_cast<DWORD>(source);
  RegSetKeyValueW(HKEY_CURRENT_USER, kAppKey, kAutoValue, REG_DWORD, &value, sizeof(value));
  StartAuto();
  ResizeWindow();
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::StartAuto() {
  switch (autoSource_) {
    case AutoSource::light:
      lightSensor_ = std::make_unique<LightSensor>(hwnd_, kLightReadingMessage, kLightChangedMessage);
      return;
    case AutoSource::time:
      UpdateClock();
      return;
    case AutoSource::off:
      return;
  }
}

void App::StopAuto() {
  if (isDraggingPreview_) ReleaseCapture();
  StopLightSteps();
  KillTimer(hwnd_, kClockTimer);
  lightSensor_.reset();
  liveCondition_ = kNoCondition;
  lightTarget_ = kNoCondition;
  previewCondition_ = kNoCondition;
}

void App::OnLightReading(LPARAM lParam) {
  if (!lightSensor_) return;
  const float lux = LightSensor::UnpackLux(lParam);
  const float condition = LuxToCondition(lux);
  if (liveCondition_ < 0) {
    liveCondition_ = condition;
    lightTarget_ = condition;
    ApplyAuto();
    InvalidateStrip();
    return;
  }
  const float change = std::abs(condition - lightTarget_);
  if (change < kLightHysteresis) return;
  lightTarget_ = condition;
  if (isLightStepping_) return;
  isLightStepping_ = true;
  SetTimer(hwnd_, kLightTimer, kLightStepMS, nullptr);
}

void App::OnLightChanged() {
  if (!lightSensor_) return;
  lightSensor_->Attach();
  if (!lightSensor_->IsAvailable()) {
    StopLightSteps();
    liveCondition_ = kNoCondition;
    lightTarget_ = kNoCondition;
  }
  InvalidateStrip();
}

void App::StepLight() {
  const float remaining = lightTarget_ - liveCondition_;
  const bool isSettled = std::abs(remaining) <= kLightSettle;
  if (isSettled) {
    liveCondition_ = lightTarget_;
    StopLightSteps();
  } else {
    liveCondition_ += remaining * kLightSmoothing;
  }
  if (!IsPreviewing()) ApplyAuto();
  InvalidateStrip();
}

void App::StopLightSteps() {
  if (!isLightStepping_) return;
  isLightStepping_ = false;
  KillTimer(hwnd_, kLightTimer);
}

void App::UpdateClock() {
  SYSTEMTIME now{};
  GetLocalTime(&now);
  const int minutes = now.wHour * 60 + now.wMinute;
  liveCondition_ = static_cast<float>(minutes) / kMinutesPerDay;
  const UINT elapsedMS = now.wSecond * 1000u + now.wMilliseconds;
  const UINT untilNextMinuteMS = kMinuteMS - elapsedMS + kClockSlackMS;
  SetTimer(hwnd_, kClockTimer, untilNextMinuteMS, nullptr);
  if (!IsPreviewing()) ApplyAuto();
  InvalidateStrip();
}

float App::AutoCondition() const { return IsPreviewing() ? previewCondition_ : liveCondition_; }

bool App::IsPreviewing() const { return previewCondition_ >= 0; }

void App::ApplyAuto() {
  bool didChange = false;
  for (int i = 0; i < static_cast<int>(monitors_.size()); ++i) didChange |= ApplyAutoLevels(i);
  if (didChange) ScheduleSave();
}

bool App::ApplyAutoLevels(int index) {
  const float condition = AutoCondition();
  if (condition < 0 || isSuspended_ || index == dragMonitor_) return false;
  const Monitor& monitor = monitors_[index];
  const CalibrationCurve& curve = monitor.curves[CurveIndex()];
  if (curve.IsEmpty()) return false;
  const bool isCircular = IsCircularAxis();
  const CalibrationCurve::Levels levels = curve.Evaluate(condition, isCircular);
  const bool canDim = IsSliderEnabled(monitor, SliderKind::dim);
  const bool canBrighten = levels.brightness != CalibrationCurve::kNoLevel && IsSliderEnabled(monitor, SliderKind::brightness);
  bool didChange = false;
  if (canDim) didChange |= ChangeLevel(index, SliderKind::dim, levels.dim);
  if (canBrighten) didChange |= ChangeLevel(index, SliderKind::brightness, levels.brightness);
  return didChange;
}

void App::RecordLevels(int index) {
  const float condition = AutoCondition();
  if (condition < 0) return;
  Monitor& monitor = monitors_[index];
  const bool hasBrightness = monitor.ddc == DdcState::supported;
  const int brightness = hasBrightness ? monitor.brightness : CalibrationCurve::kNoLevel;
  const float radius = 0.5f / StepCount();
  const bool isCircular = IsCircularAxis();
  CalibrationCurve& curve = monitor.curves[CurveIndex()];
  curve.Record(condition, radius, isCircular, brightness, monitor.dim);
  monitor.hasCurveChanges = true;
  ScheduleSave();
  InvalidateStrip();
}

void App::SetPreview(float condition) {
  if (condition == previewCondition_) return;
  previewCondition_ = condition;
  ApplyAuto();
  InvalidateStrip();
}

void App::StepPreview(int steps) {
  const float condition = AutoCondition();
  if (condition < 0) return;
  const float stepped = StepCondition(condition, steps);
  SetPreview(stepped);
}

void App::ExitPreview() {
  if (!IsPreviewing()) return;
  previewCondition_ = kNoCondition;
  ApplyAuto();
  InvalidateStrip();
}

void App::RemoveCalibrationNear(float condition, float radius) {
  const bool isCircular = IsCircularAxis();
  const size_t curveIndex = CurveIndex();
  bool didRemove = false;
  for (Monitor& monitor : monitors_) {
    const bool didRemoveHere = monitor.curves[curveIndex].RemoveNear(condition, radius, isCircular);
    if (!didRemoveHere) continue;
    monitor.hasCurveChanges = true;
    didRemove = true;
  }
  if (!didRemove) return;
  ScheduleSave();
  ApplyAuto();
  InvalidateStrip();
}

void App::ResetCalibration() {
  const bool isLight = autoSource_ == AutoSource::light;
  const wchar_t* question = isLight ? L"Remove all saved levels for the light sensor?" : L"Remove all saved levels for the time of day?";
  const int answer = MessageBoxW(hwnd_, question, kAppName, MB_YESNO | MB_ICONWARNING);
  if (answer != IDYES) return;
  const size_t curveIndex = CurveIndex();
  for (Monitor& monitor : monitors_) monitor.curves[curveIndex].Clear();
  RegDeleteKeyW(HKEY_CURRENT_USER, kCurvesKeys[curveIndex]);
  InvalidateStrip();
}

size_t App::CurveIndex() const { return static_cast<size_t>(autoSource_); }

bool App::IsCircularAxis() const { return autoSource_ == AutoSource::time; }

int App::StepCount() const { return IsCircularAxis() ? kTimeSteps : kLightSteps; }

float App::SnapCondition(float condition) const {
  const int count = StepCount();
  const int lastIndex = IsCircularAxis() ? count - 1 : count;
  const long nearest = std::lround(condition * count);
  const long clamped = std::clamp(nearest, 0L, static_cast<long>(lastIndex));
  return static_cast<float>(clamped) / count;
}

float App::StepCondition(float condition, int steps) const {
  const int count = StepCount();
  const long nearest = std::lround(condition * count);
  const long index = nearest + steps;
  if (!IsCircularAxis()) {
    const long clamped = std::clamp(index, 0L, static_cast<long>(count));
    return static_cast<float>(clamped) / count;
  }
  const long wrapped = ((index % count) + count) % count;
  return static_cast<float>(wrapped) / count;
}

std::wstring App::FormatCondition(float condition) const {
  if (IsCircularAxis()) return FormatTime(condition);
  const double lux = ConditionToLux(condition);
  return FormatLux(lux);
}

float App::LuxToCondition(float lux) {
  const double clamped = std::clamp(static_cast<double>(lux), 1.0, kMaxLux);
  const double decades = std::log10(clamped);
  return static_cast<float>(decades / kLuxDecades);
}

double App::ConditionToLux(float condition) { return std::pow(10.0, condition * kLuxDecades); }

std::wstring App::FormatLux(double lux) {
  if (lux < 1.05) return L"\x2264 1 lux";
  wchar_t text[24];
  if (lux < 10.0) {
    swprintf_s(text, L"%.1f lux", lux);
    return text;
  }
  if (lux < 10000.0) {
    const double digits = std::floor(std::log10(lux));
    const double magnitude = std::pow(10.0, digits - 1.0);
    const double rounded = std::round(lux / magnitude) * magnitude;
    swprintf_s(text, L"%.0f lux", rounded);
    return text;
  }
  const double thousands = lux / 1000.0;
  swprintf_s(text, L"%.0fk lux", thousands);
  return text;
}

std::wstring App::FormatTime(float condition) {
  const long rounded = std::lround(condition * kMinutesPerDay);
  const int minutes = static_cast<int>(rounded % kMinutesPerDay);
  SYSTEMTIME time{};
  time.wHour = static_cast<WORD>(minutes / 60);
  time.wMinute = static_cast<WORD>(minutes % 60);
  wchar_t text[32];
  const int length = GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &time, nullptr, text, ARRAYSIZE(text));
  if (length == 0) return {};
  return text;
}

}  // namespace yami

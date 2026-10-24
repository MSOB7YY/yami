// by claude

#include <algorithm>

#include "app.h"
#include "display_targets.h"
#include "gamma_ramp.h"

namespace yami {

void App::EnumerateMonitors() {
  if (dragMonitor_ >= 0) ReleaseCapture();
  KillTimer(hwnd_, kSaveTimer);
  SaveAll();
  ReleaseMonitorDcs();
  monitors_.clear();
  std::vector<HMONITOR> handles;
  const LPARAM handlesParam = reinterpret_cast<LPARAM>(&handles);
  EnumDisplayMonitors(nullptr, nullptr, CollectMonitor, handlesParam);
  const std::vector<DisplayTarget> targets = QueryDisplayTargets();
  for (const HMONITOR handle : handles) {
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(handle, &info)) continue;
    const auto target = std::find_if(targets.begin(), targets.end(), [&info](const DisplayTarget& t) { return t.gdiName == info.szDevice; });
    const bool hasTarget = target != targets.end();
    Monitor monitor;
    monitor.handle = handle;
    monitor.origin = {info.rcMonitor.left, info.rcMonitor.top};
    if (hasTarget) {
      monitor.key = target->path;
      monitor.name = target->name;
    } else {
      monitor.key = info.szDevice;
    }
    monitor.dc = CreateDCW(L"DISPLAY", info.szDevice, nullptr, nullptr);
    monitor.gamma = gamma_ramp::ReadLeftover(monitor.dc);
    const int storedDim = LoadDim(monitor.key);
    monitor.dim = std::clamp(storedDim, 0, dimCap_);
    for (size_t i = 0; i < kCurveCount; ++i) monitor.curves[i].Load(kCurvesKeys[i], monitor.key);
    monitors_.push_back(std::move(monitor));
  }
  std::sort(monitors_.begin(), monitors_.end(), [](const Monitor& a, const Monitor& b) {
    if (a.origin.x != b.origin.x) return a.origin.x < b.origin.x;
    return a.origin.y < b.origin.y;
  });
  std::vector<HMONITOR> ordered;
  ordered.reserve(monitors_.size());
  for (size_t i = 0; i < monitors_.size(); ++i) {
    Monitor& monitor = monitors_[i];
    if (monitor.name.empty()) {
      const std::wstring number = std::to_wstring(i + 1);
      monitor.name = L"Display " + number;
    }
    ApplyDim(monitor);
    ordered.push_back(monitor.handle);
  }
  ++generation_;
  worker_->Reset(generation_, std::move(ordered));
  ApplyAuto();
  StartGammaRestore();
  ResizeWindow();
  InvalidateRect(hwnd_, nullptr, FALSE);
  UpdateTrayTip();
}

void App::ReleaseMonitorDcs() {
  for (Monitor& monitor : monitors_) {
    if (monitor.dc) DeleteDC(monitor.dc);
    monitor.dc = nullptr;
  }
}

void App::OnProbed(WPARAM wParam, LPARAM lParam) {
  const uint32_t generation = static_cast<uint32_t>(wParam);
  if (generation != generation_) return;
  const DdcWorker::ProbeResult probe = DdcWorker::UnpackProbe(lParam);
  if (probe.index >= monitors_.size()) return;
  const int index = static_cast<int>(probe.index);
  const bool isDraggingBrightness = dragMonitor_ == index && dragKind_ == SliderKind::brightness;
  if (isDraggingBrightness) return;
  Monitor& monitor = monitors_[index];
  const bool isSupported = probe.maxBrightness > 0;
  monitor.ddc = isSupported ? DdcState::supported : DdcState::unsupported;
  monitor.maxBrightness = probe.maxBrightness;
  monitor.brightness = std::min(probe.brightness, probe.maxBrightness);
  InvalidateColumn(index);
  UpdateTrayTip();
  const bool didApply = ApplyAutoLevels(index);
  if (didApply) ScheduleSave();
}

void App::SetSliderValue(int index, SliderKind kind, int value) {
  const bool didChange = ChangeLevel(index, kind, value);
  if (!didChange) return;
  ScheduleSave();
  if (index == dragMonitor_) {
    didDragChange_ = true;
    return;
  }
  RecordLevels(index);
}

bool App::ChangeLevel(int index, SliderKind kind, int value) {
  Monitor& monitor = monitors_[index];
  if (kind == SliderKind::brightness) {
    const int clamped = std::clamp(value, 0, monitor.maxBrightness);
    if (clamped == monitor.brightness) return false;
    monitor.brightness = clamped;
    worker_->SetBrightness(generation_, index, clamped);
  } else {
    const int clamped = std::clamp(value, 0, dimCap_);
    if (clamped == monitor.dim) return false;
    monitor.dim = clamped;
    ApplyDim(monitor);
  }
  InvalidateColumn(index);
  return true;
}

void App::ApplyDim(Monitor& monitor) {
  const int previousCap = dimCap_;
  for (;;) {
    const double gamma = DimToGamma(monitor.dim);
    if (gamma == monitor.gamma) break;
    const bool didWrite = gamma_ramp::Write(monitor.dc, gamma);
    if (didWrite) {
      monitor.gamma = gamma;
      monitor.isRampNudged = false;
      break;
    }
    if (!monitor.dc || monitor.dim == 0) break;
    monitor.dim -= 1;
    dimCap_ = std::min(dimCap_, monitor.dim);
  }
  UpdateGammaWatch();
  if (dimCap_ == previousCap) return;
  ResizeWindow();
  InvalidateRect(hwnd_, nullptr, FALSE);
}

double App::DimToGamma(int dim) { return 1.0 - dim / 100.0; }

void App::TurnDimOff() {
  SetAutoSource(AutoSource::off);
  for (int i = 0; i < static_cast<int>(monitors_.size()); ++i) SetSliderValue(i, SliderKind::dim, 0);
}

bool App::HasDimmedMonitor() const {
  return std::any_of(monitors_.begin(), monitors_.end(), [](const Monitor& m) { return m.gamma < 1.0; });
}

void App::UpdateGammaWatch() {
  if (!kShouldPollGamma) return;
  const bool isNeeded = HasDimmedMonitor();
  if (isNeeded == isGammaWatchActive_) return;
  isGammaWatchActive_ = isNeeded;
  if (isNeeded) {
    SetTimer(hwnd_, kGammaWatchTimer, kGammaWatchMS, nullptr);
  } else {
    KillTimer(hwnd_, kGammaWatchTimer);
  }
}

void App::StartGammaRestore() {
  if (!HasDimmedMonitor()) return;
  RestoreGammaRamps();
  gammaRestoresLeft_ = kGammaRestoreCount;
  SetTimer(hwnd_, kGammaRestoreTimer, kGammaRestoreMS, nullptr);
}

void App::StepGammaRestore() {
  RestoreGammaRamps();
  --gammaRestoresLeft_;
  if (gammaRestoresLeft_ > 0) return;
  KillTimer(hwnd_, kGammaRestoreTimer);
}

void App::RestoreGammaRamps() {
  if (isSuspended_) return;
  for (Monitor& monitor : monitors_) {
    if (monitor.gamma >= 1.0 || !monitor.dc) continue;
    monitor.isRampNudged = !monitor.isRampNudged;
    gamma_ramp::Write(monitor.dc, monitor.gamma, monitor.isRampNudged);
  }
}

void App::ResetGammaRamps() {
  for (Monitor& monitor : monitors_) {
    if (monitor.gamma >= 1.0) continue;
    gamma_ramp::Write(monitor.dc, 1.0);
    monitor.gamma = 1.0;
  }
}

void App::StartGammaUnlock() {
  isGammaRangeUnlocked_ = gamma_ramp::UnlockRange(hwnd_);
  if (!isGammaRangeUnlocked_) return;
  dimCap_ = kMaxDim;
  ResizeWindow();
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::ScheduleSave() { SetTimer(hwnd_, kSaveTimer, kSaveDelayMS, nullptr); }

void App::SaveAll() {
  for (Monitor& monitor : monitors_) {
    const DWORD dim = static_cast<DWORD>(monitor.dim);
    RegSetKeyValueW(HKEY_CURRENT_USER, kSettingsKey, monitor.key.c_str(), REG_DWORD, &dim, sizeof(dim));
    if (!monitor.hasCurveChanges) continue;
    monitor.hasCurveChanges = false;
    for (size_t i = 0; i < kCurveCount; ++i) monitor.curves[i].Save(kCurvesKeys[i], monitor.key);
  }
}

int App::LoadDim(const std::wstring& key) {
  DWORD dim = 0;
  DWORD size = sizeof(dim);
  const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, key.c_str(), RRF_RT_REG_DWORD, nullptr, &dim, &size);
  if (status != ERROR_SUCCESS) return 0;
  return static_cast<int>(dim);
}

BOOL CALLBACK App::CollectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
  auto* handles = reinterpret_cast<std::vector<HMONITOR>*>(data);
  handles->push_back(monitor);
  return TRUE;
}

}  // namespace yami

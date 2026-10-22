// by claude

#include "ddc_worker.h"

#include <lowlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>

namespace yami {

static_assert(sizeof(LPARAM) == 8, "probe results are packed into a 64-bit LPARAM");

DdcWorker::DdcWorker(HWND notifyWindow, UINT probedMessage)
    : notifyWindow_(notifyWindow), probedMessage_(probedMessage), thread_([this] { Run(); }) {}

DdcWorker::~DdcWorker() {
  {
    std::lock_guard lock(mutex_);
    isQuitting_ = true;
  }
  wake_.notify_one();
  thread_.join();
}

void DdcWorker::Reset(uint32_t generation, std::vector<HMONITOR> monitors) {
  {
    std::lock_guard lock(mutex_);
    pendingGeneration_ = generation;
    pendingMonitors_ = std::move(monitors);
    hasPendingReset_ = true;
  }
  wake_.notify_one();
}

void DdcWorker::Refresh() {
  {
    std::lock_guard lock(mutex_);
    hasPendingRefresh_ = true;
  }
  wake_.notify_one();
}

void DdcWorker::SetBrightness(uint32_t generation, size_t index, int brightness) {
  {
    std::lock_guard lock(mutex_);
    if (generation != generation_ || index >= desired_.size()) return;
    desired_[index] = brightness;
  }
  wake_.notify_one();
}

void DdcWorker::Pause() {
  std::unique_lock lock(mutex_);
  isPaused_ = true;
  idle_.wait(lock, [this] { return !isBusy_; });
}

void DdcWorker::Resume() {
  {
    std::lock_guard lock(mutex_);
    isPaused_ = false;
  }
  wake_.notify_one();
}

DdcWorker::ProbeResult DdcWorker::UnpackProbe(LPARAM packed) {
  const uint64_t bits = static_cast<uint64_t>(packed);
  const size_t index = static_cast<size_t>(bits >> 32);
  const int maxBrightness = static_cast<int>((bits >> 16) & 0xFFFF);
  const int brightness = static_cast<int>(bits & 0xFFFF);
  return {index, brightness, maxBrightness};
}

void DdcWorker::Run() {
  std::unique_lock lock(mutex_);
  for (;;) {
    isBusy_ = false;
    idle_.notify_all();
    wake_.wait(lock, [this] { return isQuitting_ || HasWork(); });
    isBusy_ = true;
    const int dirty = isPaused_ ? -1 : FindDirty();
    if (dirty >= 0) {
      const int brightness = desired_[dirty];
      const HANDLE handle = physicals_[dirty].handle;
      applied_[dirty] = brightness;
      lock.unlock();
      WriteBrightness(handle, brightness);
      lock.lock();
      continue;
    }
    if (isQuitting_) break;
    if (hasPendingReset_) {
      hasPendingReset_ = false;
      hasPendingRefresh_ = false;
      generation_ = pendingGeneration_;
      desired_.assign(pendingMonitors_.size(), -1);
      const uint32_t generation = generation_;
      const std::vector<HMONITOR> monitors = std::move(pendingMonitors_);
      lock.unlock();
      ReleaseHandles();
      ProbeAll(generation, monitors);
      lock.lock();
      continue;
    }
    hasPendingRefresh_ = false;
    RefreshAll(lock);
  }
  lock.unlock();
  ReleaseHandles();
}

bool DdcWorker::HasWork() const {
  if (isPaused_) return false;
  return hasPendingReset_ || hasPendingRefresh_ || FindDirty() >= 0;
}

int DdcWorker::FindDirty() const {
  if (physicals_.size() != desired_.size()) return -1;
  for (size_t i = 0; i < desired_.size(); ++i) {
    const int brightness = desired_[i];
    if (brightness < 0 || brightness == applied_[i] || !physicals_[i].isOpen) continue;
    return static_cast<int>(i);
  }
  return -1;
}

void DdcWorker::ProbeAll(uint32_t generation, const std::vector<HMONITOR>& monitors) {
  physicals_.assign(monitors.size(), Physical{nullptr, false});
  applied_.assign(monitors.size(), -1);
  for (size_t i = 0; i < monitors.size(); ++i) {
    const std::optional<HANDLE> handle = OpenPhysicalMonitor(monitors[i]);
    Reading reading{0, 0};
    if (handle) reading = ReadBrightness(*handle);
    const bool isSupported = reading.maximum > 0;
    if (handle && !isSupported) DestroyPhysicalMonitor(*handle);
    if (isSupported) {
      physicals_[i] = Physical{*handle, true};
      applied_[i] = reading.current;
    }
    Post(generation, i, reading);
  }
}

void DdcWorker::RefreshAll(std::unique_lock<std::mutex>& lock) {
  const uint32_t generation = generation_;
  for (size_t i = 0; i < physicals_.size(); ++i) {
    const Physical physical = physicals_[i];
    if (!physical.isOpen) continue;
    lock.unlock();
    const Reading reading = ReadBrightness(physical.handle);
    lock.lock();
    if (hasPendingReset_ || isPaused_) return;
    if (reading.maximum == 0) continue;
    const int desired = desired_[i];
    const bool hasPendingWrite = desired >= 0 && desired != applied_[i];
    applied_[i] = reading.current;
    if (hasPendingWrite) continue;
    desired_[i] = reading.current;
    Post(generation, i, reading);
  }
}

void DdcWorker::Post(uint32_t generation, size_t index, const Reading& reading) const {
  const uint64_t indexBits = static_cast<uint64_t>(index) << 32;
  const uint64_t maximumBits = static_cast<uint64_t>(reading.maximum & 0xFFFF) << 16;
  const uint64_t currentBits = static_cast<uint64_t>(reading.current & 0xFFFF);
  const LPARAM packed = static_cast<LPARAM>(indexBits | maximumBits | currentBits);
  PostMessageW(notifyWindow_, probedMessage_, generation, packed);
}

void DdcWorker::ReleaseHandles() {
  for (const Physical& physical : physicals_) {
    if (physical.isOpen) DestroyPhysicalMonitor(physical.handle);
  }
  physicals_.clear();
  applied_.clear();
}

void DdcWorker::WaitForCommandGap() {
  const ULONGLONG elapsed = GetTickCount64() - lastCommandTick_;
  if (elapsed < kCommandGapMS) {
    const DWORD remaining = static_cast<DWORD>(kCommandGapMS - elapsed);
    Sleep(remaining);
  }
}

DdcWorker::Reading DdcWorker::ReadBrightness(HANDLE handle) {
  for (int attempt = 0; attempt < kReadAttempts; ++attempt) {
    DWORD current = 0;
    DWORD maximum = 0;
    WaitForCommandGap();
    const bool didRead = GetVCPFeatureAndVCPFeatureReply(handle, kVcpBrightness, nullptr, &current, &maximum);
    lastCommandTick_ = GetTickCount64();
    if (didRead) return {static_cast<int>(current), static_cast<int>(maximum)};
  }
  return {0, 0};
}

void DdcWorker::WriteBrightness(HANDLE handle, int brightness) {
  const DWORD value = static_cast<DWORD>(brightness);
  WaitForCommandGap();
  SetVCPFeature(handle, kVcpBrightness, value);
  lastCommandTick_ = GetTickCount64();
}

std::optional<HANDLE> DdcWorker::OpenPhysicalMonitor(HMONITOR monitor) {
  DWORD count = 0;
  const bool hasCount = GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &count);
  if (!hasCount || count == 0) return std::nullopt;
  std::vector<PHYSICAL_MONITOR> physical(count);
  const bool didGet = GetPhysicalMonitorsFromHMONITOR(monitor, count, physical.data());
  if (!didGet) return std::nullopt;
  for (DWORD i = 1; i < count; ++i) DestroyPhysicalMonitor(physical[i].hPhysicalMonitor);
  return physical[0].hPhysicalMonitor;
}

}  // namespace yami

// by claude

#pragma once

#include <windows.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace yami {

class DdcWorker {
 public:
  struct ProbeResult {
    size_t index;
    int brightness;
    int maxBrightness;
  };

  DdcWorker(HWND notifyWindow, UINT probedMessage);
  ~DdcWorker();

  DdcWorker(const DdcWorker&) = delete;
  DdcWorker& operator=(const DdcWorker&) = delete;

  void Reset(uint32_t generation, std::vector<HMONITOR> monitors);
  void Refresh();
  void SetBrightness(uint32_t generation, size_t index, int brightness);
  void Pause();
  void Resume();

  static ProbeResult UnpackProbe(LPARAM packed);

 private:
  struct Reading {
    int current;
    int maximum;
  };

  struct Physical {
    HANDLE handle;
    bool isOpen;
  };

  static constexpr BYTE kVcpBrightness = 0x10;
  static constexpr ULONGLONG kCommandGapMS = 50;
  static constexpr int kReadAttempts = 3;

  void Run();
  bool HasWork() const;
  int FindDirty() const;
  void ProbeAll(uint32_t generation, const std::vector<HMONITOR>& monitors);
  void RefreshAll(std::unique_lock<std::mutex>& lock);
  void Post(uint32_t generation, size_t index, const Reading& reading) const;
  void ReleaseHandles();
  void WaitForCommandGap();
  Reading ReadBrightness(HANDLE handle);
  void WriteBrightness(HANDLE handle, int brightness);
  static std::optional<HANDLE> OpenPhysicalMonitor(HMONITOR monitor);

  HWND notifyWindow_;
  UINT probedMessage_;
  std::mutex mutex_;
  std::condition_variable wake_;
  std::condition_variable idle_;
  bool isQuitting_ = false;
  bool isPaused_ = false;
  bool isBusy_ = false;
  bool hasPendingReset_ = false;
  bool hasPendingRefresh_ = false;
  uint32_t pendingGeneration_ = 0;
  std::vector<HMONITOR> pendingMonitors_;
  uint32_t generation_ = 0;
  std::vector<int> desired_;
  std::vector<Physical> physicals_;
  std::vector<int> applied_;
  ULONGLONG lastCommandTick_ = 0;
  std::thread thread_;
};

}  // namespace yami

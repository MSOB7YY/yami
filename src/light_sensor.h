// by claude

#pragma once

#include <windows.h>
#include <wrl/client.h>

struct ISensor;
struct ISensorManager;

namespace yami {

class LightSensor {
 public:
  LightSensor(HWND notifyWindow, UINT readingMessage, UINT changedMessage);
  ~LightSensor();

  LightSensor(const LightSensor&) = delete;
  LightSensor& operator=(const LightSensor&) = delete;

  void Attach();
  bool IsAvailable() const { return sensor_.Get() != nullptr; }

  static float UnpackLux(LPARAM packed);

 private:
  class Events;

  void Detach();

  Microsoft::WRL::ComPtr<Events> events_;
  Microsoft::WRL::ComPtr<ISensorManager> manager_;
  Microsoft::WRL::ComPtr<ISensor> sensor_;
};

}  // namespace yami

// by claude

#include "light_sensor.h"

#include <initguid.h>
#include <SensorsApi.h>
#include <sensors.h>

#include <atomic>
#include <bit>
#include <cstdint>

namespace yami {

using Microsoft::WRL::ComPtr;

class LightSensor::Events final : public ISensorManagerEvents, public ISensorEvents {
 public:
  Events(HWND notifyWindow, UINT readingMessage, UINT changedMessage);

  IFACEMETHODIMP QueryInterface(REFIID iid, void** object) override;
  IFACEMETHODIMP_(ULONG) AddRef() override;
  IFACEMETHODIMP_(ULONG) Release() override;
  IFACEMETHODIMP OnSensorEnter(ISensor* sensor, SensorState state) override;
  IFACEMETHODIMP OnStateChanged(ISensor* sensor, SensorState state) override;
  IFACEMETHODIMP OnDataUpdated(ISensor* sensor, ISensorDataReport* report) override;
  IFACEMETHODIMP OnEvent(ISensor* sensor, REFGUID eventId, IPortableDeviceValues* eventData) override;
  IFACEMETHODIMP OnLeave(REFSENSOR_ID id) override;

  void PostReading(ISensorDataReport* report) const;

 private:
  void PostChanged() const;

  std::atomic<ULONG> references_{0};
  HWND notifyWindow_;
  UINT readingMessage_;
  UINT changedMessage_;
};

LightSensor::LightSensor(HWND notifyWindow, UINT readingMessage, UINT changedMessage)
    : events_(new Events(notifyWindow, readingMessage, changedMessage)) {
  const HRESULT created = CoCreateInstance(CLSID_SensorManager, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager_));
  if (FAILED(created)) return;
  manager_->SetEventSink(events_.Get());
  Attach();
}

LightSensor::~LightSensor() {
  Detach();
  if (!manager_) return;
  manager_->SetEventSink(nullptr);
}

void LightSensor::Attach() {
  Detach();
  if (!manager_) return;
  ComPtr<ISensorCollection> sensors;
  const HRESULT found = manager_->GetSensorsByType(SENSOR_TYPE_AMBIENT_LIGHT, &sensors);
  if (FAILED(found)) return;
  ComPtr<ISensor> sensor;
  const HRESULT got = sensors->GetAt(0, &sensor);
  if (FAILED(got)) return;
  const HRESULT subscribed = sensor->SetEventSink(events_.Get());
  if (FAILED(subscribed)) return;
  sensor_ = std::move(sensor);
  ComPtr<ISensorDataReport> report;
  const HRESULT read = sensor_->GetData(&report);
  if (SUCCEEDED(read)) events_->PostReading(report.Get());
}

float LightSensor::UnpackLux(LPARAM packed) {
  const uint32_t bits = static_cast<uint32_t>(packed);
  return std::bit_cast<float>(bits);
}

void LightSensor::Detach() {
  if (!sensor_) return;
  sensor_->SetEventSink(nullptr);
  sensor_.Reset();
}

LightSensor::Events::Events(HWND notifyWindow, UINT readingMessage, UINT changedMessage)
    : notifyWindow_(notifyWindow), readingMessage_(readingMessage), changedMessage_(changedMessage) {}

IFACEMETHODIMP LightSensor::Events::QueryInterface(REFIID iid, void** object) {
  if (iid == __uuidof(IUnknown) || iid == __uuidof(ISensorEvents)) {
    *object = static_cast<ISensorEvents*>(this);
  } else if (iid == __uuidof(ISensorManagerEvents)) {
    *object = static_cast<ISensorManagerEvents*>(this);
  } else {
    *object = nullptr;
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

IFACEMETHODIMP_(ULONG) LightSensor::Events::AddRef() { return ++references_; }

IFACEMETHODIMP_(ULONG) LightSensor::Events::Release() {
  const ULONG remaining = --references_;
  if (remaining == 0) delete this;
  return remaining;
}

IFACEMETHODIMP LightSensor::Events::OnSensorEnter(ISensor* sensor, SensorState) {
  SENSOR_TYPE_ID type{};
  const HRESULT typeResult = sensor->GetType(&type);
  const bool isLight = SUCCEEDED(typeResult) && type == SENSOR_TYPE_AMBIENT_LIGHT;
  if (isLight) PostChanged();
  return S_OK;
}

IFACEMETHODIMP LightSensor::Events::OnStateChanged(ISensor*, SensorState) { return S_OK; }

IFACEMETHODIMP LightSensor::Events::OnDataUpdated(ISensor*, ISensorDataReport* report) {
  PostReading(report);
  return S_OK;
}

IFACEMETHODIMP LightSensor::Events::OnEvent(ISensor*, REFGUID, IPortableDeviceValues*) { return S_OK; }

IFACEMETHODIMP LightSensor::Events::OnLeave(REFSENSOR_ID) {
  PostChanged();
  return S_OK;
}

void LightSensor::Events::PostReading(ISensorDataReport* report) const {
  PROPVARIANT value;
  PropVariantInit(&value);
  const HRESULT result = report->GetSensorValue(SENSOR_DATA_TYPE_LIGHT_LEVEL_LUX, &value);
  const bool hasLux = SUCCEEDED(result) && value.vt == VT_R4;
  if (hasLux) {
    const uint32_t bits = std::bit_cast<uint32_t>(value.fltVal);
    PostMessageW(notifyWindow_, readingMessage_, 0, static_cast<LPARAM>(bits));
  }
  PropVariantClear(&value);
}

void LightSensor::Events::PostChanged() const { PostMessageW(notifyWindow_, changedMessage_, 0, 0); }

}  // namespace yami

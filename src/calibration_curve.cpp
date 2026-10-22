// by claude

#include "calibration_curve.h"

#include <algorithm>
#include <cmath>

namespace yami {

void CalibrationCurve::Record(float condition, float radius, bool isCircular, int brightness, int dim) {
  RemoveNear(condition, radius, isCircular);
  const auto isBefore = [](const Point& point, float value) { return point.condition < value; };
  const auto position = std::lower_bound(points_.begin(), points_.end(), condition, isBefore);
  points_.insert(position, Point{condition, brightness, dim});
}

bool CalibrationCurve::RemoveNear(float condition, float radius, bool isCircular) {
  const auto isNear = [=](const Point& point) { return Distance(point.condition, condition, isCircular) <= radius; };
  const size_t removed = std::erase_if(points_, isNear);
  return removed > 0;
}

CalibrationCurve::Levels CalibrationCurve::Evaluate(float condition, bool isCircular) const {
  const int brightness = Interpolate(&Point::brightness, condition, isCircular);
  const int dim = Interpolate(&Point::dim, condition, isCircular);
  return {brightness, dim};
}

void CalibrationCurve::Load(const wchar_t* subKey, const std::wstring& name) {
  points_.clear();
  DWORD size = 0;
  const LSTATUS sizeStatus = RegGetValueW(HKEY_CURRENT_USER, subKey, name.c_str(), RRF_RT_REG_BINARY, nullptr, nullptr, &size);
  if (sizeStatus != ERROR_SUCCESS || size % sizeof(Point) != 0) return;
  points_.resize(size / sizeof(Point));
  const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, subKey, name.c_str(), RRF_RT_REG_BINARY, nullptr, points_.data(), &size);
  if (status != ERROR_SUCCESS) points_.clear();
}

void CalibrationCurve::Save(const wchar_t* subKey, const std::wstring& name) const {
  if (points_.empty()) {
    RegDeleteKeyValueW(HKEY_CURRENT_USER, subKey, name.c_str());
    return;
  }
  const DWORD size = static_cast<DWORD>(points_.size() * sizeof(Point));
  RegSetKeyValueW(HKEY_CURRENT_USER, subKey, name.c_str(), REG_BINARY, points_.data(), size);
}

float CalibrationCurve::Distance(float a, float b, bool isCircular) {
  const float direct = std::abs(a - b);
  if (!isCircular) return direct;
  return std::min(direct, 1.0f - direct);
}

int CalibrationCurve::Interpolate(Field field, float condition, bool isCircular) const {
  const Point* first = nullptr;
  const Point* last = nullptr;
  const Point* lower = nullptr;
  const Point* upper = nullptr;
  for (const Point& point : points_) {
    if (point.*field == kNoLevel) continue;
    if (!first) first = &point;
    last = &point;
    if (point.condition <= condition) {
      lower = &point;
    } else if (!upper) {
      upper = &point;
    }
  }
  if (!first) return kNoLevel;
  if (!isCircular && !lower) return upper->*field;
  if (!isCircular && !upper) return lower->*field;
  const float lowerCondition = lower ? lower->condition : last->condition - 1.0f;
  const float upperCondition = upper ? upper->condition : first->condition + 1.0f;
  if (!lower) lower = last;
  if (!upper) upper = first;
  const float span = upperCondition - lowerCondition;
  if (span <= 0.0f) return lower->*field;
  const float progress = (condition - lowerCondition) / span;
  const int lowerLevel = lower->*field;
  const int upperLevel = upper->*field;
  const float level = lowerLevel + (upperLevel - lowerLevel) * progress;
  return static_cast<int>(std::lround(level));
}

}  // namespace yami

// by claude

#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace yami {

class CalibrationCurve {
 public:
  struct Levels {
    int brightness;
    int dim;
  };

  static constexpr int kNoLevel = -1;

  bool IsEmpty() const { return points_.empty(); }
  size_t Count() const { return points_.size(); }
  float ConditionAt(size_t index) const { return points_[index].condition; }
  void Clear() { points_.clear(); }
  void Record(float condition, float radius, bool isCircular, int brightness, int dim);
  bool RemoveNear(float condition, float radius, bool isCircular);
  Levels Evaluate(float condition, bool isCircular) const;
  void Load(const wchar_t* subKey, const std::wstring& name);
  void Save(const wchar_t* subKey, const std::wstring& name) const;

 private:
  struct Point {
    float condition;
    int32_t brightness;
    int32_t dim;
  };

  static_assert(sizeof(Point) == 12, "points are stored in the registry as raw bytes");

  using Field = int32_t Point::*;

  static float Distance(float a, float b, bool isCircular);
  int Interpolate(Field field, float condition, bool isCircular) const;

  std::vector<Point> points_;
};

}  // namespace yami

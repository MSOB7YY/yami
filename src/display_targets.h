// by claude

#pragma once

#include <string>
#include <vector>

namespace yami {

struct DisplayTarget {
  std::wstring gdiName;
  std::wstring path;
  std::wstring name;
};

std::vector<DisplayTarget> QueryDisplayTargets();

}  // namespace yami

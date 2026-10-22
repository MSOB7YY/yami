// by claude

#include "display_targets.h"

#include <windows.h>

namespace yami {

std::vector<DisplayTarget> QueryDisplayTargets() {
  std::vector<DISPLAYCONFIG_PATH_INFO> paths;
  std::vector<DISPLAYCONFIG_MODE_INFO> modes;
  LONG status = ERROR_INSUFFICIENT_BUFFER;
  UINT32 pathCount = 0;
  while (status == ERROR_INSUFFICIENT_BUFFER) {
    UINT32 modeCount = 0;
    status = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount);
    if (status != ERROR_SUCCESS) return {};
    paths.resize(pathCount);
    modes.resize(modeCount);
    status = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
  }
  if (status != ERROR_SUCCESS) return {};
  paths.resize(pathCount);
  std::vector<DisplayTarget> targets;
  targets.reserve(paths.size());
  for (const DISPLAYCONFIG_PATH_INFO& path : paths) {
    DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
    source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    source.header.size = sizeof(source);
    source.header.adapterId = path.sourceInfo.adapterId;
    source.header.id = path.sourceInfo.id;
    DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
    target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
    target.header.size = sizeof(target);
    target.header.adapterId = path.targetInfo.adapterId;
    target.header.id = path.targetInfo.id;
    const bool hasSource = DisplayConfigGetDeviceInfo(&source.header) == ERROR_SUCCESS;
    const bool hasTarget = DisplayConfigGetDeviceInfo(&target.header) == ERROR_SUCCESS;
    if (!hasSource || !hasTarget) continue;
    targets.push_back({source.viewGdiDeviceName, target.monitorDevicePath, target.monitorFriendlyDeviceName});
  }
  return targets;
}

}  // namespace yami

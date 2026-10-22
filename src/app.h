// by claude

#pragma once

#include <windows.h>
#include <shellapi.h>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "calibration_curve.h"
#include "ddc_worker.h"
#include "light_sensor.h"

namespace yami {

class App {
 public:
  static constexpr wchar_t kWindowClass[] = L"YamiWindow";
  static constexpr wchar_t kTrayArgument[] = L"--tray";
  static constexpr UINT kShowMessage = WM_APP + 2;

  int Run(HINSTANCE instance, bool isStartHidden);

 private:
  enum class DdcState : uint8_t {
    probing,
    supported,
    unsupported,
  };

  enum class SliderKind : uint8_t {
    brightness,
    dim,
  };

  enum class TitleButton : uint8_t {
    none,
    help,
    autoAdjust,
    close,
  };

  enum class AutoSource : uint8_t {
    light,
    time,
    off,
  };

  static constexpr size_t kCurveCount = static_cast<size_t>(AutoSource::off);

  struct Monitor {
    HMONITOR handle = nullptr;
    POINT origin{};
    std::wstring key;
    std::wstring name;
    HDC dc = nullptr;
    DdcState ddc = DdcState::probing;
    int brightness = 0;
    int maxBrightness = 0;
    int dim = 0;
    double gamma = 1.0;
    std::array<CalibrationCurve, kCurveCount> curves;
    bool hasCurveChanges = false;
  };

  struct SliderLayout {
    int centerX;
    int trackTop;
    int trackBottom;
    RECT value;
    RECT label;
  };

  struct ColumnLayout {
    RECT box;
    RECT name;
    SliderLayout brightness;
    SliderLayout dim;
  };

  struct SliderHit {
    int monitor;
    SliderKind kind;
  };

  struct StripLayout {
    RECT box;
    RECT glyph;
    RECT value;
    RECT now;
    int trackLeft;
    int trackRight;
    int trackY;
  };

  struct HelpRow {
    const wchar_t* keys;
    const wchar_t* action;
  };

  struct HelpSection {
    const wchar_t* title;
    std::span<const HelpRow> rows;
  };

  static constexpr HelpRow kSliderHelp[] = {
      {L"Drag or scroll", L"Move by 1"},
      {L"Click the track", L"Move by 10"},
  };
  static constexpr HelpRow kAutoBarHelp[] = {
      {L"Drag or scroll", L"Preview a light level or time"},
      {L"Move a slider", L"Save levels for the shown condition"},
      {L"Right-click a mark", L"Remove those saved levels"},
      {L"Now", L"Back to the current condition"},
  };
  static constexpr HelpRow kPanelHelp[] = {
      {L"Sun or clock", L"Choose the auto adjust mode"},
      {L"Question mark", L"Show or hide this help"},
      {L"Esc", L"Close help or hide the panel"},
      {L"Ctrl+Alt+Shift+D", L"Turn dim off on all displays"},
  };
  static constexpr HelpRow kTrayHelp[] = {
      {L"Left-click", L"Show or hide the panel"},
      {L"Right-click", L"Open the menu"},
  };
  static constexpr HelpSection kHelpSections[] = {
      {L"Sliders", kSliderHelp},
      {L"Auto adjust bar", kAutoBarHelp},
      {L"Panel", kPanelHelp},
      {L"Tray icon", kTrayHelp},
  };
  static constexpr TitleButton kTitleButtons[] = {TitleButton::help, TitleButton::autoAdjust, TitleButton::close};

  static constexpr wchar_t kAppName[] = L"Yami";
  static constexpr wchar_t kCloseGlyph = L'\xE8BB';
  static constexpr wchar_t kLightGlyph = L'\xE706';
  static constexpr wchar_t kClockGlyph = L'\xE121';
  static constexpr wchar_t kHelpGlyph = L'\xE9CE';
  static constexpr UINT kProbedMessage = WM_APP + 1;
  static constexpr UINT kTrayMessage = WM_APP + 3;
  static constexpr UINT kLightReadingMessage = WM_APP + 4;
  static constexpr UINT kLightChangedMessage = WM_APP + 5;
  static constexpr UINT kTrayId = 1;
  static constexpr UINT kCommandAutorun = 1;
  static constexpr UINT kCommandExit = 2;
  static constexpr UINT kCommandDimOff = 3;
  static constexpr UINT kCommandAutoOff = 4;
  static constexpr UINT kCommandAutoLight = 5;
  static constexpr UINT kCommandAutoTime = 6;
  static constexpr UINT kCommandResetCalibration = 7;
  static constexpr int kDimOffHotkeyId = 1;
  static constexpr UINT kDimOffHotkeyModifiers = MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT;
  static constexpr UINT kDimOffHotkeyKey = 'D';
  static constexpr UINT_PTR kSaveTimer = 1;
  static constexpr UINT_PTR kGammaWatchTimer = 2;
  static constexpr UINT_PTR kLightTimer = 3;
  static constexpr UINT_PTR kClockTimer = 4;
  static constexpr UINT kSaveDelayMS = 400;
  static constexpr UINT kGammaWatchMS = 3000;
  static constexpr UINT kLightStepMS = 100;
  static constexpr UINT kMinuteMS = 60000;
  static constexpr UINT kClockSlackMS = 50;
  static constexpr bool kShouldPollGamma = false;
  static constexpr DWORD kDisplayStateOn = 1;
  static constexpr DWORD kWindowStyle = WS_POPUP;
  static constexpr DWORD kWindowExStyle = WS_EX_TOOLWINDOW;

  static constexpr wchar_t kSettingsKey[] = L"Software\\Yami\\Monitors";
  static constexpr wchar_t kAppKey[] = L"Software\\Yami";
  static constexpr wchar_t kAutoValue[] = L"AutoAdjust";
  static constexpr const wchar_t* kCurvesKeys[kCurveCount] = {L"Software\\Yami\\Curves\\Light", L"Software\\Yami\\Curves\\Time"};
  static constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

  static constexpr int kMaxDim = 90;
  static constexpr int kLockedDimCap = 50;
  static constexpr int kClickStep = 10;
  static constexpr int kDefaultMaxBrightness = 100;

  static constexpr float kNoCondition = -1.0f;
  static constexpr float kLightHysteresis = 0.02f;
  static constexpr float kLightSmoothing = 0.15f;
  static constexpr float kLightSettle = 0.002f;
  static constexpr double kLuxDecades = 5.0;
  static constexpr double kMaxLux = 100000.0;
  static constexpr int kLightSteps = 50;
  static constexpr int kTimeSteps = 96;
  static constexpr int kLightTickCount = 5;
  static constexpr int kTimeTickCount = 4;
  static constexpr int kMinutesPerDay = 1440;

  static constexpr int kTitleHeight = 32;
  static constexpr int kTitleInset = 12;
  static constexpr int kTitleButtonSize = 24;
  static constexpr int kTitleButtonGap = 2;
  static constexpr int kTitleButtonMargin = 8;
  static constexpr int kTitleButtonRadius = 6;
  static constexpr int kPad = 12;
  static constexpr int kBoxTop = 36;
  static constexpr int kColumnInset = 6;
  static constexpr int kSliderWidth = 64;
  static constexpr int kColumnWidth = kSliderWidth * 2 + kColumnInset * 2;
  static constexpr int kColumnGap = 8;
  static constexpr int kMinClientWidth = 164;
  static constexpr int kNameTop = 8;
  static constexpr int kNameHeight = 18;
  static constexpr int kNameInset = 6;
  static constexpr int kTrackOffset = 42;
  static constexpr int kTrackHeight = 170;
  static constexpr int kTrackHalfWidth = 2;
  static constexpr int kTickInner = 9;
  static constexpr int kTickOuter = 15;
  static constexpr int kThumbWidth = 30;
  static constexpr int kThumbHeight = 12;
  static constexpr int kValueGap = 14;
  static constexpr int kValueWidth = 44;
  static constexpr int kValueHeight = 24;
  static constexpr int kLabelGap = 4;
  static constexpr int kLabelHeight = 14;
  static constexpr int kBoxBottomPad = 8;
  static constexpr int kBoxHeight = kTrackOffset + kTrackHeight + kThumbHeight / 2 + kValueGap + kValueHeight + kLabelGap + kLabelHeight + kBoxBottomPad;
  static constexpr int kStripGap = 8;
  static constexpr int kStripHeight = 48;
  static constexpr int kStripInset = 10;
  static constexpr int kStripTextTop = 6;
  static constexpr int kStripTextHeight = 18;
  static constexpr int kStripGlyphSize = 12;
  static constexpr int kStripGlyphGap = 6;
  static constexpr int kStripNowWidth = 40;
  static constexpr int kStripTrackY = 35;
  static constexpr int kStripTrackInset = 14;
  static constexpr int kStripThumbWidth = 10;
  static constexpr int kStripThumbHeight = 16;
  static constexpr int kStripMarkerWidth = 2;
  static constexpr int kStripMarkerHeight = 10;
  static constexpr int kStripMarkerReach = 6;
  static constexpr int kStripTickTop = 5;
  static constexpr int kStripTickHeight = 3;
  static constexpr int kStripDotSize = 6;
  static constexpr int kHelpWidth = 350;
  static constexpr int kHelpHeaderInset = 4;
  static constexpr int kHelpHeaderHeight = 24;
  static constexpr int kHelpSectionGap = 6;
  static constexpr int kHelpCardPad = 4;
  static constexpr int kHelpRowInset = 12;
  static constexpr int kHelpRowHeight = 20;
  static constexpr int kHelpKeysWidth = 104;
  static constexpr int kHelpGap = 8;
  static constexpr int kHelpScrollStep = 40;
  static constexpr int kHelpScrollbarWidth = 3;
  static constexpr int kHelpScrollbarInset = 3;
  static constexpr int kFooterGap = 8;
  static constexpr int kFooterHeight = 28;
  static constexpr int kBoxRadius = 8;
  static constexpr int kThumbRadius = 4;
  static constexpr int kValueRadius = 6;
  static constexpr int kFooterRadius = 6;
  static constexpr int kWindowMargin = 12;

  static constexpr COLORREF kColorBackground = RGB(28, 28, 28);
  static constexpr COLORREF kColorBorder = RGB(62, 62, 64);
  static constexpr COLORREF kColorColumn = RGB(38, 38, 40);
  static constexpr COLORREF kColorText = RGB(225, 225, 225);
  static constexpr COLORREF kColorTextDim = RGB(130, 130, 130);
  static constexpr COLORREF kColorTrack = RGB(70, 70, 72);
  static constexpr COLORREF kColorTrackLocked = RGB(46, 46, 48);
  static constexpr COLORREF kColorTrackFill = RGB(120, 120, 124);
  static constexpr COLORREF kColorTick = RGB(78, 78, 80);
  static constexpr COLORREF kColorTickLocked = RGB(52, 52, 54);
  static constexpr COLORREF kColorThumb = RGB(165, 165, 168);
  static constexpr COLORREF kColorThumbActive = RGB(215, 215, 218);
  static constexpr COLORREF kColorValue = RGB(52, 52, 55);
  static constexpr COLORREF kColorValueBorder = RGB(78, 78, 82);
  static constexpr COLORREF kColorCloseHot = RGB(196, 43, 28);
  static constexpr COLORREF kColorClosePressed = RGB(160, 36, 24);
  static constexpr COLORREF kColorTitleButtonHot = RGB(48, 48, 50);
  static constexpr COLORREF kColorTitleButtonPressed = RGB(60, 60, 63);
  static constexpr COLORREF kColorCloseGlyphHot = RGB(255, 255, 255);
  static constexpr COLORREF kColorFooter = RGB(45, 45, 48);
  static constexpr COLORREF kColorFooterBorder = RGB(78, 78, 82);

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
  LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
  void OnCreate();
  void OnDestroy();
  void OnTimer(WPARAM timer);
  void OnDpiChanged(WPARAM wParam);
  void OnSettingChange(LPARAM lParam);
  void OnDisplayChange();
  void OnPowerBroadcast(WPARAM event, LPARAM lParam);
  void OnResumed();
  void ToggleWindow();
  bool IsUncovered() const;
  void HideWindow();
  void OnEscape();
  void ToggleHelp();
  void ScrollHelp(int wheelDelta);
  void ShowAtCorner();
  void ResizeWindow();

  void EnumerateMonitors();
  void ReleaseMonitorDcs();
  void OnProbed(WPARAM wParam, LPARAM lParam);
  void SetSliderValue(int index, SliderKind kind, int value);
  bool ChangeLevel(int index, SliderKind kind, int value);
  void ApplyDim(Monitor& monitor);
  static double DimToGamma(int dim);
  void TurnDimOff();
  void UpdateGammaWatch();
  void RestoreGammaRamps() const;
  void ResetGammaRamps();
  void StartGammaUnlock();
  void ScheduleSave();
  void SaveAll();
  static int LoadDim(const std::wstring& key);
  static BOOL CALLBACK CollectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data);

  void LoadAutoSource();
  void SetAutoSource(AutoSource source);
  void StartAuto();
  void StopAuto();
  void OnLightReading(LPARAM lParam);
  void OnLightChanged();
  void StepLight();
  void StopLightSteps();
  void UpdateClock();
  float AutoCondition() const;
  bool IsPreviewing() const;
  void ApplyAuto();
  bool ApplyAutoLevels(int index);
  void RecordLevels(int index);
  void SetPreview(float condition);
  void StepPreview(int steps);
  void ExitPreview();
  void RemoveCalibrationNear(float condition, float radius);
  void ResetCalibration();
  size_t CurveIndex() const;
  bool IsCircularAxis() const;
  int StepCount() const;
  float SnapCondition(float condition) const;
  float StepCondition(float condition, int steps) const;
  std::wstring FormatCondition(float condition) const;
  static float LuxToCondition(float lux);
  static double ConditionToLux(float condition);
  static std::wstring FormatLux(double lux);
  static std::wstring FormatTime(float condition);

  int Scale(int value) const;
  bool HasFooter() const;
  bool HasStrip() const;
  int ColumnsWidth() const;
  int ClientWidth() const;
  int ClientHeight() const;
  RECT TitleRect() const;
  RECT TitleButtonRect(TitleButton button) const;
  TitleButton HitTitleButton(POINT point) const;
  static int HelpContentHeight();
  int HelpMaxScroll() const;
  RECT FooterRect() const;
  StripLayout LayoutStrip() const;
  static int ConditionToX(const StripLayout& strip, float condition);
  static float ConditionAtX(const StripLayout& strip, int x);
  static bool IsOnStripTrack(const StripLayout& strip, POINT point);
  ColumnLayout LayoutColumn(int index) const;
  SliderLayout LayoutSlider(int boxTop, int centerX) const;
  static const SliderLayout& SliderOf(const ColumnLayout& layout, SliderKind kind);
  static int SliderValue(const Monitor& monitor, SliderKind kind);
  static int SliderMax(const Monitor& monitor, SliderKind kind);
  int SliderCap(const Monitor& monitor, SliderKind kind) const;
  static bool IsSliderEnabled(const Monitor& monitor, SliderKind kind);
  static int ValueToY(const SliderLayout& slider, int value, int maximum);
  static int ValueFromY(const SliderLayout& slider, int y, int maximum);
  RECT ThumbRect(const SliderLayout& slider, int value, int maximum) const;
  SliderHit HitSlider(POINT point) const;

  static POINT PointFromLParam(LPARAM lParam);
  LRESULT HitTest(LPARAM lParam) const;
  void OnMouseDown(LPARAM lParam);
  void OnMouseMove(LPARAM lParam);
  void OnMouseUp(LPARAM lParam);
  void OnMouseLeave();
  void OnCaptureChanged();
  void UpdateTitleHover(POINT point);
  void OnMouseWheel(WPARAM wParam, LPARAM lParam);
  int TakeWheelSteps(WPARAM wParam);
  void OnStripMouseDown(const StripLayout& strip, POINT point);
  void OnRightClick(LPARAM lParam);
  void InvalidateColumn(int index) const;
  void InvalidateTitle() const;
  void InvalidateStrip() const;

  void Paint(HDC target, const RECT& dirty);
  void PaintTitle(HDC dc) const;
  void PaintIconButton(HDC dc, TitleButton button, wchar_t glyph, bool isPressed, bool isLit) const;
  void PaintHelp(HDC dc) const;
  void PaintHelpScrollbar(HDC dc, int scroll, int maxScroll) const;
  void PaintColumn(HDC dc, const Monitor& monitor, const ColumnLayout& layout, int index) const;
  void PaintSlider(HDC dc, const Monitor& monitor, SliderKind kind, const SliderLayout& slider, bool isDragging) const;
  void PaintStrip(HDC dc, const StripLayout& strip) const;
  void PaintStripTrack(HDC dc, const StripLayout& strip, float condition) const;
  void PaintFooter(HDC dc, const RECT& footer) const;
  static const wchar_t* SliderLabel(const Monitor& monitor, SliderKind kind);
  static void FillColor(HDC dc, const RECT& rect, COLORREF color);
  static void FillRound(HDC dc, const RECT& rect, COLORREF fill, COLORREF border, int radius);
  static void DrawLabel(HDC dc, std::wstring_view text, RECT rect, HFONT font, COLORREF color, UINT alignment);
  static void DrawGlyph(HDC dc, wchar_t glyph, const RECT& rect, HFONT font, COLORREF color);
  void EnsureBackBuffer(HDC target, int width, int height);
  void ReleaseBackBuffer();
  void CreateFonts();
  void DestroyFonts();
  HFONT CreateUiFont(int size, int weight, const wchar_t* face) const;

  void OnTrayMessage(WPARAM wParam, LPARAM lParam);
  void ShowTrayMenu(int x, int y);
  void ShowAutoMenu();
  HMENU CreateAutoMenu() const;
  void RunAutoCommand(UINT command);
  NOTIFYICONDATAW TrayData() const;
  void AddTrayIcon();
  void RemoveTrayIcon() const;
  void UpdateTrayTip() const;
  void RefreshTrayIcon();
  void WriteTrayTip(NOTIFYICONDATAW& data) const;
  static UINT AutoCommand(AutoSource source);
  static std::wstring AutorunCommand();
  static bool IsAutorunEnabled();
  static void SetAutorun(bool isEnabled);

  HWND hwnd_ = nullptr;
  HWND menuOwner_ = nullptr;
  UINT dpi_ = 96;
  UINT taskbarCreatedMessage_ = 0;
  std::unique_ptr<DdcWorker> worker_;
  std::vector<Monitor> monitors_;
  uint32_t generation_ = 0;
  bool isGammaRangeUnlocked_ = false;
  int dimCap_ = kLockedDimCap;
  bool isGammaWatchActive_ = false;
  int dragMonitor_ = -1;
  SliderKind dragKind_ = SliderKind::dim;
  int dragOffset_ = 0;
  bool didDragChange_ = false;
  int wheelRemainder_ = 0;
  TitleButton hotButton_ = TitleButton::none;
  bool isClosePressed_ = false;
  bool isAutoMenuOpen_ = false;
  bool isHelpVisible_ = false;
  int helpScroll_ = 0;
  bool isTrackingLeave_ = false;
  bool isDimOffHotkeyRegistered_ = false;
  HPOWERNOTIFY displayStateNotification_ = nullptr;
  bool isSuspended_ = false;
  bool hasPendingEnumerate_ = false;
  AutoSource autoSource_ = AutoSource::off;
  std::unique_ptr<LightSensor> lightSensor_;
  float liveCondition_ = kNoCondition;
  float lightTarget_ = kNoCondition;
  float previewCondition_ = kNoCondition;
  bool isLightStepping_ = false;
  bool isDraggingPreview_ = false;
  HICON trayIcon_ = nullptr;
  HFONT titleFont_ = nullptr;
  HFONT nameFont_ = nullptr;
  HFONT valueFont_ = nullptr;
  HFONT labelFont_ = nullptr;
  HFONT iconFont_ = nullptr;
  HFONT glyphFont_ = nullptr;
  HFONT smallFont_ = nullptr;
  HFONT smallBoldFont_ = nullptr;
  HDC backDc_ = nullptr;
  HBITMAP backBitmap_ = nullptr;
  HGDIOBJ backPreviousBitmap_ = nullptr;
  SIZE backSize_{};
};

}  // namespace yami

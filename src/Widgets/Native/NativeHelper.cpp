module;
#include <dwmapi.h>
#include <QMainWindow>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
module Widgets.NativeHelper;

namespace Artifact
{

 void NativeGUIHelper::applyMicaEffect(QMainWindow* window, bool darkTheme /*= true*/)
 {
  if (!window) return;

  HWND hwnd = reinterpret_cast<HWND>(window->winId());

  BOOL isDark = darkTheme ? TRUE : FALSE;
  DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &isDark, sizeof(isDark));

  // Mica Backdrop Type (2: Mica, 3: Acrylic, 4: Mica Alt)
  DWORD backdropType = 2;
  DwmSetWindowAttribute(hwnd, 38, &backdropType, sizeof(backdropType)); // DWMWA_SYSTEMBACKDROP_TYPE = 38
 }

 void NativeGUIHelper::applyWindowRound(QMainWindow* window)
 {
  if (!window) return;

  HWND hwnd = reinterpret_cast<HWND>(window->winId());
  // DWMWCP_ROUND = 2
  DWORD preference = 2;
  ::DwmSetWindowAttribute(hwnd, 33, &preference, sizeof(preference)); // DWMWA_WINDOW_CORNER_PREFERENCE = 33
 }

};
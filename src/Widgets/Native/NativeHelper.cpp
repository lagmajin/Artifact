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

  // 2. MicaŒø‰Ê‚ÌŽw’è (DWM_SYSTEMBACKDROP_TYPE)
  // 2: Mica, 3: Acrylic, 4: Mica Alt (Tabbed)
  DWORD backdropType = 2;
  DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));

  // 3. QMainWindow‘¤‚Ì”wŒi‚ð“§–¾‚É‚µ‚ÄMica‚ðŒ©‚¦‚é‚æ‚¤‚É‚·‚é
  window->setAttribute(Qt::WA_TranslucentBackground);
  window->setStyleSheet("QMainWindow { background: transparent; }");
 }

 void NativeGUIHelper::applyWindowRound(QMainWindow* window)
 {
  if (!window) return;

  HWND hwnd = reinterpret_cast<HWND>(window->winId());
  DWORD preference = 2;
  ::DwmSetWindowAttribute(hwnd, 33, &preference, sizeof(preference));
 }

};
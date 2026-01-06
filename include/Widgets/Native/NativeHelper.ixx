module;
#include <dwmapi.h>
#include <qwindowdefs.h>
#include <QMainWindow>
export module Widgets.NativeHelper;

export namespace Artifact
{
 
	
 class NativeGUIHelper
 {
 private:
 	
 public:
  static void applyMicaEffect(QMainWindow* window, bool darkTheme = true);
  static void applyWindowRound(QMainWindow* window);
 };










};
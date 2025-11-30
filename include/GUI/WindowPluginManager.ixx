module;
#include <QString>

export module WindowManager;
import std;

export namespace Artifact
{
 struct WindowTypeInfo {
  QString name;
  bool allowMultiple = false;
 };
 class WindowPluginManager
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  WindowPluginManager();
  ~WindowPluginManager();
  void registerWindowFactory();
 };

};
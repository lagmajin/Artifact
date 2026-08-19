module;


#include <QString>
#include <QVector>

export module WindowManager;

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
  bool registerWindowType(const WindowTypeInfo& info);
  bool unregisterWindowType(const QString& name);
  bool hasWindowType(const QString& name) const;
  bool canOpenWindow(const QString& name, int openInstanceCount) const;
  QVector<WindowTypeInfo> windowTypes() const;
 };

};

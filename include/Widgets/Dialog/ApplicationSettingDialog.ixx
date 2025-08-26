module;
#include <QWidget>
#include <QDialog>
export module ApplicationSettingDialog;

export namespace ArtifactCore {

 class LabelColorSettingWidget :public QWidget {
 private:
  //LabelColorSettingWidgetPrivate* const	pWidget_;
 public:
  explicit LabelColorSettingWidget(const QString& labelname, const QColor& color, QWidget* parent = NULL);
  ~LabelColorSettingWidget();
 };


 class MemoryAndCpuSettingPage :public QWidget {
  //Q_OBJECT
 private:
  class Impl;
  Impl* impl_;
 protected:
 public:
  explicit MemoryAndCpuSettingPage(QWidget* parent = nullptr);
  virtual ~MemoryAndCpuSettingPage();
  int cpuCount() const;
  void resetSetting();

 };


 class ShortcutSettingPage : public QWidget {
  //Q_OBJECT
  class Impl;
  Impl* impl_;
 public:
  explicit ShortcutSettingPage(QWidget* parent = nullptr);
  ~ShortcutSettingPage();

  QVector<QWidget*> settingWidgets() const;

 };

 class PluginSettingPage : public QWidget {
  Q_OBJECT
 public:
  explicit PluginSettingPage(QWidget* parent = nullptr);
  ~PluginSettingPage();

  QVector<QWidget*> settingWidgets() const;

 private:
  class Impl;
  Impl* impl_;
 };

class ApplicationSettingDialog:public QDialog
{
private:
 class Impl;
 Impl* impl_;
public:
 explicit ApplicationSettingDialog(QWidget* parent = nullptr);
 ~ApplicationSettingDialog();
};






};
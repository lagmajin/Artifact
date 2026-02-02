module;
#include <QWidget>
#include <QDialog>

#include <wobjectdefs.h>

export module ApplicationSettingDialog;

export namespace ArtifactCore {

 // General Settings Page
 class GeneralSettingPage : public QWidget {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit GeneralSettingPage(QWidget* parent = nullptr);
  ~GeneralSettingPage();
 };

 // Import Settings Page
 class ImportSettingPage : public QWidget {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ImportSettingPage(QWidget* parent = nullptr);
  ~ImportSettingPage();
 };

 // Preview Settings Page
 class PreviewSettingPage : public QWidget {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit PreviewSettingPage(QWidget* parent = nullptr);
  ~PreviewSettingPage();
 };

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
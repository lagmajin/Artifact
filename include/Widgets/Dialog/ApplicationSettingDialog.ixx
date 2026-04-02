module;
#include <QWidget>
#include <QDialog>

#include <wobjectdefs.h>

export module ApplicationSettingDialog;

export namespace ArtifactCore {

 struct SettingItemInfo {
  QString label;
  QString description;
  QString category;
  QWidget* widget = nullptr;
 };

  class ISettingPage {
  public:
   virtual ~ISettingPage() = default;
   virtual void loadSettings() = 0;
   virtual void saveSettings() = 0;
   virtual QList<SettingItemInfo> searchableItems() const = 0;
  };

  // General Settings Page
 class GeneralSettingPage : public QWidget, public ISettingPage {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit GeneralSettingPage(QWidget* parent = nullptr);
  ~GeneralSettingPage();
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;
 };

 // Import Settings Page
 class ImportSettingPage : public QWidget, public ISettingPage {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ImportSettingPage(QWidget* parent = nullptr);
  ~ImportSettingPage();
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;
 };

 // Preview Settings Page
 class PreviewSettingPage : public QWidget, public ISettingPage {
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit PreviewSettingPage(QWidget* parent = nullptr);
  ~PreviewSettingPage();
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;
 };

 class LabelColorSettingWidget :public QWidget {
 private:
  //LabelColorSettingWidgetPrivate* const	pWidget_;
 public:
  explicit LabelColorSettingWidget(const QString& labelname, const QColor& color, QWidget* parent = NULL);
  ~LabelColorSettingWidget();
 };


 class MemoryAndCpuSettingPage :public QWidget, public ISettingPage {
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
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;
 };


 class ShortcutSettingPage : public QWidget, public ISettingPage {
  //Q_OBJECT
  class Impl;
  Impl* impl_;
 public:
  explicit ShortcutSettingPage(QWidget* parent = nullptr);
  ~ShortcutSettingPage();

  QVector<QWidget*> settingWidgets() const;
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;
 };

 class PluginSettingPage : public QWidget, public ISettingPage {
   W_OBJECT(PluginSettingPage)
  public:
   explicit PluginSettingPage(QWidget* parent = nullptr);
   ~PluginSettingPage();

   QVector<QWidget*> settingWidgets() const;
   void loadSettings() override;
   void saveSettings() override;
   QList<SettingItemInfo> searchableItems() const;

  private:
   class Impl;
   Impl* impl_;
  };

  class AISettingPage : public QWidget, public ISettingPage {
   W_OBJECT(AISettingPage)
  public:
   explicit AISettingPage(QWidget* parent = nullptr);
   ~AISettingPage();

   QVector<QWidget*> settingWidgets() const;
   void loadSettings() override;
   void saveSettings() override;
   QList<SettingItemInfo> searchableItems() const;

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
 void accept() override;
 void loadSettings();
 void saveSettings();
protected:
 void keyPressEvent(QKeyEvent* event) override;
};






};

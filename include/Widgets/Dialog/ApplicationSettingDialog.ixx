module;
#include <utility>

#include <QColor>
#include <QDialog>
#include <QList>
#include <QString>
#include <QVector>
#include <QWidget>
#include <wobjectdefs.h>
export module ApplicationSettingDialog;

export namespace ArtifactCore {

struct SettingItemInfo {
  QString label;
  QString description;
  QString category;
  QWidget *widget = nullptr;
};

class ISettingPage {
public:
  virtual ~ISettingPage() = default;
  virtual void loadSettings() = 0;
  virtual void saveSettings() = 0;
  virtual QList<SettingItemInfo> searchableItems() const = 0;
};

class GeneralSettingPage : public QWidget, public ISettingPage {
private:
  class Impl;
  Impl *impl_;

public:
  explicit GeneralSettingPage(QWidget *parent = nullptr);
  ~GeneralSettingPage();
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;
};

class ImportSettingPage : public QWidget, public ISettingPage {
private:
  class Impl;
  Impl *impl_;

public:
  explicit ImportSettingPage(QWidget *parent = nullptr);
  ~ImportSettingPage();
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;
};

class PreviewSettingPage : public QWidget, public ISettingPage {
private:
  class Impl;
  Impl *impl_;

public:
  explicit PreviewSettingPage(QWidget *parent = nullptr);
  ~PreviewSettingPage();
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;
};

class ProjectDefaultsSettingPage : public QWidget, public ISettingPage {
private:
  class Impl;
  Impl *impl_;

public:
  explicit ProjectDefaultsSettingPage(QWidget *parent = nullptr);
  ~ProjectDefaultsSettingPage();
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;
};

class LabelColorSettingWidget : public QWidget {
public:
  explicit LabelColorSettingWidget(const QString &labelname, const QColor &color,
                                   QWidget *parent = nullptr);
  ~LabelColorSettingWidget();
};

class MemoryAndCpuSettingPage : public QWidget, public ISettingPage {
private:
  class Impl;
  Impl *impl_;

public:
  explicit MemoryAndCpuSettingPage(QWidget *parent = nullptr);
  virtual ~MemoryAndCpuSettingPage();
  int cpuCount() const;
  void resetSetting();
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;
};

class ShortcutSettingPage : public QWidget, public ISettingPage {
private:
  class Impl;
  Impl *impl_;

public:
  explicit ShortcutSettingPage(QWidget *parent = nullptr);
  ~ShortcutSettingPage();

  QVector<QWidget *> settingWidgets() const;
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;
};

class PluginSettingPage : public QWidget, public ISettingPage {
  W_OBJECT(PluginSettingPage)
public:
  explicit PluginSettingPage(QWidget *parent = nullptr);
  ~PluginSettingPage();

  QVector<QWidget *> settingWidgets() const;
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;

private:
  class Impl;
  Impl *impl_;
};

class AISettingPage : public QWidget, public ISettingPage {
  W_OBJECT(AISettingPage)
public:
  explicit AISettingPage(QWidget *parent = nullptr);
  ~AISettingPage();

  QVector<QWidget *> settingWidgets() const;
  void loadSettings() override;
  void saveSettings() override;
  QList<SettingItemInfo> searchableItems() const override;

private:
  class Impl;
  Impl *impl_;
};

class ApplicationSettingDialog : public QDialog {
private:
  class Impl;
  Impl *impl_;

protected:
  void showEvent(QShowEvent *event) override;

public:
  explicit ApplicationSettingDialog(QWidget *parent = nullptr);
  ~ApplicationSettingDialog();
  void accept() override;
  void loadSettings();
  void saveSettings();
};

};

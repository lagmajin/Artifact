module;
#include <utility>
#include <QMenu>
#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QUrl>
#include <QString>
#include <wobjectimpl.h>

module Menu.Option;
import std;

import Application.AppSettings;
import ApplicationSettingDialog;
import Artifact.MainWindow;
import Utils.Path;

namespace Artifact {

W_OBJECT_IMPL(ArtifactOptionMenu)

class ArtifactOptionMenu::Impl {
private:

public:
  explicit Impl(ArtifactOptionMenu* menu);
  ~Impl();

  ArtifactOptionMenu* menu_ = nullptr;
  QAction* preferencesAction = nullptr;
  QAction* safeModeAction = nullptr;
  QAction* resetMenuFontAction = nullptr;
  QAction* resetDockFontAction = nullptr;
  QAction* openAppDataAction = nullptr;

  void handleCompositionOpened();
  void handleCompositionClosed();
  void refreshState();
};

ArtifactOptionMenu::Impl::Impl(ArtifactOptionMenu* menu)
  : menu_(menu)
{
  preferencesAction = menu_->addAction("環境設定 (&P)...");
  preferencesAction->setIcon(QIcon(resolveIconPath("Studio/settings.svg")));

  safeModeAction = menu_->addAction("セーフモード");
  safeModeAction->setCheckable(true);
  safeModeAction->setIcon(QIcon(resolveIconPath("Studio/security.svg")));

  menu_->addSeparator();
  resetMenuFontAction = menu_->addAction("メニューフォントを既定に戻す");
  resetMenuFontAction->setIcon(QIcon(resolveIconPath("Studio/text_fields.svg")));
  resetDockFontAction = menu_->addAction("ドックフォントを既定に戻す");
  resetDockFontAction->setIcon(QIcon(resolveIconPath("Studio/view_sidebar.svg")));

  menu_->addSeparator();
  openAppDataAction = menu_->addAction("アプリデータフォルダを開く");
  openAppDataAction->setIcon(QIcon(resolveIconPath("Studio/folder_open.svg")));

  QObject::connect(preferencesAction, &QAction::triggered, menu_, [this]() {
    auto* dialog = new ArtifactCore::ApplicationSettingDialog(menu_->window());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
  });
  QObject::connect(safeModeAction, &QAction::toggled, menu_, [](bool checked) {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
      settings->setSafeMode(checked);
    }
  });
  QObject::connect(resetMenuFontAction, &QAction::triggered, menu_, []() {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
      settings->setMenuBarFontScalePercent(132);
    }
  });
  QObject::connect(resetDockFontAction, &QAction::triggered, menu_, []() {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
      settings->setDockTabFontPointSize(16);
    }
  });
  QObject::connect(openAppDataAction, &QAction::triggered, menu_, []() {
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(appData))) {
      QMessageBox::information(nullptr, QStringLiteral("アプリデータ"),
                               QStringLiteral("アプリデータフォルダを開けませんでした。"));
    }
  });

  if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
    QObject::connect(settings, &ArtifactCore::ArtifactAppSettings::settingsChanged,
                     menu_, [this]() { refreshState(); });
  }

  QObject::connect(menu_, &QMenu::aboutToShow, menu_, [this]() {
    refreshState();
  });
  refreshState();
}

ArtifactOptionMenu::Impl::~Impl()
{
}

void ArtifactOptionMenu::Impl::refreshState()
{
  if (safeModeAction) {
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
      const QSignalBlocker blocker(safeModeAction);
      safeModeAction->setChecked(settings->isSafeMode());
    }
  }
}

void ArtifactOptionMenu::Impl::handleCompositionOpened()
{
  refreshState();
}

void ArtifactOptionMenu::Impl::handleCompositionClosed()
{
  refreshState();
}

ArtifactOptionMenu::ArtifactOptionMenu(QWidget* parent/*=nullptr*/)
  :QMenu(parent),impl_(new Impl(this))
{
  setTitle("Options");
  setTearOffEnabled(true);
  setSeparatorsCollapsible(true);
  setMinimumWidth(240);
  setIcon(QIcon(resolveIconPath("Studio/settings.svg")));
}

ArtifactOptionMenu::~ArtifactOptionMenu()
{
  delete impl_;
}

};

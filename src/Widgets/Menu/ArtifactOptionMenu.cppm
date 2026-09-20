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
import Translation.Manager;

import Application.AppSettings;
import ApplicationSettingDialog;
import Event.Bus;
import Utils.Path;

namespace Artifact {

W_OBJECT_IMPL(ArtifactOptionMenu)

namespace {
QString menuText(const QString& key, const QString& fallback)
{
  return TranslationManager::instance().tr(key, fallback);
}
} // namespace

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
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  ArtifactCore::EventBus::Subscription settingsSubscription_;

  void handleCompositionOpened();
  void handleCompositionClosed();
  void refreshState();
};

ArtifactOptionMenu::Impl::Impl(ArtifactOptionMenu* menu)
  : menu_(menu)
{
  preferencesAction = menu_->addAction(menuText(QStringLiteral("menu.option.preferences"), QStringLiteral("環境設定 (&P)...")));
  preferencesAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/optionmenu_settings.svg")));

  safeModeAction = menu_->addAction(menuText(QStringLiteral("menu.option.safe_mode"), QStringLiteral("セーフモード")));
  safeModeAction->setCheckable(true);
  safeModeAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/optionmenu_security.svg")));

  menu_->addSeparator();
  resetMenuFontAction = menu_->addAction(menuText(QStringLiteral("menu.option.reset_menu_font"), QStringLiteral("メニューフォントを既定に戻す")));
  resetMenuFontAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/optionmenu_text_fields.svg")));
  resetDockFontAction = menu_->addAction(menuText(QStringLiteral("menu.option.reset_dock_font"), QStringLiteral("ドックフォントを既定に戻す")));
  resetDockFontAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/optionmenu_view_sidebar.svg")));

  menu_->addSeparator();
  openAppDataAction = menu_->addAction(menuText(QStringLiteral("menu.option.open_app_data"), QStringLiteral("アプリデータフォルダを開く")));
  openAppDataAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/optionmenu_folder_open.svg")));

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
      QMessageBox::information(nullptr, menuText(QStringLiteral("dialog.app_data.title"), QStringLiteral("アプリデータ")),
                               menuText(QStringLiteral("dialog.app_data.open_failed"), QStringLiteral("アプリデータフォルダを開けませんでした。")));
    }
  });

  if (ArtifactCore::ArtifactAppSettings::instance()) {
    settingsSubscription_ = eventBus_.subscribe<ArtifactCore::AppSettingsChangedEvent>(
        [this](const ArtifactCore::AppSettingsChangedEvent&) {
          if (menu_) {
            refreshState();
          }
        });
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
  setTitle(TranslationManager::instance().tr(QStringLiteral("menu.options.label"), QStringLiteral("オプション(&O)")));
  setTearOffEnabled(true);
  setSeparatorsCollapsible(true);
  setMinimumWidth(240);
  setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/menubar_options.svg")));
}

ArtifactOptionMenu::~ArtifactOptionMenu()
{
  delete impl_;
}

};

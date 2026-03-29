module;
#include <wobjectimpl.h>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QSysInfo>
#include <QVariant>
#include <QWidget>
#include <QMenuBar>
module Menu.Help;
import std;

import Core.FastSettingsStore;
import Utils.Path;
import Artifact.Service.Application;

namespace Artifact {
 using namespace ArtifactCore;
 W_OBJECT_IMPL(ArtifactHelpMenu)

 class ArtifactHelpMenu::Impl {
 public:
  Impl();
  ~Impl();

  QAction* versionInfoAction_ = nullptr;
  QAction* aboutAction_ = nullptr;
  QAction* docsAction_ = nullptr;
  QAction* checkUpdatesAction_ = nullptr;
  QAction* exportDiagnosticsAction_ = nullptr;
 };

 ArtifactHelpMenu::Impl::Impl()
 {
  versionInfoAction_ = new QAction(u8"Version Info");
  versionInfoAction_->setIcon(QIcon(resolveIconPath("Material/info.svg")));

  aboutAction_ = new QAction(u8"About Artifact");
  aboutAction_->setIcon(QIcon(resolveIconPath("Material/info.svg")));

  docsAction_ = new QAction(u8"Documentation");
  docsAction_->setIcon(QIcon(resolveIconPath("Material/help.svg")));

  checkUpdatesAction_ = new QAction(u8"Check for Updates");
  checkUpdatesAction_->setIcon(QIcon(resolveIconPath("Material/history.svg")));

  exportDiagnosticsAction_ = new QAction(u8"Export Diagnostics...");
  exportDiagnosticsAction_->setIcon(QIcon(resolveIconPath("Material/inventory.svg")));
 }

 ArtifactHelpMenu::Impl::~Impl()
 {
  delete versionInfoAction_;
  delete aboutAction_;
  delete docsAction_;
  delete checkUpdatesAction_;
  delete exportDiagnosticsAction_;
 }

 ArtifactHelpMenu::ArtifactHelpMenu(QWidget* parent /*= nullptr*/):QMenu(parent),impl_(new Impl())
 {
  setObjectName("HelpMenu");
  setTitle(tr("Help(&H)"));
  setTearOffEnabled(true);

  // add actions
  addAction(impl_->versionInfoAction_);
  addAction(impl_->aboutAction_);
  addAction(impl_->docsAction_);
  addSeparator();
  addAction(impl_->checkUpdatesAction_);
  addAction(impl_->exportDiagnosticsAction_);

  // connections
  connect(impl_->versionInfoAction_, &QAction::triggered, this, [this]() {
    const QString ver = ApplicationService::instance()->applicationVersion();
    QMessageBox::information(this, tr("Version"), tr("Artifact Version: %1").arg(ver));
  });

  connect(impl_->aboutAction_, &QAction::triggered, this, [this]() {
    QMessageBox::about(this, tr("About Artifact"), tr("Artifact - A creative tool."));
  });

  connect(impl_->docsAction_, &QAction::triggered, this, [this]() {
    QDesktopServices::openUrl(QUrl("https://github.com/lagmajin/Artifact"));
  });

  connect(impl_->checkUpdatesAction_, &QAction::triggered, this, [this]() {
    QMessageBox::information(this, tr("Updates"), tr("No updates available."));
  });

  connect(impl_->exportDiagnosticsAction_, &QAction::triggered, this, [this]() {
    const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString defaultName = QStringLiteral("artifact_diagnostics_%1.txt")
      .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(
      this,
      tr("Export Diagnostics"),
      QDir(defaultDir).filePath(defaultName),
      tr("Text Files (*.txt);;All Files (*)"));
    if (path.trimmed().isEmpty()) {
      return;
    }

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
      QMessageBox::warning(this, tr("Diagnostics"), tr("Failed to open file for writing."));
      return;
    }

    const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString sessionPath = QDir(appDataDir).filePath(QStringLiteral("session_state.cbor"));
    const QString historyPath = QDir(appDataDir).filePath(QStringLiteral("render_queue_history.cbor"));
    const QString layoutPath = QDir(appDataDir).filePath(QStringLiteral("main_window_layout.cbor"));
    const QString recoveryDir = QDir(appDataDir).filePath(QStringLiteral("Recovery"));
    ArtifactCore::FastSettingsStore sessionStore(sessionPath);

    QTextStream ts(&out);
    ts << "Artifact Diagnostics Report\n";
    ts << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
    ts << "[Runtime]\n";
    ts << "Qt Version: " << qVersion() << "\n";
    ts << "Product: " << QSysInfo::prettyProductName() << "\n";
    ts << "CPU Arch: " << QSysInfo::currentCpuArchitecture() << "\n";
    ts << "App Dir: " << QCoreApplication::applicationDirPath() << "\n";
    ts << "AppData Dir: " << appDataDir << "\n\n";

    ts << "[Session State]\n";
    ts << "Session File: " << sessionPath << "\n";
    ts << "Session File Exists: " << (QFileInfo::exists(sessionPath) ? "true" : "false") << "\n";
    ts << "RenderQueue History File: " << historyPath << "\n";
    ts << "RenderQueue History Exists: " << (QFileInfo::exists(historyPath) ? "true" : "false") << "\n";
    ts << "Layout File: " << layoutPath << "\n";
    ts << "Layout File Exists: " << (QFileInfo::exists(layoutPath) ? "true" : "false") << "\n";
    ts << "Recovery Dir: " << recoveryDir << "\n";
    ts << "Recovery Dir Exists: " << (QDir(recoveryDir).exists() ? "true" : "false") << "\n\n";

    ts << "[Session Keys]\n";
    ts << "Session/isRunning: " << sessionStore.value(QStringLiteral("Session/isRunning"), false).toBool() << "\n";
    ts << "Session/startTimestamp: " << sessionStore.value(QStringLiteral("Session/startTimestamp"), QString()).toString() << "\n";
    ts << "Session/lastCleanExitTimestamp: " << sessionStore.value(QStringLiteral("Session/lastCleanExitTimestamp"), QString()).toString() << "\n";
    ts << "Session/pid: " << sessionStore.value(QStringLiteral("Session/pid"), QVariant()).toString() << "\n\n";

    ts << "[Layout Restore Result]\n";
    ts << "Session/layoutRestoreAttempted: " << sessionStore.value(QStringLiteral("Session/layoutRestoreAttempted"), false).toBool() << "\n";
    ts << "Session/layoutGeometryRestored: " << sessionStore.value(QStringLiteral("Session/layoutGeometryRestored"), false).toBool() << "\n";
    ts << "Session/layoutStateRestored: " << sessionStore.value(QStringLiteral("Session/layoutStateRestored"), false).toBool() << "\n";
    ts << "Session/layoutResetApplied: " << sessionStore.value(QStringLiteral("Session/layoutResetApplied"), false).toBool() << "\n";
    ts << "Session/layoutRestoreTimestamp: " << sessionStore.value(QStringLiteral("Session/layoutRestoreTimestamp"), QString()).toString() << "\n\n";

    ts << "[Recovery Snapshots]\n";
    int snapshotCount = 0;
    if (QDir(recoveryDir).exists()) {
      QDirIterator it(recoveryDir, QDir::Files, QDirIterator::Subdirectories);
      while (it.hasNext()) {
        const QString filePath = it.next();
        const QFileInfo fi(filePath);
        ts << "- " << fi.fileName() << " | " << fi.size() << " bytes | " << fi.lastModified().toString(Qt::ISODate) << "\n";
        ++snapshotCount;
      }
    }
    if (snapshotCount == 0) {
      ts << "(none)\n";
    }
    ts << "\n";

    ts << "[Dock Widgets]\n";
    int dockCount = 0;
    if (auto* top = this->window()) {
      const auto objects = top->findChildren<QObject*>();
      for (auto* obj : objects) {
        if (!obj) continue;
        const QString className = obj->metaObject() ? QString::fromLatin1(obj->metaObject()->className()) : QString();
        if (!className.contains(QStringLiteral("CDockWidget"), Qt::CaseInsensitive)) {
          continue;
        }
        auto* widget = qobject_cast<QWidget*>(obj);
        const QString title = obj->property("windowTitle").toString();
        ts << "- " << (title.isEmpty() ? obj->objectName() : title)
           << " | object=" << obj->objectName()
           << " | visible=" << ((widget && widget->isVisible()) ? "true" : "false")
           << "\n";
        ++dockCount;
      }
    }
    if (dockCount == 0) {
      ts << "(none)\n";
    }
    ts << "\n";

    ts << "[Menu Actions]\n";
    int actionCount = 0;
    if (auto* top = this->window()) {
      auto* menuBar = top->findChild<QMenuBar*>();
      if (menuBar) {
        const auto topMenus = menuBar->actions();
        for (auto* menuAction : topMenus) {
          if (!menuAction) continue;
          auto* menu = menuAction->menu();
          if (!menu) continue;
          ts << "Menu: " << menu->title() << "\n";
          const auto actions = menu->actions();
          for (auto* action : actions) {
            if (!action || action->isSeparator()) continue;
            ts << "  - " << action->text()
               << " | enabled=" << (action->isEnabled() ? "true" : "false")
               << " | checked=" << (action->isCheckable() ? (action->isChecked() ? "true" : "false") : "n/a")
               << "\n";
            ++actionCount;
          }
        }
      }
    }
    if (actionCount == 0) {
      ts << "(none)\n";
    }
    ts << "\n";

    ts << "[Notes]\n";
    ts << "- If previous session crashed, check Recovery folder and RenderQueue history.\n";
    ts << "- Attach this report when filing bug reports.\n";
    out.close();

    QMessageBox::information(this, tr("Diagnostics"), tr("Diagnostics exported:\n%1").arg(path));
  });
 }

 ArtifactHelpMenu::~ArtifactHelpMenu()
 {
  delete impl_;
 }

};

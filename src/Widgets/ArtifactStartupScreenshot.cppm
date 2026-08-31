module;

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>
#include <QSettings>
#include <QStandardPaths>
#include <QWidget>

export module Artifact.Widgets.StartupScreenshot;

export namespace Artifact {

namespace {
constexpr auto kStartupScreenshotEnabledKey =
    "UI/Diagnostics/CaptureStartupScreenshot";
}

bool startupScreenshotEnabled() {
  const QByteArray environmentValue =
      qgetenv("ARTIFACT_STARTUP_SCREENSHOT").trimmed().toLower();
  if (!environmentValue.isEmpty()) {
    return environmentValue != QByteArrayLiteral("0") &&
           environmentValue != QByteArrayLiteral("false") &&
           environmentValue != QByteArrayLiteral("off");
  }
  return QSettings().value(QString::fromLatin1(kStartupScreenshotEnabledKey),
                           true).toBool();
}

void captureStartupScreenshot(QWidget *window) {
  if (!window || !window->isVisible() || window->isMinimized()) {
    return;
  }

  QString appData =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (appData.isEmpty()) {
    appData = QDir::homePath();
  }
  QDir outputDir(QDir(appData).filePath(
      QStringLiteral("StartupScreenshots")));
  if (!outputDir.mkpath(QStringLiteral("."))) {
    qWarning() << "[StartupScreenshot] failed to create directory"
               << outputDir.absolutePath();
    return;
  }

  const QPixmap screenshot = window->grab();
  if (screenshot.isNull()) {
    qWarning() << "[StartupScreenshot] window capture returned an empty pixmap";
    return;
  }

  const QString filePath = outputDir.filePath(
      QStringLiteral("startup_%1.png")
          .arg(QDateTime::currentDateTime().toString(
              QStringLiteral("yyyyMMdd_HHmmss_zzz"))));
  if (!screenshot.save(filePath, "PNG")) {
    qWarning() << "[StartupScreenshot] failed to save" << filePath;
    return;
  }

  const QFileInfoList captures = outputDir.entryInfoList(
      {QStringLiteral("startup_*.png")}, QDir::Files, QDir::Time);
  for (int index = 10; index < captures.size(); ++index) {
    QFile::remove(captures[index].absoluteFilePath());
  }
  qInfo() << "[StartupScreenshot] saved" << filePath
          << "size=" << screenshot.size();
}

} // namespace Artifact

#pragma once

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QStandardPaths>
#include <QThread>

#include <utility>

namespace Artifact::WidgetCreationDiagnostics {

namespace Detail {

struct LogState {
  QMutex mutex;
  QString path;
  QFile file;
  bool pathAnnounced = false;
};

inline LogState &logState() {
  static LogState state;
  return state;
}

inline QString ensureLogPath(LogState &state) {
  if (!state.path.isEmpty()) {
    return state.path;
  }

  const QString appData =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir reportDir(appData);
  if (!reportDir.mkpath(QStringLiteral("WidgetCreationReports")) ||
      !reportDir.cd(QStringLiteral("WidgetCreationReports"))) {
    return {};
  }

  const QString startedAt = QDateTime::currentDateTimeUtc().toString(
      QStringLiteral("yyyyMMdd_HHmmss_zzz"));
  const qint64 processId = QCoreApplication::applicationPid();
  state.path = reportDir.filePath(
      QStringLiteral("widget-creation_%1_%2.jsonl").arg(startedAt).arg(processId));
  return state.path;
}

}  // namespace Detail

inline QString logFilePath() {
  auto &state = Detail::logState();
  QMutexLocker locker(&state.mutex);
  return Detail::ensureLogPath(state);
}

inline void record(QObject *widget, const QString &label,
                   const QString &stage, const QString &reason,
                   const double factoryMs, const double totalMs,
                   const QString &outcome = QStringLiteral("created"),
                   const QString &dockTitle = {}, const QString &dockId = {},
                   const QString &detail = {}) {
  QJsonObject event;
  event.insert(QStringLiteral("schema"),
               QStringLiteral("artifact-widget-creation-v1"));
  event.insert(QStringLiteral("timestampUtc"),
               QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
  event.insert(QStringLiteral("processId"),
               QString::number(QCoreApplication::applicationPid()));
  event.insert(QStringLiteral("label"), label);
  event.insert(QStringLiteral("stage"), stage);
  event.insert(QStringLiteral("reason"), reason);
  event.insert(QStringLiteral("outcome"), outcome);
  event.insert(QStringLiteral("factoryMs"), factoryMs);
  event.insert(QStringLiteral("totalMs"), totalMs);
  event.insert(QStringLiteral("slowThresholdMs"), 50.0);
  event.insert(QStringLiteral("slow"), totalMs >= 50.0);
  event.insert(QStringLiteral("dockTitle"), dockTitle);
  event.insert(QStringLiteral("dockId"), dockId);
  event.insert(QStringLiteral("detail"), detail);
  event.insert(QStringLiteral("widgetClass"),
               widget ? QString::fromLatin1(widget->metaObject()->className())
                      : QString());
  event.insert(QStringLiteral("widgetObjectName"),
               widget ? widget->objectName() : QString());
  auto *application = QCoreApplication::instance();
  event.insert(QStringLiteral("mainThread"),
               application && QThread::currentThread() == application->thread());

  const QByteArray payload = QJsonDocument(event).toJson(QJsonDocument::Compact) +
                             QByteArray(1, '\n');
  auto &state = Detail::logState();
  QMutexLocker locker(&state.mutex);
  const QString path = Detail::ensureLogPath(state);
  if (path.isEmpty()) {
    qWarning() << "[WidgetCreation] failed to resolve report path"
               << "label=" << label << "reason=" << reason;
    return;
  }

  if (!state.file.isOpen()) {
    state.file.setFileName(path);
    if (!state.file.open(QIODevice::WriteOnly | QIODevice::Append)) {
      qWarning() << "[WidgetCreation] failed to open" << path
                 << "label=" << label;
      return;
    }
  }
  if (state.file.write(payload) != payload.size()) {
    qWarning() << "[WidgetCreation] failed to append" << path
               << "label=" << label;
    return;
  }
  if (outcome != QStringLiteral("created") || totalMs >= 50.0) {
    state.file.flush();
  }

  if (!state.pathAnnounced) {
    state.pathAnnounced = true;
    qInfo() << "[WidgetCreation] report=" << path;
  }
  qInfo().noquote() << QStringLiteral(
                           "[WidgetCreation] label=%1 class=%2 reason=%3 "
                           "stage=%4 factoryMs=%5 totalMs=%6 outcome=%7")
                           .arg(label,
                                event.value(QStringLiteral("widgetClass"))
                                    .toString(),
                                reason, stage)
                           .arg(factoryMs, 0, 'f', 2)
                           .arg(totalMs, 0, 'f', 2)
                           .arg(outcome);
}

inline void recordPhase(const QString &label, const QString &stage,
                        const QString &reason, const double elapsedMs,
                        const QString &detail = {},
                        const QString &outcome = QStringLiteral("measured")) {
  record(nullptr, label, stage, reason, elapsedMs, elapsedMs, outcome, {}, {},
         detail);
}

template <typename Factory>
auto createMeasured(const QString &label, const QString &stage,
                    const QString &reason, Factory &&factory)
    -> decltype(factory()) {
  QElapsedTimer timer;
  timer.start();
  auto *widget = std::forward<Factory>(factory)();
  const double elapsedMs =
      static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
  record(widget, label, stage, reason, elapsedMs, elapsedMs,
         widget ? QStringLiteral("created") : QStringLiteral("factory-null"));
  return widget;
}

}  // namespace Artifact::WidgetCreationDiagnostics

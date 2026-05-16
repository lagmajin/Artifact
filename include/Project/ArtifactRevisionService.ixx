module;
#include <memory>
#include <vector>

#include <QObject>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

export module Artifact.Project.RevisionService;

import Artifact.Project.Manager;

export namespace Artifact {

struct ProjectRevisionRecord {
  QString id;
  QString parentId;
  QString message;
  QString author;
  QString projectKey;
  QString projectName;
  QString snapshotFile;
  QDateTime timestampUtc;
  qint64 snapshotBytes = 0;
  QStringList tags;
};

class ArtifactRevisionService : public QObject {
public:
  explicit ArtifactRevisionService(QObject* parent = nullptr);
  ~ArtifactRevisionService() override;

  static ArtifactRevisionService* instance();

  void setAutoCommitEnabled(bool enabled);
  bool autoCommitEnabled() const;
  void setAutoCommitDelayMs(int delayMs);
  int autoCommitDelayMs() const;
  void suspendAutoCommit(bool suspend);
  bool autoCommitSuspended() const;

  QString projectKey() const;
  QString storageRoot() const;
  QString revisionsRoot() const;

  QVector<ProjectRevisionRecord> revisions() const;
  QStringList revisionIds() const;
  QString headRevisionId() const;

  bool commitCurrentProject(const QString& message = QString(),
                           const QString& author = QString(),
                           const QStringList& tags = {});
  bool restoreRevision(const QString& revisionId);

  QJsonObject revisionSnapshot(const QString& revisionId) const;
  QJsonObject diffRevisions(const QString& leftRevisionId,
                            const QString& rightRevisionId) const;

  void noteProjectChanged();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}

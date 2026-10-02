module;
#include <memory>
#include <vector>

#include <QObject>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

export module Artifact.Project.RevisionService;

import Artifact.Project.Manager;

export namespace Artifact {

struct ProjectRevisionRecord {
  QString id;
  QString parentId;
  // Merge commits carry more than one parent. The common case stays in
  // parentId so existing records and single-parent tooling keep working.
  QStringList parentIds;
  QString message;
  QString author;
  QString projectKey;
  QString projectName;
  QString snapshotFile;
  QDateTime timestampUtc;
  qint64 snapshotBytes = 0;
  QStringList tags;
};

struct RevisionBranchRef {
  QString id;
  QString displayName;
  QString headRevisionId;
  QString baseRevisionId;
  QDateTime createdAt;
  QDateTime updatedAt;
};

// One unresolved difference found while merging two histories. The path uses
// the same identity notation as diffRevisions() so both views read alike.
struct RevisionMergeConflict {
  enum class Kind { Content, DeleteModify, Order };
  Kind kind = Kind::Content;
  QString path;
  QString oursLayerId;
  QString theirsLayerId;
  QJsonValue baseValue;
  QJsonValue oursValue;
  QJsonValue theirsValue;
};

struct RevisionMergeResult {
  bool ok = false;
  QString mergeBaseRevisionId;
  QJsonObject mergedSnapshot;
  QVector<RevisionMergeConflict> conflicts;
  // Paths that a heuristic settled without asking, so the UI can say what
  // happened instead of the edit appearing to vanish.
  QVector<QPair<QString, QString>> autoResolved;
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
  // Creates a merge commit. parentRevisionIds must hold the two heads being
  // combined; the snapshot is supplied by the caller because automatic
  // snapshot merging is a separate decision.
  bool commitMerge(const QJsonObject& snapshot,
                   const QStringList& parentRevisionIds,
                   const QString& message = QString(),
                   const QString& author = QString());
  bool restoreRevision(const QString& revisionId);

  // Branch refs are named pointers at a revision. They let several lines of
  // work coexist without the linear history rewriting earlier commits.
  QVector<RevisionBranchRef> branches() const;
  QString currentBranchId() const;
  bool setCurrentBranchId(const QString& branchId);
  bool createBranch(const QString& displayName,
                    const QString& fromRevisionId = QString(),
                    const QString& branchId = QString());
  bool renameBranch(const QString& branchId, const QString& displayName);
  bool moveBranchHead(const QString& branchId, const QString& revisionId);
  bool deleteBranch(const QString& branchId);
  QStringList ancestorRevisions(const QString& revisionId) const;
  QStringList commonAncestorRevisions(const QString& leftRevisionId,
                                      const QString& rightRevisionId) const;

  // Three-way merge of two revisions. ours wins conflicts so the result is
  // always defined; the caller decides whether to accept the conflicts by
  // reviewing them before committing.
  RevisionMergeResult mergeRevisions(const QString& oursRevisionId,
                                     const QString& theirsRevisionId,
                                     const QStringList& oursPreferredPaths = {});
  bool mergeRevisionIntoProject(const QString& oursRevisionId,
                                const QString& theirsRevisionId,
                                const QStringList& resolvedPaths = {},
                                const QString& message = QString());

  QJsonObject revisionSnapshot(const QString& revisionId) const;
  QJsonObject diffRevisions(const QString& leftRevisionId,
                            const QString& rightRevisionId) const;

  void noteProjectChanged();

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}

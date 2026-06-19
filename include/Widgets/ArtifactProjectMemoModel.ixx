module;
#include <QColor>
#include <QString>
#include <QDateTime>
#include <QUuid>

export module Artifact.Widgets.ProjectMemoModel;

import std;

export namespace Artifact {

struct ProjectMemo {
  QUuid id;
  qint64 frame = 0;
  QString text;
  QColor color;
  QDateTime createdAt;
};

class ArtifactProjectMemoModel {
public:
  ArtifactProjectMemoModel();
  ~ArtifactProjectMemoModel();

  void setCompositionId(const QString &compositionId);
  QString compositionId() const;

  QList<ProjectMemo> memos() const;
  void addMemo(qint64 frame, const QString &text, const QColor &color);
  void removeMemo(const QUuid &id);
  void updateMemo(const QUuid &id, const QString &text, const QColor &color);

private:
  class Impl;
  Impl *impl_;
};

} // namespace Artifact

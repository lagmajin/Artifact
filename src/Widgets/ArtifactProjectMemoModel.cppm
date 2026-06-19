module;
#include <QColor>
#include <QDateTime>
#include <QString>
#include <QUuid>

module Artifact.Widgets.ProjectMemoModel;

import std;

namespace Artifact {

class ArtifactProjectMemoModel::Impl {
public:
  QString compositionId_;
  QList<ProjectMemo> memos_;
};

ArtifactProjectMemoModel::ArtifactProjectMemoModel()
    : impl_(new Impl()) {}

ArtifactProjectMemoModel::~ArtifactProjectMemoModel() {
  delete impl_;
}

void ArtifactProjectMemoModel::setCompositionId(const QString &compositionId) {
  impl_->compositionId_ = compositionId;
}

QString ArtifactProjectMemoModel::compositionId() const {
  return impl_->compositionId_;
}

QList<ProjectMemo> ArtifactProjectMemoModel::memos() const {
  return impl_->memos_;
}

void ArtifactProjectMemoModel::addMemo(qint64 frame, const QString &text,
                                       const QColor &color) {
  ProjectMemo memo;
  memo.id = QUuid::createUuid();
  memo.frame = frame;
  memo.text = text;
  memo.color = color.isValid() ? color : QColor(255, 220, 100);
  memo.createdAt = QDateTime::currentDateTimeUtc();
  impl_->memos_.append(memo);
}

void ArtifactProjectMemoModel::removeMemo(const QUuid &id) {
  for (auto it = impl_->memos_.begin(); it != impl_->memos_.end(); ++it) {
    if (it->id == id) {
      impl_->memos_.erase(it);
      return;
    }
  }
}

void ArtifactProjectMemoModel::updateMemo(const QUuid &id, const QString &text,
                                          const QColor &color) {
  for (auto &memo : impl_->memos_) {
    if (memo.id == id) {
      memo.text = text;
      memo.color = color.isValid() ? color : memo.color;
      return;
    }
  }
}

} // namespace Artifact

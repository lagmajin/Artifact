module;
#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Widgets.ProjectMemoWidget;

import Artifact.Widgets.ProjectMemoModel;

export namespace Artifact {

class ArtifactProjectMemoWidget : public QWidget {
  W_OBJECT(ArtifactProjectMemoWidget)
public:
  explicit ArtifactProjectMemoWidget(QWidget *parent = nullptr);
  ~ArtifactProjectMemoWidget();

  void setCompositionId(const QString &compositionId);
  void setCurrentFrame(qint64 frame);

public:
  void memoJumpRequested(qint64 frame) W_SIGNAL(memoJumpRequested, frame);

private:
  class Impl;
  Impl *impl_;
};

} // namespace Artifact

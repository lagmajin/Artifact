module;
#include <QWidget>
#include <QMouseEvent>
#include <QPaintEvent>
#include <wobjectdefs.h>

export module Artifact.Widgets.AnimationTimelineWidget;

import Utils.Id;

export namespace Artifact {

class ArtifactAnimationTimelineWidget : public QWidget {
  W_OBJECT(ArtifactAnimationTimelineWidget)
public:
  explicit ArtifactAnimationTimelineWidget(QWidget* parent = nullptr);
  ~ArtifactAnimationTimelineWidget() override;

  void setComposition(const ArtifactCore::CompositionID& compositionId);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  class Impl;
  Impl* impl_ = nullptr;
  void refresh();
  bool commitClipRetime();
};

} // namespace Artifact

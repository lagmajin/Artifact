module;

#include <QMouseEvent>
#include <QString>
#include <QToolButton>
#include <QWidget>

#include <functional>
#include <vector>

export module Artifact.Widgets.CompositionCleanup;

import Artifact.Composition.Abstract;
import Artifact.Widgets.CompositionRenderController;

export namespace Artifact {

class ViewportLayoutButton final : public QToolButton {
public:
  explicit ViewportLayoutButton(QWidget* parent = nullptr);

  void setActivatedCallback(std::function<void()> callback);

protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  std::function<void()> activatedCallback_;
  bool pressedInside_ = false;
};

struct CompositionCleanupMove {
  QString layerId;
  float beforeX = 0.0f;
  float beforeY = 0.0f;
  float afterX = 0.0f;
  float afterY = 0.0f;
};

struct CompositionCleanupCandidate {
  QString ruleId;
  QString message;
  QString actionLabel;
  std::vector<CompositionCleanupMove> moves;
};

std::vector<CompositionCleanupCandidate> analyzeCompositionCleanup(
    const ArtifactCompositionPtr& composition);
bool applyCompositionCleanupCandidate(
    const ArtifactCompositionPtr& composition,
    const CompositionCleanupCandidate& candidate);
void previewCompositionCleanupCandidate(
    CompositionRenderController* controller,
    const ArtifactCompositionPtr& composition,
    const CompositionCleanupCandidate& candidate);
void showCompositionCleanupDialog(
    QWidget* parent,
    CompositionRenderController* controller,
    const ArtifactCompositionPtr& composition);

}

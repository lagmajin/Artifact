module;
#include <QWidget>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <wobjectdefs.h>

export module Artifact.Widgets.AudioMiniWidget;

import Utils.Id;

export namespace Artifact {

class ArtifactAudioMiniWidget : public QWidget {
  W_OBJECT(ArtifactAudioMiniWidget)
public:
  explicit ArtifactAudioMiniWidget(QWidget* parent = nullptr);
  ~ArtifactAudioMiniWidget() override;

  void setComposition(const ArtifactCore::CompositionID& compositionId);

protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

private:
  class Impl;
  Impl* impl_ = nullptr;
  void refresh();
  void seekBeat(bool next);
  bool addGuideMarkers(bool everyFourBeats);
};

} // namespace Artifact

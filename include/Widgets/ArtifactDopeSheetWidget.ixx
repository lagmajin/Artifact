module;

#include <QWidget>
#include <QString>

export module Artifact.Widgets.DopeSheetWidget;

import Utils.Id;

export namespace Artifact {

class ArtifactDopeSheetWidget : public QWidget {
private:
  class Impl;
  Impl* impl_ = nullptr;

public:
  explicit ArtifactDopeSheetWidget(QWidget* parent = nullptr);
  ~ArtifactDopeSheetWidget() override;

  void setComposition(const ArtifactCore::CompositionID& id);
  ArtifactCore::CompositionID composition() const;

  void setCurrentFrame(int64_t frame);
  int64_t currentFrame() const;

  void refreshFromCurrentContext();
};

} // namespace Artifact

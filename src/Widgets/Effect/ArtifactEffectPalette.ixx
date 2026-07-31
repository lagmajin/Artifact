module;
#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Widgets.EffectPalette;

export namespace Artifact {

class ArtifactEffectPalette : public QWidget {
  W_OBJECT(ArtifactEffectPalette)
public:
  explicit ArtifactEffectPalette(QWidget* parent = nullptr);
  ~ArtifactEffectPalette() override;

  void refreshEffects();
private:
  class Impl;
  Impl* impl_ = nullptr;
};

}

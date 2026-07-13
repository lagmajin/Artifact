module;
#include <QWidget>

export module Artifact.Widgets.Inspector.EffectTabSurface;

export namespace Artifact {
class ArtifactEffectTabSurface final : public QWidget {
 public:
  explicit ArtifactEffectTabSurface(QWidget* stackPanel,
                                    QWidget* detailPanel,
                                    QWidget* parent = nullptr);
};
}

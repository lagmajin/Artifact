module;
#include <QWidget>

export module Artifact.Widgets.Inspector.ComponentTabSurface;

export namespace Artifact {
class ArtifactComponentTabSurface final : public QWidget {
 public:
  explicit ArtifactComponentTabSurface(QWidget* componentPanel,
                                       QWidget* parent = nullptr);
};
}

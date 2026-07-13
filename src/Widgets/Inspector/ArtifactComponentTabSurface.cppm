module;
#include <QVBoxLayout>

module Artifact.Widgets.Inspector.ComponentTabSurface;

namespace Artifact {
ArtifactComponentTabSurface::ArtifactComponentTabSurface(QWidget* componentPanel,
                                                         QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  if (componentPanel) layout->addWidget(componentPanel, 1);
}
}

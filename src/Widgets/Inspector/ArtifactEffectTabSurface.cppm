module;
#include <QVBoxLayout>

module Artifact.Widgets.Inspector.EffectTabSurface;

namespace Artifact {
ArtifactEffectTabSurface::ArtifactEffectTabSurface(QWidget* stackPanel,
                                                   QWidget* detailPanel,
                                                   QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(1);
  if (stackPanel) layout->addWidget(stackPanel);
  if (detailPanel) layout->addWidget(detailPanel, 1);
}
}

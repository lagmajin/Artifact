module;
#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

module Artifact.Widgets.Inspector.ComponentTabSurface;

namespace Artifact {
ArtifactComponentTabSurface::ArtifactComponentTabSurface(QWidget* componentPanel,
                                                         QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);
  auto* title = new QLabel(QStringLiteral("Components"), this);
  auto titleFont = title->font();
  titleFont.setPointSize(14);
  titleFont.setWeight(QFont::DemiBold);
  title->setFont(titleFont);
  layout->addWidget(title);
  if (componentPanel) layout->addWidget(componentPanel, 1);
}
}

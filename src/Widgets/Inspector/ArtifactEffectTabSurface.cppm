module;
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QFont>

module Artifact.Widgets.Inspector.EffectTabSurface;

namespace Artifact {
ArtifactEffectTabSurface::ArtifactEffectTabSurface(QWidget* stackPanel,
                                                   QWidget* detailPanel,
                                                   QWidget* parent)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  auto* header = new QFrame(this);
  auto* headerLayout = new QVBoxLayout(header);
  headerLayout->setContentsMargins(10, 10, 10, 10);
  headerLayout->setSpacing(4);
  auto* title = new QLabel(QStringLiteral("Effects"), header);
  auto titleFont = title->font();
  titleFont.setPointSize(14);
  titleFont.setWeight(QFont::DemiBold);
  title->setFont(titleFont);
  headerLayout->addWidget(title);
  auto* hint = new QLabel(
      QStringLiteral("Build the selected layer from ordered effects."), header);
  hint->setWordWrap(true);
  headerLayout->addWidget(hint);
  layout->addWidget(header);
  if (stackPanel) layout->addWidget(stackPanel);
  if (detailPanel) layout->addWidget(detailPanel, 1);
}
}

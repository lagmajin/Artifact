module;
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <wobjectimpl.h>

module Artifact.Widgets.Timeline.Label;

namespace Artifact
{
W_OBJECT_IMPL(ArtifactTimelineBottomLabel)

class ArtifactTimelineBottomLabel::Impl
{
public:
  QLabel* frameRenderingLabel = nullptr;
};

ArtifactTimelineBottomLabel::ArtifactTimelineBottomLabel(QWidget* parent)
    : QWidget(parent)
{
  auto* layout = new QHBoxLayout();
  setLayout(layout);
  setFixedHeight(28);
}

ArtifactTimelineBottomLabel::~ArtifactTimelineBottomLabel() = default;

} // namespace Artifact

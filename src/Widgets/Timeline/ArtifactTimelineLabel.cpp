module;
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QPalette>
#include <QFont>
#include <QColor>
#include <wobjectimpl.h>
#include <algorithm>
#include <cmath>

module Artifact.Widgets.Timeline.Label;

import Widgets.Utils.CSS;

namespace Artifact
{
W_OBJECT_IMPL(ArtifactTimelineBottomLabel)

class ArtifactTimelineBottomLabel::Impl
{
public:
  QLabel* compositionLabel = nullptr;
  QLabel* fpsLabel = nullptr;
  QLabel* frameLabel = nullptr;
  QLabel* zoomLabel = nullptr;
};

ArtifactTimelineBottomLabel::ArtifactTimelineBottomLabel(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
  setObjectName(QStringLiteral("timelineBottomSummary"));
  setAutoFillBackground(true);
  const auto& theme = ArtifactCore::currentDCCTheme();
  QPalette panelPalette = palette();
  panelPalette.setColor(QPalette::Window,
                        QColor(theme.backgroundColor).darker(106));
  panelPalette.setColor(QPalette::WindowText, QColor(theme.textColor));
  setPalette(panelPalette);

  auto* layout = new QHBoxLayout();
  layout->setContentsMargins(20, 0, 20, 0);
  layout->setSpacing(12);
  setLayout(layout);
  setFixedHeight(48);

  impl_->compositionLabel = new QLabel(QStringLiteral("Composition"), this);
  impl_->fpsLabel = new QLabel(QStringLiteral("24 fps"), this);
  impl_->frameLabel = new QLabel(QStringLiteral("Frame 0"), this);
  impl_->zoomLabel = new QLabel(QStringLiteral("Zoom 100%"), this);

  QFont primaryFont = impl_->compositionLabel->font();
  primaryFont.setWeight(QFont::DemiBold);
  impl_->compositionLabel->setFont(primaryFont);
  impl_->frameLabel->setFont(primaryFont);

  const QColor text(theme.textColor);
  const QColor muted = text.darker(135);
  for (auto* label : {impl_->compositionLabel, impl_->fpsLabel,
                      impl_->frameLabel, impl_->zoomLabel}) {
    QPalette labelPalette = label->palette();
    labelPalette.setColor(QPalette::WindowText, muted);
    label->setPalette(labelPalette);
    label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  }
  {
    QPalette labelPalette = impl_->compositionLabel->palette();
    labelPalette.setColor(QPalette::WindowText, text);
    impl_->compositionLabel->setPalette(labelPalette);
  }

  layout->addWidget(impl_->compositionLabel);
  layout->addWidget(impl_->fpsLabel);
  layout->addStretch(1);
  layout->addWidget(impl_->frameLabel);
  layout->addWidget(impl_->zoomLabel);
}

ArtifactTimelineBottomLabel::~ArtifactTimelineBottomLabel()
{
  delete impl_;
}

void ArtifactTimelineBottomLabel::setCompositionSummary(const QString& name,
                                                        const double fps)
{
  if (!impl_) return;
  const QString trimmed = name.trimmed();
  impl_->compositionLabel->setText(trimmed.isEmpty()
                                       ? QStringLiteral("Composition")
                                       : trimmed);
  impl_->fpsLabel->setText(QStringLiteral("%1 fps")
                               .arg(QString::number(fps, 'f',
                                                   std::abs(fps - std::round(fps)) < 0.001
                                                       ? 0
                                                       : 2)));
}

void ArtifactTimelineBottomLabel::setCurrentFrame(const int frame)
{
  if (impl_) impl_->frameLabel->setText(QStringLiteral("Frame %1").arg(frame));
}

void ArtifactTimelineBottomLabel::setZoomPercent(const double percent)
{
  if (!impl_) return;
  impl_->zoomLabel->setText(QStringLiteral("Zoom %1%")
                                .arg(QString::number(std::clamp(percent, 1.0, 6400.0),
                                                     'f', 0)));
}

} // namespace Artifact

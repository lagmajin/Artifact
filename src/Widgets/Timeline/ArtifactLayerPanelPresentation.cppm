module;
#include <QApplication>
#include <QMessageBox>
#include <QScreen>
#include <QString>

module Artifact.Widgets.LayerPanelWidget;

import Artifact.Layer.Abstract;
import Artifact.Layer.Audio;
import Artifact.Layer.Composition;
import Artifact.Layer.CompositionBackground;
import Artifact.Layer.Camera;
import Artifact.Layer.Light;
import Artifact.Layer.Image;
import Artifact.Layer.Paint;
import Artifact.Layer.Particle;
import Artifact.Layer.FormParticle;
import Artifact.Layer.Group;
import Artifact.Layer.ParametricComposition;
import Artifact.Layer.Video;

namespace Artifact {

QMessageBox::StandardButton centeredQuestion(QWidget* parent,
                                             const QString& title,
                                             const QString& text)
{
  QMessageBox box(parent ? parent->window() : nullptr);
  box.setWindowTitle(title);
  box.setIcon(QMessageBox::Question);
  box.setText(text);
  box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
  box.setDefaultButton(QMessageBox::No);
  box.adjustSize();

  if (const QScreen* screen = QApplication::primaryScreen()) {
    const QRect available = screen->availableGeometry();
    const QSize dialogSize = box.sizeHint();
    box.move(available.center() - QPoint(dialogSize.width() / 2, dialogSize.height() / 2));
  }

  return static_cast<QMessageBox::StandardButton>(box.exec());
}

LayerPresentationDescriptor applyMatteSummary(const ArtifactAbstractLayerPtr& layer,
                                              LayerPresentationDescriptor descriptor)
{
  if (!layer) {
    return descriptor;
  }

  const int matteCount = static_cast<int>(layer->matteReferences().size());
  if (matteCount > 0) {
    descriptor.capabilitySummaryText = QStringLiteral("Matte x%1").arg(matteCount);
    descriptor.timelineBadgeText = QStringLiteral("%1 + Matte")
                                      .arg(descriptor.timelineBadgeText);
    descriptor.propertySummaryTitle += QStringLiteral(" · Matte Linked");
    descriptor.inspectorTypeLabel += QStringLiteral(" · Matte Linked");
    descriptor.badgeTone = LayerPresentationBadgeTone::Special;
  }
  return descriptor;
}

LayerPresentationDescriptor describeLayerPresentation(const ArtifactAbstractLayerPtr& layer)
{
  LayerPresentationDescriptor descriptor;
  descriptor.typeText = QStringLiteral("Layer");
  descriptor.timelineBadgeText = QStringLiteral("Layer");
  descriptor.propertySummaryTitle = QStringLiteral("Summary");
  descriptor.inspectorTypeLabel = QStringLiteral("Type: N/A");
  descriptor.capabilitySummaryText = QString();
  descriptor.badgeTone = LayerPresentationBadgeTone::Neutral;

  if (!layer) {
    return descriptor;
  }
  if (layer->isNullLayer()) {
    descriptor.typeText = QStringLiteral("Null Layer");
    descriptor.timelineBadgeText = QStringLiteral("Null");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Null Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Null Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Specialized");
    descriptor.badgeTone = LayerPresentationBadgeTone::Special;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (layer->isAdjustmentLayer()) {
    descriptor.typeText = QStringLiteral("Adjustment Layer");
    descriptor.timelineBadgeText = QStringLiteral("Adjust");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Adjustment Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Adjustment Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Specialized");
    descriptor.badgeTone = LayerPresentationBadgeTone::Motion;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (layer->isGroupLayer()) {
    const auto* group = dynamic_cast<const ArtifactGroupLayer*>(layer.get());
    const GroupOutputMode outputMode = group ? group->outputMode() : GroupOutputMode::All;
    if (outputMode == GroupOutputMode::Single) {
      descriptor.typeText = QStringLiteral("Multiplexer Group");
      descriptor.timelineBadgeText = QStringLiteral("Mux");
      descriptor.propertySummaryTitle = QStringLiteral("Summary · Multiplexer Group");
      descriptor.inspectorTypeLabel = QStringLiteral("Type: Multiplexer Group");
      descriptor.capabilitySummaryText = QStringLiteral("Exclusive Child");
    } else if (outputMode == GroupOutputMode::Share) {
      descriptor.typeText = QStringLiteral("Multiplexer Group (Share)");
      descriptor.timelineBadgeText = QStringLiteral("Share");
      descriptor.propertySummaryTitle = QStringLiteral("Summary · Multiplexer Group · Share");
      descriptor.inspectorTypeLabel = QStringLiteral("Type: Multiplexer Group · Share");
      descriptor.capabilitySummaryText = QStringLiteral("Children share 100%");
    } else {
      descriptor.typeText = QStringLiteral("Group Layer");
      descriptor.timelineBadgeText = QStringLiteral("Group");
      descriptor.propertySummaryTitle = QStringLiteral("Summary · Group Layer");
      descriptor.inspectorTypeLabel = QStringLiteral("Type: Group Layer");
      descriptor.capabilitySummaryText = QStringLiteral("Container");
    }
    descriptor.badgeTone = LayerPresentationBadgeTone::Container;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (layer->isCloneLayer()) {
    descriptor.typeText = QStringLiteral("Clone Layer");
    descriptor.timelineBadgeText = QStringLiteral("Clone");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Clone Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Clone Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Instanced");
    descriptor.badgeTone = LayerPresentationBadgeTone::Special;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (layer->isConstructionLayer()) {
    descriptor.typeText = QStringLiteral("Construction Layer");
    descriptor.timelineBadgeText = QStringLiteral("Const");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Construction Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Construction Layer");
    descriptor.capabilitySummaryText = layer->shouldIncludeInFinalRender()
        ? QStringLiteral("Editor + Export")
        : QStringLiteral("Editor only");
    descriptor.badgeTone = LayerPresentationBadgeTone::Special;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (dynamic_cast<ArtifactCompositionBackgroundLayer *>(layer.get())) {
    descriptor.typeText = QStringLiteral("Composition Background Layer");
    descriptor.timelineBadgeText = QStringLiteral("Bg");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Composition Background Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Composition Background Layer");
    descriptor.capabilitySummaryText = layer->shouldIncludeInFinalRender()
        ? QStringLiteral("Specialized + Export")
        : QStringLiteral("Specialized");
    descriptor.badgeTone = LayerPresentationBadgeTone::Special;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (dynamic_cast<ArtifactCameraLayer *>(layer.get())) {
    descriptor.typeText = QStringLiteral("Camera");
    descriptor.timelineBadgeText = QStringLiteral("Camera");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Camera");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Camera");
    descriptor.capabilitySummaryText = QStringLiteral("View control");
    descriptor.badgeTone = LayerPresentationBadgeTone::Motion;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (dynamic_cast<ArtifactLightLayer *>(layer.get())) {
    descriptor.typeText = QStringLiteral("Light");
    descriptor.timelineBadgeText = QStringLiteral("Light");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Light");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Light");
    descriptor.capabilitySummaryText = QStringLiteral("Scene lighting");
    descriptor.badgeTone = LayerPresentationBadgeTone::Motion;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (layer->is3D()) {
    descriptor.typeText = QStringLiteral("3D Model Layer");
    descriptor.timelineBadgeText = QStringLiteral("3D");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · 3D Model Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: 3D Model Layer");
    descriptor.capabilitySummaryText = QStringLiteral("3D Space");
    descriptor.badgeTone = LayerPresentationBadgeTone::Motion;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (dynamic_cast<ArtifactCompositionLayer *>(layer.get())) {
    descriptor.typeText = QStringLiteral("Precomp Layer");
    descriptor.timelineBadgeText = QStringLiteral("Precomp");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Precomp Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Precomp Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Nested Composition");
    descriptor.badgeTone = LayerPresentationBadgeTone::Container;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (dynamic_cast<ArtifactParametricCompositionLayer*>(layer.get())) {
    descriptor.typeText = QStringLiteral("Parametric Comp Layer");
    descriptor.timelineBadgeText = QStringLiteral("Parametric");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Parametric Comp Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Parametric Comp Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Slot-based Composition");
    descriptor.badgeTone = LayerPresentationBadgeTone::Container;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (dynamic_cast<ArtifactImageLayer *>(layer.get())) {
    descriptor.typeText = QStringLiteral("Image Layer");
    descriptor.timelineBadgeText = QStringLiteral("Image");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Image Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Image Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Image");
    descriptor.badgeTone = LayerPresentationBadgeTone::Media;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (dynamic_cast<ArtifactPaintLayer *>(layer.get())) {
    descriptor.typeText = QStringLiteral("Paint Layer");
    descriptor.timelineBadgeText = QStringLiteral("Paint");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Paint Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Paint Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Frame-by-frame raster");
    descriptor.badgeTone = LayerPresentationBadgeTone::Motion;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (dynamic_cast<ArtifactParticleLayer *>(layer.get()) ||
      dynamic_cast<ArtifactFormParticleLayer *>(layer.get())) {
    descriptor.typeText = QStringLiteral("Form Particle Layer");
    descriptor.timelineBadgeText = QStringLiteral("Form");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Form Particle Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Form Particle Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Particle");
    descriptor.badgeTone = LayerPresentationBadgeTone::Motion;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (dynamic_cast<ArtifactVideoLayer *>(layer.get())) {
    descriptor.typeText = QStringLiteral("Video Layer");
    descriptor.timelineBadgeText = QStringLiteral("Video");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Video Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Video Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Video");
    descriptor.badgeTone = LayerPresentationBadgeTone::Media;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (dynamic_cast<ArtifactAudioLayer *>(layer.get())) {
    descriptor.typeText = QStringLiteral("Audio Layer");
    descriptor.timelineBadgeText = QStringLiteral("Audio");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Audio Layer · Waveform Preview");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Audio Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Waveform preview");
    descriptor.badgeTone = LayerPresentationBadgeTone::Media;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (layer->hasAudio() && layer->hasVideo()) {
    descriptor.typeText = QStringLiteral("Audio-Video Layer");
    descriptor.timelineBadgeText = QStringLiteral("A/V");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Audio-Video Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Audio-Video Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Audio + Video");
    descriptor.badgeTone = LayerPresentationBadgeTone::Media;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (layer->hasAudio()) {
    descriptor.typeText = QStringLiteral("Audio Layer");
    descriptor.timelineBadgeText = QStringLiteral("Audio");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Audio Layer · Waveform Preview");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Audio Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Waveform preview");
    descriptor.badgeTone = LayerPresentationBadgeTone::Media;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  if (layer->hasVideo()) {
    descriptor.typeText = QStringLiteral("Video Layer");
    descriptor.timelineBadgeText = QStringLiteral("Video");
    descriptor.propertySummaryTitle = QStringLiteral("Summary · Video Layer");
    descriptor.inspectorTypeLabel = QStringLiteral("Type: Video Layer");
    descriptor.capabilitySummaryText = QStringLiteral("Video");
    descriptor.badgeTone = LayerPresentationBadgeTone::Media;
    descriptor = applyMatteSummary(layer, std::move(descriptor));
    return descriptor;
  }
  return applyMatteSummary(layer, std::move(descriptor));
}

QString describeLayerType(const ArtifactAbstractLayerPtr& layer)
{
  return describeLayerPresentation(layer).typeText;
}

} // namespace Artifact

module;
#include <QImage>
#include <QSize>
#include <QString>
#include <QPointF>

export module Artifact.Render.SoftwareCompositor;

import Layer.Blend;

export namespace Artifact::SoftwareRender {

enum class CompositeBackend {
 QtPainter,
 OpenCV
};

enum class CvEffectMode {
 None,
 GaussianBlur,
 EdgeOverlay
};

struct CompositeRequest {
 QImage background;
 QImage foreground;
 QImage overlay;
 QSize outputSize;
 ArtifactCore::BlendMode blendMode = ArtifactCore::BlendMode::Normal;
 CompositeBackend backend = CompositeBackend::QtPainter;
 CvEffectMode cvEffect = CvEffectMode::None;
 float overlayOpacity = 1.0f;
 QPointF overlayOffset = QPointF(0.0, 0.0);
 float overlayScale = 1.0f;
 float overlayRotationDeg = 0.0f;
 bool useForeground = true;
};

QImage compose(const CompositeRequest& request);
QString backendText(CompositeBackend backend);
QString blendModeText(ArtifactCore::BlendMode mode);
QString cvEffectText(CvEffectMode mode);

} // namespace Artifact::SoftwareRender

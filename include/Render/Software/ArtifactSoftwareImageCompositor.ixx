module;
#include <utility>
#include <QImage>
#include <QPainter>
#include <QSize>
#include <QString>
#include <QPointF>

export module Artifact.Render.SoftwareCompositor;

import std;
import Layer.Blend;
import Image.ImageF32x4_RGBA;

export namespace Artifact::SoftwareRender {

enum class CompositeBackend {
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
 CompositeBackend backend = CompositeBackend::OpenCV;
 CvEffectMode cvEffect = CvEffectMode::None;
 float overlayOpacity = 1.0f;
 QPointF overlayOffset = QPointF(0.0, 0.0);
 float overlayScale = 1.0f;
 float overlayRotationDeg = 0.0f;
 bool useForeground = true;
};

QImage compose(const CompositeRequest& request);
bool composeToBuffer(const CompositeRequest& request,
                     ArtifactCore::ImageF32x4_RGBA& output);
QString backendText(CompositeBackend backend);
QString blendModeText(ArtifactCore::BlendMode mode);
QString cvEffectText(CvEffectMode mode);

// QPainter has no native Subtract mode. The float/CV paths implement it
// exactly; QPainter consumers must fall back to SourceOver instead of the
// visually different Difference approximation.
QPainter::CompositionMode qPainterCompositionMode(ArtifactCore::BlendMode mode);

// True when QPainter's native CompositionMode reproduces the blend mode
// faithfully; false when callers must route through blendSurface() instead
// (Subtract, LinearBurn, Divide, Pin/Vivid/LinearLight, HardMix, LinearDodge,
// Classic*, HSL, Dissolve, Stencil, Silhouette collapse to SourceOver).
bool qPainterSupportsBlendMode(ArtifactCore::BlendMode mode);

// Blends `surface` over `canvas` in place with the float blend engine for
// modes QPainter cannot express (see qPainterSupportsBlendMode). Both images
// must be Format_RGBA8888 and the same size; `opacity` is the layer opacity.
// Returns false (no change) when qPainterSupportsBlendMode(mode) is true or
// the image contracts are violated.
bool blendSurface(QImage& canvas,
                  const QImage& surface,
                  float opacity,
                  ArtifactCore::BlendMode mode);

} // namespace Artifact::SoftwareRender

module;

#define QT_NO_KEYWORDS

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QString>

module Artifact.Widgets.CompositionRenderController;

import :Impl;
import :LayerDrawing;
import Artifact.Grid.System;
import Artifact.Render.IRenderer;
import Color.Float;
import Configuration.LayeredConfigStore;
import Math.Vec;

namespace Artifact {

using namespace CompositionRenderControllerDetail;

void CompositionRenderController::setClearColor(const FloatColor &color) {

  if (toQColor(impl_->viewportClearColor_) == toQColor(color)) {

    return;

  }

  impl_->viewportClearColor_ = color;

  if (impl_->renderer_) {

    impl_->renderer_->setClearColor(color);

  }

  impl_->invalidateBaseComposite();

  markRenderDirty();

}



FloatColor CompositionRenderController::clearColor() const {

  return impl_->viewportClearColor_;

}



void CompositionRenderController::setShowGrid(bool show) {

  if (impl_->showGrid_ == show) {

    return;

  }

  impl_->showGrid_ = show;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

void CompositionRenderController::setShowViewportRuler(bool show) {
  if (impl_->showViewportRuler_ == show) {
    return;
  }
  impl_->showViewportRuler_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isShowViewportRuler() const {
  return impl_->showViewportRuler_;
}

void CompositionRenderController::setViewportRulerUnit(ViewportRulerUnit unit) {
  if (impl_->viewportRulerUnit_ == unit) return;
  impl_->viewportRulerUnit_ = unit;
  impl_->viewportRulerCacheZoom_ = -1.0f;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

CompositionRenderController::ViewportRulerUnit
CompositionRenderController::viewportRulerUnit() const {
  return impl_->viewportRulerUnit_;
}

void CompositionRenderController::setViewportOriginMode(ViewportOriginMode mode) {
  if (impl_->viewportOriginMode_ == mode) return;
  impl_->viewportOriginMode_ = mode;
  impl_->viewportRulerCacheZoom_ = -1.0f;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

CompositionRenderController::ViewportOriginMode
CompositionRenderController::viewportOriginMode() const {
  return impl_->viewportOriginMode_;
}

void CompositionRenderController::setShowPixelGrid(bool show) {
  if (impl_->showPixelGrid_ == show) return;
  impl_->showPixelGrid_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isShowPixelGrid() const {
  return impl_->showPixelGrid_;
}

void CompositionRenderController::setShowOutsideComposition(bool show) {
  if (impl_->showOutsideComposition_ == show) return;
  impl_->showOutsideComposition_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isShowOutsideComposition() const {
  return impl_->showOutsideComposition_;
}

bool CompositionRenderController::isShowGrid() const {

  return impl_->showGrid_;

}

void CompositionRenderController::setLineDebugKindVisible(LineDebugKind kind,

                                                          bool visible) {

  if (!impl_) {

    return;

  }

  const size_t index = lineDebugKindIndex(kind);

  if (index >= impl_->lineDebugVisibility_.size()) {

    return;

  }

  if (impl_->lineDebugVisibility_[index] == visible) {

    return;

  }

  impl_->lineDebugVisibility_[index] = visible;
  const QString settingKey = lineDebugVisibilitySettingKey(kind);
  if (!settingKey.isEmpty()) {
    ArtifactCore::LayeredConfigStore::instance().setValue(settingKey, visible);
  }

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

bool CompositionRenderController::isLineDebugKindVisible(

    LineDebugKind kind) const {

  if (!impl_) {

    return false;

  }

  return lineDebugKindVisible(impl_->lineDebugVisibility_, kind);

}

void CompositionRenderController::setShowCheckerboard(bool show) {

  const auto nextMode = show ? CompositionBackgroundMode::Checkerboard

                             : CompositionBackgroundMode::Solid;

  if (impl_->compositionBackgroundMode_ == nextMode) {

    return;

  }

  impl_->compositionBackgroundMode_ = nextMode;

  impl_->invalidateBaseComposite();

  markRenderDirty();

}

bool CompositionRenderController::isShowCheckerboard() const {

  return impl_->compositionBackgroundMode_ ==

         CompositionBackgroundMode::Checkerboard;

}

void CompositionRenderController::setCheckerboardSize(float size) {

  const float clamped = std::clamp(size, 2.0f, 128.0f);

  if (std::abs(impl_->checkerboardTileSize_ - clamped) <= 0.001f) {

    return;

  }

  impl_->checkerboardTileSize_ = clamped;

  impl_->invalidateBaseComposite();

  markRenderDirty();

}

float CompositionRenderController::checkerboardSize() const {

  return impl_->checkerboardTileSize_;

}

void CompositionRenderController::setGridSettings(

    const Artifact::Grid::GridSettings &settings) {

  impl_->gridSettings_ = settings;

  if (impl_->showGrid_) {

    impl_->invalidateOverlayComposite();

    markRenderDirty();

  }

}

Artifact::Grid::GridSettings CompositionRenderController::gridSettings() const {

  return impl_->gridSettings_;

}

void CompositionRenderController::setGridAutoStepEnabled(bool enabled) {

  if (impl_->gridAutoStepEnabled_ == enabled) return;

  impl_->gridAutoStepEnabled_ = enabled;
  impl_->invalidateOverlayComposite();
  markRenderDirty();

}

bool CompositionRenderController::isGridAutoStepEnabled() const {

  return impl_->gridAutoStepEnabled_;

}

void CompositionRenderController::setGridAutoStepTargetViewportInterval(
    float pixels) {

  const float clamped = std::clamp(pixels, 24.0f, 512.0f);
  if (std::abs(impl_->gridAutoStepTargetViewportInterval_ - clamped) < 0.01f) {
    return;
  }
  impl_->gridAutoStepTargetViewportInterval_ = clamped;
  impl_->invalidateOverlayComposite();
  markRenderDirty();

}

float CompositionRenderController::gridAutoStepTargetViewportInterval() const {

  return impl_->gridAutoStepTargetViewportInterval_;

}

void CompositionRenderController::setGridPolarMode(bool enabled) {

  if (impl_->gridPolarMode_ == enabled &&
      (!enabled || !impl_->gridIsometricMode_)) return;

  impl_->gridPolarMode_ = enabled;
  if (enabled) impl_->gridIsometricMode_ = false;
  impl_->invalidateOverlayComposite();
  markRenderDirty();

}

bool CompositionRenderController::isGridPolarMode() const {

  return impl_->gridPolarMode_;

}

void CompositionRenderController::setGridIsometricMode(bool enabled) {

  if (impl_->gridIsometricMode_ == enabled &&
      (!enabled || !impl_->gridPolarMode_)) return;

  impl_->gridIsometricMode_ = enabled;
  if (enabled) impl_->gridPolarMode_ = false;
  impl_->invalidateOverlayComposite();
  markRenderDirty();

}

bool CompositionRenderController::isGridIsometricMode() const {

  return impl_->gridIsometricMode_;

}

ArtifactCore::Coordinates::CompositionPoint2
CompositionRenderController::snapCanvasToGrid(
    ArtifactCore::Coordinates::CompositionPoint2 canvasPoint) const {

  const QPointF canvasPosition =
      ArtifactCore::Coordinates::toQPointF(canvasPoint);

  if (!impl_->gridSettings_.snapToGrid) return canvasPoint;

  const auto niceGridInterval = [](float raw) {
    if (!(raw > 0.0f) || !std::isfinite(raw)) return 1.0f;
    const float exponent = std::floor(std::log10(raw));
    const float scale = std::pow(10.0f, exponent);
    const float normalized = raw / scale;
    const float nice = normalized <= 1.0f ? 1.0f
        : normalized <= 2.0f ? 2.0f
        : normalized <= 5.0f ? 5.0f : 10.0f;
    return std::max(1.0f, nice * scale);
  };
  const float zoom = impl_->renderer_
      ? std::max(0.001f, impl_->renderer_->getZoom()) : 1.0f;
  const float majorSpacing = impl_->gridAutoStepEnabled_
      ? niceGridInterval(impl_->gridAutoStepTargetViewportInterval_ / zoom)
      : std::max(1.0f, impl_->gridSettings_.majorInterval);
  const float spacing = majorSpacing /
      static_cast<float>(std::max(1, impl_->gridSettings_.subdivisions));

  if (impl_->gridPolarMode_) {
    const float width = impl_->lastCanvasWidth_;
    const float height = impl_->lastCanvasHeight_;
    const QPointF center(width * 0.5, height * 0.5);
    const QPointF delta = canvasPosition - center;
    const float radius = std::hypot(static_cast<float>(delta.x()),
                                    static_cast<float>(delta.y()));
    const float angle = std::atan2(static_cast<float>(delta.y()),
                                   static_cast<float>(delta.x()));
    const float radial = std::round(radius / spacing) * spacing;
    const float angularStep = 15.0f * 0.017453292519943295f;
    const float snappedAngle = std::round(angle / angularStep) * angularStep;
    return ArtifactCore::Coordinates::compositionPointFromQPointF(
        center + QPointF(std::cos(snappedAngle) * radial,
                         std::sin(snappedAngle) * radial));
  }

  if (impl_->gridIsometricMode_) {
    // The overlay uses two oblique basis vectors; round in lattice
    // coordinates instead of independently rounding screen X/Y.  This keeps
    // the snap point on an actual intersection of the displayed grid.
    const float halfWidth = spacing * 0.5f;
    const float obliqueHeight = spacing * 0.57735026919f;
    if (obliqueHeight > 0.0001f) {
      const float latticeV = static_cast<float>(canvasPosition.y()) /
                             obliqueHeight;
      const float latticeU = (static_cast<float>(canvasPosition.x()) -
                              latticeV * halfWidth) / spacing;
      const float snappedU = std::round(latticeU);
      const float snappedV = std::round(latticeV);
      return {snappedU * spacing + snappedV * halfWidth,
              snappedV * obliqueHeight};
    }
  }

  return {std::round(static_cast<float>(canvasPosition.x()) / spacing) * spacing,
          std::round(static_cast<float>(canvasPosition.y()) / spacing) * spacing};
}

void CompositionRenderController::setCompositionBackgroundMode(int mode) {

  const auto backgroundMode = static_cast<CompositionBackgroundMode>(mode);

  if (impl_->compositionBackgroundMode_ == backgroundMode) {

    return;

  }

  impl_->compositionBackgroundMode_ = backgroundMode;

  impl_->invalidateBaseComposite();

  markRenderDirty();

}

int CompositionRenderController::compositionBackgroundMode() const {

  return static_cast<int>(impl_->compositionBackgroundMode_);

}

void CompositionRenderController::setShowGuides(bool show) {

  if (impl_->showGuides_ == show) {

    return;

  }

  impl_->showGuides_ = show;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

bool CompositionRenderController::isShowGuides() const {

  return impl_->showGuides_;

}

void CompositionRenderController::setSnapToGuides(bool snap) {

  if (impl_->snapToGuides_ == snap) {

    return;

  }

  impl_->snapToGuides_ = snap;
  if (!snap) {
    impl_->projectedFrameSnapVerticalValid_ = false;
    impl_->projectedFrameSnapHorizontalValid_ = false;
  }

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

bool CompositionRenderController::isSnapToGuides() const {

  return impl_->snapToGuides_;

}

void CompositionRenderController::setShowSafeMargins(bool show) {

  if (impl_->showSafeMargins_ == show) {

    return;

  }

  impl_->showSafeMargins_ = show;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}

bool CompositionRenderController::isShowSafeMargins() const {

  return impl_->showSafeMargins_;

}



void CompositionRenderController::setShowAnchorCenterOverlay(bool show) {

  if (impl_->showAnchorCenterOverlay_ == show) {

    return;

  }

  impl_->showAnchorCenterOverlay_ = show;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}



bool CompositionRenderController::isShowAnchorCenterOverlay() const {

  return impl_ ? impl_->showAnchorCenterOverlay_ : false;

}

void CompositionRenderController::setShowOriginOverlay(bool show) {
  if (impl_->showOriginOverlay_ == show) return;
  impl_->showOriginOverlay_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isShowOriginOverlay() const {
  return impl_ ? impl_->showOriginOverlay_ : false;
}

void CompositionRenderController::setMagnifierEnabled(bool enable) {
  if (impl_->magnifierEnabled_ == enable) return;
  impl_->magnifierEnabled_ = enable;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isMagnifierEnabled() const {
  return impl_ ? impl_->magnifierEnabled_ : false;
}

void CompositionRenderController::setMagnifierScale(int scale) {
  const int clamped = std::clamp(scale, 2, 8);
  if (impl_->magnifierScale_ == clamped) return;
  impl_->magnifierScale_ = clamped;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

int CompositionRenderController::magnifierScale() const {
  return impl_ ? impl_->magnifierScale_ : 2;
}

void CompositionRenderController::setMagnifierFollowCursor(bool follow) {
  if (impl_->magnifierFollowCursor_ == follow) return;
  impl_->magnifierFollowCursor_ = follow;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isMagnifierFollowCursor() const {
  return impl_ ? impl_->magnifierFollowCursor_ : false;
}

bool CompositionRenderController::adjustMagnifierScaleAt(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPosLogical,
    float delta) {
  if (!impl_ || !impl_->magnifierEnabled_ || delta == 0.0f) return false;
  QRectF loupe;
  if (!impl_->magnifierLoupeRect(loupe)) return false;
  const QPointF physical =
      ArtifactCore::Coordinates::toQPointF(viewportPosLogical) *
      impl_->devicePixelRatio_;
  if (!loupe.contains(physical)) return false;
  const int next =
      std::clamp(impl_->magnifierScale_ + (delta > 0.0f ? 1 : -1), 2, 8);
  // Consume the wheel over the loupe even at the clamp limits so the viewport
  // does not zoom while the pointer is over the magnifier.
  setMagnifierScale(next);
  return true;
}



void CompositionRenderController::setShowCameraFrustumOverlay(bool show) {

  if (impl_->showCameraFrustumOverlay_ == show) {

    return;

  }

  impl_->showCameraFrustumOverlay_ = show;

  impl_->invalidateOverlayComposite();

  markRenderDirty();

}



bool CompositionRenderController::isShowCameraFrustumOverlay() const {

  return impl_ ? impl_->showCameraFrustumOverlay_ : false;

}

void CompositionRenderController::setShowGizmoOverlay(bool show) {

  if (!impl_ || impl_->showGizmoOverlay_ == show) return;
  impl_->showGizmoOverlay_ = show;
  impl_->invalidateOverlayComposite();
  markRenderDirty();

}

bool CompositionRenderController::isShowGizmoOverlay() const {

  return impl_ ? impl_->showGizmoOverlay_ : true;

}

void CompositionRenderController::setShowXRayOverlay(bool show) {

  if (!impl_ || impl_->showXRayOverlay_ == show) return;
  impl_->showXRayOverlay_ = show;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/XRay/Enabled"), show);
  markRenderDirty();

}

bool CompositionRenderController::isShowXRayOverlay() const {

  return impl_ ? impl_->showXRayOverlay_ : false;

}

void CompositionRenderController::setXRayOverlayOpacity(float opacity) {

  if (!impl_) return;
  const float clamped = std::clamp(opacity, 0.0f, 1.0f);
  if (impl_->xRayOverlayOpacity_ == clamped) return;
  impl_->xRayOverlayOpacity_ = clamped;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/XRay/Opacity"), clamped);
  markRenderDirty();

}

float CompositionRenderController::xRayOverlayOpacity() const {

  return impl_ ? impl_->xRayOverlayOpacity_ : 0.35f;

}

void CompositionRenderController::setViewportShadingMode(
    CompositionViewportShadingMode mode) {

  if (!impl_ || impl_->viewportShadingMode_ == mode) return;
  impl_->viewportShadingMode_ = mode;
  if (impl_->renderer_) {
    // BoundingBox intentionally keeps the shaded mesh; its bounds are drawn
    // by the overlay pass (S3) instead of changing the mesh pipeline.
    impl_->renderer_->setMeshViewportShadingMode(
        mode == CompositionViewportShadingMode::Wireframe ? 1 :
        mode == CompositionViewportShadingMode::WireframeOnShaded ? 2 : 0);
  }
  impl_->invalidateBaseComposite();
  markRenderDirty();

}

CompositionViewportShadingMode
CompositionRenderController::viewportShadingMode() const {

  return impl_ ? impl_->viewportShadingMode_
               : CompositionViewportShadingMode::Shaded;

}

void CompositionRenderController::setViewportBackfaceCulling(bool enabled) {

  if (!impl_ || impl_->viewportBackfaceCulling_ == enabled) return;
  impl_->viewportBackfaceCulling_ = enabled;
  if (impl_->renderer_) {
    impl_->renderer_->setMeshBackfaceCulling(enabled);
  }
  impl_->invalidateBaseComposite();
  markRenderDirty();

}

bool CompositionRenderController::isViewportBackfaceCulling() const {

  return impl_ ? impl_->viewportBackfaceCulling_ : false;

}

void CompositionRenderController::setViewportAspectMask(
    CompositionViewportAspectMask mask) {

  if (!impl_ || impl_->viewportAspectMask_ == mask) return;
  impl_->viewportAspectMask_ = mask;
  markRenderDirty();

}

CompositionViewportAspectMask
CompositionRenderController::viewportAspectMask() const {

  return impl_ ? impl_->viewportAspectMask_
               : CompositionViewportAspectMask::Off;

}

void CompositionRenderController::setShowIsolationOverlay(bool show) {

  if (!impl_ || impl_->showIsolationOverlay_ == show) return;
  impl_->showIsolationOverlay_ = show;
  markRenderDirty();

}

bool CompositionRenderController::isShowIsolationOverlay() const {

  return impl_ ? impl_->showIsolationOverlay_ : false;

}

void CompositionRenderController::setShowOnionSkin(bool show) {

  if (impl_->showOnionSkin_ == show) return;
  impl_->showOnionSkin_ = show;
  impl_->resetOnionSkinHistory();
  impl_->invalidateOverlayComposite();
  markRenderDirty();

}

bool CompositionRenderController::isShowOnionSkin() const {

  return impl_->showOnionSkin_;

}

void CompositionRenderController::setViewportChannelDisplayMode(
    ViewportChannelDisplayMode mode) {

  if (!impl_ || impl_->viewportChannelDisplayMode_ == mode) {

    return;

  }

  impl_->viewportChannelDisplayMode_ = mode;
  impl_->syncViewportChannelReadbackConfiguration();

  markRenderDirty();

}

ViewportChannelDisplayMode
CompositionRenderController::viewportChannelDisplayMode() const {

  return impl_ ? impl_->viewportChannelDisplayMode_
               : ViewportChannelDisplayMode::Color;

}

namespace {
// Clamps the exposure controls to the ranges documented in
// docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md (P1-5). Non-finite
// input falls back to the identity value so a bad config file cannot produce a
// black or NaN viewport.
float sanitizeExposureGain(float value) {
  return std::isfinite(value) ? std::clamp(value, -6.0f, 6.0f) : 0.0f;
}
float sanitizeExposureGamma(float value) {
  return std::isfinite(value) ? std::clamp(value, 0.0f, 5.0f) : 1.0f;
}
float sanitizeExposureSaturation(float value) {
  return std::isfinite(value) ? std::clamp(value, 0.0f, 4.0f) : 1.0f;
}
float sanitizeClippingUnderThreshold(float value) {
  return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.01f;
}
float sanitizeClippingOverThreshold(float value) {
  return std::isfinite(value) ? std::clamp(value, 0.0f, 4.0f) : 1.0f;
}
} // namespace

void CompositionRenderController::setViewportExposureEnabled(bool enabled) {

  if (!impl_ || impl_->viewportExposureEnabled_ == enabled) {

    return;

  }

  impl_->viewportExposureEnabled_ = enabled;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/Exposure/Enabled"), enabled);
  markRenderDirty();

}

bool CompositionRenderController::isViewportExposureEnabled() const {

  return impl_ ? impl_->viewportExposureEnabled_ : false;

}

void CompositionRenderController::setViewportExposureGain(float gainStops) {

  if (!impl_) {

    return;

  }

  const float clamped = sanitizeExposureGain(gainStops);
  if (impl_->viewportExposureGain_ == clamped) {

    return;

  }

  impl_->viewportExposureGain_ = clamped;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/Exposure/Gain"), clamped);
  markRenderDirty();

}

float CompositionRenderController::viewportExposureGain() const {

  return impl_ ? impl_->viewportExposureGain_ : 0.0f;

}

void CompositionRenderController::setViewportExposureGamma(float gamma) {

  if (!impl_) {

    return;

  }

  const float clamped = sanitizeExposureGamma(gamma);
  if (impl_->viewportExposureGamma_ == clamped) {

    return;

  }

  impl_->viewportExposureGamma_ = clamped;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/Exposure/Gamma"), clamped);
  markRenderDirty();

}

float CompositionRenderController::viewportExposureGamma() const {

  return impl_ ? impl_->viewportExposureGamma_ : 1.0f;

}

void CompositionRenderController::setViewportExposureSaturation(float saturation) {

  if (!impl_) {

    return;

  }

  const float clamped = sanitizeExposureSaturation(saturation);
  if (impl_->viewportExposureSaturation_ == clamped) {

    return;

  }

  impl_->viewportExposureSaturation_ = clamped;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/Exposure/Saturation"), clamped);
  markRenderDirty();

}

float CompositionRenderController::viewportExposureSaturation() const {

  return impl_ ? impl_->viewportExposureSaturation_ : 1.0f;

}

void CompositionRenderController::setViewportClippingWarningsEnabled(
    bool enabled) {

  if (!impl_ || impl_->viewportClippingWarningsEnabled_ == enabled) {

    return;

  }

  impl_->viewportClippingWarningsEnabled_ = enabled;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/ClippingWarnings/Enabled"), enabled);
  markRenderDirty();

}

bool CompositionRenderController::isViewportClippingWarningsEnabled() const {

  return impl_ ? impl_->viewportClippingWarningsEnabled_ : false;

}

void CompositionRenderController::setViewportClippingUnderThreshold(
    float threshold) {

  if (!impl_) {

    return;

  }

  const float clamped = sanitizeClippingUnderThreshold(threshold);
  if (impl_->viewportClippingUnderThreshold_ == clamped) {

    return;

  }

  impl_->viewportClippingUnderThreshold_ = clamped;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/ClippingWarnings/UnderThreshold"), clamped);
  markRenderDirty();

}

float CompositionRenderController::viewportClippingUnderThreshold() const {

  return impl_ ? impl_->viewportClippingUnderThreshold_ : 0.01f;

}

void CompositionRenderController::setViewportClippingOverThreshold(
    float threshold) {

  if (!impl_) {

    return;

  }

  const float clamped = sanitizeClippingOverThreshold(threshold);
  if (impl_->viewportClippingOverThreshold_ == clamped) {

    return;

  }

  impl_->viewportClippingOverThreshold_ = clamped;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/ClippingWarnings/OverThreshold"), clamped);
  markRenderDirty();

}

float CompositionRenderController::viewportClippingOverThreshold() const {

  return impl_ ? impl_->viewportClippingOverThreshold_ : 1.0f;

}

void CompositionRenderController::Impl::syncViewportChannelReadbackConfiguration() {

  if (!renderer_) {
    return;
  }

  const bool needsAuxChannel =
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Color &&
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Alpha &&
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::ColorAlpha &&
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Red &&
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Green &&
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Blue &&
      // P1-6: Autograph-style variants only consume RGB/Alpha SRVs.
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Unpremultiplied &&
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Luminance &&
      viewportChannelDisplayMode_ != ViewportChannelDisplayMode::Matte;

  // Reset stale channel flags first: without this, switching e.g. Normal ->
  // Depth leaves NormalX/Y/Z enabled, keeping aux pipeline targets allocated
  // and multi-channel forced on even after returning to Color.
  renderer_->setMultiChannelEnabled(needsAuxChannel);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::Depth, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::Emission, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::ObjectId, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::MaterialId, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoR, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoG, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoB, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalX, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalY, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalZ, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::VelocityX, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::VelocityY, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::PositionX, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::PositionY, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::PositionZ, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::U, false);
  renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::V, false);

  switch (viewportChannelDisplayMode_) {
  case ViewportChannelDisplayMode::Depth:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::Depth, true);
    break;
  case ViewportChannelDisplayMode::Emission:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::Emission, true);
    break;
  case ViewportChannelDisplayMode::ObjectId:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::ObjectId, true);
    break;
  case ViewportChannelDisplayMode::MaterialId:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::MaterialId, true);
    break;
  case ViewportChannelDisplayMode::Albedo:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoR, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoG, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoB, true);
    break;
  case ViewportChannelDisplayMode::AlbedoR:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoR, true);
    break;
  case ViewportChannelDisplayMode::AlbedoG:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoG, true);
    break;
  case ViewportChannelDisplayMode::AlbedoB:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::AlbedoB, true);
    break;
  case ViewportChannelDisplayMode::Normal:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalX, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalY, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalZ, true);
    break;
  case ViewportChannelDisplayMode::NormalX:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalX, true);
    break;
  case ViewportChannelDisplayMode::NormalY:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalY, true);
    break;
  case ViewportChannelDisplayMode::NormalZ:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::NormalZ, true);
    break;
  case ViewportChannelDisplayMode::Velocity:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::VelocityX, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::VelocityY, true);
    break;
  case ViewportChannelDisplayMode::VelocityX:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::VelocityX, true);
    break;
  case ViewportChannelDisplayMode::VelocityY:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::VelocityY, true);
    break;
  case ViewportChannelDisplayMode::Position:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::PositionX, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::PositionY, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::PositionZ, true);
    break;
  case ViewportChannelDisplayMode::PositionX:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::PositionX, true);
    break;
  case ViewportChannelDisplayMode::PositionY:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::PositionY, true);
    break;
  case ViewportChannelDisplayMode::PositionZ:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::PositionZ, true);
    break;
  case ViewportChannelDisplayMode::UV:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::U, true);
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::V, true);
    break;
  case ViewportChannelDisplayMode::U:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::U, true);
    break;
  case ViewportChannelDisplayMode::V:
    renderer_->setChannelEnabled(ArtifactIRenderer::ChannelType::V, true);
    break;
  case ViewportChannelDisplayMode::Color:
  case ViewportChannelDisplayMode::Alpha:
  case ViewportChannelDisplayMode::Red:
  case ViewportChannelDisplayMode::Green:
  case ViewportChannelDisplayMode::Blue:
  // P1-6: Autograph-style channel variants consume only the RGB and
  // Alpha SRVs (no auxiliary channel). The post-process step that turns
  // RGB into unpremultiplied / luminance / matte runs in the presenter
  // (finalizeGpuRenderToViewport) into tempUAV, using the existing straight
  // conversion and displayComposite modes 4 / 5.
  case ViewportChannelDisplayMode::Unpremultiplied:
  case ViewportChannelDisplayMode::Luminance:
  case ViewportChannelDisplayMode::Matte:
    break;
  }

}

} // namespace Artifact

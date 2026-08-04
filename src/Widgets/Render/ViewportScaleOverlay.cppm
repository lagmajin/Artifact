module;
#include <algorithm>
#include <cmath>
#include <limits>

module Artifact.Widgets.Render.ViewportScaleOverlay;

namespace Artifact {
namespace {

QString formatStep(float value, const QString& unitName)
{
  const float absolute = std::abs(value);
  const int decimals = absolute < 1.0f ? 2 : (absolute < 10.0f ? 1 : 0);
  return QStringLiteral("%1 %2").arg(value, 0, 'f', decimals).arg(unitName);
}

float safeZoom(float zoom)
{
  return std::max(0.0001f, std::abs(zoom));
}

bool nearlyEqual(float left, float right)
{
  return std::abs(left - right) <= 0.00001f *
      std::max(1.0f, std::max(std::abs(left), std::abs(right)));
}

bool nearlyEqual(const QPointF& left, const QPointF& right)
{
  return nearlyEqual(static_cast<float>(left.x()),
                     static_cast<float>(right.x())) &&
         nearlyEqual(static_cast<float>(left.y()),
                     static_cast<float>(right.y()));
}

bool nearlyEqual(const QSizeF& left, const QSizeF& right)
{
  return nearlyEqual(static_cast<float>(left.width()),
                     static_cast<float>(right.width())) &&
         nearlyEqual(static_cast<float>(left.height()),
                     static_cast<float>(right.height()));
}

}

float ViewportTickCalculator::snapToNiceValue(float rawValue)
{
  if (!std::isfinite(rawValue) || rawValue <= 0.0f) {
    return 1.0f;
  }
  const float exponent = std::floor(std::log10(rawValue));
  const float magnitude = std::pow(10.0f, exponent);
  const float normalized = rawValue / magnitude;
  const float base = normalized <= 1.0f ? 1.0f
                     : normalized <= 2.0f ? 2.0f
                     : normalized <= 5.0f ? 5.0f
                                          : 10.0f;
  return base * magnitude;
}

ViewportTickStep ViewportTickCalculator::compute(float zoom, float targetPixels,
                                                 const QString& unitName)
{
  const float safeTarget = std::max(8.0f, targetPixels);
  const float rawValue = safeTarget / safeZoom(zoom);
  const float step = snapToNiceValue(rawValue);
  const float viewportPixels = step * safeZoom(zoom);
  const float sub = viewportPixels >= 40.0f ? step / 5.0f
                    : viewportPixels >= 20.0f ? step / 2.0f
                                               : 0.0f;
  const int subdivisions = sub > 0.0f
                               ? std::max(1, static_cast<int>(std::lround(step / sub)))
                               : 0;
  return ViewportTickStep{step, step, sub, formatStep(step, unitName), subdivisions};
}

std::vector<ViewportRulerTick> ViewportRulerData::generateTicks(
    float zoom, const QPointF& viewportOrigin, const QSizeF& viewportSize,
    const QSizeF& canvasSize, bool horizontal, float targetPixels,
    const QString& unitName)
{
  const ViewportTickStep step =
      ViewportTickCalculator::compute(zoom, targetPixels, unitName);
  const float origin = horizontal ? static_cast<float>(viewportOrigin.x())
                                  : static_cast<float>(viewportOrigin.y());
  const float extent = horizontal ? static_cast<float>(viewportSize.width())
                                  : static_cast<float>(viewportSize.height());
  const float canvasExtent = horizontal ? static_cast<float>(canvasSize.width())
                                        : static_cast<float>(canvasSize.height());
  std::vector<ViewportRulerTick> ticks;
  if (!std::isfinite(origin) || !std::isfinite(extent) ||
      !std::isfinite(canvasExtent) || !std::isfinite(step.interval) ||
      step.interval <= 0.0f || extent <= 0.0f || canvasExtent <= 0.0f) {
    return ticks;
  }
  const float visibleStart = std::max(0.0f, origin);
  const float viewportEnd = origin + extent;
  if (!std::isfinite(viewportEnd)) {
    return ticks;
  }
  const float visibleEnd = std::min(viewportEnd, canvasExtent);
  if (visibleEnd < visibleStart) {
    return ticks;
  }
  const float first = std::floor(visibleStart / step.interval) * step.interval;
  const float last = visibleEnd;

  const int count = std::min(4096, std::max(0, static_cast<int>(
      std::ceil((last - first) / std::max(step.subInterval, step.interval)) + 1.0f)));
  ticks.reserve(static_cast<size_t>(count));
  const float increment = step.subInterval > 0.0f ? step.subInterval : step.interval;
  if (!(increment > 0.0f) || !std::isfinite(increment)) {
    return ticks;
  }
  int emitted = 0;
  for (float canvasPos = first;
       canvasPos <= last + increment * 0.25f && emitted < 4096;
       canvasPos += increment) {
    if (canvasPos < 0.0f || canvasPos > canvasExtent) {
      continue;
    }
    const float majorRatio = canvasPos / step.interval;
    const float nearestMajor = std::round(majorRatio);
    const bool major = std::abs(majorRatio - nearestMajor) < 0.001f;
    const int subdivisionIndex = step.subInterval > 0.0f
                                     ? static_cast<int>(std::lround(
                                           canvasPos / step.subInterval))
                                     : 0;
    const bool mediumMinor = !major && step.subdivisionsPerMajor >= 5 &&
                             subdivisionIndex % 5 == 0;
    const auto level = major
                           ? ViewportRulerTickLevel::Major
                           : mediumMinor ? ViewportRulerTickLevel::Minor
                                         : ViewportRulerTickLevel::SubMinor;
    const float absoluteStep = std::abs(step.interval);
    const int labelDecimals = absoluteStep < 1.0f ? 2
                              : absoluteStep < 10.0f ? 1
                                                     : 0;
    const QString label = major
                              ? QStringLiteral("%1 %2")
                                    .arg(canvasPos, 0, 'f', labelDecimals)
                                    .arg(unitName)
                              : QString();
    const float viewportPos = (canvasPos - origin) * safeZoom(zoom);
    ticks.push_back(ViewportRulerTick{
        level, canvasPos, viewportPos, label});
    ++emitted;
  }
  return ticks;
}

ViewportScaleBarData ViewportScaleBarDataFactory::generate(
    float zoom, const QSizeF& viewportSize, float targetPixels,
    const QString& unitName)
{
  const ViewportTickStep step =
      ViewportTickCalculator::compute(zoom, targetPixels, unitName);
  const float width = step.interval * safeZoom(zoom);
  return ViewportScaleBarData{step.interval,
                              16.0f,
                              std::max(16.0f, static_cast<float>(viewportSize.height()) - 24.0f),
                              width,
                              step.label};
}

int ViewportOverlayManager::addRuler(const ViewportRulerConfig& config)
{
  const int id = nextId_++;
  rulers_.push_back(RulerEntry{id, config, true});
  return id;
}

int ViewportOverlayManager::addScaleBar(const ViewportScaleBarConfig& config)
{
  const int id = nextId_++;
  scaleBars_.push_back(ScaleBarEntry{id, config, config.visible});
  return id;
}

int ViewportOverlayManager::addGridLabel(const ViewportGridLabelConfig& config)
{
  const int id = nextId_++;
  gridLabels_.push_back(GridLabelEntry{id, config, config.visible});
  return id;
}

int ViewportOverlayManager::addCompass(const ViewportCompassConfig& config)
{
  const int id = nextId_++;
  compasses_.push_back(CompassEntry{id, config, config.visible});
  return id;
}

bool ViewportOverlayManager::configureRuler(
    int id, const ViewportRulerConfig& config)
{
  for (auto& entry : rulers_) {
    if (entry.id == id) {
      entry.config = config;
      entry.cacheValid = false;
      entry.cachedTicks.clear();
      return true;
    }
  }
  return false;
}

bool ViewportOverlayManager::configureScaleBar(
    int id, const ViewportScaleBarConfig& config)
{
  for (auto& entry : scaleBars_) {
    if (entry.id == id) {
      entry.config = config;
      entry.visible = config.visible;
      return true;
    }
  }
  return false;
}

bool ViewportOverlayManager::configureGridLabel(
    int id, const ViewportGridLabelConfig& config)
{
  for (auto& entry : gridLabels_) {
    if (entry.id == id) {
      entry.config = config;
      entry.visible = config.visible;
      return true;
    }
  }
  return false;
}

bool ViewportOverlayManager::configureCompass(
    int id, const ViewportCompassConfig& config)
{
  for (auto& entry : compasses_) {
    if (entry.id == id) {
      entry.config = config;
      entry.visible = config.visible;
      return true;
    }
  }
  return false;
}

void ViewportOverlayManager::remove(int id)
{
  rulers_.erase(std::remove_if(rulers_.begin(), rulers_.end(),
                               [id](const RulerEntry& entry) {
                                 return entry.id == id;
                               }),
                rulers_.end());
  scaleBars_.erase(std::remove_if(scaleBars_.begin(), scaleBars_.end(),
                                  [id](const ScaleBarEntry& entry) {
                                    return entry.id == id;
                                  }),
                   scaleBars_.end());
  gridLabels_.erase(std::remove_if(gridLabels_.begin(), gridLabels_.end(),
                                   [id](const GridLabelEntry& entry) {
                                     return entry.id == id;
                                   }),
                    gridLabels_.end());
  compasses_.erase(std::remove_if(compasses_.begin(), compasses_.end(),
                                  [id](const CompassEntry& entry) {
                                    return entry.id == id;
                                  }),
                   compasses_.end());
}

void ViewportOverlayManager::clear()
{
  rulers_.clear();
  scaleBars_.clear();
  gridLabels_.clear();
  compasses_.clear();
}

void ViewportOverlayManager::invalidateCache()
{
  for (auto& entry : rulers_) {
    entry.cacheValid = false;
    entry.cachedTicks.clear();
  }
}

void ViewportOverlayManager::setVisible(int id, bool visible)
{
  for (auto& entry : rulers_) {
    if (entry.id == id) {
      entry.visible = visible;
      return;
    }
  }
  for (auto& entry : scaleBars_) {
    if (entry.id == id) {
      entry.visible = visible;
      return;
    }
  }
  for (auto& entry : gridLabels_) {
    if (entry.id == id) {
      entry.visible = visible;
      return;
    }
  }
  for (auto& entry : compasses_) {
    if (entry.id == id) {
      entry.visible = visible;
      return;
    }
  }
}

bool ViewportOverlayManager::isVisible(int id) const
{
  for (const auto& entry : rulers_) {
    if (entry.id == id) return entry.visible;
  }
  for (const auto& entry : scaleBars_) {
    if (entry.id == id) return entry.visible;
  }
  for (const auto& entry : gridLabels_) {
    if (entry.id == id) return entry.visible;
  }
  for (const auto& entry : compasses_) {
    if (entry.id == id) return entry.visible;
  }
  return false;
}

std::vector<ViewportRulerTick> ViewportOverlayManager::generateRulerTicks(
    int id, float zoom, const QPointF& viewportOrigin,
    const QSizeF& viewportSize, const QSizeF& canvasSize) const
{
  for (const auto& entry : rulers_) {
    if (entry.id != id || !entry.visible) return {};
    const bool horizontal = entry.config.orientation ==
                            ViewportRulerOrientation::Horizontal;
    const bool originIndependent = entry.config.anchor !=
                                   ViewportRulerAnchor::Start;
    const float normalizedZoom = safeZoom(zoom);
    if (entry.cacheValid && nearlyEqual(entry.cachedZoom, normalizedZoom) &&
        (originIndependent || nearlyEqual(entry.cachedOrigin, viewportOrigin)) &&
        nearlyEqual(entry.cachedViewportSize, viewportSize) &&
        nearlyEqual(entry.cachedCanvasSize, canvasSize)) {
      return entry.cachedTicks;
    }
    QPointF effectiveOrigin = viewportOrigin;
    const float canvasExtent = horizontal
        ? static_cast<float>(canvasSize.width())
        : static_cast<float>(canvasSize.height());
    const float viewportExtent = horizontal
        ? static_cast<float>(viewportSize.width())
        : static_cast<float>(viewportSize.height());
    if (entry.config.anchor == ViewportRulerAnchor::Center) {
      const float centered = (canvasExtent - viewportExtent) * 0.5f;
      if (horizontal) {
        effectiveOrigin.setX(centered);
      } else {
        effectiveOrigin.setY(centered);
      }
    } else if (entry.config.anchor == ViewportRulerAnchor::End) {
      const float endOrigin = canvasExtent - viewportExtent;
      if (horizontal) {
        effectiveOrigin.setX(endOrigin);
      } else {
        effectiveOrigin.setY(endOrigin);
      }
    }
    auto ticks = ViewportRulerData::generateTicks(
        zoom, effectiveOrigin, viewportSize, canvasSize, horizontal,
        entry.config.targetPixels, entry.config.unitName);
    if (!entry.config.showLabels) {
      for (auto& tick : ticks) tick.label.clear();
    }
    if (!entry.config.showTicks) ticks.clear();
    entry.cachedZoom = normalizedZoom;
    entry.cachedOrigin = effectiveOrigin;
    entry.cachedViewportSize = viewportSize;
    entry.cachedCanvasSize = canvasSize;
    entry.cachedTicks = ticks;
    entry.cacheValid = true;
    return ticks;
  }
  return {};
}

ViewportScaleBarData ViewportOverlayManager::generateScaleBarData(
    int id, float zoom, const QSizeF& viewportSize) const
{
  for (const auto& entry : scaleBars_) {
    if (entry.id != id || !entry.visible) return {};
    auto data = ViewportScaleBarDataFactory::generate(
        zoom, viewportSize, entry.config.targetPixels, entry.config.unitName);
    const float widthValue = static_cast<float>(viewportSize.width());
    const float heightValue = static_cast<float>(viewportSize.height());
    const float width = std::isfinite(widthValue) ? std::max(0.0f, widthValue) : 0.0f;
    const float height = std::isfinite(heightValue) ? std::max(0.0f, heightValue) : 0.0f;
    const float marginX = std::isfinite(entry.config.marginX)
        ? std::max(0.0f, entry.config.marginX) : 0.0f;
    const float marginY = std::isfinite(entry.config.marginY)
        ? std::max(0.0f, entry.config.marginY) : 0.0f;
    const bool right = entry.config.anchor ==
                       ViewportScaleBarConfig::Anchor::BottomRight ||
                       entry.config.anchor ==
                       ViewportScaleBarConfig::Anchor::TopRight;
    const bool top = entry.config.anchor ==
                     ViewportScaleBarConfig::Anchor::TopLeft ||
                     entry.config.anchor ==
                     ViewportScaleBarConfig::Anchor::TopRight;
    data.barViewportX = right
        ? std::max(marginX, width - marginX - data.barWidthPx)
        : marginX;
    data.barViewportY = top
        ? marginY + 8.0f
        : std::max(marginY + 8.0f, height - marginY - 8.0f);
    return data;
  }
  return {};
}

ViewportGridLabelData ViewportOverlayManager::generateGridLabelData(
    int id, float zoom) const
{
  for (const auto& entry : gridLabels_) {
    if (entry.id != id || !entry.visible) return {};
    const auto step = ViewportTickCalculator::compute(
        zoom, entry.config.targetPixels, entry.config.unitName);
    const float interval = std::isfinite(step.interval) && step.interval > 0.0f
        ? step.interval : 1.0f;
    const QPointF position(
        std::isfinite(entry.config.viewportPosition.x())
            ? entry.config.viewportPosition.x() : 16.0,
        std::isfinite(entry.config.viewportPosition.y())
            ? entry.config.viewportPosition.y() : 16.0);
    return ViewportGridLabelData{interval, position,
                                 step.label.isEmpty() ? QStringLiteral("1 px")
                                                      : step.label};
  }
  return {};
}

ViewportCompassData ViewportOverlayManager::generateCompassData(
    int id, float yawDegrees) const
{
  for (const auto& entry : compasses_) {
    if (entry.id != id || !entry.visible) return {};
    const float safeYaw = std::isfinite(yawDegrees) ? yawDegrees : 0.0f;
    const float radians = safeYaw * 0.017453292519943295f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const QPointF center(
        std::isfinite(entry.config.viewportPosition.x())
            ? entry.config.viewportPosition.x() : 48.0,
        std::isfinite(entry.config.viewportPosition.y())
            ? entry.config.viewportPosition.y() : 48.0);
    const float size = std::isfinite(entry.config.size)
        ? std::max(1.0f, entry.config.size) : 32.0f;
    const QPointF xAxisEnd(center.x() + cosine * size,
                            center.y() + sine * size);
    const QPointF yAxisEnd(center.x() - sine * size,
                            center.y() - cosine * size);
    return ViewportCompassData{center, xAxisEnd, yAxisEnd};
  }
  return {};
}

ViewportOverlayFrameData ViewportOverlayManager::generateAll(
    float zoom, const QPointF& viewportOrigin, const QSizeF& viewportSize,
    const QSizeF& canvasSize, float yawDegrees) const
{
  ViewportOverlayFrameData result;
  result.rulers.reserve(rulers_.size());
  result.scaleBars.reserve(scaleBars_.size());
  result.gridLabels.reserve(gridLabels_.size());
  result.compasses.reserve(compasses_.size());

  for (const auto& entry : rulers_) {
    if (!entry.visible) continue;
    result.rulers.push_back(ViewportOverlayFrameData::Ruler{
        entry.id, generateRulerTicks(entry.id, zoom, viewportOrigin,
                                      viewportSize, canvasSize)});
  }
  for (const auto& entry : scaleBars_) {
    if (!entry.visible) continue;
    result.scaleBars.push_back(ViewportOverlayFrameData::ScaleBar{
        entry.id, generateScaleBarData(entry.id, zoom, viewportSize)});
  }
  for (const auto& entry : gridLabels_) {
    if (!entry.visible) continue;
    result.gridLabels.push_back(ViewportOverlayFrameData::GridLabel{
        entry.id, generateGridLabelData(entry.id, zoom)});
  }
  for (const auto& entry : compasses_) {
    if (!entry.visible) continue;
    result.compasses.push_back(ViewportOverlayFrameData::Compass{
        entry.id, generateCompassData(entry.id, yawDegrees)});
  }
  return result;
}

}

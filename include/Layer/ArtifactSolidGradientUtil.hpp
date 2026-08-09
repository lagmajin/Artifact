#pragma once
/// @file Shared gradient rendering utilities for solid layers (Solid2D / SolidImage).
/// Included in the global module fragment of layer implementation files.

#include <QColor>
#include <QConicalGradient>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPointF>
#include <QRadialGradient>
#include <QSize>
#include <algorithm>
#include <cmath>

namespace ArtifactSolidGradientUtil {

inline QColor toQColor(const QColor& color, const float alphaScale = 1.0f) {
    const auto safeChannel = [](const float value, const float fallback) {
        return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
    };
    const float safeAlphaScale = std::isfinite(alphaScale)
        ? std::clamp(alphaScale, 0.0f, 1.0f) : 1.0f;
    return QColor::fromRgbF(safeChannel(color.redF(), 0.0f),
                            safeChannel(color.greenF(), 0.0f),
                            safeChannel(color.blueF(), 0.0f),
                            safeChannel(color.alphaF() * safeAlphaScale, 1.0f));
}

inline QPointF gradientPointForAngle(
    const float angleDegrees, const QSize& size, const bool startPoint,
    const bool reverse, const float centerX, const float centerY,
    const float scale, const float offset)
{
    const float safeAngle = std::isfinite(angleDegrees) ? angleDegrees : 90.0f;
    const float safeCenterX = std::isfinite(centerX) ? std::clamp(centerX, 0.0f, 1.0f) : 0.5f;
    const float safeCenterY = std::isfinite(centerY) ? std::clamp(centerY, 0.0f, 1.0f) : 0.5f;
    const float safeScale = std::isfinite(scale) ? std::clamp(scale, 0.01f, 1000000.0f) : 1.0f;
    const float safeOffset = std::isfinite(offset) ? std::clamp(offset, -1000000.0f, 1000000.0f) : 0.0f;
    const float radians = safeAngle * 3.14159265358979323846f / 180.0f;
    const QPointF center(
        static_cast<qreal>(size.width()) * safeCenterX,
        static_cast<qreal>(size.height()) * safeCenterY);
    const qreal dx = std::cos(radians);
    const qreal dy = -std::sin(radians);
    const qreal halfSpan =
        std::max(1.0, std::hypot(static_cast<double>(size.width()),
                                 static_cast<double>(size.height()))) * 0.5 *
        safeScale;
    const qreal direction = reverse ? -1.0 : 1.0;
    const qreal sign = startPoint ? -1.0 : 1.0;
    return QPointF(
        center.x() + dx * halfSpan * direction * sign + dx * halfSpan * safeOffset,
        center.y() + dy * halfSpan * direction * sign + dy * halfSpan * safeOffset);
}

inline QPointF gradientCenterPoint(const QSize& size, const float centerX, const float centerY) {
    const float safeCenterX = std::isfinite(centerX) ? std::clamp(centerX, 0.0f, 1.0f) : 0.5f;
    const float safeCenterY = std::isfinite(centerY) ? std::clamp(centerY, 0.0f, 1.0f) : 0.5f;
    return QPointF(
        static_cast<qreal>(size.width()) * safeCenterX,
        static_cast<qreal>(size.height()) * safeCenterY);
}

inline QImage makeSolidGradientImage(
    const QSize& size,
    const QColor& startColor,
    const QColor& endColor,
    int fillType,
    const float angleDegrees,
    const bool reverse,
    const float centerX,
    const float centerY,
    const float scale,
    const float offset)
{
    const QSize safeSize(std::clamp(size.width(), 1, 16384),
                         std::clamp(size.height(), 1, 16384));
    const int safeFillType = std::clamp(fillType, 0, 5);
    const float safeAngle = std::isfinite(angleDegrees) ? angleDegrees : 90.0f;
    const float safeScale = std::isfinite(scale) ? std::clamp(scale, 0.01f, 1000000.0f) : 1.0f;
    const float safeOffset = std::isfinite(offset) ? std::clamp(offset, -1000000.0f, 1000000.0f) : 0.0f;
    QImage image(safeSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    const QPointF center = gradientCenterPoint(safeSize, centerX, centerY);

    if (safeFillType == 2) { // Radial
        const qreal radius = std::max<qreal>(1.0,
            std::hypot(static_cast<double>(safeSize.width()),
                       static_cast<double>(safeSize.height())) * 0.5 * safeScale);
        QRadialGradient gradient(center, radius);
        gradient.setColorAt(0.0, reverse ? toQColor(endColor) : toQColor(startColor));
        gradient.setColorAt(1.0, reverse ? toQColor(startColor) : toQColor(endColor));
        painter.fillRect(image.rect(), gradient);
        return image;
    }

    if (safeFillType == 3) { // Conical
        QConicalGradient gradient(center, safeAngle);
        gradient.setColorAt(0.0, reverse ? toQColor(endColor) : toQColor(startColor));
        gradient.setColorAt(1.0, reverse ? toQColor(startColor) : toQColor(endColor));
        painter.fillRect(image.rect(), gradient);
        return image;
    }

    // Default: Linear
    const QPointF p1 = gradientPointForAngle(safeAngle, safeSize, true, reverse,
                                              centerX, centerY, safeScale, safeOffset);
    const QPointF p2 = gradientPointForAngle(safeAngle, safeSize, false, reverse,
                                              centerX, centerY, safeScale, safeOffset);
    QLinearGradient gradient(p1, p2);
    if (safeFillType == 4) { // Repeating
        gradient.setSpread(QGradient::RepeatSpread);
    } else if (safeFillType == 5) { // Mirrored
        gradient.setSpread(QGradient::ReflectSpread);
    }
    gradient.setColorAt(0.0, reverse ? toQColor(endColor) : toQColor(startColor));
    gradient.setColorAt(1.0, reverse ? toQColor(startColor) : toQColor(endColor));
    painter.fillRect(image.rect(), gradient);
    return image;
}

} // namespace ArtifactSolidGradientUtil

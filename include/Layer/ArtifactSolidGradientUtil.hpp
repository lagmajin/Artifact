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
    return QColor::fromRgbF(std::clamp(color.redF(), 0.0f, 1.0f),
                            std::clamp(color.greenF(), 0.0f, 1.0f),
                            std::clamp(color.blueF(), 0.0f, 1.0f),
                            std::clamp(color.alphaF() * alphaScale, 0.0f, 1.0f));
}

inline QPointF gradientPointForAngle(
    const float angleDegrees, const QSize& size, const bool startPoint,
    const bool reverse, const float centerX, const float centerY,
    const float scale, const float offset)
{
    const float radians = angleDegrees * 3.14159265358979323846f / 180.0f;
    const QPointF center(
        static_cast<qreal>(size.width()) * std::clamp(centerX, 0.0f, 1.0f),
        static_cast<qreal>(size.height()) * std::clamp(centerY, 0.0f, 1.0f));
    const qreal dx = std::cos(radians);
    const qreal dy = -std::sin(radians);
    const qreal halfSpan =
        std::max(1.0, std::hypot(static_cast<double>(size.width()),
                                 static_cast<double>(size.height()))) * 0.5 *
        std::max(0.01f, scale);
    const qreal direction = reverse ? -1.0 : 1.0;
    const qreal sign = startPoint ? -1.0 : 1.0;
    return QPointF(
        center.x() + dx * halfSpan * direction * sign + dx * halfSpan * offset,
        center.y() + dy * halfSpan * direction * sign + dy * halfSpan * offset);
}

inline QPointF gradientCenterPoint(const QSize& size, const float centerX, const float centerY) {
    return QPointF(
        static_cast<qreal>(size.width()) * std::clamp(centerX, 0.0f, 1.0f),
        static_cast<qreal>(size.height()) * std::clamp(centerY, 0.0f, 1.0f));
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
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    const QPointF center = gradientCenterPoint(size, centerX, centerY);

    if (fillType == 2) { // Radial
        const qreal radius = std::max<qreal>(1.0,
            std::hypot(static_cast<double>(size.width()),
                       static_cast<double>(size.height())) * 0.5 *
            std::max(0.01f, scale));
        QRadialGradient gradient(center, radius);
        gradient.setColorAt(0.0, reverse ? toQColor(endColor) : toQColor(startColor));
        gradient.setColorAt(1.0, reverse ? toQColor(startColor) : toQColor(endColor));
        painter.fillRect(image.rect(), gradient);
        return image;
    }

    if (fillType == 3) { // Conical
        QConicalGradient gradient(center, angleDegrees);
        gradient.setColorAt(0.0, reverse ? toQColor(endColor) : toQColor(startColor));
        gradient.setColorAt(1.0, reverse ? toQColor(startColor) : toQColor(endColor));
        painter.fillRect(image.rect(), gradient);
        return image;
    }

    // Default: Linear
    const QPointF p1 = gradientPointForAngle(angleDegrees, size, true, reverse,
                                              centerX, centerY, scale, offset);
    const QPointF p2 = gradientPointForAngle(angleDegrees, size, false, reverse,
                                              centerX, centerY, scale, offset);
    QLinearGradient gradient(p1, p2);
    gradient.setColorAt(0.0, reverse ? toQColor(endColor) : toQColor(startColor));
    gradient.setColorAt(1.0, reverse ? toQColor(startColor) : toQColor(endColor));
    painter.fillRect(image.rect(), gradient);
    return image;
}

} // namespace ArtifactSolidGradientUtil

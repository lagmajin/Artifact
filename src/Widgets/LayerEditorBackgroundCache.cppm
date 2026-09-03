module;

#include <QColor>
#include <QLinearGradient>
#include <QPainter>

#include <algorithm>

module Artifact.Widgets.LayerEditor.BackgroundCache;

import Color.Float;

namespace Artifact {

QImage makeLayerEditorMayaGradientSprite(
    const QSize& size,
    const ArtifactCore::FloatColor& backgroundColor)
{
 const int width = std::max(1, size.width());
 const int height = std::max(1, size.height());
 QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
 image.fill(Qt::transparent);

 QPainter painter(&image);
 QLinearGradient gradient(0.0, 0.0, 0.0, static_cast<qreal>(height));
 gradient.setColorAt(0.00, QColor::fromRgbF(0.26f, 0.32f, 0.38f, 1.0f));
 gradient.setColorAt(0.07, QColor::fromRgbF(0.24f, 0.30f, 0.36f, 1.0f));
 gradient.setColorAt(0.14, QColor::fromRgbF(0.21f, 0.27f, 0.33f, 1.0f));
 gradient.setColorAt(0.22, QColor::fromRgbF(0.19f, 0.25f, 0.30f, 1.0f));
 gradient.setColorAt(0.30, QColor::fromRgbF(0.17f, 0.22f, 0.27f, 1.0f));
 gradient.setColorAt(0.41, QColor::fromRgbF(0.15f, 0.20f, 0.25f, 1.0f));
 gradient.setColorAt(0.52, QColor::fromRgbF(0.13f, 0.17f, 0.22f, 1.0f));
 gradient.setColorAt(0.64, QColor::fromRgbF(0.12f, 0.15f, 0.20f, 1.0f));
 gradient.setColorAt(0.76, QColor::fromRgbF(0.10f, 0.13f, 0.17f, 1.0f));
 gradient.setColorAt(0.88, QColor::fromRgbF(0.09f, 0.12f, 0.15f, 1.0f));
 gradient.setColorAt(1.00, QColor::fromRgbF(0.08f, 0.10f, 0.13f, 1.0f));
 painter.fillRect(image.rect(), gradient);

 QLinearGradient glow(0.0, 0.0,
                      static_cast<qreal>(width), static_cast<qreal>(height));
 QColor tint = QColor::fromRgbF(
     backgroundColor.r(), backgroundColor.g(), backgroundColor.b(), 1.0f);
 tint.setAlpha(72);
 glow.setColorAt(0.0, tint.lighter(112));
 QColor tintDark = tint.darker(140);
 tintDark.setAlpha(28);
 glow.setColorAt(1.0, tintDark);
 painter.fillRect(image.rect(), glow);
 return image;
}

}

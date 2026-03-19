module;
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>
#include <QColor>
#include <QString>
#include <QRect>
#include <QSize>
#include <QVariant>
#include <QTextOption>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Layer.Text;




import Artifact.Layer.Abstract;
import Utils.String.UniString;
import FloatRGBA;
import Image.ImageF32x4_RGBA;
import Size;
import Property.Abstract;
import Property.Group;
import Font.FreeFont;
import Text.Style;

namespace Artifact
{
using namespace ArtifactCore;

class ArtifactTextLayer::Impl
{
public:
    UniString text_;
    TextStyle textStyle_;
    ParagraphStyle paragraphStyle_;
    QImage renderedImage_;

    Impl();
};

ArtifactTextLayer::Impl::Impl()
    : text_(QString("New Text Layer"))
{
    textStyle_.fontFamily = UniString(QStringLiteral("Arial"));
    textStyle_.fontSize = 60.0f;
    textStyle_.fillColor = FloatRGBA(1.0f, 1.0f, 1.0f, 1.0f);
    paragraphStyle_.horizontalAlignment = TextHorizontalAlignment::Left;
}

ArtifactTextLayer::ArtifactTextLayer() 
    : ArtifactAbstractLayer()
    , impl_(new Impl())
{
    updateImage();
}

ArtifactTextLayer::~ArtifactTextLayer()
{
    delete impl_;
}

void ArtifactTextLayer::setText(const UniString& text)
{
    impl_->text_ = text;
    updateImage();
}

UniString ArtifactTextLayer::text() const
{
    return impl_->text_;
}

void ArtifactTextLayer::setFontSize(float size)
{
    if (size <= 0.0f) size = 1.0f;
    impl_->textStyle_.fontSize = size;
    updateImage();
}

float ArtifactTextLayer::fontSize() const
{
    return impl_->textStyle_.fontSize;
}

void ArtifactTextLayer::setFontFamily(const UniString& family)
{
    impl_->textStyle_.fontFamily = family;
    updateImage();
}

UniString ArtifactTextLayer::fontFamily() const
{
    return impl_->textStyle_.fontFamily;
}

void ArtifactTextLayer::setTextColor(const FloatRGBA& color)
{
    impl_->textStyle_.fillColor = color;
    updateImage();
}

FloatRGBA ArtifactTextLayer::textColor() const
{
    return impl_->textStyle_.fillColor;
}

void ArtifactTextLayer::setStrokeEnabled(bool enabled)
{
    impl_->textStyle_.strokeEnabled = enabled;
    updateImage();
}

bool ArtifactTextLayer::isStrokeEnabled() const
{
    return impl_->textStyle_.strokeEnabled;
}

void ArtifactTextLayer::setStrokeColor(const FloatRGBA& color)
{
    impl_->textStyle_.strokeColor = color;
    updateImage();
}

FloatRGBA ArtifactTextLayer::strokeColor() const
{
    return impl_->textStyle_.strokeColor;
}

void ArtifactTextLayer::setStrokeWidth(float width)
{
    impl_->textStyle_.strokeWidth = width;
    updateImage();
}

float ArtifactTextLayer::strokeWidth() const
{
    return impl_->textStyle_.strokeWidth;
}

void ArtifactTextLayer::setShadowEnabled(bool enabled)
{
    impl_->textStyle_.shadowEnabled = enabled;
    updateImage();
}

bool ArtifactTextLayer::isShadowEnabled() const
{
    return impl_->textStyle_.shadowEnabled;
}

void ArtifactTextLayer::setShadowColor(const FloatRGBA& color)
{
    impl_->textStyle_.shadowColor = color;
    updateImage();
}

FloatRGBA ArtifactTextLayer::shadowColor() const
{
    return impl_->textStyle_.shadowColor;
}

void ArtifactTextLayer::setShadowOffset(float x, float y)
{
    impl_->textStyle_.shadowOffsetX = x;
    impl_->textStyle_.shadowOffsetY = y;
    updateImage();
}

float ArtifactTextLayer::shadowOffsetX() const
{
    return impl_->textStyle_.shadowOffsetX;
}

float ArtifactTextLayer::shadowOffsetY() const
{
    return impl_->textStyle_.shadowOffsetY;
}

void ArtifactTextLayer::setShadowBlur(float blur)
{
    impl_->textStyle_.shadowBlur = blur;
    updateImage();
}

float ArtifactTextLayer::shadowBlur() const
{
    return impl_->textStyle_.shadowBlur;
}

void ArtifactTextLayer::setTracking(float tracking)
{
    impl_->textStyle_.tracking = tracking;
    updateImage();
}

float ArtifactTextLayer::tracking() const
{
    return impl_->textStyle_.tracking;
}

void ArtifactTextLayer::setLeading(float leading)
{
    impl_->textStyle_.leading = leading;
    updateImage();
}

float ArtifactTextLayer::leading() const
{
    return impl_->textStyle_.leading;
}

void ArtifactTextLayer::setBold(bool enabled)
{
    impl_->textStyle_.bold = enabled;
    updateImage();
}

bool ArtifactTextLayer::isBold() const
{
    return impl_->textStyle_.bold;
}

void ArtifactTextLayer::setItalic(bool enabled)
{
    impl_->textStyle_.italic = enabled;
    updateImage();
}

bool ArtifactTextLayer::isItalic() const
{
    return impl_->textStyle_.italic;
}

void ArtifactTextLayer::setAllCaps(bool enabled)
{
    impl_->textStyle_.allCaps = enabled;
    updateImage();
}

bool ArtifactTextLayer::isAllCaps() const
{
    return impl_->textStyle_.allCaps;
}

void ArtifactTextLayer::setHorizontalAlignment(TextHorizontalAlignment alignment)
{
    impl_->paragraphStyle_.horizontalAlignment = alignment;
    updateImage();
}

TextHorizontalAlignment ArtifactTextLayer::horizontalAlignment() const
{
    return impl_->paragraphStyle_.horizontalAlignment;
}

QImage ArtifactTextLayer::toQImage() const
{
    return impl_->renderedImage_;
}

void ArtifactTextLayer::draw(ArtifactIRenderer* renderer)
{
    if (!renderer) {
        return;
    }
    if (impl_->renderedImage_.isNull()) {
        updateImage();
    }
    const auto size = sourceSize();
    if (impl_->renderedImage_.isNull() || size.width <= 0 || size.height <= 0) {
        return;
    }
    renderer->drawSprite(0.0f, 0.0f, static_cast<float>(size.width), static_cast<float>(size.height), impl_->renderedImage_, this->opacity());
}

std::vector<ArtifactCore::PropertyGroup> ArtifactTextLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    ArtifactCore::PropertyGroup textGroup(QStringLiteral("Text"));

    auto makeProp = [](const QString& name, ArtifactCore::PropertyType type, const QVariant& value, int priority = 0) {
        auto p = std::make_shared<ArtifactCore::AbstractProperty>();
        p->setName(name);
        p->setType(type);
        p->setValue(value);
        p->setDisplayPriority(priority);
        return p;
    };

    textGroup.addProperty(makeProp(QStringLiteral("text.value"), ArtifactCore::PropertyType::String, text().toQString(), -120));
    textGroup.addProperty(makeProp(QStringLiteral("text.fontFamily"), ArtifactCore::PropertyType::String, fontFamily().toQString(), -110));
    textGroup.addProperty(makeProp(QStringLiteral("text.fontSize"), ArtifactCore::PropertyType::Float, fontSize(), -100));
    textGroup.addProperty(makeProp(QStringLiteral("text.tracking"), ArtifactCore::PropertyType::Float, tracking(), -95));
    textGroup.addProperty(makeProp(QStringLiteral("text.leading"), ArtifactCore::PropertyType::Float, leading(), -94));
    textGroup.addProperty(makeProp(QStringLiteral("text.bold"), ArtifactCore::PropertyType::Boolean, isBold(), -93));
    textGroup.addProperty(makeProp(QStringLiteral("text.italic"), ArtifactCore::PropertyType::Boolean, isItalic(), -92));
    textGroup.addProperty(makeProp(QStringLiteral("text.allCaps"), ArtifactCore::PropertyType::Boolean, isAllCaps(), -91));

    auto alignmentProp = makeProp(QStringLiteral("text.alignment"), ArtifactCore::PropertyType::Integer,
        static_cast<int>(horizontalAlignment()), -89);
    alignmentProp->setTooltip(QStringLiteral("0=Left, 1=Center, 2=Right, 3=Justify"));
    textGroup.addProperty(alignmentProp);

    const auto c = textColor();
    auto colorProp = std::make_shared<ArtifactCore::AbstractProperty>();
    colorProp->setName(QStringLiteral("text.color"));
    colorProp->setType(ArtifactCore::PropertyType::Color);
    colorProp->setColorValue(QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()));
    colorProp->setValue(colorProp->getColorValue());
    colorProp->setDisplayPriority(-90);
    textGroup.addProperty(colorProp);

    // Stroke
    textGroup.addProperty(makeProp(QStringLiteral("text.strokeEnabled"), ArtifactCore::PropertyType::Boolean, isStrokeEnabled(), -85));
    const auto sc = strokeColor();
    auto strokeColorProp = std::make_shared<ArtifactCore::AbstractProperty>();
    strokeColorProp->setName(QStringLiteral("text.strokeColor"));
    strokeColorProp->setType(ArtifactCore::PropertyType::Color);
    strokeColorProp->setColorValue(QColor::fromRgbF(sc.r(), sc.g(), sc.b(), sc.a()));
    strokeColorProp->setValue(strokeColorProp->getColorValue());
    strokeColorProp->setDisplayPriority(-84);
    textGroup.addProperty(strokeColorProp);
    textGroup.addProperty(makeProp(QStringLiteral("text.strokeWidth"), ArtifactCore::PropertyType::Float, strokeWidth(), -83));

    // Shadow
    textGroup.addProperty(makeProp(QStringLiteral("text.shadowEnabled"), ArtifactCore::PropertyType::Boolean, isShadowEnabled(), -80));
    const auto shc = shadowColor();
    auto shadowColorProp = std::make_shared<ArtifactCore::AbstractProperty>();
    shadowColorProp->setName(QStringLiteral("text.shadowColor"));
    shadowColorProp->setType(ArtifactCore::PropertyType::Color);
    shadowColorProp->setColorValue(QColor::fromRgbF(shc.r(), shc.g(), shc.b(), shc.a()));
    shadowColorProp->setValue(shadowColorProp->getColorValue());
    shadowColorProp->setDisplayPriority(-79);
    textGroup.addProperty(shadowColorProp);
    textGroup.addProperty(makeProp(QStringLiteral("text.shadowOffsetX"), ArtifactCore::PropertyType::Float, shadowOffsetX(), -78));
    textGroup.addProperty(makeProp(QStringLiteral("text.shadowOffsetY"), ArtifactCore::PropertyType::Float, shadowOffsetY(), -77));
    textGroup.addProperty(makeProp(QStringLiteral("text.shadowBlur"), ArtifactCore::PropertyType::Float, shadowBlur(), -76));

    groups.push_back(textGroup);
    return groups;
}

bool ArtifactTextLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == QStringLiteral("text.value")) {
        setText(UniString(value.toString()));
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.fontFamily")) {
        setFontFamily(UniString(value.toString()));
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.fontSize")) {
        setFontSize(static_cast<float>(value.toDouble()));
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.tracking")) {
        setTracking(static_cast<float>(value.toDouble()));
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.leading")) {
        setLeading(static_cast<float>(value.toDouble()));
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.bold")) {
        setBold(value.toBool());
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.italic")) {
        setItalic(value.toBool());
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.allCaps")) {
        setAllCaps(value.toBool());
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.alignment")) {
        setHorizontalAlignment(static_cast<TextHorizontalAlignment>(value.toInt()));
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.color")) {
        const auto c = value.value<QColor>();
        setTextColor(FloatRGBA(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.strokeEnabled")) {
        setStrokeEnabled(value.toBool());
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.strokeColor")) {
        const auto c = value.value<QColor>();
        setStrokeColor(FloatRGBA(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.strokeWidth")) {
        setStrokeWidth(static_cast<float>(value.toDouble()));
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.shadowEnabled")) {
        setShadowEnabled(value.toBool());
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.shadowColor")) {
        const auto c = value.value<QColor>();
        setShadowColor(FloatRGBA(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.shadowOffsetX")) {
        setShadowOffset(static_cast<float>(value.toDouble()), shadowOffsetY());
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.shadowOffsetY")) {
        setShadowOffset(shadowOffsetX(), static_cast<float>(value.toDouble()));
        Q_EMIT changed();
        return true;
    }
    if (propertyPath == QStringLiteral("text.shadowBlur")) {
        setShadowBlur(static_cast<float>(value.toDouble()));
        Q_EMIT changed();
        return true;
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

void ArtifactTextLayer::updateImage() {
    QString displayText = impl_->text_.toQString();
    if (impl_->textStyle_.allCaps) {
        displayText = displayText.toUpper();
    }
    if (displayText.isEmpty()) {
        displayText = QStringLiteral(" ");
    }

    QFont font = FontManager::makeFont(impl_->textStyle_);
    QFontMetricsF metrics(font);
    const QStringList lines = displayText.split('\n');
    
    // Calculate content size
    qreal maxWidth = 0.0;
    for (const QString& line : lines) {
        maxWidth = std::max(maxWidth, metrics.horizontalAdvance(line.isEmpty() ? QStringLiteral(" ") : line));
    }

    const qreal lineAdvance = impl_->textStyle_.leading > 0.0f ? impl_->textStyle_.leading : metrics.lineSpacing();
    const qreal paragraphSpacing = impl_->paragraphStyle_.paragraphSpacing;
    const qreal lineCount = static_cast<qreal>(lines.size());
    const qreal contentHeight = std::max<qreal>(
        lineAdvance,
        lineCount * lineAdvance + std::max<qreal>(0.0, (lineCount - 1.0) * paragraphSpacing));

    // Padding for stroke and shadow blur
    const qreal margin = 24.0 + (impl_->textStyle_.strokeEnabled ? impl_->textStyle_.strokeWidth : 0.0) 
                              + (impl_->textStyle_.shadowEnabled ? impl_->textStyle_.shadowBlur * 2.0 : 0.0);
    
    const int width = std::max(1, static_cast<int>(std::ceil(maxWidth + margin * 2.0)));
    const int height = std::max(1, static_cast<int>(std::ceil(contentHeight + margin * 2.0)));

    impl_->renderedImage_ = QImage(width, height, QImage::Format_ARGB32_Premultiplied);
    impl_->renderedImage_.fill(Qt::transparent);

    QPainter painter(&impl_->renderedImage_);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // Helper to draw the text path
    auto drawTextPath = [&](QPainter& p, const QColor& fill, bool drawStroke, const QColor& stroke, qreal sw) {
        qreal currentY = margin + metrics.ascent();
        for (const QString& line : lines) {
            const QString actualLine = line.isEmpty() ? QStringLiteral(" ") : line;
            const qreal lineWidth = metrics.horizontalAdvance(actualLine);
            qreal currentX = margin;
            
            switch (impl_->paragraphStyle_.horizontalAlignment) {
            case TextHorizontalAlignment::Center:
                currentX = (static_cast<qreal>(width) - lineWidth) * 0.5;
                break;
            case TextHorizontalAlignment::Right:
                currentX = static_cast<qreal>(width) - lineWidth - margin;
                break;
            default:
                break;
            }

            QPainterPath path;
            path.addText(QPointF(currentX, currentY), font, actualLine);

            if (drawStroke && sw > 0.0) {
                QPen pen(stroke, sw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                p.strokePath(path, pen);
            }
            
            p.fillPath(path, fill);
            currentY += lineAdvance + paragraphSpacing;
        }
    };

    // 1. Draw Shadow
    if (impl_->textStyle_.shadowEnabled) {
        QImage shadowImg(width, height, QImage::Format_ARGB32_Premultiplied);
        shadowImg.fill(Qt::transparent);
        QPainter shadowPainter(&shadowImg);
        shadowPainter.setRenderHint(QPainter::Antialiasing);
        
        QColor shColor = QColor::fromRgbF(
            impl_->textStyle_.shadowColor.r(),
            impl_->textStyle_.shadowColor.g(),
            impl_->textStyle_.shadowColor.b(),
            impl_->textStyle_.shadowColor.a());
            
        drawTextPath(shadowPainter, shColor, false, Qt::black, 0.0);
        shadowPainter.end();

        // High quality blur using OpenCV
        if (impl_->textStyle_.shadowBlur > 0.1f) {
            cv::Mat mat(shadowImg.height(), shadowImg.width(), CV_8UC4, const_cast<uchar*>(shadowImg.bits()), shadowImg.bytesPerLine());
            // OpenCV uses BGRA by default for CV_8UC4 from QImage::Format_ARGB32
            int ksize = static_cast<int>(impl_->textStyle_.shadowBlur * 3.0f);
            if (ksize % 2 == 0) ksize++;
            if (ksize >= 3) {
                cv::GaussianBlur(mat, mat, cv::Size(ksize, ksize), impl_->textStyle_.shadowBlur);
            }
        }

        painter.save();
        painter.translate(impl_->textStyle_.shadowOffsetX, impl_->textStyle_.shadowOffsetY);
        painter.drawImage(0, 0, shadowImg);
        painter.restore();
    }

    // 2. Draw Main Text (Fill + Stroke)
    QColor fillCol = QColor::fromRgbF(
        impl_->textStyle_.fillColor.r(),
        impl_->textStyle_.fillColor.g(),
        impl_->textStyle_.fillColor.b(),
        impl_->textStyle_.fillColor.a());
        
    QColor strokeCol = QColor::fromRgbF(
        impl_->textStyle_.strokeColor.r(),
        impl_->textStyle_.strokeColor.g(),
        impl_->textStyle_.strokeColor.b(),
        impl_->textStyle_.strokeColor.a());

    drawTextPath(painter, fillCol, impl_->textStyle_.strokeEnabled, strokeCol, impl_->textStyle_.strokeWidth);

    painter.end();
    setSourceSize(Size_2D(width, height));
}

} // namespace Artifact

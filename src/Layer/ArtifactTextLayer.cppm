module;
#include <QMatrix4x4>
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
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextFrame>
#include <QTextFragment>
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
#include <Layer/ArtifactCloneEffectSupport.hpp>
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
    bool isDirty_ = true;
    
    // Cache key to detect if we actually need to re-render
    struct CacheKey {
        UniString text;
        TextStyle style;
        ParagraphStyle paragraph;
        bool operator==(const CacheKey& o) const {
            return text == o.text && style == o.style && paragraph == o.paragraph;
        }
    };
    std::optional<CacheKey> lastCacheKey_;

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
    // Defer expensive text rasterization until the layer is actually drawn.
    impl_->renderedImage_ = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
    impl_->renderedImage_.fill(Qt::transparent);
    impl_->isDirty_ = true;
    setSourceSize(Size_2D(1, 1));
}

ArtifactTextLayer::~ArtifactTextLayer()
{
    delete impl_;
}

void ArtifactTextLayer::markDirty()
{
    if (impl_) {
        impl_->isDirty_ = true;
    }
}

void ArtifactTextLayer::setText(const UniString& text)
{
    impl_->text_ = text;
    markDirty();
}

UniString ArtifactTextLayer::text() const
{
    return impl_->text_;
}

void ArtifactTextLayer::setFontSize(float size)
{
    if (size <= 0.0f) size = 1.0f;
    impl_->textStyle_.fontSize = size;
    markDirty();
}

float ArtifactTextLayer::fontSize() const
{
    return impl_->textStyle_.fontSize;
}

void ArtifactTextLayer::setFontFamily(const UniString& family)
{
    impl_->textStyle_.fontFamily = family;
    markDirty();
}

UniString ArtifactTextLayer::fontFamily() const
{
    return impl_->textStyle_.fontFamily;
}

void ArtifactTextLayer::setTextColor(const FloatRGBA& color)
{
    impl_->textStyle_.fillColor = color;
    markDirty();
}

FloatRGBA ArtifactTextLayer::textColor() const
{
    return impl_->textStyle_.fillColor;
}

void ArtifactTextLayer::setStrokeEnabled(bool enabled)
{
    impl_->textStyle_.strokeEnabled = enabled;
    markDirty();
}

bool ArtifactTextLayer::isStrokeEnabled() const
{
    return impl_->textStyle_.strokeEnabled;
}

void ArtifactTextLayer::setStrokeColor(const FloatRGBA& color)
{
    impl_->textStyle_.strokeColor = color;
    markDirty();
}

FloatRGBA ArtifactTextLayer::strokeColor() const
{
    return impl_->textStyle_.strokeColor;
}

void ArtifactTextLayer::setStrokeWidth(float width)
{
    impl_->textStyle_.strokeWidth = width;
    markDirty();
}

float ArtifactTextLayer::strokeWidth() const
{
    return impl_->textStyle_.strokeWidth;
}

void ArtifactTextLayer::setShadowEnabled(bool enabled)
{
    impl_->textStyle_.shadowEnabled = enabled;
    markDirty();
}

bool ArtifactTextLayer::isShadowEnabled() const
{
    return impl_->textStyle_.shadowEnabled;
}

void ArtifactTextLayer::setShadowColor(const FloatRGBA& color)
{
    impl_->textStyle_.shadowColor = color;
    markDirty();
}

FloatRGBA ArtifactTextLayer::shadowColor() const
{
    return impl_->textStyle_.shadowColor;
}

void ArtifactTextLayer::setShadowOffset(float x, float y)
{
    impl_->textStyle_.shadowOffsetX = x;
    impl_->textStyle_.shadowOffsetY = y;
    markDirty();
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
    markDirty();
}

float ArtifactTextLayer::shadowBlur() const
{
    return impl_->textStyle_.shadowBlur;
}

void ArtifactTextLayer::setTracking(float tracking)
{
    impl_->textStyle_.tracking = tracking;
    markDirty();
}

float ArtifactTextLayer::tracking() const
{
    return impl_->textStyle_.tracking;
}

void ArtifactTextLayer::setLeading(float leading)
{
    impl_->textStyle_.leading = leading;
    markDirty();
}

float ArtifactTextLayer::leading() const
{
    return impl_->textStyle_.leading;
}

void ArtifactTextLayer::setBold(bool enabled)
{
    impl_->textStyle_.bold = enabled;
    markDirty();
}

bool ArtifactTextLayer::isBold() const
{
    return impl_->textStyle_.bold;
}

void ArtifactTextLayer::setItalic(bool enabled)
{
    impl_->textStyle_.italic = enabled;
    markDirty();
}

bool ArtifactTextLayer::isItalic() const
{
    return impl_->textStyle_.italic;
}

void ArtifactTextLayer::setAllCaps(bool enabled)
{
    impl_->textStyle_.allCaps = enabled;
    markDirty();
}

bool ArtifactTextLayer::isAllCaps() const
{
    return impl_->textStyle_.allCaps;
}

void ArtifactTextLayer::setHorizontalAlignment(TextHorizontalAlignment alignment)
{
    impl_->paragraphStyle_.horizontalAlignment = alignment;
    markDirty();
}

TextHorizontalAlignment ArtifactTextLayer::horizontalAlignment() const
{
    return impl_->paragraphStyle_.horizontalAlignment;
}

QImage ArtifactTextLayer::toQImage() const
{
    if (impl_->isDirty_ || impl_->renderedImage_.isNull()) {
        const_cast<ArtifactTextLayer*>(this)->updateImage();
    }
    return impl_->renderedImage_;
}

void ArtifactTextLayer::draw(ArtifactIRenderer* renderer)
{
    if (!renderer) {
        return;
    }
    if (impl_->isDirty_ || impl_->renderedImage_.isNull()) {
        updateImage();
    }
    const auto size = sourceSize();
    if (impl_->renderedImage_.isNull() || size.width <= 0 || size.height <= 0) {
        return;
    }
    
    // Use drawSpriteTransformed to support rotation/scaling in Diligent
    const QMatrix4x4 baseTransform = getGlobalTransform4x4();
    drawWithClonerEffect(this, baseTransform, [renderer, size, this](const QMatrix4x4& transform, float weight) {
        renderer->drawSpriteTransformed(0.0f, 0.0f,
                                        static_cast<float>(size.width),
                                        static_cast<float>(size.height),
                                        transform, impl_->renderedImage_,
                                        this->opacity() * weight);
    });
}

std::vector<ArtifactCore::PropertyGroup> ArtifactTextLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    ArtifactCore::PropertyGroup textGroup(QStringLiteral("Text"));

    auto makeProp = [this](const QString& name, ArtifactCore::PropertyType type, const QVariant& value, int priority = 0) {
        return persistentLayerProperty(name, type, value, priority);
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
    auto colorProp = persistentLayerProperty(QStringLiteral("text.color"),
        ArtifactCore::PropertyType::Color,
        QVariant(),
        -90);
    colorProp->setColorValue(QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()));
    colorProp->setValue(colorProp->getColorValue());
    textGroup.addProperty(colorProp);

    // Stroke
    textGroup.addProperty(makeProp(QStringLiteral("text.strokeEnabled"), ArtifactCore::PropertyType::Boolean, isStrokeEnabled(), -85));
    const auto sc = strokeColor();
    auto strokeColorProp = persistentLayerProperty(QStringLiteral("text.strokeColor"),
        ArtifactCore::PropertyType::Color,
        QVariant(),
        -84);
    strokeColorProp->setColorValue(QColor::fromRgbF(sc.r(), sc.g(), sc.b(), sc.a()));
    strokeColorProp->setValue(strokeColorProp->getColorValue());
    textGroup.addProperty(strokeColorProp);
    textGroup.addProperty(makeProp(QStringLiteral("text.strokeWidth"), ArtifactCore::PropertyType::Float, strokeWidth(), -83));

    // Shadow
    textGroup.addProperty(makeProp(QStringLiteral("text.shadowEnabled"), ArtifactCore::PropertyType::Boolean, isShadowEnabled(), -80));
    const auto shc = shadowColor();
    auto shadowColorProp = persistentLayerProperty(QStringLiteral("text.shadowColor"),
        ArtifactCore::PropertyType::Color,
        QVariant(),
        -79);
    shadowColorProp->setColorValue(QColor::fromRgbF(shc.r(), shc.g(), shc.b(), shc.a()));
    shadowColorProp->setValue(shadowColorProp->getColorValue());
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
    // Cache check
    Impl::CacheKey currentKey{impl_->text_, impl_->textStyle_, impl_->paragraphStyle_};
    if (!impl_->isDirty_ && impl_->lastCacheKey_ && *impl_->lastCacheKey_ == currentKey && !impl_->renderedImage_.isNull()) {
        return;
    }
    impl_->lastCacheKey_ = currentKey;

    QString displayText = impl_->text_.toQString();
    if (impl_->textStyle_.allCaps) {
        displayText = displayText.toUpper();
    }
    if (displayText.isEmpty()) {
        displayText = QStringLiteral(" ");
    }

    // Set up QTextDocument for rich text support
    QTextDocument doc;
    doc.setUndoRedoEnabled(false);
    
    // Set default font
    QFont defaultFont = FontManager::makeFont(impl_->textStyle_);
    doc.setDefaultFont(defaultFont);

    // Apply basic styles to the whole document
    QTextOption option = doc.defaultTextOption();
    switch (impl_->paragraphStyle_.horizontalAlignment) {
        case TextHorizontalAlignment::Center: option.setAlignment(Qt::AlignCenter); break;
        case TextHorizontalAlignment::Right:  option.setAlignment(Qt::AlignRight); break;
        case TextHorizontalAlignment::Justify: option.setAlignment(Qt::AlignJustify); break;
        default: option.setAlignment(Qt::AlignLeft); break;
    }
    doc.setDefaultTextOption(option);

    // Set content - handles HTML or Plain Text
    if (Qt::mightBeRichText(displayText)) {
        doc.setHtml(displayText);
    } else {
        doc.setPlainText(displayText);
    }

    // Calculate size
    doc.adjustSize();
    qreal docWidth = doc.size().width();
    qreal docHeight = doc.size().height();

    // Padding for stroke and shadow blur
    const qreal margin = 24.0 + (impl_->textStyle_.strokeEnabled ? impl_->textStyle_.strokeWidth : 0.0) 
                              + (impl_->textStyle_.shadowEnabled ? impl_->textStyle_.shadowBlur * 2.0 : 0.0);
    
    const int width = std::max(1, static_cast<int>(std::ceil(docWidth + margin * 2.0)));
    const int height = std::max(1, static_cast<int>(std::ceil(docHeight + margin * 2.0)));

    impl_->renderedImage_ = QImage(width, height, QImage::Format_ARGB32_Premultiplied);
    impl_->renderedImage_.fill(Qt::transparent);

    QPainter painter(&impl_->renderedImage_);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Offset for margins
    painter.translate(margin, margin);

    // Helper to draw document layout as paths (for stroke support)
    auto drawDocAsPath = [&](QPainter& p, bool drawFill, bool drawStroke) {
        QAbstractTextDocumentLayout* layout = doc.documentLayout();
        QTextBlock block = doc.begin();
        while (block.isValid()) {
            QRectF blockRect = layout->blockBoundingRect(block);
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                QTextFragment fragment = it.fragment();
                if (fragment.isValid()) {
                    QFont f = fragment.charFormat().font();
                    QTextLayout* blockLayout = block.layout();
                    int relativePos = fragment.position() - block.position();
                    
                    // We iterate through lines to handle wrapping and alignment
                    for (int i = 0; i < blockLayout->lineCount(); ++i) {
                        QTextLine line = blockLayout->lineAt(i);
                        if (relativePos >= line.textStart() && relativePos < (line.textStart() + line.textLength())) {
                            QPointF linePos = line.position();
                            qreal x = line.cursorToX(relativePos);
                            
                            QPainterPath path;
                            path.addText(blockRect.topLeft() + linePos + QPointF(x, line.ascent()), f, fragment.text());
                            
                            if (drawStroke && impl_->textStyle_.strokeEnabled) {
                                QColor strokeCol = QColor::fromRgbF(
                                    impl_->textStyle_.strokeColor.r(),
                                    impl_->textStyle_.strokeColor.g(),
                                    impl_->textStyle_.strokeColor.b(),
                                    impl_->textStyle_.strokeColor.a());
                                QPen pen(strokeCol, impl_->textStyle_.strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                                p.strokePath(path, pen);
                            }
                            
                            if (drawFill) {
                                QColor fillCol;
                                if (fragment.charFormat().foreground().style() != Qt::NoBrush) {
                                    fillCol = fragment.charFormat().foreground().color();
                                } else {
                                    fillCol = QColor::fromRgbF(
                                        impl_->textStyle_.fillColor.r(),
                                        impl_->textStyle_.fillColor.g(),
                                        impl_->textStyle_.fillColor.b(),
                                        impl_->textStyle_.fillColor.a());
                                }
                                p.fillPath(path, fillCol);
                            }
                        }
                    }
                }
            }
            block = block.next();
        }
    };

    // 1. Draw Shadow
    if (impl_->textStyle_.shadowEnabled) {
        QImage shadowImg(width, height, QImage::Format_ARGB32_Premultiplied);
        shadowImg.fill(Qt::transparent);
        QPainter shadowPainter(&shadowImg);
        shadowPainter.setRenderHint(QPainter::Antialiasing);
        shadowPainter.translate(margin, margin);
        
        // Shadow is simplified as a single color fill path
        drawDocAsPath(shadowPainter, true, false); 
        shadowPainter.end();

        if (impl_->textStyle_.shadowBlur > 0.1f) {
            cv::Mat mat(shadowImg.height(), shadowImg.width(), CV_8UC4, const_cast<uchar*>(shadowImg.bits()), shadowImg.bytesPerLine());
            int ksize = static_cast<int>(impl_->textStyle_.shadowBlur * 3.0f);
            if (ksize % 2 == 0) ksize++;
            if (ksize >= 3) {
                cv::GaussianBlur(mat, mat, cv::Size(ksize, ksize), impl_->textStyle_.shadowBlur);
            }
        }

        painter.save();
        painter.translate(-margin + impl_->textStyle_.shadowOffsetX, -margin + impl_->textStyle_.shadowOffsetY);
        painter.drawImage(0, 0, shadowImg);
        painter.restore();
    }

    // 2. Draw Main Content (Stroke then Fill)
    drawDocAsPath(painter, true, true);

    painter.end();
    setSourceSize(Size_2D(width, height));
    impl_->isDirty_ = false;
}

} // namespace Artifact

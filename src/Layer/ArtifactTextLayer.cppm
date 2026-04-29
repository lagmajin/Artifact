module;
#include <QAbstractTextDocumentLayout>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QMatrix4x4>
#include <QPainter>
#include <QPainterPath>
#include <QRect>
#include <QSize>
#include <QString>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextLayout>
#include <QTextOption>
#include <QVariant>
#include <Layer/ArtifactCloneEffectSupport.hpp>
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <opencv2/opencv.hpp>

module Artifact.Layer.Text;

import Artifact.Layers.Abstract._2D;
import Utils.String.UniString;
import Color.Float;
import FloatRGBA;
import Image.ImageF32x4_RGBA;
import CvUtils;
import Size;
import Property.Abstract;
import Property.Group;
import Font.FreeFont;
import Text.Style;
import Text.GlyphLayout;
import Text.Animator;

namespace Artifact {
using namespace ArtifactCore;

struct TextAnimatorState {
  QString name;
  bool enabled = true;
  RangeSelector range;
  WigglySelector wiggly;
  AnimatorProperties properties;
};

TextLayoutMode defaultTextLayoutMode() {
  return TextLayoutMode::Point;
}

class ArtifactTextLayer::Impl {
public:
  UniString text_;
  TextStyle textStyle_;
  ParagraphStyle paragraphStyle_;
  TextLayoutMode layoutMode_ = defaultTextLayoutMode();
  QImage renderedImage_;
  mutable std::shared_ptr<ArtifactCore::ImageF32x4_RGBA> renderedBuffer_;
  bool isDirty_ = true;

  // Text Animator support
  std::vector<GlyphItem> glyphs_;
  std::vector<TextAnimatorState> animators_;
  bool perGlyphMode_ = false;
  // Cache key to detect if we actually need to re-render
  struct CacheKey {
    UniString text;
    TextStyle style;
    ParagraphStyle paragraph;
    bool operator==(const CacheKey &o) const {
      return text == o.text && style == o.style && paragraph == o.paragraph;
    }
  };
  std::optional<CacheKey> lastCacheKey_;

  Impl();
};

ArtifactTextLayer::Impl::Impl() : text_(QString("New Text Layer")) {
  textStyle_.fontFamily = UniString(QStringLiteral("Arial"));
  textStyle_.fontSize = 60.0f;
  textStyle_.fillColor = FloatRGBA(1.0f, 1.0f, 1.0f, 1.0f);
  paragraphStyle_.horizontalAlignment = TextHorizontalAlignment::Left;
  paragraphStyle_.verticalAlignment = TextVerticalAlignment::Top;
  paragraphStyle_.wrapMode = TextWrapMode::WordWrap;
}

namespace {

QRectF unitedGlyphBounds(const std::vector<GlyphItem> &glyphs);
QFont makeTextFont(const TextStyle &style, const QString &sampleText);

TextAnimatorState defaultTextAnimatorState(const int index) {
  TextAnimatorState state;
  state.name = QStringLiteral("Animator %1").arg(index + 1);
  return state;
}

QColor toQColor(const FloatRGBA &color) {
  return QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
}

FloatRGBA toFloatRGBA(const QColor &color) {
  return FloatRGBA(color.redF(), color.greenF(), color.blueF(), color.alphaF());
}

QColor colorWithOpacity(const QColor &color, const float opacity) {
  QColor out = color;
  out.setAlphaF(std::clamp(color.alphaF() * opacity, 0.0f, 1.0f));
  return out;
}

QString selectorUnitsTooltip() {
  return QStringLiteral("0=Percentage, 1=Index");
}

QString selectorShapeTooltip() {
  return QStringLiteral(
      "0=Square, 1=Ramp Up, 2=Ramp Down, 3=Triangle, 4=Round, 5=Smooth");
}

QJsonObject colorToJson(const FloatRGBA &color) {
  QJsonObject obj;
  obj["r"] = color.r();
  obj["g"] = color.g();
  obj["b"] = color.b();
  obj["a"] = color.a();
  return obj;
}

FloatRGBA colorFromJsonValue(const QJsonValue &value,
                             const FloatRGBA &fallback) {
  if (value.isObject()) {
    const QJsonObject obj = value.toObject();
    return FloatRGBA(static_cast<float>(obj.value("r").toDouble(fallback.r())),
                     static_cast<float>(obj.value("g").toDouble(fallback.g())),
                     static_cast<float>(obj.value("b").toDouble(fallback.b())),
                     static_cast<float>(obj.value("a").toDouble(fallback.a())));
  }
  if (value.isString()) {
    const QColor color(value.toString());
    if (color.isValid()) {
      return toFloatRGBA(color);
    }
  }
  return fallback;
}

QJsonObject textAnimatorToJson(const TextAnimatorState &animator) {
  QJsonObject obj;
  obj["name"] = animator.name;
  obj["enabled"] = animator.enabled;

  QJsonObject rangeObj;
  rangeObj["start"] = animator.range.start;
  rangeObj["end"] = animator.range.end;
  rangeObj["offset"] = animator.range.offset;
  rangeObj["units"] = static_cast<int>(animator.range.units);
  rangeObj["shape"] = static_cast<int>(animator.range.shape);
  rangeObj["easeHigh"] = animator.range.easeHigh;
  rangeObj["easeLow"] = animator.range.easeLow;
  obj["range"] = rangeObj;

  QJsonObject wigglyObj;
  wigglyObj["enabled"] = animator.wiggly.enabled;
  wigglyObj["wigglesPerSecond"] = animator.wiggly.wigglesPerSecond;
  wigglyObj["correlation"] = animator.wiggly.correlation;
  wigglyObj["phase"] = animator.wiggly.phase;
  wigglyObj["seed"] = animator.wiggly.seed;
  obj["wiggly"] = wigglyObj;

  QJsonObject propsObj;
  propsObj["positionX"] = animator.properties.position.x();
  propsObj["positionY"] = animator.properties.position.y();
  propsObj["scale"] = animator.properties.scale;
  propsObj["rotation"] = animator.properties.rotation;
  propsObj["opacity"] = animator.properties.opacity;
  propsObj["skew"] = animator.properties.skew;
  propsObj["tracking"] = animator.properties.tracking;
  propsObj["z"] = animator.properties.z;
  propsObj["colorEnabled"] = animator.properties.colorEnabled;
  propsObj["fillColor"] = colorToJson(animator.properties.fillColor);
  propsObj["strokeEnabled"] = animator.properties.strokeEnabled;
  propsObj["strokeColor"] = colorToJson(animator.properties.strokeColor);
  propsObj["strokeWidth"] = animator.properties.strokeWidth;
  propsObj["blur"] = animator.properties.blur;
  obj["properties"] = propsObj;
  return obj;
}

TextAnimatorState textAnimatorFromJson(const QJsonObject &obj, const int index) {
  TextAnimatorState animator = defaultTextAnimatorState(index);
  animator.name = obj.value("name").toString(animator.name);
  animator.enabled = obj.value("enabled").toBool(true);

  if (obj.contains("range") && obj.value("range").isObject()) {
    const QJsonObject rangeObj = obj.value("range").toObject();
    animator.range.start = static_cast<float>(rangeObj.value("start").toDouble(animator.range.start));
    animator.range.end = static_cast<float>(rangeObj.value("end").toDouble(animator.range.end));
    animator.range.offset = static_cast<float>(rangeObj.value("offset").toDouble(animator.range.offset));
    animator.range.units = static_cast<SelectorUnits>(rangeObj.value("units").toInt(static_cast<int>(animator.range.units)));
    animator.range.shape = static_cast<SelectorShape>(rangeObj.value("shape").toInt(static_cast<int>(animator.range.shape)));
    animator.range.easeHigh = static_cast<float>(rangeObj.value("easeHigh").toDouble(animator.range.easeHigh));
    animator.range.easeLow = static_cast<float>(rangeObj.value("easeLow").toDouble(animator.range.easeLow));
  }

  if (obj.contains("wiggly") && obj.value("wiggly").isObject()) {
    const QJsonObject wigglyObj = obj.value("wiggly").toObject();
    animator.wiggly.enabled = wigglyObj.value("enabled").toBool(animator.wiggly.enabled);
    animator.wiggly.wigglesPerSecond = static_cast<float>(wigglyObj.value("wigglesPerSecond").toDouble(animator.wiggly.wigglesPerSecond));
    animator.wiggly.correlation = static_cast<float>(wigglyObj.value("correlation").toDouble(animator.wiggly.correlation));
    animator.wiggly.phase = static_cast<float>(wigglyObj.value("phase").toDouble(animator.wiggly.phase));
    animator.wiggly.seed = wigglyObj.value("seed").toInt(animator.wiggly.seed);
  }

  if (obj.contains("properties") && obj.value("properties").isObject()) {
    const QJsonObject propsObj = obj.value("properties").toObject();
    animator.properties.position.setX(static_cast<float>(propsObj.value("positionX").toDouble(animator.properties.position.x())));
    animator.properties.position.setY(static_cast<float>(propsObj.value("positionY").toDouble(animator.properties.position.y())));
    animator.properties.scale = static_cast<float>(propsObj.value("scale").toDouble(animator.properties.scale));
    animator.properties.rotation = static_cast<float>(propsObj.value("rotation").toDouble(animator.properties.rotation));
    animator.properties.opacity = static_cast<float>(propsObj.value("opacity").toDouble(animator.properties.opacity));
    animator.properties.skew = static_cast<float>(propsObj.value("skew").toDouble(animator.properties.skew));
    animator.properties.tracking = static_cast<float>(propsObj.value("tracking").toDouble(animator.properties.tracking));
    animator.properties.z = static_cast<float>(propsObj.value("z").toDouble(animator.properties.z));
    animator.properties.colorEnabled = propsObj.value("colorEnabled").toBool(animator.properties.colorEnabled);
    animator.properties.fillColor = colorFromJsonValue(propsObj.value("fillColor"), animator.properties.fillColor);
    animator.properties.strokeEnabled = propsObj.value("strokeEnabled").toBool(animator.properties.strokeEnabled);
    animator.properties.strokeColor = colorFromJsonValue(propsObj.value("strokeColor"), animator.properties.strokeColor);
    animator.properties.strokeWidth = static_cast<float>(propsObj.value("strokeWidth").toDouble(animator.properties.strokeWidth));
    animator.properties.blur = static_cast<float>(propsObj.value("blur").toDouble(animator.properties.blur));
  }

  return animator;
}

std::optional<std::pair<int, QString>>
parseAnimatorPropertyPath(const QString &propertyPath) {
  static const QString prefix = QStringLiteral("text.animators.");
  if (!propertyPath.startsWith(prefix)) {
    return std::nullopt;
  }
  const QString remainder = propertyPath.mid(prefix.size());
  const int dot = remainder.indexOf('.');
  if (dot <= 0 || dot + 1 >= remainder.size()) {
    return std::nullopt;
  }
  bool ok = false;
  const int index = remainder.left(dot).toInt(&ok);
  if (!ok || index < 0) {
    return std::nullopt;
  }
  return std::make_pair(index, remainder.mid(dot + 1));
}

QPainterPath glyphPathForCode(const char32_t code, const TextStyle &style) {
  const QString glyphText = QString::fromUcs4(&code, 1);
  if (glyphText.isEmpty() || glyphText.at(0).isSpace()) {
    return {};
  }
  const QFont font = makeTextFont(style, glyphText);
  QPainterPath path;
  path.addText(QPointF(0.0, 0.0), font, glyphText);
  return path;
}

QTransform glyphTransform(const GlyphItem &glyph, const QPainterPath &path,
                          const QPointF &origin = QPointF()) {
  QTransform transform;
  const QRectF localBounds = path.boundingRect();
  const QPointF center = localBounds.center();
  transform.translate(origin.x() + glyph.basePosition.x() + glyph.offsetPosition.x(),
                      origin.y() + glyph.basePosition.y() + glyph.offsetPosition.y());
  transform.translate(center.x(), center.y());
  if (std::abs(glyph.offsetSkew) > 0.0001f) {
    transform.shear(std::tan(static_cast<qreal>(glyph.offsetSkew) *
                             std::numbers::pi_v<qreal> / 180.0),
                    0.0);
  }
  transform.rotate(glyph.baseRotation + glyph.offsetRotation);
  const qreal scale = std::max<qreal>(0.0001, glyph.baseScale * glyph.offsetScale);
  transform.scale(scale, scale);
  transform.translate(-center.x(), -center.y());
  return transform;
}

QRectF animatedGlyphBounds(const std::vector<GlyphItem> &glyphs,
                           const TextStyle &style) {
  QRectF bounds;
  bool hasBounds = false;
  for (const auto &glyph : glyphs) {
    const QPainterPath path = glyphPathForCode(glyph.charCode, style);
    if (path.isEmpty()) {
      continue;
    }
    const QRectF mapped = glyphTransform(glyph, path).map(path).boundingRect();
    if (!mapped.isValid() || mapped.isEmpty()) {
      continue;
    }
    bounds = hasBounds ? bounds.united(mapped) : mapped;
    hasBounds = true;
  }
  if (!hasBounds) {
    return unitedGlyphBounds(glyphs);
  }
  return bounds;
}

void drawAnimatedGlyphRun(QPainter &painter, const std::vector<GlyphItem> &glyphs,
                          const TextStyle &style, const QColor &fillColor,
                          const QColor &strokeColor, const bool drawFill,
                          const bool drawStroke,
                          const bool useGlyphOverrides = true) {
  for (const auto &glyph : glyphs) {
    const QPainterPath path = glyphPathForCode(glyph.charCode, style);
    if (path.isEmpty()) {
      continue;
    }

    const QTransform transform = glyphTransform(glyph, path);
    const float glyphOpacity = std::clamp(glyph.offsetOpacity, 0.0f, 1.0f);
    const QColor resolvedFill =
        colorWithOpacity(useGlyphOverrides && glyph.hasColorOverride
                             ? toQColor(glyph.fillColorOverride)
                             : fillColor,
                         glyphOpacity);
    const QColor resolvedStroke =
        colorWithOpacity(useGlyphOverrides && glyph.hasStrokeOverride
                             ? toQColor(glyph.strokeColorOverride)
                             : strokeColor,
                         glyphOpacity);
    const qreal strokeWidth =
        std::max<qreal>(0.0, (style.strokeEnabled ? style.strokeWidth : 0.0f) +
                                 glyph.offsetStrokeWidth);

    painter.save();
    painter.setTransform(transform, true);
    if (drawStroke &&
        ((style.strokeEnabled || glyph.hasStrokeOverride) && strokeWidth > 0.01)) {
      painter.strokePath(path, QPen(resolvedStroke, strokeWidth, Qt::SolidLine,
                                    Qt::RoundCap, Qt::RoundJoin));
    }
    if (drawFill) {
      painter.fillPath(path, resolvedFill);
    }
    painter.restore();
  }
}

QRectF unitedGlyphBounds(const std::vector<GlyphItem> &glyphs) {
  QRectF bounds;
  bool hasBounds = false;
  for (const auto &glyph : glyphs) {
    if (!hasBounds) {
      bounds = glyph.bounds;
      hasBounds = true;
    } else {
      bounds = bounds.united(glyph.bounds);
    }
  }
  return hasBounds ? bounds : QRectF(0.0, 0.0, 1.0, 1.0);
}

Qt::Alignment alignmentFromParagraph(const ParagraphStyle &paragraph) {
  int flags = static_cast<int>(Qt::AlignLeft | Qt::AlignTop);
  switch (paragraph.horizontalAlignment) {
  case TextHorizontalAlignment::Center:
    flags &= ~static_cast<int>(Qt::AlignLeft);
    flags |= static_cast<int>(Qt::AlignHCenter);
    break;
  case TextHorizontalAlignment::Right:
    flags &= ~static_cast<int>(Qt::AlignLeft);
    flags |= static_cast<int>(Qt::AlignRight);
    break;
  case TextHorizontalAlignment::Justify:
    flags &= ~static_cast<int>(Qt::AlignLeft);
    flags |= static_cast<int>(Qt::AlignJustify);
    break;
  case TextHorizontalAlignment::Left:
  default:
    flags &= ~(static_cast<int>(Qt::AlignHCenter) |
               static_cast<int>(Qt::AlignRight) |
               static_cast<int>(Qt::AlignJustify));
    flags |= static_cast<int>(Qt::AlignLeft);
    break;
  }

  switch (paragraph.verticalAlignment) {
  case TextVerticalAlignment::Middle:
    flags &= ~static_cast<int>(Qt::AlignTop);
    flags |= static_cast<int>(Qt::AlignVCenter);
    break;
  case TextVerticalAlignment::Bottom:
    flags &= ~static_cast<int>(Qt::AlignTop);
    flags |= static_cast<int>(Qt::AlignBottom);
    break;
  case TextVerticalAlignment::Top:
  default:
    flags &= ~(static_cast<int>(Qt::AlignVCenter) |
               static_cast<int>(Qt::AlignBottom));
    flags |= static_cast<int>(Qt::AlignTop);
    break;
  }
  return static_cast<Qt::Alignment>(flags);
}

qreal textEffectMargin(const TextStyle &style) {
  return 24.0 + (style.strokeEnabled ? style.strokeWidth : 0.0) +
         (style.shadowEnabled ? style.shadowBlur * 2.0 : 0.0);
}

QFont makeTextFont(const TextStyle &style, const QString &sampleText) {
  QFont font = FontManager::makeFont(style, sampleText);
  font.setUnderline(style.underline);
  font.setStrikeOut(style.strikethrough);
  font.setLetterSpacing(QFont::AbsoluteSpacing, style.tracking);
  return font;
}

} // namespace

ArtifactTextLayer::ArtifactTextLayer()
    : ArtifactAbstract2DLayer(), impl_(new Impl()) {
  // Defer expensive text rasterization until the layer is actually drawn.
  impl_->renderedImage_ = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
  impl_->renderedImage_.fill(Qt::transparent);
  impl_->isDirty_ = true;
  setSourceSize(Size_2D(1, 1));
}

ArtifactTextLayer::~ArtifactTextLayer() { delete impl_; }

void ArtifactTextLayer::markDirty() {
  if (impl_) {
    impl_->isDirty_ = true;
  }
}

void ArtifactTextLayer::setText(const UniString &text) {
  impl_->text_ = text;
  markDirty();
}

UniString ArtifactTextLayer::text() const { return impl_->text_; }

void ArtifactTextLayer::setFontSize(float size) {
  if (size <= 0.0f)
    size = 1.0f;
  impl_->textStyle_.fontSize = size;
  markDirty();
}

float ArtifactTextLayer::fontSize() const { return impl_->textStyle_.fontSize; }

void ArtifactTextLayer::setFontFamily(const UniString &family) {
  impl_->textStyle_.fontFamily = family;
  markDirty();
}

UniString ArtifactTextLayer::fontFamily() const {
  return impl_->textStyle_.fontFamily;
}

void ArtifactTextLayer::setTextColor(const FloatColor &color) {
  impl_->textStyle_.fillColor =
      FloatRGBA(color.r(), color.g(), color.b(), color.a());
  markDirty();
}

FloatColor ArtifactTextLayer::textColor() const {
  return FloatColor(impl_->textStyle_.fillColor.r(),
                    impl_->textStyle_.fillColor.g(),
                    impl_->textStyle_.fillColor.b(),
                    impl_->textStyle_.fillColor.a());
}

void ArtifactTextLayer::setStrokeEnabled(bool enabled) {
  impl_->textStyle_.strokeEnabled = enabled;
  markDirty();
}

bool ArtifactTextLayer::isStrokeEnabled() const {
  return impl_->textStyle_.strokeEnabled;
}

void ArtifactTextLayer::setStrokeColor(const FloatColor &color) {
  impl_->textStyle_.strokeColor =
      FloatRGBA(color.r(), color.g(), color.b(), color.a());
  markDirty();
}

FloatColor ArtifactTextLayer::strokeColor() const {
  return FloatColor(impl_->textStyle_.strokeColor.r(),
                    impl_->textStyle_.strokeColor.g(),
                    impl_->textStyle_.strokeColor.b(),
                    impl_->textStyle_.strokeColor.a());
}

void ArtifactTextLayer::setStrokeWidth(float width) {
  impl_->textStyle_.strokeWidth = width;
  markDirty();
}

float ArtifactTextLayer::strokeWidth() const {
  return impl_->textStyle_.strokeWidth;
}

void ArtifactTextLayer::setShadowEnabled(bool enabled) {
  impl_->textStyle_.shadowEnabled = enabled;
  markDirty();
}

bool ArtifactTextLayer::isShadowEnabled() const {
  return impl_->textStyle_.shadowEnabled;
}

void ArtifactTextLayer::setShadowColor(const FloatColor &color) {
  impl_->textStyle_.shadowColor =
      FloatRGBA(color.r(), color.g(), color.b(), color.a());
  markDirty();
}

FloatColor ArtifactTextLayer::shadowColor() const {
  return FloatColor(impl_->textStyle_.shadowColor.r(),
                    impl_->textStyle_.shadowColor.g(),
                    impl_->textStyle_.shadowColor.b(),
                    impl_->textStyle_.shadowColor.a());
}

void ArtifactTextLayer::setShadowOffset(float x, float y) {
  impl_->textStyle_.shadowOffsetX = x;
  impl_->textStyle_.shadowOffsetY = y;
  markDirty();
}

float ArtifactTextLayer::shadowOffsetX() const {
  return impl_->textStyle_.shadowOffsetX;
}

float ArtifactTextLayer::shadowOffsetY() const {
  return impl_->textStyle_.shadowOffsetY;
}

void ArtifactTextLayer::setShadowBlur(float blur) {
  impl_->textStyle_.shadowBlur = blur;
  markDirty();
}

float ArtifactTextLayer::shadowBlur() const {
  return impl_->textStyle_.shadowBlur;
}

void ArtifactTextLayer::setTracking(float tracking) {
  impl_->textStyle_.tracking = tracking;
  markDirty();
}

float ArtifactTextLayer::tracking() const { return impl_->textStyle_.tracking; }

void ArtifactTextLayer::setLeading(float leading) {
  impl_->textStyle_.leading = leading;
  markDirty();
}

float ArtifactTextLayer::leading() const { return impl_->textStyle_.leading; }

void ArtifactTextLayer::setBold(bool enabled) {
  impl_->textStyle_.fontWeight =
      enabled ? FontWeight::Bold : FontWeight::Normal;
  markDirty();
}

bool ArtifactTextLayer::isBold() const {
  return impl_->textStyle_.fontWeight == FontWeight::Bold;
}

void ArtifactTextLayer::setItalic(bool enabled) {
  impl_->textStyle_.fontStyle = enabled ? FontStyle::Italic : FontStyle::Normal;
  markDirty();
}

bool ArtifactTextLayer::isItalic() const {
  return impl_->textStyle_.fontStyle == FontStyle::Italic;
}

void ArtifactTextLayer::setAllCaps(bool enabled) {
  impl_->textStyle_.allCaps = enabled;
  markDirty();
}

bool ArtifactTextLayer::isAllCaps() const { return impl_->textStyle_.allCaps; }

void ArtifactTextLayer::setUnderline(bool enabled) {
  impl_->textStyle_.underline = enabled;
  markDirty();
}

bool ArtifactTextLayer::isUnderline() const {
  return impl_->textStyle_.underline;
}

void ArtifactTextLayer::setStrikethrough(bool enabled) {
  impl_->textStyle_.strikethrough = enabled;
  markDirty();
}

bool ArtifactTextLayer::isStrikethrough() const {
  return impl_->textStyle_.strikethrough;
}

void ArtifactTextLayer::setHorizontalAlignment(
    TextHorizontalAlignment alignment) {
  impl_->paragraphStyle_.horizontalAlignment = alignment;
  markDirty();
}

TextHorizontalAlignment ArtifactTextLayer::horizontalAlignment() const {
  return impl_->paragraphStyle_.horizontalAlignment;
}

void ArtifactTextLayer::setVerticalAlignment(TextVerticalAlignment alignment) {
  impl_->paragraphStyle_.verticalAlignment = alignment;
  markDirty();
}

TextVerticalAlignment ArtifactTextLayer::verticalAlignment() const {
  return impl_->paragraphStyle_.verticalAlignment;
}

void ArtifactTextLayer::setWrapMode(TextWrapMode wrapMode) {
  impl_->paragraphStyle_.wrapMode = wrapMode;
  markDirty();
}

TextWrapMode ArtifactTextLayer::wrapMode() const {
  return impl_->paragraphStyle_.wrapMode;
}

void ArtifactTextLayer::setLayoutMode(TextLayoutMode mode) {
  impl_->layoutMode_ = mode;
  markDirty();
}

TextLayoutMode ArtifactTextLayer::layoutMode() const {
  return impl_->layoutMode_;
}

bool ArtifactTextLayer::isBoxText() const {
  return impl_->layoutMode_ == TextLayoutMode::Box;
}

void ArtifactTextLayer::setMaxWidth(float width) {
  impl_->paragraphStyle_.boxWidth = (width <= 0.0f) ? 0.0f : width;
  if (impl_->paragraphStyle_.boxWidth > 0.0f ||
      impl_->paragraphStyle_.boxHeight > 0.0f) {
    impl_->layoutMode_ = TextLayoutMode::Box;
  } else {
    impl_->layoutMode_ = TextLayoutMode::Point;
  }
  markDirty();
}

float ArtifactTextLayer::maxWidth() const {
  return impl_->paragraphStyle_.boxWidth;
}

void ArtifactTextLayer::setBoxHeight(float height) {
  impl_->paragraphStyle_.boxHeight = (height <= 0.0f) ? 0.0f : height;
  if (impl_->paragraphStyle_.boxWidth > 0.0f ||
      impl_->paragraphStyle_.boxHeight > 0.0f) {
    impl_->layoutMode_ = TextLayoutMode::Box;
  } else {
    impl_->layoutMode_ = TextLayoutMode::Point;
  }
  markDirty();
}

float ArtifactTextLayer::boxHeight() const {
  return impl_->paragraphStyle_.boxHeight;
}

void ArtifactTextLayer::setParagraphSpacing(float spacing) {
  impl_->paragraphStyle_.paragraphSpacing = std::max(0.0f, spacing);
  markDirty();
}

float ArtifactTextLayer::paragraphSpacing() const {
  return impl_->paragraphStyle_.paragraphSpacing;
}

void ArtifactTextLayer::addAnimator() {
  impl_->animators_.push_back(defaultTextAnimatorState(animatorCount()));
  markDirty();
}

void ArtifactTextLayer::removeAnimator(const int index) {
  if (index < 0 || index >= animatorCount()) {
    return;
  }
  impl_->animators_.erase(impl_->animators_.begin() + index);
  markDirty();
}

void ArtifactTextLayer::setAnimatorCount(const int count) {
  const int clampedCount = std::max(0, count);
  const int currentCount = animatorCount();
  if (clampedCount == currentCount) {
    return;
  }
  if (clampedCount > currentCount) {
    while (animatorCount() < clampedCount) {
      addAnimator();
    }
    return;
  }
  impl_->animators_.resize(static_cast<size_t>(clampedCount));
  markDirty();
}

int ArtifactTextLayer::animatorCount() const {
  return static_cast<int>(impl_->animators_.size());
}

QJsonObject ArtifactTextLayer::toJson() const {
  QJsonObject obj = ArtifactAbstract2DLayer::toJson();
  obj["type"] = static_cast<int>(LayerType::Text);
  obj["text.value"] = text().toQString();
  obj["text.fontFamily"] = fontFamily().toQString();
  obj["text.fontSize"] = fontSize();
  obj["text.tracking"] = tracking();
  obj["text.leading"] = leading();
  obj["text.bold"] = isBold();
  obj["text.italic"] = isItalic();
  obj["text.allCaps"] = isAllCaps();
  obj["text.underline"] = isUnderline();
  obj["text.strikethrough"] = isStrikethrough();
  obj["text.alignment"] = static_cast<int>(horizontalAlignment());
  obj["text.verticalAlignment"] = static_cast<int>(verticalAlignment());
  obj["text.wrapMode"] = static_cast<int>(wrapMode());
  obj["text.layoutMode"] = static_cast<int>(layoutMode());
  obj["text.maxWidth"] = maxWidth();
  obj["text.boxHeight"] = boxHeight();
  obj["text.paragraphSpacing"] = paragraphSpacing();
  obj["text.color"] = toQColor(impl_->textStyle_.fillColor).name(QColor::HexArgb);
  obj["text.strokeEnabled"] = isStrokeEnabled();
  obj["text.strokeColor"] =
      toQColor(impl_->textStyle_.strokeColor).name(QColor::HexArgb);
  obj["text.strokeWidth"] = strokeWidth();
  obj["text.shadowEnabled"] = isShadowEnabled();
  obj["text.shadowColor"] =
      toQColor(impl_->textStyle_.shadowColor).name(QColor::HexArgb);
  obj["text.shadowOffsetX"] = shadowOffsetX();
  obj["text.shadowOffsetY"] = shadowOffsetY();
  obj["text.shadowBlur"] = shadowBlur();

  QJsonArray animatorArray;
  for (const auto &animator : impl_->animators_) {
    animatorArray.append(textAnimatorToJson(animator));
  }
  obj["text.animators"] = animatorArray;
  return obj;
}

void ArtifactTextLayer::fromJsonProperties(const QJsonObject &obj) {
  ArtifactAbstract2DLayer::fromJsonProperties(obj);

  if (obj.contains("text.value")) {
    setText(UniString(obj.value("text.value").toString()));
  }
  if (obj.contains("text.fontFamily")) {
    setFontFamily(UniString(obj.value("text.fontFamily").toString()));
  }
  if (obj.contains("text.fontSize")) {
    setFontSize(static_cast<float>(obj.value("text.fontSize").toDouble(fontSize())));
  }
  if (obj.contains("text.tracking")) {
    setTracking(static_cast<float>(obj.value("text.tracking").toDouble(tracking())));
  }
  if (obj.contains("text.leading")) {
    setLeading(static_cast<float>(obj.value("text.leading").toDouble(leading())));
  }
  if (obj.contains("text.bold")) {
    setBold(obj.value("text.bold").toBool(isBold()));
  }
  if (obj.contains("text.italic")) {
    setItalic(obj.value("text.italic").toBool(isItalic()));
  }
  if (obj.contains("text.allCaps")) {
    setAllCaps(obj.value("text.allCaps").toBool(isAllCaps()));
  }
  if (obj.contains("text.underline")) {
    setUnderline(obj.value("text.underline").toBool(isUnderline()));
  }
  if (obj.contains("text.strikethrough")) {
    setStrikethrough(
        obj.value("text.strikethrough").toBool(isStrikethrough()));
  }
  if (obj.contains("text.alignment")) {
    setHorizontalAlignment(static_cast<TextHorizontalAlignment>(
        obj.value("text.alignment")
            .toInt(static_cast<int>(horizontalAlignment()))));
  }
  if (obj.contains("text.verticalAlignment")) {
    setVerticalAlignment(static_cast<TextVerticalAlignment>(
        obj.value("text.verticalAlignment")
            .toInt(static_cast<int>(verticalAlignment()))));
  }
  if (obj.contains("text.wrapMode")) {
    setWrapMode(static_cast<TextWrapMode>(
        obj.value("text.wrapMode").toInt(static_cast<int>(wrapMode()))));
  }
  const bool hasLayoutMode = obj.contains("text.layoutMode");
  if (hasLayoutMode) {
    setLayoutMode(static_cast<TextLayoutMode>(
        obj.value("text.layoutMode").toInt(static_cast<int>(layoutMode()))));
  }
  if (obj.contains("text.maxWidth")) {
    setMaxWidth(static_cast<float>(obj.value("text.maxWidth").toDouble(maxWidth())));
  }
  if (obj.contains("text.boxHeight")) {
    setBoxHeight(
        static_cast<float>(obj.value("text.boxHeight").toDouble(boxHeight())));
  }
  if (obj.contains("text.paragraphSpacing")) {
    setParagraphSpacing(static_cast<float>(
        obj.value("text.paragraphSpacing").toDouble(paragraphSpacing())));
  }
  if (!hasLayoutMode) {
    if (maxWidth() > 0.0f || boxHeight() > 0.0f) {
      setLayoutMode(TextLayoutMode::Box);
    } else {
      setLayoutMode(TextLayoutMode::Point);
    }
  }
  if (obj.contains("text.color")) {
    const auto color =
        colorFromJsonValue(obj.value("text.color"), impl_->textStyle_.fillColor);
    setTextColor(FloatColor(color.r(), color.g(), color.b(), color.a()));
  }
  if (obj.contains("text.strokeEnabled")) {
    setStrokeEnabled(
        obj.value("text.strokeEnabled").toBool(isStrokeEnabled()));
  }
  if (obj.contains("text.strokeColor")) {
    const auto color = colorFromJsonValue(obj.value("text.strokeColor"),
                                          impl_->textStyle_.strokeColor);
    setStrokeColor(FloatColor(color.r(), color.g(), color.b(), color.a()));
  }
  if (obj.contains("text.strokeWidth")) {
    setStrokeWidth(static_cast<float>(
        obj.value("text.strokeWidth").toDouble(strokeWidth())));
  }
  if (obj.contains("text.shadowEnabled")) {
    setShadowEnabled(
        obj.value("text.shadowEnabled").toBool(isShadowEnabled()));
  }
  if (obj.contains("text.shadowColor")) {
    const auto color = colorFromJsonValue(obj.value("text.shadowColor"),
                                          impl_->textStyle_.shadowColor);
    setShadowColor(FloatColor(color.r(), color.g(), color.b(), color.a()));
  }
  if (obj.contains("text.shadowOffsetX") || obj.contains("text.shadowOffsetY")) {
    setShadowOffset(static_cast<float>(
                        obj.value("text.shadowOffsetX").toDouble(shadowOffsetX())),
                    static_cast<float>(
                        obj.value("text.shadowOffsetY").toDouble(shadowOffsetY())));
  }
  if (obj.contains("text.shadowBlur")) {
    setShadowBlur(
        static_cast<float>(obj.value("text.shadowBlur").toDouble(shadowBlur())));
  }

  impl_->animators_.clear();
  if (obj.contains("text.animators") && obj.value("text.animators").isArray()) {
    const QJsonArray animatorArray = obj.value("text.animators").toArray();
    impl_->animators_.reserve(animatorArray.size());
    for (int i = 0; i < animatorArray.size(); ++i) {
      if (!animatorArray.at(i).isObject()) {
        continue;
      }
      impl_->animators_.push_back(
          textAnimatorFromJson(animatorArray.at(i).toObject(), i));
    }
  }

  markDirty();
}

QImage ArtifactTextLayer::toQImage() const {
  if (impl_->isDirty_ || impl_->renderedImage_.isNull()) {
    const_cast<ArtifactTextLayer *>(this)->updateImage();
  }
  return impl_->renderedImage_;
}

const ArtifactCore::ImageF32x4_RGBA &ArtifactTextLayer::currentFrameBuffer() const {
  if (impl_->isDirty_ || impl_->renderedImage_.isNull() || !impl_->renderedBuffer_) {
    const_cast<ArtifactTextLayer *>(this)->updateImage();
  }
  if (impl_->renderedBuffer_) {
    return *impl_->renderedBuffer_;
  }
  static ArtifactCore::ImageF32x4_RGBA empty;
  return empty;
}

bool ArtifactTextLayer::hasCurrentFrameBuffer() const {
  if (impl_->isDirty_ || impl_->renderedImage_.isNull() || !impl_->renderedBuffer_) {
    const_cast<ArtifactTextLayer *>(this)->updateImage();
  }
  return impl_->renderedBuffer_ && !impl_->renderedBuffer_->isEmpty();
}

void ArtifactTextLayer::draw(ArtifactIRenderer *renderer) {
  if (!renderer) {
    return;
  }
  const bool boxLayout = isBoxText();
  const bool hasEnabledAnimators =
      std::any_of(impl_->animators_.begin(), impl_->animators_.end(),
                  [](const TextAnimatorState &animator) {
                    return animator.enabled;
                  });
  QString displayText = impl_->text_.toQString();
  const bool isRichText = Qt::mightBeRichText(displayText);
  const bool containsCjk = FontManager::containsCjkCharacters(displayText);
  if (impl_->textStyle_.allCaps && !isRichText) {
    displayText = displayText.toUpper();
  }
  const bool plainGpuText = !isRichText && !hasEnabledAnimators &&
                            !containsCjk &&
                            impl_->textStyle_.leading <= 0.0f &&
                            impl_->paragraphStyle_.paragraphSpacing <= 0.0f;

  if (plainGpuText) {
    if (displayText.isEmpty()) {
      displayText = QStringLiteral(" ");
    }

    const Impl::CacheKey currentKey{impl_->text_, impl_->textStyle_,
                                    impl_->paragraphStyle_};
    if (impl_->isDirty_ || !impl_->lastCacheKey_ ||
        *impl_->lastCacheKey_ != currentKey || sourceSize().width <= 0 ||
        sourceSize().height <= 0) {
      impl_->glyphs_ = TextLayoutEngine::layout(
          UniString(displayText), impl_->textStyle_, impl_->paragraphStyle_);
      const QRectF glyphBounds = unitedGlyphBounds(impl_->glyphs_);
      const qreal contentWidth = boxLayout && impl_->paragraphStyle_.boxWidth > 0.0f
                                     ? impl_->paragraphStyle_.boxWidth
                                     : std::max<qreal>(1.0, std::ceil(glyphBounds.width()));
      const qreal contentHeight = boxLayout && impl_->paragraphStyle_.boxHeight > 0.0f
                                      ? impl_->paragraphStyle_.boxHeight
                                      : std::max<qreal>(1.0, std::ceil(glyphBounds.height()));
      const qreal margin = textEffectMargin(impl_->textStyle_);
      setSourceSize(Size_2D(
          std::max(1, static_cast<int>(std::ceil(contentWidth + margin * 2.0))),
          std::max(1, static_cast<int>(std::ceil(contentHeight + margin * 2.0)))));
      impl_->renderedImage_ = QImage();
      impl_->renderedBuffer_.reset();
      impl_->lastCacheKey_ = currentKey;
      impl_->isDirty_ = false;
    }

    const auto size = sourceSize();
    if (size.width <= 0 || size.height <= 0) {
      return;
    }

    const qreal margin = textEffectMargin(impl_->textStyle_);
    const qreal contentWidth = boxLayout && impl_->paragraphStyle_.boxWidth > 0.0f
                                   ? impl_->paragraphStyle_.boxWidth
                                   : std::max<qreal>(1.0, size.width - margin * 2.0);
    const qreal contentHeight = boxLayout && impl_->paragraphStyle_.boxHeight > 0.0f
                                    ? impl_->paragraphStyle_.boxHeight
                                    : std::max<qreal>(1.0, size.height - margin * 2.0);
    const QRectF textRect(margin, margin, contentWidth, contentHeight);
    const QFont font = makeTextFont(impl_->textStyle_, displayText);
    const Qt::Alignment alignment = alignmentFromParagraph(impl_->paragraphStyle_);
    const QMatrix4x4 baseTransform = getGlobalTransform4x4();
    const auto fillColor = FloatColor(
        impl_->textStyle_.fillColor.r(), impl_->textStyle_.fillColor.g(),
        impl_->textStyle_.fillColor.b(), impl_->textStyle_.fillColor.a());
    const auto strokeColor = FloatColor(
        impl_->textStyle_.strokeColor.r(), impl_->textStyle_.strokeColor.g(),
        impl_->textStyle_.strokeColor.b(), impl_->textStyle_.strokeColor.a());
    const auto shadowColor = FloatColor(
        impl_->textStyle_.shadowColor.r(), impl_->textStyle_.shadowColor.g(),
        impl_->textStyle_.shadowColor.b(), impl_->textStyle_.shadowColor.a());

    drawWithClonerEffect(
        this, baseTransform,
        [renderer, textRect, displayText, font, alignment, fillColor,
         strokeColor, shadowColor, this](const QMatrix4x4 &transform,
                                         float weight) {
          const float opacity = this->opacity() * weight;
          if (impl_->textStyle_.shadowEnabled) {
            renderer->drawTextTransformed(
                textRect.translated(impl_->textStyle_.shadowOffsetX,
                                    impl_->textStyle_.shadowOffsetY),
                displayText, font, shadowColor, transform, alignment, opacity);
          }
          if (impl_->textStyle_.strokeEnabled) {
            const float radius =
                std::max(1.0f, impl_->textStyle_.strokeWidth * 0.5f);
            static constexpr std::array<QPointF, 8> offsets = {
                QPointF(-1.0, 0.0), QPointF(1.0, 0.0), QPointF(0.0, -1.0),
                QPointF(0.0, 1.0),   QPointF(-1.0, -1.0), QPointF(1.0, -1.0),
                QPointF(-1.0, 1.0),  QPointF(1.0, 1.0)};
            for (const auto &off : offsets) {
              renderer->drawTextTransformed(
                  textRect.translated(off.x() * radius, off.y() * radius),
                  displayText, font, strokeColor, transform, alignment,
                  opacity);
            }
          }
          renderer->drawTextTransformed(textRect, displayText, font, fillColor,
                                        transform, alignment, opacity);
        });
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
  drawWithClonerEffect(
      this, baseTransform,
      [renderer, size, this](const QMatrix4x4 &transform, float weight) {
        renderer->drawSpriteTransformed(
            0.0f, 0.0f, static_cast<float>(size.width),
            static_cast<float>(size.height), transform, impl_->renderedImage_,
            this->opacity() * weight);
      });
}

std::vector<ArtifactCore::PropertyGroup>
ArtifactTextLayer::getLayerPropertyGroups() const {
  auto groups = ArtifactAbstract2DLayer::getLayerPropertyGroups();
  ArtifactCore::PropertyGroup textGroup(QStringLiteral("Text"));

  auto makeProp = [this](const QString &name, ArtifactCore::PropertyType type,
                         const QVariant &value, int priority = 0) {
    return persistentLayerProperty(name, type, value, priority);
  };

  textGroup.addProperty(makeProp(QStringLiteral("text.value"),
                                 ArtifactCore::PropertyType::String,
                                 text().toQString(), -120));
  textGroup.addProperty(makeProp(QStringLiteral("text.fontFamily"),
                                 ArtifactCore::PropertyType::String,
                                 fontFamily().toQString(), -110));
  textGroup.addProperty(makeProp(QStringLiteral("text.fontSize"),
                                 ArtifactCore::PropertyType::Float, fontSize(),
                                 -100));
  textGroup.addProperty(makeProp(QStringLiteral("text.tracking"),
                                 ArtifactCore::PropertyType::Float, tracking(),
                                 -95));
  textGroup.addProperty(makeProp(QStringLiteral("text.leading"),
                                 ArtifactCore::PropertyType::Float, leading(),
                                 -94));
  textGroup.addProperty(makeProp(QStringLiteral("text.bold"),
                                 ArtifactCore::PropertyType::Boolean, isBold(),
                                 -93));
  textGroup.addProperty(makeProp(QStringLiteral("text.italic"),
                                 ArtifactCore::PropertyType::Boolean,
                                 isItalic(), -92));
  textGroup.addProperty(makeProp(QStringLiteral("text.allCaps"),
                                 ArtifactCore::PropertyType::Boolean,
                                 isAllCaps(), -91));
  textGroup.addProperty(makeProp(QStringLiteral("text.underline"),
                                 ArtifactCore::PropertyType::Boolean,
                                 isUnderline(), -90));
  textGroup.addProperty(makeProp(QStringLiteral("text.strikethrough"),
                                 ArtifactCore::PropertyType::Boolean,
                                 isStrikethrough(), -89));

  auto alignmentProp = makeProp(QStringLiteral("text.alignment"),
                                ArtifactCore::PropertyType::Integer,
                                static_cast<int>(horizontalAlignment()), -88);
  alignmentProp->setTooltip(
      QStringLiteral("0=Left, 1=Center, 2=Right, 3=Justify"));
  textGroup.addProperty(alignmentProp);

  auto verticalAlignmentProp =
      makeProp(QStringLiteral("text.verticalAlignment"),
               ArtifactCore::PropertyType::Integer,
               static_cast<int>(verticalAlignment()), -87);
  verticalAlignmentProp->setTooltip(
      QStringLiteral("0=Top, 1=Middle, 2=Bottom"));
  textGroup.addProperty(verticalAlignmentProp);

  auto wrapModeProp = makeProp(QStringLiteral("text.wrapMode"),
                               ArtifactCore::PropertyType::Integer,
                               static_cast<int>(wrapMode()), -86);
  wrapModeProp->setTooltip(
      QStringLiteral("0=NoWrap, 1=WordWrap, 2=WrapAnywhere, 3=ManualWrap"));
  textGroup.addProperty(wrapModeProp);

  auto layoutModeProp = makeProp(QStringLiteral("text.layoutMode"),
                                 ArtifactCore::PropertyType::Integer,
                                 static_cast<int>(layoutMode()), -85);
  layoutModeProp->setTooltip(
      QStringLiteral("0=Point text, 1=Box text"));
  textGroup.addProperty(layoutModeProp);

  auto maxWidthProp =
      makeProp(QStringLiteral("text.maxWidth"),
               ArtifactCore::PropertyType::Float, maxWidth(), -84);
  maxWidthProp->setHardRange(0.0, 100000.0);
  maxWidthProp->setSoftRange(0.0, 1920.0);
  maxWidthProp->setStep(1.0);
  maxWidthProp->setTooltip(
      QStringLiteral("0 = Auto width, > 0 = fixed wrap width"));
  textGroup.addProperty(maxWidthProp);

  auto boxHeightProp =
      makeProp(QStringLiteral("text.boxHeight"),
               ArtifactCore::PropertyType::Float, boxHeight(), -83);
  boxHeightProp->setHardRange(0.0, 100000.0);
  boxHeightProp->setSoftRange(0.0, 1080.0);
  boxHeightProp->setStep(1.0);
  boxHeightProp->setTooltip(
      QStringLiteral("0 = Auto height, > 0 = fixed box height"));
  textGroup.addProperty(boxHeightProp);

  auto paragraphSpacingProp =
      makeProp(QStringLiteral("text.paragraphSpacing"),
               ArtifactCore::PropertyType::Float, paragraphSpacing(), -82);
  paragraphSpacingProp->setHardRange(0.0, 1000.0);
  paragraphSpacingProp->setSoftRange(0.0, 80.0);
  paragraphSpacingProp->setStep(0.5);
  textGroup.addProperty(paragraphSpacingProp);

  const auto c = textColor();
  auto colorProp = persistentLayerProperty(QStringLiteral("text.color"),
                                           ArtifactCore::PropertyType::Color,
                                           QVariant(), -81);
  colorProp->setColorValue(QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()));
  colorProp->setValue(colorProp->getColorValue());
  textGroup.addProperty(colorProp);

  // Stroke
  textGroup.addProperty(makeProp(QStringLiteral("text.strokeEnabled"),
                                 ArtifactCore::PropertyType::Boolean,
                                 isStrokeEnabled(), -80));
  const auto sc = strokeColor();
  auto strokeColorProp = persistentLayerProperty(
      QStringLiteral("text.strokeColor"), ArtifactCore::PropertyType::Color,
      QVariant(), -79);
  strokeColorProp->setColorValue(
      QColor::fromRgbF(sc.r(), sc.g(), sc.b(), sc.a()));
  strokeColorProp->setValue(strokeColorProp->getColorValue());
  textGroup.addProperty(strokeColorProp);
  textGroup.addProperty(makeProp(QStringLiteral("text.strokeWidth"),
                                 ArtifactCore::PropertyType::Float,
                                 strokeWidth(), -78));

  // Shadow
  textGroup.addProperty(makeProp(QStringLiteral("text.shadowEnabled"),
                                 ArtifactCore::PropertyType::Boolean,
                                 isShadowEnabled(), -77));
  const auto shc = shadowColor();
  auto shadowColorProp = persistentLayerProperty(
      QStringLiteral("text.shadowColor"), ArtifactCore::PropertyType::Color,
      QVariant(), -76);
  shadowColorProp->setColorValue(
      QColor::fromRgbF(shc.r(), shc.g(), shc.b(), shc.a()));
  shadowColorProp->setValue(shadowColorProp->getColorValue());
  textGroup.addProperty(shadowColorProp);
  textGroup.addProperty(makeProp(QStringLiteral("text.shadowOffsetX"),
                                 ArtifactCore::PropertyType::Float,
                                 shadowOffsetX(), -75));
  textGroup.addProperty(makeProp(QStringLiteral("text.shadowOffsetY"),
                                 ArtifactCore::PropertyType::Float,
                                 shadowOffsetY(), -74));
  textGroup.addProperty(makeProp(QStringLiteral("text.shadowBlur"),
                                 ArtifactCore::PropertyType::Float,
                                 shadowBlur(), -73));
  auto animatorCountProp =
      makeProp(QStringLiteral("text.animatorCount"),
               ArtifactCore::PropertyType::Integer, animatorCount(), -72);
  animatorCountProp->setHardRange(0, 16);
  animatorCountProp->setSoftRange(0, 8);
  animatorCountProp->setStep(1);
  animatorCountProp->setTooltip(
      QStringLiteral("Increase to add text animators. Decrease to remove from the end."));
  textGroup.addProperty(animatorCountProp);

  groups.push_back(textGroup);

  for (int i = 0; i < animatorCount(); ++i) {
    const auto &animator = impl_->animators_[i];
    const QString prefix = QStringLiteral("text.animators.%1.").arg(i);
    ArtifactCore::PropertyGroup animatorGroup(
        animator.name.trimmed().isEmpty()
            ? QStringLiteral("Animator %1").arg(i + 1)
            : animator.name);

    auto makeAnimatorProp = [this, &prefix](
                                const QString &suffix,
                                ArtifactCore::PropertyType type,
                                const QVariant &value, int priority = 0) {
      return persistentLayerProperty(prefix + suffix, type, value, priority);
    };

    auto nameProp = makeAnimatorProp(QStringLiteral("name"),
                                     ArtifactCore::PropertyType::String,
                                     animator.name, -120);
    nameProp->setDisplayLabel(QStringLiteral("Name"));
    animatorGroup.addProperty(nameProp);

    auto enabledProp = makeAnimatorProp(QStringLiteral("enabled"),
                                        ArtifactCore::PropertyType::Boolean,
                                        animator.enabled, -119);
    enabledProp->setDisplayLabel(QStringLiteral("Enabled"));
    animatorGroup.addProperty(enabledProp);

    auto startProp = makeAnimatorProp(QStringLiteral("start"),
                                      ArtifactCore::PropertyType::Float,
                                      animator.range.start, -118);
    startProp->setDisplayLabel(QStringLiteral("Start"));
    startProp->setSoftRange(0.0, 100.0);
    animatorGroup.addProperty(startProp);

    auto endProp = makeAnimatorProp(QStringLiteral("end"),
                                    ArtifactCore::PropertyType::Float,
                                    animator.range.end, -117);
    endProp->setDisplayLabel(QStringLiteral("End"));
    endProp->setSoftRange(0.0, 100.0);
    animatorGroup.addProperty(endProp);

    auto offsetProp = makeAnimatorProp(QStringLiteral("offset"),
                                       ArtifactCore::PropertyType::Float,
                                       animator.range.offset, -116);
    offsetProp->setDisplayLabel(QStringLiteral("Offset"));
    offsetProp->setSoftRange(-100.0, 100.0);
    animatorGroup.addProperty(offsetProp);

    auto unitsProp = makeAnimatorProp(QStringLiteral("units"),
                                      ArtifactCore::PropertyType::Integer,
                                      static_cast<int>(animator.range.units), -115);
    unitsProp->setDisplayLabel(QStringLiteral("Units"));
    unitsProp->setTooltip(selectorUnitsTooltip());
    animatorGroup.addProperty(unitsProp);

    auto shapeProp = makeAnimatorProp(QStringLiteral("shape"),
                                      ArtifactCore::PropertyType::Integer,
                                      static_cast<int>(animator.range.shape), -114);
    shapeProp->setDisplayLabel(QStringLiteral("Shape"));
    shapeProp->setTooltip(selectorShapeTooltip());
    animatorGroup.addProperty(shapeProp);

    auto wigglyEnabledProp =
        makeAnimatorProp(QStringLiteral("wigglyEnabled"),
                         ArtifactCore::PropertyType::Boolean,
                         animator.wiggly.enabled, -113);
    wigglyEnabledProp->setDisplayLabel(QStringLiteral("Wiggly"));
    animatorGroup.addProperty(wigglyEnabledProp);

    auto wpsProp = makeAnimatorProp(QStringLiteral("wigglesPerSecond"),
                                    ArtifactCore::PropertyType::Float,
                                    animator.wiggly.wigglesPerSecond, -112);
    wpsProp->setDisplayLabel(QStringLiteral("Wiggles/Sec"));
    wpsProp->setSoftRange(0.0, 20.0);
    animatorGroup.addProperty(wpsProp);

    auto correlationProp =
        makeAnimatorProp(QStringLiteral("correlation"),
                         ArtifactCore::PropertyType::Float,
                         animator.wiggly.correlation, -111);
    correlationProp->setDisplayLabel(QStringLiteral("Correlation"));
    correlationProp->setHardRange(0.0, 100.0);
    correlationProp->setSoftRange(0.0, 100.0);
    animatorGroup.addProperty(correlationProp);

    auto phaseProp = makeAnimatorProp(QStringLiteral("phase"),
                                      ArtifactCore::PropertyType::Float,
                                      animator.wiggly.phase, -110);
    phaseProp->setDisplayLabel(QStringLiteral("Phase"));
    animatorGroup.addProperty(phaseProp);

    auto seedProp = makeAnimatorProp(QStringLiteral("seed"),
                                     ArtifactCore::PropertyType::Integer,
                                     animator.wiggly.seed, -109);
    seedProp->setDisplayLabel(QStringLiteral("Seed"));
    animatorGroup.addProperty(seedProp);

    auto posXProp = makeAnimatorProp(QStringLiteral("positionX"),
                                     ArtifactCore::PropertyType::Float,
                                     animator.properties.position.x(), -108);
    posXProp->setDisplayLabel(QStringLiteral("Position X"));
    posXProp->setSoftRange(-500.0, 500.0);
    animatorGroup.addProperty(posXProp);

    auto posYProp = makeAnimatorProp(QStringLiteral("positionY"),
                                     ArtifactCore::PropertyType::Float,
                                     animator.properties.position.y(), -107);
    posYProp->setDisplayLabel(QStringLiteral("Position Y"));
    posYProp->setSoftRange(-500.0, 500.0);
    animatorGroup.addProperty(posYProp);

    auto scaleProp = makeAnimatorProp(QStringLiteral("scale"),
                                      ArtifactCore::PropertyType::Float,
                                      animator.properties.scale, -106);
    scaleProp->setDisplayLabel(QStringLiteral("Scale"));
    scaleProp->setHardRange(0.0, 8.0);
    scaleProp->setSoftRange(0.0, 2.0);
    scaleProp->setStep(0.01);
    animatorGroup.addProperty(scaleProp);

    auto rotationProp = makeAnimatorProp(QStringLiteral("rotation"),
                                         ArtifactCore::PropertyType::Float,
                                         animator.properties.rotation, -105);
    rotationProp->setDisplayLabel(QStringLiteral("Rotation"));
    rotationProp->setUnit(QStringLiteral("deg"));
    rotationProp->setSoftRange(-180.0, 180.0);
    animatorGroup.addProperty(rotationProp);

    auto opacityProp = makeAnimatorProp(QStringLiteral("opacity"),
                                        ArtifactCore::PropertyType::Float,
                                        animator.properties.opacity, -104);
    opacityProp->setDisplayLabel(QStringLiteral("Opacity"));
    opacityProp->setHardRange(0.0, 1.0);
    opacityProp->setSoftRange(0.0, 1.0);
    opacityProp->setStep(0.01);
    animatorGroup.addProperty(opacityProp);

    auto skewProp = makeAnimatorProp(QStringLiteral("skew"),
                                     ArtifactCore::PropertyType::Float,
                                     animator.properties.skew, -103);
    skewProp->setDisplayLabel(QStringLiteral("Skew"));
    skewProp->setUnit(QStringLiteral("deg"));
    skewProp->setSoftRange(-60.0, 60.0);
    animatorGroup.addProperty(skewProp);

    auto trackingProp = makeAnimatorProp(QStringLiteral("tracking"),
                                         ArtifactCore::PropertyType::Float,
                                         animator.properties.tracking, -102);
    trackingProp->setDisplayLabel(QStringLiteral("Tracking"));
    trackingProp->setSoftRange(-100.0, 100.0);
    animatorGroup.addProperty(trackingProp);

    auto colorEnabledProp =
        makeAnimatorProp(QStringLiteral("colorEnabled"),
                         ArtifactCore::PropertyType::Boolean,
                         animator.properties.colorEnabled, -101);
    colorEnabledProp->setDisplayLabel(QStringLiteral("Fill Override"));
    animatorGroup.addProperty(colorEnabledProp);

    auto fillColorProp =
        persistentLayerProperty(prefix + QStringLiteral("fillColor"),
                                ArtifactCore::PropertyType::Color, QVariant(),
                                -100);
    fillColorProp->setDisplayLabel(QStringLiteral("Fill Color"));
    fillColorProp->setColorValue(toQColor(animator.properties.fillColor));
    fillColorProp->setValue(fillColorProp->getColorValue());
    animatorGroup.addProperty(fillColorProp);

    groups.push_back(animatorGroup);
  }
  return groups;
}

bool ArtifactTextLayer::setLayerPropertyValue(const QString &propertyPath,
                                              const QVariant &value) {
  if (propertyPath == QStringLiteral("text.value")) {
    setText(UniString(value.toString()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.fontFamily")) {
    setFontFamily(UniString(value.toString()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.fontSize")) {
    setFontSize(static_cast<float>(value.toDouble()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.tracking")) {
    setTracking(static_cast<float>(value.toDouble()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.leading")) {
    setLeading(static_cast<float>(value.toDouble()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.bold")) {
    setBold(value.toBool());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.italic")) {
    setItalic(value.toBool());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.allCaps")) {
    setAllCaps(value.toBool());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.underline")) {
    setUnderline(value.toBool());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.strikethrough")) {
    setStrikethrough(value.toBool());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.alignment")) {
    setHorizontalAlignment(static_cast<TextHorizontalAlignment>(value.toInt()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.verticalAlignment")) {
    setVerticalAlignment(static_cast<TextVerticalAlignment>(value.toInt()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.wrapMode")) {
    setWrapMode(static_cast<TextWrapMode>(value.toInt()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.layoutMode")) {
    setLayoutMode(static_cast<TextLayoutMode>(value.toInt(static_cast<int>(layoutMode()))));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.maxWidth")) {
    setMaxWidth(static_cast<float>(value.toDouble()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.boxHeight")) {
    setBoxHeight(static_cast<float>(value.toDouble()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.paragraphSpacing")) {
    setParagraphSpacing(static_cast<float>(value.toDouble()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.color")) {
    const auto c = value.value<QColor>();
    setTextColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.strokeEnabled")) {
    setStrokeEnabled(value.toBool());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.strokeColor")) {
    const auto c = value.value<QColor>();
    setStrokeColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.strokeWidth")) {
    setStrokeWidth(static_cast<float>(value.toDouble()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.shadowEnabled")) {
    setShadowEnabled(value.toBool());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.shadowColor")) {
    const auto c = value.value<QColor>();
    setShadowColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.shadowOffsetX")) {
    setShadowOffset(static_cast<float>(value.toDouble()), shadowOffsetY());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.shadowOffsetY")) {
    setShadowOffset(shadowOffsetX(), static_cast<float>(value.toDouble()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.shadowBlur")) {
    setShadowBlur(static_cast<float>(value.toDouble()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.animatorCount")) {
    setAnimatorCount(value.toInt());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (const auto animatorPath = parseAnimatorPropertyPath(propertyPath)) {
    const int index = animatorPath->first;
    const QString field = animatorPath->second;
    if (index < 0 || index >= animatorCount()) {
      return false;
    }

    auto &animator = impl_->animators_[index];
    bool handled = true;
    if (field == QStringLiteral("name")) {
      animator.name = value.toString().trimmed();
      if (animator.name.isEmpty()) {
        animator.name = QStringLiteral("Animator %1").arg(index + 1);
      }
    } else if (field == QStringLiteral("enabled")) {
      animator.enabled = value.toBool();
    } else if (field == QStringLiteral("start")) {
      animator.range.start = static_cast<float>(value.toDouble());
    } else if (field == QStringLiteral("end")) {
      animator.range.end = static_cast<float>(value.toDouble());
    } else if (field == QStringLiteral("offset")) {
      animator.range.offset = static_cast<float>(value.toDouble());
    } else if (field == QStringLiteral("units")) {
      animator.range.units =
          static_cast<SelectorUnits>(value.toInt());
    } else if (field == QStringLiteral("shape")) {
      animator.range.shape =
          static_cast<SelectorShape>(value.toInt());
    } else if (field == QStringLiteral("wigglyEnabled")) {
      animator.wiggly.enabled = value.toBool();
    } else if (field == QStringLiteral("wigglesPerSecond")) {
      animator.wiggly.wigglesPerSecond = static_cast<float>(value.toDouble());
    } else if (field == QStringLiteral("correlation")) {
      animator.wiggly.correlation =
          std::clamp(static_cast<float>(value.toDouble()), 0.0f, 100.0f);
    } else if (field == QStringLiteral("phase")) {
      animator.wiggly.phase = static_cast<float>(value.toDouble());
    } else if (field == QStringLiteral("seed")) {
      animator.wiggly.seed = value.toInt();
    } else if (field == QStringLiteral("positionX")) {
      animator.properties.position.setX(static_cast<float>(value.toDouble()));
    } else if (field == QStringLiteral("positionY")) {
      animator.properties.position.setY(static_cast<float>(value.toDouble()));
    } else if (field == QStringLiteral("scale")) {
      animator.properties.scale =
          std::max(0.0f, static_cast<float>(value.toDouble()));
    } else if (field == QStringLiteral("rotation")) {
      animator.properties.rotation = static_cast<float>(value.toDouble());
    } else if (field == QStringLiteral("opacity")) {
      animator.properties.opacity =
          std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
    } else if (field == QStringLiteral("skew")) {
      animator.properties.skew = static_cast<float>(value.toDouble());
    } else if (field == QStringLiteral("tracking")) {
      animator.properties.tracking = static_cast<float>(value.toDouble());
    } else if (field == QStringLiteral("colorEnabled")) {
      animator.properties.colorEnabled = value.toBool();
    } else if (field == QStringLiteral("fillColor")) {
      const QColor color = value.value<QColor>();
      animator.properties.fillColor = toFloatRGBA(color);
      animator.properties.colorEnabled = true;
    } else {
      handled = false;
    }

    if (handled) {
      markDirty();
      setDirty(LayerDirtyFlag::Property);
      return true;
    }
  }
  return ArtifactAbstract2DLayer::setLayerPropertyValue(propertyPath, value);
}

void ArtifactTextLayer::updateImage() {
  const bool hasAnimators =
      std::any_of(impl_->animators_.begin(), impl_->animators_.end(),
                  [](const TextAnimatorState &animator) {
                    return animator.enabled;
                  });
  const bool boxLayout = isBoxText();
  Impl::CacheKey currentKey{impl_->text_, impl_->textStyle_,
                            impl_->paragraphStyle_};
  if (!hasAnimators && !impl_->isDirty_ && impl_->lastCacheKey_ &&
      *impl_->lastCacheKey_ == currentKey && !impl_->renderedImage_.isNull()) {
    return;
  }
  impl_->lastCacheKey_ = currentKey;

  QString displayText = impl_->text_.toQString();
  const bool isRichText = Qt::mightBeRichText(displayText);
  if (impl_->textStyle_.allCaps && !isRichText) {
    displayText = displayText.toUpper();
  }
  if (displayText.isEmpty()) {
    displayText = QStringLiteral(" ");
  }

  impl_->glyphs_.clear();
  if (!isRichText) {
    impl_->glyphs_ = TextLayoutEngine::layout(UniString(displayText),
                                              impl_->textStyle_,
                                              impl_->paragraphStyle_);
  }
  impl_->perGlyphMode_ = hasAnimators && !isRichText;

  if (impl_->perGlyphMode_) {
    const float timeSeconds =
        static_cast<float>(currentFrame()) / 30.0f;
    std::vector<std::tuple<RangeSelector, WigglySelector, AnimatorProperties>> animatorStack;
    animatorStack.reserve(impl_->animators_.size());
    for (const auto &animator : impl_->animators_) {
      if (!animator.enabled) {
        continue;
      }
      animatorStack.emplace_back(animator.range, animator.wiggly,
                                 animator.properties);
    }
    TextAnimatorEngine::applyAnimatorStack(impl_->glyphs_, animatorStack,
                                           timeSeconds);

    const QRectF glyphBounds = animatedGlyphBounds(impl_->glyphs_,
                                                   impl_->textStyle_);
    const qreal margin = textEffectMargin(impl_->textStyle_) +
                         std::max<qreal>(12.0, impl_->textStyle_.fontSize * 0.5);
    const qreal contentWidth = boxLayout && impl_->paragraphStyle_.boxWidth > 0.0f
                                   ? impl_->paragraphStyle_.boxWidth
                                   : std::max<qreal>(1.0, glyphBounds.width());
    const qreal contentHeight = boxLayout && impl_->paragraphStyle_.boxHeight > 0.0f
                                    ? impl_->paragraphStyle_.boxHeight
                                    : std::max<qreal>(1.0, glyphBounds.height());
    const int width = std::max(
        1, static_cast<int>(std::ceil(contentWidth + margin * 2.0)));
    const int height = std::max(
        1, static_cast<int>(std::ceil(contentHeight + margin * 2.0)));
    const QPointF drawOrigin(margin - glyphBounds.left(),
                             margin - glyphBounds.top());

    impl_->renderedImage_ =
        QImage(width, height, QImage::Format_ARGB32_Premultiplied);
    impl_->renderedImage_.fill(Qt::transparent);

    QPainter painter(&impl_->renderedImage_);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (impl_->textStyle_.shadowEnabled) {
      QImage shadowImg(width, height, QImage::Format_ARGB32_Premultiplied);
      shadowImg.fill(Qt::transparent);
      QPainter shadowPainter(&shadowImg);
      shadowPainter.setRenderHint(QPainter::Antialiasing);
      shadowPainter.setRenderHint(QPainter::TextAntialiasing);
      shadowPainter.translate(drawOrigin + QPointF(impl_->textStyle_.shadowOffsetX,
                                                   impl_->textStyle_.shadowOffsetY));
      drawAnimatedGlyphRun(shadowPainter, impl_->glyphs_, impl_->textStyle_,
                           toQColor(impl_->textStyle_.shadowColor),
                           toQColor(impl_->textStyle_.shadowColor), true, false,
                           false);
      shadowPainter.end();

      if (impl_->textStyle_.shadowBlur > 0.1f) {
        cv::Mat mat(shadowImg.height(), shadowImg.width(), CV_8UC4,
                    const_cast<uchar *>(shadowImg.bits()),
                    shadowImg.bytesPerLine());
        int ksize = static_cast<int>(impl_->textStyle_.shadowBlur * 3.0f);
        if (ksize % 2 == 0) {
          ++ksize;
        }
        if (ksize >= 3) {
          cv::GaussianBlur(mat, mat, cv::Size(ksize, ksize),
                           impl_->textStyle_.shadowBlur);
        }
      }

      painter.drawImage(0, 0, shadowImg);
    }

    painter.save();
    painter.translate(drawOrigin);
    drawAnimatedGlyphRun(painter, impl_->glyphs_, impl_->textStyle_,
                         toQColor(impl_->textStyle_.fillColor),
                         toQColor(impl_->textStyle_.strokeColor), true, true,
                         true);
    painter.restore();
    painter.end();

    setSourceSize(Size_2D(width, height));
    impl_->renderedBuffer_ = std::make_shared<ArtifactCore::ImageF32x4_RGBA>();
    const cv::Mat mat =
        ArtifactCore::CvUtils::qImageToCvMat(impl_->renderedImage_, true);
    if (!mat.empty()) {
      cv::Mat rgba = mat;
      if (rgba.type() != CV_32FC4) {
        rgba.convertTo(rgba, CV_32FC4, 1.0 / 255.0);
      }
      impl_->renderedBuffer_->setFromCVMat(rgba);
    }
    impl_->isDirty_ = false;
    return;
  }

  // Set up QTextDocument for rich text support
  QTextDocument doc;
  doc.setUndoRedoEnabled(false);

  // Set default font
  QFont defaultFont = FontManager::makeFont(impl_->textStyle_, displayText);
  doc.setDefaultFont(defaultFont);

  // Apply basic styles to the whole document
  QTextOption option = doc.defaultTextOption();
  switch (impl_->paragraphStyle_.wrapMode) {
  case TextWrapMode::NoWrap:
    option.setWrapMode(QTextOption::NoWrap);
    break;
  case TextWrapMode::WrapAnywhere:
    option.setWrapMode(QTextOption::WrapAnywhere);
    break;
  case TextWrapMode::ManualWrap:
    option.setWrapMode(QTextOption::ManualWrap);
    break;
  case TextWrapMode::WordWrap:
  default:
    option.setWrapMode(QTextOption::WordWrap);
    break;
  }
  switch (impl_->paragraphStyle_.horizontalAlignment) {
  case TextHorizontalAlignment::Center:
    option.setAlignment(Qt::AlignCenter);
    break;
  case TextHorizontalAlignment::Right:
    option.setAlignment(Qt::AlignRight);
    break;
  case TextHorizontalAlignment::Justify:
    option.setAlignment(Qt::AlignJustify);
    break;
  default:
    option.setAlignment(Qt::AlignLeft);
    break;
  }
  doc.setDefaultTextOption(option);

  // Set content - handles HTML or Plain Text
  if (isRichText) {
    doc.setHtml(displayText);
  } else {
    doc.setPlainText(displayText);
  }

  if (impl_->paragraphStyle_.paragraphSpacing > 0.0f ||
      impl_->textStyle_.leading > 0.0f) {
    for (QTextBlock block = doc.begin(); block.isValid();
         block = block.next()) {
      QTextCursor cursor(block);
      QTextBlockFormat format = block.blockFormat();
      if (impl_->paragraphStyle_.paragraphSpacing > 0.0f) {
        format.setBottomMargin(impl_->paragraphStyle_.paragraphSpacing);
      }
      if (impl_->textStyle_.leading > 0.0f) {
        format.setLineHeight(impl_->textStyle_.leading * 100.0f,
                             QTextBlockFormat::ProportionalHeight);
      }
      cursor.setBlockFormat(format);
    }
  }

  if (boxLayout && impl_->paragraphStyle_.boxWidth > 0.0f) {
    doc.setTextWidth(impl_->paragraphStyle_.boxWidth);
  }

  // Calculate size
  doc.adjustSize();
  qreal docWidth = doc.size().width();
  qreal docHeight = doc.size().height();

  const qreal boxWidth = boxLayout && impl_->paragraphStyle_.boxWidth > 0.0f
                             ? impl_->paragraphStyle_.boxWidth
                             : docWidth;
  const qreal boxHeight = boxLayout && impl_->paragraphStyle_.boxHeight > 0.0f
                              ? impl_->paragraphStyle_.boxHeight
                              : docHeight;

  // Padding for stroke and shadow blur
  const qreal margin =
      24.0 +
      (impl_->textStyle_.strokeEnabled ? impl_->textStyle_.strokeWidth : 0.0) +
      (impl_->textStyle_.shadowEnabled ? impl_->textStyle_.shadowBlur * 2.0
                                       : 0.0);

  const int width =
      std::max(1, static_cast<int>(std::ceil(boxWidth + margin * 2.0)));
  const int height =
      std::max(1, static_cast<int>(std::ceil(boxHeight + margin * 2.0)));

  impl_->renderedImage_ =
      QImage(width, height, QImage::Format_ARGB32_Premultiplied);
  impl_->renderedImage_.fill(Qt::transparent);

  QPainter painter(&impl_->renderedImage_);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  qreal verticalOffset = 0.0;
  if (boxLayout && boxHeight > docHeight) {
    switch (impl_->paragraphStyle_.verticalAlignment) {
    case TextVerticalAlignment::Middle:
      verticalOffset = (boxHeight - docHeight) * 0.5;
      break;
    case TextVerticalAlignment::Bottom:
      verticalOffset = boxHeight - docHeight;
      break;
    case TextVerticalAlignment::Top:
    default:
      verticalOffset = 0.0;
      break;
    }
  }

  // Offset for margins
  painter.translate(margin, margin + verticalOffset);

  // Helper to draw document layout as paths (for stroke support)
  auto drawDocAsPath = [&](QPainter &p, bool drawFill, bool drawStroke,
                           const QColor &fillOverride = QColor()) {
    QAbstractTextDocumentLayout *layout = doc.documentLayout();
    QTextBlock block = doc.begin();
    while (block.isValid()) {
      QRectF blockRect = layout->blockBoundingRect(block);
      for (auto it = block.begin(); !it.atEnd(); ++it) {
        QTextFragment fragment = it.fragment();
        if (fragment.isValid()) {
          QFont f = fragment.charFormat().font();
          QTextLayout *blockLayout = block.layout();
          int relativePos = fragment.position() - block.position();

          // We iterate through lines to handle wrapping and alignment
          for (int i = 0; i < blockLayout->lineCount(); ++i) {
            QTextLine line = blockLayout->lineAt(i);
            if (relativePos >= line.textStart() &&
                relativePos < (line.textStart() + line.textLength())) {
              QPointF linePos = line.position();
              qreal x = line.cursorToX(relativePos);

              QPainterPath path;
              path.addText(blockRect.topLeft() + linePos +
                               QPointF(x, line.ascent()),
                           f, fragment.text());

              if (drawStroke && impl_->textStyle_.strokeEnabled) {
                QColor strokeCol =
                    QColor::fromRgbF(impl_->textStyle_.strokeColor.r(),
                                     impl_->textStyle_.strokeColor.g(),
                                     impl_->textStyle_.strokeColor.b(),
                                     impl_->textStyle_.strokeColor.a());
                QPen pen(strokeCol, impl_->textStyle_.strokeWidth,
                         Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                p.strokePath(path, pen);
              }

              if (drawFill) {
                QColor fillCol;
                if (fragment.charFormat().foreground().style() != Qt::NoBrush) {
                  fillCol = fragment.charFormat().foreground().color();
                } else if (fillOverride.isValid()) {
                  fillCol = fillOverride;
                } else {
                  fillCol = QColor::fromRgbF(impl_->textStyle_.fillColor.r(),
                                             impl_->textStyle_.fillColor.g(),
                                             impl_->textStyle_.fillColor.b(),
                                             impl_->textStyle_.fillColor.a());
                }
                p.fillPath(path, fillCol);

                if (impl_->textStyle_.underline ||
                    impl_->textStyle_.strikethrough) {
                  const QFontMetricsF metrics(f);
                  const QPointF origin =
                      blockRect.topLeft() + linePos + QPointF(x, line.ascent());
                  const qreal fragmentWidth = std::max<qreal>(
                      0.0, metrics.horizontalAdvance(fragment.text()));
                  const qreal decoWidth = std::max<qreal>(
                      1.0, std::max<qreal>(impl_->textStyle_.strokeWidth, 1.2));
                  QPen decoPen(fillCol, decoWidth, Qt::SolidLine, Qt::SquareCap,
                               Qt::MiterJoin);
                  p.setPen(decoPen);
                  if (impl_->textStyle_.underline) {
                    const qreal underlineY =
                        origin.y() + metrics.underlinePos();
                    p.drawLine(QPointF(origin.x(), underlineY),
                               QPointF(origin.x() + fragmentWidth, underlineY));
                  }
                  if (impl_->textStyle_.strikethrough) {
                    const qreal strikeY = origin.y() + metrics.strikeOutPos();
                    p.drawLine(QPointF(origin.x(), strikeY),
                               QPointF(origin.x() + fragmentWidth, strikeY));
                  }
                }
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
    shadowPainter.translate(margin, margin + verticalOffset);

    // Shadow is simplified as a single color fill path
    drawDocAsPath(shadowPainter, true, false,
                  QColor::fromRgbF(impl_->textStyle_.shadowColor.r(),
                                   impl_->textStyle_.shadowColor.g(),
                                   impl_->textStyle_.shadowColor.b(),
                                   impl_->textStyle_.shadowColor.a()));
    shadowPainter.end();

    if (impl_->textStyle_.shadowBlur > 0.1f) {
      cv::Mat mat(shadowImg.height(), shadowImg.width(), CV_8UC4,
                  const_cast<uchar *>(shadowImg.bits()),
                  shadowImg.bytesPerLine());
      int ksize = static_cast<int>(impl_->textStyle_.shadowBlur * 3.0f);
      if (ksize % 2 == 0)
        ksize++;
      if (ksize >= 3) {
        cv::GaussianBlur(mat, mat, cv::Size(ksize, ksize),
                         impl_->textStyle_.shadowBlur);
      }
    }

    painter.save();
    painter.translate(-margin + impl_->textStyle_.shadowOffsetX,
                      -margin + impl_->textStyle_.shadowOffsetY);
    painter.drawImage(0, 0, shadowImg);
    painter.restore();
  }

  // 2. Draw Main Content (Stroke then Fill)
  drawDocAsPath(painter, true, true);

  painter.end();
  setSourceSize(Size_2D(width, height));
  impl_->renderedBuffer_ = std::make_shared<ArtifactCore::ImageF32x4_RGBA>();
  const cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(impl_->renderedImage_, true);
  if (!mat.empty()) {
    cv::Mat rgba = mat;
    if (rgba.type() != CV_32FC4) {
      rgba.convertTo(rgba, CV_32FC4, 1.0 / 255.0);
    }
    impl_->renderedBuffer_->setFromCVMat(rgba);
  }
  impl_->isDirty_ = false;
}

} // namespace Artifact

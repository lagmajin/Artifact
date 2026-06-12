module;
#include <QAbstractTextDocumentLayout>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
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
#include <QStringList>
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
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;
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
import Text.LayoutContract;
import Text.GlyphLayout;
import Text.ShapingBackend;
import Text.Animator;
import Time.Rational;
import Artifact.Mask.Path;
import Artifact.Mask.LayerMask;

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
  TextWritingMode writingMode_ = TextWritingMode::Horizontal;
  QString rubyText_;
  float rubyScale_ = 0.5f;
  TextLayoutMode layoutMode_ = defaultTextLayoutMode();
  QImage renderedImage_;
  mutable std::shared_ptr<ArtifactCore::ImageF32x4_RGBA> renderedBuffer_;
  bool isDirty_ = true;

  // Text Animator support
  std::vector<GlyphItem> glyphs_;
  TextLayoutContract layoutContract_;
  std::vector<TextAnimatorState> animators_;
  bool perGlyphMode_ = false;

  // Path Text support
  std::vector<ArtifactCore::BezierSegment> pathSegments_;

  // Cache key to detect if we actually need to re-render
  struct CacheKey {
    UniString text;
    TextStyle style;
    ParagraphStyle paragraph;
    TextWritingMode writingMode;
    QString rubyText;
    float rubyScale;
    TextLayoutMode layoutMode;
    std::vector<ArtifactCore::BezierSegment> pathSegments;

    bool operator==(const CacheKey &o) const {
      return text == o.text && style == o.style && paragraph == o.paragraph &&
             writingMode == o.writingMode && rubyText == o.rubyText &&
             rubyScale == o.rubyScale && layoutMode == o.layoutMode &&
             pathSegments == o.pathSegments;
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

QVector<TextRubyAttachment> buildRubyAttachments(const UniString &text,
                                                 const QString &rubyText,
                                                 const float rubyScale) {
  QVector<TextRubyAttachment> attachments;
  if (rubyText.isEmpty() || text.length() <= 0) {
    return attachments;
  }
  attachments.push_back(TextRubyAttachment{
      .baseLogicalStart = 0,
      .baseLogicalLength = static_cast<int>(text.length()),
      .rubyText = rubyText,
      .rubyScale = rubyScale,
      .rubyOffset = 0.0f,
  });
  return attachments;
}

int64_t effectiveTextTimelineFrame(const ArtifactTextLayer *layer) {
  if (!layer) {
    return 0;
  }
  if (auto *composition = static_cast<ArtifactAbstractComposition *>(
          layer->composition())) {
    return composition->framePosition().framePosition();
  }
  return layer->currentFrame();
}

int64_t effectiveTextTimelineFps(const ArtifactTextLayer *layer) {
  if (!layer) {
    return 30;
  }
  if (auto *composition = static_cast<ArtifactAbstractComposition *>(
          layer->composition())) {
    const double fps = composition->frameRate().framerate();
    return std::max<int64_t>(1, static_cast<int64_t>(std::llround(fps)));
  }
  return 30;
}

RationalTime effectiveTextTimelineTime(const ArtifactTextLayer *layer) {
  return RationalTime(effectiveTextTimelineFrame(layer),
                      effectiveTextTimelineFps(layer));
}

QString resolvedSourceTextAtTime(const ArtifactTextLayer *layer) {
  if (!layer) {
    return QString();
  }
  if (const auto property = layer->getProperty(QStringLiteral("text.value"));
      property && !property->getKeyFrames().empty()) {
    const QVariant value =
        property->interpolateValue(effectiveTextTimelineTime(layer));
    if (value.isValid()) {
      return value.toString();
    }
  }
  return layer->text().toQString();
}

bool sourceTextIsAnimated(const ArtifactTextLayer *layer) {
  if (!layer) {
    return false;
  }
  if (const auto property = layer->getProperty(QStringLiteral("text.value"));
      property) {
    return !property->getKeyFrames().empty();
  }
  return false;
}

bool isAnimatorPropertyAnimatable(const QString &suffix) {
  return suffix == QStringLiteral("start") ||
         suffix == QStringLiteral("end") ||
         suffix == QStringLiteral("offset") ||
         suffix == QStringLiteral("wigglesPerSecond") ||
         suffix == QStringLiteral("correlation") ||
         suffix == QStringLiteral("phase") ||
         suffix == QStringLiteral("seed") ||
         suffix == QStringLiteral("positionX") ||
         suffix == QStringLiteral("positionY") ||
         suffix == QStringLiteral("scale") ||
         suffix == QStringLiteral("rotation") ||
         suffix == QStringLiteral("opacity") ||
         suffix == QStringLiteral("skew") ||
         suffix == QStringLiteral("tracking") ||
         suffix == QStringLiteral("z") ||
         suffix == QStringLiteral("fillColor") ||
         suffix == QStringLiteral("strokeColor") ||
         suffix == QStringLiteral("strokeWidth") ||
         suffix == QStringLiteral("strokeEnabled") ||
         suffix == QStringLiteral("colorEnabled") ||
         suffix == QStringLiteral("blur");
}

QString selectorUnitsTooltip() {
  return QStringLiteral("0=Percentage, 1=Index, 2=Cluster, 3=Line, 4=Tag");
}

QString selectorShapeTooltip() {
  return QStringLiteral(
      "0=Square, 1=Ramp Up, 2=Ramp Down, 3=Triangle, 4=Round, 5=Smooth");
}

QVector<float> selectorWeightPreviewForAnimators(
    const std::vector<TextAnimatorState> &animators,
    int sampleCount,
    int textLength) {
  QVector<float> preview;
  if (sampleCount <= 0) {
    return preview;
  }
  preview.fill(0.0f, sampleCount);
  if (textLength <= 0 || animators.empty()) {
    return preview;
  }

  for (int sample = 0; sample < sampleCount; ++sample) {
    const int index = (sampleCount == 1)
                          ? 0
                          : static_cast<int>(std::round(
                                (static_cast<double>(sample) /
                                 static_cast<double>(sampleCount - 1)) *
                                static_cast<double>(std::max(0, textLength - 1))));
    float maxWeight = 0.0f;
    for (const auto &animator : animators) {
      if (!animator.enabled) {
        continue;
      }
      maxWeight = std::max(
          maxWeight,
          TextAnimatorEngine::calculateWeight(index, std::max(textLength, 1),
                                              animator.range));
    }
    preview[sample] = std::clamp(maxWeight, 0.0f, 1.0f);
  }
  return preview;
}

QVector<float> selectorWeightPreviewForGlyphs(
    const std::vector<GlyphItem> &glyphs,
    const std::vector<TextAnimatorState> &animators,
    int sampleCount) {
  QVector<float> preview;
  if (sampleCount <= 0) {
    return preview;
  }
  preview.fill(0.0f, sampleCount);
  if (glyphs.empty() || animators.empty()) {
    return preview;
  }

  int clusterCount = 0;
  int lineCount = 0;
  QHash<QString, int> tagOrder;
  std::vector<int> tagIndices;
  tagIndices.reserve(glyphs.size());
  for (const auto &glyph : glyphs) {
    if (glyph.clusterIndex >= 0) {
      clusterCount = std::max(clusterCount, glyph.clusterIndex + 1);
    }
    if (glyph.lineIndex >= 0) {
      lineCount = std::max(lineCount, glyph.lineIndex + 1);
    }
    const QString tag = glyph.selectorTag.isEmpty() ? QStringLiteral("untagged")
                                                    : glyph.selectorTag;
    const auto it = tagOrder.constFind(tag);
    if (it == tagOrder.cend()) {
      const int index = tagOrder.size();
      tagOrder.insert(tag, index);
      tagIndices.push_back(index);
    } else {
      tagIndices.push_back(it.value());
    }
  }
  const int tagCount = tagOrder.size();

  const int glyphCount = static_cast<int>(glyphs.size());
  for (int sample = 0; sample < sampleCount; ++sample) {
    const int glyphIndex = (sampleCount == 1)
                               ? 0
                               : static_cast<int>(std::round(
                                     (static_cast<double>(sample) /
                                      static_cast<double>(sampleCount - 1)) *
                                     static_cast<double>(std::max(0, glyphCount - 1))));
    const GlyphItem &glyph = glyphs[static_cast<size_t>(glyphIndex)];
    float maxWeight = 0.0f;
    for (const auto &animator : animators) {
      if (!animator.enabled) {
        continue;
      }
      maxWeight = std::max(
          maxWeight,
          TextAnimatorEngine::calculateWeightForGlyph(
              glyph, glyphIndex, glyphCount, clusterCount, lineCount,
              tagIndices[static_cast<size_t>(glyphIndex)], tagCount,
              animator.range));
    }
    preview[sample] = std::clamp(maxWeight, 0.0f, 1.0f);
  }
  return preview;
}

QVector<float> selectorBoundaryPreviewForGlyphs(
    const std::vector<GlyphItem> &glyphs,
    const bool useLineIndex) {
  QVector<float> preview;
  if (glyphs.size() < 2) {
    return preview;
  }

  const int total = static_cast<int>(glyphs.size());
  for (int i = 1; i < total; ++i) {
    const int previous = useLineIndex ? glyphs[static_cast<size_t>(i - 1)].lineIndex
                                      : glyphs[static_cast<size_t>(i - 1)].clusterIndex;
    const int current = useLineIndex ? glyphs[static_cast<size_t>(i)].lineIndex
                                     : glyphs[static_cast<size_t>(i)].clusterIndex;
    if (previous == current) {
      continue;
    }
    const float position = static_cast<float>(i) / static_cast<float>(std::max(1, total - 1));
    preview.push_back(position);
  }
  return preview;
}

QString animatorPresetTooltip() {
  return QStringLiteral(
      "0=Custom, 1=Typewriter, 2=Slide Up, 3=Scale In, 4=Rotation In, "
      "5=Tracking Fade, 6=Wiggly Position, 7=Blur Reveal");
}

QString textUnitBadgeForState(const QString &displayText,
                              const TextWritingMode writingMode,
                              const TextLayoutMode layoutMode,
                              const bool hasAnimators,
                              const bool hasRuby,
                              const bool hasCjk,
                              const bool isRichText) {
  const QString coreUnit =
      isRichText ? QStringLiteral("glyph-aware") :
      (hasAnimators || hasRuby || hasCjk || writingMode == TextWritingMode::Vertical
           ? QStringLiteral("cluster-aware")
           : QStringLiteral("glyph-aware"));
  const QString flowUnit =
      writingMode == TextWritingMode::Vertical ? QStringLiteral("vertical")
                                               : QStringLiteral("horizontal");

  QStringList tags;
  tags.push_back(coreUnit);
  tags.push_back(flowUnit);
  if (layoutMode == TextLayoutMode::Path) {
    tags.push_back(QStringLiteral("path"));
  } else if (layoutMode == TextLayoutMode::Box) {
    tags.push_back(QStringLiteral("box"));
  } else {
    tags.push_back(QStringLiteral("point"));
  }
  if (hasRuby) {
    tags.push_back(QStringLiteral("ruby"));
  }
  if (hasAnimators) {
    tags.push_back(QStringLiteral("animators"));
  }
  if (hasCjk && !displayText.isEmpty()) {
    tags.push_back(QStringLiteral("cjk"));
  }
  if (displayText.contains(QChar::LineFeed) || displayText.contains(QChar::CarriageReturn) ||
      layoutMode == TextLayoutMode::Box) {
    tags.push_back(QStringLiteral("line-aware"));
  }
  return tags.join(QStringLiteral(" / "));
}

QString textSelectionTargetForState(const bool hasAnimators,
                                    const bool hasRuby,
                                    const bool hasCjk,
                                    const bool isRichText,
                                    const TextWritingMode writingMode,
                                    const bool lineAware,
                                    const bool hasRegexSelector) {
  const QString unit =
      (isRichText || hasAnimators || hasRuby || hasCjk ||
       writingMode == TextWritingMode::Vertical)
          ? QStringLiteral("cluster")
          : QStringLiteral("glyph");
  QStringList tags;
  tags.push_back(unit);
  if (lineAware) {
    tags.push_back(QStringLiteral("line"));
  }
  if (hasRegexSelector) {
    tags.push_back(QStringLiteral("tag"));
  }
  return tags.join(QStringLiteral(" / "));
}

QString textSelectionTargetLabelForState(const bool hasAnimators,
                                         const bool hasRuby,
                                         const bool hasCjk,
                                         const bool isRichText,
                                         const TextWritingMode writingMode,
                                         const bool lineAware,
                                         const bool hasRegexSelector) {
  const QString unit =
      (isRichText || hasAnimators || hasRuby || hasCjk ||
       writingMode == TextWritingMode::Vertical)
          ? QStringLiteral("cluster")
          : QStringLiteral("glyph");
  QStringList tags;
  tags.push_back(unit);
  if (lineAware) {
    tags.push_back(QStringLiteral("line"));
  }
  if (hasRegexSelector) {
    tags.push_back(QStringLiteral("tag"));
  }
  return tags.join(QStringLiteral("-"));
}

QString textSelectionUnitLabelForState(const bool hasAnimators,
                                       const bool hasRuby,
                                       const bool hasCjk,
                                       const bool isRichText,
                                       const TextWritingMode writingMode,
                                       const bool lineAware) {
  const QString unit =
      (isRichText || hasAnimators || hasRuby || hasCjk ||
       writingMode == TextWritingMode::Vertical)
          ? QStringLiteral("cluster")
          : QStringLiteral("glyph");
  return lineAware ? QStringLiteral("unit=%1;lineAware=true").arg(unit)
                   : QStringLiteral("unit=%1").arg(unit);
}

QString selectorTagLabelForText(const QString &text) {
  bool hasHangul = false;
  bool hasCjk = false;
  bool hasRtl = false;
  bool hasThai = false;
  bool hasIndic = false;
  bool hasEmoji = false;
  bool hasLatin = false;

  for (const QChar ch : text) {
    const uint code = ch.unicode();
    if (code >= 0xAC00 && code <= 0xD7AF) {
      hasHangul = true;
    } else if ((code >= 0x3040 && code <= 0x30FF) ||
               (code >= 0x4E00 && code <= 0x9FFF)) {
      hasCjk = true;
    } else if ((code >= 0x0590 && code <= 0x08FF) ||
               (code >= 0xFB1D && code <= 0xFEFF)) {
      hasRtl = true;
    } else if (code >= 0x0E00 && code <= 0x0E7F) {
      hasThai = true;
    } else if ((code >= 0x0900 && code <= 0x097F) ||
               (code >= 0x0980 && code <= 0x09FF) ||
               (code >= 0x0A00 && code <= 0x0DFF) ||
               (code >= 0x1780 && code <= 0x17FF) ||
               (code >= 0x1000 && code <= 0x109F) ||
               (code >= 0x0F00 && code <= 0x0FFF)) {
      hasIndic = true;
    } else if (code >= 0x1F300 && code <= 0x1FAFF) {
      hasEmoji = true;
    } else if ((code >= U'A' && code <= U'Z') || (code >= U'a' && code <= U'z')) {
      hasLatin = true;
    }
  }

  int familyCount = 0;
  QString tag;
  const auto assignTag = [&](const QString &candidate) {
    ++familyCount;
    tag = candidate;
  };
  if (hasHangul) assignTag(QStringLiteral("Hang"));
  if (hasCjk) assignTag(QStringLiteral("Hani"));
  if (hasRtl) assignTag(QStringLiteral("Rtl"));
  if (hasThai) assignTag(QStringLiteral("Thai"));
  if (hasIndic) assignTag(QStringLiteral("Indic"));
  if (hasEmoji) assignTag(QStringLiteral("Emoji"));
  if (hasLatin) assignTag(QStringLiteral("Latn"));

  if (familyCount == 0) {
    return QStringLiteral("tag=unknown");
  }
  if (familyCount > 1) {
    return QStringLiteral("tag=mixed");
  }
  return QStringLiteral("tag=%1").arg(tag);
}

QString selectorTokenLabelForGlyphs(const std::vector<GlyphItem> &glyphs) {
  if (glyphs.empty()) {
    return QStringLiteral("token=none");
  }
  const GlyphItem &glyph = glyphs.front();
  if (glyph.stableTokenId.isEmpty()) {
    return QStringLiteral("token=unstable");
  }
  return QStringLiteral("token=%1").arg(glyph.stableTokenId);
}

QString selectorScriptLabelForContract(const TextLayoutContract &contract) {
  if (contract.scriptRuns.isEmpty()) {
    return QStringLiteral("scripts=0");
  }

  const TextScriptRun &run = contract.scriptRuns.front();
  const QString direction =
      run.direction == TextDirection::RightToLeft
          ? QStringLiteral("rtl")
          : (run.direction == TextDirection::LeftToRight
                 ? QStringLiteral("ltr")
                 : QStringLiteral("auto"));
  return QStringLiteral("scripts=%1;script=%2;dir=%3;complex=%4")
      .arg(contract.scriptRuns.size())
      .arg(run.scriptTag)
      .arg(direction)
      .arg(run.isComplexScript ? QStringLiteral("yes")
                               : QStringLiteral("no"));
}

QString selectorVerticalLabelForContract(const TextLayoutContract &contract) {
  return QStringLiteral("tcy=%1;punct=%2;brackets=%3;kinsoku=%4")
      .arg(contract.tateChuYokoRuns.size())
      .arg(contract.punctuationRuns.size())
      .arg(contract.bracketOrientationRuns.size())
      .arg(contract.kinsokuBoundaryInfos.size());
}

QString textWritingModeLabel(const TextWritingMode writingMode) {
  return writingMode == TextWritingMode::Vertical ? QStringLiteral("mode=vertical")
                                                   : QStringLiteral("mode=horizontal");
}

QString textOrderingLabel(const TextWritingMode writingMode) {
  return writingMode == TextWritingMode::Vertical
             ? QStringLiteral("source=logical;visual=column")
             : QStringLiteral("source=logical;visual=flow");
}

QString selectorOverviewTooltip() {
  return QStringLiteral(
      "Primary summary for the current selector state in compact key=value form, including target, mode, source, visual, unit, regex, tag, script, token, clusters, and lines.");
}

bool containsRtlCodepoint(const QString &text) {
  for (const QChar ch : text) {
    const uint code = ch.unicode();
    if ((code >= 0x0590 && code <= 0x08FF) ||
        (code >= 0xFB1D && code <= 0xFDFF) ||
        (code >= 0xFE70 && code <= 0xFEFF)) {
      return true;
    }
  }
  return false;
}

TextDirection inferredBaseDirection(const QString &text) {
  return containsRtlCodepoint(text) ? TextDirection::RightToLeft
                                    : TextDirection::LeftToRight;
}

TextShapingResult layoutTextShape(const UniString &text,
                                  const TextStyle &style,
                                  const ParagraphStyle &paragraph,
                                  const TextWritingMode writingMode,
                                  const QVector<TextRubyAttachment> &rubyAttachments,
                                  const TextLayoutMode layoutMode,
                                  const std::vector<ArtifactCore::BezierSegment> &pathSegments) {
  if (layoutMode == TextLayoutMode::Path) {
    TextShapingResult result;
    result.glyphs =
        TextLayoutEngine::layoutOnPath(text, style, paragraph, pathSegments);
    return result;
  }

  QtShapingBackend backend;
  TextShapingRequest request;
  request.text = text.toQString();
  request.style = style;
  request.paragraph = paragraph;
  request.writingMode = writingMode;
  request.baseDirection = inferredBaseDirection(request.text);
  request.rubyAttachments = rubyAttachments;
  return backend.shape(request);
}

RangeSelector presetRange() {
  RangeSelector range;
  range.start = 0.0f;
  range.end = 100.0f;
  range.offset = 0.0f;
  range.units = SelectorUnits::Percentage;
  range.shape = SelectorShape::RampUp;
  return range;
}

TextAnimatorState makePresetAnimator(const QString &name) {
  TextAnimatorState state;
  state.name = name;
  state.range = presetRange();
  return state;
}

bool fuzzyEqual(const float a, const float b, const float epsilon = 0.0001f) {
  return std::abs(a - b) <= epsilon;
}

bool fuzzyEqual(const QPointF &a, const QPointF &b,
                const float epsilon = 0.0001f) {
  return fuzzyEqual(static_cast<float>(a.x()), static_cast<float>(b.x()),
                    epsilon) &&
         fuzzyEqual(static_cast<float>(a.y()), static_cast<float>(b.y()),
                    epsilon);
}

bool fuzzyEqual(const FloatRGBA &a, const FloatRGBA &b,
                const float epsilon = 0.0001f) {
  return fuzzyEqual(a.r(), b.r(), epsilon) && fuzzyEqual(a.g(), b.g(), epsilon) &&
         fuzzyEqual(a.b(), b.b(), epsilon) && fuzzyEqual(a.a(), b.a(), epsilon);
}

bool sameTextAnimatorState(const TextAnimatorState &a,
                           const TextAnimatorState &b) {
  return a.name == b.name && a.enabled == b.enabled &&
         fuzzyEqual(a.range.start, b.range.start) &&
         fuzzyEqual(a.range.end, b.range.end) &&
         fuzzyEqual(a.range.offset, b.range.offset) &&
         a.range.units == b.range.units && a.range.shape == b.range.shape &&
         a.range.regexEnabled == b.range.regexEnabled &&
         a.range.selectorPattern == b.range.selectorPattern &&
         fuzzyEqual(a.range.easeHigh, b.range.easeHigh) &&
         fuzzyEqual(a.range.easeLow, b.range.easeLow) &&
         a.wiggly.enabled == b.wiggly.enabled &&
         fuzzyEqual(a.wiggly.wigglesPerSecond, b.wiggly.wigglesPerSecond) &&
         fuzzyEqual(a.wiggly.correlation, b.wiggly.correlation) &&
         fuzzyEqual(a.wiggly.phase, b.wiggly.phase) &&
         a.wiggly.seed == b.wiggly.seed &&
         fuzzyEqual(a.properties.position, b.properties.position) &&
         fuzzyEqual(a.properties.scale, b.properties.scale) &&
         fuzzyEqual(a.properties.rotation, b.properties.rotation) &&
         fuzzyEqual(a.properties.opacity, b.properties.opacity) &&
         fuzzyEqual(a.properties.skew, b.properties.skew) &&
         fuzzyEqual(a.properties.tracking, b.properties.tracking) &&
         fuzzyEqual(a.properties.z, b.properties.z) &&
         a.properties.colorEnabled == b.properties.colorEnabled &&
         fuzzyEqual(a.properties.fillColor, b.properties.fillColor) &&
         a.properties.strokeEnabled == b.properties.strokeEnabled &&
         fuzzyEqual(a.properties.strokeColor, b.properties.strokeColor) &&
         fuzzyEqual(a.properties.strokeWidth, b.properties.strokeWidth) &&
         fuzzyEqual(a.properties.blur, b.properties.blur);
}

std::vector<TextAnimatorState> buildTextAnimatorPreset(const int presetId) {
  std::vector<TextAnimatorState> animators;
  switch (presetId) {
  case 1: {
    auto animator = makePresetAnimator(QStringLiteral("Typewriter"));
    animator.properties.scale = 0.0f;
    animator.properties.opacity = 0.0f;
    animator.properties.tracking = 12.0f;
    animator.properties.blur = 3.0f;
    animators.push_back(animator);
    break;
  }
  case 2: {
    auto animator = makePresetAnimator(QStringLiteral("Slide Up"));
    animator.properties.position = QPointF(0.0, 72.0);
    animator.properties.opacity = 0.25f;
    animators.push_back(animator);
    break;
  }
  case 3: {
    auto animator = makePresetAnimator(QStringLiteral("Scale In"));
    animator.properties.scale = 0.0f;
    animator.properties.opacity = 0.0f;
    animators.push_back(animator);
    break;
  }
  case 4: {
    auto animator = makePresetAnimator(QStringLiteral("Rotation In"));
    animator.properties.scale = 0.0f;
    animator.properties.rotation = 35.0f;
    animator.properties.opacity = 0.0f;
    animators.push_back(animator);
    break;
  }
  case 5: {
    auto animator = makePresetAnimator(QStringLiteral("Tracking Fade"));
    animator.properties.tracking = 24.0f;
    animator.properties.scale = 0.9f;
    animator.properties.opacity = 0.25f;
    animators.push_back(animator);
    break;
  }
  case 6: {
    auto animator = makePresetAnimator(QStringLiteral("Wiggly Position"));
    animator.range.shape = SelectorShape::Smooth;
    animator.wiggly.enabled = true;
    animator.wiggly.wigglesPerSecond = 4.5f;
    animator.wiggly.correlation = 60.0f;
    animator.wiggly.phase = 0.0f;
    animator.wiggly.seed = 1337;
    animator.properties.position = QPointF(28.0, -18.0);
    animator.properties.rotation = 4.0f;
    animators.push_back(animator);
    break;
  }
  case 7: {
    auto animator = makePresetAnimator(QStringLiteral("Blur Reveal"));
    animator.properties.scale = 0.0f;
    animator.properties.opacity = 0.0f;
    animator.properties.blur = 10.0f;
    animators.push_back(animator);
    break;
  }
  case 0:
  default:
    break;
  }
  return animators;
}

int inferTextAnimatorPresetId(const std::vector<TextAnimatorState> &animators) {
  for (int presetId = 1; presetId <= 7; ++presetId) {
    const auto presetAnimators = buildTextAnimatorPreset(presetId);
    if (presetAnimators.size() != animators.size()) {
      continue;
    }
    bool match = true;
    for (size_t i = 0; i < animators.size(); ++i) {
      if (!sameTextAnimatorState(animators[i], presetAnimators[i])) {
        match = false;
        break;
      }
    }
    if (match) {
      return presetId;
    }
  }
  return 0;
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
  rangeObj["regexEnabled"] = animator.range.regexEnabled;
  rangeObj["selectorPattern"] = animator.range.selectorPattern;
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
    animator.range.regexEnabled = rangeObj.value("regexEnabled").toBool(animator.range.regexEnabled);
    animator.range.selectorPattern = rangeObj.value("selectorPattern").toString(animator.range.selectorPattern);
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

void ArtifactTextLayer::setWritingMode(TextWritingMode mode) {
  impl_->writingMode_ = mode;
  markDirty();
}

TextWritingMode ArtifactTextLayer::writingMode() const {
  return impl_->writingMode_;
}

void ArtifactTextLayer::setRubyText(const QString &text) {
  impl_->rubyText_ = text;
  markDirty();
}

QString ArtifactTextLayer::rubyText() const {
  return impl_->rubyText_;
}

void ArtifactTextLayer::setRubyScale(float scale) {
  impl_->rubyScale_ = std::clamp(scale, 0.1f, 1.0f);
  markDirty();
}

float ArtifactTextLayer::rubyScale() const {
  return impl_->rubyScale_;
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

void ArtifactTextLayer::setPathSegments(const std::vector<ArtifactCore::BezierSegment>& segments) {
  impl_->pathSegments_ = segments;
  if (impl_->layoutMode_ != TextLayoutMode::Path && !segments.empty()) {
    impl_->layoutMode_ = TextLayoutMode::Path;
  }
  markDirty();
}

std::vector<ArtifactCore::BezierSegment> ArtifactTextLayer::pathSegments() const {
  return impl_->pathSegments_;
}

void ArtifactTextLayer::setPathStartOffset(double offset) {
  if (!impl_->paragraphStyle_.pathBinding) impl_->paragraphStyle_.pathBinding.emplace();
  impl_->paragraphStyle_.pathBinding->startOffset = offset;
  markDirty();
}

double ArtifactTextLayer::pathStartOffset() const {
  return impl_->paragraphStyle_.pathBinding ? impl_->paragraphStyle_.pathBinding->startOffset : 0.0;
}

void ArtifactTextLayer::setPathEndOffset(double offset) {
  if (!impl_->paragraphStyle_.pathBinding) impl_->paragraphStyle_.pathBinding.emplace();
  impl_->paragraphStyle_.pathBinding->endOffset = offset;
  markDirty();
}

double ArtifactTextLayer::pathEndOffset() const {
  return impl_->paragraphStyle_.pathBinding ? impl_->paragraphStyle_.pathBinding->endOffset : 0.0;
}

void ArtifactTextLayer::setPathAlignToPath(bool align) {
  if (!impl_->paragraphStyle_.pathBinding) impl_->paragraphStyle_.pathBinding.emplace();
  impl_->paragraphStyle_.pathBinding->alignToPath = align;
  markDirty();
}

bool ArtifactTextLayer::pathAlignToPath() const {
  return impl_->paragraphStyle_.pathBinding ? impl_->paragraphStyle_.pathBinding->alignToPath : true;
}

void ArtifactTextLayer::setPathReverse(bool reverse) {
  if (!impl_->paragraphStyle_.pathBinding) impl_->paragraphStyle_.pathBinding.emplace();
  impl_->paragraphStyle_.pathBinding->reversePath = reverse;
  markDirty();
}

bool ArtifactTextLayer::pathReverse() const {
  return impl_->paragraphStyle_.pathBinding ? impl_->paragraphStyle_.pathBinding->reversePath : false;
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

int ArtifactTextLayer::applyColorToSelectorRange(
    const int charStart, const int charEnd,
    const ArtifactCore::FloatRGBA& color) {
  const int textLen = text().length();
  if (textLen <= 0) return -1;
  const int start = std::clamp(charStart, 0, textLen);
  const int end = std::clamp(charEnd, start + 1, textLen);
  if (end - start < 1) return -1;

  TextAnimatorState animator = defaultTextAnimatorState(animatorCount());
  animator.name = QStringLiteral("Color %1-%2").arg(start).arg(end);
  animator.range.start = (static_cast<float>(start) / textLen) * 100.0f;
  animator.range.end = (static_cast<float>(end) / textLen) * 100.0f;
  animator.range.units = SelectorUnits::Percentage;
  animator.properties.colorEnabled = true;
  animator.properties.fillColor = FloatRGBA(color.r(), color.g(), color.b(), color.a());

  impl_->animators_.push_back(std::move(animator));
  setDirty(LayerDirtyFlag::Property);
  markDirty();
  Q_EMIT changed();

  return static_cast<int>(impl_->animators_.size()) - 1;
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
  obj["text.writingMode"] = static_cast<int>(writingMode());
  obj["text.rubyText"] = rubyText();
  obj["text.rubyScale"] = rubyScale();
  obj["text.layoutMode"] = static_cast<int>(layoutMode());
  obj["text.maxWidth"] = maxWidth();
  obj["text.boxHeight"] = boxHeight();
  obj["text.paragraphSpacing"] = paragraphSpacing();

  obj["text.pathStartOffset"] = pathStartOffset();
  obj["text.pathEndOffset"] = pathEndOffset();
  obj["text.pathReverse"] = pathReverse();
  obj["text.pathAlignToPath"] = pathAlignToPath();

  QJsonArray pathSegmentsArray;
  for (const auto &seg : impl_->pathSegments_) {
    QJsonObject segObj;
    segObj["p0x"] = seg.p0.x();
    segObj["p0y"] = seg.p0.y();
    segObj["cp1x"] = seg.cp1.x();
    segObj["cp1y"] = seg.cp1.y();
    segObj["cp2x"] = seg.cp2.x();
    segObj["cp2y"] = seg.cp2.y();
    segObj["p1x"] = seg.p1.x();
    segObj["p1y"] = seg.p1.y();
    pathSegmentsArray.append(segObj);
  }
  obj["text.pathSegments"] = pathSegmentsArray;

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
  if (obj.contains("text.writingMode")) {
    setWritingMode(static_cast<TextWritingMode>(
        obj.value("text.writingMode")
            .toInt(static_cast<int>(writingMode()))));
  }
  if (obj.contains("text.rubyText")) {
    setRubyText(obj.value("text.rubyText").toString());
  }
  if (obj.contains("text.rubyScale")) {
    setRubyScale(static_cast<float>(obj.value("text.rubyScale").toDouble(rubyScale())));
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

  if (obj.contains("text.pathStartOffset")) {
    setPathStartOffset(obj.value("text.pathStartOffset").toDouble());
  }
  if (obj.contains("text.pathEndOffset")) {
    setPathEndOffset(obj.value("text.pathEndOffset").toDouble());
  }
  if (obj.contains("text.pathReverse")) {
    setPathReverse(obj.value("text.pathReverse").toBool());
  }
  if (obj.contains("text.pathAlignToPath")) {
    setPathAlignToPath(obj.value("text.pathAlignToPath").toBool());
  }
  if (obj.contains("text.pathSegments") && obj.value("text.pathSegments").isArray()) {
    const QJsonArray pathSegmentsArray = obj.value("text.pathSegments").toArray();
    std::vector<ArtifactCore::BezierSegment> segments;
    segments.reserve(pathSegmentsArray.size());
    for (int i = 0; i < pathSegmentsArray.size(); ++i) {
      const QJsonObject segObj = pathSegmentsArray.at(i).toObject();
      segments.emplace_back(
          QPointF(segObj.value("p0x").toDouble(), segObj.value("p0y").toDouble()),
          QPointF(segObj.value("cp1x").toDouble(), segObj.value("cp1y").toDouble()),
          QPointF(segObj.value("cp2x").toDouble(), segObj.value("cp2y").toDouble()),
          QPointF(segObj.value("p1x").toDouble(), segObj.value("p1y").toDouble()));
    }
    impl_->pathSegments_ = segments;
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

  const auto current = sourceSize();
  if (current.width <= 0 || current.height <= 0) {
    setSourceSize(Size_2D(std::max(1, current.width),
                          std::max(1, current.height)));
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

QString ArtifactTextLayer::debugState() const {
  const QSize imageSize = impl_ && !impl_->renderedImage_.isNull()
                              ? impl_->renderedImage_.size()
                              : QSize();
  const QSize bufferSize = impl_ && impl_->renderedBuffer_
                               ? QSize(impl_->renderedBuffer_->width(),
                                       impl_->renderedBuffer_->height())
                               : QSize();
  return QStringLiteral("textLen=%1 animators=%2 dirty=%3 layout=%4 box=%5x%6 font=%7/%8 hasImage=%9 image=%10 hasBuffer=%11 buffer=%12 bounds={%13} selectorOverview=%14")
      .arg(impl_ ? impl_->text_.toQString().size() : 0)
      .arg(animatorCount())
      .arg(impl_ && impl_->isDirty_ ? QStringLiteral("true") : QStringLiteral("false"))
      .arg(static_cast<int>(layoutMode()))
      .arg(maxWidth(), 0, 'f', 1)
      .arg(boxHeight(), 0, 'f', 1)
      .arg(fontFamily().toQString())
      .arg(fontSize(), 0, 'f', 1)
      .arg(impl_ && !impl_->renderedImage_.isNull() ? QStringLiteral("true") : QStringLiteral("false"))
      .arg(imageSize.isValid() ? QStringLiteral("%1x%2").arg(imageSize.width()).arg(imageSize.height())
                               : QStringLiteral("0x0"))
      .arg(impl_ && impl_->renderedBuffer_ && !impl_->renderedBuffer_->isEmpty()
               ? QStringLiteral("true")
               : QStringLiteral("false"))
      .arg(bufferSize.isValid() ? QStringLiteral("%1x%2").arg(bufferSize.width()).arg(bufferSize.height())
                                : QStringLiteral("0x0"))
      .arg(contentBoundsSummary())
      .arg(selectorOverviewSummary());
}

QVector<float> ArtifactTextLayer::selectorWeightPreview(const int sampleCount) const {
  if (!impl_) {
    return {};
  }
  if (!impl_->glyphs_.empty()) {
    return selectorWeightPreviewForGlyphs(impl_->glyphs_, impl_->animators_,
                                         sampleCount);
  }
  const QString displayText = resolvedSourceTextAtTime(this);
  const int textLength = std::max(1, displayText.size());
  return selectorWeightPreviewForAnimators(impl_->animators_, sampleCount,
                                           textLength);
}

QVector<float> ArtifactTextLayer::selectorClusterBoundaryPreview() const {
  if (!impl_) {
    return {};
  }
  return selectorBoundaryPreviewForGlyphs(impl_->glyphs_, false);
}

QVector<float> ArtifactTextLayer::selectorLineBoundaryPreview() const {
  if (!impl_) {
    return {};
  }
  return selectorBoundaryPreviewForGlyphs(impl_->glyphs_, true);
}

QString ArtifactTextLayer::selectorDebugSummary() const {
  if (!impl_) {
    return QStringLiteral("glyph");
  }
  const QString displayText = resolvedSourceTextAtTime(this);
  const bool hasAnimators =
      std::any_of(impl_->animators_.begin(), impl_->animators_.end(),
                  [](const TextAnimatorState &animator) {
                    return animator.enabled;
                  });
  const bool hasRuby = !impl_->rubyText_.isEmpty();
  const bool hasCjk = FontManager::containsCjkCharacters(displayText);
  const bool isRichText = Qt::mightBeRichText(displayText);
  const bool lineAware = displayText.contains(QChar::LineFeed) ||
                         displayText.contains(QChar::CarriageReturn) ||
                         impl_->layoutMode_ == TextLayoutMode::Box;
  const bool hasRegexSelector =
      std::any_of(impl_->animators_.begin(), impl_->animators_.end(),
                  [](const TextAnimatorState &animator) {
                    return animator.range.regexEnabled &&
                           !animator.range.selectorPattern.isEmpty();
                  });
  return textSelectionTargetLabelForState(hasAnimators, hasRuby, hasCjk,
                                          isRichText, impl_->writingMode_,
                                          lineAware, hasRegexSelector);
}

QString ArtifactTextLayer::selectorBoundarySummary() const {
  if (!impl_) {
    return QStringLiteral("clusters=0 lines=0");
  }
  const int clusterCount = selectorClusterBoundaryPreview().size();
  const int lineCount = selectorLineBoundaryPreview().size();
  return QStringLiteral("clusters=%1 lines=%2")
      .arg(clusterCount)
      .arg(lineCount);
}

QString ArtifactTextLayer::selectorOverviewSummary() const {
  if (!impl_) {
    return QStringLiteral(
        "target=glyph;mode=horizontal;source=logical;visual=flow;unit=glyph;regex=off;tag=unknown;token=none;scripts=0;vertical=tcy=0;punct=0;brackets=0;kinsoku=0;clusters=0;lines=0");
  }
  const QString displayText = resolvedSourceTextAtTime(this);
  const bool hasAnimators =
      std::any_of(impl_->animators_.begin(), impl_->animators_.end(),
                  [](const TextAnimatorState &animator) {
                    return animator.enabled;
                  });
  const bool hasRegexSelector =
      std::any_of(impl_->animators_.begin(), impl_->animators_.end(),
                  [](const TextAnimatorState &animator) {
                    return animator.range.regexEnabled &&
                           !animator.range.selectorPattern.isEmpty();
                  });
  const bool hasRuby = !impl_->rubyText_.isEmpty();
  const bool hasCjk = FontManager::containsCjkCharacters(displayText);
  const bool isRichText = Qt::mightBeRichText(displayText);
  const bool lineAware = displayText.contains(QChar::LineFeed) ||
                         displayText.contains(QChar::CarriageReturn) ||
                         impl_->layoutMode_ == TextLayoutMode::Box;
  const QString tokenSummary = selectorTokenLabelForGlyphs(impl_->glyphs_);
  const QString tagSummary = selectorTagLabelForText(displayText);
  const QString scriptSummary = selectorScriptLabelForContract(impl_->layoutContract_);
  const QString verticalSummary = selectorVerticalLabelForContract(impl_->layoutContract_);
  return QStringLiteral("target=%1;%2;%3;%4;regex=%5;%6;%7;%8;vertical=%9;clusters=%10;lines=%11")
      .arg(selectorDebugSummary(), textWritingModeLabel(impl_->writingMode_),
           textOrderingLabel(impl_->writingMode_),
           textSelectionUnitLabelForState(hasAnimators, hasRuby, hasCjk,
                                          isRichText, impl_->writingMode_,
                                          lineAware, hasRegexSelector),
           hasRegexSelector ? QStringLiteral("on") : QStringLiteral("off"),
           tagSummary,
           tokenSummary,
           scriptSummary,
           verticalSummary,
           selectorClusterBoundaryPreview().size(),
           selectorLineBoundaryPreview().size());
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
  QString displayText = resolvedSourceTextAtTime(this);
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
                                    impl_->paragraphStyle_,
                                    impl_->writingMode_, impl_->rubyText_,
                                    impl_->rubyScale_, impl_->layoutMode_,
                                    impl_->pathSegments_};
    if (impl_->isDirty_ || !impl_->lastCacheKey_ ||
        *impl_->lastCacheKey_ != currentKey || sourceSize().width <= 0 ||
        sourceSize().height <= 0) {
      const TextShapingResult shaped = layoutTextShape(
          UniString(displayText), impl_->textStyle_, impl_->paragraphStyle_,
          impl_->writingMode_,
          buildRubyAttachments(UniString(displayText), impl_->rubyText_,
                               impl_->rubyScale_),
          impl_->layoutMode_, impl_->pathSegments_);
      impl_->glyphs_ = shaped.glyphs;
      impl_->layoutContract_ = shaped.contract;
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

  if (impl_->isDirty_ || impl_->renderedImage_.isNull() ||
      sourceTextIsAnimated(this)) {
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
  if (const auto textProp = getProperty(QStringLiteral("text.value"))) {
    textProp->setDisplayLabel(QStringLiteral("Source Text"));
    textProp->setTooltip(
        QStringLiteral("Animate this text string over time."));
    textProp->setAnimatable(true);
  }
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

  auto writingModeProp = makeProp(QStringLiteral("text.writingMode"),
                                  ArtifactCore::PropertyType::Integer,
                                  static_cast<int>(writingMode()), -85);
  writingModeProp->setTooltip(
      QStringLiteral("0=Horizontal, 1=Vertical"));
  textGroup.addProperty(writingModeProp);

  const QString badgeSourceText = resolvedSourceTextAtTime(this);
  const bool hasAnimators =
      std::any_of(impl_->animators_.begin(), impl_->animators_.end(),
                  [](const TextAnimatorState &animator) {
                    return animator.enabled;
                  });
  const bool hasRuby = !impl_->rubyText_.isEmpty();
  const bool hasCjk = FontManager::containsCjkCharacters(badgeSourceText);
  const bool isRichText = Qt::mightBeRichText(badgeSourceText);
  const bool lineAware = badgeSourceText.contains(QChar::LineFeed) ||
                         badgeSourceText.contains(QChar::CarriageReturn) ||
                         impl_->layoutMode_ == TextLayoutMode::Box;
  const bool hasRegexSelector =
      std::any_of(impl_->animators_.begin(), impl_->animators_.end(),
                  [](const TextAnimatorState &animator) {
                    return animator.range.regexEnabled &&
                           !animator.range.selectorPattern.isEmpty();
                  });
  const QString selectionTarget = textSelectionTargetForState(
      hasAnimators, hasRuby, hasCjk, isRichText, impl_->writingMode_,
      lineAware, hasRegexSelector);
  const QString badgeText = textUnitBadgeForState(
      badgeSourceText, impl_->writingMode_, impl_->layoutMode_,
      hasAnimators, hasRuby, hasCjk, isRichText);
  auto selectionTargetProp = makeProp(QStringLiteral("text.selectionTarget"),
                                      ArtifactCore::PropertyType::String,
                                      selectionTarget, -84);
  selectionTargetProp->setDisplayLabel(QStringLiteral("Selection Target"));
  selectionTargetProp->setTooltip(
      QStringLiteral("Detail used by Selector Overview: glyph, cluster, line-aware, or tag-aware."));
  textGroup.addProperty(selectionTargetProp);
  auto selectorOverviewProp = makeProp(QStringLiteral("text.selectorOverview"),
                                       ArtifactCore::PropertyType::String,
                                       selectorOverviewSummary(), -83);
  selectorOverviewProp->setDisplayLabel(QStringLiteral("Selector Overview"));
  selectorOverviewProp->setTooltip(
      QStringLiteral("Primary compact key=value summary for target, mode, source, visual, unit, tag, script, vertical, token, clusters, and lines."));
  textGroup.addProperty(selectorOverviewProp);
  auto selectorScriptProp = makeProp(QStringLiteral("text.selectorScript"),
                                     ArtifactCore::PropertyType::String,
                                     selectorScriptLabelForContract(impl_->layoutContract_),
                                     -82);
  selectorScriptProp->setDisplayLabel(QStringLiteral("Selector Script"));
  selectorScriptProp->setTooltip(
      QStringLiteral("Script run summary from the shaped layout contract."));
  textGroup.addProperty(selectorScriptProp);
  auto selectorVerticalProp = makeProp(
      QStringLiteral("text.selectorVertical"), ArtifactCore::PropertyType::String,
      selectorVerticalLabelForContract(impl_->layoutContract_), -81);
  selectorVerticalProp->setDisplayLabel(QStringLiteral("Selector Vertical"));
  selectorVerticalProp->setTooltip(
      QStringLiteral("Vertical-writing summary from the shaped layout contract."));
  textGroup.addProperty(selectorVerticalProp);
  auto selectorTokenProp = makeProp(QStringLiteral("text.selectorToken"),
                                   ArtifactCore::PropertyType::String,
                                   selectorTokenLabelForGlyphs(impl_->glyphs_),
                                   -80);
  selectorTokenProp->setDisplayLabel(QStringLiteral("Selector Token"));
  selectorTokenProp->setTooltip(
      QStringLiteral("Representative stable token id for the current shaped glyph stream."));
  textGroup.addProperty(selectorTokenProp);
  auto selectorTagProp = makeProp(QStringLiteral("text.selectorTag"),
                                  ArtifactCore::PropertyType::String,
                                  selectorTagLabelForText(badgeSourceText),
                                  -79);
  selectorTagProp->setDisplayLabel(QStringLiteral("Selector Tag"));
  selectorTagProp->setTooltip(
      QStringLiteral("Representative script-family tag for the current text stream."));
  textGroup.addProperty(selectorTagProp);
  auto unitBadgeProp = makeProp(QStringLiteral("text.unitBadge"),
                                ArtifactCore::PropertyType::String,
                                badgeText, -78);
  unitBadgeProp->setDisplayLabel(QStringLiteral("Unit Badge"));
  unitBadgeProp->setTooltip(
      QStringLiteral("Detail used by Selector Overview: current text unit semantics."));
  textGroup.addProperty(unitBadgeProp);

  textGroup.addProperty(makeProp(QStringLiteral("text.rubyText"),
                                 ArtifactCore::PropertyType::String,
                                 rubyText(), -78));
  auto rubyScaleProp =
      makeProp(QStringLiteral("text.rubyScale"),
               ArtifactCore::PropertyType::Float, rubyScale(), -77);
  rubyScaleProp->setHardRange(0.1, 1.0);
  rubyScaleProp->setSoftRange(0.25, 0.8);
  rubyScaleProp->setStep(0.05);
  rubyScaleProp->setTooltip(QStringLiteral("0.1-1.0"));
  textGroup.addProperty(rubyScaleProp);

  auto layoutModeProp = makeProp(QStringLiteral("text.layoutMode"),
                                 ArtifactCore::PropertyType::Integer,
                                 static_cast<int>(layoutMode()), -76);
  layoutModeProp->setTooltip(
      QStringLiteral("0=Point text, 1=Box text"));
  textGroup.addProperty(layoutModeProp);

  auto maxWidthProp =
      makeProp(QStringLiteral("text.maxWidth"),
               ArtifactCore::PropertyType::Float, maxWidth(), -75);
  maxWidthProp->setHardRange(0.0, 100000.0);
  maxWidthProp->setSoftRange(0.0, 1920.0);
  maxWidthProp->setStep(1.0);
  maxWidthProp->setTooltip(
      QStringLiteral("0 = Auto width, > 0 = fixed wrap width"));
  textGroup.addProperty(maxWidthProp);

  auto boxHeightProp =
      makeProp(QStringLiteral("text.boxHeight"),
               ArtifactCore::PropertyType::Float, boxHeight(), -74);
  boxHeightProp->setHardRange(0.0, 100000.0);
  boxHeightProp->setSoftRange(0.0, 1080.0);
  boxHeightProp->setStep(1.0);
  boxHeightProp->setTooltip(
      QStringLiteral("0 = Auto height, > 0 = fixed box height"));
  textGroup.addProperty(boxHeightProp);

  auto paragraphSpacingProp =
      makeProp(QStringLiteral("text.paragraphSpacing"),
               ArtifactCore::PropertyType::Float, paragraphSpacing(), -73);
  paragraphSpacingProp->setHardRange(0.0, 1000.0);
  paragraphSpacingProp->setSoftRange(0.0, 80.0);
  paragraphSpacingProp->setStep(0.5);
  textGroup.addProperty(paragraphSpacingProp);

  const auto c = textColor();
  auto colorProp = persistentLayerProperty(QStringLiteral("text.color"),
                                           ArtifactCore::PropertyType::Color,
                                           QVariant(), -72);
  colorProp->setColorValue(QColor::fromRgbF(c.r(), c.g(), c.b(), c.a()));
  colorProp->setValue(colorProp->getColorValue());
  textGroup.addProperty(colorProp);

  // Stroke
  textGroup.addProperty(makeProp(QStringLiteral("text.strokeEnabled"),
                                 ArtifactCore::PropertyType::Boolean,
                                 isStrokeEnabled(), -71));
  const auto sc = strokeColor();
  auto strokeColorProp = persistentLayerProperty(
      QStringLiteral("text.strokeColor"), ArtifactCore::PropertyType::Color,
      QVariant(), -70);
  strokeColorProp->setColorValue(
      QColor::fromRgbF(sc.r(), sc.g(), sc.b(), sc.a()));
  strokeColorProp->setValue(strokeColorProp->getColorValue());
  textGroup.addProperty(strokeColorProp);
  textGroup.addProperty(makeProp(QStringLiteral("text.strokeWidth"),
                                 ArtifactCore::PropertyType::Float,
                                 strokeWidth(), -69));

  // Shadow
  textGroup.addProperty(makeProp(QStringLiteral("text.shadowEnabled"),
                                 ArtifactCore::PropertyType::Boolean,
                                 isShadowEnabled(), -68));
  const auto shc = shadowColor();
  auto shadowColorProp = persistentLayerProperty(
      QStringLiteral("text.shadowColor"), ArtifactCore::PropertyType::Color,
      QVariant(), -67);
  shadowColorProp->setColorValue(
      QColor::fromRgbF(shc.r(), shc.g(), shc.b(), shc.a()));
  shadowColorProp->setValue(shadowColorProp->getColorValue());
  textGroup.addProperty(shadowColorProp);
  textGroup.addProperty(makeProp(QStringLiteral("text.shadowOffsetX"),
                                 ArtifactCore::PropertyType::Float,
                                 shadowOffsetX(), -66));
  textGroup.addProperty(makeProp(QStringLiteral("text.shadowOffsetY"),
                                 ArtifactCore::PropertyType::Float,
                                 shadowOffsetY(), -65));
  textGroup.addProperty(makeProp(QStringLiteral("text.shadowBlur"),
                                 ArtifactCore::PropertyType::Float,
                                 shadowBlur(), -64));
  auto animatorCountProp =
      makeProp(QStringLiteral("text.animatorCount"),
               ArtifactCore::PropertyType::Integer, animatorCount(), -63);
  animatorCountProp->setHardRange(0, 16);
  animatorCountProp->setSoftRange(0, 8);
  animatorCountProp->setStep(1);
  animatorCountProp->setTooltip(
      QStringLiteral("Increase to add text animators. Decrease to remove from the end."));
  textGroup.addProperty(animatorCountProp);

  auto animatorPresetProp =
      makeProp(QStringLiteral("text.animatorPreset"),
               ArtifactCore::PropertyType::Integer, 0, -62);
  animatorPresetProp->setDisplayLabel(QStringLiteral("Preset"));
  animatorPresetProp->setTooltip(animatorPresetTooltip());
  animatorPresetProp->setValue(inferTextAnimatorPresetId(impl_->animators_));
  textGroup.addProperty(animatorPresetProp);

  groups.push_back(textGroup);

  // Path Options
  ArtifactCore::PropertyGroup pathGroup(QStringLiteral("Path Options"));
  pathGroup.addProperty(makeProp(QStringLiteral("text.pathStartOffset"),
                                 ArtifactCore::PropertyType::Float,
                                 pathStartOffset(), -70));
  pathGroup.addProperty(makeProp(QStringLiteral("text.pathEndOffset"),
                                 ArtifactCore::PropertyType::Float,
                                 pathEndOffset(), -69));
  pathGroup.addProperty(makeProp(QStringLiteral("text.pathReverse"),
                                 ArtifactCore::PropertyType::Boolean,
                                 pathReverse(), -68));
  pathGroup.addProperty(makeProp(QStringLiteral("text.pathAlignToPath"),
                                 ArtifactCore::PropertyType::Boolean,
                                 pathAlignToPath(), -67));
  groups.push_back(pathGroup);

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
      auto property = persistentLayerProperty(prefix + suffix, type, value, priority);
      if (property && isAnimatorPropertyAnimatable(suffix)) {
        property->setAnimatable(true);
      }
      return property;
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

    auto regexEnabledProp =
        makeAnimatorProp(QStringLiteral("regexEnabled"),
                         ArtifactCore::PropertyType::Boolean,
                         animator.range.regexEnabled, -113);
    regexEnabledProp->setDisplayLabel(QStringLiteral("Regex"));
    regexEnabledProp->setTooltip(
        QStringLiteral("Enable regular-expression filtering against cluster id, tag, and glyph index."));
    animatorGroup.addProperty(regexEnabledProp);

    auto selectorPatternProp = makeAnimatorProp(
        QStringLiteral("selectorPattern"), ArtifactCore::PropertyType::String,
        animator.range.selectorPattern, -112);
    selectorPatternProp->setDisplayLabel(QStringLiteral("Pattern"));
    selectorPatternProp->setTooltip(
        QStringLiteral("Pattern matched against cluster id, tag, and glyph index when Regex is on."));
    animatorGroup.addProperty(selectorPatternProp);

    auto wigglyEnabledProp =
        makeAnimatorProp(QStringLiteral("wigglyEnabled"),
                         ArtifactCore::PropertyType::Boolean,
                         animator.wiggly.enabled, -111);
    wigglyEnabledProp->setDisplayLabel(QStringLiteral("Wiggly"));
    animatorGroup.addProperty(wigglyEnabledProp);

    auto wpsProp = makeAnimatorProp(QStringLiteral("wigglesPerSecond"),
                                    ArtifactCore::PropertyType::Float,
                                    animator.wiggly.wigglesPerSecond, -110);
    wpsProp->setDisplayLabel(QStringLiteral("Wiggles/Sec"));
    wpsProp->setSoftRange(0.0, 20.0);
    animatorGroup.addProperty(wpsProp);

    auto correlationProp =
        makeAnimatorProp(QStringLiteral("correlation"),
                         ArtifactCore::PropertyType::Float,
                         animator.wiggly.correlation, -109);
    correlationProp->setDisplayLabel(QStringLiteral("Correlation"));
    correlationProp->setHardRange(0.0, 100.0);
    correlationProp->setSoftRange(0.0, 100.0);
    animatorGroup.addProperty(correlationProp);

    auto phaseProp = makeAnimatorProp(QStringLiteral("phase"),
                                      ArtifactCore::PropertyType::Float,
                                      animator.wiggly.phase, -108);
    phaseProp->setDisplayLabel(QStringLiteral("Phase"));
    animatorGroup.addProperty(phaseProp);

    auto seedProp = makeAnimatorProp(QStringLiteral("seed"),
                                     ArtifactCore::PropertyType::Integer,
                                     animator.wiggly.seed, -107);
    seedProp->setDisplayLabel(QStringLiteral("Seed"));
    animatorGroup.addProperty(seedProp);

    auto posXProp = makeAnimatorProp(QStringLiteral("positionX"),
                                     ArtifactCore::PropertyType::Float,
                                     animator.properties.position.x(), -106);
    posXProp->setDisplayLabel(QStringLiteral("Position X"));
    posXProp->setSoftRange(-500.0, 500.0);
    animatorGroup.addProperty(posXProp);

    auto posYProp = makeAnimatorProp(QStringLiteral("positionY"),
                                     ArtifactCore::PropertyType::Float,
                                     animator.properties.position.y(), -105);
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

    auto zProp = makeAnimatorProp(QStringLiteral("z"),
                                  ArtifactCore::PropertyType::Float,
                                  animator.properties.z, -101);
    zProp->setDisplayLabel(QStringLiteral("Z"));
    zProp->setSoftRange(-1000.0, 1000.0);
    animatorGroup.addProperty(zProp);

    auto colorEnabledProp =
        makeAnimatorProp(QStringLiteral("colorEnabled"),
                         ArtifactCore::PropertyType::Boolean,
                         animator.properties.colorEnabled, -100);
    colorEnabledProp->setDisplayLabel(QStringLiteral("Fill Override"));
    animatorGroup.addProperty(colorEnabledProp);

    auto fillColorProp =
        persistentLayerProperty(prefix + QStringLiteral("fillColor"),
                                ArtifactCore::PropertyType::Color, QVariant(),
                                -99);
    fillColorProp->setDisplayLabel(QStringLiteral("Fill Color"));
    fillColorProp->setColorValue(toQColor(animator.properties.fillColor));
    fillColorProp->setValue(fillColorProp->getColorValue());
    animatorGroup.addProperty(fillColorProp);

    auto strokeEnabledProp =
        makeAnimatorProp(QStringLiteral("strokeEnabled"),
                         ArtifactCore::PropertyType::Boolean,
                         animator.properties.strokeEnabled, -98);
    strokeEnabledProp->setDisplayLabel(QStringLiteral("Stroke Override"));
    animatorGroup.addProperty(strokeEnabledProp);

    auto strokeColorProp =
        persistentLayerProperty(prefix + QStringLiteral("strokeColor"),
                                ArtifactCore::PropertyType::Color, QVariant(),
                                -97);
    strokeColorProp->setDisplayLabel(QStringLiteral("Stroke Color"));
    strokeColorProp->setColorValue(toQColor(animator.properties.strokeColor));
    strokeColorProp->setValue(strokeColorProp->getColorValue());
    animatorGroup.addProperty(strokeColorProp);

    auto strokeWidthProp = makeAnimatorProp(QStringLiteral("strokeWidth"),
                                            ArtifactCore::PropertyType::Float,
                                            animator.properties.strokeWidth,
                                            -96);
    strokeWidthProp->setDisplayLabel(QStringLiteral("Stroke Width"));
    strokeWidthProp->setSoftRange(0.0, 100.0);
    animatorGroup.addProperty(strokeWidthProp);

    auto blurProp = makeAnimatorProp(QStringLiteral("blur"),
                                     ArtifactCore::PropertyType::Float,
                                     animator.properties.blur, -95);
    blurProp->setDisplayLabel(QStringLiteral("Blur"));
    blurProp->setSoftRange(0.0, 128.0);
    animatorGroup.addProperty(blurProp);

    groups.push_back(animatorGroup);
  }
  return groups;
}

bool ArtifactTextLayer::setLayerPropertyValue(const QString &propertyPath,
                                              const QVariant &value) {
  auto clearPresetSelection = [this]() {
    if (const auto presetProperty =
            getProperty(QStringLiteral("text.animatorPreset"))) {
      presetProperty->setValue(0);
    }
  };

  if (propertyPath == QStringLiteral("source.width")) {
    const auto current = sourceSize();
    const int width = std::max(1, value.toInt());
    if (current.width == width) {
      return true;
    }
    setSourceSize(Size_2D(width, std::max(1, current.height)));
    setDirty(LayerDirtyFlag::Source);
    addDirtyReason(LayerDirtyReason::PropertyChanged);
    markDirty();
    Q_EMIT changed();
    return true;
  }
  if (propertyPath == QStringLiteral("source.height")) {
    const auto current = sourceSize();
    const int height = std::max(1, value.toInt());
    if (current.height == height) {
      return true;
    }
    setSourceSize(Size_2D(std::max(1, current.width), height));
    setDirty(LayerDirtyFlag::Source);
    addDirtyReason(LayerDirtyReason::PropertyChanged);
    markDirty();
    Q_EMIT changed();
    return true;
  }

  if (propertyPath == QStringLiteral("text.value")) {
    setText(UniString(value.toString()));
    setDirty(LayerDirtyFlag::Property);
    addDirtyReason(LayerDirtyReason::PropertyChanged);
    markDirty();
    Q_EMIT changed();
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
  if (propertyPath == QStringLiteral("text.writingMode")) {
    setWritingMode(static_cast<TextWritingMode>(value.toInt()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.rubyText")) {
    setRubyText(value.toString());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.rubyScale")) {
    setRubyScale(static_cast<float>(value.toDouble()));
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.layoutMode")) {
    bool ok = false;
    const int layoutModeValue = value.toInt(&ok);
    setLayoutMode(static_cast<TextLayoutMode>(ok ? layoutModeValue
                                                 : static_cast<int>(layoutMode())));
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
  if (propertyPath == QStringLiteral("text.pathStartOffset")) {
    setPathStartOffset(value.toDouble());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.pathEndOffset")) {
    setPathEndOffset(value.toDouble());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.pathReverse")) {
    setPathReverse(value.toBool());
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.pathAlignToPath")) {
    setPathAlignToPath(value.toBool());
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
    const int val = value.toInt();
    if (val >= 100) {
      const int presetId = val / 100;
      if (presetId >= 1 && presetId <= 7) {
        const auto presetAnimators = buildTextAnimatorPreset(presetId);
        if (!presetAnimators.empty()) {
          impl_->animators_.push_back(presetAnimators.front());
          impl_->animators_.back().name = QStringLiteral("%1 %2")
              .arg(presetAnimators.front().name)
              .arg(animatorCount());
        }
      } else {
        addAnimator();
      }
    } else {
      setAnimatorCount(val);
    }
    clearPresetSelection();
    setDirty(LayerDirtyFlag::Property);
    return true;
  }
  if (propertyPath == QStringLiteral("text.animatorPreset")) {
    const int presetId = value.toInt();
    if (presetId == 0) {
      return true;
    }
    const auto animators = buildTextAnimatorPreset(presetId);
    if (animators.empty()) {
      return false;
    }
    impl_->animators_ = animators;
    markDirty();
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
    } else if (field == QStringLiteral("regexEnabled")) {
      animator.range.regexEnabled = value.toBool();
    } else if (field == QStringLiteral("selectorPattern")) {
      animator.range.selectorPattern = value.toString();
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
    } else if (field == QStringLiteral("strokeEnabled")) {
      animator.properties.strokeEnabled = value.toBool();
    } else if (field == QStringLiteral("strokeColor")) {
      const QColor color = value.value<QColor>();
      animator.properties.strokeColor = toFloatRGBA(color);
      animator.properties.strokeEnabled = true;
    } else if (field == QStringLiteral("strokeWidth")) {
      animator.properties.strokeWidth =
          std::max(0.0f, static_cast<float>(value.toDouble()));
    } else if (field == QStringLiteral("blur")) {
      animator.properties.blur = std::max(0.0f, static_cast<float>(value.toDouble()));
    } else if (field == QStringLiteral("z")) {
      animator.properties.z = static_cast<float>(value.toDouble());
    } else {
      handled = false;
    }

    if (handled) {
      clearPresetSelection();
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
  const bool pathLayout = impl_->layoutMode_ == TextLayoutMode::Path;
  QString displayText = resolvedSourceTextAtTime(this);
  const bool sourceTextAnimated = sourceTextIsAnimated(this);
  Impl::CacheKey currentKey{UniString(displayText), impl_->textStyle_,
                            impl_->paragraphStyle_, impl_->writingMode_,
                            impl_->rubyText_, impl_->rubyScale_,
                            impl_->layoutMode_, impl_->pathSegments_};
  if (!hasAnimators && !sourceTextAnimated && !impl_->isDirty_ && impl_->lastCacheKey_ &&
      *impl_->lastCacheKey_ == currentKey && !impl_->renderedImage_.isNull()) {
    return;
  }
  impl_->lastCacheKey_ = currentKey;

  const bool isRichText = Qt::mightBeRichText(displayText);
  if (impl_->textStyle_.allCaps && !isRichText) {
    displayText = displayText.toUpper();
  }
  if (displayText.isEmpty()) {
    displayText = QStringLiteral(" ");
  }

  impl_->glyphs_.clear();
  impl_->layoutContract_ = TextLayoutContract{};
  if (!isRichText) {
    const TextShapingResult shaped = layoutTextShape(
        UniString(displayText), impl_->textStyle_, impl_->paragraphStyle_,
        impl_->writingMode_,
        buildRubyAttachments(UniString(displayText), impl_->rubyText_,
                             impl_->rubyScale_),
        impl_->layoutMode_, impl_->pathSegments_);
    impl_->glyphs_ = shaped.glyphs;
    impl_->layoutContract_ = shaped.contract;
  }
  impl_->perGlyphMode_ = (hasAnimators || pathLayout) && !isRichText;

  if (impl_->perGlyphMode_) {
    const RationalTime time = effectiveTextTimelineTime(this);
    const float timeSeconds = static_cast<float>(time.toSeconds());
    std::vector<std::tuple<RangeSelector, WigglySelector, AnimatorProperties>> animatorStack;
    animatorStack.reserve(impl_->animators_.size());
    for (int i = 0; i < static_cast<int>(impl_->animators_.size()); ++i) {
      const auto &animator = impl_->animators_[i];
      if (!animator.enabled) {
        continue;
      }
      auto resolvedAnimator = animator;
      const QString prefix = QStringLiteral("text.animators.%1.").arg(i);
      if (const auto property = getProperty(prefix + QStringLiteral("start"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.range.start = static_cast<float>(value.toDouble());
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("end"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.range.end = static_cast<float>(value.toDouble());
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("offset"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.range.offset = static_cast<float>(value.toDouble());
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("wigglesPerSecond"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.wiggly.wigglesPerSecond =
              static_cast<float>(value.toDouble());
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("correlation"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.wiggly.correlation =
              std::clamp(static_cast<float>(value.toDouble()), 0.0f, 100.0f);
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("phase"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.wiggly.phase = static_cast<float>(value.toDouble());
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("seed"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.wiggly.seed = value.toInt();
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("positionX"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.properties.position.setX(
              static_cast<float>(value.toDouble()));
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("positionY"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.properties.position.setY(
              static_cast<float>(value.toDouble()));
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("scale"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.properties.scale =
              std::max(0.0f, static_cast<float>(value.toDouble()));
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("rotation"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.properties.rotation =
              static_cast<float>(value.toDouble());
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("opacity"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.properties.opacity =
              std::clamp(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("skew"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.properties.skew =
              static_cast<float>(value.toDouble());
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("tracking"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.properties.tracking =
              static_cast<float>(value.toDouble());
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("z"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.properties.z =
              static_cast<float>(value.toDouble());
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("colorEnabled"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.properties.colorEnabled = value.toBool();
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("fillColor"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.canConvert<QColor>()) {
          resolvedAnimator.properties.fillColor =
              toFloatRGBA(value.value<QColor>());
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("strokeEnabled"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.properties.strokeEnabled = value.toBool();
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("strokeColor"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.canConvert<QColor>()) {
          resolvedAnimator.properties.strokeColor =
              toFloatRGBA(value.value<QColor>());
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("strokeWidth"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.properties.strokeWidth =
              std::max(0.0f, static_cast<float>(value.toDouble()));
        }
      }
      if (const auto property = getProperty(prefix + QStringLiteral("blur"));
          property) {
        const QVariant value = property->getKeyFrames().empty()
                                   ? property->getValue()
                                   : property->interpolateValue(time);
        if (value.isValid()) {
          resolvedAnimator.properties.blur =
              std::max(0.0f, static_cast<float>(value.toDouble()));
        }
      }

      animatorStack.emplace_back(resolvedAnimator.range, resolvedAnimator.wiggly,
                                 resolvedAnimator.properties);
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

// -- createMaskFromText --

namespace Artifact {

LayerMask ArtifactTextLayer::createMaskFromText() const
{
    LayerMask mask;

    const QString text = impl_->text_.toQString().trimmed();
    if (text.isEmpty()) {
        mask.setEnabled(false);
        return mask;
    }

    const QFont font = makeTextFont(impl_->textStyle_, text);
    QPainterPath path;
    path.addText(QPointF(0.0, 0.0), font, text);

    auto maskPaths = MaskPath::fromQPainterPath(path, text);
    for (auto& mp : maskPaths) {
        mask.addMaskPath(mp);
    }

    if (mask.maskPathCount() == 0) {
        MaskPath fallback;
        fallback.setName(UniString(text));
        fallback.setMode(MaskMode::Add);
        mask.addMaskPath(fallback);
    }

    return mask;
}

} // namespace Artifact

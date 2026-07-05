// ReSharper disable All
module;
#include <utility>
#include <QJsonDocument>
#include <QList>
#include <qforeach.h>
#include <QHash>
#include <QVector>
#include <QMultiMap>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>
#include <QSet>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <cmath>
#include <vector>

module Artifact.Composition.Abstract;

import std;

import Container;
import Frame.Position;
import Frame.Range;
import Frame.Rate;
import Time.Rational;
import Composition.Context;
import Composition.Settings;
import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Effect.Abstract;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Event.Types;
import Event.Bus;
import Audio.Mixer;
import Audio.Bus;
import Audio.Mixer;
import Audio.Bus;
import Artifact.Layer.Audio;
import Artifact.Layer.Video;
import ArtifactCore.Control.External;

//import Playback.Clock;

namespace Artifact {
 using namespace ArtifactCore;

namespace {

const Artifact::CompositionStateVariant* findStateVariantById(
    const QVector<Artifact::CompositionStateVariant>& states,
    const QString& stateId)
{
    for (const auto& state : states) {
        if (state.stateId == stateId) {
            return &state;
        }
    }
    return nullptr;
}
Artifact::CompositionStateVariant* findStateVariantById(
    QVector<Artifact::CompositionStateVariant>& states,
    const QString& stateId)
{
    for (auto& state : states) {
        if (state.stateId == stateId) {
            return &state;
        }
    }
    return nullptr;
}
QString uniqueStateVariantId(const QVector<Artifact::CompositionStateVariant>& states,
                             QString stateId)
{
    stateId = stateId.trimmed();
    if (stateId.isEmpty()) {
        stateId = QStringLiteral("state");
    }
    QString candidate = stateId;
    int suffix = 2;
    while (findStateVariantById(states, candidate) != nullptr) {
        candidate = QStringLiteral("%1_%2").arg(stateId).arg(suffix++);
    }
    return candidate;
}

// transform 系プロパティの keyframe 値を解像度変更に合わせて再計算する。
// position / anchor は X/Y が別プロパティなので、同時刻の keyframe をペアリングして
// QPointF を組み立ててから ResolutionRemap::remapPosition に渡す。
// CenterLocked 等の policy は X と Y の双方にまたがる平行移動成分を持つため、
// X と Y を独立には remap できない。
// scale は X/Y 別々の倍率値として remapScale で処理する。rotation は角度なので対象外。

// 同じ時間を持つ2つのプロパティの keyframe を時刻で整列させ、(xValue, yValue) のペア列を返す。
// 一方にしか無い時刻は補間値で揃える。
std::vector<std::pair<ArtifactCore::RationalTime, QPointF>>
alignKeyframesByTime(const ArtifactCore::AbstractProperty& xProp,
                     const ArtifactCore::AbstractProperty& yProp)
{
    const auto xKeys = xProp.getKeyFrames();
    const auto yKeys = yProp.getKeyFrames();

    std::vector<std::pair<ArtifactCore::RationalTime, QPointF>> result;
    if (xKeys.empty() && yKeys.empty()) {
        return result;
    }

    // 時刻の和集合を取る（RationalTime は同値比較可能）。
    std::vector<ArtifactCore::RationalTime> times;
    times.reserve(xKeys.size() + yKeys.size());
    for (const auto& k : xKeys) times.push_back(k.time);
    for (const auto& k : yKeys) times.push_back(k.time);
    std::sort(times.begin(), times.end(),
              [](const ArtifactCore::RationalTime& a, const ArtifactCore::RationalTime& b) {
                  return a < b;
              });
    times.erase(std::unique(times.begin(), times.end(),
                            [](const ArtifactCore::RationalTime& a, const ArtifactCore::RationalTime& b) {
                                return !(a < b) && !(b < a);
                            }),
                times.end());

    result.reserve(times.size());
    for (const auto& t : times) {
        const double xv = xProp.interpolateValue(t).toDouble();
        const double yv = yProp.interpolateValue(t).toDouble();
        result.emplace_back(t, QPointF(xv, yv));
    }
    return result;
}

// X/Y 別プロパティをPointF単位で remap し直す。keyframe が無ければ現在値を1回だけ remap する。
void remapPointPropertyPair(ArtifactCore::AbstractProperty& xProp,
                            ArtifactCore::AbstractProperty& yProp,
                            const QSize& oldSize,
                            const QSize& newSize,
                            ArtifactCore::RemapPolicy policy)
{
    const auto xKeys = xProp.getKeyFrames();
    const auto yKeys = yProp.getKeyFrames();

    if (xKeys.empty() && yKeys.empty()) {
        // 非アニメーション: 現在値を1回 remap して setValue する。
        const QPointF remapped = ArtifactCore::ResolutionRemap::remapPosition(
            QPointF(xProp.getValue().toDouble(), yProp.getValue().toDouble()),
            oldSize, newSize, policy);
        xProp.setValue(remapped.x());
        yProp.setValue(remapped.y());
        return;
    }

    // アニメーション: 時刻で整列した (x, y) ペアを remap し直して再設定する。
    // interpolation / bezier handle / roving は元 keyframe のものを可能な限り維持する。
    // X と Y で別々に clear/add するが、時刻と補間情報は整列済みペアから復元する。
    const auto pairs = alignKeyframesByTime(xProp, yProp);
    if (pairs.empty()) {
        return;
    }

    // 元の keyframe メタデータ（補間/ハンドル/roving）を時刻で引けるようにする。
    auto findKey = [](const std::vector<ArtifactCore::KeyFrame>& keys,
                      const ArtifactCore::RationalTime& t) -> const ArtifactCore::KeyFrame* {
        for (const auto& k : keys) {
            if (!(k.time < t) && !(t < k.time)) return &k;
        }
        return nullptr;
    };

    xProp.clearKeyFrames();
    yProp.clearKeyFrames();
    for (const auto& [t, pt] : pairs) {
        const QPointF remapped = ArtifactCore::ResolutionRemap::remapPosition(
            pt, oldSize, newSize, policy);
        const auto* xk = findKey(xKeys, t);
        const auto* yk = findKey(yKeys, t);
        if (xk) {
            xProp.addKeyFrame(t, QVariant(remapped.x()), xk->interpolation,
                              xk->cp1_x, xk->cp1_y, xk->cp2_x, xk->cp2_y, xk->roving);
        } else {
            xProp.addKeyFrame(t, QVariant(remapped.x()));
        }
        if (yk) {
            yProp.addKeyFrame(t, QVariant(remapped.y()), yk->interpolation,
                              yk->cp1_x, yk->cp1_y, yk->cp2_x, yk->cp2_y, yk->roving);
        } else {
            yProp.addKeyFrame(t, QVariant(remapped.y()));
        }
    }
}

// X/Y 別のスカラー倍率プロパティをそれぞれ remapScale で再計算する。
void remapScalePropertyPair(ArtifactCore::AbstractProperty& xProp,
                            ArtifactCore::AbstractProperty& yProp,
                            const QSize& oldSize,
                            const QSize& newSize,
                            ArtifactCore::RemapPolicy policy)
{
    auto remapScalar = [&](ArtifactCore::AbstractProperty& prop) {
        const auto keys = prop.getKeyFrames();
        if (keys.empty()) {
            const double v = ArtifactCore::ResolutionRemap::remapScale(
                prop.getValue().toDouble(), oldSize, newSize, policy);
            prop.setValue(v);
            return;
        }
        prop.clearKeyFrames();
        for (const auto& k : keys) {
            const double v = ArtifactCore::ResolutionRemap::remapScale(
                k.value.toDouble(), oldSize, newSize, policy);
            prop.addKeyFrame(k.time, QVariant(v), k.interpolation,
                             k.cp1_x, k.cp1_y, k.cp2_x, k.cp2_y, k.roving);
        }
    };
    remapScalar(xProp);
    remapScalar(yProp);
}

// レイヤーの transform 系プロパティ keyframe を解像度変更に合わせて remap する。
void remapLayerTransformProperties(const ArtifactAbstractLayerPtr& layer,
                                   const QSize& oldSize,
                                   const QSize& newSize,
                                   ArtifactCore::RemapPolicy policy)
{
    if (!layer) return;

    // 対象プロパティ名。ArtifactAbstractLayer が "Transform" グループ以下に
    // transform.position.x/.y, transform.scale.x/.y, transform.anchor.x/.y として公開する。
    // rotation は角度で aspect 非依存のため対象外。
    auto findInGroups = [&layer](const QString& name) -> ArtifactCore::AbstractPropertyPtr {
        for (const auto& group : layer->getLayerPropertyGroups()) {
            if (auto p = group.findProperty(name)) {
                return p;
            }
        }
        return nullptr;
    };

    if (auto px = findInGroups(QStringLiteral("transform.position.x"))) {
        if (auto py = findInGroups(QStringLiteral("transform.position.y"))) {
            remapPointPropertyPair(*px, *py, oldSize, newSize, policy);
        }
    }
    if (auto ax = findInGroups(QStringLiteral("transform.anchor.x"))) {
        if (auto ay = findInGroups(QStringLiteral("transform.anchor.y"))) {
            remapPointPropertyPair(*ax, *ay, oldSize, newSize, policy);
        }
    }
    if (auto sx = findInGroups(QStringLiteral("transform.scale.x"))) {
        if (auto sy = findInGroups(QStringLiteral("transform.scale.y"))) {
            remapScalePropertyPair(*sx, *sy, oldSize, newSize, policy);
        }
    }
}

void collectAssetSourcePaths(const QJsonValue& value, QSet<QString>& out)
{
  if (value.isObject()) {
    const QJsonObject obj = value.toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      const QString key = it.key();
      const QString loweredKey = key.toLower();
      if ((loweredKey == QStringLiteral("sourcepath")) ||
          (loweredKey == QStringLiteral("filepath")) ||
          loweredKey.endsWith(QStringLiteral(".sourcepath")) ||
          loweredKey.endsWith(QStringLiteral(".filepath")) ||
          loweredKey.contains(QStringLiteral("sourcepath")) ||
          loweredKey.contains(QStringLiteral("filepath"))) {
        const QString candidate = it.value().toString().trimmed();
        if (!candidate.isEmpty()) {
          out.insert(candidate);
        }
      }
      collectAssetSourcePaths(it.value(), out);
    }
    return;
  }

  if (value.isArray()) {
    const QJsonArray array = value.toArray();
    for (const auto& child : array) {
      collectAssetSourcePaths(child, out);
    }
  }
}

ArtifactCore::AssetID assetIdForPath(const QString& path)
{
  const QFileInfo info(path);
  QString normalized = info.canonicalFilePath();
  if (normalized.isEmpty()) {
    normalized = QDir::cleanPath(path);
  }
  return ArtifactCore::AssetID(QCryptographicHash::hash(normalized.toUtf8(), QCryptographicHash::Sha256).toHex().constData());
}

QString slugifyEffectId(const QString& text)
{
  QString slug;
  slug.reserve(text.size());
  bool lastWasDash = false;
  for (const QChar ch : text.trimmed().toLower()) {
    if (ch.isLetterOrNumber()) {
      slug.append(ch);
      lastWasDash = false;
    } else if (!slug.isEmpty() && !lastWasDash) {
      slug.append(QChar('-'));
      lastWasDash = true;
    }
  }
  while (slug.endsWith(QChar('-'))) {
    slug.chop(1);
  }
  return slug.isEmpty() ? QStringLiteral("effect") : slug;
}

QString uniqueEffectIdForComposition(
    const std::vector<std::shared_ptr<ArtifactAbstractEffect>>& effects,
    const QString& displayName,
    const QString& preferredId)
{
  QString baseId = preferredId.trimmed();
  if (baseId.isEmpty()) {
    baseId = slugifyEffectId(displayName);
  }

  const auto idExists = [&effects](const QString& candidate) {
    return std::any_of(
        effects.begin(), effects.end(),
        [&candidate](const std::shared_ptr<ArtifactAbstractEffect>& effect) {
          return effect && effect->effectID().toQString() == candidate;
        });
  };

  if (!idExists(baseId)) {
    return baseId;
  }

  QString uniqueId = baseId;
  int suffix = 2;
  while (idExists(uniqueId)) {
    uniqueId = QStringLiteral("%1-%2").arg(baseId).arg(suffix++);
  }
  return uniqueId;
}

bool layerBooleanProperty(const ArtifactAbstractLayerPtr& layer,
                          const QString& propertyPath,
                          bool fallback = false)
{
  if (!layer) {
    return fallback;
  }
  const auto property = layer->getProperty(propertyPath);
  return property ? property->getValue().toBool() : fallback;
}

int layerIntProperty(const ArtifactAbstractLayerPtr& layer,
                     const QString& propertyPath,
                     int fallback = 0)
{
  if (!layer) {
    return fallback;
  }
  const auto property = layer->getProperty(propertyPath);
  return property ? property->getValue().toInt() : fallback;
}

float layerFloatProperty(const ArtifactAbstractLayerPtr& layer,
                         const QString& propertyPath,
                         float fallback = 0.0f)
{
  if (!layer) {
    return fallback;
  }
  const auto property = layer->getProperty(propertyPath);
  return property ? property->getValue().toFloat() : fallback;
}

QString recordingSnapshotKey(const LayerID& layerId, const QString& propertyPath)
{
  return layerId.toString() + QStringLiteral("::") + propertyPath;
}

QRectF compositionCollisionLocalBounds(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) {
    return QRectF();
  }

  const QRectF localBounds = layer->localBounds();
  if (!localBounds.isValid()) {
    return QRectF();
  }

  const int shape = layerIntProperty(
      layer, QStringLiteral("component.collision.shape"), 0);
  const float width = std::max(
      0.0f, layerFloatProperty(
                layer, QStringLiteral("component.collision.width"), 0.0f));
  const float height = std::max(
      0.0f, layerFloatProperty(
                layer, QStringLiteral("component.collision.height"), 0.0f));
  const float radius = std::max(
      0.0f, layerFloatProperty(
                layer, QStringLiteral("component.collision.radius"), 0.0f));
  const float offsetX = layerFloatProperty(
      layer, QStringLiteral("component.collision.offsetX"), 0.0f);
  const float offsetY = layerFloatProperty(
      layer, QStringLiteral("component.collision.offsetY"), 0.0f);
  const QPointF center = localBounds.center() +
                         QPointF(static_cast<qreal>(offsetX),
                                 static_cast<qreal>(offsetY));

  if (shape == 1) {
    const qreal boxWidth = width > 0.0f ? static_cast<qreal>(width)
                                        : localBounds.width();
    const qreal boxHeight = height > 0.0f ? static_cast<qreal>(height)
                                          : localBounds.height();
    return QRectF(center.x() - boxWidth * 0.5, center.y() - boxHeight * 0.5,
                  boxWidth, boxHeight);
  }

  if (shape == 2) {
    const qreal circleRadius =
        radius > 0.0f
            ? static_cast<qreal>(radius)
            : static_cast<qreal>(
                  std::max(localBounds.width(), localBounds.height()) * 0.5);
    return QRectF(center.x() - circleRadius, center.y() - circleRadius,
                  circleRadius * 2.0, circleRadius * 2.0);
  }

  return localBounds.translated(static_cast<qreal>(offsetX),
                                static_cast<qreal>(offsetY));
}

QRectF compositionCollisionBounds(const ArtifactAbstractLayerPtr& layer)
{
  if (!layer) {
    return QRectF();
  }
  const QRectF localBounds = compositionCollisionLocalBounds(layer);
  if (!localBounds.isValid()) {
    return QRectF();
  }
  return layer->getGlobalTransform().mapRect(localBounds);
}

QString collisionPairKey(const ArtifactAbstractLayerPtr& a,
                         const ArtifactAbstractLayerPtr& b)
{
  if (!a || !b) {
    return QString();
  }
  const QString idA = a->id().toString();
  const QString idB = b->id().toString();
  return idA <= idB ? QStringLiteral("%1|%2").arg(idA, idB)
                    : QStringLiteral("%1|%2").arg(idB, idA);
}

QJsonObject serializeEffect(const std::shared_ptr<ArtifactAbstractEffect>& effect)
{
  QJsonObject eobj;
  if (!effect) {
    return eobj;
  }
  eobj["id"] = effect->effectID().toQString();
  eobj["displayName"] = effect->displayName().toQString();
  eobj["enabled"] = effect->isEnabled();
  eobj["pipelineStage"] = static_cast<int>(effect->pipelineStage());

  QJsonArray propsArr;
  for (const auto& property : effect->getProperties()) {
    QJsonObject pobj;
    pobj["name"] = property.getName();
    pobj["type"] = static_cast<int>(property.getType());
    switch (property.getType()) {
    case ArtifactCore::PropertyType::Float:
    case ArtifactCore::PropertyType::Integer:
    case ArtifactCore::PropertyType::Boolean:
    case ArtifactCore::PropertyType::String:
      pobj["value"] = QJsonValue::fromVariant(property.getValue());
      break;
    case ArtifactCore::PropertyType::Color: {
      const QColor color = property.getColorValue();
      QJsonObject colorObj;
      colorObj["r"] = color.redF();
      colorObj["g"] = color.greenF();
      colorObj["b"] = color.blueF();
      colorObj["a"] = color.alphaF();
      pobj["value"] = colorObj;
      break;
    }
    default:
      pobj["value"] = QJsonValue();
      break;
    }
    propsArr.append(pobj);
  }
  eobj["properties"] = propsArr;
  return eobj;
}

std::shared_ptr<ArtifactAbstractEffect> deserializeEffect(const QJsonObject& eobj)
{
  auto effect = std::make_shared<ArtifactAbstractEffect>();
  effect->setEffectID(UniString::fromQString(eobj.value(QStringLiteral("id")).toString()));
  effect->setDisplayName(UniString::fromQString(
      eobj.value(QStringLiteral("displayName")).toString(effect->effectID().toQString())));
  effect->setEnabled(eobj.value(QStringLiteral("enabled")).toBool(true));
  if (eobj.contains(QStringLiteral("pipelineStage"))) {
    effect->setPipelineStage(static_cast<EffectPipelineStage>(
        eobj.value(QStringLiteral("pipelineStage")).toInt(
            static_cast<int>(EffectPipelineStage::Rasterizer))));
  }

  const auto propsArr = eobj.value(QStringLiteral("properties")).toArray();
  for (const auto& value : propsArr) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject pobj = value.toObject();
    const QString name = pobj.value(QStringLiteral("name")).toString();
    if (name.trimmed().isEmpty()) {
      continue;
    }
    const auto type = static_cast<ArtifactCore::PropertyType>(
        pobj.value(QStringLiteral("type")).toInt(
            static_cast<int>(ArtifactCore::PropertyType::String)));
    QVariant propertyValue;
    if (type == ArtifactCore::PropertyType::Color &&
        pobj.value(QStringLiteral("value")).isObject()) {
      const QJsonObject colorObj = pobj.value(QStringLiteral("value")).toObject();
      QColor color;
      color.setRedF(static_cast<float>(colorObj.value(QStringLiteral("r")).toDouble(0.0)));
      color.setGreenF(static_cast<float>(colorObj.value(QStringLiteral("g")).toDouble(0.0)));
      color.setBlueF(static_cast<float>(colorObj.value(QStringLiteral("b")).toDouble(0.0)));
      color.setAlphaF(static_cast<float>(colorObj.value(QStringLiteral("a")).toDouble(1.0)));
      propertyValue = color;
    } else {
      propertyValue = pobj.value(QStringLiteral("value")).toVariant();
    }
    effect->setPropertyValue(UniString::fromQString(name), propertyValue);
  }
  return effect;
}

Artifact::ResponsiveLayoutVariant makeDefaultResponsiveLayoutVariant(const QSize& size)
 {
   Artifact::ResponsiveLayoutVariant variant;
   variant.variantId = QStringLiteral("default");
   variant.displayName = QStringLiteral("Default");
   variant.baseSize = size.isValid() ? size : QSize(1920, 1080);
   variant.aspectRatio = variant.baseSize.height() > 0
       ? static_cast<qreal>(variant.baseSize.width()) / static_cast<qreal>(variant.baseSize.height())
       : 0.0;
   variant.safeArea = QRectF(0.0, 0.0, 1.0, 1.0);
   variant.contentAnchor = QPointF(0.5, 0.5);
   variant.layoutRules.insert(QStringLiteral("scaleMode"), QStringLiteral("fit"));
   variant.layoutRules.insert(QStringLiteral("cropMode"), QStringLiteral("none"));
   variant.layoutRules.insert(QStringLiteral("guidePreset"), QStringLiteral("default"));
   variant.enabled = true;
   return variant;
 }

 Artifact::ResponsiveLayoutSet makeDefaultResponsiveLayoutSet(const QSize& size)
 {
   Artifact::ResponsiveLayoutSet layout;
   layout.activeVariantId = QStringLiteral("default");
   layout.defaultPolicy = QStringLiteral("manual");
   layout.variants.append(makeDefaultResponsiveLayoutVariant(size));
   return layout;
 }

  // Safety net: soft-clipper with Pade [2/2] tanh approximation
  static void softClip(AudioSegment& segment) {
    constexpr float knee = 0.85f;
    constexpr float ceiling = 0.98f;
    constexpr float invRange = 1.0f / (1.0f - knee);
    for (auto& channel : segment.channelData) {
      for (float& sample : channel) {
        float absVal = std::abs(sample);
        if (absVal > knee) {
          float t = (absVal - knee) * invRange;
          float t2 = t * t;
          float fastTanh = t * (27.0f + t2) / (27.0f + 9.0f * t2);
          sample = (sample >= 0.0f ? 1.0f : -1.0f) * (knee + (ceiling - knee) * fastTanh);
        }
      }
    }
  }

} // anonymous namespace

// AudioLimiter (module-linkage, used by Impl for per-composition limiting state)
// Look-ahead: delays the signal by lookAheadMs_ to let gain reduction start
// before the transient reaches the output.
class AudioLimiter {
public:
  AudioLimiter() : lookAheadMs_(3.0f) {}

  void process(AudioSegment& segment, int sampleRate) {
    if (sampleRate != sampleRate_) {
      const int prevSR = sampleRate_;
      sampleRate_ = sampleRate;
      attackCoeff_ = 1.0f - std::exp(-1.0f / (attackMs_ * 0.001f * sampleRate_));
      releaseCoeff_ = 1.0f - std::exp(-1.0f / (releaseMs_ * 0.001f * sampleRate_));
      if (lookAheadMs_ > 0.0f) {
        delaySize_ = std::max(1, static_cast<int>(lookAheadMs_ * 0.001f * sampleRate_));
      } else {
        delaySize_ = 0;
      }
      if (prevSR != sampleRate_) {
        delayBuf_.clear();
        delayPos_ = 0;
      }
    }
    const int channels = segment.channelCount();
    const int frames = segment.frameCount();

    // Ensure delay buffer has per-channel storage
    if (delaySize_ > 0 && static_cast<int>(delayBuf_.size()) < channels) {
      delayBuf_.resize(channels);
      for (auto& buf : delayBuf_) buf.assign(delaySize_, 0.0f);
      delayPos_ = 0;
    }

    for (int i = 0; i < frames; ++i) {
      // 1) Peak-detect from the FUTURE (current) sample
      float peak = 0.0f;
      for (int ch = 0; ch < channels; ++ch) {
        peak = std::max(peak, std::abs(segment.channelData[ch][i]));
      }

      // 2) Compute desired gain, smooth envelope
      const float desiredGain = (peak > threshold_) ? threshold_ / peak : 1.0f;
      if (desiredGain < envelope_) {
        envelope_ = envelope_ + (desiredGain - envelope_) * attackCoeff_;
      } else {
        envelope_ = envelope_ + (desiredGain - envelope_) * releaseCoeff_;
      }

      // 3) Delay line: write current sample, read delayed sample, apply gain
      if (delaySize_ > 0 && !delayBuf_.empty()) {
        for (int ch = 0; ch < channels && ch < static_cast<int>(delayBuf_.size()); ++ch) {
          float cur = segment.channelData[ch][i];
          float delayed = delayBuf_[ch][delayPos_];
          segment.channelData[ch][i] = delayed * envelope_;
          delayBuf_[ch][delayPos_] = cur;
        }
        delayPos_ = (delayPos_ + 1) % delaySize_;
      } else {
        for (int ch = 0; ch < channels; ++ch) {
          segment.channelData[ch][i] *= envelope_;
        }
      }
    }
  }

  void reset() {
    envelope_ = 1.0f;
    for (auto& buf : delayBuf_) {
      std::fill(buf.begin(), buf.end(), 0.0f);
    }
    delayPos_ = 0;
  }

private:
  int sampleRate_ = 0;
  float threshold_ = 0.9f;
  float lookAheadMs_ = 3.0f;
  float attackMs_ = 1.0f;
  float releaseMs_ = 100.0f;
  float envelope_ = 1.0f;
  float attackCoeff_ = 1.0f;
  float releaseCoeff_ = 0.0f;
  int delaySize_ = 0;
  int delayPos_ = 0;
  std::vector<std::vector<float>> delayBuf_;
};

void ArtifactAbstractComposition::changed()
{
  ArtifactCore::globalEventBus().publish<CompositionChangedEvent>(
      CompositionChangedEvent{ id().toString() });
}

void ArtifactAbstractComposition::compositionNoteChanged(const QString& note)
{
  ArtifactCore::globalEventBus().publish<CompositionNoteChangedEvent>(
      CompositionNoteChangedEvent{ id().toString(), note });
}

class ArtifactAbstractComposition::Impl {
 private:
  

 public:
  Impl(ArtifactAbstractComposition* owner);
  ~Impl();
  ArtifactAbstractComposition* owner_;
  MultiIndexLayerContainer layerMultiIndex_;
  CompositionSettings settings_;
  CompositionContext context_;
  FramePosition position_;
  FrameRange frameRange_ = FrameRange(0, 300);
  FrameRange workAreaRange_ = FrameRange(0, 300);
  FrameRate frameRate_;
  ResponsiveLayoutSet responsiveLayout_;
  QVector<CompositionTransformField> transformFields_;
  QVector<CompositionStateVariant> stateVariants_;
  QString activeStateVariantId_;
  struct RecordedPropertySnapshot {
    LayerID layerId;
    QString propertyPath;
    QVariant value;
    std::vector<KeyFrame> keyframes;
  };
  struct LiveControlRecordingState {
    bool active = false;
    LiveControlRecordingOptions options;
    QHash<QString, qint64> lastRecordedFrameByAddress;
    QHash<QString, double> lastRecordedValueByAddress;
    QHash<QString, RecordedPropertySnapshot> snapshotsByAddress;
  } liveControlRecording_;
  bool looping_ = false;
  float playbackSpeed_ = 1.0f;
  AudioLimiter limiter_;
  std::shared_ptr<AudioMixer> audioMixer_;
  CompositionID id_;
  QString compositionNote_;
  FloatColor backgroundColor_ = { 0.47f, 0.47f, 0.47f, 1.0f };
  std::vector<std::shared_ptr<ArtifactAbstractEffect>> effects_;
  mutable QImage thumbnailCache_;
  mutable QSize thumbnailCacheSize_;
  mutable bool thumbnailCacheValid_ = false;
  //PlaybackClock playbackClock_;  // 高精度再生クロック
  
  AppendLayerToCompositionResult appendLayerTop(ArtifactAbstractLayerPtr layer);
  AppendLayerToCompositionResult appendLayerBottom(ArtifactAbstractLayerPtr layer);
  void invalidateThumbnailCache();
  bool containsLayerById(const LayerID& id) const;
  void removeAllLayers();
  void recalculateFrameRange();
  void removeLayer(const LayerID& id);
  const FramePosition framePosition() const;
  void setFramePosition(const FramePosition& position);
  void goToStartFrame();
  void goToEndFrame();
  void goToFrame(int64_t frame=0);
  QList<ArtifactAbstractLayerPtr> allLayer() const;
  QVector<ArtifactAbstractLayerPtr> allLayerBackToFront() const;

   ArtifactAbstractLayerPtr frontMostLayer() const;
   ArtifactAbstractLayerPtr backMostLayer() const;
   bool hasVideo() const;
   bool hasAudio() const;
   void moveLayerToIndex(const LayerID& id, int newIndex);
   void bringToFront(const LayerID& id);
   void sendToBack(const LayerID& id);
   void addEffect(std::shared_ptr<ArtifactAbstractEffect> effect);
   void removeEffect(const UniString& effectID);
   void clearEffects();
   std::vector<std::shared_ptr<ArtifactAbstractEffect>> getEffects() const;
   std::shared_ptr<ArtifactAbstractEffect> getEffect(const UniString& effectID) const;
   int effectCount() const;

    bool isPlaying_ = false;
    QSet<QString> activeCollisionPairs_;

    // Asset usage tracking
    QVector<ArtifactCore::AssetID> getUsedAssets() const;
    void evaluateLayerCollisionPairs();
  };

 ArtifactAbstractComposition::Impl::Impl(ArtifactAbstractComposition* owner) : owner_(owner)
 {

 }

 ArtifactAbstractComposition::Impl::~Impl()
 {

 }

void ArtifactAbstractComposition::Impl::invalidateThumbnailCache()
{
  thumbnailCacheValid_ = false;
  thumbnailCache_.fill(Qt::transparent);
  thumbnailCacheSize_ = QSize();
}

 AppendLayerToCompositionResult ArtifactAbstractComposition::Impl::appendLayerTop(ArtifactAbstractLayerPtr layer)
 {
  AppendLayerToCompositionResult result;

  if (!layer) {
   result.success = false;
   result.error = AppendLayerToCompositionError::LayerNotFound;
   result.message = QString("Layer not found");
   return result;
  }

  auto id = layer->id();

  layer->setComposition(owner_);
  
  // Newly created layers often have a default duration. 
  // If it's a solid/plane layer and hasn't been configured yet, match composition duration.
  const int64_t currentOut = layer->outPoint().framePosition();
  const int64_t currentIn = layer->inPoint().framePosition();
  const int64_t currentStart = layer->startTime().framePosition();
  const int64_t compEnd = frameRange_.end();
  
  if (currentOut == 300 && currentIn == 0 && currentStart == 0 && compEnd > 0) {
    layer->setOutPoint(FramePosition(compEnd));
  }
  
  layerMultiIndex_.add(layer,id,layer->type_index());
  invalidateThumbnailCache();
  recalculateFrameRange();

  result.success = true;
  result.error = AppendLayerToCompositionError::None;
  result.message = QString("Layer added successfully");

  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      owner_->id().toString(), id.toString(),
      LayerChangedEvent::ChangeType::Created});
  return result;
  }

  void ArtifactAbstractComposition::Impl::recalculateFrameRange()
  {
   int64_t maxOut = 0;
   for (const auto& layer : layerMultiIndex_) {
    if (layer) {
     maxOut = std::max(maxOut, layer->outPoint().framePosition());
    }
   }
   if (maxOut > 0) {
    frameRange_ = FrameRange(0, maxOut);
   }
  }

  void ArtifactAbstractComposition::Impl::removeAllLayers()
 {
  for (auto& layer : layerMultiIndex_) {
   if (layer) {
    layer->setComposition(static_cast<ArtifactAbstractComposition *>(nullptr));
   }
  }
  layerMultiIndex_.clear();
  invalidateThumbnailCache();
  ArtifactCore::globalEventBus().publish(LayerChangedEvent{
      owner_->id().toString(), QString{},
      LayerChangedEvent::ChangeType::Removed});
 }

void ArtifactAbstractComposition::Impl::removeLayer(const LayerID& id)
{
   auto removedLayer = layerMultiIndex_.findById(id);
   // Safe Detachment: Clear parent link of any layer that refers to this ID
    for (auto& layer : layerMultiIndex_) {
       if (layer->parentLayerId() == id) {
           layer->clearParent();
       }
   }
    layerMultiIndex_.removeById(id);
    if (removedLayer) {
     removedLayer->setComposition(static_cast<ArtifactAbstractComposition *>(nullptr));
     invalidateThumbnailCache();
     ArtifactCore::globalEventBus().publish(LayerChangedEvent{
         owner_->id().toString(), id.toString(),
         LayerChangedEvent::ChangeType::Removed});
    }
}

 bool ArtifactAbstractComposition::Impl::containsLayerById(const LayerID& id) const
 {
  return layerMultiIndex_.containsId(id);
 }

 void ArtifactAbstractComposition::Impl::goToStartFrame()
 {
  goToFrame(frameRange_.start());
 }

 void ArtifactAbstractComposition::Impl::goToEndFrame()
 {
  goToFrame(frameRange_.end());
 }

 void ArtifactAbstractComposition::Impl::setFramePosition(const FramePosition& position)
 {
    if (position_ == position) {
        return;
    }
    // Playback updates only need the composition's current frame state.
    // Layer propagation is handled by goToFrame() for explicit timeline edits/seeks.
    position_ = position;
 }

 const FramePosition ArtifactAbstractComposition::Impl::framePosition() const
 {
  return position_;
 }

  void ArtifactAbstractComposition::Impl::goToFrame(int64_t frame/*=0*/)
  {
    position_ = FramePosition(frame);
    for (auto& layer : layerMultiIndex_) {
        if (layer) layer->goToFrame(frame);
    }
    evaluateLayerCollisionPairs();
  }

void ArtifactAbstractComposition::Impl::evaluateLayerCollisionPairs()
{
  QSet<QString> nextPairs;
  std::vector<ArtifactAbstractLayerPtr> collisionLayers;
  collisionLayers.reserve(static_cast<std::size_t>(layerMultiIndex_.all().size()));

  for (const auto& layer : layerMultiIndex_) {
    if (!layer || !layer->isActiveAt(position_)) {
      continue;
    }
    if (!layerBooleanProperty(
            layer, QStringLiteral("component.collision.enabled"), false)) {
      continue;
    }
    collisionLayers.push_back(layer);
  }

  for (std::size_t i = 0; i < collisionLayers.size(); ++i) {
    const auto& first = collisionLayers[i];
    const QRectF firstBounds = compositionCollisionBounds(first);
    if (!firstBounds.isValid()) {
      continue;
    }
    for (std::size_t j = i + 1; j < collisionLayers.size(); ++j) {
      const auto& second = collisionLayers[j];
      const QRectF secondBounds = compositionCollisionBounds(second);
      if (!secondBounds.isValid() || !firstBounds.intersects(secondBounds)) {
        continue;
      }

      const QString pairKey = collisionPairKey(first, second);
      if (pairKey.isEmpty()) {
        continue;
      }
      nextPairs.insert(pairKey);
      if (activeCollisionPairs_.contains(pairKey)) {
        continue;
      }

      const QRectF overlap = firstBounds.intersected(secondBounds);
      const QPointF center = overlap.center();
      const float overlapExtent = static_cast<float>(
          std::max(overlap.width(), overlap.height()));

      FractureImpact impact;
      impact.impulse = std::max(0.1f, overlapExtent / 64.0f);
      impact.speed = overlapExtent;
      impact.stress = impact.impulse;
      impact.area = std::max(1.0f, static_cast<float>(overlap.width() * overlap.height()));

      first->applyFractureImpact(impact);
      second->applyFractureImpact(impact);
    }
  }

  activeCollisionPairs_ = std::move(nextPairs);
}

 QList<ArtifactAbstractLayerPtr> ArtifactAbstractComposition::Impl::allLayer() const
 {
  return layerMultiIndex_.all();
 }

  AppendLayerToCompositionResult ArtifactAbstractComposition::Impl::appendLayerBottom(ArtifactAbstractLayerPtr layer)
  {
      AppendLayerToCompositionResult result;
      if (!layer) {
          result.success = false;
          result.error = AppendLayerToCompositionError::LayerNotFound;
          return result;
      }
      layer->setComposition(owner_);
      
      const int64_t currentOut = layer->outPoint().framePosition();
      const int64_t currentIn = layer->inPoint().framePosition();
      const int64_t currentStart = layer->startTime().framePosition();
      const int64_t compEnd = frameRange_.end();
      
      if (currentOut == 300 && currentIn == 0 && currentStart == 0 && compEnd > 0) {
          layer->setOutPoint(FramePosition(compEnd));
      }
      
      layerMultiIndex_.insertAt(0, layer, layer->id(), layer->type_index());
      invalidateThumbnailCache();
      recalculateFrameRange();
      result.success = true;
      result.error = AppendLayerToCompositionError::None;
      ArtifactCore::globalEventBus().publish(LayerChangedEvent{
          owner_->id().toString(), layer->id().toString(),
          LayerChangedEvent::ChangeType::Created});
      return result;
  }

  void ArtifactAbstractComposition::Impl::moveLayerToIndex(const LayerID& id, int newIndex)
  {
      auto layer = layerMultiIndex_.findById(id);
      if (!layer) return;
      int oldIndex = layerMultiIndex_.indexOf(layer);
      if (oldIndex == -1) return;
      layerMultiIndex_.move(oldIndex, newIndex);
      invalidateThumbnailCache();
      ArtifactCore::globalEventBus().publish(LayerChangedEvent{
          owner_->id().toString(), id.toString(),
          LayerChangedEvent::ChangeType::Modified});
  }

  void ArtifactAbstractComposition::Impl::bringToFront(const LayerID& id)
  {
      auto layer = layerMultiIndex_.findById(id);
      if (!layer) return;
      int oldIndex = layerMultiIndex_.indexOf(layer);
      if (oldIndex == -1) return;
      layerMultiIndex_.move(oldIndex, layerMultiIndex_.all().size() - 1);
      invalidateThumbnailCache();
      ArtifactCore::globalEventBus().publish(LayerChangedEvent{
          owner_->id().toString(), id.toString(),
          LayerChangedEvent::ChangeType::Modified});
  }

  void ArtifactAbstractComposition::Impl::sendToBack(const LayerID& id)
  {
      auto layer = layerMultiIndex_.findById(id);
      if (!layer) return;
      int oldIndex = layerMultiIndex_.indexOf(layer);
      if (oldIndex == -1) return;
      layerMultiIndex_.move(oldIndex, 0);
      invalidateThumbnailCache();
      ArtifactCore::globalEventBus().publish(LayerChangedEvent{
          owner_->id().toString(), id.toString(),
          LayerChangedEvent::ChangeType::Modified});
  }

 bool ArtifactAbstractComposition::Impl::hasVideo() const
 {
  for (const auto& layer : layerMultiIndex_) {
   if (layer->hasVideo())
   {
	return true;
   }
  }
 	
  return false;
 }

 bool ArtifactAbstractComposition::Impl::hasAudio() const
 {
  for (const auto& layer : layerMultiIndex_) {
   if (layer->hasAudio())
   {
	return true;
   }
  }
  return false;
 }

ArtifactAbstractLayerPtr ArtifactAbstractComposition::Impl::frontMostLayer() const
{
    const auto& all = layerMultiIndex_.all();
    if (all.isEmpty()) {
      return ArtifactAbstractLayerPtr();
    }
    ArtifactAbstractLayerPtr layer = all.constLast();
    return layer;
}

ArtifactAbstractLayerPtr ArtifactAbstractComposition::Impl::backMostLayer() const
{
    const auto& all = layerMultiIndex_.all();
    if (all.isEmpty()) {
      return ArtifactAbstractLayerPtr();
    }
    ArtifactAbstractLayerPtr layer = all.constFirst();
    return layer;
}

  QVector<ArtifactAbstractLayerPtr> ArtifactAbstractComposition::Impl::allLayerBackToFront() const
  {
    QVector<ArtifactAbstractLayerPtr> v = layerMultiIndex_.all();
    std::reverse(v.begin(), v.end());
    return v;
   }

  QVector<ArtifactCore::AssetID> ArtifactAbstractComposition::Impl::getUsedAssets() const
  {
    QVector<ArtifactCore::AssetID> usedAssets;
    QSet<QString> sourcePaths;

    // Collect assets from all layers
    for (const auto& layer : layerMultiIndex_.all()) {
      if (!layer) continue;
      collectAssetSourcePaths(layer->toJson(), sourcePaths);
    }

    for (const auto& path : sourcePaths) {
      usedAssets.append(assetIdForPath(path));
    }

    return usedAssets;
  }

QJsonObject ResponsiveLayoutVariant::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("variantId"), variantId);
    obj.insert(QStringLiteral("displayName"), displayName);
    obj.insert(QStringLiteral("width"), baseSize.width());
    obj.insert(QStringLiteral("height"), baseSize.height());
    obj.insert(QStringLiteral("aspectRatio"), aspectRatio);
    obj.insert(QStringLiteral("safeArea"), QJsonObject{
        {QStringLiteral("x"), safeArea.x()},
        {QStringLiteral("y"), safeArea.y()},
        {QStringLiteral("width"), safeArea.width()},
        {QStringLiteral("height"), safeArea.height()},
    });
    obj.insert(QStringLiteral("contentAnchor"), QJsonObject{
        {QStringLiteral("x"), contentAnchor.x()},
        {QStringLiteral("y"), contentAnchor.y()},
    });
    obj.insert(QStringLiteral("layoutRules"), layoutRules);
    obj.insert(QStringLiteral("enabled"), enabled);
    return obj;
}

ResponsiveLayoutVariant ResponsiveLayoutVariant::fromJson(const QJsonObject& obj)
{
    ResponsiveLayoutVariant variant;
    variant.variantId = obj.value(QStringLiteral("variantId")).toString();
    variant.displayName = obj.value(QStringLiteral("displayName")).toString();
    const int width = obj.value(QStringLiteral("width")).toInt(0);
    const int height = obj.value(QStringLiteral("height")).toInt(0);
    variant.baseSize = QSize(width, height);
    variant.aspectRatio = obj.value(QStringLiteral("aspectRatio")).toDouble(
        height > 0 ? static_cast<double>(width) / static_cast<double>(height) : 0.0);
    const QJsonObject safeAreaObj = obj.value(QStringLiteral("safeArea")).toObject();
    variant.safeArea = QRectF(
        safeAreaObj.value(QStringLiteral("x")).toDouble(0.0),
        safeAreaObj.value(QStringLiteral("y")).toDouble(0.0),
        safeAreaObj.value(QStringLiteral("width")).toDouble(1.0),
        safeAreaObj.value(QStringLiteral("height")).toDouble(1.0));
    const QJsonObject anchorObj = obj.value(QStringLiteral("contentAnchor")).toObject();
    variant.contentAnchor = QPointF(
        anchorObj.value(QStringLiteral("x")).toDouble(0.5),
        anchorObj.value(QStringLiteral("y")).toDouble(0.5));
    variant.layoutRules = obj.value(QStringLiteral("layoutRules")).toObject();
    variant.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    if (variant.variantId.isEmpty()) {
        variant.variantId = QStringLiteral("default");
    }
    if (variant.displayName.isEmpty()) {
        variant.displayName = variant.variantId;
    }
    if (!variant.baseSize.isValid()) {
        variant.baseSize = QSize(1920, 1080);
    }
    if (variant.aspectRatio <= 0.0 && variant.baseSize.height() > 0) {
        variant.aspectRatio = static_cast<qreal>(variant.baseSize.width()) / static_cast<qreal>(variant.baseSize.height());
    }
    return variant;
}

QJsonObject ResponsiveLayoutSet::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("activeVariantId"), activeVariantId);
    obj.insert(QStringLiteral("defaultPolicy"), defaultPolicy);
    QJsonArray variantArray;
    for (const auto& variant : variants) {
        variantArray.append(variant.toJson());
    }
    obj.insert(QStringLiteral("variants"), variantArray);
    return obj;
}

ResponsiveLayoutSet ResponsiveLayoutSet::fromJson(const QJsonObject& obj)
{
    ResponsiveLayoutSet layout;
    layout.activeVariantId = obj.value(QStringLiteral("activeVariantId")).toString();
    layout.defaultPolicy = obj.value(QStringLiteral("defaultPolicy")).toString(QStringLiteral("manual"));
    const QJsonArray variantArray = obj.value(QStringLiteral("variants")).toArray();
    for (const auto& value : variantArray) {
        if (value.isObject()) {
            layout.variants.append(ResponsiveLayoutVariant::fromJson(value.toObject()));
        }
    }
    if (layout.variants.isEmpty()) {
        layout.variants.append(makeDefaultResponsiveLayoutVariant(QSize(1920, 1080)));
    }
    if (layout.activeVariantId.isEmpty() || !layout.hasVariant(layout.activeVariantId)) {
        layout.activeVariantId = layout.variants.front().variantId;
    }
    return layout;
}

bool ResponsiveLayoutSet::hasVariant(const QString& variantId) const
{
    for (const auto& variant : variants) {
        if (variant.variantId == variantId) {
            return true;
        }
    }
    return false;
}

bool CompositionTransformField::targetsLayer(const LayerID& layerId) const
{
    return targetLayerIds.contains(layerId);
}

QJsonObject CompositionTransformField::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("fieldId"), fieldId);
    obj.insert(QStringLiteral("displayName"), displayName);
    obj.insert(QStringLiteral("type"), QStringLiteral("radial-transform"));
    obj.insert(QStringLiteral("enabled"), enabled);
    obj.insert(QStringLiteral("center"), QJsonObject{
        {QStringLiteral("x"), center.x()},
        {QStringLiteral("y"), center.y()},
    });
    obj.insert(QStringLiteral("radius"), radius);
    obj.insert(QStringLiteral("expansion"), expansion);
    obj.insert(QStringLiteral("edgeScale"), edgeScale);
    obj.insert(
        QStringLiteral("coordinateParentLayerId"),
        coordinateParentLayerId.toString());
    QJsonArray targetArray;
    for (const auto& layerId : targetLayerIds) {
        targetArray.append(layerId.toString());
    }
    obj.insert(QStringLiteral("targetLayerIds"), targetArray);
    return obj;
}

CompositionTransformField CompositionTransformField::fromJson(const QJsonObject& obj)
{
    CompositionTransformField field;
    field.fieldId = obj.value(QStringLiteral("fieldId")).toString();
    field.displayName = obj.value(QStringLiteral("displayName"))
                            .toString(QStringLiteral("Radial Transform Field"));
    field.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    const QJsonObject centerObj = obj.value(QStringLiteral("center")).toObject();
    field.center = QPointF(
        centerObj.value(QStringLiteral("x")).toDouble(0.0),
        centerObj.value(QStringLiteral("y")).toDouble(0.0));
    field.radius = std::max<qreal>(
        0.0001, obj.value(QStringLiteral("radius")).toDouble(1.0));
    field.expansion = obj.value(QStringLiteral("expansion")).toDouble(0.0);
    field.edgeScale = std::max<qreal>(
        0.0001, obj.value(QStringLiteral("edgeScale")).toDouble(1.0));
    field.coordinateParentLayerId = LayerID(
        obj.value(QStringLiteral("coordinateParentLayerId")).toString());
    const QJsonArray targetArray =
        obj.value(QStringLiteral("targetLayerIds")).toArray();
    for (const auto& value : targetArray) {
        const LayerID layerId(value.toString());
        if (!layerId.isNil() && !field.targetLayerIds.contains(layerId)) {
            field.targetLayerIds.append(layerId);
        }
    }
    return field;
}

QJsonObject CompositionStatePropertyOverride::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("layerId"), layerId.toString());
    obj.insert(QStringLiteral("propertyPath"), propertyPath);
    obj.insert(QStringLiteral("value"), QJsonValue::fromVariant(value));
    obj.insert(QStringLiteral("baselineValue"), QJsonValue::fromVariant(baselineValue));
    obj.insert(QStringLiteral("enabled"), enabled);
    return obj;
}

CompositionStatePropertyOverride CompositionStatePropertyOverride::fromJson(
    const QJsonObject& obj)
{
    CompositionStatePropertyOverride overrideItem;
    overrideItem.layerId = LayerID(obj.value(QStringLiteral("layerId")).toString());
    overrideItem.propertyPath = obj.value(QStringLiteral("propertyPath")).toString();
    overrideItem.value = obj.value(QStringLiteral("value")).toVariant();
    overrideItem.baselineValue = obj.value(QStringLiteral("baselineValue")).toVariant();
    overrideItem.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    return overrideItem;
}

QJsonObject CompositionStateVariant::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("stateId"), stateId);
    obj.insert(QStringLiteral("displayName"), displayName);
    obj.insert(QStringLiteral("enabled"), enabled);
    QJsonArray overrideArray;
    for (const auto& overrideItem : overrides) {
        overrideArray.append(overrideItem.toJson());
    }
    obj.insert(QStringLiteral("overrides"), overrideArray);
    return obj;
}

CompositionStateVariant CompositionStateVariant::fromJson(const QJsonObject& obj)
{
    CompositionStateVariant state;
    state.stateId = obj.value(QStringLiteral("stateId")).toString();
    state.displayName = obj.value(QStringLiteral("displayName")).toString();
    state.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    const auto overrideArray = obj.value(QStringLiteral("overrides")).toArray();
    for (const auto& value : overrideArray) {
        if (value.isObject()) {
            state.overrides.append(CompositionStatePropertyOverride::fromJson(
                value.toObject()));
        }
    }
    if (state.stateId.isEmpty()) {
        state.stateId = QStringLiteral("state");
    }
    if (state.displayName.trimmed().isEmpty()) {
        state.displayName = state.stateId;
    }
    return state;
}

bool CompositionStateVariant::hasOverride(const LayerID& layerId,
                                          const QString& propertyPath) const
{
    for (const auto& overrideItem : overrides) {
        if (overrideItem.layerId == layerId &&
            overrideItem.propertyPath == propertyPath) {
            return true;
        }
    }
    return false;
}

 ArtifactAbstractComposition::ArtifactAbstractComposition(const CompositionID& id, const ArtifactCompositionInitParams& params) :impl_(new Impl(this))
 {
  impl_->id_ = id;

  impl_->settings_.setCompositionName(params.compositionName());
  impl_->settings_.setCompositionSize(QSize(params.width(), params.height()));
  // Keep init params and live composition state consistent for viewers.
  impl_->backgroundColor_ = params.backgroundColor();
  impl_->frameRate_ = params.frameRate();

  const int64_t totalFrames = std::max<int64_t>(1, params.durationFrames());
  impl_->frameRange_ = FrameRange(0, totalFrames);

  const auto workArea = params.workArea();
  if (workArea.enabled) {
   const int64_t workStart = std::clamp<int64_t>(workArea.inPoint.rescaledTo(static_cast<int64_t>(std::round(impl_->frameRate_.framerate()))), 0, totalFrames);
   const int64_t workEnd = std::clamp<int64_t>(workArea.outPoint.rescaledTo(static_cast<int64_t>(std::round(impl_->frameRate_.framerate()))), workStart, totalFrames);
   impl_->workAreaRange_ = FrameRange(workStart, workEnd);
  } else {
   impl_->workAreaRange_ = impl_->frameRange_;
  }
  impl_->responsiveLayout_ = makeDefaultResponsiveLayoutSet(impl_->settings_.compositionSize());
 }

 ArtifactAbstractComposition::~ArtifactAbstractComposition()
 {
  delete impl_;
 }

 bool ArtifactAbstractComposition::containsLayerById(const LayerID& id)
 {
  return impl_->containsLayerById(id);
 }

ArtifactAbstractLayerPtr ArtifactAbstractComposition::layerById(const LayerID& id) const
{
  ArtifactAbstractLayerPtr layer = impl_->layerMultiIndex_.findById(id);
  return layer;
}


 void ArtifactAbstractComposition::setBackGroundColor(const FloatColor& color)
 {
  impl_->backgroundColor_ = color;
  Q_EMIT changed();
 }

 FloatColor ArtifactAbstractComposition::backgroundColor() const
 {
  return impl_->backgroundColor_;
 }

 void ArtifactAbstractComposition::Impl::addEffect(std::shared_ptr<ArtifactAbstractEffect> effect)
 {
  if (!effect) {
   return;
  }
  const QString uniqueId = uniqueEffectIdForComposition(
      effects_, effect->displayName().toQString(), effect->effectID().toQString());
  effect->setEffectID(UniString::fromQString(uniqueId));
  effects_.push_back(std::move(effect));
  invalidateThumbnailCache();
 }

 void ArtifactAbstractComposition::Impl::removeEffect(const UniString& effectID)
 {
  const auto it = std::remove_if(
      effects_.begin(), effects_.end(),
      [&effectID](const std::shared_ptr<ArtifactAbstractEffect>& effect) {
       return effect && effect->effectID() == effectID;
      });
  if (it != effects_.end()) {
   effects_.erase(it, effects_.end());
   invalidateThumbnailCache();
  }
 }

 void ArtifactAbstractComposition::Impl::clearEffects()
 {
  effects_.clear();
  invalidateThumbnailCache();
 }

 std::vector<std::shared_ptr<ArtifactAbstractEffect>>
 ArtifactAbstractComposition::Impl::getEffects() const
 {
  return effects_;
 }

 std::shared_ptr<ArtifactAbstractEffect>
 ArtifactAbstractComposition::Impl::getEffect(const UniString& effectID) const
 {
  for (const auto& effect : effects_) {
   if (effect && effect->effectID() == effectID) {
    return effect;
   }
  }
  return nullptr;
 }

 int ArtifactAbstractComposition::Impl::effectCount() const
 {
  return static_cast<int>(effects_.size());
 }

 void ArtifactAbstractComposition::addEffect(std::shared_ptr<ArtifactAbstractEffect> effect)
 {
  impl_->addEffect(std::move(effect));
  changed();
 }

 void ArtifactAbstractComposition::removeEffect(const UniString& effectID)
 {
  impl_->removeEffect(effectID);
  changed();
 }

 void ArtifactAbstractComposition::clearEffects()
 {
  impl_->clearEffects();
  changed();
 }

 std::vector<std::shared_ptr<ArtifactAbstractEffect>>
 ArtifactAbstractComposition::getEffects() const
 {
  return impl_->getEffects();
 }

 std::shared_ptr<ArtifactAbstractEffect>
 ArtifactAbstractComposition::getEffect(const UniString& effectID) const
 {
  return impl_->getEffect(effectID);
 }

 int ArtifactAbstractComposition::effectCount() const
 {
  return impl_->effectCount();
 }
 
 void ArtifactAbstractComposition::setFramePosition(const FramePosition& position)
 {
  impl_->setFramePosition(position);
 }

  FramePosition ArtifactAbstractComposition::framePosition() const
  {
   return impl_->framePosition();
  }

 void ArtifactAbstractComposition::goToStartFrame()
 {
  impl_->goToStartFrame();
 }

 void ArtifactAbstractComposition::goToEndFrame()
 {
  impl_->goToEndFrame();
 }

 void ArtifactAbstractComposition::goToFrame(int64_t frameNumber /*= 0*/)
 {
  impl_->goToFrame(frameNumber);
 }

  FrameRange ArtifactAbstractComposition::frameRange() const
  {
   return impl_->frameRange_;
  }

  FrameRate ArtifactAbstractComposition::frameRate() const
  {
   return impl_->frameRate_;
  }

bool ArtifactAbstractComposition::hasVideo() const
{
  return impl_->hasVideo();
}

bool ArtifactAbstractComposition::hasAudio() const
{
  return impl_->hasAudio();
}

bool ArtifactAbstractComposition::getAudio(AudioSegment &outSegment, const FramePosition &start,
                                            int frameCount, int sampleRate)
{
    bool hasAnyAudio = false;
    int activeAudioLayerCount = 0;
    int producedAudioLayerCount = 0;
    
    // If mixer is active, use the mixer path for proper bus-based mixing.
    if (impl_->audioMixer_) {
        AudioMixer& mixer = *impl_->audioMixer_;
        for (auto &layer : impl_->layerMultiIndex_) {
            if (layer && layer->hasAudio()) {
                const std::string busName = "layer_" + layer->id().toString().toStdString();
                if (!mixer.findBusByName(busName)) {
                    mixer.createBus(busName);
                }
            }
        }
        AudioSegment mixOutput;
        mixOutput.sampleRate = sampleRate;
        mixOutput.channelData.resize(2);
        mixOutput.setFrameCount(frameCount);
        mixOutput.zero();
        AudioSegment layerSegment;
        for (auto &layer : impl_->layerMultiIndex_) {
            if (layer && layer->isActiveAt(start) && layer->hasAudio()) {
                ++activeAudioLayerCount;
                const std::string busName = "layer_" + layer->id().toString().toStdString();
                auto bus = mixer.findBusByName(busName);
                if (!bus) continue;
                bus->clearInput(frameCount, sampleRate);
                if (layer->getAudio(layerSegment, start, frameCount, sampleRate)) {
                    float layerVol = 1.0f;
                    if (auto al = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
                        layerVol = al->volume();
                    } else if (auto vl = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
                        layerVol = static_cast<float>(vl->audioVolume());
                    }
                    bus->addInput(layerSegment, layerVol);
                    ++producedAudioLayerCount;
                    hasAnyAudio = true;
                }
            }
        }
        if (hasAnyAudio) {
            mixer.process(mixOutput);
            outSegment = std::move(mixOutput);
            impl_->limiter_.process(outSegment, sampleRate);
            softClip(outSegment);
        } else {
            outSegment = std::move(mixOutput);
        }
    }
    
    // Fallback: direct sum path (legacy, no mixer)
    if (!impl_->audioMixer_) {
        if (outSegment.channelCount() < 2) {
            outSegment.channelData.resize(2);
        }
        outSegment.sampleRate = sampleRate;
        outSegment.setFrameCount(frameCount);
        outSegment.zero();

        AudioSegment layerSeg;
        for (auto &layer : impl_->layerMultiIndex_) {
            if (layer && layer->isActiveAt(start) && layer->hasAudio()) {
                ++activeAudioLayerCount;
                if (layer->getAudio(layerSeg, start, frameCount, sampleRate)) {
                    int chCount = std::min(outSegment.channelCount(), layerSeg.channelCount());
                    int fCount = std::min(outSegment.frameCount(), layerSeg.frameCount());
                    for (int ch = 0; ch < chCount; ++ch) {
                        float* outData = outSegment.channelData[ch].data();
                        const float* layerData = layerSeg.channelData[ch].constData();
                        for (int i = 0; i < fCount; ++i) {
                            outData[i] += layerData[i];
                        }
                    }
                    ++producedAudioLayerCount;
                    hasAnyAudio = true;
                }
            }
        }
        if (activeAudioLayerCount > 0 && !hasAnyAudio) {
            qWarning() << "[Composition][Audio] active layers produced no audio"
                       << "startFrame=" << start.framePosition()
                       << "frameCount=" << frameCount
                       << "sampleRate=" << sampleRate
                       << "activeAudioLayers=" << activeAudioLayerCount
                       << "producedAudioLayers=" << producedAudioLayerCount;
        }
        if (hasAnyAudio) {
            impl_->limiter_.process(outSegment, sampleRate);
            softClip(outSegment);
        }
    }

    return hasAnyAudio;
}

void ArtifactAbstractComposition::ensureAudioMixer()
{
    if (!impl_->audioMixer_) {
        impl_->audioMixer_ = std::make_shared<AudioMixer>();
        qDebug() << "[Composition] AudioMixer enabled for" << settings().compositionName().toQString();
    }
}

std::shared_ptr<AudioMixer> ArtifactAbstractComposition::getAudioMixer() const
{
    return impl_->audioMixer_;
}

QList<Artifact::ArtifactAbstractLayerPtr> ArtifactAbstractComposition::allLayer()
{
  QList<ArtifactAbstractLayerPtr> layers = impl_->layerMultiIndex_.all();
  return layers;
}

const QList<Artifact::ArtifactAbstractLayerPtr>&
ArtifactAbstractComposition::allLayerRef() const
{
  return impl_->layerMultiIndex_.all();
}

AppendLayerToCompositionResult ArtifactAbstractComposition::appendLayerTop(ArtifactAbstractLayerPtr layer)
 {
  return impl_->appendLayerTop(layer);
 }

 AppendLayerToCompositionResult ArtifactAbstractComposition::appendLayerBottom(ArtifactAbstractLayerPtr layer)
 {
   return impl_->appendLayerBottom(layer);
 }

void ArtifactAbstractComposition::insertLayerAt(ArtifactAbstractLayerPtr layer, int index/*=0*/)
{
    if (!layer) return;
    layer->setComposition(this);
    
    const int64_t currentOut = layer->outPoint().framePosition();
    const int64_t currentIn = layer->inPoint().framePosition();
    const int64_t compEnd = impl_->frameRange_.end();
    
    if (currentOut == 300 && currentIn == 0 && compEnd > 300) {
        layer->setOutPoint(FramePosition(compEnd));
    }
    
    impl_->layerMultiIndex_.insertAt(index, layer, layer->id(), layer->type_index());
    impl_->recalculateFrameRange();
    Q_EMIT changed();
}

void ArtifactAbstractComposition::moveLayerToIndex(const LayerID& id, int newIndex)
{
    impl_->moveLayerToIndex(id, newIndex);
}

void ArtifactAbstractComposition::removeLayer(const LayerID& id)
{
  impl_->removeLayer(id);
  impl_->recalculateFrameRange();
}

void ArtifactAbstractComposition::removeAllLayers()
{
    impl_->removeAllLayers();
}

ArtifactAbstractLayerPtr ArtifactAbstractComposition::frontMostLayer() const
{
    ArtifactAbstractLayerPtr layer = impl_->frontMostLayer();
    return layer;
}

ArtifactAbstractLayerPtr ArtifactAbstractComposition::backMostLayer() const
{
    ArtifactAbstractLayerPtr layer = impl_->backMostLayer();
    return layer;
}

void ArtifactAbstractComposition::bringToFront(const LayerID& id)
{
    impl_->bringToFront(id);
}

void ArtifactAbstractComposition::sendToBack(const LayerID& id)
{
    impl_->sendToBack(id);
}

CompositionID ArtifactAbstractComposition::id() const
{
    return impl_->id_;
}

int ArtifactAbstractComposition::layerCount() const
{
    return impl_->layerMultiIndex_.all().size();
}

bool ArtifactAbstractComposition::isAudioOnly() const
{
 return false;
}

CompositionSettings ArtifactAbstractComposition::settings() const
{
  return impl_->settings_;
}

CompositionContext& ArtifactAbstractComposition::compositionContext()
{
  return impl_->context_;
}

const CompositionContext& ArtifactAbstractComposition::compositionContext() const
{
  return impl_->context_;
}

void ArtifactAbstractComposition::setCompositionContext(const CompositionContext& context)
{
  impl_->context_ = context;
  Q_EMIT changed();
}

void ArtifactAbstractComposition::setCompositionName(const UniString& name)
{
    if (impl_->settings_.compositionName() == name) {
        return;
    }
    impl_->settings_.setCompositionName(name);
    Q_EMIT changed();
}

QString ArtifactAbstractComposition::compositionNote() const
{
    return impl_->compositionNote_;
}

void ArtifactAbstractComposition::setCompositionNote(const QString& note)
{
    if (impl_->compositionNote_ == note) {
        return;
    }
    impl_->compositionNote_ = note;
    Q_EMIT compositionNoteChanged(note);
    Q_EMIT changed();
}

void ArtifactAbstractComposition::setCompositionSize(const QSize& size)
{
    if (impl_->settings_.compositionSize() == size) {
        return;
    }
    impl_->settings_.setCompositionSize(size);
    for (auto& variant : impl_->responsiveLayout_.variants) {
        if (variant.variantId == impl_->responsiveLayout_.activeVariantId) {
            variant.baseSize = size;
            variant.aspectRatio = size.height() > 0
                ? static_cast<qreal>(size.width()) / static_cast<qreal>(size.height())
                : 0.0;
            break;
        }
    }
    impl_->invalidateThumbnailCache();
    Q_EMIT changed();
}

bool ArtifactAbstractComposition::isVisual() const
{
 return true;
}

bool ArtifactAbstractComposition::isPlaying() const
{
 return impl_->isPlaying_;
}

void ArtifactAbstractComposition::play()
{
    impl_->isPlaying_ = true;
}

void ArtifactAbstractComposition::pause()
{
    impl_->isPlaying_ = false;
}

void ArtifactAbstractComposition::stop()
{
    impl_->isPlaying_ = false;
    goToStartFrame();
}

void ArtifactAbstractComposition::togglePlayPause()
{
    impl_->isPlaying_ = !impl_->isPlaying_;
}

float ArtifactAbstractComposition::playbackSpeed() const
{
    return impl_->playbackSpeed_;
}

void ArtifactAbstractComposition::setPlaybackSpeed(float speed)
{
    impl_->playbackSpeed_ = speed;
}

bool ArtifactAbstractComposition::isLooping() const
{
    return impl_->looping_;
}

void ArtifactAbstractComposition::setLooping(bool loop)
{
    impl_->looping_ = loop;
}

void ArtifactAbstractComposition::setFrameRange(const FrameRange& range)
{
    const FrameRange normalized = range.normalized();
    if (impl_->frameRange_ == normalized) {
        return;
    }
    impl_->frameRange_ = normalized;
    impl_->workAreaRange_.clip(impl_->frameRange_);
    if (!impl_->workAreaRange_.isValid() || impl_->workAreaRange_.isEmpty()) {
        impl_->workAreaRange_ = impl_->frameRange_;
    }
    Q_EMIT changed();
}

FrameRange ArtifactAbstractComposition::workAreaRange() const
{
    return impl_->workAreaRange_;
}

void ArtifactAbstractComposition::setWorkAreaRange(const FrameRange& range)
{
    FrameRange normalized = range.normalized();
    normalized.clip(impl_->frameRange_);
    if (!normalized.isValid() || normalized.isEmpty()) {
        normalized = impl_->frameRange_;
    }
    if (impl_->workAreaRange_ == normalized) {
        return;
    }
    impl_->workAreaRange_ = normalized;
    Q_EMIT changed();
}

void ArtifactAbstractComposition::setFrameRate(const FrameRate& rate)
{
    if (impl_->frameRate_ == rate) {
        return;
    }
    impl_->frameRate_ = rate;
    Q_EMIT changed();
}

ResponsiveLayoutSet ArtifactAbstractComposition::responsiveLayout() const
{
    return impl_->responsiveLayout_;
}

void ArtifactAbstractComposition::setResponsiveLayout(const ResponsiveLayoutSet& layout)
{
    ResponsiveLayoutSet normalized = layout;
    if (normalized.variants.isEmpty()) {
        normalized = makeDefaultResponsiveLayoutSet(impl_->settings_.compositionSize());
    }
    if (normalized.activeVariantId.isEmpty() || !normalized.hasVariant(normalized.activeVariantId)) {
        normalized.activeVariantId = normalized.variants.front().variantId;
    }
    impl_->responsiveLayout_ = normalized;

    for (const auto& variant : impl_->responsiveLayout_.variants) {
        if (variant.variantId == impl_->responsiveLayout_.activeVariantId && variant.baseSize.isValid()) {
            impl_->settings_.setCompositionSize(variant.baseSize);
            break;
        }
    }
    impl_->invalidateThumbnailCache();
    Q_EMIT changed();
}

QString ArtifactAbstractComposition::activeResponsiveLayoutVariantId() const
{
    return impl_->responsiveLayout_.activeVariantId;
}

void ArtifactAbstractComposition::setActiveResponsiveLayoutVariantId(const QString& variantId)
{
    if (variantId.isEmpty() || impl_->responsiveLayout_.activeVariantId == variantId) {
        return;
    }
    if (!impl_->responsiveLayout_.hasVariant(variantId)) {
        return;
    }
    impl_->responsiveLayout_.activeVariantId = variantId;
    for (const auto& variant : impl_->responsiveLayout_.variants) {
        if (variant.variantId == variantId && variant.baseSize.isValid()) {
            impl_->settings_.setCompositionSize(variant.baseSize);
            break;
        }
    }
    impl_->invalidateThumbnailCache();
    Q_EMIT changed();
}

QVector<ResponsiveLayoutVariant> ArtifactAbstractComposition::responsiveLayoutVariants() const
{
    return impl_->responsiveLayout_.variants;
}

QSize ArtifactAbstractComposition::effectiveCompositionSize() const
{
    for (const auto& variant : impl_->responsiveLayout_.variants) {
        if (variant.variantId == impl_->responsiveLayout_.activeVariantId && variant.baseSize.isValid()) {
            return variant.baseSize;
        }
    }
    return impl_->settings_.compositionSize();
}

QVector<CompositionTransformField> ArtifactAbstractComposition::transformFields() const
{
    return impl_->transformFields_;
}

void ArtifactAbstractComposition::setTransformFields(
    const QVector<CompositionTransformField>& fields)
{
    impl_->transformFields_ = fields;
    for (const auto& layer : impl_->layerMultiIndex_.all()) {
        if (layer) {
            layer->setDirty(LayerDirtyFlag::Transform);
            layer->changed();
        }
    }
    Q_EMIT changed();
}

void ArtifactAbstractComposition::addTransformField(
    const CompositionTransformField& field)
{
    if (field.fieldId.isEmpty()) {
        return;
    }
    for (auto& existing : impl_->transformFields_) {
        if (existing.fieldId == field.fieldId) {
            existing = field;
            for (const auto& layerId : field.targetLayerIds) {
                if (const auto layer = layerById(layerId)) {
                    layer->setDirty(LayerDirtyFlag::Transform);
                    layer->changed();
                }
            }
            Q_EMIT changed();
            return;
        }
    }
    impl_->transformFields_.append(field);
    for (const auto& layerId : field.targetLayerIds) {
        if (const auto layer = layerById(layerId)) {
            layer->setDirty(LayerDirtyFlag::Transform);
            layer->changed();
        }
    }
    Q_EMIT changed();
}

bool ArtifactAbstractComposition::removeTransformField(const QString& fieldId)
{
    for (qsizetype index = 0; index < impl_->transformFields_.size(); ++index) {
        if (impl_->transformFields_.at(index).fieldId != fieldId) {
            continue;
        }
        const auto targetLayerIds = impl_->transformFields_.at(index).targetLayerIds;
        impl_->transformFields_.removeAt(index);
        for (const auto& layerId : targetLayerIds) {
            if (const auto layer = layerById(layerId)) {
                layer->setDirty(LayerDirtyFlag::Transform);
                layer->changed();
            }
        }
        Q_EMIT changed();
        return true;
    }
    return false;
}

void ArtifactAbstractComposition::clearTransformFields()
{
    if (impl_->transformFields_.isEmpty()) {
        return;
    }
    impl_->transformFields_.clear();
    for (const auto& layer : impl_->layerMultiIndex_.all()) {
        if (layer) {
            layer->setDirty(LayerDirtyFlag::Transform);
            layer->changed();
        }
    }
    Q_EMIT changed();
}

CompositionFieldTransformAdjustment
ArtifactAbstractComposition::evaluateTransformFields(
    const LayerID& layerId, const QPointF& basePosition) const
{
    CompositionFieldTransformAdjustment adjustment;
    for (const auto& field : impl_->transformFields_) {
        if (!field.enabled || field.radius <= 0.0001 ||
            !field.targetsLayer(layerId)) {
            continue;
        }
        const QPointF delta = basePosition - field.center;
        const qreal normalizedDistance = std::clamp<qreal>(
            std::hypot(delta.x(), delta.y()) / field.radius, 0.0, 1.0);
        const qreal influence = normalizedDistance * normalizedDistance *
                                (3.0 - 2.0 * normalizedDistance);
        adjustment.positionOffset += delta * field.expansion * influence;
        adjustment.scaleMultiplier *=
            std::lerp<qreal>(1.0, field.edgeScale, influence);
        adjustment.affected = true;
    }
    return adjustment;
}

bool ArtifactAbstractComposition::applyExternalControlValue(
    const QString& address,
    double rawValue,
    bool resetSmoothing)
{
    if (address.trimmed().isEmpty()) {
        return false;
    }

    auto& controlManager = ArtifactCore::ExternalControlManager::instance();
    const auto mapping = controlManager.getMappingDefinition(address);
    if (!mapping.has_value() || !mapping->enabled) {
        return false;
    }

    const auto processedValue =
        controlManager.processIncomingValue(address, rawValue, resetSmoothing);
    if (!processedValue.has_value()) {
        return false;
    }

    const auto targetLayer = layerById(mapping->target.layerId);
    if (!targetLayer) {
        return false;
    }
    auto& recording = impl_->liveControlRecording_;
    const bool shouldConsiderRecording =
        recording.active &&
        (recording.options.addresses.isEmpty() ||
         recording.options.addresses.contains(address));
    const auto property = shouldConsiderRecording
        ? targetLayer->getProperty(mapping->target.propertyPath)
        : nullptr;
    const bool shouldRecord =
        shouldConsiderRecording && property && property->isAnimatable();

    const QString snapshotKey =
        shouldRecord
            ? recordingSnapshotKey(mapping->target.layerId, mapping->target.propertyPath)
            : QString();
    if (shouldRecord && !recording.snapshotsByAddress.contains(snapshotKey)) {
        Impl::RecordedPropertySnapshot snapshot;
        snapshot.layerId = mapping->target.layerId;
        snapshot.propertyPath = mapping->target.propertyPath;
        snapshot.value = property->getValue();
        snapshot.keyframes = property->getKeyFrames();
        recording.snapshotsByAddress.insert(snapshotKey, snapshot);
    }

    const bool applied = targetLayer->setLayerPropertyValue(
        mapping->target.propertyPath, QVariant(processedValue.value()));
    if (!applied) {
        return false;
    }

    if (!shouldRecord) {
        return true;
    }

    const qint64 currentFrame = framePosition().framePosition();
    const int frameStride = std::max(1, recording.options.sampleEveryNFrames);
    auto lastFrameIt = recording.lastRecordedFrameByAddress.constFind(address);
    if (lastFrameIt != recording.lastRecordedFrameByAddress.constEnd() &&
        std::abs(currentFrame - lastFrameIt.value()) < frameStride) {
        return true;
    }

    auto lastValueIt = recording.lastRecordedValueByAddress.constFind(address);
    if (lastValueIt != recording.lastRecordedValueByAddress.constEnd() &&
        std::abs(processedValue.value() - lastValueIt.value()) < recording.options.deadZone) {
        return true;
    }

    const RationalTime keyTime(
        currentFrame,
        static_cast<int64_t>(std::max<double>(
            1.0, static_cast<double>(frameRate().framerate()))));
    property->addKeyFrame(keyTime, QVariant(processedValue.value()));
    recording.lastRecordedFrameByAddress.insert(address, currentFrame);
    recording.lastRecordedValueByAddress.insert(address, processedValue.value());
    return true;
}

bool ArtifactAbstractComposition::applyAudioAnalysis(
    const ArtifactCore::AudioAnalyzer::AnalysisResult& analysis,
    const QString& addressPrefix,
    bool resetSmoothing)
{
    const QString prefix = addressPrefix.trimmed().isEmpty()
        ? QStringLiteral("audio")
        : addressPrefix.trimmed();

    bool applied = false;
    applied = applyExternalControlValue(prefix + QStringLiteral(".amplitude"),
                                        analysis.rms,
                                        resetSmoothing) || applied;
    applied = applyExternalControlValue(prefix + QStringLiteral(".peak"),
                                        analysis.peak,
                                        resetSmoothing) || applied;
    applied = applyExternalControlValue(prefix + QStringLiteral(".low"),
                                        analysis.lowIntensity,
                                        resetSmoothing) || applied;
    applied = applyExternalControlValue(prefix + QStringLiteral(".mid"),
                                        analysis.midIntensity,
                                        resetSmoothing) || applied;
    applied = applyExternalControlValue(prefix + QStringLiteral(".high"),
                                        analysis.highIntensity,
                                        resetSmoothing) || applied;
    return applied;
}

bool ArtifactAbstractComposition::beginLiveControlRecording(
    const LiveControlRecordingOptions& options)
{
    if (impl_->liveControlRecording_.active) {
        return false;
    }
    impl_->liveControlRecording_.active = true;
    impl_->liveControlRecording_.options = options;
    impl_->liveControlRecording_.options.sampleEveryNFrames =
        std::max(1, options.sampleEveryNFrames);
    impl_->liveControlRecording_.options.deadZone =
        std::max(0.0, options.deadZone);
    impl_->liveControlRecording_.lastRecordedFrameByAddress.clear();
    impl_->liveControlRecording_.lastRecordedValueByAddress.clear();
    impl_->liveControlRecording_.snapshotsByAddress.clear();
    Q_EMIT changed();
    return true;
}

bool ArtifactAbstractComposition::isLiveControlRecordingActive() const
{
    return impl_->liveControlRecording_.active;
}

LiveControlRecordingOptions ArtifactAbstractComposition::liveControlRecordingOptions() const
{
    return impl_->liveControlRecording_.options;
}

void ArtifactAbstractComposition::commitLiveControlRecording()
{
    if (!impl_->liveControlRecording_.active) {
        return;
    }
    impl_->liveControlRecording_ = Impl::LiveControlRecordingState{};
    Q_EMIT changed();
}

void ArtifactAbstractComposition::cancelLiveControlRecording()
{
    if (!impl_->liveControlRecording_.active) {
        return;
    }
    const auto recording = impl_->liveControlRecording_;
    if (recording.options.restoreOnCancel) {
        for (auto it = recording.snapshotsByAddress.constBegin();
             it != recording.snapshotsByAddress.constEnd();
             ++it) {
            const auto layer = layerById(it.value().layerId);
            if (!layer) {
                continue;
            }
            const auto property = layer->getProperty(it.value().propertyPath);
            if (!property) {
                continue;
            }
            property->clearKeyFrames();
            for (const auto& keyframe : it.value().keyframes) {
                property->addKeyFrame(keyframe.time,
                                      keyframe.value,
                                      keyframe.interpolation,
                                      keyframe.cp1_x,
                                      keyframe.cp1_y,
                                      keyframe.cp2_x,
                                      keyframe.cp2_y,
                                      keyframe.roving);
                property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
                property->setKeyFrameColorLabelAt(keyframe.time,
                                                  keyframe.colorLabel);
            }
            property->setValue(it.value().value);
        }
    }
    impl_->liveControlRecording_ = Impl::LiveControlRecordingState{};
    Q_EMIT changed();
}

QVector<CompositionStateVariant> ArtifactAbstractComposition::stateVariants() const
{
    return impl_->stateVariants_;
}

void ArtifactAbstractComposition::setStateVariants(
    const QVector<CompositionStateVariant>& states)
{
    QVector<CompositionStateVariant> normalizedStates;
    normalizedStates.reserve(states.size());
    for (auto state : states) {
        state.stateId = uniqueStateVariantId(normalizedStates, state.stateId);
        if (state.displayName.trimmed().isEmpty()) {
            state.displayName = state.stateId;
        }
        normalizedStates.append(state);
    }
    impl_->stateVariants_ = normalizedStates;
    if (!impl_->activeStateVariantId_.isEmpty() &&
        findStateVariantById(impl_->stateVariants_, impl_->activeStateVariantId_) == nullptr) {
        impl_->activeStateVariantId_.clear();
    }
    Q_EMIT changed();
}

void ArtifactAbstractComposition::addStateVariant(
    const CompositionStateVariant& state)
{
    CompositionStateVariant normalized = state;
    normalized.stateId = uniqueStateVariantId(impl_->stateVariants_, normalized.stateId);
    if (normalized.displayName.trimmed().isEmpty()) {
        normalized.displayName = normalized.stateId;
    }
    impl_->stateVariants_.append(normalized);
    Q_EMIT changed();
}

bool ArtifactAbstractComposition::removeStateVariant(const QString& stateId)
{
    for (qsizetype index = 0; index < impl_->stateVariants_.size(); ++index) {
        if (impl_->stateVariants_.at(index).stateId != stateId) {
            continue;
        }
        impl_->stateVariants_.removeAt(index);
        if (impl_->activeStateVariantId_ == stateId) {
            impl_->activeStateVariantId_.clear();
        }
        Q_EMIT changed();
        return true;
    }
    return false;
}

QString ArtifactAbstractComposition::activeStateVariantId() const
{
    return impl_->activeStateVariantId_;
}

bool ArtifactAbstractComposition::setActiveStateVariantId(const QString& stateId)
{
    const QString trimmedStateId = stateId.trimmed();
    if (trimmedStateId == impl_->activeStateVariantId_) {
        return true;
    }

    const CompositionStateVariant* nextState =
        trimmedStateId.isEmpty() ? nullptr
                                 : findStateVariantById(impl_->stateVariants_,
                                                        trimmedStateId);
    if (!trimmedStateId.isEmpty() && (!nextState || !nextState->enabled)) {
        return false;
    }

    const CompositionStateVariant* previousState =
        impl_->activeStateVariantId_.isEmpty()
            ? nullptr
            : findStateVariantById(impl_->stateVariants_, impl_->activeStateVariantId_);

    if (previousState) {
        for (const auto& previousOverride : previousState->overrides) {
            if (!previousOverride.enabled || !previousOverride.baselineValue.isValid()) {
                continue;
            }
            if (nextState &&
                nextState->hasOverride(previousOverride.layerId,
                                       previousOverride.propertyPath)) {
                continue;
            }
            const auto layer = layerById(previousOverride.layerId);
            if (layer) {
                layer->setLayerPropertyValue(previousOverride.propertyPath,
                                             previousOverride.baselineValue);
            }
        }
    }

    if (nextState) {
        for (const auto& overrideItem : nextState->overrides) {
            if (!overrideItem.enabled) {
                continue;
            }
            const auto layer = layerById(overrideItem.layerId);
            if (layer) {
                layer->setLayerPropertyValue(overrideItem.propertyPath,
                                             overrideItem.value);
            }
        }
    }

    impl_->activeStateVariantId_ = trimmedStateId;
    impl_->invalidateThumbnailCache();
    Q_EMIT changed();
    return true;
}

QJsonDocument ArtifactAbstractComposition::toJson() const{
    QJsonObject obj;
    obj["id"] = id().toString();
    obj["frameRange"] = impl_->frameRange_.toJson();
    obj["workAreaRange"] = impl_->workAreaRange_.toJson();
    obj["currentFrame"] = impl_->position_.framePosition();
    obj["playbackSpeed"] = impl_->playbackSpeed_;
    obj["looping"] = impl_->looping_;
    obj["isPlaying"] = impl_->isPlaying_;
    obj["name"] = impl_->settings_.compositionName().toQString();
    obj["compositionNote"] = impl_->compositionNote_;
    obj["width"] = impl_->settings_.compositionSize().width();
    obj["height"] = impl_->settings_.compositionSize().height();
    QJsonObject backgroundColorObj;
    backgroundColorObj["r"] = impl_->backgroundColor_.r();
    backgroundColorObj["g"] = impl_->backgroundColor_.g();
    backgroundColorObj["b"] = impl_->backgroundColor_.b();
    backgroundColorObj["a"] = impl_->backgroundColor_.a();
    obj["backgroundColor"] = backgroundColorObj;
    obj["responsiveLayout"] = impl_->responsiveLayout_.toJson();
    obj["activeStateVariantId"] = impl_->activeStateVariantId_;
    QJsonArray stateVariantsArray;
    for (const auto& state : impl_->stateVariants_) {
        stateVariantsArray.append(state.toJson());
    }
    obj["stateVariants"] = stateVariantsArray;
    QJsonArray transformFieldsArray;
    for (const auto& field : impl_->transformFields_) {
        transformFieldsArray.append(field.toJson());
    }
    obj["transformFields"] = transformFieldsArray;
    QJsonArray effectsArray;
    for (const auto& effect : impl_->effects_) {
        if (effect) {
            effectsArray.append(serializeEffect(effect));
        }
    }
    obj["effects"] = effectsArray;
    QJsonArray layersArray;
    for (const auto& layer : impl_->layerMultiIndex_.all()) {
        if (layer) {
            layersArray.append(layer->toJson());
        }
    }
    obj["layers"] = layersArray;
    // 必要に応じて他のプロパティも追加可能
    return QJsonDocument(obj);
}

void ArtifactAbstractComposition::removeLayerById(const ArtifactCore::LayerID& id)
{
    removeLayer(id);
}

ArtifactCompositionPtr ArtifactAbstractComposition::fromJson(const QJsonDocument& doc){
    if (!doc.isObject()) return nullptr;
    QJsonObject obj = doc.object();
    
    CompositionID compId;
    if (obj.contains("id")) {
        compId = CompositionID(obj["id"].toString());
    }
    
    ArtifactCompositionInitParams params;
    if (obj.contains("name")) {
        params.setCompositionName(obj["name"].toString());
    }
    if (obj.contains("width") && obj.contains("height")) {
        params.setResolution(obj["width"].toInt(), obj["height"].toInt());
    }
    if (obj.contains("backgroundColor") && obj["backgroundColor"].isObject()) {
        const QJsonObject backgroundColorObj = obj["backgroundColor"].toObject();
        params.setBackgroundColor(FloatColor{
            static_cast<float>(backgroundColorObj["r"].toDouble()),
            static_cast<float>(backgroundColorObj["g"].toDouble()),
            static_cast<float>(backgroundColorObj["b"].toDouble()),
            static_cast<float>(backgroundColorObj["a"].toDouble(1.0))
        });
    }
    auto comp = ArtifactCore::makeShared<ArtifactAbstractComposition>(compId, params);
    if (obj.contains("frameRange") && obj["frameRange"].isObject()) {
        comp->setFrameRange(FrameRange::fromJson(obj["frameRange"].toObject()));
    }
    if (obj.contains("workAreaRange") && obj["workAreaRange"].isObject()) {
        comp->setWorkAreaRange(FrameRange::fromJson(obj["workAreaRange"].toObject()));
    }
    if (obj.contains("compositionNote")) {
        comp->setCompositionNote(obj["compositionNote"].toString());
    }
    if (obj.contains("playbackSpeed")) {
        comp->setPlaybackSpeed(static_cast<float>(obj["playbackSpeed"].toDouble(1.0)));
    }
    if (obj.contains("looping")) {
        comp->setLooping(obj["looping"].toBool(false));
    }
    if (obj.contains("responsiveLayout") && obj["responsiveLayout"].isObject()) {
        comp->impl_->responsiveLayout_ = ResponsiveLayoutSet::fromJson(obj["responsiveLayout"].toObject());
        for (const auto& variant : comp->impl_->responsiveLayout_.variants) {
            if (variant.variantId == comp->impl_->responsiveLayout_.activeVariantId && variant.baseSize.isValid()) {
                comp->impl_->settings_.setCompositionSize(variant.baseSize);
                break;
            }
        }
    } else {
        comp->impl_->responsiveLayout_ = makeDefaultResponsiveLayoutSet(comp->impl_->settings_.compositionSize());
    }
    if (obj.contains("stateVariants") && obj["stateVariants"].isArray()) {
        const QJsonArray stateVariantsArray = obj["stateVariants"].toArray();
        for (const auto& value : stateVariantsArray) {
            if (value.isObject()) {
                comp->impl_->stateVariants_.append(
                    CompositionStateVariant::fromJson(value.toObject()));
            }
        }
    }
    comp->impl_->activeStateVariantId_ =
        obj.value("activeStateVariantId").toString();
    if (!comp->impl_->activeStateVariantId_.isEmpty() &&
        findStateVariantById(comp->impl_->stateVariants_,
                             comp->impl_->activeStateVariantId_) == nullptr) {
        comp->impl_->activeStateVariantId_.clear();
    }
    if (obj.contains("transformFields") && obj["transformFields"].isArray()) {
        const QJsonArray transformFieldsArray = obj["transformFields"].toArray();
        for (const auto& value : transformFieldsArray) {
            if (!value.isObject()) {
                continue;
            }
            const auto field = CompositionTransformField::fromJson(value.toObject());
            if (!field.fieldId.isEmpty()) {
                comp->impl_->transformFields_.append(field);
            }
        }
    }

    if (obj.contains("effects") && obj["effects"].isArray()) {
        const QJsonArray effectsArray = obj["effects"].toArray();
        for (const auto& value : effectsArray) {
            if (!value.isObject()) {
                continue;
            }
            comp->addEffect(deserializeEffect(value.toObject()));
        }
    }

    if (obj.contains("layers") && obj["layers"].isArray()) {
        QJsonArray arr = obj["layers"].toArray();
        QVector<ArtifactAbstractLayerPtr> loadedLayers;
        for (const auto& v : arr) {
            if (v.isObject()) {
                ArtifactAbstractLayerPtr layer = createArtifactLayerFromJson(v.toObject());
                if (layer) {
                    comp->appendLayerTop(layer);
                    loadedLayers.append(layer);
                }
            }
        }

        // Parent resolution pass
        for (int i = 0; i < loadedLayers.size() && i < arr.size(); ++i) {
            const auto& layer = loadedLayers.at(i);
            QJsonObject lobj = arr.at(i).toObject();
            if (lobj.contains("parentId")) {
                LayerID pid(lobj["parentId"].toString());
                layer->setParentById(pid);
            }
        }
    }
    const int64_t restoredFrame = obj.contains("currentFrame")
        ? obj["currentFrame"].toVariant().toLongLong()
        : comp->frameRange().start();
    comp->goToFrame(comp->frameRange().clampFrame(restoredFrame));
    if (obj.contains("isPlaying")) {
        if (obj["isPlaying"].toBool(false)) {
            comp->play();
        } else {
            comp->pause();
        }
    }
    return comp;
}

QVector<ArtifactCore::AssetID> ArtifactAbstractComposition::getUsedAssets() const
{
  return impl_->getUsedAssets();
}

void ArtifactAbstractComposition::applyResolutionRemap(const QSize& newSize, RemapPolicy policy)
{
    const QSize oldSize = impl_->settings_.compositionSize();
    if (oldSize == newSize) return;

    setCompositionSize(newSize);

    const auto& layers = allLayerRef();
    for (const auto& layer : layers) {
        if (!layer) continue;

        // Remap mask vertex positions
        if (layer->hasMasks()) {
            for (int mi = 0; mi < layer->maskCount(); ++mi) {
                auto lm = layer->mask(mi);
                for (int pi = 0; pi < lm.maskPathCount(); ++pi) {
                    auto path = lm.maskPath(pi);
                    const int vc = path.vertexCount();
                    for (int vi = 0; vi < vc; ++vi) {
                        auto v = path.vertex(vi);
                        v.position = ResolutionRemap::remapPosition(
                            v.position, oldSize, newSize, policy);
                        v.inTangent = ResolutionRemap::remapPosition(
                            v.inTangent, oldSize, newSize, policy);
                        v.outTangent = ResolutionRemap::remapPosition(
                            v.outTangent, oldSize, newSize, policy);
                        path.setVertex(vi, v);
                    }
                    // Remap animation keyframe snapshots
                    if (path.hasAnimationKeyframes()) {
                        auto snaps = path.animationKeyframes();
                        for (auto& snap : snaps) {
                            for (auto& sv : snap.vertices) {
                                sv.position = ResolutionRemap::remapPosition(
                                    sv.position, oldSize, newSize, policy);
                                sv.inTangent = ResolutionRemap::remapPosition(
                                    sv.inTangent, oldSize, newSize, policy);
                                sv.outTangent = ResolutionRemap::remapPosition(
                                    sv.outTangent, oldSize, newSize, policy);
                            }
                            path.clearAnimationKeyframes();
                            path.setAnimationKeyframe(snap.frame, snap);
                        }
                    }
                    lm.setMaskPath(pi, path);
                }
                layer->setMask(mi, lm);
            }
        }

        // Remap transform property keyframes (position / anchor / scale).
        // rotation は角度で aspect 非依存のため対象外。rotation / opacity はそのまま。
        remapLayerTransformProperties(layer, oldSize, newSize, policy);
    }
}

QImage ArtifactAbstractComposition::getThumbnail(int width, int height) const
{
    const int safeWidth = std::max(1, width);
    const int safeHeight = std::max(1, height);
    const QSize targetSize(safeWidth, safeHeight);

    if (impl_->thumbnailCacheValid_ &&
        impl_->thumbnailCacheSize_ == targetSize &&
        !impl_->thumbnailCache_.isNull()) {
        return impl_->thumbnailCache_;
    }

    const auto layers = impl_->allLayerBackToFront();
    for (const auto& layer : layers) {
        if (!layer) {
            continue;
        }
        const QImage thumbnail = layer->getThumbnail(safeWidth, safeHeight);
        if (!thumbnail.isNull()) {
            QImage composedThumbnail = thumbnail;
            applyCompositionFinalEffectsToImage(const_cast<ArtifactAbstractComposition*>(this),
                                               composedThumbnail);
            impl_->thumbnailCache_ = composedThumbnail;
            impl_->thumbnailCacheSize_ = targetSize;
            impl_->thumbnailCacheValid_ = true;
            return composedThumbnail;
        }
    }

    impl_->thumbnailCache_ = QImage(safeWidth, safeHeight,
                                    QImage::Format_ARGB32_Premultiplied);
    impl_->thumbnailCache_.fill(QColor(24, 24, 24, 255));
    impl_->thumbnailCacheSize_ = targetSize;
    impl_->thumbnailCacheValid_ = true;
    QImage fallback = impl_->thumbnailCache_;
    applyCompositionFinalEffectsToImage(const_cast<ArtifactAbstractComposition*>(this),
                                       fallback);
    return fallback;
}

QImage ArtifactAbstractComposition::getThumbnailAtFrame(int64_t frameNumber,
                                                       int width, int height) {
    const int safeWidth = std::max(1, width);
    const int safeHeight = std::max(1, height);

    // Seek the composition to the requested time so each layer samples its
    // own state at that frame. This deliberately bypasses the cross-frame
    // thumbnail cache used by getThumbnail(), because the cached entry is tied
    // to a single representative frame and must not be reused for other times.
    goToFrame(frameNumber);

    const auto layers = impl_->allLayerBackToFront();
    for (const auto& layer : layers) {
        if (!layer) {
            continue;
        }
        const QImage thumbnail = layer->getThumbnail(safeWidth, safeHeight);
        if (!thumbnail.isNull()) {
            QImage composedThumbnail = thumbnail;
            applyCompositionFinalEffectsToImage(this, composedThumbnail);
            return composedThumbnail;
        }
    }

    QImage fallback(safeWidth, safeHeight, QImage::Format_ARGB32_Premultiplied);
    fallback.fill(QColor(24, 24, 24, 255));
    return fallback;
}

};


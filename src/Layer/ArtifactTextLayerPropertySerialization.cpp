module;
#include <array>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

module Artifact.Layer.Text;
import Memory.SharedPtr;
import Property.Abstract;
import Property.SerializationBridge;

namespace Artifact {
using namespace ArtifactCore;

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
         suffix == QStringLiteral("scaleX") ||
         suffix == QStringLiteral("scaleY") ||
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

const std::array<QString, 23> &animatableTextAnimatorPropertySuffixes() {
  static const std::array<QString, 23> suffixes{
      QStringLiteral("start"), QStringLiteral("end"),
      QStringLiteral("offset"), QStringLiteral("wigglesPerSecond"),
      QStringLiteral("correlation"), QStringLiteral("phase"),
      QStringLiteral("seed"), QStringLiteral("positionX"),
      QStringLiteral("positionY"), QStringLiteral("scale"),
      QStringLiteral("scaleX"), QStringLiteral("scaleY"),
      QStringLiteral("rotation"), QStringLiteral("opacity"),
      QStringLiteral("skew"), QStringLiteral("tracking"),
      QStringLiteral("z"), QStringLiteral("fillColor"),
      QStringLiteral("strokeColor"), QStringLiteral("strokeWidth"),
      QStringLiteral("strokeEnabled"), QStringLiteral("colorEnabled"),
      QStringLiteral("blur")};
  return suffixes;
}

QJsonObject serializedAnimatorProperties(const ArtifactTextLayer *layer,
                                         const int animatorIndex) {
  QJsonObject result;
  if (!layer || animatorIndex < 0) return result;
  const QString prefix = QStringLiteral("text.animators.%1.").arg(animatorIndex);
  for (const QString &suffix : animatableTextAnimatorPropertySuffixes()) {
    const auto property = layer->getProperty(prefix + suffix);
    if (!property) continue;
    const auto serialized =
        PropertySerializationBridge::serializeProperty(property);
    if (serialized.expression.isEmpty() && serialized.keyframes.isEmpty() &&
        serialized.envelopes.isEmpty()) {
      continue;
    }
    QJsonObject propertyObject;
    propertyObject[QStringLiteral("type")] = serialized.type;
    propertyObject[QStringLiteral("value")] = serialized.value;
    if (!serialized.expression.isEmpty()) {
      propertyObject[QStringLiteral("expression")] = serialized.expression;
    }
    if (!serialized.keyframes.isEmpty()) {
      propertyObject[QStringLiteral("keyframes")] = serialized.keyframes;
    }
    if (!serialized.envelopes.isEmpty()) {
      propertyObject[QStringLiteral("envelopes")] = serialized.envelopes;
    }
    result[suffix] = propertyObject;
  }
  return result;
}

void restoreAnimatorProperties(ArtifactTextLayer *layer, const int animatorIndex,
                               const QJsonObject &serializedProperties,
                               const int removedAnimatorIndex) {
  if (!layer || animatorIndex < 0 || serializedProperties.isEmpty()) return;
  const QString prefix = QStringLiteral("text.animators.%1.").arg(animatorIndex);
  for (auto it = serializedProperties.constBegin();
       it != serializedProperties.constEnd(); ++it) {
    if (!it.value().isObject() ||
        !isAnimatorPropertyAnimatable(it.key())) {
      continue;
    }
    auto property = layer->getProperty(prefix + it.key());
    if (!property) continue;
    const QJsonObject propertyObject = it.value().toObject();
    SerializedProperty serialized;
    serialized.name = prefix + it.key();
    serialized.type = propertyObject.value(QStringLiteral("type")).toInt(
        static_cast<int>(property->getType()));
    serialized.value = propertyObject.value(QStringLiteral("value"));
    serialized.expression =
        propertyObject.value(QStringLiteral("expression"))
            .toString().trimmed().left(16384);
    serialized.keyframes =
        propertyObject.value(QStringLiteral("keyframes")).toArray();
    serialized.envelopes =
        propertyObject.value(QStringLiteral("envelopes")).toArray();
    if (removedAnimatorIndex >= 0 && !serialized.envelopes.isEmpty()) {
      static const QString animatorPrefix = QStringLiteral("text.animators.");
      for (qsizetype envelopeIndex = 0;
           envelopeIndex < serialized.envelopes.size(); ++envelopeIndex) {
        QJsonObject envelope =
            serialized.envelopes.at(envelopeIndex).toObject();
        const QString targetPath =
            envelope.value(QStringLiteral("targetPropertyPath")).toString();
        if (!targetPath.startsWith(animatorPrefix)) continue;
        const QString remainder = targetPath.mid(animatorPrefix.size());
        const int dot = remainder.indexOf(QLatin1Char('.'));
        if (dot <= 0) continue;
        bool ok = false;
        const int targetAnimatorIndex = remainder.left(dot).toInt(&ok);
        if (!ok) continue;
        if (targetAnimatorIndex == removedAnimatorIndex) {
          envelope[QStringLiteral("enabled")] = false;
        } else if (targetAnimatorIndex > removedAnimatorIndex) {
          envelope[QStringLiteral("targetPropertyPath")] =
              QStringLiteral("text.animators.%1.%2")
                  .arg(targetAnimatorIndex - 1)
                  .arg(remainder.mid(dot + 1));
        }
        serialized.envelopes[envelopeIndex] = envelope;
      }
    }
    PropertySerializationBridge::deserializeProperty(property, serialized);
  }
}


} // namespace Artifact

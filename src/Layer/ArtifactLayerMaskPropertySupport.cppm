#include <QString>
#include <QVariant>

import Artifact.Layer.Abstract;
import Artifact.Layer.RuntimeSupport;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Property.Abstract;
import Time.Rational;

// Qt preprocessor macros are not carried by an imported IFC.
#define QStringLiteral(text) QString::fromUtf8(text)

namespace Artifact {

RationalTime currentTimelineTime(const ArtifactAbstractLayer* layer);

void applyMaskPropertyState(const ArtifactAbstractLayer* layer, int maskIndex,
                            LayerMask& mask) {
  if (!layer) return;
  const QString maskPrefix = maskPropertyPrefix(maskIndex);
  if (!layer->hasCachedAnimatedPropertiesWithPrefix(
          maskPrefix + QStringLiteral("."))) {
    return;
  }
  const RationalTime time = currentTimelineTime(layer);
  const auto resolveBool = [layer, time](const QString& propertyPath, bool fallback) {
    const auto property = layer->getProperty(propertyPath);
    if (!property) return fallback;
    const QVariant value = property->evaluateValue(time);
    return value.isValid() ? value.toBool() : fallback;
  };
  const auto resolveInt = [layer, time](const QString& propertyPath, int fallback) {
    const auto property = layer->getProperty(propertyPath);
    if (!property) return fallback;
    const QVariant value = property->evaluateValue(time);
    return value.isValid() ? value.toInt() : fallback;
  };
  const auto resolveDouble = [layer, time](const QString& propertyPath, double fallback) {
    const auto property = layer->getProperty(propertyPath);
    if (!property) return fallback;
    const QVariant value = property->evaluateValue(time);
    return value.isValid() ? value.toDouble() : fallback;
  };
  const auto resolveString = [layer, time](const QString& propertyPath, const QString& fallback) {
    const auto property = layer->getProperty(propertyPath);
    if (!property) return fallback;
    const QVariant value = property->evaluateValue(time);
    return value.isValid() ? value.toString() : fallback;
  };

  mask.setEnabled(resolveBool(maskPrefix + QStringLiteral(".enabled"), mask.isEnabled()));
  for (int pathIndex = 0; pathIndex < mask.maskPathCount(); ++pathIndex) {
    MaskPath path = mask.maskPath(pathIndex);
    const QString pathPrefix = maskPathPropertyPrefix(maskIndex, pathIndex);
    path.setClosed(resolveBool(pathPrefix + QStringLiteral(".closed"), path.isClosed()));
    path.setOpacity(static_cast<float>(resolveDouble(pathPrefix + QStringLiteral(".opacity"), path.opacity())));
    path.setFeather(static_cast<float>(resolveDouble(pathPrefix + QStringLiteral(".feather"), path.feather())));
    path.setFeatherHorizontal(static_cast<float>(resolveDouble(pathPrefix + QStringLiteral(".featherHorizontal"), path.featherHorizontal())));
    path.setFeatherVertical(static_cast<float>(resolveDouble(pathPrefix + QStringLiteral(".featherVertical"), path.featherVertical())));
    path.setFeatherInner(static_cast<float>(resolveDouble(pathPrefix + QStringLiteral(".featherInner"), path.featherInner())));
    path.setFeatherOuter(static_cast<float>(resolveDouble(pathPrefix + QStringLiteral(".featherOuter"), path.featherOuter())));
    path.setFalloff(static_cast<MaskFeatherFalloff>(resolveInt(pathPrefix + QStringLiteral(".falloff"), static_cast<int>(path.falloff()))));
    path.setExpansion(static_cast<float>(resolveDouble(pathPrefix + QStringLiteral(".expansion"), path.expansion())));
    path.setInverted(resolveBool(pathPrefix + QStringLiteral(".inverted"), path.isInverted()));
    path.setMode(static_cast<MaskMode>(resolveInt(pathPrefix + QStringLiteral(".mode"), static_cast<int>(path.mode()))));
    path.setName(UniString(resolveString(pathPrefix + QStringLiteral(".name"), path.name().toQString()).toStdString()));
    mask.setMaskPath(pathIndex, path);
  }
}

} // namespace Artifact

#undef QStringLiteral

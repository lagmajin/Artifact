#include <QString>
#include <QVariant>

#include <utility>
#include <vector>

import Artifact.Layer.Abstract;
import Artifact.Layer.RuntimeSupport;
import Animation.Transform2D;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Property.Abstract;
import Property.Group;

// Qt preprocessor macros are not carried by an imported IFC.
#define QStringLiteral(text) QString::fromUtf8(text)

namespace Artifact {

void ArtifactAbstractLayer::append3DTransformProperties(
    ArtifactCore::PropertyGroup &transformGroup,
    const ArtifactCore::AnimatableTransform3D &transform,
    double positionRange, double anchorRange) const {
  using namespace ArtifactCore;

  auto posZProp = persistentLayerProperty(
      QStringLiteral("transform.position.z"), PropertyType::Float,
      transform.positionZ(), -293);
  posZProp->setDisplayLabel(QStringLiteral("Position Z"));
  posZProp->setUnit(QStringLiteral("px"));
  posZProp->setStep(1.0);
  posZProp->setSoftRange(-positionRange, positionRange);
  posZProp->setAnimatable(true);
  transformGroup.addProperty(posZProp);

  auto rotXProp = persistentLayerProperty(
      QStringLiteral("transform.rotation.x"), PropertyType::Float,
      transform.rotationX(), -292);
  rotXProp->setDisplayLabel(QStringLiteral("Rotation X"));
  rotXProp->setUnit(QStringLiteral("deg"));
  rotXProp->setStep(1.0);
  rotXProp->setSoftRange(-180.0, 180.0);
  rotXProp->setAnimatable(true);
  transformGroup.addProperty(rotXProp);

  auto rotYProp = persistentLayerProperty(
      QStringLiteral("transform.rotation.y"), PropertyType::Float,
      transform.rotationY(), -291);
  rotYProp->setDisplayLabel(QStringLiteral("Rotation Y"));
  rotYProp->setUnit(QStringLiteral("deg"));
  rotYProp->setStep(1.0);
  rotYProp->setSoftRange(-180.0, 180.0);
  rotYProp->setAnimatable(true);
  transformGroup.addProperty(rotYProp);

  auto scaleZProp = persistentLayerProperty(
      QStringLiteral("transform.scale.z"), PropertyType::Float,
      transform.scaleZ(), -289);
  scaleZProp->setDisplayLabel(QStringLiteral("Scale Z"));
  scaleZProp->setAnimatable(true);
  scaleZProp->setStep(0.01);
  scaleZProp->setSoftRange(0.0, 2.0);
  transformGroup.addProperty(scaleZProp);

  auto anchorZProp = persistentLayerProperty(
      QStringLiteral("transform.anchor.z"), PropertyType::Float,
      transform.anchorZ(), -288);
  anchorZProp->setDisplayLabel(QStringLiteral("Anchor Z"));
  anchorZProp->setUnit(QStringLiteral("px"));
  anchorZProp->setStep(1.0);
  anchorZProp->setSoftRange(-anchorRange, anchorRange);
  anchorZProp->setAnimatable(true);
  transformGroup.addProperty(anchorZProp);
}

void ArtifactAbstractLayer::appendMaskPropertyGroups(
    std::vector<ArtifactCore::PropertyGroup>& groups) const {
  using namespace ArtifactCore;
  auto makeProp = [this](const QString& name, PropertyType type,
                         const QVariant& value, int priority = 0) {
    return persistentLayerProperty(name, type, value, priority);
  };
  for (int maskIndex = 0; maskIndex < maskCount(); ++maskIndex) {
    const LayerMask resolvedMask = mask(maskIndex);
    PropertyGroup maskGroup(QStringLiteral("Mask %1").arg(maskIndex + 1));

    auto maskEnabledProp =
        makeProp(maskPropertyPrefix(maskIndex) + QStringLiteral(".enabled"),
                 PropertyType::Boolean, resolvedMask.isEnabled(),
                 -240 - maskIndex);
    maskEnabledProp->setAnimatable(true);
    maskEnabledProp->setDisplayLabel(QStringLiteral("Enabled"));
    maskGroup.addProperty(maskEnabledProp);

    auto maskLockedProp =
        makeProp(maskPropertyPrefix(maskIndex) + QStringLiteral(".locked"),
                 PropertyType::Boolean, resolvedMask.isLocked(),
                 -239 - maskIndex);
    maskLockedProp->setDisplayLabel(QStringLiteral("Locked"));
    maskGroup.addProperty(maskLockedProp);

    for (int pathIndex = 0; pathIndex < resolvedMask.maskPathCount();
         ++pathIndex) {
      const MaskPath path = resolvedMask.maskPath(pathIndex);
      const QString pathPrefix = maskPathPropertyPrefix(maskIndex, pathIndex);
      const QString pathLabel =
          QStringLiteral("Path %1").arg(pathIndex + 1);

      auto closedProp = makeProp(pathPrefix + QStringLiteral(".closed"),
                                 PropertyType::Boolean, path.isClosed(),
                                 -230 - pathIndex);
      closedProp->setAnimatable(true);
      closedProp->setDisplayLabel(pathLabel + QStringLiteral(" Closed"));
      maskGroup.addProperty(closedProp);

      auto opacityProp = makeProp(pathPrefix + QStringLiteral(".opacity"),
                                  PropertyType::Float,
                                  static_cast<double>(path.opacity()),
                                  -229 - pathIndex);
      opacityProp->setAnimatable(true);
      opacityProp->setHardRange(0.0, 1.0);
      opacityProp->setSoftRange(0.0, 1.0);
      opacityProp->setStep(0.01);
      opacityProp->setDisplayLabel(pathLabel + QStringLiteral(" Opacity"));
      maskGroup.addProperty(opacityProp);

      auto featherProp = makeProp(pathPrefix + QStringLiteral(".feather"),
                                  PropertyType::Float,
                                  static_cast<double>(path.feather()),
                                  -228 - pathIndex);
      featherProp->setAnimatable(true);
      featherProp->setSoftRange(0.0, 128.0);
      featherProp->setStep(0.5);
      featherProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather"));
      maskGroup.addProperty(featherProp);

      auto fhProp = makeProp(pathPrefix + QStringLiteral(".featherHorizontal"),
                             PropertyType::Float,
                             static_cast<double>(path.featherHorizontal()),
                             -232 - pathIndex);
      fhProp->setAnimatable(true);
      fhProp->setSoftRange(0.0, 128.0);
      fhProp->setStep(0.5);
      fhProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather H"));
      maskGroup.addProperty(fhProp);

      auto fvProp = makeProp(pathPrefix + QStringLiteral(".featherVertical"),
                             PropertyType::Float,
                             static_cast<double>(path.featherVertical()),
                             -233 - pathIndex);
      fvProp->setAnimatable(true);
      fvProp->setSoftRange(0.0, 128.0);
      fvProp->setStep(0.5);
      fvProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather V"));
      maskGroup.addProperty(fvProp);

      auto fiProp = makeProp(pathPrefix + QStringLiteral(".featherInner"),
                             PropertyType::Float,
                             static_cast<double>(path.featherInner()),
                             -234 - pathIndex);
      fiProp->setAnimatable(true);
      fiProp->setSoftRange(0.0, 128.0);
      fiProp->setStep(0.5);
      fiProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather Inner"));
      maskGroup.addProperty(fiProp);

      auto foProp = makeProp(pathPrefix + QStringLiteral(".featherOuter"),
                             PropertyType::Float,
                             static_cast<double>(path.featherOuter()),
                             -235 - pathIndex);
      foProp->setAnimatable(true);
      foProp->setSoftRange(0.0, 128.0);
      foProp->setStep(0.5);
      foProp->setDisplayLabel(pathLabel + QStringLiteral(" Feather Outer"));
      maskGroup.addProperty(foProp);

      auto falloffProp = makeProp(pathPrefix + QStringLiteral(".falloff"),
                                  PropertyType::Integer,
                                  static_cast<int>(path.falloff()),
                                  -236 - pathIndex);
      falloffProp->setAnimatable(false);
      falloffProp->setTooltip(
          QStringLiteral("0=Gaussian,1=Linear,2=Smooth,3=Sharp"));
      falloffProp->setInlineHelp(QStringLiteral("How the feather fades."));
      falloffProp->setWhatsThis(QStringLiteral("Shape of the mask feather fade.\nGaussian is the classic soft edge; Linear fades evenly; Smooth eases both ends; Sharp keeps a harder rim.\nTry Sharp when a soft mask looks washed out."));
      falloffProp->setDisplayLabel(pathLabel + QStringLiteral(" Falloff"));
      maskGroup.addProperty(falloffProp);

      auto expansionProp = makeProp(pathPrefix + QStringLiteral(".expansion"),
                                    PropertyType::Float,
                                    static_cast<double>(path.expansion()),
                                    -227 - pathIndex);
      expansionProp->setAnimatable(true);
      expansionProp->setSoftRange(-256.0, 256.0);
      expansionProp->setStep(0.5);
      expansionProp->setDisplayLabel(pathLabel + QStringLiteral(" Expansion"));
      maskGroup.addProperty(expansionProp);

      auto invertedProp = makeProp(pathPrefix + QStringLiteral(".inverted"),
                                   PropertyType::Boolean, path.isInverted(),
                                   -226 - pathIndex);
      invertedProp->setAnimatable(true);
      invertedProp->setDisplayLabel(pathLabel + QStringLiteral(" Inverted"));
      maskGroup.addProperty(invertedProp);

      auto modeProp = makeProp(pathPrefix + QStringLiteral(".mode"),
                               PropertyType::Integer,
                               static_cast<int>(path.mode()),
                               -225 - pathIndex);
      modeProp->setAnimatable(true);
      modeProp->setTooltip(
          QStringLiteral("0=Add,1=Subtract,2=Intersect,3=Difference"));
      modeProp->setInlineHelp(QStringLiteral("How this mask combines."));
      modeProp->setWhatsThis(QStringLiteral("How this mask combines with the other masks on the layer.\nAdd shows, Subtract cuts out, Intersect keeps only the overlap, Difference keeps the non-overlap."));
      modeProp->setDisplayLabel(pathLabel + QStringLiteral(" Mode"));
      maskGroup.addProperty(modeProp);
    }

    groups.push_back(std::move(maskGroup));
  }
}


} // namespace Artifact

#undef QStringLiteral

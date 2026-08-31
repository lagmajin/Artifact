module;

#include <QString>
#include <QStringList>

#include <algorithm>
#include <utility>
#include <vector>

export module Artifact.Widgets.PropertyPresentation;

import Artifact.Layer.Abstract;
import Artifact.Layer.Text;
import Property.Abstract;
import Property.Group;

export namespace Artifact {

namespace detail {

struct PropertyPresentationProfile {
  QString id;
  QStringList visibleGroups;
};

PropertyPresentationProfile
propertyPresentationProfile(const ArtifactAbstractLayerPtr &layer) {
  if (ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer)) {
    return {QStringLiteral("text"),
            {QStringLiteral("Initial"), QStringLiteral("Transform"),
             QStringLiteral("Text"), QStringLiteral("Path Options")}};
  }
  if (layer && layer->getProperty(QStringLiteral("solid.color"))) {
    return {QStringLiteral("solid"),
            {QStringLiteral("Initial"), QStringLiteral("Transform"),
             QStringLiteral("Solid")}};
  }
  // Crop / Pan is opt-in: before activation its only affordance is the
  // Transform group's Add Crop / Pan button. After activation its group is
  // inserted as though the user had added a new property block.
  if (layer && layer->getProperty(QStringLiteral("image.sourcePath"))) {
    QStringList visibleGroups{QStringLiteral("Transform"),
                              QStringLiteral("Initial"),
                              QStringLiteral("Image")};
    const auto cropEnabled =
        layer->getProperty(QStringLiteral("sourceCrop.enabled"));
    if (cropEnabled && cropEnabled->getValue().toBool()) {
      visibleGroups.append(QStringLiteral("Source Reframe"));
    }
    return {QStringLiteral("image"), std::move(visibleGroups)};
  }
  // A fixed plane has width and height but no depth. Its geometry controls
  // remain available after the transform, without widening the profile for
  // every 3D-model subtype yet.
  if (layer && layer->getProperty(QStringLiteral("geometry.width")) &&
      !layer->getProperty(QStringLiteral("geometry.depth"))) {
    return {QStringLiteral("plane"),
            {QStringLiteral("Transform"), QStringLiteral("Initial"),
             QStringLiteral("Geometry")}};
  }
  return {QStringLiteral("basic"),
          {QStringLiteral("Initial"), QStringLiteral("Transform")}};
}

bool presentationAllowsGroup(const PropertyPresentationProfile &profile,
                             const QString &groupName) {
  if (profile.visibleGroups.isEmpty()) {
    return true;
  }
  return profile.visibleGroups.contains(groupName.trimmed(),
                                        Qt::CaseInsensitive);
}

bool isTextAnimatorPropertyGroup(const ArtifactCore::PropertyGroup &group) {
  const auto properties = group.sortedProperties();
  return std::any_of(properties.begin(), properties.end(), [](const auto &property) {
    return property && property->getName().startsWith(
                           QStringLiteral("text.animators."),
                           Qt::CaseInsensitive);
  });
}

void applyPresentationPropertyRules(
    const PropertyPresentationProfile &profile, const QString &groupName,
    std::vector<ArtifactCore::AbstractPropertyPtr> &properties) {
  if (profile.id != QStringLiteral("solid") ||
      groupName.compare(QStringLiteral("Solid"), Qt::CaseInsensitive) != 0) {
    return;
  }

  const auto fillMode = std::find_if(
      properties.begin(), properties.end(), [](const auto &property) {
        return property && property->getName().compare(
                               QStringLiteral("solid.fillType"),
                               Qt::CaseInsensitive) == 0;
      });
  const bool usesGradient =
      fillMode != properties.end() && (*fillMode)->getValue().toInt() != 0;
  if (usesGradient) {
    return;
  }

  std::erase_if(properties, [](const auto &property) {
    if (!property) {
      return true;
    }
    const QString name = property->getName();
    return name.compare(QStringLiteral("solid.color"),
                        Qt::CaseInsensitive) != 0 &&
           name.compare(QStringLiteral("solid.fillType"),
                        Qt::CaseInsensitive) != 0;
  });
}

} // namespace detail

} // namespace Artifact

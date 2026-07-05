module;

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <vector>
#include <functional>
#include <wobjectdefs.h>

export module Artifact.Timeline.KeyframeModel;

import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Property.Abstract;
import Math.Interpolate;
import Time.Rational;
import Frame.Position;
import Utils.Id;

using namespace ArtifactCore;

W_REGISTER_ARGTYPE(ArtifactCore::LayerID)

export namespace Artifact {

struct DopeSheetKeyframeRef {
  LayerID layerId;
  QString propertyPath;
  RationalTime time;

  bool isValid() const {
    return !propertyPath.trimmed().isEmpty();
  }
};

struct DopeSheetKeyframeEntry {
  LayerID layerId;
  QString propertyPath;
  KeyFrame keyframe;
};

class ArtifactTimelineKeyframeModel : public QObject {
  W_OBJECT(ArtifactTimelineKeyframeModel)
public:
  explicit ArtifactTimelineKeyframeModel(QObject* parent = nullptr);
  ~ArtifactTimelineKeyframeModel();

  static QStringList transformPropertyPaths();
  static QString displayLabelForPropertyPath(QString propertyPath);
  static bool isTransformPropertyPath(const QString& propertyPath);
  static bool shouldHideTimelinePropertyGroup(const QString& groupName);
  static bool isTimelinePropertyGroupExpandedByDefault(
      const QString& groupName);

  std::vector<KeyFrame> getKeyframesFor(const CompositionID& compId,
                                        const LayerID& layerId,
                                        const QString& propertyPath) const;

  bool addKeyframe(const CompositionID& compId,
                   const LayerID& layerId,
                   const QString& propertyPath,
                   const RationalTime& time,
                   const QVariant& value,
                   InterpolationType interpolation = InterpolationType::Linear);

  bool addKeyframeWithBezier(const CompositionID& compId,
                             const LayerID& layerId,
                             const QString& propertyPath,
                             const RationalTime& time,
                             const QVariant& value,
                             float cp1_x, float cp1_y, float cp2_x, float cp2_y);

  bool moveKeyframe(const CompositionID& compId,
                    const LayerID& layerId,
                    const QString& propertyPath,
                    const RationalTime& fromTime,
                    const RationalTime& toTime);

  bool removeKeyframe(const CompositionID& compId,
                      const LayerID& layerId,
                      const QString& propertyPath,
                      const RationalTime& time);

  std::vector<DopeSheetKeyframeEntry>
  collectDopeSheetKeyframesForLayer(const CompositionID& compId,
                                    const LayerID& layerId) const;

  bool offsetKeyframes(const CompositionID& compId,
                       const std::vector<DopeSheetKeyframeRef>& refs,
                       const RationalTime& delta);

  bool scaleKeyframes(const CompositionID& compId,
                      const std::vector<DopeSheetKeyframeRef>& refs,
                      const RationalTime& pivot,
                      double factor);

};

} // namespace Artifact

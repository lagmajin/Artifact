module;

#include <QObject>
#include <QString>
#include <QVariant>
#include <vector>
#include <functional>
#include <wobjectdefs.h>

export module Artifact.Timeline.KeyframeModel;

import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Property.Abstract;
import Time.Rational;
import Frame.Position;
import Utils.Id;

using namespace ArtifactCore;

W_REGISTER_ARGTYPE(ArtifactCore::LayerID)

export namespace Artifact {

class ArtifactTimelineKeyframeModel : public QObject {
  W_OBJECT(ArtifactTimelineKeyframeModel)
public:
  explicit ArtifactTimelineKeyframeModel(QObject* parent = nullptr);
  ~ArtifactTimelineKeyframeModel();

  std::vector<KeyFrame> getKeyframesFor(const CompositionID& compId,
                                        const LayerID& layerId,
                                        const QString& propertyPath) const;

  bool addKeyframe(const CompositionID& compId,
                   const LayerID& layerId,
                   const QString& propertyPath,
                   const RationalTime& time,
                   const QVariant& value,
                   EasingType easing = EasingType::Linear);

  bool removeKeyframe(const CompositionID& compId,
                      const LayerID& layerId,
                      const QString& propertyPath,
                      const RationalTime& time);

public /*signals*/:
  void keyframesChanged(LayerID layerId, QString propertyPath) W_SIGNAL(keyframesChanged, layerId, propertyPath);
};

} // namespace Artifact

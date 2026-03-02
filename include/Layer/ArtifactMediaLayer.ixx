module;
#include <QString>
#include <QJsonObject>
#include <memory>

export module Artifact.Layer.Media;

import Artifact.Layer.Video;

export namespace Artifact {

class ArtifactMediaLayer : public ArtifactVideoLayer {
public:
 ArtifactMediaLayer();
 ~ArtifactMediaLayer() override;

 // Media-first serialization entry points.
 QJsonObject toJson() const;
 static std::shared_ptr<ArtifactMediaLayer> fromJson(const QJsonObject& obj);
};

}

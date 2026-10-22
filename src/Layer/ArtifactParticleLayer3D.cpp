module;
#include <QJsonObject>
#include <QString>

module Artifact.Layer.Particle;

namespace Artifact {

ArtifactParticle3DLayer::ArtifactParticle3DLayer()
{
    setIs3D(true);
}

ArtifactParticle3DLayer::~ArtifactParticle3DLayer() = default;

QJsonObject ArtifactParticle3DLayer::toJson() const
{
    QJsonObject json = ArtifactParticleLayer::toJson();
    json[QStringLiteral("type")] = static_cast<int>(LayerType::Particle3D);
    json[QStringLiteral("layerType")] = QStringLiteral("Particle3DLayer");
    json[QStringLiteral("is3D")] = true;
    return json;
}

void ArtifactParticle3DLayer::fromJsonProperties(const QJsonObject& obj)
{
    ArtifactParticleLayer::fromJsonProperties(obj);
    setIs3D(true);
}

} // namespace Artifact

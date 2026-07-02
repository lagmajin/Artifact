module;
#include <QString>
export module Artifact.Layer.EnvironmentMapInitParams;

import Artifact.Layer.InitParams;

export namespace Artifact {

class ArtifactEnvironmentMapLayerInitParams : public ArtifactLayerInitParams
{
private:
    QString hdriPath_;

public:
    explicit ArtifactEnvironmentMapLayerInitParams(const QString& name = QStringLiteral("Environment Map"))
        : ArtifactLayerInitParams(name, LayerType::EnvironmentMap) {}
    ~ArtifactEnvironmentMapLayerInitParams() = default;

    QString hdriPath() const { return hdriPath_; }
    void setHdriPath(const QString& path) { hdriPath_ = path; }
};

} // namespace Artifact

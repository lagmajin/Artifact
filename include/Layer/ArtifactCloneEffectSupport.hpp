#pragma once

#include <QMatrix4x4>

#include <functional>
#include <vector>
#include <memory>

namespace Artifact {

class ArtifactAbstractLayer;
using ArtifactAbstractLayerPtr = std::shared_ptr<ArtifactAbstractLayer>;

struct CloneRenderInstance {
    QMatrix4x4 transform;
    float weight = 1.0f;
};

std::vector<CloneRenderInstance> cloneRenderInstances(const ArtifactAbstractLayer* layer,
                                                      const QMatrix4x4& baseTransform);

inline std::vector<CloneRenderInstance> cloneRenderInstances(const ArtifactAbstractLayerPtr& layer,
                                                             const QMatrix4x4& baseTransform)
{
    return cloneRenderInstances(layer.get(), baseTransform);
}

void drawWithClonerEffect(const ArtifactAbstractLayer* layer,
                          const QMatrix4x4& baseTransform,
                          const std::function<void(const QMatrix4x4&, float)>& drawFn);

inline void drawWithClonerEffect(const ArtifactAbstractLayerPtr& layer,
                                 const QMatrix4x4& baseTransform,
                                 const std::function<void(const QMatrix4x4&, float)>& drawFn)
{
    drawWithClonerEffect(layer.get(), baseTransform, drawFn);
}

} // namespace Artifact

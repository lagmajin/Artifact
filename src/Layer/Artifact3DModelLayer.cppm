module;
#include <QObject>
#include <Artifact/Render/ArtifactIRenderer.ixx>
#include <Artifact/Layer/ArtifactAbstractLayer.ixx>
module Artifact.Layers.Model3D;

namespace Artifact {

class Artifact3DLayer::Impl {
public:
    Impl() {
        // Set this layer as 3D
        if (auto self = qobject_cast<Artifact3DLayer*>(QObject::parent())) {
            self->setIs3D(true);
        }
    }
    ~Impl() {}
    // TODO: add 3D model data and resource management members.
};

Artifact3DLayer::Artifact3DLayer() : impl_(new Impl()) {
    // Set the 3D flag directly as well (redundant but safe)
    setIs3D(true);
}
Artifact3DLayer::~Artifact3DLayer() { delete impl_; }

void Artifact3DLayer::loadFromFile() {
    // TODO: Implement loading from file using ufbx or tinyobjloader
}

void Artifact3DLayer::draw(ArtifactIRenderer* renderer) {
    // TODO: Implement 3D rendering
}

void Artifact3DLayer::drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) {
    // TODO: Implement LOD rendering
}

} // namespace Artifact

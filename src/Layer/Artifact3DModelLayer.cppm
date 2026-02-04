module;
#include <QObject>
module Artifact.Layers.Model3D;

namespace Artifact {

class Artifact3DLayer::Impl {
public:
    Impl() {}
    ~Impl() {}
    // TODO: ここに3Dモデルデータやリソース管理用のメンバーを追加
};

Artifact3DLayer::Artifact3DLayer() : impl_(new Impl()) {}
Artifact3DLayer::~Artifact3DLayer() { delete impl_; }

} // namespace Artifact

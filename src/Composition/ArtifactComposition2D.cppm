module;

module Artifact.Composition._2D;

namespace Artifact {

class ArtifactComposition2D::Impl {
public:
  Impl() = default;
  ~Impl() = default;
};

ArtifactComposition2D::ArtifactComposition2D(const CompositionID& id,
                                             const ArtifactCompositionInitParams& params)
    : ArtifactAbstractComposition(id, params), impl_(new Impl())
{
}

ArtifactComposition2D::~ArtifactComposition2D()
{
  delete impl_;
}

} // namespace Artifact

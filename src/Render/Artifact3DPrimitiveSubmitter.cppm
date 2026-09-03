module;
#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <Buffer.h>
#include <vector>
#include <cstring>
module Artifact.Render.ThreeDPrimitiveSubmitter.Contract;

namespace Artifact {

class Artifact3DPrimitiveSubmitter::Impl {
public:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Artifact3DPrimitivePipelineProvider pipelines;

    // CPU-side staging copy of the last uploaded mesh. Used to keep the
    // contract honest even when the provider is empty (Phase 1 L1): the
    // submitter must accept an upload, hold the data, and report
    // !isInitialized() if the provider does not yet have a usable PSO.
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<Diligent::Uint32> indices;
    bool meshUploaded = false;
};

Artifact3DPrimitiveSubmitter::Artifact3DPrimitiveSubmitter() = default;
Artifact3DPrimitiveSubmitter::~Artifact3DPrimitiveSubmitter() { destroy(); }

bool Artifact3DPrimitiveSubmitter::initialize(
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
    Artifact3DPrimitivePipelineProvider pipelines) {
    destroy();
    if (!device) {
        return false;
    }
    // The provider is allowed to be empty during the Phase 1 / L1
    // migration window; we still want callers to be able to upload
    // their mesh and check isInitialized() to gate real draws.
    impl_ = new Impl();
    impl_->device = std::move(device);
    impl_->pipelines = pipelines;
    return true;
}

bool Artifact3DPrimitiveSubmitter::isInitialized() const {
    return impl_ != nullptr;
}

bool Artifact3DPrimitiveSubmitter::uploadMesh(
    std::span<const float> positions,
    std::span<const float> normals,
    std::span<const Diligent::Uint32> indices) {
    if (!impl_) {
        return false;
    }
    if (positions.empty() || indices.empty()) {
        return false;
    }
    // Position/normal counts are derived from the position buffer length
    // because the contract uses stride-bytes but spans are dense float
    // triples for simplicity. Callers that need interleaved layouts
    // should fill the span with the interleaved stream and adapt here
    // in Phase 2 / L2.
    if (positions.size() % 3 != 0) {
        return false;
    }
    if (!normals.empty() && normals.size() != positions.size()) {
        return false;
    }
    impl_->positions.assign(positions.begin(), positions.end());
    if (!normals.empty()) {
        impl_->normals.assign(normals.begin(), normals.end());
    } else {
        impl_->normals.clear();
    }
    impl_->indices.assign(indices.begin(), indices.end());
    impl_->meshUploaded = true;
    return true;
}

bool Artifact3DPrimitiveSubmitter::submit(
    Diligent::IDeviceContext* /*context*/,
    Diligent::ITextureView* /*target*/,
    Stage /*stage*/,
    const SubmitPacket& /*packet*/) {
    // Phase 1 (L1) ships only the provider boundary. Until the matching
    // Unlit / FlatLit / Wire shaders are added in Phase 2, the submitter
    // intentionally no-ops. Callers must keep using the legacy 3D line
    // path while isInitialized() reports the provider as empty.
    if (!impl_ || !impl_->meshUploaded) {
        return false;
    }
    return false;
}

void Artifact3DPrimitiveSubmitter::destroy() {
    delete impl_;
    impl_ = nullptr;
}

} // namespace Artifact

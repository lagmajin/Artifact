module;
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>
#include <span>
export module Artifact.Render.ThreeDPrimitiveSubmitter.Contract;

import Color.Float;

export namespace Artifact {

/// Provider boundary for the three 3D-primitive mesh pipelines.
///
/// Phase 1 (L1) of the 3D shader variant plan keeps this contract thin.
/// The three PSO slots are reserved for the future Unlit / FlatLit / Wire
/// variants; until the matching shaders are added in Phase 2 (L2), all
/// members are expected to be nullptr and the submitter reports
/// !isValid(). Callers must treat the boundary as a no-op until
/// `isValid()` returns true so the legacy 3D line path keeps working.
struct Artifact3DPrimitivePipelineProvider {
    Diligent::IPipelineState* unlitPipeline = nullptr;
    Diligent::IShaderResourceBinding* unlitBinding = nullptr;
    Diligent::IPipelineState* flatLitPipeline = nullptr;
    Diligent::IShaderResourceBinding* flatLitBinding = nullptr;
    Diligent::IPipelineState* wirePipeline = nullptr;
    Diligent::IShaderResourceBinding* wireBinding = nullptr;

    bool hasUnlit() const noexcept {
        return unlitPipeline && unlitBinding;
    }
    bool hasFlatLit() const noexcept {
        return flatLitPipeline && flatLitBinding;
    }
    bool hasWire() const noexcept {
        return wirePipeline && wireBinding;
    }
    bool isValid() const noexcept {
        return hasUnlit() || hasFlatLit() || hasWire();
    }
};

/// 3D-primitive GPU submission contract. Owns vertex/index buffers, the
/// per-frame constant buffer, and the cached PSO/SRB references obtained
/// from the provider. The submitter does not know about layers, lights,
/// gizmos, or the composition; it is purely a mesh-draw helper.
class Artifact3DPrimitiveSubmitter final {
public:
    /// Stage used to pick which PSO the draw call uses. The boundary
    /// keeps the stages explicit so future variants (textured, normal
    /// mapped, ...) can be added without changing the call sites.
    enum class Stage {
        Unlit = 0,
        FlatLit = 1,
        Wire = 2,
    };

    /// Per-draw packet. Positions and normals are taken in the layer's
    /// local space; the submitter uploads a single per-frame MVP/clip
    /// transform along with the packed material constants.
    struct SubmitPacket {
        const float* positions = nullptr;
        Uint32 positionCount = 0;
        Uint32 positionStrideBytes = sizeof(float) * 3;

        const float* normals = nullptr;
        Uint32 normalStrideBytes = sizeof(float) * 3;

        const Uint32* indices = nullptr;
        Uint32 indexCount = 0;

        const QMatrix4x4* modelMatrix = nullptr;
        const QMatrix4x4* viewMatrix = nullptr;
        const QMatrix4x4* projectionMatrix = nullptr;

        ArtifactCore::FloatColor baseColor{1.0f, 1.0f, 1.0f, 1.0f};
        ArtifactCore::FloatColor emissionColor{0.0f, 0.0f, 0.0f, 1.0f};
        float emissionStrength = 0.0f;
        float opacity = 1.0f;
        bool useVertexAlpha = true;
    };

    Artifact3DPrimitiveSubmitter();
    ~Artifact3DPrimitiveSubmitter();
    Artifact3DPrimitiveSubmitter(const Artifact3DPrimitiveSubmitter&) = delete;
    Artifact3DPrimitiveSubmitter& operator=(const Artifact3DPrimitiveSubmitter&) = delete;

    bool initialize(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
                    Artifact3DPrimitivePipelineProvider pipelines);
    bool isInitialized() const;

    /// Upload the mesh used by the next submit() call. The submitter
    /// keeps a copy of the vertex/index data so callers can free their
    /// staging buffers immediately.
    bool uploadMesh(std::span<const float> positions,
                    std::span<const float> normals,
                    std::span<const Uint32> indices);

    bool submit(Diligent::IDeviceContext* context,
                Diligent::ITextureView* target,
                Stage stage,
                const SubmitPacket& packet);

    void destroy();

private:
    class Impl;
    Impl* impl_ = nullptr;
};

} // namespace Artifact

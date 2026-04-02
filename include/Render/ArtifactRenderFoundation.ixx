module;
#include <QString>
#include <QVector>

export module Artifact.Render.Foundation;

export namespace Artifact {

enum class RenderFoundationTechnique {
    EarlyZDepthPrepass,
    ClusteredForwardPlus,
    TileBinning,
    GpuDrivenRendering,
    AsyncCompute,
    TemporalReprojection,
    CheckerboardHalfRes,
    FoveatedROI,
    BindlessDescriptors,
    PersistentMappedUpload
};

enum class RenderFoundationPriority {
    Core,
    HighValue,
    NiceToHave
};

struct RenderTechniquePlan {
    RenderFoundationTechnique technique = RenderFoundationTechnique::EarlyZDepthPrepass;
    RenderFoundationPriority priority = RenderFoundationPriority::Core;
    QString title;
    QString description;
    bool preservesCurrentOutput = true;
    bool requiresBackendWork = false;
};

struct RenderFoundationProfile {
    QString name;
    QString summary;
    bool allowCPUFallback = true;
    bool keepExistingOutputStable = true;
    QVector<RenderTechniquePlan> techniques;
};

inline QString techniqueName(RenderFoundationTechnique technique)
{
    switch (technique) {
    case RenderFoundationTechnique::EarlyZDepthPrepass:
        return QStringLiteral("Early-Z / Depth Prepass");
    case RenderFoundationTechnique::ClusteredForwardPlus:
        return QStringLiteral("Clustered / Forward+");
    case RenderFoundationTechnique::TileBinning:
        return QStringLiteral("Tile / Binning");
    case RenderFoundationTechnique::GpuDrivenRendering:
        return QStringLiteral("GPU Driven Rendering");
    case RenderFoundationTechnique::AsyncCompute:
        return QStringLiteral("Async Compute");
    case RenderFoundationTechnique::TemporalReprojection:
        return QStringLiteral("Temporal Reprojection");
    case RenderFoundationTechnique::CheckerboardHalfRes:
        return QStringLiteral("Checkerboard / Half Res");
    case RenderFoundationTechnique::FoveatedROI:
        return QStringLiteral("Foveated / ROI Rendering");
    case RenderFoundationTechnique::BindlessDescriptors:
        return QStringLiteral("Bindless / Descriptor Indexing");
    case RenderFoundationTechnique::PersistentMappedUpload:
        return QStringLiteral("Persistent Mapped Upload");
    }
    return QStringLiteral("Unknown");
}

inline QVector<RenderTechniquePlan> makeDefaultTechniquePlans()
{
    return {
        {
            RenderFoundationTechnique::PersistentMappedUpload,
            RenderFoundationPriority::Core,
            techniqueName(RenderFoundationTechnique::PersistentMappedUpload),
            QStringLiteral("Reduce CPU-GPU transfer overhead and prepare a stable upload path."),
            true,
            false,
        },
        {
            RenderFoundationTechnique::TemporalReprojection,
            RenderFoundationPriority::Core,
            techniqueName(RenderFoundationTechnique::TemporalReprojection),
            QStringLiteral("Reuse prior frames for blur, DOF, denoise, and quality reconstruction."),
            true,
            false,
        },
        {
            RenderFoundationTechnique::EarlyZDepthPrepass,
            RenderFoundationPriority::HighValue,
            techniqueName(RenderFoundationTechnique::EarlyZDepthPrepass),
            QStringLiteral("Establish a low-risk opaque depth-prepass foundation."),
            true,
            false,
        },
        {
            RenderFoundationTechnique::GpuDrivenRendering,
            RenderFoundationPriority::HighValue,
            techniqueName(RenderFoundationTechnique::GpuDrivenRendering),
            QStringLiteral("Move draw submission pressure from CPU to GPU via indirect execution."),
            true,
            true,
        },
        {
            RenderFoundationTechnique::ClusteredForwardPlus,
            RenderFoundationPriority::HighValue,
            techniqueName(RenderFoundationTechnique::ClusteredForwardPlus),
            QStringLiteral("Cull light/effect work per screen region to remove wasted loops."),
            true,
            true,
        },
        {
            RenderFoundationTechnique::CheckerboardHalfRes,
            RenderFoundationPriority::NiceToHave,
            techniqueName(RenderFoundationTechnique::CheckerboardHalfRes),
            QStringLiteral("Lower full-screen cost with alternating or reduced-resolution rendering."),
            true,
            false,
        },
        {
            RenderFoundationTechnique::AsyncCompute,
            RenderFoundationPriority::NiceToHave,
            techniqueName(RenderFoundationTechnique::AsyncCompute),
            QStringLiteral("Overlap compute work with graphics when dependency chains allow it."),
            true,
            true,
        },
        {
            RenderFoundationTechnique::BindlessDescriptors,
            RenderFoundationPriority::NiceToHave,
            techniqueName(RenderFoundationTechnique::BindlessDescriptors),
            QStringLiteral("Reduce resource switching overhead when asset counts grow."),
            true,
            true,
        },
        {
            RenderFoundationTechnique::FoveatedROI,
            RenderFoundationPriority::NiceToHave,
            techniqueName(RenderFoundationTechnique::FoveatedROI),
            QStringLiteral("Spend quality where the user is actually looking or editing."),
            true,
            false,
        },
        {
            RenderFoundationTechnique::TileBinning,
            RenderFoundationPriority::NiceToHave,
            techniqueName(RenderFoundationTechnique::TileBinning),
            QStringLiteral("Split the frame into regions and process only relevant work."),
            true,
            true,
        },
    };
}

inline RenderFoundationProfile makeDefaultRenderFoundationProfile()
{
    RenderFoundationProfile profile;
    profile.name = QStringLiteral("Artifact Render Foundation");
    profile.summary = QStringLiteral("A no-op-safe portfolio of rendering techniques ordered by expected payoff.");
    profile.techniques = makeDefaultTechniquePlans();
    return profile;
}

} // namespace Artifact

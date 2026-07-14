module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>

#include <QDebug>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

export module Artifact.TestRunner;

import Artifact.Render.DiligentDeviceManager;
import Artifact.Test.AIToolBridge;
import Artifact.Test.AdjustmentLayer;
import Artifact.Test.LayerGroup;
import Artifact.Test.PreCompose;
import Artifact.Test.PropertyKeyframe;
import Artifact.Test.TimingEventView;
import Graphics.GPUcomputeContext;
import Graphics.LayerBlendPipeline;
import Layer.Blend;

namespace {

constexpr Diligent::Uint32 kBlendTestSize = 8;
constexpr float kBlendTestSentinel = -99.0f;

using BlendTestPixels = std::array<float, kBlendTestSize * kBlendTestSize * 4>;

void fillPixels(BlendTestPixels& pixels, const std::array<float, 4>& color)
{
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        std::copy(color.begin(), color.end(), pixels.begin() + i);
    }
}

Diligent::RefCntAutoPtr<Diligent::ITexture> createBlendTestTexture(
    Diligent::IRenderDevice* device,
    const char* name,
    const Diligent::BIND_FLAGS bindFlags,
    const BlendTestPixels* initialPixels)
{
    Diligent::TextureDesc desc;
    desc.Name = name;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = kBlendTestSize;
    desc.Height = kBlendTestSize;
    desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleCount = 1;
    desc.Usage = Diligent::USAGE_DEFAULT;
    desc.BindFlags = bindFlags;

    Diligent::TextureSubResData subresource;
    Diligent::TextureData initialData;
    const Diligent::TextureData* initialDataPtr = nullptr;
    if (initialPixels != nullptr) {
        subresource.pData = initialPixels->data();
        subresource.Stride = kBlendTestSize * sizeof(float) * 4;
        initialData.pSubResources = &subresource;
        initialData.NumSubresources = 1;
        initialDataPtr = &initialData;
    }

    Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
    device->CreateTexture(desc, initialDataPtr, &texture);
    return texture;
}

bool resetBlendTestOutput(Diligent::IDeviceContext* context,
                          Diligent::ITexture* output,
                          const BlendTestPixels& sentinelPixels)
{
    if (!context || !output) {
        return false;
    }

    Diligent::TextureSubResData subresource;
    subresource.pData = sentinelPixels.data();
    subresource.Stride = kBlendTestSize * sizeof(float) * 4;
    const Diligent::Box box(0, kBlendTestSize, 0, kBlendTestSize, 0, 1);
    context->UpdateTexture(output, 0, 0, box, subresource,
                           Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                           Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    return true;
}

bool readBlendTestPixel(Diligent::IDeviceContext* context,
                        Diligent::ITexture* output,
                        Diligent::ITexture* staging,
                        std::array<float, 4>& pixel)
{
    if (!context || !output || !staging) {
        return false;
    }

    Diligent::CopyTextureAttribs copy(
        output, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        staging, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->CopyTexture(copy);
    context->Flush();
    context->WaitForIdle();

    Diligent::MappedTextureSubresource mapped;
    context->MapTextureSubresource(staging, 0, 0, Diligent::MAP_READ,
                                   Diligent::MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
    if (!mapped.pData || mapped.Stride < sizeof(float) * 4) {
        return false;
    }

    std::memcpy(pixel.data(), mapped.pData, sizeof(float) * 4);
    context->UnmapTextureSubresource(staging, 0, 0);
    return true;
}

bool nearlyEqual(const float actual, const float expected, const float epsilon = 1.0e-4f)
{
    return std::abs(actual - expected) <= epsilon;
}

bool matchesExpected(const std::array<float, 4>& actual,
                     const std::array<float, 4>& expected)
{
    for (std::size_t i = 0; i < actual.size(); ++i) {
        if (!nearlyEqual(actual[i], expected[i])) {
            return false;
        }
    }
    return true;
}

} // namespace

export namespace Artifact {

int runAllTests()
{
    int failures = 0;

    qInfo().noquote() << "[Test] Running built-in tests";

    failures += runAIToolBridgeTests();
    failures += runAdjustmentLayerTests();
    failures += runLayerGroupTests();
    failures += runPreComposeTests();
    failures += runPropertyKeyframeTests();

    ArtifactTestTimingEventView timingEventViewTests;
    timingEventViewTests.runAllTests();

    if (failures == 0) {
        qInfo().noquote() << "[Test] All built-in tests passed";
    } else {
        qWarning().noquote() << "[Test] Built-in tests finished with failures:" << failures;
    }

    return failures;
}

int runGpuBlendTests()
{
    qInfo().noquote() << "[GpuBlendTest] Starting headless GPU blend validation";

    DiligentDeviceManager deviceManager;
    deviceManager.initializeHeadless();
    auto device = deviceManager.device();
    auto context = deviceManager.immediateContext();
    if (!device || !context) {
        qCritical().noquote() << "[GpuBlendTest] Headless GPU device initialization failed";
        return 1;
    }

    auto gpuContext = std::make_shared<ArtifactCore::GpuContext>(device, context);
    ArtifactCore::LayerBlendPipeline pipeline(gpuContext);
    if (!pipeline.initialize() || !pipeline.ready()) {
        qCritical().noquote() << "[GpuBlendTest] LayerBlendPipeline initialization failed";
        return 1;
    }

    BlendTestPixels sourcePixels;
    BlendTestPixels destinationPixels;
    BlendTestPixels sentinelPixels;
    fillPixels(sourcePixels, {0.8f, 0.2f, 0.7f, 0.6f});
    fillPixels(destinationPixels, {0.08f, 0.56f, 0.32f, 0.8f});
    sentinelPixels.fill(kBlendTestSentinel);

    auto source = createBlendTestTexture(
        device, "GpuBlendTest.Source", Diligent::BIND_SHADER_RESOURCE, &sourcePixels);
    auto destination = createBlendTestTexture(
        device, "GpuBlendTest.Destination", Diligent::BIND_SHADER_RESOURCE, &destinationPixels);
    auto output = createBlendTestTexture(
        device, "GpuBlendTest.Output",
        Diligent::BIND_SHADER_RESOURCE | Diligent::BIND_UNORDERED_ACCESS,
        &sentinelPixels);

    Diligent::TextureDesc stagingDesc;
    stagingDesc.Name = "GpuBlendTest.Staging";
    stagingDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    stagingDesc.Width = kBlendTestSize;
    stagingDesc.Height = kBlendTestSize;
    stagingDesc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.SampleCount = 1;
    stagingDesc.Usage = Diligent::USAGE_STAGING;
    stagingDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
    Diligent::RefCntAutoPtr<Diligent::ITexture> staging;
    device->CreateTexture(stagingDesc, nullptr, &staging);

    if (!source || !destination || !output || !staging) {
        qCritical().noquote() << "[GpuBlendTest] Failed to create test textures";
        return 1;
    }

    auto* sourceSrv = source->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    auto* destinationSrv = destination->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    auto* outputUav = output->GetDefaultView(Diligent::TEXTURE_VIEW_UNORDERED_ACCESS);
    if (!sourceSrv || !destinationSrv || !outputUav) {
        qCritical().noquote() << "[GpuBlendTest] Failed to create required texture views";
        return 1;
    }

    int failures = 0;
    int dispatchPasses = 0;
    constexpr int lastMode = static_cast<int>(ArtifactCore::BlendMode::SilhouetteLuma);
    for (int modeValue = 0; modeValue <= lastMode; ++modeValue) {
        const auto mode = static_cast<ArtifactCore::BlendMode>(modeValue);
        resetBlendTestOutput(context, output, sentinelPixels);

        const bool dispatched = pipeline.blend(
            context, sourceSrv, destinationSrv, outputUav, mode, 1.0f);
        std::array<float, 4> pixel{};
        const bool readback = dispatched && readBlendTestPixel(context, output, staging, pixel);
        const bool finite = readback && std::all_of(pixel.begin(), pixel.end(), [](const float value) {
            return std::isfinite(value);
        });
        const bool written = finite && std::any_of(pixel.begin(), pixel.end(), [](const float value) {
            return value != kBlendTestSentinel;
        });

        bool referenceMatch = true;
        if (mode == ArtifactCore::BlendMode::Normal) {
            referenceMatch = matchesExpected(pixel, {0.512f, 0.344f, 0.548f, 0.92f});
        } else if (mode == ArtifactCore::BlendMode::Add) {
            referenceMatch = matchesExpected(pixel, {0.56f, 0.68f, 0.692f, 0.92f});
        } else if (mode == ArtifactCore::BlendMode::Multiply) {
            referenceMatch = matchesExpected(pixel, {0.1664f, 0.3152f, 0.3464f, 0.92f});
        } else if (mode == ArtifactCore::BlendMode::SoftLight) {
            referenceMatch = matchesExpected(pixel, {0.232448f, 0.52352f, 0.448631f, 0.92f});
        } else if (mode == ArtifactCore::BlendMode::Dissolve ||
                   mode == ArtifactCore::BlendMode::DancingDissolve) {
            const bool sourceSelected = matchesExpected(pixel, {0.8f, 0.2f, 0.7f, 1.0f});
            const bool destinationSelected = matchesExpected(pixel, {0.08f, 0.56f, 0.32f, 0.8f});
            referenceMatch = sourceSelected || destinationSelected;
        }

        if (dispatched && readback && finite && written && referenceMatch) {
            ++dispatchPasses;
            qInfo().noquote() << "[GpuBlendTest] PASS"
                              << ArtifactCore::BlendModeUtils::toString(mode)
                              << pixel[0] << pixel[1] << pixel[2] << pixel[3];
        } else {
            ++failures;
            qCritical().noquote() << "[GpuBlendTest] FAIL"
                                  << ArtifactCore::BlendModeUtils::toString(mode)
                                  << "dispatch=" << dispatched
                                  << "readback=" << readback
                                  << "finite=" << finite
                                  << "written=" << written
                                  << "reference=" << referenceMatch
                                  << "pixel=" << pixel[0] << pixel[1] << pixel[2] << pixel[3];
        }
    }

    qInfo().noquote() << "[GpuBlendTest] Completed"
                      << dispatchPasses << "passed," << failures << "failed";
    return failures;
}

} // namespace Artifact

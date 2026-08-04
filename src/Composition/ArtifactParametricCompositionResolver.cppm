module;

#include <memory>

#include <QImage>
#include <QRectF>
#include <QSize>
#include <QTransform>

module Artifact.Composition.ParametricCompositionResolver;

import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Text;
import Artifact.Layer.Video;
import Composition.ParametricComposition;
import Image.ImageF32x4_RGBA;

namespace Artifact {

ArtifactCore::ParametricCompositionInputResolver buildParametricCompositionInputResolver()
{
    ArtifactCore::ParametricCompositionInputResolver resolver;

    resolver.resolve =
        [](const ParametricCompositionInputBinding& binding,
           const ParametricCompositionRenderContext& context) -> ArtifactCore::Optional<ImageF32x4_RGBA>
    {
        // Bool slots have no image data - skip
        if (binding.kind == ParametricCompositionSlotKind::Bool) {
            return {};
        }

        // Image/Matte: use embedded data directly
        if (binding.kind == ParametricCompositionSlotKind::Image) {
            if (!binding.image.isEmpty()) {
                return binding.image;
            }
            return {};
        }
        if (binding.kind == ParametricCompositionSlotKind::Matte) {
            if (!binding.matte.isEmpty()) {
                return binding.matte;
            }
            return {};
        }

        // SourceLayer: look up the layer and render it
        if (binding.kind == ParametricCompositionSlotKind::SourceLayer) {
            if (binding.sourceLayerId.isNil()) {
                return {};
            }

            auto* svc = ArtifactProjectService::instance();
            if (!svc) {
                return {};
            }

            // Find the composition that owns this layer
            // For now: iterate through all compositions via the project
            // Alternatively, the binding could store the composition context
            auto project = svc->getCurrentProjectSharedPtr();
            if (!project) {
                return {};
            }

            // Try to find the layer in any composition
            // In practice, the caller should set the composition context via binding metadata
            // For now, search through all compositions
            const auto items = svc->projectItems();
            for (const auto* item : items) {
                if (!item || item->type() != eProjectItemType::Composition) {
                    continue;
                }
                const auto& compItem = *static_cast<const CompositionItem*>(item);
                auto findResult = svc->findComposition(compItem.compositionId);
                if (!findResult.success) {
                    continue;
                }
                auto comp = findResult.ptr.lock();
                if (!comp) {
                    continue;
                }
                auto layer = comp->layerById(binding.sourceLayerId);
                if (!layer) {
                    continue;
                }

                // Found the layer - use the image layer's explicit CPU
                // framebuffer bridge when available.  This keeps source
                // image bindings functional without inventing a second
                // renderer inside the parametric resolver.
                // Use the output size from context, or layer's local bounds
                const QRectF layerBounds = layer->localBounds();
                const QSize targetSize = context.outputSize.isValid() && context.outputSize.width() > 0
                    ? context.outputSize
                    : QSize(static_cast<int>(std::ceil(layerBounds.width())),
                            static_cast<int>(std::ceil(layerBounds.height())));

                const auto resolveBuffer = [&targetSize](const ImageF32x4_RGBA& source)
                    -> ArtifactCore::Optional<ImageF32x4_RGBA> {
                    if (source.isEmpty()) {
                        return {};
                    }
                    QImage image = source.toQImage();
                    if (image.isNull()) {
                        return {};
                    }
                    if (image.size() != targetSize) {
                        image = image.scaled(targetSize, Qt::IgnoreAspectRatio,
                                             Qt::SmoothTransformation);
                    }
                    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
                    ImageF32x4_RGBA result;
                    result.setFromRGBA8(rgba.constBits(), rgba.width(), rgba.height());
                    return result;
                };

                if (auto imageLayer = ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(layer)) {
                    if (auto result = resolveBuffer(imageLayer->currentFrameBuffer())) return result;
                }
                if (auto videoLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(layer)) {
                    if (auto result = resolveBuffer(videoLayer->currentFrameImageBuffer())) return result;
                }
                if (auto svgLayer = ArtifactCore::dynamicPointerCast<ArtifactSvgLayer>(layer)) {
                    if (auto result = resolveBuffer(svgLayer->currentFrameBuffer())) return result;
                }
                if (auto textLayer = ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer)) {
                    if (auto result = resolveBuffer(textLayer->currentFrameBuffer())) return result;
                }

                // Non-raster layers still require the composition render
                // controller; preserve the resolver's transparent fallback.
                ImageF32x4_RGBA result;
                result.resize(targetSize.width(), targetSize.height());
                result.fill(FloatRGBA(0.0f, 0.0f, 0.0f, 0.0f));
                return result;
            }
        }

        // Text, RGBA, Alpha, MotionPath, Control, Event: not yet supported by default resolver
        return {};
    };

    return resolver;
}

} // namespace Artifact

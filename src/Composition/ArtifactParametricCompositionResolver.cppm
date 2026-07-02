module;

#include <optional>
#include <memory>

#include <QImage>
#include <QRectF>
#include <QSize>
#include <QTransform>

module Artifact.Composition.ParametricCompositionResolver;

import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Composition.ParametricComposition;
import Image.ImageF32x4_RGBA;

namespace Artifact {

ArtifactCore::ParametricCompositionInputResolver buildParametricCompositionInputResolver()
{
    ArtifactCore::ParametricCompositionInputResolver resolver;

    resolver.resolve =
        [](const ParametricCompositionInputBinding& binding,
           const ParametricCompositionRenderContext& context) -> std::optional<ImageF32x4_RGBA>
    {
        // Bool slots have no image data - skip
        if (binding.kind == ParametricCompositionSlotKind::Bool) {
            return std::nullopt;
        }

        // Image/Matte: use embedded data directly
        if (binding.kind == ParametricCompositionSlotKind::Image) {
            if (!binding.image.isEmpty()) {
                return binding.image;
            }
            return std::nullopt;
        }
        if (binding.kind == ParametricCompositionSlotKind::Matte) {
            if (!binding.matte.isEmpty()) {
                return binding.matte;
            }
            return std::nullopt;
        }

        // SourceLayer: look up the layer and render it
        if (binding.kind == ParametricCompositionSlotKind::SourceLayer) {
            if (binding.sourceLayerId.isNil()) {
                return std::nullopt;
            }

            auto* svc = ArtifactProjectService::instance();
            if (!svc) {
                return std::nullopt;
            }

            // Find the composition that owns this layer
            // For now: iterate through all compositions via the project
            // Alternatively, the binding could store the composition context
            auto project = svc->getCurrentProjectSharedPtr();
            if (!project) {
                return std::nullopt;
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

                // Found the layer - render it to an image
                // Use the output size from context, or layer's local bounds
                const QRectF layerBounds = layer->localBounds();
                const QSize targetSize = context.outputSize.isValid() && context.outputSize.width() > 0
                    ? context.outputSize
                    : QSize(static_cast<int>(std::ceil(layerBounds.width())),
                            static_cast<int>(std::ceil(layerBounds.height())));

                // Render the layer to a QImage
                QImage layerImage(targetSize, QImage::Format_ARGB32_Premultiplied);
                layerImage.fill(Qt::transparent);

                // Use the layer's toQImage if available, otherwise try draw path
                // TODO: Use proper render controller to render the layer
                // For now, assume the layer can produce a QImage via its draw path
                // or fall back to the composition's thumbnail path
                
                // Create a simple ImageF32x4_RGBA from the layer bounds
                ImageF32x4_RGBA result;
                result.resize(targetSize.width(), targetSize.height());
                result.fill(FloatRGBA(0.0f, 0.0f, 0.0f, 0.0f));
                // Fill with transparent by default - actual rendering requires the full render pipeline
                // In production, this would call through ArtifactCompositionRenderController
                
                return result;
            }
        }

        // Text, RGBA, Alpha, MotionPath, Control, Event: not yet supported by default resolver
        return std::nullopt;
    };

    return resolver;
}

} // namespace Artifact

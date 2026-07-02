module;

#include <functional>
#include <memory>
#include <optional>

#include <QImage>
#include <QRectF>
#include <QSize>
#include <QString>

export module Artifact.Composition.ParametricCompositionResolver;

import Composition.ParametricComposition;
import Image.ImageF32x4_RGBA;

export namespace Artifact {

/// Build a default input resolver that connects to the Artifact project service.
/// The resolver looks up SourceLayer bindings by layer ID and renders them to images.
/// Image/Matte bindings use the embedded image data directly.
ArtifactCore::ParametricCompositionInputResolver buildParametricCompositionInputResolver();

} // namespace Artifact

module;
#include <QSize>
#include <QString>
#include <QtGlobal>

namespace Diligent {
class ITextureView;
}

export module Artifact.Render.CompositionRenderPassContext;

export namespace Artifact {

class ArtifactIRenderer;
class RenderPipeline;

struct RenderPassResources {
  RenderPipeline* pipeline = nullptr;
  Diligent::ITextureView* layerRTV = nullptr;
  Diligent::ITextureView* layerSRV = nullptr;
  Diligent::ITextureView* layerFloatSRV = nullptr;
  Diligent::ITextureView* layerFloatUAV = nullptr;
  Diligent::ITextureView* accumSRV = nullptr;
  Diligent::ITextureView* tempUAV = nullptr;
};

struct RenderPassContext {
  ArtifactIRenderer* renderer = nullptr;
  quint64 frame = 0;
};

}

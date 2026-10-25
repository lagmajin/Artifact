module;

#include <QSize>
#include <QString>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

export module Artifact.Widgets.CompositionSpaceGpuCache;

import Artifact.Render.IRenderer;

export namespace Artifact {

// Owns the bounded offscreen targets used by the opt-in composition-space
// presentation cache. The renderer remains the GPU resource factory and the
// caller remains responsible for using this cache on its renderer owner lane.
struct CompositionSpaceGpuCache final {
  void* colorTargetView = nullptr;
  Diligent::ITextureView* colorShaderResourceView = nullptr;
  void* postProcessTargetView = nullptr;
  Diligent::ITextureView* postProcessShaderResourceView = nullptr;
  Diligent::ITextureView* postProcessUnorderedAccessView = nullptr;
  QSize size;
  QString contentKey;

  bool isReady() const {
    return colorTargetView && colorShaderResourceView && !size.isEmpty() &&
           !contentKey.isEmpty();
  }

  void clear(ArtifactIRenderer* renderer) {
    if (renderer && colorTargetView) {
      renderer->destroyOffscreenTexture(colorTargetView);
    }
    if (renderer && postProcessTargetView) {
      renderer->destroyOffscreenTexture(postProcessTargetView);
    }
    *this = {};
  }

  bool ensure(ArtifactIRenderer* renderer, const QSize& requestedSize) {
    if (!renderer || requestedSize.width() <= 0 ||
        requestedSize.height() <= 0) {
      return false;
    }
    if (colorTargetView && size == requestedSize &&
        colorShaderResourceView && postProcessTargetView &&
        postProcessShaderResourceView && postProcessUnorderedAccessView) {
      return true;
    }

    clear(renderer);
    colorTargetView =
        renderer->createOffscreenTexture(requestedSize.width(),
                                         requestedSize.height());
    colorShaderResourceView =
        renderer->offscreenTextureShaderResourceView(colorTargetView);
    postProcessTargetView = renderer->createOffscreenComputeTexture(
        requestedSize.width(), requestedSize.height());
    postProcessShaderResourceView =
        renderer->offscreenTextureShaderResourceView(postProcessTargetView);
    postProcessUnorderedAccessView =
        renderer->offscreenTextureUnorderedAccessView(postProcessTargetView);
    size = requestedSize;
    if (!colorTargetView || !colorShaderResourceView ||
        !postProcessTargetView || !postProcessShaderResourceView ||
        !postProcessUnorderedAccessView) {
      clear(renderer);
      return false;
    }
    return true;
  }
};

} // namespace Artifact

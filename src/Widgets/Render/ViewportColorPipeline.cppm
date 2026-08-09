module;

#include <QString>
#include <QVector>
#include <memory>

module Artifact.Widgets.ViewportColorPipeline;

import Artifact.Render.FinalPostProcess;
import Artifact.Color.OCIOManager;
import Graphics.GPUcomputeContext;

namespace Artifact {

class ViewportColorPipeline::Impl {
public:
  std::unique_ptr<ArtifactCore::GpuContext> gpuContext;
  std::unique_ptr<ArtifactFinalPostProcess> postProcess;
  QString lutKey;
};

ViewportColorPipeline::ViewportColorPipeline(Diligent::IRenderDevice* device,
                                             Diligent::IDeviceContext* context)
    : impl_(new Impl()) {
  if (device && context) {
    impl_->gpuContext = std::make_unique<ArtifactCore::GpuContext>(device, context);
    impl_->postProcess =
        std::make_unique<ArtifactFinalPostProcess>(*impl_->gpuContext);
  }
}

ViewportColorPipeline::~ViewportColorPipeline() {
  delete impl_;
  impl_ = nullptr;
}

void ViewportColorPipeline::clear() {
  if (impl_ && impl_->postProcess) {
    impl_->postProcess->clearLUT();
  }
  if (impl_) {
    impl_->lutKey.clear();
  }
}

Diligent::ITextureView* ViewportColorPipeline::apply(
    Diligent::IDeviceContext* context, Diligent::ITextureView* source,
    Diligent::ITextureView* destination, int width, int height) {
  if (!impl_ || !impl_->postProcess || !context || !source || !destination) {
    return source;
  }

  auto* ocio = ArtifactOCIOManager::instance();
  const auto* config = ocio ? ocio->activeConfig() : nullptr;
  const QString key =
      config ? QStringLiteral("%1|%2|%3|%4|ev=%5|gamma=%6")
                    .arg(ocio->workingSpace(), ocio->display(), ocio->view(),
                         ocio->looks())
                    .arg(ocio->viewerExposure(), 0, 'f', 4)
                    .arg(ocio->viewerGamma(), 0, 'f', 4)
             : QString();
  if (config && !key.isEmpty() && key != impl_->lutKey) {
    QVector<float> lut;
    if (ocio->bakeViewTransformLUT(33, lut, 0.0f, 4.0f)) {
      impl_->postProcess->updateFromColorLUT(lut.constData(), 33);
      impl_->postProcess->setLUTInputDomain(0.0f, 4.0f);
      impl_->postProcess->setViewTransformEnabled(true);
      impl_->lutKey = key;
    } else {
      clear();
    }
  } else if (!config || key.isEmpty()) {
    clear();
  }
  if (!impl_->postProcess->hasActiveLUT()) {
    return source;
  }
  return impl_->postProcess->apply(context, source, destination, width, height)
             ? destination
             : source;
}

} // namespace Artifact

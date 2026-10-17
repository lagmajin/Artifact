module;

#include <QString>
#include <QVector>
#include <utility>

module Artifact.Widgets.ViewportColorPipeline;

import Artifact.Color.OCIOManager;

namespace Artifact {

class ViewportColorPipeline::Impl {
public:
  QString lutKey;
  QVector<float> lut;
  bool active = false;
};

ViewportColorPipeline::ViewportColorPipeline() : impl_(new Impl()) {}

ViewportColorPipeline::~ViewportColorPipeline() {
  delete impl_;
  impl_ = nullptr;
}

void ViewportColorPipeline::clear() {
  if (impl_) {
    impl_->lutKey.clear();
    impl_->lut.clear();
    impl_->active = false;
  }
}

ViewportColorLutUpdate ViewportColorPipeline::prepare() {
  if (!impl_) return {};
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
      impl_->lut = std::move(lut);
      impl_->lutKey = key;
      impl_->active = true;
      return {impl_->lut.constData(), 33, 0.0f, 4.0f, true, false, true};
    } else {
      const bool shouldClear = impl_->active;
      clear();
      return {nullptr, 0, 0.0f, 4.0f, false, shouldClear, false};
    }
  } else if (!config || key.isEmpty()) {
    const bool shouldClear = impl_->active;
    clear();
    return {nullptr, 0, 0.0f, 4.0f, false, shouldClear, false};
  }
  return {nullptr, 0, 0.0f, 4.0f, false, false, impl_->active};
}

} // namespace Artifact

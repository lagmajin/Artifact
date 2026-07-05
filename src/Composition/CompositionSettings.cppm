module;

#include <string>

// composition settings are a tiny value object; keep the implementation local to this module.
// The public declarations live in include/Composition/CompositionSettings.ixx.

module Composition.Settings;

import Utils.String.UniString;

namespace ArtifactCore {

class CompositionSettings::Impl {
public:
  UniString compositionName_;
  QSize compositionSize_;

  Impl()
      : compositionName_(std::string("New Composition")),
        compositionSize_(1920, 1080) {}
};

CompositionSettings::CompositionSettings()
    : impl_(new Impl()) {}

CompositionSettings::CompositionSettings(const CompositionSettings& settings)
    : impl_(new Impl(*settings.impl_)) {}

CompositionSettings::~CompositionSettings() {
  delete impl_;
}

UniString CompositionSettings::compositionName() const {
  return impl_->compositionName_;
}

void CompositionSettings::setCompositionName(const UniString& string) {
  impl_->compositionName_ = string;
}

QSize CompositionSettings::compositionSize() const {
  return impl_->compositionSize_;
}

void CompositionSettings::setCompositionSize(const QSize& size) {
  impl_->compositionSize_ = size;
}

} // namespace ArtifactCore

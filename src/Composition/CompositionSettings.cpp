module ;
#include <QString>
#include <QSize>

module Composition.Settings;

import std;
import Utils;
import Utils.String.UniString;

namespace ArtifactCore {

class CompositionSettings::Impl {
public:
    UniString compositionName_;
    QSize compositionSize_;

    Impl()
        : compositionName_(std::string("")), compositionSize_(0, 0)
    {}

    Impl(const Impl& other) = default;
    ~Impl() = default;
};

CompositionSettings::CompositionSettings()
    : impl_(new Impl())
{
}

CompositionSettings::CompositionSettings(const CompositionSettings& settings)
    : impl_(new Impl(*settings.impl_))
{
}

CompositionSettings::~CompositionSettings()
{
    delete impl_;
}

UniString CompositionSettings::compositionName() const
{
    return impl_->compositionName_;
}

void CompositionSettings::setCompositionName(const UniString& string)
{
    impl_->compositionName_ = string;
}

QSize CompositionSettings::compositionSize() const
{
    return impl_->compositionSize_;
}

void CompositionSettings::setCompositionSize(const QSize& size)
{
    impl_->compositionSize_ = size;
}

} // namespace ArtifactCore

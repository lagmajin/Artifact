module ;
#include <string.h>
#include <string.h>
#include <QString>
#include <QSize>

module Composition.Settings;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>



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

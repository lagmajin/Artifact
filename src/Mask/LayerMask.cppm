module;
#include <vector>
#include <cmath>
#include <algorithm>

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
#include <opencv2/opencv.hpp>
module Artifact.Mask.LayerMask;




import Artifact.Mask.Path;

namespace Artifact {

class LayerMask::Impl {
public:
    std::vector<MaskPath> paths;
    bool enabled = true;
};

LayerMask::LayerMask() : impl_(new Impl()) {}
LayerMask::~LayerMask() { delete impl_; }

LayerMask::LayerMask(const LayerMask& other) : impl_(new Impl(*other.impl_)) {}
LayerMask& LayerMask::operator=(const LayerMask& other) {
    if (this != &other) {
        delete impl_;
        impl_ = new Impl(*other.impl_);
    }
    return *this;
}

void LayerMask::addMaskPath(const MaskPath& path) { impl_->paths.push_back(path); }

void LayerMask::removeMaskPath(int index) {
    if (index >= 0 && index < static_cast<int>(impl_->paths.size()))
        impl_->paths.erase(impl_->paths.begin() + index);
}

void LayerMask::setMaskPath(int index, const MaskPath& path) {
    if (index >= 0 && index < static_cast<int>(impl_->paths.size()))
        impl_->paths[index] = path;
}

MaskPath LayerMask::maskPath(int index) const {
    if (index >= 0 && index < static_cast<int>(impl_->paths.size()))
        return impl_->paths[index];
    return {};
}

int LayerMask::maskPathCount() const { return static_cast<int>(impl_->paths.size()); }

void LayerMask::clearMaskPaths() { impl_->paths.clear(); }

bool LayerMask::isEnabled() const { return impl_->enabled; }
void LayerMask::setEnabled(bool enabled) { impl_->enabled = enabled; }

void LayerMask::compositeAlphaMask(int width, int height, void* outMat) const
{
    cv::Mat& dst = *static_cast<cv::Mat*>(outMat);

    if (!impl_->enabled || impl_->paths.empty()) {
        // no mask = fully opaque
        dst = cv::Mat::ones(height, width, CV_32FC1);
        return;
    }

    // Start with all zeros (transparent); first Add mask will define visibility
    dst = cv::Mat::zeros(height, width, CV_32FC1);
    bool firstAdd = true;

    for (const auto& path : impl_->paths) {
        cv::Mat pathMask;
        path.rasterizeToAlpha(width, height, &pathMask);

        switch (path.mode()) {
            case MaskMode::Add:
                if (firstAdd) {
                    dst = pathMask.clone();
                    firstAdd = false;
                } else {
                    // Union: max
                    cv::max(dst, pathMask, dst);
                }
                break;
            case MaskMode::Subtract:
                // dst = dst * (1 - pathMask)
                {
                    cv::Mat inv = cv::Scalar(1.0f) - pathMask;
                    cv::multiply(dst, inv, dst);
                }
                break;
            case MaskMode::Intersect:
                cv::multiply(dst, pathMask, dst);
                break;
            case MaskMode::Difference:
                // dst = |dst - pathMask|
                cv::absdiff(dst, pathMask, dst);
                break;
        }
    }

    // Clamp 0~1
    cv::min(dst, 1.0f, dst);
    cv::max(dst, 0.0f, dst);
}

void LayerMask::applyToImage(int width, int height, void* imageMat) const
{
    cv::Mat& img = *static_cast<cv::Mat*>(imageMat);
    if (img.empty() || img.type() != CV_32FC4) return;
    if (!impl_->enabled || impl_->paths.empty()) return;

    cv::Mat alphaMask;
    compositeAlphaMask(width, height, &alphaMask);

    // Split channels, multiply alpha by mask, merge back
    std::vector<cv::Mat> channels(4);
    cv::split(img, channels);
    cv::multiply(channels[3], alphaMask, channels[3]);
    cv::merge(channels, img);
}

}

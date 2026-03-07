module;
#include <opencv2/opencv.hpp>
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
export module Image.ImageF32xN;

export namespace ArtifactCore {

class ImageF32xN {
private:
    int width_ = 0;
    int height_ = 0;
    int channels_ = 0;
    std::vector<float> data_; // row-major, channel-last: [r,g,b,a,...]
public:
    ImageF32xN() = default;
    ImageF32xN(int width, int height, int channels) {
        resize(width, height, channels);
    }

    int width() const { return width_; }
    int height() const { return height_; }
    int channels() const { return channels_; }

    void resize(int width, int height, int channels) {
        width_ = width; height_ = height; channels_ = channels;
        data_.assign(static_cast<size_t>(width_) * static_cast<size_t>(height_) * static_cast<size_t>(channels_), 0.0f);
    }

    // Populate from an OpenCV cv::Mat. Supported input types: CV_8UCn, CV_32FCn
    void setFromCVMat(const cv::Mat& mat) {
        if (mat.empty()) return;
        cv::Mat fmat;
        if (mat.depth() == CV_32F) fmat = mat;
        else mat.convertTo(fmat, CV_32F, 1.0/255.0);

        int ch = fmat.channels();
        int w = fmat.cols;
        int h = fmat.rows;
        resize(w,h,ch);

        // Copy data channel-last
        for (int y=0;y<h;++y) {
            const float* row = fmat.ptr<float>(y);
            for (int x=0;x<w;++x) {
                for (int c=0;c<ch;++c) {
                    data_[static_cast<size_t>(y*w + x)*static_cast<size_t>(ch) + static_cast<size_t>(c)] = row[x*ch + c];
                }
            }
        }
    }

    // Returns a CV_32F cv::Mat with channel count == channels()
    cv::Mat toCVMat() const {
        if (width_<=0||height_<=0||channels_<=0) return cv::Mat();
        cv::Mat mat(height_, width_, CV_MAKETYPE(CV_32F, channels_));
        for (int y=0;y<height_;++y) {
            float* row = mat.ptr<float>(y);
            for (int x=0;x<width_;++x) {
                for (int c=0;c<channels_;++c) {
                    row[x*channels_ + c] = data_[static_cast<size_t>(y*width_ + x)*static_cast<size_t>(channels_) + static_cast<size_t>(c)];
                }
            }
        }
        return mat;
    }

    const float* data() const { return data_.data(); }
    float* data() { return data_.data(); }

    // Return a copy with channels remapped according to map.
    // map is a vector of source channel indices for each output channel.
    // If an index is out of range, the channel is filled with padValue.
    ImageF32xN remapChannels(const std::vector<int>& map, float padValue = 0.0f) const {
        ImageF32xN out(width_, height_, static_cast<int>(map.size()));
        if (width_<=0||height_<=0) return out;
        for (int y=0;y<height_;++y) {
            for (int x=0;x<width_;++x) {
                for (size_t oc=0; oc<map.size(); ++oc) {
                    int srcc = map[oc];
                    float v = padValue;
                    if (srcc >= 0 && srcc < channels_) {
                        v = data_[static_cast<size_t>(y*width_ + x)*static_cast<size_t>(channels_) + static_cast<size_t>(srcc)];
                    }
                    out.data_[static_cast<size_t>(y*width_ + x)*static_cast<size_t>(map.size()) + oc] = v;
                }
            }
        }
        return out;
    }

    // Pad or truncate channels in-place to newChannels. If padding, new channels filled with padValue.
    void padOrTruncateChannels(int newChannels, float padValue = 0.0f) {
        if (newChannels == channels_) return;
        if (newChannels <= 0) {
            data_.clear(); width_ = height_ = channels_ = 0; return;
        }
        std::vector<float> newData(static_cast<size_t>(width_) * static_cast<size_t>(height_) * static_cast<size_t>(newChannels), padValue);
        int minCh = std::min(newChannels, channels_);
        for (int y=0;y<height_;++y) {
            for (int x=0;x<width_;++x) {
                for (int c=0;c<minCh;++c) {
                    newData[static_cast<size_t>(y*width_ + x)*static_cast<size_t>(newChannels) + c] =
                        data_[static_cast<size_t>(y*width_ + x)*static_cast<size_t>(channels_) + c];
                }
            }
        }
        data_.swap(newData);
        channels_ = newChannels;
    }

    // Extract a single channel into a new ImageF32xN with channels==1. If idx out of range, result is zeros.
    ImageF32xN extractChannel(int idx) const {
        ImageF32xN out(width_, height_, 1);
        if (idx < 0 || idx >= channels_) return out;
        for (int y=0;y<height_;++y) {
            for (int x=0;x<width_;++x) {
                out.data_[static_cast<size_t>(y*width_ + x)] = data_[static_cast<size_t>(y*width_ + x)*static_cast<size_t>(channels_) + static_cast<size_t>(idx)];
            }
        }
        return out;
    }
};

using ImageF32xNPtr = std::shared_ptr<ImageF32xN>;

}

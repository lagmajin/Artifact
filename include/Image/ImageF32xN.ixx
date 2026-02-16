module;
#include <opencv2/opencv.hpp>
export module Image.ImageF32xN;

import std;
import <vector>;
import <memory>;

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
};

using ImageF32xNPtr = std::shared_ptr<ImageF32xN>;

}

module;
#include <opencv2/opencv.hpp>
export module Image.ImageF32x4_RGBA.Compat;

import Image.ImageF32xN;
import Image.ImageF32x4_RGBA;

export namespace ArtifactCore {

// Thin compatibility helpers between ImageF32xN and ImageF32x4_RGBA
inline ImageF32x4_RGBA ImageFromF32xN(const ImageF32xN& src, bool srcIsBGR = true, bool srcHasAlpha = false, float alphaValue = 1.0f) {
    ImageF32x4_RGBA out;
    cv::Mat m = src.toCVMat();
    if (m.empty()) return out;

    cv::Mat conv;
    int ch = m.channels();
    if (ch == 4) {
        // assume data is RGBA or BGRA depending on srcIsBGR
        if (srcIsBGR) cv::cvtColor(m, conv, cv::COLOR_BGRA2RGBA);
        else conv = m.clone();
    } else if (ch == 3) {
        if (srcIsBGR) cv::cvtColor(m, conv, cv::COLOR_BGR2RGBA);
        else cv::cvtColor(m, conv, cv::COLOR_RGB2RGBA);
    } else if (ch == 1) {
        cv::cvtColor(m, conv, cv::COLOR_GRAY2RGBA);
    } else {
        // fallback: expand channels to 4 by padding
        cv::Mat tmp = m.clone();
        tmp.convertTo(tmp, CV_32F);
        std::vector<cv::Mat> channels;
        for (int i = 0; i < ch; ++i) {
            cv::Mat c;
            cv::extractChannel(tmp, c, i);
            channels.push_back(c);
        }
        while (channels.size() < 3) channels.push_back(cv::Mat::zeros(tmp.rows, tmp.cols, CV_32F));
        channels.push_back(cv::Mat(tmp.rows, tmp.cols, CV_32F, cv::Scalar(alphaValue)));
        cv::merge(channels, conv);
    }

    // If source did not have alpha, ensure alpha channel is set
    if (!srcHasAlpha && conv.channels() == 4) {
        std::vector<cv::Mat> chs;
        cv::split(conv, chs);
        chs[3].setTo(alphaValue);
        cv::merge(chs, conv);
    }

    // conv is float or non-float; setFromCVMat will normalize/convert
    out.setFromCVMat(conv);
    return out;
}

inline ImageF32xN ImageToF32xN(const ImageF32x4_RGBA& src, int outChannels = 4, bool outIsBGR = false) {
    ImageF32xN out;
    cv::Mat m = src.toCVMat();
    if (m.empty()) return out;

    cv::Mat conv;
    if (outChannels == 4) {
        if (outIsBGR) cv::cvtColor(m, conv, cv::COLOR_RGBA2BGRA);
        else conv = m.clone();
    } else if (outChannels == 3) {
        if (outIsBGR) cv::cvtColor(m, conv, cv::COLOR_RGBA2BGR);
        else cv::cvtColor(m, conv, cv::COLOR_RGBA2RGB);
    } else if (outChannels == 1) {
        cv::cvtColor(m, conv, cv::COLOR_RGBA2GRAY);
    } else {
        // unsupported, default to 4
        conv = m.clone();
    }

    out.setFromCVMat(conv);
    return out;
}

// Build an ImageF32x4_RGBA from planar Y, U, V mats (Y: HxW, U/V: H/2 x W/2 or HxW)
inline ImageF32x4_RGBA ImageFromYUV420Planes(const cv::Mat& yPlane, const cv::Mat& uPlane, const cv::Mat& vPlane) {
    ImageF32x4_RGBA out;
    if (yPlane.empty()) return out;

    // Convert planes to CV_32F normalized [0,1]
    cv::Mat yf, uf, vf;
    if (yPlane.depth() == CV_32F) yf = yPlane; else yPlane.convertTo(yf, CV_32F, 1.0/255.0);
    if (uPlane.empty()) {
        // treat as zeros
        uf = cv::Mat::zeros(yf.size(), CV_32F);
    } else if (uPlane.depth() == CV_32F) uf = uPlane; else uPlane.convertTo(uf, CV_32F, 1.0/255.0);
    if (vPlane.empty()) {
        vf = cv::Mat::zeros(yf.size(), CV_32F);
    } else if (vPlane.depth() == CV_32F) vf = vPlane; else vPlane.convertTo(vf, CV_32F, 1.0/255.0);

    // Upsample U/V if needed
    if (uf.size() != yf.size()) cv::resize(uf, uf, yf.size(), 0, 0, cv::INTER_LINEAR);
    if (vf.size() != yf.size()) cv::resize(vf, vf, yf.size(), 0, 0, cv::INTER_LINEAR);

    // Merge into YUV image (using YCrCb ordering expected by OpenCV conversions)
    std::vector<cv::Mat> ycrcb{yf, vf, uf}; // Y, Cr, Cb
    cv::Mat ycrcb3;
    cv::merge(ycrcb, ycrcb3);

    // Convert to BGR then to RGBA float
    cv::Mat bgr;
    ycrcb3.convertTo(ycrcb3, CV_32F);
    cv::cvtColor(ycrcb3, bgr, cv::COLOR_YCrCb2BGR);
    cv::Mat rgba;
    cv::cvtColor(bgr, rgba, cv::COLOR_BGR2RGBA);
    rgba.convertTo(rgba, CV_32F);
    out.setFromCVMat(rgba);
    return out;
}

// Build an ImageF32x4_RGBA from NV12 buffers (Y plane and interleaved UV plane)
inline ImageF32x4_RGBA ImageFromNV12(const cv::Mat& yPlane, const cv::Mat& uvInterleaved) {
    ImageF32x4_RGBA out;
    if (yPlane.empty() || uvInterleaved.empty()) return out;

    // Ensure 8-bit input for OpenCV NV12 conversion
    cv::Mat y8, uv8;
    if (yPlane.depth() == CV_8U) y8 = yPlane; else { yPlane.convertTo(y8, CV_8U, 255.0); }
    if (uvInterleaved.depth() == CV_8U) uv8 = uvInterleaved; else { uvInterleaved.convertTo(uv8, CV_8U, 255.0); }

    // Combine into single Mat (height = H + H/2) for cvtColor NV12
    cv::Mat combined;
    cv::vconcat(y8, uv8, combined);
    cv::Mat rgba;
    cv::cvtColor(combined, rgba, cv::COLOR_YUV2RGBA_NV12);
    rgba.convertTo(rgba, CV_32F, 1.0/255.0);
    out.setFromCVMat(rgba);
    return out;
}

}

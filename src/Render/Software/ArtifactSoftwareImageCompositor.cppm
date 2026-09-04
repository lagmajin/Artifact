module;
#include <utility>
#include <QImage>
#include <QPainter>
#include <QColor>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <opencv2/opencv.hpp>

module Artifact.Render.SoftwareCompositor;

import Layer.Blend;
import Color.Float;
import Color.Conversion;
import Color.Luminance;
import Core.Parallel;
import Image.ImageF32x4_RGBA;

namespace Artifact::SoftwareRender {

namespace {

inline float clamp01(const float value) {
 return std::clamp(value, 0.0f, 1.0f);
}

inline float outAlpha(const float srcAlpha, const float dstAlpha) {
 return srcAlpha + dstAlpha * (1.0f - srcAlpha);
}

inline ArtifactCore::FloatColor composeBlendResult(const ArtifactCore::FloatColor& base,
                                                   const ArtifactCore::FloatColor& blendColor,
                                                   const float srcAlpha,
                                                   const ArtifactCore::FloatColor& blendedStraight) {
 const float dstAlpha = clamp01(base.a());
 const float outA = outAlpha(srcAlpha, dstAlpha);
 const float premulR =
  base.r() * dstAlpha * (1.0f - srcAlpha) +
  (blendedStraight.r() * dstAlpha + blendColor.r() * (1.0f - dstAlpha)) * srcAlpha;
 const float premulG =
  base.g() * dstAlpha * (1.0f - srcAlpha) +
  (blendedStraight.g() * dstAlpha + blendColor.g() * (1.0f - dstAlpha)) * srcAlpha;
 const float premulB =
  base.b() * dstAlpha * (1.0f - srcAlpha) +
  (blendedStraight.b() * dstAlpha + blendColor.b() * (1.0f - dstAlpha)) * srcAlpha;
 if (outA <= 1e-6f) {
  return ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f);
 }
 return ArtifactCore::FloatColor(clamp01(premulR / outA),
                                 clamp01(premulG / outA),
                                 clamp01(premulB / outA),
                                 outA);
}

inline ArtifactCore::FloatColor applyStencilLikeBlend(const ArtifactCore::FloatColor& base,
                                                      const float factor) {
 const float clampedFactor = clamp01(factor);
 return ArtifactCore::FloatColor(base.r(), base.g(), base.b(), clamp01(base.a() * clampedFactor));
}

inline ArtifactCore::FloatColor applySilhouetteLikeBlend(const ArtifactCore::FloatColor& base,
                                                         const float factor) {
 const float clampedFactor = clamp01(1.0f - factor);
 return ArtifactCore::FloatColor(base.r(), base.g(), base.b(), clamp01(base.a() * clampedFactor));
}

inline float blendAdd(const float b, const float f) { return std::min(b + f, 1.0f); }
inline float blendSubtract(const float b, const float f) { return std::max(b - f, 0.0f); }
inline float blendMultiply(const float b, const float f) { return b * f; }
inline float blendScreen(const float b, const float f) { return 1.0f - (1.0f - b) * (1.0f - f); }
inline float blendOverlay(const float b, const float f) {
 return (b < 0.5f) ? (2.0f * b * f) : (1.0f - 2.0f * (1.0f - b) * (1.0f - f));
}
inline float blendDarken(const float b, const float f) { return std::min(b, f); }
inline float blendLighten(const float b, const float f) { return std::max(b, f); }
inline float blendColorDodge(const float b, const float f) {
 if (f == 1.0f) return 1.0f;
 return std::min(1.0f, b / (1.0f - f));
}
inline float blendColorBurn(const float b, const float f) {
 if (f == 0.0f) return 0.0f;
 return std::max(0.0f, 1.0f - (1.0f - b) / f);
}
inline float blendHardLight(const float b, const float f) { return blendOverlay(f, b); }
inline float blendSoftLight(const float b, const float f) {
 if (f < 0.5f) {
  return b - (1.0f - 2.0f * f) * b * (1.0f - b);
 }
 const float d = (b <= 0.25f) ? ((16.0f * b - 12.0f) * b + 4.0f) * b : std::sqrt(b);
 return b + (2.0f * f - 1.0f) * (d - b);
}
inline float blendDifference(const float b, const float f) { return std::abs(b - f); }
inline float blendExclusion(const float b, const float f) { return b + f - 2.0f * b * f; }
inline float blendLinearBurn(const float b, const float f) { return std::max(b + f - 1.0f, 0.0f); }
inline float blendDivide(const float b, const float f) { return std::min(b / std::max(f, 1e-6f), 1.0f); }
inline float blendPinLight(const float b, const float f) {
 return (f < 0.5f) ? std::min(b, 2.0f * f) : std::max(b, 2.0f * (f - 0.5f));
}
inline float blendVividLight(const float b, const float f) {
 return (f < 0.5f)
  ? (f == 0.0f ? 0.0f : std::max(1.0f - (1.0f - b) / (2.0f * f), 0.0f))
  : (f == 1.0f ? 1.0f : std::min(b / (2.0f * (1.0f - f)), 1.0f));
}
inline float blendLinearLight(const float b, const float f) { return std::clamp(b + 2.0f * f - 1.0f, 0.0f, 1.0f); }
inline float blendHardMix(const float b, const float f) { return (b + f >= 1.0f) ? 1.0f : 0.0f; }

inline ArtifactCore::FloatColor blendColor(const ArtifactCore::FloatColor& base,
                                           const ArtifactCore::FloatColor& blendColor,
                                           const ArtifactCore::BlendMode mode,
                                           const float opacity) {
 const float srcAlpha = clamp01(opacity);
 if (srcAlpha <= 0.0f) {
  return base;
 }

 const auto applyBlend = [&](float (*blendFunc)(float, float)) -> ArtifactCore::FloatColor {
  const ArtifactCore::FloatColor blendedStraight(
   clamp01(blendFunc(base.r(), blendColor.r())),
   clamp01(blendFunc(base.g(), blendColor.g())),
   clamp01(blendFunc(base.b(), blendColor.b())),
   1.0f);
  return composeBlendResult(base, blendColor, srcAlpha, blendedStraight);
 };

 switch (mode) {
 case ArtifactCore::BlendMode::Normal: return applyBlend([](float, float f) { return f; });
 case ArtifactCore::BlendMode::Add: return applyBlend(blendAdd);
 case ArtifactCore::BlendMode::Subtract: return applyBlend(blendSubtract);
 case ArtifactCore::BlendMode::Multiply: return applyBlend(blendMultiply);
 case ArtifactCore::BlendMode::Screen: return applyBlend(blendScreen);
 case ArtifactCore::BlendMode::Overlay: return applyBlend(blendOverlay);
 case ArtifactCore::BlendMode::Darken: return applyBlend(blendDarken);
 case ArtifactCore::BlendMode::Lighten: return applyBlend(blendLighten);
 case ArtifactCore::BlendMode::ColorDodge: return applyBlend(blendColorDodge);
 case ArtifactCore::BlendMode::ColorBurn: return applyBlend(blendColorBurn);
 case ArtifactCore::BlendMode::HardLight: return applyBlend(blendHardLight);
 case ArtifactCore::BlendMode::SoftLight: return applyBlend(blendSoftLight);
 case ArtifactCore::BlendMode::Difference: return applyBlend(blendDifference);
 case ArtifactCore::BlendMode::Exclusion: return applyBlend(blendExclusion);
 case ArtifactCore::BlendMode::LinearBurn: return applyBlend(blendLinearBurn);
 case ArtifactCore::BlendMode::Divide: return applyBlend(blendDivide);
 case ArtifactCore::BlendMode::PinLight: return applyBlend(blendPinLight);
 case ArtifactCore::BlendMode::VividLight: return applyBlend(blendVividLight);
 case ArtifactCore::BlendMode::LinearLight: return applyBlend(blendLinearLight);
 case ArtifactCore::BlendMode::HardMix: return applyBlend(blendHardMix);
 case ArtifactCore::BlendMode::ClassicColorBurn: return applyBlend(blendColorBurn);
 case ArtifactCore::BlendMode::LinearDodge: return applyBlend(blendAdd);
 case ArtifactCore::BlendMode::ClassicColorDodge: return applyBlend(blendColorDodge);
 case ArtifactCore::BlendMode::ClassicDifference: return applyBlend(blendDifference);
 case ArtifactCore::BlendMode::Hue:
 case ArtifactCore::BlendMode::Saturation:
 case ArtifactCore::BlendMode::Color:
 case ArtifactCore::BlendMode::Luminosity: {
  const ArtifactCore::HSLColor baseHsl = ArtifactCore::ColorConversion::RGBToHSL(base.r(), base.g(), base.b());
  const ArtifactCore::HSLColor blendHsl = ArtifactCore::ColorConversion::RGBToHSL(blendColor.r(), blendColor.g(), blendColor.b());
  ArtifactCore::HSLColor resultHsl = baseHsl;
  if (mode == ArtifactCore::BlendMode::Hue || mode == ArtifactCore::BlendMode::Color) {
   resultHsl.h = blendHsl.h;
  }
  if (mode == ArtifactCore::BlendMode::Saturation || mode == ArtifactCore::BlendMode::Color) {
   resultHsl.s = blendHsl.s;
  }
  if (mode == ArtifactCore::BlendMode::Luminosity) {
   resultHsl.l = blendHsl.l;
  }
  const auto rgb = ArtifactCore::ColorConversion::HSLToRGB(resultHsl);
  return composeBlendResult(
   base, blendColor, srcAlpha,
   ArtifactCore::FloatColor(clamp01(rgb[0]), clamp01(rgb[1]), clamp01(rgb[2]), 1.0f));
 }
 case ArtifactCore::BlendMode::Dissolve:
 case ArtifactCore::BlendMode::DancingDissolve:
  return composeBlendResult(base, blendColor, srcAlpha, blendColor);
 case ArtifactCore::BlendMode::StencilAlpha:
  return applyStencilLikeBlend(base, srcAlpha);
 case ArtifactCore::BlendMode::StencilLuma:
  return applyStencilLikeBlend(
   base, clamp01(ArtifactCore::ColorLuminance::calculate(blendColor.r(), blendColor.g(), blendColor.b()) * srcAlpha));
 case ArtifactCore::BlendMode::SilhouetteAlpha:
  return applySilhouetteLikeBlend(base, srcAlpha);
 case ArtifactCore::BlendMode::SilhouetteLuma:
  return applySilhouetteLikeBlend(
   base, clamp01(ArtifactCore::ColorLuminance::calculate(blendColor.r(), blendColor.g(), blendColor.b()) * srcAlpha));
 default:
  return base;
 }
}

QPainter::CompositionMode qPainterCompositionModeImpl(const ArtifactCore::BlendMode mode)
{
 switch (mode) {
 // QPainter has no native Subtract; the exact float/CV implementation lives
 // in blendChannel(). Approximating it as Difference changes the result, so
 // QPainter consumers fall back to SourceOver.
 case ArtifactCore::BlendMode::Add: return QPainter::CompositionMode_Plus;
 case ArtifactCore::BlendMode::Multiply: return QPainter::CompositionMode_Multiply;
 case ArtifactCore::BlendMode::Screen: return QPainter::CompositionMode_Screen;
 case ArtifactCore::BlendMode::Overlay: return QPainter::CompositionMode_Overlay;
 case ArtifactCore::BlendMode::Darken: return QPainter::CompositionMode_Darken;
 case ArtifactCore::BlendMode::Lighten: return QPainter::CompositionMode_Lighten;
 case ArtifactCore::BlendMode::ColorDodge: return QPainter::CompositionMode_ColorDodge;
 case ArtifactCore::BlendMode::ColorBurn: return QPainter::CompositionMode_ColorBurn;
 case ArtifactCore::BlendMode::HardLight: return QPainter::CompositionMode_HardLight;
 case ArtifactCore::BlendMode::SoftLight: return QPainter::CompositionMode_SoftLight;
 case ArtifactCore::BlendMode::Difference: return QPainter::CompositionMode_Difference;
 case ArtifactCore::BlendMode::Exclusion: return QPainter::CompositionMode_Exclusion;
 case ArtifactCore::BlendMode::Hue:
 case ArtifactCore::BlendMode::Saturation:
 case ArtifactCore::BlendMode::Color:
 case ArtifactCore::BlendMode::Luminosity:
 case ArtifactCore::BlendMode::Dissolve:
 case ArtifactCore::BlendMode::DancingDissolve:
 case ArtifactCore::BlendMode::ClassicColorBurn:
 case ArtifactCore::BlendMode::LinearDodge:
 case ArtifactCore::BlendMode::ClassicColorDodge:
 case ArtifactCore::BlendMode::ClassicDifference:
 case ArtifactCore::BlendMode::StencilAlpha:
 case ArtifactCore::BlendMode::StencilLuma:
 case ArtifactCore::BlendMode::SilhouetteAlpha:
 case ArtifactCore::BlendMode::SilhouetteLuma:
 case ArtifactCore::BlendMode::Subtract:
 case ArtifactCore::BlendMode::Normal:
 default:
  return QPainter::CompositionMode_SourceOver;
 }
}

QImage buildOverlayCanvas(const CompositeRequest& request, const QSize& outSize)
{
 if (request.overlay.isNull()) {
  return {};
 }
 QImage overlayCanvas(outSize, QImage::Format_RGBA8888);
 overlayCanvas.fill(Qt::transparent);

 QImage ovScaled = request.overlay;
 if (!ovScaled.isNull() && std::abs(request.overlayScale - 1.0f) > 0.0001f) {
  const int sw = std::max(1, static_cast<int>(std::round(ovScaled.width() * request.overlayScale)));
  const int sh = std::max(1, static_cast<int>(std::round(ovScaled.height() * request.overlayScale)));
  ovScaled = ovScaled.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
 }

 QPainter p(&overlayCanvas);
 p.setRenderHint(QPainter::SmoothPixmapTransform, true);
 p.translate(outSize.width() * 0.5 + request.overlayOffset.x(),
             outSize.height() * 0.5 + request.overlayOffset.y());
 p.rotate(request.overlayRotationDeg);
 p.translate(-ovScaled.width() * 0.5, -ovScaled.height() * 0.5);
 p.drawImage(0, 0, ovScaled);
 return overlayCanvas;
}

cv::Mat qImageToMatRGBA(const QImage& image)
{
 QImage rgba = (image.format() == QImage::Format_RGBA8888)
                   ? image
                   : image.convertToFormat(QImage::Format_RGBA8888);
 cv::Mat view(rgba.height(), rgba.width(), CV_8UC4, const_cast<uchar*>(rgba.bits()), rgba.bytesPerLine());
 return view.clone();
}

QImage matRGBAToQImage(const cv::Mat& mat)
{
 if (mat.empty()) {
  return {};
 }
 cv::Mat rgba;
 if (mat.type() == CV_8UC4) {
  rgba = mat;
 } else if (mat.type() == CV_8UC3) {
  cv::cvtColor(mat, rgba, cv::COLOR_BGR2RGBA);
 } else {
  return {};
 }
 QImage image(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step), QImage::Format_RGBA8888);
 return image.copy();
}

float blendChannel(const float dst, const float src, const ArtifactCore::BlendMode mode)
{
 const float d = std::clamp(dst, 0.0f, 1.0f);
 const float s = std::clamp(src, 0.0f, 1.0f);
 switch (mode) {
 case ArtifactCore::BlendMode::Normal:
  return s;
 case ArtifactCore::BlendMode::Add:
  return std::clamp(d + s, 0.0f, 1.0f);
 case ArtifactCore::BlendMode::Subtract:
  return std::clamp(d - s, 0.0f, 1.0f);
 case ArtifactCore::BlendMode::Multiply:
  return d * s;
 case ArtifactCore::BlendMode::Screen:
  return 1.0f - (1.0f - d) * (1.0f - s);
 case ArtifactCore::BlendMode::Overlay:
  return d <= 0.5f ? (2.0f * d * s) : (1.0f - 2.0f * (1.0f - d) * (1.0f - s));
 case ArtifactCore::BlendMode::Darken:
  return std::min(d, s);
 case ArtifactCore::BlendMode::Lighten:
  return std::max(d, s);
 case ArtifactCore::BlendMode::ColorDodge:
  return s >= 1.0f ? 1.0f : std::clamp(d / (1.0f - s), 0.0f, 1.0f);
 case ArtifactCore::BlendMode::ColorBurn:
  return s <= 0.0f ? 0.0f : std::clamp(1.0f - ((1.0f - d) / s), 0.0f, 1.0f);
 case ArtifactCore::BlendMode::HardLight:
  return s <= 0.5f ? (2.0f * d * s) : (1.0f - 2.0f * (1.0f - d) * (1.0f - s));
 case ArtifactCore::BlendMode::SoftLight: {
  const float g = d <= 0.25f
   ? (((16.0f * d - 12.0f) * d + 4.0f) * d)
   : std::sqrt(d);
  const float result = s <= 0.5f
   ? (d - (1.0f - 2.0f * s) * d * (1.0f - d))
   : (d + (2.0f * s - 1.0f) * (g - d));
  return std::clamp(result, 0.0f, 1.0f);
 }
 case ArtifactCore::BlendMode::Difference:
  return std::abs(d - s);
  case ArtifactCore::BlendMode::Exclusion:
  return d + s - 2.0f * d * s;
  case ArtifactCore::BlendMode::LinearBurn:
   return std::clamp(d + s - 1.0f, 0.0f, 1.0f);
  case ArtifactCore::BlendMode::Divide:
   return std::clamp(d / std::max(s, 1e-6f), 0.0f, 1.0f);
  case ArtifactCore::BlendMode::PinLight:
   return s < 0.5f ? std::min(d, 2.0f * s) : std::max(d, 2.0f * (s - 0.5f));
  case ArtifactCore::BlendMode::VividLight:
   return s < 0.5f
              ? (s <= 0.0f ? 0.0f
                           : std::clamp(1.0f - ((1.0f - d) / (2.0f * s)), 0.0f, 1.0f))
              : (s >= 1.0f ? 1.0f
                           : std::clamp(d / (2.0f * (1.0f - s)), 0.0f, 1.0f));
  case ArtifactCore::BlendMode::LinearLight:
   return std::clamp(d + 2.0f * s - 1.0f, 0.0f, 1.0f);
  case ArtifactCore::BlendMode::HardMix:
   return d + s >= 1.0f ? 1.0f : 0.0f;
  case ArtifactCore::BlendMode::Hue:
  case ArtifactCore::BlendMode::Saturation:
  case ArtifactCore::BlendMode::Color:
  case ArtifactCore::BlendMode::Luminosity:
  case ArtifactCore::BlendMode::ClassicColorBurn:
   return s <= 0.0f ? 0.0f : std::clamp(1.0f - ((1.0f - d) / s), 0.0f, 1.0f);
  case ArtifactCore::BlendMode::LinearDodge:
  case ArtifactCore::BlendMode::ClassicColorDodge:
   return s >= 1.0f ? 1.0f : std::clamp(d / (1.0f - s), 0.0f, 1.0f);
  case ArtifactCore::BlendMode::ClassicDifference:
   return std::abs(d - s);
  case ArtifactCore::BlendMode::Dissolve:
  case ArtifactCore::BlendMode::DancingDissolve:
  case ArtifactCore::BlendMode::StencilAlpha:
  case ArtifactCore::BlendMode::StencilLuma:
  case ArtifactCore::BlendMode::SilhouetteAlpha:
  case ArtifactCore::BlendMode::SilhouetteLuma:
  default:
   return s;
  }
 }

bool shouldUseQPainterFallback(const ArtifactCore::BlendMode mode)
{
 switch (mode) {
  case ArtifactCore::BlendMode::Normal:
  case ArtifactCore::BlendMode::Add:
  case ArtifactCore::BlendMode::Subtract:
  case ArtifactCore::BlendMode::Multiply:
  case ArtifactCore::BlendMode::Screen:
  case ArtifactCore::BlendMode::Overlay:
  case ArtifactCore::BlendMode::Darken:
  case ArtifactCore::BlendMode::Lighten:
  case ArtifactCore::BlendMode::ColorDodge:
  case ArtifactCore::BlendMode::ColorBurn:
  case ArtifactCore::BlendMode::HardLight:
  case ArtifactCore::BlendMode::SoftLight:
  case ArtifactCore::BlendMode::Difference:
  case ArtifactCore::BlendMode::Exclusion:
  case ArtifactCore::BlendMode::LinearBurn:
  case ArtifactCore::BlendMode::Divide:
  case ArtifactCore::BlendMode::PinLight:
  case ArtifactCore::BlendMode::VividLight:
  case ArtifactCore::BlendMode::LinearLight:
  case ArtifactCore::BlendMode::HardMix:
  case ArtifactCore::BlendMode::ClassicColorBurn:
  case ArtifactCore::BlendMode::LinearDodge:
  case ArtifactCore::BlendMode::ClassicColorDodge:
  case ArtifactCore::BlendMode::ClassicDifference:
  case ArtifactCore::BlendMode::Hue:
  case ArtifactCore::BlendMode::Saturation:
  case ArtifactCore::BlendMode::Color:
  case ArtifactCore::BlendMode::Luminosity:
  case ArtifactCore::BlendMode::Dissolve:
  case ArtifactCore::BlendMode::DancingDissolve:
   return false;
  default:
   return true;
 }
}

float deterministicNoise(const int x, const int y, const int seed)
{
 const uint32_t ux = static_cast<uint32_t>(x);
 const uint32_t uy = static_cast<uint32_t>(y);
 uint32_t v = ux * 1664525u + uy * 1013904223u + static_cast<uint32_t>(seed);
 v = v * 747796405u + 2891336453u;
 return static_cast<float>(v) / 4294967296.0f;
}

void blendBgrWithQPainter(cv::Mat& dstBgr, const cv::Mat& srcBgr, const float opacity, const ArtifactCore::BlendMode mode)
{
 cv::Mat dstBgra;
 cv::Mat srcBgra;
 cv::cvtColor(dstBgr, dstBgra, cv::COLOR_BGR2BGRA);
 cv::cvtColor(srcBgr, srcBgra, cv::COLOR_BGR2BGRA);

 QImage dstImage(dstBgra.data, dstBgra.cols, dstBgra.rows, static_cast<int>(dstBgra.step), QImage::Format_ARGB32);
 QImage srcImage(srcBgra.data, srcBgra.cols, srcBgra.rows, static_cast<int>(srcBgra.step), QImage::Format_ARGB32);
 QImage dstCopy = dstImage.copy();
 QImage srcCopy = srcImage.copy();

 QPainter painter(&dstCopy);
 painter.setOpacity(std::clamp(opacity, 0.0f, 1.0f));
  painter.setCompositionMode(qPainterCompositionMode(mode));
 painter.drawImage(0, 0, srcCopy);
 painter.end();

 QImage dstRgba = (dstCopy.format() == QImage::Format_RGBA8888)
                      ? dstCopy
                      : dstCopy.convertToFormat(QImage::Format_RGBA8888);
 cv::Mat rgbaView(dstRgba.height(), dstRgba.width(), CV_8UC4, const_cast<uchar*>(dstRgba.bits()), dstRgba.bytesPerLine());
 cv::cvtColor(rgbaView, dstBgr, cv::COLOR_RGBA2BGR);
}

void blendBgrInPlace(cv::Mat& dstBgr, const cv::Mat& srcBgr, const float opacity, const ArtifactCore::BlendMode mode)
{
 const float a = std::clamp(opacity, 0.0f, 1.0f);
 if (a <= 0.0f) {
  return;
 }

 if (shouldUseQPainterFallback(mode)) {
  blendBgrWithQPainter(dstBgr, srcBgr, a, mode);
  return;
 }

 cv::Mat dstF;
 cv::Mat srcF;
 dstBgr.convertTo(dstF, CV_32FC3, 1.0 / 255.0);
 srcBgr.convertTo(srcF, CV_32FC3, 1.0 / 255.0);

 cv::Mat blended = dstF.clone();
 ArtifactCore::Parallel::For(0, dstF.rows, dstF.rows * dstF.cols, [&](int y) {
   const cv::Vec3f* dstRow = dstF.ptr<cv::Vec3f>(y);
   const cv::Vec3f* srcRow = srcF.ptr<cv::Vec3f>(y);
   cv::Vec3f* outRow = blended.ptr<cv::Vec3f>(y);
   for (int x = 0; x < dstF.cols; ++x) {
    if (mode == ArtifactCore::BlendMode::Dissolve ||
        mode == ArtifactCore::BlendMode::DancingDissolve) {
     const int seed = mode == ArtifactCore::BlendMode::DancingDissolve
                          ? static_cast<int>(std::round(a * 1000.0f))
                          : 0;
     outRow[x] = deterministicNoise(x, y, seed) < a ? srcRow[x] : dstRow[x];
     continue;
    }
    if (mode == ArtifactCore::BlendMode::Hue ||
        mode == ArtifactCore::BlendMode::Saturation ||
        mode == ArtifactCore::BlendMode::Color ||
        mode == ArtifactCore::BlendMode::Luminosity) {
     const ArtifactCore::HSLColor dstHsl = ArtifactCore::ColorConversion::RGBToHSL(
         dstRow[x][2], dstRow[x][1], dstRow[x][0]);
     const ArtifactCore::HSLColor srcHsl = ArtifactCore::ColorConversion::RGBToHSL(
         srcRow[x][2], srcRow[x][1], srcRow[x][0]);
     ArtifactCore::HSLColor resultHsl = dstHsl;
     if (mode == ArtifactCore::BlendMode::Hue ||
         mode == ArtifactCore::BlendMode::Color) {
      resultHsl.h = srcHsl.h;
     }
     if (mode == ArtifactCore::BlendMode::Saturation ||
         mode == ArtifactCore::BlendMode::Color) {
      resultHsl.s = srcHsl.s;
     }
     if (mode == ArtifactCore::BlendMode::Luminosity) {
      resultHsl.l = srcHsl.l;
     }
     const auto rgb = ArtifactCore::ColorConversion::HSLToRGB(resultHsl);
     outRow[x] = cv::Vec3f(rgb[2], rgb[1], rgb[0]);
     continue;
    }
    cv::Vec3f pixel;
    for (int c = 0; c < 3; ++c) {
     pixel[c] = blendChannel(dstRow[x][c], srcRow[x][c], mode);
    }
    outRow[x] = pixel;
   }
 });

 cv::Mat mixed = dstF * (1.0f - a) + blended * a;
 mixed.convertTo(dstBgr, CV_8UC3, 255.0);
}

void applyAlphaBlendMode(cv::Mat& dstAlpha,
                         const cv::Mat& srcRgba,
                         const float opacity,
                         const ArtifactCore::BlendMode mode)
{
 if (dstAlpha.empty() || srcRgba.empty() || opacity <= 0.0f) return;
 const float clampedOpacity = std::clamp(opacity, 0.0f, 1.0f);
 for (int y = 0; y < dstAlpha.rows; ++y) {
  auto* dstRow = dstAlpha.ptr<std::uint8_t>(y);
  const auto* srcRow = srcRgba.ptr<cv::Vec4b>(y);
  for (int x = 0; x < dstAlpha.cols; ++x) {
   const float srcR = static_cast<float>(srcRow[x][0]) / 255.0f;
   const float srcG = static_cast<float>(srcRow[x][1]) / 255.0f;
   const float srcB = static_cast<float>(srcRow[x][2]) / 255.0f;
   const float srcA = static_cast<float>(srcRow[x][3]) / 255.0f;
   float factor = clampedOpacity * srcA;
   if (mode == ArtifactCore::BlendMode::StencilLuma ||
       mode == ArtifactCore::BlendMode::SilhouetteLuma) {
    factor = clampedOpacity * std::clamp(
        ArtifactCore::ColorLuminance::calculate(srcR, srcG, srcB), 0.0f, 1.0f);
   }
   const float dstA = static_cast<float>(dstRow[x]) / 255.0f;
   const bool silhouette = mode == ArtifactCore::BlendMode::SilhouetteAlpha ||
                           mode == ArtifactCore::BlendMode::SilhouetteLuma;
   const float result = silhouette ? dstA * (1.0f - factor) : dstA * factor;
   dstRow[x] = static_cast<std::uint8_t>(std::round(
       std::clamp(result, 0.0f, 1.0f) * 255.0f));
  }
 }
}

QImage composeOpenCV(const CompositeRequest& request)
{
 const int w = std::max(1, request.outputSize.width());
 const int h = std::max(1, request.outputSize.height());
 cv::Mat canvasBgr(h, w, CV_8UC3, cv::Scalar(28, 24, 22));
 cv::Mat canvasAlpha(h, w, CV_8UC1, cv::Scalar(255));

 if (!request.background.isNull()) {
  const QImage bgScaled = request.background.scaled(
   QSize(w, h), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  cv::Mat bgRgba = qImageToMatRGBA(bgScaled);
  cv::Mat bgBgr;
  cv::cvtColor(bgRgba, bgBgr, cv::COLOR_RGBA2BGR);
  if (bgBgr.size() == canvasBgr.size()) {
   bgBgr.copyTo(canvasBgr);
   cv::Mat bgAlpha;
   cv::extractChannel(bgRgba, bgAlpha, 3);
   bgAlpha.copyTo(canvasAlpha);
  } else {
   cv::resize(bgBgr, canvasBgr, canvasBgr.size(), 0.0, 0.0, cv::INTER_LINEAR);
   cv::Mat bgAlpha;
   cv::extractChannel(bgRgba, bgAlpha, 3);
   cv::resize(bgAlpha, canvasAlpha, canvasAlpha.size(), 0.0, 0.0, cv::INTER_LINEAR);
  }
 }

 if (request.useForeground && !request.foreground.isNull()) {
  cv::Mat fgRgba = qImageToMatRGBA(request.foreground);
  cv::Mat fgBgr;
  cv::cvtColor(fgRgba, fgBgr, cv::COLOR_RGBA2BGR);
  if (fgBgr.size() != canvasBgr.size()) {
   cv::resize(fgBgr, fgBgr, canvasBgr.size(), 0.0, 0.0, cv::INTER_LINEAR);
  }
  blendBgrInPlace(canvasBgr, fgBgr, 1.0f, ArtifactCore::BlendMode::Normal);
 }

 if (!request.overlay.isNull()) {
  CompositeRequest transformedReq = request;
  transformedReq.overlay = request.overlay.scaled(
   QSize(w, h), Qt::KeepAspectRatio, Qt::SmoothTransformation);
  const QImage overlayCanvas = buildOverlayCanvas(transformedReq, QSize(w, h));
  cv::Mat ovRgba = qImageToMatRGBA(overlayCanvas);
  cv::Mat ovBgr;
  cv::cvtColor(ovRgba, ovBgr, cv::COLOR_RGBA2BGR);
  if (request.blendMode == ArtifactCore::BlendMode::StencilAlpha ||
      request.blendMode == ArtifactCore::BlendMode::StencilLuma ||
      request.blendMode == ArtifactCore::BlendMode::SilhouetteAlpha ||
      request.blendMode == ArtifactCore::BlendMode::SilhouetteLuma) {
   applyAlphaBlendMode(canvasAlpha, ovRgba, request.overlayOpacity, request.blendMode);
  } else {
   blendBgrInPlace(canvasBgr, ovBgr, request.overlayOpacity, request.blendMode);
  }
 }

 if (request.cvEffect == CvEffectMode::GaussianBlur) {
  cv::GaussianBlur(canvasBgr, canvasBgr, cv::Size(9, 9), 0.0);
 } else if (request.cvEffect == CvEffectMode::EdgeOverlay) {
  cv::Mat gray;
  cv::cvtColor(canvasBgr, gray, cv::COLOR_BGR2GRAY);
  cv::Mat edges;
  cv::Canny(gray, edges, 80.0, 160.0);
  cv::Mat edgesBgr;
  cv::cvtColor(edges, edgesBgr, cv::COLOR_GRAY2BGR);
  cv::Mat mixed;
  cv::addWeighted(canvasBgr, 0.85, edgesBgr, 0.65, 0.0, mixed);
  canvasBgr = mixed;
 }

 cv::Mat outRgba;
 cv::cvtColor(canvasBgr, outRgba, cv::COLOR_BGR2RGBA);
 cv::Mat channels[4];
 cv::split(outRgba, channels);
 canvasAlpha.copyTo(channels[3]);
 cv::merge(channels, 4, outRgba);
 return matRGBAToQImage(outRgba);
}

} // namespace

QPainter::CompositionMode qPainterCompositionMode(const ArtifactCore::BlendMode mode)
{
 return qPainterCompositionModeImpl(mode);
}

QImage compose(const CompositeRequest& request)
{
 return composeOpenCV(request);
}

bool qPainterSupportsBlendMode(const ArtifactCore::BlendMode mode)
{
 switch (mode) {
 case ArtifactCore::BlendMode::Normal:
 case ArtifactCore::BlendMode::Add:
 case ArtifactCore::BlendMode::Multiply:
 case ArtifactCore::BlendMode::Screen:
 case ArtifactCore::BlendMode::Overlay:
 case ArtifactCore::BlendMode::Darken:
 case ArtifactCore::BlendMode::Lighten:
 case ArtifactCore::BlendMode::ColorDodge:
 case ArtifactCore::BlendMode::ColorBurn:
 case ArtifactCore::BlendMode::HardLight:
 case ArtifactCore::BlendMode::SoftLight:
 case ArtifactCore::BlendMode::Difference:
 case ArtifactCore::BlendMode::Exclusion:
  return true;
 default:
  return false;
 }
}

bool blendSurface(QImage& canvas,
                  const QImage& surface,
                  const float opacity,
                  const ArtifactCore::BlendMode mode)
{
 if (qPainterSupportsBlendMode(mode)) {
  return false;
 }
 if (canvas.isNull() || surface.isNull() ||
     canvas.format() != QImage::Format_RGBA8888 ||
     surface.format() != QImage::Format_RGBA8888 ||
     canvas.size() != surface.size()) {
  return false;
 }

 const float srcAlpha = clamp01(opacity);
 if (srcAlpha <= 0.0f) {
  return false;
 }

 const int w = canvas.width();
 const int h = canvas.height();
 const bool dissolve =
     mode == ArtifactCore::BlendMode::Dissolve ||
     mode == ArtifactCore::BlendMode::DancingDissolve;
 const int dissolveSeed = mode == ArtifactCore::BlendMode::DancingDissolve
                              ? static_cast<int>(std::round(srcAlpha * 1000.0f))
                              : 0;

 ArtifactCore::Parallel::For(0, h, w * h, [&](int y) {
  auto* dstRow = canvas.bits() + static_cast<qsizetype>(y) * canvas.bytesPerLine();
  const uchar* srcRow = surface.constBits() + static_cast<qsizetype>(y) * surface.bytesPerLine();
  for (int x = 0; x < w; ++x) {
   const int i = x * 4;
   const float dr = dstRow[i + 0] / 255.0f;
   const float dg = dstRow[i + 1] / 255.0f;
   const float db = dstRow[i + 2] / 255.0f;
   const float da = dstRow[i + 3] / 255.0f;

   if (dissolve) {
    if (deterministicNoise(x, y, dissolveSeed) < srcAlpha) {
     dstRow[i + 0] = srcRow[i + 0];
     dstRow[i + 1] = srcRow[i + 1];
     dstRow[i + 2] = srcRow[i + 2];
     dstRow[i + 3] = srcRow[i + 3];
    }
    continue;
   }

   const float pixA = srcRow[i + 3] / 255.0f;
   const float effA = pixA * srcAlpha;
   if (effA <= 0.0f) {
    continue;
   }

   const ArtifactCore::FloatColor base(dr, dg, db, da);
   const ArtifactCore::FloatColor blend(
       srcRow[i + 0] / 255.0f,
       srcRow[i + 1] / 255.0f,
       srcRow[i + 2] / 255.0f,
       pixA);
   const ArtifactCore::FloatColor result = blendColor(base, blend, mode, effA);

   dstRow[i + 0] = static_cast<uchar>(std::round(clamp01(result.r()) * 255.0f));
   dstRow[i + 1] = static_cast<uchar>(std::round(clamp01(result.g()) * 255.0f));
   dstRow[i + 2] = static_cast<uchar>(std::round(clamp01(result.b()) * 255.0f));
   dstRow[i + 3] = static_cast<uchar>(std::round(clamp01(result.a()) * 255.0f));
  }
 });
 return true;
}

bool composeToBuffer(const CompositeRequest& request,
                     ArtifactCore::ImageF32x4_RGBA& output)
{
 const QImage composed = compose(request);
 if (composed.isNull()) {
  return false;
 }

 const QImage rgba = composed.convertToFormat(QImage::Format_RGBA8888);
 cv::Mat rgbaMat = qImageToMatRGBA(rgba);
 if (rgbaMat.empty()) {
  return false;
 }

 cv::Mat floatRgba;
 rgbaMat.convertTo(floatRgba, CV_32FC4, 1.0 / 255.0);
 output.setFromCVMat(floatRgba);
 return !output.isEmpty();
}

QString backendText(const CompositeBackend backend)
{
 Q_UNUSED(backend);
 return QStringLiteral("OpenCV");
}

QString blendModeText(const ArtifactCore::BlendMode mode)
{
 return ArtifactCore::BlendModeUtils::toString(mode);
}

QString cvEffectText(const CvEffectMode mode)
{
 switch (mode) {
 case CvEffectMode::None: return QStringLiteral("None");
 case CvEffectMode::GaussianBlur: return QStringLiteral("GaussianBlur");
 case CvEffectMode::EdgeOverlay: return QStringLiteral("EdgeOverlay");
 default: return QStringLiteral("None");
 }
}

} // namespace Artifact::SoftwareRender

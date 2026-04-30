module;
#include <utility>
#include <QImage>
#include <QPainter>
#include <QColor>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>

module Artifact.Render.SoftwareCompositor;

import Layer.Blend;

namespace Artifact::SoftwareRender {

namespace {

QPainter::CompositionMode toCompositionMode(const ArtifactCore::BlendMode mode)
{
 switch (mode) {
 case ArtifactCore::BlendMode::Subtract: return QPainter::CompositionMode_Difference;
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
  return QPainter::CompositionMode_SourceOver;
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
 case ArtifactCore::BlendMode::Hue:
 case ArtifactCore::BlendMode::Saturation:
 case ArtifactCore::BlendMode::Color:
 case ArtifactCore::BlendMode::Luminosity:
 default:
  return s;
 }
}

bool shouldUseQPainterFallback(const ArtifactCore::BlendMode mode)
{
 switch (mode) {
 case ArtifactCore::BlendMode::Hue:
 case ArtifactCore::BlendMode::Saturation:
 case ArtifactCore::BlendMode::Color:
 case ArtifactCore::BlendMode::Luminosity:
  return true;
 default:
  return false;
 }
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
 painter.setCompositionMode(toCompositionMode(mode));
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
 for (int y = 0; y < dstF.rows; ++y) {
  const cv::Vec3f* dstRow = dstF.ptr<cv::Vec3f>(y);
  const cv::Vec3f* srcRow = srcF.ptr<cv::Vec3f>(y);
  cv::Vec3f* outRow = blended.ptr<cv::Vec3f>(y);
  for (int x = 0; x < dstF.cols; ++x) {
   cv::Vec3f pixel;
   for (int c = 0; c < 3; ++c) {
    pixel[c] = blendChannel(dstRow[x][c], srcRow[x][c], mode);
   }
   outRow[x] = pixel;
  }
 }

 cv::Mat mixed = dstF * (1.0f - a) + blended * a;
 mixed.convertTo(dstBgr, CV_8UC3, 255.0);
}

QImage composeOpenCV(const CompositeRequest& request)
{
 const int w = std::max(1, request.outputSize.width());
 const int h = std::max(1, request.outputSize.height());
 cv::Mat canvasBgr(h, w, CV_8UC3, cv::Scalar(28, 24, 22));

 if (!request.background.isNull()) {
  const QImage bgScaled = request.background.scaled(
   QSize(w, h), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  cv::Mat bgRgba = qImageToMatRGBA(bgScaled);
  cv::Mat bgBgr;
  cv::cvtColor(bgRgba, bgBgr, cv::COLOR_RGBA2BGR);
  if (bgBgr.size() == canvasBgr.size()) {
   bgBgr.copyTo(canvasBgr);
  } else {
   cv::resize(bgBgr, canvasBgr, canvasBgr.size(), 0.0, 0.0, cv::INTER_LINEAR);
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
  blendBgrInPlace(canvasBgr, ovBgr, request.overlayOpacity, request.blendMode);
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
 return matRGBAToQImage(outRgba);
}

} // namespace

QImage compose(const CompositeRequest& request)
{
 Q_UNUSED(request);
 return composeOpenCV(request);
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

module;
#include <QImage>
#include <QPainter>
#include <QColor>
#include <QTransform>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

module Artifact.Render.SoftwareCompositor;

namespace Artifact::SoftwareRender {

namespace {

QPainter::CompositionMode toCompositionMode(const BlendMode mode)
{
 switch (mode) {
 case BlendMode::Add: return QPainter::CompositionMode_Plus;
 case BlendMode::Multiply: return QPainter::CompositionMode_Multiply;
 case BlendMode::Screen: return QPainter::CompositionMode_Screen;
 case BlendMode::Normal:
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
 QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
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

void blendBgrInPlace(cv::Mat& dstBgr, const cv::Mat& srcBgr, const float opacity, const BlendMode mode)
{
 const float a = std::clamp(opacity, 0.0f, 1.0f);
 if (a <= 0.0f) {
  return;
 }

 cv::Mat dstF;
 cv::Mat srcF;
 dstBgr.convertTo(dstF, CV_32FC3, 1.0 / 255.0);
 srcBgr.convertTo(srcF, CV_32FC3, 1.0 / 255.0);

 cv::Mat blended = dstF.clone();
 switch (mode) {
 case BlendMode::Normal:
  blended = srcF;
  break;
 case BlendMode::Add:
  cv::add(dstF, srcF, blended);
  cv::min(blended, 1.0f, blended);
  break;
 case BlendMode::Multiply:
  cv::multiply(dstF, srcF, blended);
  break;
 case BlendMode::Screen:
  blended = 1.0f - (1.0f - dstF).mul(1.0f - srcF);
  break;
 }

 cv::Mat mixed = dstF * (1.0f - a) + blended * a;
 mixed.convertTo(dstBgr, CV_8UC3, 255.0);
}

QImage composeQtPainter(const CompositeRequest& request)
{
 const int w = std::max(1, request.outputSize.width());
 const int h = std::max(1, request.outputSize.height());
 QImage output(w, h, QImage::Format_ARGB32_Premultiplied);
 output.fill(QColor(22, 24, 28, 255));

 QPainter painter(&output);
 painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

 if (!request.background.isNull()) {
  const QImage bgScaled = request.background.scaled(
   output.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
  const int x = (output.width() - bgScaled.width()) / 2;
  const int y = (output.height() - bgScaled.height()) / 2;
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  painter.setOpacity(1.0);
  painter.drawImage(x, y, bgScaled);
 }

 if (request.useForeground && !request.foreground.isNull()) {
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  painter.setOpacity(1.0);
  painter.drawImage(0, 0, request.foreground);
 }

 if (!request.overlay.isNull()) {
  QImage ovScaled = request.overlay.scaled(
   output.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
  CompositeRequest transformedReq = request;
  transformedReq.overlay = ovScaled;
  const QImage overlayCanvas = buildOverlayCanvas(transformedReq, output.size());
  painter.setOpacity(std::clamp(request.overlayOpacity, 0.0f, 1.0f));
  painter.setCompositionMode(toCompositionMode(request.blendMode));
  painter.drawImage(0, 0, overlayCanvas);
  painter.setOpacity(1.0);
 }

 return output;
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
  blendBgrInPlace(canvasBgr, fgBgr, 1.0f, BlendMode::Normal);
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
 if (request.backend == CompositeBackend::OpenCV) {
  return composeOpenCV(request);
 }
 return composeQtPainter(request);
}

QString backendText(const CompositeBackend backend)
{
 switch (backend) {
 case CompositeBackend::QtPainter: return QStringLiteral("QImage/QPainter");
 case CompositeBackend::OpenCV: return QStringLiteral("OpenCV");
 default: return QStringLiteral("QImage/QPainter");
 }
}

QString blendModeText(const BlendMode mode)
{
 switch (mode) {
 case BlendMode::Normal: return QStringLiteral("Normal");
 case BlendMode::Add: return QStringLiteral("Add");
 case BlendMode::Multiply: return QStringLiteral("Multiply");
 case BlendMode::Screen: return QStringLiteral("Screen");
 default: return QStringLiteral("Normal");
 }
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

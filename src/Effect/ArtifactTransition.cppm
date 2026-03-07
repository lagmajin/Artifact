module;
#include <QObject>
#include <QVector2D>
#include <QVector3D>
#include <QColor>
#include <QImage>
#include <QEasingCurve>
#include <QPainter>
#include <QtMath>
#include <QRandomGenerator>
#include <cmath>
#include <algorithm>
#include <wobjectimpl.h>

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
module Artifact.Effect.Transition;





namespace Artifact {

// ==================== AbstractTransition ====================

AbstractTransition::AbstractTransition(TransitionType type, QObject* parent)
    : QObject(parent)
    , type_(type)
{
}

AbstractTransition::~AbstractTransition()
{
}

float AbstractTransition::applyEasing(float progress) const
{
    QEasingCurve curve(params_.easing);
    float eased = curve.valueForProgress(progress);
    
    if (params_.reverse) {
        eased = 1.0f - eased;
    }
    
    return std::clamp(eased, 0.0f, 1.0f);
}

void AbstractTransition::processGPU(const float* fromPixels,
                                   const float* toPixels,
                                   float* outputPixels,
                                   int width, int height,
                                   float progress)
{
    Q_UNUSED(fromPixels);
    Q_UNUSED(toPixels);
    Q_UNUSED(outputPixels);
    Q_UNUSED(width);
    Q_UNUSED(height);
    Q_UNUSED(progress);
    // Base implementation does nothing - subclasses should override
}

// ==================== CrossDissolveTransition ====================

CrossDissolveTransition::CrossDissolveTransition(QObject* parent)
    : AbstractTransition(TransitionType::CrossDissolve, parent)
{
}

CrossDissolveTransition::~CrossDissolveTransition()
{
}

void CrossDissolveTransition::process(const QImage& fromFrame,
                                     const QImage& toFrame,
                                     QImage& output,
                                     float progress)
{
    float t = applyEasing(progress);
    
    if (output.size() != fromFrame.size()) {
        output = QImage(fromFrame.size(), QImage::Format_RGB32);
    }
    
    int w = output.width();
    int h = output.height();
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            QRgb fromPixel = fromFrame.pixel(x, y);
            QRgb toPixel = toFrame.pixel(x, y);
            
            int r = static_cast<int>(qRed(fromPixel) * (1 - t) + qRed(toPixel) * t);
            int g = static_cast<int>(qGreen(fromPixel) * (1 - t) + qGreen(toPixel) * t);
            int b = static_cast<int>(qBlue(fromPixel) * (1 - t) + qBlue(toPixel) * t);
            int a = static_cast<int>(qAlpha(fromPixel) * (1 - t) + qAlpha(toPixel) * t);
            
            output.setPixel(x, y, qRgba(r, g, b, a));
        }
    }
}

// ==================== WipeTransition ====================

WipeTransition::WipeTransition(TransitionType type, QObject* parent)
    : AbstractTransition(type, parent)
{
}

WipeTransition::~WipeTransition()
{
}

float WipeTransition::calculateWipeMask(float x, float y, float progress) const
{
    float maskValue = 0.0f;
    float softness = wipeParams_.softness;
    
    switch (type_) {
        case TransitionType::WipeLeft: {
            maskValue = x - progress;
            break;
        }
        case TransitionType::WipeRight: {
            maskValue = (1.0f - x) - progress;
            break;
        }
        case TransitionType::WipeUp: {
            maskValue = y - progress;
            break;
        }
        case TransitionType::WipeDown: {
            maskValue = (1.0f - y) - progress;
            break;
        }
        case TransitionType::WipeRadial: {
            float cx = x - wipeParams_.origin.x();
            float cy = y - wipeParams_.origin.y();
            float dist = std::sqrt(cx * cx + cy * cy);
            maskValue = dist - progress * 1.414f; // sqrt(2) for diagonal
            break;
        }
        case TransitionType::WipeClock: {
            float cx = x - wipeParams_.origin.x();
            float cy = y - wipeParams_.origin.y();
            float angle = std::atan2(cy, cx);
            maskValue = (angle + M_PI) / (2.0f * M_PI) - progress;
            break;
        }
        case TransitionType::WipeDiamond: {
            float cx = std::abs(x - wipeParams_.origin.x());
            float cy = std::abs(y - wipeParams_.origin.y());
            float diamondDist = cx + cy;
            maskValue = diamondDist - progress * 2.0f;
            break;
        }
        default: {
            // Angle-based wipe
            float rad = qDegreesToRadians(wipeParams_.angle);
            float nx = x * std::cos(rad) + y * std::sin(rad);
            maskValue = nx - progress;
            break;
        }
    }
    
    if (wipeParams_.invert) {
        maskValue = -maskValue;
    }
    
    // Apply softness
    if (softness > 0.0f) {
        return std::clamp(maskValue / softness + 0.5f, 0.0f, 1.0f);
    }
    
    return maskValue > 0 ? 1.0f : 0.0f;
}

void WipeTransition::process(const QImage& fromFrame,
                            const QImage& toFrame,
                            QImage& output,
                            float progress)
{
    float t = applyEasing(progress);
    
    if (output.size() != fromFrame.size()) {
        output = QImage(fromFrame.size(), QImage::Format_RGB32);
    }
    
    int w = output.width();
    int h = output.height();
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float nx = static_cast<float>(x) / w;
            float ny = static_cast<float>(y) / h;
            
            float mask = calculateWipeMask(nx, ny, t);
            
            QRgb fromPixel = fromFrame.pixel(x, y);
            QRgb toPixel = toFrame.pixel(x, y);
            
            int r = static_cast<int>(qRed(fromPixel) * (1 - mask) + qRed(toPixel) * mask);
            int g = static_cast<int>(qGreen(fromPixel) * (1 - mask) + qGreen(toPixel) * mask);
            int b = static_cast<int>(qBlue(fromPixel) * (1 - mask) + qBlue(toPixel) * mask);
            int a = static_cast<int>(qAlpha(fromPixel) * (1 - mask) + qAlpha(toPixel) * mask);
            
            output.setPixel(x, y, qRgba(r, g, b, a));
        }
    }
}

// ==================== SlideTransition ====================

SlideTransition::SlideTransition(TransitionType type, QObject* parent)
    : AbstractTransition(type, parent)
{
}

SlideTransition::~SlideTransition()
{
}

void SlideTransition::process(const QImage& fromFrame,
                             const QImage& toFrame,
                             QImage& output,
                             float progress)
{
    float t = applyEasing(progress);
    
    if (output.size() != fromFrame.size()) {
        output = QImage(fromFrame.size(), QImage::Format_RGB32);
    }
    
    int w = output.width();
    int h = output.height();
    float distance = slideParams_.distance;
    
    output.fill(Qt::black);
    
    float offsetX = 0, offsetY = 0;
    float fromOffsetX = 0, fromOffsetY = 0;
    
    switch (type_) {
        case TransitionType::SlideLeft: {
            offsetX = t * w * distance;
            fromOffsetX = t * w * distance - w * distance;
            break;
        }
        case TransitionType::SlideRight: {
            offsetX = -t * w * distance;
            fromOffsetX = -t * w * distance + w * distance;
            break;
        }
        case TransitionType::SlideUp: {
            offsetY = t * h * distance;
            fromOffsetY = t * h * distance - h * distance;
            break;
        }
        case TransitionType::SlideDown: {
            offsetY = -t * h * distance;
            fromOffsetY = -t * h * distance + h * distance;
            break;
        }
        case TransitionType::PushLeft: {
            offsetX = t * w;
            fromOffsetX = t * w - w;
            break;
        }
        case TransitionType::PushRight: {
            offsetX = -t * w;
            fromOffsetX = w - t * w;
            break;
        }
        case TransitionType::PushUp: {
            offsetY = t * h;
            fromOffsetY = t * h - h;
            break;
        }
        case TransitionType::PushDown: {
            offsetY = -t * h;
            fromOffsetY = h - t * h;
            break;
        }
        default:
            break;
    }
    
    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // Draw from frame
    if (slideParams_.push) {
        painter.drawImage(QPointF(fromOffsetX, fromOffsetY), fromFrame);
    } else {
        painter.setOpacity(1.0 - t);
        painter.drawImage(QPointF(0, 0), fromFrame);
    }
    
    // Draw to frame
    painter.setOpacity(1.0);
    painter.drawImage(QPointF(offsetX, offsetY), toFrame);
}

// ==================== ZoomTransition ====================

ZoomTransition::ZoomTransition(TransitionType type, QObject* parent)
    : AbstractTransition(type, parent)
{
}

ZoomTransition::~ZoomTransition()
{
}

void ZoomTransition::process(const QImage& fromFrame,
                            const QImage& toFrame,
                            QImage& output,
                            float progress)
{
    float t = applyEasing(progress);
    
    if (output.size() != fromFrame.size()) {
        output = QImage(fromFrame.size(), QImage::Format_RGB32);
    }
    
    output.fill(Qt::black);
    
    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    int w = output.width();
    int h = output.height();
    
    float centerX = zoomParams_.center.x() * w;
    float centerY = zoomParams_.center.y() * h;
    
    switch (type_) {
        case TransitionType::ZoomIn: {
            // From frame zooms out
            float fromScale = 1.0f + t;
            painter.save();
            painter.translate(centerX, centerY);
            painter.scale(fromScale, fromScale);
            painter.translate(-centerX, -centerY);
            painter.setOpacity(1.0 - t);
            painter.drawImage(0, 0, fromFrame);
            painter.restore();
            
            // To frame fades in
            painter.setOpacity(t);
            painter.drawImage(0, 0, toFrame);
            break;
        }
        case TransitionType::ZoomOut: {
            // From frame fades out
            painter.setOpacity(1.0 - t);
            painter.drawImage(0, 0, fromFrame);
            
            // To frame zooms in
            float toScale = 1.0f + (1.0f - t);
            painter.save();
            painter.translate(centerX, centerY);
            painter.scale(toScale, toScale);
            painter.translate(-centerX, -centerY);
            painter.setOpacity(t);
            painter.drawImage(0, 0, toFrame);
            painter.restore();
            break;
        }
        case TransitionType::ZoomRotate: {
            // From frame zooms and rotates out
            float fromScale = 1.0f + t * 0.5f;
            float fromRotation = t * 15.0f;
            painter.save();
            painter.translate(centerX, centerY);
            painter.rotate(fromRotation);
            painter.scale(fromScale, fromScale);
            painter.translate(-centerX, -centerY);
            painter.setOpacity(1.0 - t);
            painter.drawImage(0, 0, fromFrame);
            painter.restore();
            
            // To frame zooms and rotates in
            float toScale = 1.5f - t * 0.5f;
            float toRotation = -15.0f + t * 15.0f;
            painter.save();
            painter.translate(centerX, centerY);
            painter.rotate(toRotation);
            painter.scale(toScale, toScale);
            painter.translate(-centerX, -centerY);
            painter.setOpacity(t);
            painter.drawImage(0, 0, toFrame);
            painter.restore();
            break;
        }
        default: {
            // Custom zoom using params
            float fromScale = zoomParams_.reverseZoom ? 
                zoomParams_.endScale - (zoomParams_.endScale - zoomParams_.startScale) * t :
                zoomParams_.startScale + (zoomParams_.endScale - zoomParams_.startScale) * t;
            
            painter.save();
            painter.translate(centerX, centerY);
            painter.rotate(zoomParams_.rotation * t);
            painter.scale(fromScale, fromScale);
            painter.translate(-centerX, -centerY);
            painter.setOpacity(1.0 - t);
            painter.drawImage(0, 0, fromFrame);
            painter.restore();
            
            painter.setOpacity(t);
            painter.drawImage(0, 0, toFrame);
            break;
        }
    }
}

// ==================== GlitchTransition ====================

GlitchTransition::GlitchTransition(QObject* parent)
    : AbstractTransition(TransitionType::Glitch, parent)
{
}

GlitchTransition::~GlitchTransition()
{
}

void GlitchTransition::process(const QImage& fromFrame,
                              const QImage& toFrame,
                              QImage& output,
                              float progress)
{
    float t = applyEasing(progress);
    
    if (output.size() != fromFrame.size()) {
        output = QImage(fromFrame.size(), QImage::Format_RGB32);
    }
    
    int w = output.width();
    int h = output.height();
    
    QRandomGenerator rng(glitchParams_.seed);
    
    // Glitch intensity peaks at middle of transition
    float intensity = glitchParams_.intensity * (1.0f - std::abs(2.0f * t - 1.0f));
    
    output = fromFrame.copy();
    
    QPainter painter(&output);
    
    // Apply horizontal glitch blocks
    if (glitchParams_.horizontalGlitch) {
        int numBlocks = static_cast<int>(intensity * 20);
        for (int i = 0; i < numBlocks; i++) {
            int y = rng.bounded(h);
            int blockHeight = rng.bounded(5, 30);
            int shift = static_cast<int>(rng.bounded(-50, 50) * intensity);
            
            QRect sourceRect(0, y, w, blockHeight);
            QRect destRect(shift, y, w, blockHeight);
            
            // Mix from and to frames
            float blockMix = rng.bounded(1.0);
            if (blockMix < t) {
                painter.drawImage(destRect, toFrame, sourceRect);
            } else {
                painter.drawImage(destRect, fromFrame, sourceRect);
            }
        }
    }
    
    // Apply vertical glitch blocks
    if (glitchParams_.verticalGlitch) {
        int numBlocks = static_cast<int>(intensity * 10);
        for (int i = 0; i < numBlocks; i++) {
            int x = rng.bounded(w);
            int blockWidth = rng.bounded(5, 20);
            int shift = static_cast<int>(rng.bounded(-30, 30) * intensity);
            
            QRect sourceRect(x, 0, blockWidth, h);
            QRect destRect(x, shift, blockWidth, h);
            
            float blockMix = rng.bounded(1.0);
            if (blockMix < t) {
                painter.drawImage(destRect, toFrame, sourceRect);
            } else {
                painter.drawImage(destRect, fromFrame, sourceRect);
            }
        }
    }
    
    // Color separation
    if (glitchParams_.colorSeparation > 0) {
        float separation = glitchParams_.colorSeparation * intensity;
        
        QImage result(output.size(), output.format());
        
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int rx = std::clamp(x + static_cast<int>(separation), 0, w - 1);
                int bx = std::clamp(x - static_cast<int>(separation), 0, w - 1);
                
                QRgb rPixel = output.pixel(rx, y);
                QRgb gPixel = output.pixel(x, y);
                QRgb bPixel = output.pixel(bx, y);
                
                result.setPixel(x, y, qRgb(qRed(rPixel), qGreen(gPixel), qBlue(bPixel)));
            }
        }
        
        output = result;
    }
    
    // Noise
    if (glitchParams_.noiseAmount > 0) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                if (rng.bounded(1.0) < glitchParams_.noiseAmount * intensity) {
                    int noise = rng.bounded(-50, 50);
                    QRgb pixel = output.pixel(x, y);
                    int r = std::clamp(qRed(pixel) + noise, 0, 255);
                    int g = std::clamp(qGreen(pixel) + noise, 0, 255);
                    int b = std::clamp(qBlue(pixel) + noise, 0, 255);
                    output.setPixel(x, y, qRgb(r, g, b));
                }
            }
        }
    }
    
    // Blend with to frame based on progress
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            QRgb fromPixel = output.pixel(x, y);
            QRgb toPixel = toFrame.pixel(x, y);
            
            int r = static_cast<int>(qRed(fromPixel) * (1 - t) + qRed(toPixel) * t);
            int g = static_cast<int>(qGreen(fromPixel) * (1 - t) + qGreen(toPixel) * t);
            int b = static_cast<int>(qBlue(fromPixel) * (1 - t) + qBlue(toPixel) * t);
            
            output.setPixel(x, y, qRgb(r, g, b));
        }
    }
}

// ==================== PageCurlTransition ====================

PageCurlTransition::PageCurlTransition(QObject* parent)
    : AbstractTransition(TransitionType::PageCurl, parent)
{
}

PageCurlTransition::~PageCurlTransition()
{
}

void PageCurlTransition::process(const QImage& fromFrame,
                                const QImage& toFrame,
                                QImage& output,
                                float progress)
{
    float t = applyEasing(progress);
    
    if (output.size() != fromFrame.size()) {
        output = QImage(fromFrame.size(), QImage::Format_RGB32);
    }
    
    int w = output.width();
    int h = output.height();
    
    // Draw to frame as background
    output = toFrame.copy();
    
    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Calculate curl position
    float curlX = t * (w + curlParams_.radius * 2);
    
    // Create curl path
    QPainterPath clipPath;
    clipPath.addRect(0, 0, curlX, h);
    
    painter.setClipPath(clipPath);
    painter.drawImage(0, 0, fromFrame);
    
    // Draw curl shadow
    if (curlParams_.shadow > 0) {
        QLinearGradient shadowGradient(curlX - 20, 0, curlX, 0);
        shadowGradient.setColorAt(0, QColor(0, 0, 0, 0));
        shadowGradient.setColorAt(1, QColor(0, 0, 0, static_cast<int>(100 * curlParams_.shadow)));
        
        painter.setClipRect(curlX - 20, 0, 20, h);
        painter.fillRect(curlX - 20, 0, 20, h, shadowGradient);
    }
    
    // Draw curled edge (simplified)
    if (curlX < w && curlX > 0) {
        float angleRad = qDegreesToRadians(curlParams_.angle);
        
        // Curl surface gradient (gives 3D effect)
        QLinearGradient curlGradient(curlX - curlParams_.radius, 0, curlX, 0);
        curlGradient.setColorAt(0, curlParams_.backColor);
        curlGradient.setColorAt(1, curlParams_.backColor.darker(150));
        
        painter.setClipping(false);
        painter.setPen(Qt::NoPen);
        painter.setBrush(curlGradient);
        
        // Draw curl region
        QPainterPath curlPath;
        curlPath.moveTo(curlX - curlParams_.radius, 0);
        curlPath.quadTo(curlX, 0, curlX + curlParams_.radius * std::cos(angleRad), 0);
        curlPath.lineTo(curlX + curlParams_.radius * std::cos(angleRad), h);
        curlPath.quadTo(curlX, h, curlX - curlParams_.radius, h);
        curlPath.closeSubpath();
        
        painter.drawPath(curlPath);
    }
}

// ==================== RippleTransition ====================

RippleTransition::RippleTransition(QObject* parent)
    : AbstractTransition(TransitionType::RippleDissolve, parent)
{
}

RippleTransition::~RippleTransition()
{
}

void RippleTransition::process(const QImage& fromFrame,
                              const QImage& toFrame,
                              QImage& output,
                              float progress)
{
    float t = applyEasing(progress);
    
    if (output.size() != fromFrame.size()) {
        output = QImage(fromFrame.size(), QImage::Format_RGB32);
    }
    
    int w = output.width();
    int h = output.height();
    
    float cx = rippleParams_.center.x() * w;
    float cy = rippleParams_.center.y() * h;
    float time = progress * rippleParams_.speed * 10.0f;
    
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // Calculate distance from center
            float dx = x - cx;
            float dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy);
            
            // Ripple wave
            float wave = std::sin(dist * rippleParams_.frequency * 0.01f - time) * 
                        rippleParams_.amplitude * (1.0f - t);
            
            // Offset coordinates
            float offsetX = x + dx / dist * wave * rippleParams_.distortion;
            float offsetY = y + dy / dist * wave * rippleParams_.distortion;
            
            // Clamp coordinates
            int sx = std::clamp(static_cast<int>(offsetX), 0, w - 1);
            int sy = std::clamp(static_cast<int>(offsetY), 0, h - 1);
            
            // Sample from both frames and blend
            QRgb fromPixel = fromFrame.pixel(sx, sy);
            QRgb toPixel = toFrame.pixel(x, y);
            
            // Ripple intensity affects blend
            float rippleIntensity = std::abs(wave) / rippleParams_.amplitude;
            float blend = t + rippleIntensity * 0.3f * (1.0f - t);
            blend = std::clamp(blend, 0.0f, 1.0f);
            
            int r = static_cast<int>(qRed(fromPixel) * (1 - blend) + qRed(toPixel) * blend);
            int g = static_cast<int>(qGreen(fromPixel) * (1 - blend) + qGreen(toPixel) * blend);
            int b = static_cast<int>(qBlue(fromPixel) * (1 - blend) + qBlue(toPixel) * blend);
            
            output.setPixel(x, y, qRgb(r, g, b));
        }
    }
}

// ==================== TransitionFactory ====================

std::unique_ptr<AbstractTransition> TransitionFactory::create(TransitionType type)
{
    switch (type) {
        case TransitionType::CrossDissolve:
        case TransitionType::Fade:
            return std::make_unique<CrossDissolveTransition>();
        
        case TransitionType::WipeLeft:
        case TransitionType::WipeRight:
        case TransitionType::WipeUp:
        case TransitionType::WipeDown:
        case TransitionType::WipeRadial:
        case TransitionType::WipeClock:
        case TransitionType::WipeDiamond:
            return std::make_unique<WipeTransition>(type);
        
        case TransitionType::SlideLeft:
        case TransitionType::SlideRight:
        case TransitionType::SlideUp:
        case TransitionType::SlideDown:
        case TransitionType::PushLeft:
        case TransitionType::PushRight:
        case TransitionType::PushUp:
        case TransitionType::PushDown:
            return std::make_unique<SlideTransition>(type);
        
        case TransitionType::ZoomIn:
        case TransitionType::ZoomOut:
        case TransitionType::ZoomRotate:
            return std::make_unique<ZoomTransition>(type);
        
        case TransitionType::Glitch:
            return std::make_unique<GlitchTransition>();
        
        case TransitionType::PageCurl:
            return std::make_unique<PageCurlTransition>();
        
        case TransitionType::RippleDissolve:
            return std::make_unique<RippleTransition>();
        
        default:
            return std::make_unique<CrossDissolveTransition>();
    }
}

QStringList TransitionFactory::availableTransitions()
{
    return {
        "CrossDissolve",
        "Fade",
        "DipToBlack",
        "DipToWhite",
        "WipeLeft",
        "WipeRight",
        "WipeUp",
        "WipeDown",
        "WipeRadial",
        "WipeClock",
        "WipeDiamond",
        "SlideLeft",
        "SlideRight",
        "SlideUp",
        "SlideDown",
        "PushLeft",
        "PushRight",
        "PushUp",
        "PushDown",
        "ZoomIn",
        "ZoomOut",
        "ZoomRotate",
        "Glitch",
        "PageCurl",
        "RippleDissolve"
    };
}

QString TransitionFactory::transitionName(TransitionType type)
{
    switch (type) {
        case TransitionType::CrossDissolve: return "CrossDissolve";
        case TransitionType::Fade: return "Fade";
        case TransitionType::DipToBlack: return "DipToBlack";
        case TransitionType::DipToWhite: return "DipToWhite";
        case TransitionType::WipeLeft: return "WipeLeft";
        case TransitionType::WipeRight: return "WipeRight";
        case TransitionType::WipeUp: return "WipeUp";
        case TransitionType::WipeDown: return "WipeDown";
        case TransitionType::WipeRadial: return "WipeRadial";
        case TransitionType::WipeClock: return "WipeClock";
        case TransitionType::WipeDiamond: return "WipeDiamond";
        case TransitionType::SlideLeft: return "SlideLeft";
        case TransitionType::SlideRight: return "SlideRight";
        case TransitionType::SlideUp: return "SlideUp";
        case TransitionType::SlideDown: return "SlideDown";
        case TransitionType::PushLeft: return "PushLeft";
        case TransitionType::PushRight: return "PushRight";
        case TransitionType::PushUp: return "PushUp";
        case TransitionType::PushDown: return "PushDown";
        case TransitionType::ZoomIn: return "ZoomIn";
        case TransitionType::ZoomOut: return "ZoomOut";
        case TransitionType::ZoomRotate: return "ZoomRotate";
        case TransitionType::Glitch: return "Glitch";
        case TransitionType::PageCurl: return "PageCurl";
        case TransitionType::RippleDissolve: return "RippleDissolve";
        default: return "Unknown";
    }
}

QString TransitionFactory::transitionDisplayName(TransitionType type)
{
    switch (type) {
        case TransitionType::CrossDissolve: return tr("Cross Dissolve");
        case TransitionType::Fade: return tr("Fade");
        case TransitionType::DipToBlack: return tr("Dip to Black");
        case TransitionType::DipToWhite: return tr("Dip to White");
        case TransitionType::WipeLeft: return tr("Wipe Left");
        case TransitionType::WipeRight: return tr("Wipe Right");
        case TransitionType::WipeUp: return tr("Wipe Up");
        case TransitionType::WipeDown: return tr("Wipe Down");
        case TransitionType::WipeRadial: return tr("Radial Wipe");
        case TransitionType::WipeClock: return tr("Clock Wipe");
        case TransitionType::WipeDiamond: return tr("Diamond Wipe");
        case TransitionType::SlideLeft: return tr("Slide Left");
        case TransitionType::SlideRight: return tr("Slide Right");
        case TransitionType::SlideUp: return tr("Slide Up");
        case TransitionType::SlideDown: return tr("Slide Down");
        case TransitionType::PushLeft: return tr("Push Left");
        case TransitionType::PushRight: return tr("Push Right");
        case TransitionType::PushUp: return tr("Push Up");
        case TransitionType::PushDown: return tr("Push Down");
        case TransitionType::ZoomIn: return tr("Zoom In");
        case TransitionType::ZoomOut: return tr("Zoom Out");
        case TransitionType::ZoomRotate: return tr("Zoom Rotate");
        case TransitionType::Glitch: return tr("Glitch");
        case TransitionType::PageCurl: return tr("Page Curl");
        case TransitionType::RippleDissolve: return tr("Ripple Dissolve");
        default: return tr("Unknown");
    }
}

// ==================== TransitionPresets ====================

CrossDissolveTransition* TransitionPresets::quickDissolve()
{
    auto* t = new CrossDissolveTransition();
    t->params().duration = 0.3f;
    t->params().easing = QEasingCurve::Type::InOutQuad;
    return t;
}

CrossDissolveTransition* TransitionPresets::slowFade()
{
    auto* t = new CrossDissolveTransition();
    t->params().duration = 2.0f;
    t->params().easing = QEasingCurve::Type::InOutCubic;
    return t;
}

WipeTransition* TransitionPresets::smoothWipeLeft()
{
    auto* t = new WipeTransition(TransitionType::WipeLeft);
    t->params().duration = 0.8f;
    t->wipeParams().softness = 0.05f;
    t->params().easing = QEasingCurve::Type::InOutQuad;
    return t;
}

WipeTransition* TransitionPresets::smoothWipeRight()
{
    auto* t = new WipeTransition(TransitionType::WipeRight);
    t->params().duration = 0.8f;
    t->wipeParams().softness = 0.05f;
    t->params().easing = QEasingCurve::Type::InOutQuad;
    return t;
}

SlideTransition* TransitionPresets::pushLeft()
{
    auto* t = new SlideTransition(TransitionType::PushLeft);
    t->params().duration = 0.5f;
    t->slideParams().push = true;
    t->params().easing = QEasingCurve::Type::OutCubic;
    return t;
}

SlideTransition* TransitionPresets::pushRight()
{
    auto* t = new SlideTransition(TransitionType::PushRight);
    t->params().duration = 0.5f;
    t->slideParams().push = true;
    t->params().easing = QEasingCurve::Type::OutCubic;
    return t;
}

ZoomTransition* TransitionPresets::cinematicZoom()
{
    auto* t = new ZoomTransition(TransitionType::ZoomIn);
    t->params().duration = 1.2f;
    t->zoomParams().startScale = 1.0f;
    t->zoomParams().endScale = 1.5f;
    t->params().easing = QEasingCurve::Type::InOutCubic;
    return t;
}

ZoomTransition* TransitionPresets::spinOut()
{
    auto* t = new ZoomTransition(TransitionType::ZoomRotate);
    t->params().duration = 0.8f;
    t->zoomParams().rotation = 45.0f;
    t->params().easing = QEasingCurve::Type::InBack;
    return t;
}

GlitchTransition* TransitionPresets::digitalGlitch()
{
    auto* t = new GlitchTransition();
    t->params().duration = 0.5f;
    t->glitchParams().intensity = 0.8f;
    t->glitchParams().blockSize = 8.0f;
    t->glitchParams().colorSeparation = 15.0f;
    return t;
}

PageCurlTransition* TransitionPresets::documentFlip()
{
    auto* t = new PageCurlTransition();
    t->params().duration = 1.0f;
    t->curlParams().angle = 30.0f;
    t->curlParams().shadow = 0.7f;
    t->params().easing = QEasingCurve::Type::OutQuad;
    return t;
}

RippleTransition* TransitionPresets::waterRipple()
{
    auto* t = new RippleTransition();
    t->params().duration = 1.5f;
    t->rippleParams().amplitude = 30.0f;
    t->rippleParams().frequency = 8.0f;
    t->params().easing = QEasingCurve::Type::InOutSine;
    return t;
}

CrossDissolveTransition* TransitionPresets::quickCut()
{
    auto* t = new CrossDissolveTransition();
    t->params().duration = 0.1f;
    return t;
}

WipeTransition* TransitionPresets::quickWipe()
{
    auto* t = new WipeTransition(TransitionType::WipeRight);
    t->params().duration = 0.2f;
    return t;
}

SlideTransition* TransitionPresets::quickSlide()
{
    auto* t = new SlideTransition(TransitionType::SlideLeft);
    t->params().duration = 0.2f;
    return t;
}

// ==================== TransitionManager ====================

class TransitionManager::Impl {
public:
    QMap<QString, std::unique_ptr<AbstractTransition>> transitions;
    QString defaultTransition = "CrossDissolve";
};

TransitionManager::TransitionManager(QObject* parent)
    : QObject(parent)
    , impl_(std::make_unique<Impl>())
{
    // Register default transitions
    registerTransition("CrossDissolve", TransitionFactory::create(TransitionType::CrossDissolve));
    registerTransition("WipeLeft", TransitionFactory::create(TransitionType::WipeLeft));
    registerTransition("WipeRight", TransitionFactory::create(TransitionType::WipeRight));
    registerTransition("SlideLeft", TransitionFactory::create(TransitionType::SlideLeft));
    registerTransition("SlideRight", TransitionFactory::create(TransitionType::SlideRight));
    registerTransition("ZoomIn", TransitionFactory::create(TransitionType::ZoomIn));
    registerTransition("ZoomOut", TransitionFactory::create(TransitionType::ZoomOut));
    registerTransition("Glitch", TransitionFactory::create(TransitionType::Glitch));
    registerTransition("PageCurl", TransitionFactory::create(TransitionType::PageCurl));
    registerTransition("RippleDissolve", TransitionFactory::create(TransitionType::RippleDissolve));
}

TransitionManager::~TransitionManager()
{
}

void TransitionManager::registerTransition(const QString& name, std::unique_ptr<AbstractTransition> transition)
{
    impl_->transitions[name] = std::move(transition);
    emit transitionListChanged();
}

AbstractTransition* TransitionManager::transition(const QString& name) const
{
    if (impl_->transitions.contains(name)) {
        return impl_->transitions[name].get();
    }
    return nullptr;
}

void TransitionManager::applyTransition(const QString& name,
                                       const QImage& fromFrame,
                                       const QImage& toFrame,
                                       QImage& output,
                                       float progress)
{
    AbstractTransition* t = transition(name);
    if (t) {
        t->process(fromFrame, toFrame, output, progress);
        emit transitionApplied(name);
    } else {
        // Fallback to simple cross dissolve
        output = toFrame;
    }
}

QStringList TransitionManager::availableTransitions() const
{
    return impl_->transitions.keys();
}

QString TransitionManager::defaultTransition() const
{
    return impl_->defaultTransition;
}

void TransitionManager::setDefaultTransition(const QString& name)
{
    impl_->defaultTransition = name;
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::AbstractTransition)
W_OBJECT_IMPL(Artifact::CrossDissolveTransition)
W_OBJECT_IMPL(Artifact::WipeTransition)
W_OBJECT_IMPL(Artifact::SlideTransition)
W_OBJECT_IMPL(Artifact::ZoomTransition)
W_OBJECT_IMPL(Artifact::GlitchTransition)
W_OBJECT_IMPL(Artifact::PageCurlTransition)
W_OBJECT_IMPL(Artifact::RippleTransition)
W_OBJECT_IMPL(Artifact::TransitionManager)
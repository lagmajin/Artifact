module;
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <QMatrix4x4>
#include <QVector3D>
#include <QJsonObject>
#include <QImage>
#include <QSize>
#include <QDebug>
#include <QObject>
#include <QString>
#include <QVariant>

module Artifact.Layer.MpmSand2D;

import Artifact.Composition.Abstract;
import Artifact.Layer.PhysicsBridge;
import Artifact.Render.IRenderer;
import FloatRGBA;
import Color.Float;
import Color.TransferFunction;
import Image.ImageF32x4_RGBA;
import Physics.Mpm2D;
import Graphics.SurfaceColorContract;
import Property.Abstract;
import Property.Group;
import Property.SerializationBridge;
import Size;
import Translation.Manager;

namespace Artifact {

using namespace ArtifactCore;

namespace {

constexpr int kMpmDensityFieldWidth = 128;
constexpr int kMpmDensityFieldHeight = 96;
constexpr int kMpmSurfaceGridWidth = kMpmDensityFieldWidth + 2;
constexpr int kMpmSurfaceGridHeight = kMpmDensityFieldHeight + 2;
constexpr std::size_t kMpmDensityFieldPixels =
    static_cast<std::size_t>(kMpmDensityFieldWidth) *
    static_cast<std::size_t>(kMpmDensityFieldHeight);
constexpr std::size_t kMpmMaximumSurfaceVertices =
    static_cast<std::size_t>(kMpmSurfaceGridWidth - 1) *
    static_cast<std::size_t>(kMpmSurfaceGridHeight - 1) * 6;
constexpr float kMpmDensityThreshold = 0.08f;
constexpr float kMpmSandRed = 0.77f;
constexpr float kMpmSandGreen = 0.66f;
constexpr float kMpmSandBlue = 0.43f;

float densityAlpha(float density) noexcept {
    constexpr float kHigh = 0.62f;
    const float t = std::clamp(
        (density - kMpmDensityThreshold) /
            (kHigh - kMpmDensityThreshold),
        0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float mpmSupportRadius(float layerWidth, float layerHeight, int columns,
                       int rows) noexcept {
    if (!std::isfinite(layerWidth) || !std::isfinite(layerHeight) ||
        layerWidth <= 0.0f || layerHeight <= 0.0f ||
        columns < 2 || columns > 500 || rows < 2 || rows > 200) {
        return 0.0f;
    }
    const double seedWidth = static_cast<double>(layerWidth) * 0.6;
    const double seedHeight = static_cast<double>(layerHeight) * 0.35;
    const double particleCount =
        static_cast<double>(columns) * static_cast<double>(rows);
    const double particleVolume = seedWidth * seedHeight / particleCount;
    const double nominalSupportRadius = std::sqrt(
        std::max(0.0, particleVolume) *
        2.0 / 3.14159265358979323846) * 1.25;
    const double maximumSupportRadius = 8.0 * std::min(
        static_cast<double>(layerWidth) / kMpmDensityFieldWidth,
        static_cast<double>(layerHeight) / kMpmDensityFieldHeight);
    const double supportRadius =
        std::min(nominalSupportRadius, maximumSupportRadius);
    if (!std::isfinite(supportRadius) || supportRadius <= 0.0 ||
        supportRadius > std::numeric_limits<float>::max()) {
        return 0.0f;
    }
    return static_cast<float>(supportRadius);
}

} // namespace

ArtifactMpmSand2DLayer::ArtifactMpmSand2DLayer() {
    setSourceSize(Size_2D(640, 480));
    // Composition evaluates authored MPM properties during simulation even
    // when the Inspector has never requested the full property groups.
    persistentLayerProperty(QStringLiteral("mpmSand.gravityY"),
                            PropertyType::Float, QVariant(gravityY_), 0);
    densityField_.resize(kMpmDensityFieldPixels);
    densityAlphaField_.resize(kMpmDensityFieldPixels);
    cpuSurface_.resize(kMpmDensityFieldWidth, kMpmDensityFieldHeight);
    cpuSurface_.setColorDescriptor(
        SurfaceColorDescriptor::linearStraightRgba32Float());
    for (auto& vertices : surfaceVertexBuffers_) {
        vertices.reserve(kMpmMaximumSurfaceVertices);
    }
}

QImage ArtifactMpmSand2DLayer::getThumbnail(int width, int height) const {
    auto* composition =
        dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    if (!composition || width <= 0 || height <= 0) {
        return ArtifactAbstractLayer::getThumbnail(width, height);
    }
    auto solver = LayerPhysicsBridge::granularMaterialSolver(
        composition->runtimePhysicsWorldId(), id());
    const float layerWidth = static_cast<float>(sourceSize().width);
    const float layerHeight = static_cast<float>(sourceSize().height);
    const float supportRadius = mpmSupportRadius(
        layerWidth, layerHeight, columns_, rows_);
    if (!solver || !std::isfinite(layerWidth) ||
        !std::isfinite(layerHeight) || layerWidth <= 0.0f ||
        layerHeight <= 0.0f || !std::isfinite(supportRadius) ||
        supportRadius <= 0.0f ||
        !solver->writeDensityField(
            densityField_.data(), densityField_.size(),
            kMpmDensityFieldWidth, kMpmDensityFieldHeight,
            0.0f, 0.0f, layerWidth, layerHeight, supportRadius)) {
        return ArtifactAbstractLayer::getThumbnail(width, height);
    }

    if (cpuSurface_.width() != kMpmDensityFieldWidth ||
        cpuSurface_.height() != kMpmDensityFieldHeight) {
        cpuSurface_.resize(kMpmDensityFieldWidth, kMpmDensityFieldHeight);
        cpuSurface_.setColorDescriptor(
            SurfaceColorDescriptor::linearStraightRgba32Float());
    }

    for (int y = 0; y < kMpmDensityFieldHeight; ++y) {
        for (int x = 0; x < kMpmDensityFieldWidth; ++x) {
            const auto index = static_cast<std::size_t>(y) *
                                   kMpmDensityFieldWidth +
                               static_cast<std::size_t>(x);
            cpuSurface_.setPixel(
                x, y, FloatRGBA(kMpmSandRed, kMpmSandGreen, kMpmSandBlue,
                                densityAlpha(densityField_[index])));
        }
    }

    const QImage image = cpuSurface_.toQImage();
    if (image.isNull()) return {};
    const QSize targetSize(std::max(1, width), std::max(1, height));
    const QSize layerAspectSize(
        std::max(1, sourceSize().width),
        std::max(1, sourceSize().height));
    const QSize outputSize =
        layerAspectSize.scaled(targetSize, Qt::KeepAspectRatio);
    return image.scaled(outputSize, Qt::IgnoreAspectRatio,
                        Qt::SmoothTransformation);
}

QImage ArtifactMpmSand2DLayer::getSoftwareRenderSurface(
    int width, int height, QRectF* outLocalBounds,
    bool preserveRequestedSize) const {
    if (outLocalBounds) *outLocalBounds = particleRenderBounds();
    auto* composition =
        dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    if (!composition || width <= 0 || height <= 0) return {};
    auto solver = LayerPhysicsBridge::granularMaterialSolver(
        composition->runtimePhysicsWorldId(), id());
    const int sourceWidth = sourceSize().width;
    const int sourceHeight = sourceSize().height;
    const float layerWidth = static_cast<float>(sourceWidth);
    const float layerHeight = static_cast<float>(sourceHeight);
    const float supportRadius = mpmSupportRadius(
        layerWidth, layerHeight, columns_, rows_);
    const QRectF renderBounds = particleRenderBounds();
    const float renderLeft = static_cast<float>(renderBounds.left());
    const float renderTop = static_cast<float>(renderBounds.top());
    const float renderWidth = static_cast<float>(renderBounds.width());
    const float renderHeight = static_cast<float>(renderBounds.height());
    if (!solver || sourceWidth <= 0 || sourceHeight <= 0 ||
        !std::isfinite(supportRadius) || supportRadius <= 0.0f ||
        !renderBounds.isValid() || renderBounds.isEmpty() ||
        !std::isfinite(renderLeft) || !std::isfinite(renderTop) ||
        !std::isfinite(renderWidth) || !std::isfinite(renderHeight) ||
        renderWidth <= 0.0f || renderHeight <= 0.0f ||
        !solver->writeDensityField(
            densityField_.data(), densityField_.size(),
            kMpmDensityFieldWidth, kMpmDensityFieldHeight,
            renderLeft, renderTop, renderWidth, renderHeight, supportRadius)) {
        return {};
    }
    for (std::size_t index = 0; index < densityField_.size(); ++index) {
        densityAlphaField_[index] = densityAlpha(densityField_[index]);
    }

    QSize outputSize(std::max(1, width), std::max(1, height));
    if (preserveRequestedSize) {
        constexpr double kMaximumSurfaceDimension = 4096.0;
        constexpr double kMaximumSurfacePixels = 4.0 * 1024.0 * 1024.0;
        const double requestedWidth = static_cast<double>(outputSize.width());
        const double requestedHeight = static_cast<double>(outputSize.height());
        const double boundedScale = std::min(
            {1.0, kMaximumSurfaceDimension / requestedWidth,
             kMaximumSurfaceDimension / requestedHeight,
             std::sqrt(kMaximumSurfacePixels / requestedWidth /
                       requestedHeight)});
        outputSize = QSize(
            static_cast<int>(std::clamp(
                std::floor(requestedWidth * boundedScale), 1.0,
                kMaximumSurfaceDimension)),
            static_cast<int>(std::clamp(
                std::floor(requestedHeight * boundedScale), 1.0,
                kMaximumSurfaceDimension)));
    } else {
        const double outputScale = std::min(
            static_cast<double>(outputSize.width()) / renderWidth,
            static_cast<double>(outputSize.height()) / renderHeight);
        if (!std::isfinite(outputScale) || outputScale <= 0.0) return {};
        const auto scaledDimension = [outputScale](double extent,
                                                    int maximum) noexcept {
            return static_cast<int>(std::clamp(
                std::ceil(extent * outputScale), 1.0,
                static_cast<double>(std::max(1, maximum))));
        };
        outputSize = QSize(scaledDimension(renderWidth, width),
                           scaledDimension(renderHeight, height));
    }
    QImage* output = nullptr;
    QImage fallbackOutput;
    for (auto& buffer : cpuRenderSurfaceBuffers_) {
        if (buffer.isNull() || buffer.isDetached()) {
            output = &buffer;
            break;
        }
    }
    // The two owned buffers cover the CPU render queue's matte prepass and
    // final layer pass. If callers retain both surfaces longer, use the
    // bounded image boundary without invalidating their images. Keep the
    // particle-expanded sampling bounds in this exceptional third-buffer path.
    if (!output) {
        fallbackOutput = QImage(
            outputSize, QImage::Format_ARGB32_Premultiplied);
        output = &fallbackOutput;
    }
    if (output->size() != outputSize ||
        output->format() != QImage::Format_ARGB32_Premultiplied) {
        *output = QImage(outputSize, QImage::Format_ARGB32_Premultiplied);
    }
    if (output->isNull()) return {};

    static const float kSandRedSrgb =
        ColorTransferFunction::linearToSRGB(kMpmSandRed);
    static const float kSandGreenSrgb =
        ColorTransferFunction::linearToSRGB(kMpmSandGreen);
    static const float kSandBlueSrgb =
        ColorTransferFunction::linearToSRGB(kMpmSandBlue);
    const auto toByte = [](float value) noexcept {
        return static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 255.0f +
                                0.5f);
    };
    static const float kSandRedSrgb8 =
        static_cast<float>(toByte(kSandRedSrgb)) / 255.0f;
    static const float kSandGreenSrgb8 =
        static_cast<float>(toByte(kSandGreenSrgb)) / 255.0f;
    static const float kSandBlueSrgb8 =
        static_cast<float>(toByte(kSandBlueSrgb)) / 255.0f;
    const float scaleX = static_cast<float>(kMpmDensityFieldWidth) /
                         static_cast<float>(outputSize.width());
    const float scaleY = static_cast<float>(kMpmDensityFieldHeight) /
                         static_cast<float>(outputSize.height());
    for (int y = 0; y < outputSize.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(output->scanLine(y));
        const float sourceY = std::clamp(
            (static_cast<float>(y) + 0.5f) * scaleY - 0.5f,
            0.0f, static_cast<float>(kMpmDensityFieldHeight - 1));
        const int y0 = static_cast<int>(sourceY);
        const int y1 = std::min(y0 + 1, kMpmDensityFieldHeight - 1);
        const float fy = sourceY - static_cast<float>(y0);
        for (int x = 0; x < outputSize.width(); ++x) {
            const float sourceX = std::clamp(
                (static_cast<float>(x) + 0.5f) * scaleX - 0.5f,
                0.0f, static_cast<float>(kMpmDensityFieldWidth - 1));
            const int x0 = static_cast<int>(sourceX);
            const int x1 = std::min(x0 + 1, kMpmDensityFieldWidth - 1);
            const float fx = sourceX - static_cast<float>(x0);
            const auto densityAt = [this](int sampleX, int sampleY) {
                const auto index = static_cast<std::size_t>(sampleY) *
                                       kMpmDensityFieldWidth +
                                   static_cast<std::size_t>(sampleX);
                return densityAlphaField_[index];
            };
            const float a00 = densityAt(x0, y0);
            const float a10 = densityAt(x1, y0);
            const float a01 = densityAt(x0, y1);
            const float a11 = densityAt(x1, y1);
            // Match the GPU surface's (top-left, bottom-left, top-right) /
            // (bottom-left, bottom-right, top-right) triangle split. Bilinear
            // interpolation produces a different field between the diagonal.
            const float alpha = std::clamp(
                fx + fy <= 1.0f
                    ? a00 * (1.0f - fx - fy) + a01 * fy + a10 * fx
                    : a01 * (1.0f - fx) +
                          a11 * (fx + fy - 1.0f) + a10 * (1.0f - fy),
                0.0f, 1.0f);
            const int alphaByte = toByte(alpha);
            const float quantizedAlpha =
                static_cast<float>(alphaByte) / 255.0f;
            row[x] = qRgba(toByte(kSandRedSrgb8 * quantizedAlpha),
                           toByte(kSandGreenSrgb8 * quantizedAlpha),
                           toByte(kSandBlueSrgb8 * quantizedAlpha), alphaByte);
        }
    }
    return *output;
}

bool ArtifactMpmSand2DLayer::configureSolver() {
    auto* composition = dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    if (!composition || id().isNil()) return false;
    const float width = static_cast<float>(sourceSize().width);
    const float height = static_cast<float>(sourceSize().height);
    const float supportRadius = mpmSupportRadius(
        width, height, columns_, rows_);
    if (!std::isfinite(supportRadius) || supportRadius <= 0.0f) return false;
    return static_cast<bool>(LayerPhysicsBridge::createGranularMaterialGrid(
        composition->runtimePhysicsWorldId(), id(), width * 0.2f, height * 0.1f,
        width * 0.6f, height * 0.35f, 0.0f, 0.0f, width, height,
        columns_, rows_, youngModulus_,
        poissonRatio_, frictionAngleRadians_, density_, gravityY_, damping_,
        supportRadius));
}

void ArtifactMpmSand2DLayer::setComposition(QObject* composition) {
    auto* oldComposition = dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    auto* newComposition = dynamic_cast<ArtifactAbstractComposition*>(composition);
    if (oldComposition == newComposition) {
        // Composition frame-rate changes reuse this binding to update the
        // base and variant transform time scales. Keep a valid live solver,
        // but recover a missing registration if another lifecycle path
        // removed it without detaching this layer.
        ArtifactAbstractLayer::setComposition(composition);
        if (newComposition &&
            !LayerPhysicsBridge::granularMaterialSolver(
                newComposition->runtimePhysicsWorldId(), id()) &&
            !configureSolver()) {
            qWarning() << "MPM sand solver rebind recovery failed for layer"
                       << id().toString() << "in physics world"
                       << newComposition->runtimePhysicsWorldId();
        }
        return;
    }
    if (oldComposition && !id().isNil()) {
        LayerPhysicsBridge::removeGranularMaterialSolver(
            oldComposition->runtimePhysicsWorldId(), id());
    }
    ArtifactAbstractLayer::setComposition(composition);
    if (newComposition && !configureSolver()) {
        qWarning() << "MPM sand solver initialization failed for layer"
                   << id().toString() << "in physics world"
                   << newComposition->runtimePhysicsWorldId();
    }
}

bool ArtifactMpmSand2DLayer::isCompositionAttachmentReady() const {
    auto* composition =
        dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    if (!composition) return true;
    return static_cast<bool>(LayerPhysicsBridge::granularMaterialSolver(
        composition->runtimePhysicsWorldId(), id()));
}

bool ArtifactMpmSand2DLayer::isGridExpansionLimitReached() const {
    auto* composition =
        dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    if (!composition) return false;
    const auto solver = LayerPhysicsBridge::granularMaterialSolver(
        composition->runtimePhysicsWorldId(), id());
    return solver && solver->gridExpansionLimitReached();
}

float ArtifactMpmSand2DLayer::simulationBacklogSeconds() const {
    auto* composition =
        dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    if (!composition) return 0.0f;
    return LayerPhysicsBridge::granularMaterialBacklogSeconds(
        composition->runtimePhysicsWorldId(), id());
}

QRectF ArtifactMpmSand2DLayer::particleRenderBounds() const {
    const Size_2D authoredSize = sourceSize();
    const float layerWidth = static_cast<float>(authoredSize.width);
    const float layerHeight = static_cast<float>(authoredSize.height);
    if (!std::isfinite(layerWidth) || !std::isfinite(layerHeight) ||
        layerWidth <= 0.0f || layerHeight <= 0.0f) {
        return {};
    }

    float left = 0.0f;
    float top = 0.0f;
    float right = layerWidth;
    float bottom = layerHeight;
    auto* composition =
        dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    if (composition) {
        const auto solver = LayerPhysicsBridge::granularMaterialSolver(
            composition->runtimePhysicsWorldId(), id());
        const float supportRadius = mpmSupportRadius(
            layerWidth, layerHeight, columns_, rows_);
        if (solver && std::isfinite(supportRadius) && supportRadius > 0.0f) {
            const auto particleBounds = solver->particleBounds(supportRadius);
            if (particleBounds.valid) {
                left = std::min(left, particleBounds.left);
                top = std::min(top, particleBounds.top);
                right = std::max(right, particleBounds.right);
                bottom = std::max(bottom, particleBounds.bottom);
            }
        }
    }
    if (left < 0.0f || right > layerWidth) {
        const float padding = (right - left) / 12.0f;
        left -= padding;
        right += padding;
    }
    if (top < 0.0f || bottom > layerHeight) {
        const float padding = (bottom - top) / 12.0f;
        top -= padding;
        bottom += padding;
    }
    if (!std::isfinite(left) || !std::isfinite(top) ||
        !std::isfinite(right) || !std::isfinite(bottom) ||
        right <= left || bottom <= top) {
        return {};
    }
    return QRectF(left, top, right - left, bottom - top);
}

void ArtifactMpmSand2DLayer::draw(ArtifactIRenderer* renderer) {
    draw(renderer, opacity());
}

void ArtifactMpmSand2DLayer::draw(ArtifactIRenderer* renderer,
                                  float opacityOverride) {
    if (!renderer) return;
    auto* composition = dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    if (!composition) return;
    auto solver = LayerPhysicsBridge::granularMaterialSolver(
        composition->runtimePhysicsWorldId(), id());
    if (!solver) return;
    const float safeOpacity = std::isfinite(opacityOverride)
        ? std::clamp(opacityOverride, 0.0f, 1.0f) : 0.0f;
    if (safeOpacity <= 0.0f) return;
    ArtifactArray<MpmSurfaceVertex>* surfaceVertices = nullptr;
    for (auto& buffer : surfaceVertexBuffers_) {
        if (buffer.capacity() >= kMpmMaximumSurfaceVertices) {
            surfaceVertices = &buffer;
            break;
        }
    }
    if (!surfaceVertices) {
        for (auto& buffer : surfaceVertexBuffers_) {
            if (renderer->acquireMpmSurfaceBuffer(
                    buffer, kMpmMaximumSurfaceVertices)) {
                surfaceVertices = &buffer;
                break;
            }
        }
    }
    if (!surfaceVertices) return;

    const float layerWidth = static_cast<float>(sourceSize().width);
    const float layerHeight = static_cast<float>(sourceSize().height);
    if (!std::isfinite(layerWidth) || !std::isfinite(layerHeight) ||
        layerWidth <= 0.0f || layerHeight <= 0.0f) {
        return;
    }
    const float supportRadius = mpmSupportRadius(
        layerWidth, layerHeight, columns_, rows_);
    if (!std::isfinite(supportRadius) || supportRadius <= 0.0f) {
        return;
    }
    const QRectF renderBounds = particleRenderBounds();
    if (!renderBounds.isValid() || renderBounds.isEmpty()) return;
    const float renderLeft = static_cast<float>(renderBounds.left());
    const float renderTop = static_cast<float>(renderBounds.top());
    const float renderWidth = static_cast<float>(renderBounds.width());
    const float renderHeight = static_cast<float>(renderBounds.height());
    if (!std::isfinite(renderLeft) || !std::isfinite(renderTop) ||
        !std::isfinite(renderWidth) || !std::isfinite(renderHeight) ||
        renderWidth <= 0.0f || renderHeight <= 0.0f ||
        !solver->writeDensityField(
            densityField_.data(), densityField_.size(),
            kMpmDensityFieldWidth, kMpmDensityFieldHeight,
            renderLeft, renderTop, renderWidth, renderHeight, supportRadius)) {
        return;
    }

    const QMatrix4x4 transform = getGlobalTransform4x4();
    struct SurfaceSample {
        float x;
        float y;
        float alpha;
    };
    const auto makeVertex = [&](const SurfaceSample& sample) {
        const float localX = renderLeft +
            (sample.x - 0.5f) * renderWidth /
                static_cast<float>(kMpmDensityFieldWidth);
        const float localY = renderTop +
            (sample.y - 0.5f) * renderHeight /
                static_cast<float>(kMpmDensityFieldHeight);
        const QVector3D mapped = transform.map(QVector3D(localX, localY, 0.0f));
        return MpmSurfaceVertex{mapped.x(), mapped.y(), sample.alpha};
    };
    const auto emitAlphaTriangle = [&](const SurfaceSample& a,
                                       const SurfaceSample& b,
                                       const SurfaceSample& c) {
        // Keep the original density-grid triangle so GPU interpolation uses
        // the same vertex alpha values and diagonal as the CPU fallback.
        // Triangles whose three vertex alphas are zero cannot affect the
        // composited image and are omitted to keep the surface sparse.
        if (a.alpha <= 0.0f && b.alpha <= 0.0f && c.alpha <= 0.0f) {
            return true;
        }
        if (surfaceVertices->size() + 3 > surfaceVertices->capacity()) {
            return false;
        }
        surfaceVertices->append(makeVertex(a));
        surfaceVertices->append(makeVertex(b));
        surfaceVertices->append(makeVertex(c));
        return true;
    };
    surfaceVertices->removeAll();
    for (int y = 0; y < kMpmSurfaceGridHeight - 1; ++y) {
        for (int x = 0; x < kMpmSurfaceGridWidth - 1; ++x) {
            const auto sampleDensity = [&](int sampleX, int sampleY) {
                const int clampedX = std::clamp(
                    sampleX, 0, kMpmDensityFieldWidth - 1);
                const int clampedY = std::clamp(
                    sampleY, 0, kMpmDensityFieldHeight - 1);
                const auto sampleIndex =
                    static_cast<std::size_t>(clampedY) *
                        kMpmDensityFieldWidth +
                    static_cast<std::size_t>(clampedX);
                return densityField_[sampleIndex];
            };
            const float d00 = sampleDensity(x - 1, y - 1);
            const float d10 = sampleDensity(x, y - 1);
            const float d01 = sampleDensity(x - 1, y);
            const float d11 = sampleDensity(x, y);
            const SurfaceSample v00{static_cast<float>(x),
                                    static_cast<float>(y), densityAlpha(d00)};
            const SurfaceSample v10{static_cast<float>(x + 1),
                                    static_cast<float>(y), densityAlpha(d10)};
            const SurfaceSample v01{static_cast<float>(x),
                                    static_cast<float>(y + 1), densityAlpha(d01)};
            const SurfaceSample v11{static_cast<float>(x + 1),
                                    static_cast<float>(y + 1), densityAlpha(d11)};
            if (!emitAlphaTriangle(v00, v01, v10) ||
                !emitAlphaTriangle(v01, v11, v10)) {
                surfaceVertices->removeAll();
                return;
            }
        }
    }
    if (surfaceVertices->isEmpty()) return;
    renderer->drawMpmSurface(
        *surfaceVertices,
        FloatColor(kMpmSandRed, kMpmSandGreen, kMpmSandBlue, safeOpacity));
}

std::vector<PropertyGroup> ArtifactMpmSand2DLayer::getLayerPropertyGroups() const {
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    auto& translations = TranslationManager::instance();
    PropertyGroup group(translations.tr(QStringLiteral("menu.layer.mpm_sand.group"),
                                        QStringLiteral("MPM Sand")));
    auto youngModulusProperty = persistentLayerProperty(
        QStringLiteral("mpmSand.youngModulus"), PropertyType::Float,
        QVariant(youngModulus_), 0);
    youngModulusProperty->setDisplayLabel(translations.tr(
        QStringLiteral("menu.layer.mpm_sand.young_modulus"),
        QStringLiteral("Young's Modulus")));
    youngModulusProperty->setHardRange(1.0, 1000000.0);
    youngModulusProperty->setSoftRange(1.0, 100000.0);
    youngModulusProperty->setStep(100.0);
    group.addProperty(youngModulusProperty);
    auto frictionAngleProperty = persistentLayerProperty(
        QStringLiteral("mpmSand.frictionAngleDegrees"), PropertyType::Float,
        QVariant(frictionAngleRadians_ * 180.0f / 3.14159265358979323846f), 0);
    frictionAngleProperty->setDisplayLabel(translations.tr(
        QStringLiteral("menu.layer.mpm_sand.friction_angle"),
        QStringLiteral("Friction Angle")));
    frictionAngleProperty->setUnit(QStringLiteral("deg"));
    frictionAngleProperty->setHardRange(0.0, 80.0);
    frictionAngleProperty->setStep(0.5);
    group.addProperty(frictionAngleProperty);
    auto poissonRatioProperty = persistentLayerProperty(
        QStringLiteral("mpmSand.poissonRatio"), PropertyType::Float,
        QVariant(poissonRatio_), 0);
    poissonRatioProperty->setDisplayLabel(translations.tr(
        QStringLiteral("menu.layer.mpm_sand.poisson_ratio"),
        QStringLiteral("Poisson Ratio")));
    poissonRatioProperty->setHardRange(-0.9, 0.45);
    poissonRatioProperty->setStep(0.01);
    group.addProperty(poissonRatioProperty);
    auto densityProperty = persistentLayerProperty(
        QStringLiteral("mpmSand.density"), PropertyType::Float,
        QVariant(density_), 0);
    densityProperty->setDisplayLabel(translations.tr(
        QStringLiteral("menu.layer.mpm_sand.density"),
        QStringLiteral("Density")));
    densityProperty->setHardRange(0.01, 100.0);
    densityProperty->setSoftRange(0.1, 20.0);
    densityProperty->setStep(0.1);
    group.addProperty(densityProperty);
    auto dampingProperty = persistentLayerProperty(
        QStringLiteral("mpmSand.damping"), PropertyType::Float,
        QVariant(damping_), 0);
    dampingProperty->setDisplayLabel(translations.tr(
        QStringLiteral("menu.layer.mpm_sand.damping"),
        QStringLiteral("Damping")));
    dampingProperty->setHardRange(0.0, 1.0);
    dampingProperty->setStep(0.01);
    group.addProperty(dampingProperty);
    auto gravityProperty = persistentLayerProperty(
        QStringLiteral("mpmSand.gravityY"), PropertyType::Float,
        QVariant(gravityY_), 0);
    gravityProperty->setDisplayLabel(translations.tr(
        QStringLiteral("menu.layer.mpm_sand.gravity_y"),
        QStringLiteral("Gravity Y")));
    gravityProperty->setUnit(QStringLiteral("px/s^2"));
    gravityProperty->setHardRange(-5000.0, 5000.0);
    gravityProperty->setSoftRange(-2000.0, 2000.0);
    gravityProperty->setStep(10.0);
    gravityProperty->setTooltip(translations.tr(
        QStringLiteral("menu.layer.mpm_sand.gravity_tooltip"),
        QStringLiteral(
            "Gravity acceleration in composition pixels per second squared.")));
    group.addProperty(gravityProperty);
    auto columnsProperty = persistentLayerProperty(
        QStringLiteral("mpmSand.columns"), PropertyType::Integer,
        QVariant(columns_), 0);
    columnsProperty->setDisplayLabel(translations.tr(
        QStringLiteral("menu.layer.mpm_sand.columns"),
        QStringLiteral("Columns")));
    columnsProperty->setHardRange(2.0, 500.0);
    columnsProperty->setStep(1.0);
    group.addProperty(columnsProperty);
    auto rowsProperty = persistentLayerProperty(
        QStringLiteral("mpmSand.rows"), PropertyType::Integer,
        QVariant(rows_), 0);
    rowsProperty->setDisplayLabel(translations.tr(
        QStringLiteral("menu.layer.mpm_sand.rows"), QStringLiteral("Rows")));
    rowsProperty->setHardRange(2.0, 200.0);
    rowsProperty->setStep(1.0);
    group.addProperty(rowsProperty);
    groups.push_back(std::move(group));
    return groups;
}

bool ArtifactMpmSand2DLayer::setLayerPropertyValue(const QString& path, const QVariant& value) {
    bool ok = false;
    const auto restoreCachedValue = [this, &path](const QVariant& currentValue) {
        if (const auto property = getProperty(path)) {
            property->setValue(currentValue);
        }
    };
    const auto syncCachedValue = [this, &path]() {
        const auto property = getProperty(path);
        if (!property) return;
        if (path == QStringLiteral("mpmSand.youngModulus")) {
            property->setValue(QVariant(youngModulus_));
        } else if (path == QStringLiteral("mpmSand.frictionAngleDegrees")) {
            property->setValue(QVariant(
                frictionAngleRadians_ * 180.0f / 3.14159265358979323846f));
        } else if (path == QStringLiteral("mpmSand.poissonRatio")) {
            property->setValue(QVariant(poissonRatio_));
        } else if (path == QStringLiteral("mpmSand.density")) {
            property->setValue(QVariant(density_));
        } else if (path == QStringLiteral("mpmSand.damping")) {
            property->setValue(QVariant(damping_));
        } else if (path == QStringLiteral("mpmSand.gravityY")) {
            property->setValue(QVariant(gravityY_));
        } else if (path == QStringLiteral("mpmSand.columns")) {
            property->setValue(QVariant(columns_));
        } else if (path == QStringLiteral("mpmSand.rows")) {
            property->setValue(QVariant(rows_));
        }
    };
    const float oldYoungModulus = youngModulus_;
    const float oldPoissonRatio = poissonRatio_;
    const float oldFrictionAngleRadians = frictionAngleRadians_;
    const float oldDensity = density_;
    const float oldDamping = damping_;
    const float oldGravityY = gravityY_;
    const int oldColumns = columns_;
    const int oldRows = rows_;
    if (path == QStringLiteral("mpmSand.youngModulus")) {
        const float next = value.toFloat(&ok);
        if (!ok || !std::isfinite(next) || next < 1.0f || next > 1000000.0f) {
            restoreCachedValue(youngModulus_);
            return false;
        }
        if (next == youngModulus_) {
            syncCachedValue();
            return true;
        }
        youngModulus_ = next;
    } else if (path == QStringLiteral("mpmSand.frictionAngleDegrees")) {
        const float degrees = value.toFloat(&ok);
        if (!ok || !std::isfinite(degrees) || degrees < 0.0f || degrees > 80.0f) {
            restoreCachedValue(frictionAngleRadians_ *
                               180.0f / 3.14159265358979323846f);
            return false;
        }
        const float nextRadians = degrees *
            (3.14159265358979323846f / 180.0f);
        if (nextRadians == frictionAngleRadians_) {
            syncCachedValue();
            return true;
        }
        frictionAngleRadians_ = nextRadians;
    } else if (path == QStringLiteral("mpmSand.poissonRatio")) {
        const float next = value.toFloat(&ok);
        if (!ok || !std::isfinite(next) || next < -0.9f || next > 0.45f) {
            restoreCachedValue(poissonRatio_);
            return false;
        }
        if (next == poissonRatio_) {
            syncCachedValue();
            return true;
        }
        poissonRatio_ = next;
    } else if (path == QStringLiteral("mpmSand.density")) {
        const float next = value.toFloat(&ok);
        if (!ok || !std::isfinite(next) || next < 0.01f || next > 100.0f) {
            restoreCachedValue(density_);
            return false;
        }
        if (next == density_) {
            syncCachedValue();
            return true;
        }
        density_ = next;
    } else if (path == QStringLiteral("mpmSand.damping")) {
        const float next = value.toFloat(&ok);
        if (!ok || !std::isfinite(next) || next < 0.0f || next > 1.0f) {
            restoreCachedValue(damping_);
            return false;
        }
        if (next == damping_) {
            syncCachedValue();
            return true;
        }
        damping_ = next;
    } else if (path == QStringLiteral("mpmSand.gravityY")) {
        const float next = value.toFloat(&ok);
        if (!ok || !std::isfinite(next) || std::abs(next) > 5000.0f) {
            restoreCachedValue(gravityY_);
            return false;
        }
        if (next == gravityY_) {
            syncCachedValue();
            return true;
        }
        gravityY_ = next;
    } else if (path == QStringLiteral("mpmSand.columns")) {
        const int next = value.toInt(&ok);
        if (!ok || next < 2 || next > 500) {
            restoreCachedValue(columns_);
            return false;
        }
        if (next == columns_) {
            syncCachedValue();
            return true;
        }
        columns_ = next;
    } else if (path == QStringLiteral("mpmSand.rows")) {
        const int next = value.toInt(&ok);
        if (!ok || next < 2 || next > 200) {
            restoreCachedValue(rows_);
            return false;
        }
        if (next == rows_) {
            syncCachedValue();
            return true;
        }
        rows_ = next;
    } else {
        const Size_2D oldSourceSize = sourceSize();
        if (!ArtifactAbstractLayer::setLayerPropertyValue(path, value)) {
            return false;
        }
        if (path != QStringLiteral("source.width") &&
            path != QStringLiteral("source.height")) {
            return true;
        }
        const Size_2D updatedSourceSize = sourceSize();
        if (updatedSourceSize.width == oldSourceSize.width &&
            updatedSourceSize.height == oldSourceSize.height) {
            return true;
        }
        auto* composition =
            dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
        if (composition && !configureSolver()) {
            setSourceSize(oldSourceSize);
            if (path == QStringLiteral("source.width")) {
                restoreCachedValue(oldSourceSize.width);
            } else {
                restoreCachedValue(oldSourceSize.height);
            }
            return false;
        }
        return true;
    }
    auto* composition = dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    if (composition && !configureSolver()) {
        youngModulus_ = oldYoungModulus;
        poissonRatio_ = oldPoissonRatio;
        frictionAngleRadians_ = oldFrictionAngleRadians;
        density_ = oldDensity;
        damping_ = oldDamping;
        gravityY_ = oldGravityY;
        columns_ = oldColumns;
        rows_ = oldRows;
        if (path == QStringLiteral("mpmSand.youngModulus")) {
            restoreCachedValue(youngModulus_);
        } else if (path == QStringLiteral("mpmSand.frictionAngleDegrees")) {
            restoreCachedValue(frictionAngleRadians_ *
                               180.0f / 3.14159265358979323846f);
        } else if (path == QStringLiteral("mpmSand.poissonRatio")) {
            restoreCachedValue(poissonRatio_);
        } else if (path == QStringLiteral("mpmSand.density")) {
            restoreCachedValue(density_);
        } else if (path == QStringLiteral("mpmSand.damping")) {
            restoreCachedValue(damping_);
        } else if (path == QStringLiteral("mpmSand.gravityY")) {
            restoreCachedValue(gravityY_);
        } else if (path == QStringLiteral("mpmSand.columns")) {
            restoreCachedValue(columns_);
        } else if (path == QStringLiteral("mpmSand.rows")) {
            restoreCachedValue(rows_);
        }
        return false;
    }
    syncCachedValue();
    return true;
}

QJsonObject ArtifactMpmSand2DLayer::toJson() const {
    auto object = ArtifactAbstractLayer::toJson();
    object[QStringLiteral("type")] = static_cast<int>(LayerType::MpmSand2D);
    object[QStringLiteral("mpmSandYoungModulus")] = youngModulus_;
    object[QStringLiteral("mpmSandPoissonRatio")] = poissonRatio_;
    object[QStringLiteral("mpmSandFrictionAngleRadians")] = frictionAngleRadians_;
    object[QStringLiteral("mpmSandDensity")] = density_;
    object[QStringLiteral("mpmSandDamping")] = damping_;
    object[QStringLiteral("mpmSandGravityY")] = gravityY_;
    if (const auto property = getProperty(QStringLiteral("mpmSand.gravityY"))) {
        const auto serialized = PropertySerializationBridge::serializeProperty(property);
        object[QStringLiteral("mpmSandGravityAnimatable")] =
            property->isAnimatable();
        if (!serialized.expression.isEmpty()) {
            object[QStringLiteral("mpmSandGravityExpression")] = serialized.expression;
        }
        if (!serialized.keyframes.isEmpty()) {
            object[QStringLiteral("mpmSandGravityKeyframes")] = serialized.keyframes;
        }
        if (!serialized.envelopes.isEmpty()) {
            object[QStringLiteral("mpmSandGravityEnvelopes")] = serialized.envelopes;
        }
    }
    object[QStringLiteral("mpmSandColumns")] = columns_;
    object[QStringLiteral("mpmSandRows")] = rows_;
    return object;
}

void ArtifactMpmSand2DLayer::fromJsonProperties(const QJsonObject& object) {
    ArtifactAbstractLayer::fromJsonProperties(object);
    const auto readFinite = [&object](const QString& key, float fallback) {
        bool ok = false;
        const double value = object.value(key).toDouble(&ok);
        return ok && std::isfinite(value) &&
                std::abs(value) <= std::numeric_limits<float>::max()
            ? static_cast<float>(value) : fallback;
    };
    youngModulus_ = std::clamp(readFinite(QStringLiteral("mpmSandYoungModulus"), youngModulus_),
                               1.0f, 1000000.0f);
    poissonRatio_ = std::clamp(readFinite(QStringLiteral("mpmSandPoissonRatio"), poissonRatio_),
                               -0.9f, 0.45f);
    frictionAngleRadians_ = std::clamp(
        readFinite(QStringLiteral("mpmSandFrictionAngleRadians"), frictionAngleRadians_),
        0.0f,
        80.0f * 3.14159265358979323846f / 180.0f);
    density_ = std::clamp(readFinite(QStringLiteral("mpmSandDensity"), density_),
                          0.01f, 100.0f);
    damping_ = std::clamp(readFinite(QStringLiteral("mpmSandDamping"), damping_),
                          0.0f, 1.0f);
    gravityY_ = std::clamp(readFinite(QStringLiteral("mpmSandGravityY"), gravityY_),
                           -5000.0f, 5000.0f);
    persistentLayerProperty(QStringLiteral("mpmSand.gravityY"),
                            PropertyType::Float, QVariant(gravityY_), 0);
    if (const auto property = getProperty(QStringLiteral("mpmSand.gravityY"))) {
        property->setValue(QVariant(gravityY_));
        const bool hasAnimatedState =
            object.contains(QStringLiteral("mpmSandGravityAnimatable")) ||
            object.contains(QStringLiteral("mpmSandGravityExpression")) ||
            object.contains(QStringLiteral("mpmSandGravityKeyframes")) ||
            object.contains(QStringLiteral("mpmSandGravityEnvelopes"));
        if (hasAnimatedState) {
            SerializedProperty serialized;
            serialized.name = QStringLiteral("mpmSand.gravityY");
            serialized.type = static_cast<int>(PropertyType::Float);
            serialized.value = gravityY_;
            serialized.expression = object.value(
                QStringLiteral("mpmSandGravityExpression")).toString();
            serialized.keyframes = object.value(
                QStringLiteral("mpmSandGravityKeyframes")).toArray();
            serialized.envelopes = object.value(
                QStringLiteral("mpmSandGravityEnvelopes")).toArray();
            AbstractPropertyPtr propertyPtr = property;
            PropertySerializationBridge::deserializeProperty(propertyPtr, serialized);
            property->setAnimatable(
                object.value(QStringLiteral("mpmSandGravityAnimatable")).toBool() ||
                !serialized.keyframes.isEmpty());
            property->setValue(QVariant(gravityY_));
        }
    }
    columns_ = std::clamp(object.value(QStringLiteral("mpmSandColumns")).toInt(columns_), 2, 500);
    rows_ = std::clamp(object.value(QStringLiteral("mpmSandRows")).toInt(rows_), 2, 200);
}

bool ArtifactMpmSand2DLayer::isAdjustmentLayer() const { return false; }
bool ArtifactMpmSand2DLayer::isNullLayer() const { return false; }

}

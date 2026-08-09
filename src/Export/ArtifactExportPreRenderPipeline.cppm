module;
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QIODevice>
#include <QRectF>
#include <QSize>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

module Artifact.Export.PreRenderPipeline;

import Artifact.Render.IRenderer;
import Artifact.Render.OffscreenComposition;
import Artifact.Render.CompositionViewDrawing;
import Frame.Position;

namespace Artifact {

namespace {

class GpuLayerRenderer {
public:
    bool initialize(const QSize& size) {
        if (!size.isValid() || size.width() <= 0 || size.height() <= 0) return false;
        bootstrap_.initializeHeadless(size.width(), size.height());
        if (!bootstrap_.isInitialized()) return false;
        const auto device = bootstrap_.device();
        if (!device) return false;
        renderer_ = std::make_unique<OffscreenCompositionRenderer>(
            device, static_cast<unsigned int>(size.width()),
            static_cast<unsigned int>(size.height()));
        return renderer_ && renderer_->renderer();
    }

    QImage render(ArtifactAbstractLayer* layer, int frame) {
        if (!renderer_ || !layer) return {};
        renderer_->renderLayerFrame(FramePosition(frame), layer);
        return renderer_->renderer()->readbackToImage();
    }

private:
    ArtifactIRenderer bootstrap_;
    std::unique_ptr<OffscreenCompositionRenderer> renderer_;
};

QSize resolveRenderSize(ArtifactAbstractLayer* layer,
                        const QSize& requested,
                        double resolutionScale) {
    if (!layer) return {};
    QSize baseSize;
    if (requested.isValid() && requested.width() > 0 && requested.height() > 0) {
        baseSize = requested;
    } else {
        const auto sourceSize = layer->sourceSize();
        if (sourceSize.width > 0 && sourceSize.height > 0) {
            baseSize = QSize(sourceSize.width, sourceSize.height);
        } else {
            const QRectF bounds = layer->localBounds();
            baseSize = QSize(std::max(1, static_cast<int>(std::ceil(bounds.width()))),
                             std::max(1, static_cast<int>(std::ceil(bounds.height()))));
        }
    }
    const double scale = std::isfinite(resolutionScale)
        ? std::clamp(resolutionScale, 1.0, 4.0)
        : 1.0;
    return QSize(
        std::max(1, static_cast<int>(std::ceil(baseSize.width() * scale))),
        std::max(1, static_cast<int>(std::ceil(baseSize.height() * scale))));
}

QImage tryGpuLayerRender(ArtifactAbstractLayer* layer, const QSize& size, int frame) {
    GpuLayerRenderer renderer;
    if (!renderer.initialize(size)) return {};
    return renderer.render(layer, frame);
}

} // namespace

static bool renderLayerImpl(
    ArtifactAbstractLayer* layer,
    const QString& outputPath,
    const ArtifactExportPreRenderOptions& options,
    QString* errorMessage,
    GpuLayerRenderer* gpuRenderer) {
    if (!layer) {
        if (errorMessage) *errorMessage = QStringLiteral("プリレンダー対象レイヤーがありません。");
        return false;
    }
    if (outputPath.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("プリレンダー出力先が指定されていません。");
        return false;
    }

    const QSize size = resolveRenderSize(layer, options.resolution, options.resolutionScale);
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0) {
        if (errorMessage) *errorMessage = QStringLiteral("レイヤーのプリレンダーサイズを決定できません。");
        return false;
    }

    const int64_t originalFrame = layer->currentFrame();
    const auto restoreFrame = [&]() { layer->goToFrame(originalFrame); };
    layer->goToFrame(options.frame);
    QImage surface = gpuRenderer
        ? gpuRenderer->render(layer, options.frame)
        : tryGpuLayerRender(layer, size, options.frame);
    if (surface.isNull()) {
        surface = layer->getThumbnail(size.width(), size.height());
    }
    if (surface.isNull()) {
        restoreFrame();
        if (errorMessage) *errorMessage = QStringLiteral("レイヤーの画像を生成できませんでした。");
        return false;
    }

    applyRasterizerEffectsAndMasksToSurface(layer, surface, DetailLevel::High);
    const QFileInfo outputInfo(outputPath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        restoreFrame();
        if (errorMessage) *errorMessage = QStringLiteral("プリレンダー出力先を作成できませんでした。");
        return false;
    }
    const bool saved = surface.save(outputPath, "PNG");
    restoreFrame();
    if (!saved) {
        if (errorMessage) *errorMessage = QStringLiteral("プリレンダー画像を保存できませんでした。");
        return false;
    }
    return true;
}

bool ArtifactExportPreRenderPipeline::renderLayer(
    ArtifactAbstractLayer* layer,
    const QString& outputPath,
    const ArtifactExportPreRenderOptions& options,
    QString* errorMessage) {
    return renderLayerImpl(layer, outputPath, options, errorMessage, nullptr);
}

bool ArtifactExportPreRenderPipeline::renderLayerSequence(
    ArtifactAbstractLayer* layer,
    const QString& outputDirectory,
    const QString& fileStem,
    const ArtifactExportPreRenderSequenceOptions& options,
    QVector<QString>* outputPaths,
    QString* errorMessage) {
    if (outputPaths) outputPaths->clear();
    if (!layer) {
        if (errorMessage) *errorMessage = QStringLiteral("プリレンダー対象レイヤーがありません。");
        return false;
    }
    if (outputDirectory.trimmed().isEmpty() || fileStem.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("プリレンダー画像列の出力先が指定されていません。");
        return false;
    }
    const int step = std::max(1, options.frameStep);
    const int start = std::min(options.startFrame, options.endFrame);
    const int end = std::max(options.startFrame, options.endFrame);
    const qint64 frameCount = (static_cast<qint64>(end) - start) / step + 1;
    if (frameCount <= 0 || frameCount > 10000) {
        if (errorMessage) *errorMessage = QStringLiteral("プリレンダー画像列のフレーム数が上限を超えています。");
        return false;
    }

    QDir output(outputDirectory);
    if (!output.exists() && !QDir().mkpath(outputDirectory)) {
        if (errorMessage) *errorMessage = QStringLiteral("プリレンダー画像列の出力先を作成できませんでした。");
        return false;
    }
    const QSize renderSize = resolveRenderSize(layer, options.resolution, options.resolutionScale);
    if (!renderSize.isValid() || renderSize.width() <= 0 || renderSize.height() <= 0) {
        if (errorMessage) *errorMessage = QStringLiteral("レイヤーのプリレンダーサイズを決定できません。");
        return false;
    }
    GpuLayerRenderer gpuRenderer;
    gpuRenderer.initialize(renderSize);
    for (int frame = start; frame <= end; frame += step) {
        const QString path = output.filePath(QStringLiteral("%1_%2.png").arg(fileStem).arg(frame, 6, 10, QLatin1Char('0')));
        ArtifactExportPreRenderOptions frameOptions;
        frameOptions.resolution = renderSize;
        frameOptions.frame = frame;
        if (!renderLayerImpl(layer, path, frameOptions, errorMessage, &gpuRenderer)) return false;
        if (outputPaths) outputPaths->push_back(path);
        if (end - frame < step) break;
    }
    return outputPaths ? !outputPaths->isEmpty() : true;
}

} // namespace Artifact

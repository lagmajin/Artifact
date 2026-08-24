module;
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QWindow>
#include <QMetaObject>
#include <QShowEvent>
#include <QHideEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QPaintEvent>
#include <QtMath>
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
module Artifact.Widgets.ModelViewer;

import Thread.PreciseTicker;
import Utils.String.UniString;
import Color.Float;
import MeshImporter;
import Mesh;
import ArtifactDiligentEngineRenderWindow;
import Memory.SharedPtr;

namespace Artifact {

using namespace ArtifactCore;

class NavHudLabel;

W_OBJECT_IMPL(Artifact3DModelViewer)
W_OBJECT_IMPL(NavHudLabel)

class Artifact3DModelViewer::Impl {
public:
    enum class NavMode { None, Orbit, Pan };

    Artifact3DModelViewer* owner = nullptr;

    ArtifactCore::UniString currentModelPath;
    ArtifactCore::FloatColor backgroundColor{0.1f, 0.1f, 0.1f, 1.0f};

    // Orbit camera model
    QVector3D orbitTarget_{0.0f, 0.0f, 0.0f};
    float orbitYaw_ = 0.0f;
    float orbitPitch_ = 0.0f;
    float orbitDistance_ = 5.0f;

    // Navigation state
    QPointF lastMousePos_;
    NavMode navMode_ = NavMode::None;

    SharedPtr<ArtifactCore::Mesh> currentMesh = nullptr;
    bool modelLoaded = false;
    DisplayMode mode = DisplayMode::Solid;
    ArtifactCore::MeshImporter::Backend lastBackend = ArtifactCore::MeshImporter::Backend::None;
    QString lastErrorText;

    ArtifactDiligentEngineRenderWindow* renderWindow = nullptr;
    QWidget* renderContainer = nullptr;
    QComboBox* modeCombo = nullptr;
    QLabel* statusLabel = nullptr;
    NavHudLabel* navHud_ = nullptr;
    std::unique_ptr<ArtifactCore::PreciseTicker> renderTimer_;
    bool animationPlaybackEnabled_ = false;
    int animationClipIndex_ = 0;
    double animationTime_ = 0.0;
    std::map<QString, float> blendShapeWeightOverrides_;
    std::chrono::steady_clock::time_point animationClock_ =
        std::chrono::steady_clock::now();

    explicit Impl(Artifact3DModelViewer* widget)
        : owner(widget)
    {
    }

    static ArtifactDiligentEngineRenderWindow::ShadingMode toShadingMode(DisplayMode mode)
    {
        switch (mode) {
            case DisplayMode::Wireframe:
                return ArtifactDiligentEngineRenderWindow::ShadingMode::Wireframe;
            case DisplayMode::SolidWithWire:
                return ArtifactDiligentEngineRenderWindow::ShadingMode::SolidWithWire;
            case DisplayMode::Solid:
            default:
                return ArtifactDiligentEngineRenderWindow::ShadingMode::Solid;
        }
    }

    QString backendText() const
    {
        switch (lastBackend) {
            case ArtifactCore::MeshImporter::Backend::Ufbx:
                return QStringLiteral("ufbx");
            case ArtifactCore::MeshImporter::Backend::TinyObj:
                return QStringLiteral("tinyobj");
            case ArtifactCore::MeshImporter::Backend::UfbxGltf:
                return QStringLiteral("glTF via ufbx");
            case ArtifactCore::MeshImporter::Backend::PMD:
                return QStringLiteral("PMD");
            case ArtifactCore::MeshImporter::Backend::Usda:
                return QStringLiteral("USD ASCII");
            case ArtifactCore::MeshImporter::Backend::None:
            default:
                return QStringLiteral("none");
        }
    }

    void updateStatus()
    {
        if (!statusLabel) {
            return;
        }

        if (modelLoaded && currentMesh) {
            const QVector3D minB = currentMesh->boundingBoxMin();
            const QVector3D maxB = currentMesh->boundingBoxMax();
            const QVector3D extents = maxB - minB;
            const auto* activeClip = currentMesh->skinAnimationClip(
                animationClipIndex_);
            const QString clipName = activeClip && !activeClip->name.isEmpty()
                ? activeClip->name
                : QStringLiteral("-");
            const QString skinningMode = currentMesh->skinBones().isEmpty()
                ? QStringLiteral("-")
                : (renderWindow && renderWindow->gpuSkinningActive()
                       ? QStringLiteral("GPU")
                       : QStringLiteral("CPU"));
            const QString influenceMode = currentMesh->skinBones().isEmpty()
                ? QStringLiteral("-")
                : (currentMesh->hasExtendedSkinningWeights()
                       ? QStringLiteral("8+")
                       : QStringLiteral("4"));
            QString sourceSkinningMethod;
            switch (currentMesh->skinningMethod()) {
                case ArtifactCore::Mesh::SkinningMethod::Rigid:
                    sourceSkinningMethod = QStringLiteral("Rigid");
                    break;
                case ArtifactCore::Mesh::SkinningMethod::DualQuaternion:
                    sourceSkinningMethod = QStringLiteral("DualQuaternion");
                    break;
                case ArtifactCore::Mesh::SkinningMethod::BlendedDualQuaternion:
                    sourceSkinningMethod = QStringLiteral("BlendedDQ");
                    break;
                case ArtifactCore::Mesh::SkinningMethod::LinearBlend:
                default:
                    sourceSkinningMethod = QStringLiteral("LBS");
                    break;
            }
            statusLabel->setText(
                QString("Preview: %1 | Vertices: %2 | Polygons: %3 | Bones: %4 | Skinning: %5 (%6, %7inf) | Morphs: %8 | Clips: %9 [#%10: %11] | Animation: %12 | Bounds: %13 x %14 x %15 | Backend: %16")
                    .arg(currentModelPath.toQString())
                    .arg(currentMesh->vertexCount())
                    .arg(currentMesh->polygonCount())
                    .arg(currentMesh->skinBones().size())
                    .arg(skinningMode)
                    .arg(sourceSkinningMethod)
                    .arg(influenceMode)
                    .arg(currentMesh->blendShapes().size())
                    .arg(currentMesh->skinAnimationClips().size())
                    .arg(animationClipIndex_)
                    .arg(clipName)
                    .arg(animationPlaybackEnabled_ ? QStringLiteral("Playing")
                                                    : QStringLiteral("Stopped"))
                    .arg(static_cast<int>(std::round(extents.x())))
                    .arg(static_cast<int>(std::round(extents.y())))
                    .arg(static_cast<int>(std::round(extents.z())))
                    .arg(backendText()));
        } else {
            const QString reason = lastErrorText.isEmpty()
                ? QStringLiteral("3D Model Viewer: no model loaded")
                : QStringLiteral("3D Model Viewer: preview unavailable (%1) [%2]")
                      .arg(lastErrorText)
                      .arg(backendText());
            statusLabel->setText(reason);
        }
    }

    void pushCamera()
    {
        if (!renderWindow) {
            return;
        }
        const float zoomFactor = 5.0f / std::max(0.05f, orbitDistance_);
        renderWindow->setPreviewCamera(zoomFactor, orbitYaw_, orbitPitch_, orbitTarget_);
    }

    void advanceAnimation()
    {
        if (!animationPlaybackEnabled_ || !currentMesh ||
            currentMesh->skinAnimationClips().isEmpty()) {
            animationClock_ = std::chrono::steady_clock::now();
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        const double rawDeltaSeconds =
            std::chrono::duration<double>(now - animationClock_).count();
        animationClock_ = now;
        const auto* clip = currentMesh->skinAnimationClip(animationClipIndex_);
        if (!clip || clip->timeEnd <= clip->timeBegin) return;
        const double deltaSeconds = std::isfinite(rawDeltaSeconds)
            ? std::clamp(rawDeltaSeconds, 0.0, 0.25)
            : 0.0;
        if (!std::isfinite(animationTime_)) animationTime_ = clip->timeBegin;
        animationTime_ += deltaSeconds;
        const double duration = clip->timeEnd - clip->timeBegin;
        animationTime_ = clip->timeBegin +
            std::fmod(std::max(0.0, animationTime_ - clip->timeBegin), duration);
        owner->loadModelAtTime(currentModelPath, animationTime_, animationClipIndex_);
    }
};

Artifact3DModelViewer::Artifact3DModelViewer(QWidget* parent)
    : QWidget(parent)
    , impl_(new Impl(this))
{
    setAccessibleName(QStringLiteral("3D model viewer"));
    setAccessibleDescription(QStringLiteral("Inspect and navigate the active 3D model"));
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(4);

    auto* toolbar = new QWidget(this);
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 6, 8, 0);
    toolbarLayout->setSpacing(8);

    auto* modeLabel = new QLabel("Viewport", toolbar);
    modeLabel->setAccessibleName(QStringLiteral("Viewport shading label"));
    impl_->modeCombo = new QComboBox(toolbar);
    impl_->modeCombo->setAccessibleName(QStringLiteral("Viewport shading mode"));
    impl_->modeCombo->setAccessibleDescription(QStringLiteral("Choose solid, wireframe, or solid with wire shading"));
    impl_->modeCombo->addItem("Solid");
    impl_->modeCombo->addItem("Wireframe");
    impl_->modeCombo->addItem("Solid + Wire");

    toolbarLayout->addWidget(modeLabel);
    toolbarLayout->addWidget(impl_->modeCombo, 0);
    toolbarLayout->addStretch(1);
    rootLayout->addWidget(toolbar);

    impl_->renderWindow = new ArtifactDiligentEngineRenderWindow();
    impl_->renderContainer = QWidget::createWindowContainer(impl_->renderWindow, this);
    impl_->renderContainer->setMinimumSize(400, 280);
    impl_->renderContainer->setFocusPolicy(Qt::StrongFocus);
    impl_->renderContainer->setAccessibleName(QStringLiteral("3D Model View"));
    impl_->renderContainer->setAccessibleDescription(QStringLiteral("Inspect and navigate the active 3D model"));
    impl_->renderContainer->installEventFilter(this);
    rootLayout->addWidget(impl_->renderContainer, 1);

    impl_->navHud_ = new NavHudLabel(this);
    impl_->navHud_->setAccessibleName(QStringLiteral("3D navigation hint"));
    impl_->navHud_->setVisible(false);
    impl_->navHud_->adjustSize();

    impl_->statusLabel = new QLabel(this);
    impl_->statusLabel->setAccessibleName(QStringLiteral("3D model status"));
    impl_->statusLabel->setAccessibleDescription(QStringLiteral("Current model preview, geometry, bounds, and backend status"));
    impl_->statusLabel->setObjectName("Artifact3DViewerStatus");
    rootLayout->addWidget(impl_->statusLabel);

    connect(impl_->modeCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        switch (index) {
            case 1:
                setDisplayMode(DisplayMode::Wireframe);
                break;
            case 2:
                setDisplayMode(DisplayMode::SolidWithWire);
                break;
            case 0:
            default:
                setDisplayMode(DisplayMode::Solid);
                break;
        }
    });

    setDisplayMode(DisplayMode::Solid);
    impl_->updateStatus();

    impl_->renderTimer_ = std::make_unique<ArtifactCore::PreciseTicker>();
    impl_->renderTimer_->setInterval(std::chrono::milliseconds(16));
    impl_->renderTimer_->setCallback([this]() {
        QMetaObject::invokeMethod(this, [this]() {
            impl_->advanceAnimation();
            requestUpdate();
        }, Qt::QueuedConnection);
    });
    // Start only when visible; stopped in hideEvent.
    if (isVisible()) {
        impl_->renderTimer_->start();
    }
}

Artifact3DModelViewer::~Artifact3DModelViewer()
{
    delete impl_;
}

void Artifact3DModelViewer::loadModel(const ArtifactCore::UniString& filePath)
{
    impl_->currentModelPath = filePath;
    impl_->blendShapeWeightOverrides_.clear();

    ArtifactCore::MeshImporter importer;
    impl_->currentMesh = importer.importMeshFromFile(filePath);
    impl_->lastBackend = importer.lastBackend();
    impl_->lastErrorText = importer.lastError();
    impl_->animationTime_ = 0.0;
    impl_->animationClipIndex_ = 0;
    impl_->animationPlaybackEnabled_ = impl_->currentMesh &&
        !impl_->currentMesh->skinAnimationClips().isEmpty();
    impl_->animationClock_ = std::chrono::steady_clock::now();

    if (impl_->currentMesh && impl_->currentMesh->isValid()) {
        impl_->modelLoaded = true;
        qDebug() << "Model loaded successfully. Vertices:" << impl_->currentMesh->vertexCount();
        if (impl_->renderWindow) {
            impl_->renderWindow->setMesh(impl_->currentMesh);
            impl_->renderWindow->setBaseColorTexture(
                importer.lastBaseColorTexture());
            impl_->renderWindow->setMetallicRoughnessTexture(
                importer.lastMetallicRoughnessTexture());
            impl_->renderWindow->setNormalTexture(
                importer.lastNormalTexture());
        }
        resetView();
    } else {
        impl_->modelLoaded = false;
        if (impl_->lastErrorText.isEmpty()) {
            impl_->lastErrorText = QStringLiteral("failed to load model");
        }
        qDebug() << "Failed to load model:" << filePath.toQString() << "-" << impl_->lastErrorText;
        if (impl_->renderWindow) {
            impl_->renderWindow->clearMesh();
            impl_->renderWindow->setBaseColorTexture(QString());
            impl_->renderWindow->setMetallicRoughnessTexture(QString());
            impl_->renderWindow->setNormalTexture(QString());
        }
    }

    impl_->updateStatus();
    requestUpdate();
}

void Artifact3DModelViewer::loadModelAtTime(
    const ArtifactCore::UniString& filePath,
    const double time,
    const int clipIndex)
{
    impl_->currentModelPath = filePath;

    ArtifactCore::MeshImporter importer;
    impl_->currentMesh = importer.importMeshFromFileAtTime(
        filePath, time, clipIndex);
    if (impl_->currentMesh) {
        for (int shapeIndex = 0;
             shapeIndex < impl_->currentMesh->blendShapes().size();
             ++shapeIndex) {
            const auto overrideIt = impl_->blendShapeWeightOverrides_.find(
                impl_->currentMesh->blendShapes()[shapeIndex].name);
            if (overrideIt != impl_->blendShapeWeightOverrides_.end()) {
                impl_->currentMesh->setBlendShapeWeight(
                    shapeIndex, overrideIt->second);
            }
        }
    }
    impl_->lastBackend = importer.lastBackend();
    impl_->lastErrorText = importer.lastError();
    impl_->animationClipIndex_ = impl_->currentMesh &&
        !impl_->currentMesh->skinAnimationClips().isEmpty()
        ? std::clamp(clipIndex, 0,
                     static_cast<int>(impl_->currentMesh->skinAnimationClips().size()) - 1)
        : 0;
    impl_->animationTime_ = std::isfinite(time)
        ? time
        : 0.0;
    if (const auto* clip = impl_->currentMesh
            ? impl_->currentMesh->skinAnimationClip(impl_->animationClipIndex_)
            : nullptr) {
        if (std::isfinite(clip->timeBegin) &&
            std::isfinite(clip->timeEnd) &&
            clip->timeEnd > clip->timeBegin) {
            const double requestedTime = std::isfinite(time)
                ? time
                : clip->timeBegin;
            impl_->animationTime_ = std::clamp(
                requestedTime, clip->timeBegin, clip->timeEnd);
        } else {
            impl_->animationTime_ = 0.0;
        }
    }
    impl_->animationPlaybackEnabled_ = impl_->currentMesh &&
        !impl_->currentMesh->skinAnimationClips().isEmpty();

    if (impl_->currentMesh && impl_->currentMesh->isValid()) {
        impl_->modelLoaded = true;
        if (impl_->renderWindow) {
            impl_->renderWindow->setMesh(impl_->currentMesh);
            impl_->renderWindow->setBaseColorTexture(
                importer.lastBaseColorTexture());
            impl_->renderWindow->setMetallicRoughnessTexture(
                importer.lastMetallicRoughnessTexture());
            impl_->renderWindow->setNormalTexture(
                importer.lastNormalTexture());
        }
    } else {
        impl_->modelLoaded = false;
        if (impl_->lastErrorText.isEmpty()) {
            impl_->lastErrorText = QStringLiteral("failed to evaluate model");
        }
        if (impl_->renderWindow) {
            impl_->renderWindow->clearMesh();
            impl_->renderWindow->setBaseColorTexture(QString());
            impl_->renderWindow->setMetallicRoughnessTexture(QString());
            impl_->renderWindow->setNormalTexture(QString());
        }
    }

    impl_->animationClock_ = std::chrono::steady_clock::now();
    impl_->updateStatus();
    requestUpdate();
}

void Artifact3DModelViewer::setAnimationPlaybackEnabled(const bool enabled)
{
    impl_->animationPlaybackEnabled_ = enabled;
    impl_->animationClock_ = std::chrono::steady_clock::now();
    if (enabled && impl_->currentMesh &&
        !impl_->currentMesh->skinAnimationClips().isEmpty()) {
        const auto* clip = impl_->currentMesh->skinAnimationClip(
            impl_->animationClipIndex_);
        if (clip) {
            impl_->animationTime_ = clip->timeBegin;
            if (!impl_->currentModelPath.isEmpty()) {
                impl_->owner->loadModelAtTime(
                    impl_->currentModelPath, clip->timeBegin,
                    impl_->animationClipIndex_);
                impl_->animationPlaybackEnabled_ = impl_->currentMesh &&
                    impl_->currentMesh->isValid();
                impl_->animationClock_ = std::chrono::steady_clock::now();
            }
        }
    }
    impl_->updateStatus();
    requestUpdate();
}

bool Artifact3DModelViewer::animationPlaybackEnabled() const
{
    return impl_->animationPlaybackEnabled_;
}

void Artifact3DModelViewer::setAnimationClipIndex(const int clipIndex)
{
    if (impl_->currentMesh) {
        const int clipCount = static_cast<int>(
            impl_->currentMesh->skinAnimationClips().size());
        impl_->animationClipIndex_ = clipCount > 0
            ? std::clamp(clipIndex, 0, clipCount - 1)
            : 0;
        if (const auto* clip = impl_->currentMesh->skinAnimationClip(
                impl_->animationClipIndex_)) {
            impl_->animationTime_ = clip->timeBegin;
            const bool wasPlaying = impl_->animationPlaybackEnabled_;
            const auto sourcePath = impl_->currentModelPath;
            if (!sourcePath.isEmpty()) {
                impl_->owner->loadModelAtTime(
                    sourcePath, clip->timeBegin, impl_->animationClipIndex_);
                impl_->animationPlaybackEnabled_ = wasPlaying;
                impl_->animationClock_ = std::chrono::steady_clock::now();
                impl_->updateStatus();
                requestUpdate();
            }
        }
    } else {
        impl_->animationClipIndex_ = std::max(0, clipIndex);
    }
}

int Artifact3DModelViewer::animationClipIndex() const
{
    return impl_->animationClipIndex_;
}

int Artifact3DModelViewer::animationClipCount() const
{
    return impl_->currentMesh
        ? static_cast<int>(impl_->currentMesh->skinAnimationClips().size())
        : 0;
}

QString Artifact3DModelViewer::animationClipName(const int clipIndex) const
{
    if (!impl_->currentMesh) return QString();
    const auto* clip = impl_->currentMesh->skinAnimationClip(clipIndex);
    return clip ? clip->name : QString();
}

int Artifact3DModelViewer::blendShapeCount() const
{
    return impl_->currentMesh
        ? static_cast<int>(impl_->currentMesh->blendShapes().size())
        : 0;
}

QString Artifact3DModelViewer::blendShapeName(const int shapeIndex) const
{
    if (!impl_->currentMesh) return QString();
    const auto& shapes = impl_->currentMesh->blendShapes();
    return shapeIndex >= 0 && shapeIndex < shapes.size()
        ? shapes[shapeIndex].name : QString();
}

float Artifact3DModelViewer::blendShapeWeight(const int shapeIndex) const
{
    return impl_->currentMesh
        ? impl_->currentMesh->blendShapeWeight(shapeIndex)
        : 0.0f;
}

void Artifact3DModelViewer::setBlendShapeWeight(const int shapeIndex,
                                                const float weight)
{
    if (!impl_->currentMesh || shapeIndex < 0 ||
        shapeIndex >= impl_->currentMesh->blendShapes().size() ||
        !std::isfinite(weight)) return;
    impl_->blendShapeWeightOverrides_[
        impl_->currentMesh->blendShapes()[shapeIndex].name] = weight;
    impl_->currentMesh->setBlendShapeWeight(shapeIndex, weight);
    if (impl_->renderWindow) {
        impl_->renderWindow->refreshMeshGeometry();
    }
    impl_->updateStatus();
    requestUpdate();
}

void Artifact3DModelViewer::clearBlendShapeWeightOverride(
    const int shapeIndex)
{
    if (!impl_->currentMesh || shapeIndex < 0 ||
        shapeIndex >= impl_->currentMesh->blendShapes().size()) {
        return;
    }
    impl_->blendShapeWeightOverrides_.erase(
        impl_->currentMesh->blendShapes()[shapeIndex].name);
    if (!impl_->currentModelPath.isEmpty()) {
        loadModelAtTime(impl_->currentModelPath, impl_->animationTime_,
                        impl_->animationClipIndex_);
    } else {
        impl_->updateStatus();
        requestUpdate();
    }
}

void Artifact3DModelViewer::clearModel()
{
    impl_->currentModelPath = ArtifactCore::UniString();
    impl_->modelLoaded = false;
    impl_->currentMesh = nullptr;
    impl_->animationTime_ = 0.0;
    impl_->animationClipIndex_ = 0;
    impl_->animationPlaybackEnabled_ = false;
    impl_->blendShapeWeightOverrides_.clear();
    impl_->animationClock_ = std::chrono::steady_clock::now();
    impl_->lastBackend = ArtifactCore::MeshImporter::Backend::None;
    impl_->lastErrorText.clear();
    if (impl_->renderWindow) {
        impl_->renderWindow->clearMesh();
        impl_->renderWindow->setBaseColorTexture(QString());
        impl_->renderWindow->setMetallicRoughnessTexture(QString());
        impl_->renderWindow->setNormalTexture(QString());
    }
    impl_->updateStatus();
    requestUpdate();
}

void Artifact3DModelViewer::setSkinPoseMatrices(
    const QVector<QMatrix4x4>& boneMatrices)
{
    if (!impl_->currentMesh || !impl_->renderWindow) {
        return;
    }
    impl_->renderWindow->setSkinPoseMatrices(boneMatrices);
    impl_->updateStatus();
    requestUpdate();
}

void Artifact3DModelViewer::resetView()
{
    impl_->orbitTarget_ = QVector3D(0.0f, 0.0f, 0.0f);
    impl_->orbitYaw_ = 0.0f;
    impl_->orbitPitch_ = 0.0f;
    impl_->orbitDistance_ = 5.0f;

    if (impl_->currentMesh && impl_->currentMesh->isValid()) {
        const QVector3D minBounds = impl_->currentMesh->boundingBoxMin();
        const QVector3D maxBounds = impl_->currentMesh->boundingBoxMax();
        const QVector3D center = (minBounds + maxBounds) * 0.5f;
        const QVector3D extents = maxBounds - minBounds;
        const float radius = 0.5f * extents.length();

        // Keep a comfortable margin while remaining usable for very small
        // imported assets. The viewer's zoom mapping is distance-based.
        impl_->orbitTarget_ = center;
        impl_->orbitDistance_ = std::clamp(radius * 2.8f, 0.5f, 100000.0f);
    }

    impl_->pushCamera();
    requestUpdate();
}

void Artifact3DModelViewer::setBackgroundColor(const ArtifactCore::FloatColor& color)
{
    impl_->backgroundColor = color;
    if (impl_->renderWindow) {
        impl_->renderWindow->setClearColor(
            QColor::fromRgbF(color.r(), color.g(), color.b(), color.a()));
    }
}

void Artifact3DModelViewer::setZoom(float factor)
{
    impl_->orbitDistance_ = 5.0f / std::max(0.05f, factor);
    impl_->pushCamera();
    requestUpdate();
}

void Artifact3DModelViewer::setCameraRotation(float yaw, float pitch)
{
    impl_->orbitYaw_ = yaw;
    impl_->orbitPitch_ = pitch;
    impl_->pushCamera();
    requestUpdate();
}

void Artifact3DModelViewer::setCameraPosition(const QVector3D& position)
{
    impl_->orbitTarget_ = position;
    impl_->pushCamera();
    requestUpdate();
}

void Artifact3DModelViewer::setDisplayMode(DisplayMode mode)
{
    if (impl_->mode == mode) {
        return;
    }
    impl_->mode = mode;
    if (impl_->renderWindow) {
        impl_->renderWindow->setShadingMode(Impl::toShadingMode(mode));
    }

    if (impl_->modeCombo) {
        const int desiredIndex = (mode == DisplayMode::Solid)
            ? 0
            : (mode == DisplayMode::Wireframe ? 1 : 2);
        if (impl_->modeCombo->currentIndex() != desiredIndex) {
            impl_->modeCombo->setCurrentIndex(desiredIndex);
        }
    }
    Q_EMIT displayModeChanged(static_cast<int>(mode));
}

void Artifact3DModelViewer::setPbrMaterial(
    const QColor& baseColor,
    float metallic,
    float roughness,
    float sheen,
    float clearcoat,
    float clearcoatRoughness,
    float transmission,
    float specular,
    float ior)
{
    if (impl_->renderWindow) {
        impl_->renderWindow->setPbrMaterial(
            baseColor, metallic, roughness, sheen, clearcoat,
            clearcoatRoughness, transmission, specular, ior);
    }
}

Artifact3DModelViewer::DisplayMode Artifact3DModelViewer::displayMode() const
{
    return impl_->mode;
}

bool Artifact3DModelViewer::hasModel() const
{
    return impl_ && impl_->modelLoaded && impl_->currentMesh;
}

int Artifact3DModelViewer::vertexCount() const
{
    return hasModel() ? impl_->currentMesh->vertexCount() : 0;
}

int Artifact3DModelViewer::polygonCount() const
{
    return hasModel() ? impl_->currentMesh->polygonCount() : 0;
}

QString Artifact3DModelViewer::backendName() const
{
    if (!impl_) {
        return QStringLiteral("none");
    }
    switch (impl_->lastBackend) {
        case ArtifactCore::MeshImporter::Backend::Ufbx:
            return QStringLiteral("ufbx");
        case ArtifactCore::MeshImporter::Backend::TinyObj:
            return QStringLiteral("tinyobj");
        case ArtifactCore::MeshImporter::Backend::UfbxGltf:
            return QStringLiteral("glTF via ufbx");
        case ArtifactCore::MeshImporter::Backend::PMD:
            return QStringLiteral("PMD");
        case ArtifactCore::MeshImporter::Backend::Usda:
            return QStringLiteral("USD ASCII");
        case ArtifactCore::MeshImporter::Backend::None:
        default:
            return QStringLiteral("none");
    }
}

QString Artifact3DModelViewer::lastErrorText() const
{
    return impl_ ? impl_->lastErrorText : QString();
}

QVector3D Artifact3DModelViewer::meshExtents() const
{
    if (!hasModel()) {
        return QVector3D(0.0f, 0.0f, 0.0f);
    }
    const QVector3D minB = impl_->currentMesh->boundingBoxMin();
    const QVector3D maxB = impl_->currentMesh->boundingBoxMax();
    return maxB - minB;
}

float Artifact3DModelViewer::zoomFactor() const
{
    return impl_ ? (5.0f / std::max(0.05f, impl_->orbitDistance_)) : 1.0f;
}

float Artifact3DModelViewer::cameraYaw() const
{
    return impl_ ? impl_->orbitYaw_ : 0.0f;
}

float Artifact3DModelViewer::cameraPitch() const
{
    return impl_ ? impl_->orbitPitch_ : 0.0f;
}

QVector3D Artifact3DModelViewer::cameraPosition() const
{
    return impl_ ? impl_->orbitTarget_ : QVector3D(0.0f, 0.0f, 0.0f);
}

void Artifact3DModelViewer::requestUpdate()
{
    if (impl_->renderWindow) {
        impl_->renderWindow->requestRender();
    }
}

bool Artifact3DModelViewer::eventFilter(QObject* obj, QEvent* event)
{
    if (obj != impl_->renderContainer) {
        return QWidget::eventFilter(obj, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if ((me->modifiers() & Qt::AltModifier) && me->button() == Qt::LeftButton) {
            impl_->navMode_ = Impl::NavMode::Orbit;
            impl_->lastMousePos_ = me->position();
            impl_->navHud_->setText(QStringLiteral("Orbit"));
            impl_->navHud_->adjustSize();
            impl_->navHud_->move(10, impl_->renderContainer->height() - impl_->navHud_->height() - 10);
            impl_->navHud_->setVisible(true);
            return true;
        }
        if (me->button() == Qt::MiddleButton) {
            impl_->navMode_ = Impl::NavMode::Pan;
            impl_->lastMousePos_ = me->position();
            setCursor(Qt::ClosedHandCursor);
            impl_->navHud_->setText(QStringLiteral("Pan"));
            impl_->navHud_->adjustSize();
            impl_->navHud_->move(10, impl_->renderContainer->height() - impl_->navHud_->height() - 10);
            impl_->navHud_->setVisible(true);
            return true;
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        if (impl_->navMode_ != Impl::NavMode::None) {
            impl_->navMode_ = Impl::NavMode::None;
            unsetCursor();
            impl_->navHud_->setVisible(false);
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (impl_->navMode_ == Impl::NavMode::Orbit) {
            const QPointF delta = me->position() - impl_->lastMousePos_;
            impl_->orbitYaw_ += delta.x() * 0.5f;
            impl_->orbitPitch_ = std::clamp(impl_->orbitPitch_ + static_cast<float>(delta.y()) * 0.5f, -89.0f, 89.0f);
            impl_->lastMousePos_ = me->position();
            impl_->pushCamera();
            return true;
        }
        if (impl_->navMode_ == Impl::NavMode::Pan) {
            const QPointF delta = me->position() - impl_->lastMousePos_;
            const float panSpeed = impl_->orbitDistance_ * 0.002f;
            const float yawRadians = qDegreesToRadians(impl_->orbitYaw_);
            const float pitchRadians = qDegreesToRadians(impl_->orbitPitch_);
            const float sinYaw = std::sin(yawRadians);
            const float cosYaw = std::cos(yawRadians);
            const float sinPitch = std::sin(pitchRadians);
            const float cosPitch = std::cos(pitchRadians);
            const QVector3D cameraRight(cosYaw, 0.0f, sinYaw);
            const QVector3D cameraUp(
                sinYaw * sinPitch,
                cosPitch,
                -cosYaw * sinPitch);
            impl_->orbitTarget_ +=
                cameraRight * static_cast<float>(-delta.x() * panSpeed) +
                cameraUp * static_cast<float>(delta.y() * panSpeed);
            impl_->lastMousePos_ = me->position();
            impl_->pushCamera();
            return true;
        }
        break;
    }
    case QEvent::Wheel: {
        auto* we = static_cast<QWheelEvent*>(event);
        const float delta = we->angleDelta().y();
        const float factor = (delta > 0) ? 0.9f : 1.1f;
        impl_->orbitDistance_ = std::clamp(impl_->orbitDistance_ * factor, 0.1f, 100.0f);
        impl_->pushCamera();
        return true;
    }
    default:
        break;
    }
    return QWidget::eventFilter(obj, event);
}

void Artifact3DModelViewer::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (impl_->renderTimer_ && !impl_->renderTimer_->isRunning()) {
        impl_->renderTimer_->start();
    }
}

void Artifact3DModelViewer::hideEvent(QHideEvent* event)
{
    if (impl_->renderTimer_) {
        impl_->renderTimer_->stop();
    }
    QWidget::hideEvent(event);
}

} // namespace Artifact

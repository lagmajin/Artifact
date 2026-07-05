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

    std::shared_ptr<ArtifactCore::Mesh> currentMesh = nullptr;
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
            statusLabel->setText(
                QString("Preview: %1 | Vertices: %2 | Polygons: %3 | Bounds: %4 x %5 x %6 | Backend: %7")
                    .arg(currentModelPath.toQString())
                    .arg(currentMesh->vertexCount())
                    .arg(currentMesh->polygonCount())
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
};

Artifact3DModelViewer::Artifact3DModelViewer(QWidget* parent)
    : QWidget(parent)
    , impl_(new Impl(this))
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(4);

    auto* toolbar = new QWidget(this);
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 6, 8, 0);
    toolbarLayout->setSpacing(8);

    auto* modeLabel = new QLabel("Viewport", toolbar);
    impl_->modeCombo = new QComboBox(toolbar);
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
    impl_->renderContainer->installEventFilter(this);
    rootLayout->addWidget(impl_->renderContainer, 1);

    impl_->navHud_ = new NavHudLabel(this);
    impl_->navHud_->setVisible(false);
    impl_->navHud_->adjustSize();

    impl_->statusLabel = new QLabel(this);
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
        QMetaObject::invokeMethod(this, [this]() { requestUpdate(); }, Qt::QueuedConnection);
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

    ArtifactCore::MeshImporter importer;
    impl_->currentMesh = importer.importMeshFromFile(filePath);
    impl_->lastBackend = importer.lastBackend();
    impl_->lastErrorText = importer.lastError();

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

void Artifact3DModelViewer::clearModel()
{
    impl_->currentModelPath = ArtifactCore::UniString();
    impl_->modelLoaded = false;
    impl_->currentMesh = nullptr;
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

void Artifact3DModelViewer::resetView()
{
    impl_->orbitTarget_ = QVector3D(0.0f, 0.0f, 0.0f);
    impl_->orbitYaw_ = 0.0f;
    impl_->orbitPitch_ = 0.0f;
    impl_->orbitDistance_ = 5.0f;
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
    float roughness)
{
    if (impl_->renderWindow) {
        impl_->renderWindow->setPbrMaterial(
            baseColor, metallic, roughness);
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

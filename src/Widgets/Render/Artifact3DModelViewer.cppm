module;
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QWindow>
#include <QTimer>
#include <QDebug>
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

import Utils.String.UniString;
import Color.Float;
import MeshImporter;
import Mesh;
import ArtifactDiligentEngineRenderWindow;

namespace Artifact {

using namespace ArtifactCore;

W_OBJECT_IMPL(Artifact3DModelViewer)

class Artifact3DModelViewer::Impl {
public:
    Artifact3DModelViewer* owner = nullptr;

    ArtifactCore::UniString currentModelPath;
    ArtifactCore::FloatColor backgroundColor{0.1f, 0.1f, 0.1f, 1.0f};
    float zoomFactor = 1.0f;
    float cameraYaw = 0.0f;
    float cameraPitch = 0.0f;
    QVector3D cameraPosition{0.0f, 0.0f, 5.0f};

    std::shared_ptr<ArtifactCore::Mesh> currentMesh = nullptr;
    bool modelLoaded = false;
    DisplayMode mode = DisplayMode::Solid;
    ArtifactCore::MeshImporter::Backend lastBackend = ArtifactCore::MeshImporter::Backend::None;
    QString lastErrorText;

    ArtifactDiligentEngineRenderWindow* renderWindow = nullptr;
    QWidget* renderContainer = nullptr;
    QComboBox* modeCombo = nullptr;
    QLabel* statusLabel = nullptr;

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

    void updateStatus()
    {
        if (!statusLabel) {
            return;
        }

        if (modelLoaded && currentMesh) {
            QString backendText = QStringLiteral("unknown");
            switch (lastBackend) {
                case ArtifactCore::MeshImporter::Backend::Ufbx:
                    backendText = QStringLiteral("ufbx");
                    break;
                case ArtifactCore::MeshImporter::Backend::TinyObj:
                    backendText = QStringLiteral("tinyobj");
                    break;
                case ArtifactCore::MeshImporter::Backend::None:
                default:
                    backendText = QStringLiteral("none");
                    break;
            }
            statusLabel->setText(
                QString("Model: %1 | Vertices: %2 | Polygons: %3 | Backend: %4")
                    .arg(currentModelPath.toQString())
                    .arg(currentMesh->vertexCount())
                    .arg(currentMesh->polygonCount())
                    .arg(backendText));
        } else {
            const QString reason = lastErrorText.isEmpty()
                ? QStringLiteral("3D Model Viewer: no model loaded")
                : QStringLiteral("3D Model Viewer: %1").arg(lastErrorText);
            statusLabel->setText(reason);
        }
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
    rootLayout->addWidget(impl_->renderContainer, 1);

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

    auto* renderTimer = new QTimer(this);
    connect(renderTimer, &QTimer::timeout, this, &Artifact3DModelViewer::requestUpdate);
    renderTimer->start(16);
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
    }
    impl_->updateStatus();
    requestUpdate();
}

void Artifact3DModelViewer::resetView()
{
    impl_->zoomFactor = 1.0f;
    impl_->cameraYaw = 0.0f;
    impl_->cameraPitch = 0.0f;
    impl_->cameraPosition = QVector3D(0.0f, 0.0f, 5.0f);
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
    impl_->zoomFactor = std::max(0.05f, factor);
    impl_->cameraPosition.setZ(5.0f / impl_->zoomFactor);
    requestUpdate();
}

void Artifact3DModelViewer::setCameraRotation(float yaw, float pitch)
{
    impl_->cameraYaw = yaw;
    impl_->cameraPitch = pitch;
    requestUpdate();
}

void Artifact3DModelViewer::setCameraPosition(const QVector3D& position)
{
    impl_->cameraPosition = position;
    requestUpdate();
}

void Artifact3DModelViewer::setDisplayMode(DisplayMode mode)
{
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
}

Artifact3DModelViewer::DisplayMode Artifact3DModelViewer::displayMode() const
{
    return impl_->mode;
}

float Artifact3DModelViewer::zoomFactor() const
{
    return impl_ ? impl_->zoomFactor : 1.0f;
}

float Artifact3DModelViewer::cameraYaw() const
{
    return impl_ ? impl_->cameraYaw : 0.0f;
}

float Artifact3DModelViewer::cameraPitch() const
{
    return impl_ ? impl_->cameraPitch : 0.0f;
}

QVector3D Artifact3DModelViewer::cameraPosition() const
{
    return impl_ ? impl_->cameraPosition : QVector3D(0.0f, 0.0f, 5.0f);
}

void Artifact3DModelViewer::requestUpdate()
{
    if (impl_->renderWindow) {
        impl_->renderWindow->requestRender();
    }
}

} // namespace Artifact

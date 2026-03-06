module;
#include <QWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <QVector3D>
#include <QVector2D>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QPainter>
#include <wobjectimpl.h>

module Artifact.Widgets.ModelViewer;

import std;
import Utils.String.UniString;
import Color.Float;
import Image.ImageF32x4RGBAWithCache;
import MeshImporter;
import Mesh;

namespace Artifact {

 using namespace ArtifactCore;

 W_OBJECT_IMPL(Artifact3DModelViewer)

  /**
   * @brief 3D Model Viewer Impl
   */
  class Artifact3DModelViewer::Impl {
 public:
  ArtifactCore::UniString currentModelPath;
  ArtifactCore::FloatColor backgroundColor{ 0.1f, 0.1f, 0.1f, 1.0f };
  float zoomFactor = 1.0f;
  float cameraYaw = 0.0f;
  float cameraPitch = 0.0f;
  QVector3D cameraPosition{0.0f, 0.0f, 5.0f};

  // GPU
  ImageF32x4RGBAWithCache* renderTarget = nullptr;

  // Assimp Mesh Data
  std::shared_ptr<ArtifactCore::Mesh> currentMesh = nullptr;

  bool modelLoaded = false;
  bool needsRedraw = true;
  QPoint lastMousePos;

  Impl() {
    renderTarget = new ImageF32x4RGBAWithCache();
  }
  ~Impl() {
    delete renderTarget;
  }

  void loadModelAsync(const ArtifactCore::UniString& path) {
    currentModelPath = path;
    
    // Load model using Assimp-based MeshImporter
    ArtifactCore::MeshImporter importer;
    currentMesh = importer.importMeshFromFile(path);
    
    if (currentMesh && currentMesh->isValid()) {
        modelLoaded = true;
        qDebug() << "Model loaded successfully. Vertices:" << currentMesh->vertexCount();
    } else {
        modelLoaded = false;
        qDebug() << "Failed to load model:" << path.toQString();
    }
    
    needsRedraw = true;
  }

  void updateCamera() {
    QMatrix4x4 viewMatrix;
    viewMatrix.translate(cameraPosition);
    viewMatrix.rotate(cameraYaw, QVector3D(0, 1, 0));
    viewMatrix.rotate(cameraPitch, QVector3D(1, 0, 0));
  }

  void render() {
    if (!needsRedraw) return;

    // TODO: 3D Render
    // - Clear Background
    // - Draw Mesh
    // - Update GPU Texture

    renderTarget->UpdateGpuTextureFromCpuData();
    needsRedraw = false;
  }
 };

 Artifact3DModelViewer::Artifact3DModelViewer(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  setAttribute(Qt::WA_OpaquePaintEvent);
  setAttribute(Qt::WA_NoSystemBackground);
  setMinimumSize(400, 300);

  auto* renderTimer = new QTimer(this);
  connect(renderTimer, &QTimer::timeout, this, &Artifact3DModelViewer::requestUpdate);
  renderTimer->start(16); // ~60fps
 }

 Artifact3DModelViewer::~Artifact3DModelViewer()
 {
  delete impl_;
 }

 void Artifact3DModelViewer::loadModel(const ArtifactCore::UniString& filePath)
 {
  impl_->loadModelAsync(filePath);
  requestUpdate();
 }

 void Artifact3DModelViewer::clearModel()
 {
  impl_->currentModelPath = ArtifactCore::UniString();
  impl_->modelLoaded = false;
  impl_->currentMesh = nullptr;
  impl_->needsRedraw = true;
  requestUpdate();
 }

 void Artifact3DModelViewer::resetView()
 {
  impl_->zoomFactor = 1.0f;
  impl_->cameraYaw = 0.0f;
  impl_->cameraPitch = 0.0f;
  impl_->cameraPosition = QVector3D(0.0f, 0.0f, 5.0f);
  impl_->updateCamera();
  impl_->needsRedraw = true;
  requestUpdate();
 }

 void Artifact3DModelViewer::setBackgroundColor(const ArtifactCore::FloatColor& color)
 {
  impl_->backgroundColor = color;
  impl_->needsRedraw = true;
  requestUpdate();
 }

 void Artifact3DModelViewer::setZoom(float factor)
 {
  impl_->zoomFactor = factor;
  impl_->cameraPosition.setZ(5.0f / factor);
  impl_->updateCamera();
  impl_->needsRedraw = true;
  requestUpdate();
 }

 void Artifact3DModelViewer::setCameraRotation(float yaw, float pitch)
 {
  impl_->cameraYaw = yaw;
  impl_->cameraPitch = pitch;
  impl_->updateCamera();
  impl_->needsRedraw = true;
  requestUpdate();
 }

 void Artifact3DModelViewer::setCameraPosition(const QVector3D& position)
 {
  impl_->cameraPosition = position;
  impl_->updateCamera();
  impl_->needsRedraw = true;
  requestUpdate();
 }

 void Artifact3DModelViewer::requestUpdate()
 {
  impl_->render();
  update(); // QWidget::update()
 }

 void Artifact3DModelViewer::mousePressEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::LeftButton) {
    impl_->lastMousePos = event->pos();
  }
 }

 void Artifact3DModelViewer::mouseMoveEvent(QMouseEvent* event)
 {
  if (event->buttons() & Qt::LeftButton) {
    QPoint delta = event->pos() - impl_->lastMousePos;
    impl_->cameraYaw += delta.x() * 0.5f;
    impl_->cameraPitch += delta.y() * 0.5f;
    impl_->updateCamera();
    impl_->needsRedraw = true;
    impl_->lastMousePos = event->pos();
  }
 }

 void Artifact3DModelViewer::wheelEvent(QWheelEvent* event)
 {
  float zoomDelta = event->angleDelta().y() > 0 ? 1.1f : 0.9f;
  setZoom(impl_->zoomFactor * zoomDelta);
 }

 void Artifact3DModelViewer::paintEvent(QPaintEvent* event)
 {
  QPainter painter(this);
  
  painter.fillRect(rect(), QColor(impl_->backgroundColor.r() * 255,
                                 impl_->backgroundColor.g() * 255,
                                 impl_->backgroundColor.b() * 255));
  painter.setPen(Qt::white);
  
  if (impl_->modelLoaded && impl_->currentMesh) {
      QString stats = QString("3D Model Loaded Successfully\n\nFile: %1\nVertices: %2\nPolygons: %3")
                      .arg(impl_->currentModelPath.toQString())
                      .arg(impl_->currentMesh->vertexCount())
                      .arg(impl_->currentMesh->polygonCount());
      painter.drawText(rect(), Qt::AlignCenter, stats);
  } else {
      painter.drawText(rect(), Qt::AlignCenter, "3D Model Viewer\nDrop model file here");
  }
 }

}
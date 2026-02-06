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
//import Graphics.GPUTexture;

namespace Artifact {

 using namespace ArtifactCore;

 W_OBJECT_IMPL(Artifact3DModelViewer)

  /**
   * @brief 内部実装クラス
   */
  class Artifact3DModelViewer::Impl {
 public:
  ArtifactCore::UniString currentModelPath;
  ArtifactCore::FloatColor backgroundColor{ 0.1f, 0.1f, 0.1f, 1.0f };
  float zoomFactor = 1.0f;
  float cameraYaw = 0.0f;
  float cameraPitch = 0.0f;
  QVector3D cameraPosition{0.0f, 0.0f, 5.0f};

  // GPU関連
  ImageF32x4RGBAWithCache* renderTarget = nullptr;
  // GPUTexture* modelTexture = nullptr; // TODO: 後で実装

  // モデルデータ（仮）
  std::vector<QVector3D> vertices;
  std::vector<QVector3D> normals;
  std::vector<QVector2D> uvs;

  bool modelLoaded = false;
  bool needsRedraw = true;
  QPoint lastMousePos;

  Impl() {
    renderTarget = new ImageF32x4RGBAWithCache();
    // modelTexture = new GPUTexture(); // TODO: 後で実装
  }
  ~Impl() {
    delete renderTarget;
    // delete modelTexture; // TODO: 後で実装
  }

  void loadModelAsync(const ArtifactCore::UniString& path) {
    // TODO: 非同期モデルロードの実装
    // 実際にはQThreadやQtConcurrentを使ってOBJ/FBXファイルをロード
    currentModelPath = path;
    modelLoaded = true;
    needsRedraw = true;
  }

  void updateCamera() {
    // カメラ行列の更新
    QMatrix4x4 viewMatrix;
    viewMatrix.translate(cameraPosition);
    viewMatrix.rotate(cameraYaw, QVector3D(0, 1, 0));
    viewMatrix.rotate(cameraPitch, QVector3D(1, 0, 0));
    // TODO: 実際のレンダラーに適用
  }

  void render() {
    if (!needsRedraw) return;

    // TODO: 実際の3Dレンダリング
    // - 背景クリア
    // - モデル描画
    // - GPUテクスチャ更新

    renderTarget->UpdateGpuTextureFromCpuData();
    needsRedraw = false;
  }
 };

 Artifact3DModelViewer::Artifact3DModelViewer(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  // UI初期化
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  // Qt初期設定
  setAttribute(Qt::WA_OpaquePaintEvent);
  setAttribute(Qt::WA_NoSystemBackground);
  setMinimumSize(400, 300);

  // 定期レンダリングタイマー
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
  impl_->vertices.clear();
  impl_->normals.clear();
  impl_->uvs.clear();
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

 // マウスイベントでカメラ操作
 void Artifact3DModelViewer::mousePressEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::LeftButton) {
    // カメラ回転開始
    impl_->lastMousePos = event->pos();
  }
 }

 void Artifact3DModelViewer::mouseMoveEvent(QMouseEvent* event)
 {
  if (event->buttons() & Qt::LeftButton) {
    // カメラ回転
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
  // TODO: ImageF32x4RGBAWithCacheからQImageへの変換を実装
  // if (impl_->renderTarget && impl_->renderTarget->width() > 0) {
  //   QImage img = impl_->renderTarget->toCVMat(); // 変換メソッドを実装
  //   painter.drawImage(rect(), img);
  // } else {
  
  // 現在はプレースホルダーのみ描画
  painter.fillRect(rect(), QColor(impl_->backgroundColor.r() * 255,
                                 impl_->backgroundColor.g() * 255,
                                 impl_->backgroundColor.b() * 255));
  painter.setPen(Qt::white);
  painter.drawText(rect(), Qt::AlignCenter, "3D Model Viewer\nDrop model file here");
  // }
 }

}

module;

#include <QWidget>

#include <wobjectdefs.h>
export module Artifact.Widgets.Render.Layer;

import Core.Scale.Zoom;
import Color.Float;

export namespace Artifact
{
 using namespace ArtifactCore;
	
 enum class EditMode
 {
  View,           // 表示専用（ズーム・パン）
  Transform,      // トランスフォーム編集
  Mask,           // マスク編集
  Paint           // ペイント（任意）
 };

 enum class DisplayMode
 {
  Color,          // 通常カラー
  Alpha,          // アルファ表示
  Mask,           // マスクオーバーレイ表示
  Wireframe       // ガイド・境界線
 };


 class ArtifactRenderLayerWidget :public QWidget
 {
  W_OBJECT(ArtifactRenderLayerWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
 public:
  explicit ArtifactRenderLayerWidget(QWidget* parent = nullptr);
  ~ArtifactRenderLayerWidget();
  void setClearColor(const FloatColor& color);
  void setEditMode(EditMode mode);
  void setDisplayMode(DisplayMode mode);
  void setTargetLayerId(int id);

  void resetView();
  float zoom() const;
  void setZoom(const ZoomScale2D& scale);
  QPointF pan() const;
  void setPan(const QPointF& offset);
  void setTargetLayer();
  QImage grabScreenShot();
  void ChangeRenderAPI();
 };



}

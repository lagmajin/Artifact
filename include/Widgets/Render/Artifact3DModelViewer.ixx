module;
#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Widgets.ModelViewer;

import Utils.String.UniString;
import Color.Float;

export namespace Artifact {

 /**
  * @brief 3Dモデルを表示・操作するためのウィジェットクラス
  */
 class Artifact3DModelViewer : public QWidget {
  W_OBJECT(Artifact3DModelViewer)
 private:
  class Impl;
  Impl* impl_;

 public:
  explicit Artifact3DModelViewer(QWidget* parent = nullptr);
  virtual ~Artifact3DModelViewer();

  // --- モデル操作 API ---
  /**
   * @brief モデルファイルをロードします
   * @param filePath モデルへのパス (obj, fbx 等)
   */
  void loadModel(const ArtifactCore::UniString& filePath);

  /**
   * @brief モデルをクリアします
   */
  void clearModel();

  // --- ビュー・表示設定 ---
  /**
   * @brief カメラのリセットを行います
   */
  void resetView();

  /**
   * @brief 背景色を設定します
   */
  void setBackgroundColor(const ArtifactCore::FloatColor& color);

  /**
   * @brief ズーム倍率を設定します
   */
  void setZoom(float factor);

  /**
   * @brief カメラの回転を設定します
   */
  void setCameraRotation(float yaw, float pitch);

  /**
   * @brief カメラの位置を設定します
   */
  void setCameraPosition(const QVector3D& position);

  // --- レンダリング制御 ---
  /**
   * @brief フレームの更新を要求します
   */
  void requestUpdate();
 };

}

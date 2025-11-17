module;
export module Tool;

export namespace Artifact {
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

}
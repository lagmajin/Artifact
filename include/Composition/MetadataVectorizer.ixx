module;

#include <QString>
#include <QVector>
#include <vector>

export module Composition.MetadataVectorizer;

import std;
import Artifact.Composition.Abstract;

export namespace Artifact {

 /// Compositionのメタデータをベクトル化するクラス
 /// AIモデルに入力するためにCompositionのメタデータを数値ベクトルに変換
 class CompositionMetadataVectorizer {
 private:
  class Impl;
  Impl* impl_;

 public:
  explicit CompositionMetadataVectorizer();
  ~CompositionMetadataVectorizer();

  /// Compositionのメタデータをベクトル化
  /// @param composition 対象のComposition
  /// @return メタデータから抽出した数値ベクトル（正規化済み）
  std::vector<float> vectorizeMetadata(const ArtifactAbstractComposition* composition) const;

  /// メタデータをベクトル化し、指定されたサイズに正規化
  /// @param composition 対象のComposition
  /// @param targetSize 出力ベクトルのサイズ
  /// @return 正規化されたサイズのベクトル
  std::vector<float> vectorizeMetadataWithSize(const ArtifactAbstractComposition* composition, int targetSize) const;

  /// 各メタデータ要素を個別に抽出
  /// @param composition 対象のComposition
  /// @return メタデータの各成分を含むベクトル
  struct MetadataComponents {
   float duration = 0.0f;        // 長さ（フレーム数の正規化値）
   float frameRate = 0.0f;        // フレームレート
   float width = 0.0f;            // 解像度幅（正規化値）
   float height = 0.0f;           // 解像度高さ（正規化値）
   float layerCount = 0.0f;       // レイヤー数（正規化値）
   float keyframeCount = 0.0f;    // キーフレーム総数（正規化値）
   float complexity = 0.0f;       // 複雑度スコア（0-1）
  };

  /// メタデータコンポーネントを取得
  /// @param composition 対象のComposition
  /// @return メタデータの各成分
  MetadataComponents extractMetadataComponents(const ArtifactAbstractComposition* composition) const;

  /// 最大ベクトルサイズを設定
  /// @param size 最大サイズ
  void setMaxVectorSize(int size);

  /// 現在の最大ベクトルサイズを取得
  /// @return 最大ベクトルサイズ
  int getMaxVectorSize() const;

 Q_SIGNALS:
  void vectorizationComplete(const std::vector<float>& vector);
 };

}

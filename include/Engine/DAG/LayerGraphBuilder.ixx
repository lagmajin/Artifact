module;
#include <vector>
#include <QString>
export module Artifact.Engine.DAG.LayerGraphBuilder;




import Utils.Id;
import Utils.String.UniString;
import Artifact.Engine.DAG.Graph;
import Artifact.Effect.Abstract;
import Memory.SharedPtr;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    //  LayerGraphBuilder  –  UIスタック → DAG 変換ブリッジ
    //
    //   仕様書 4.1:
    //     フロントエンド (UI) ではユーザーに分かりやすい
    //     「固定ラック (スタック) 形式」で表示し、
    //     バックエンドではDAGとして最適化実行する。
    //
    //   この Builder はレイヤーに追加されたエフェクトを
    //   パイプラインステージ順に並べ替え、
    //   自動的にノード間をチェーン接続して EffectGraph を構築する。
    // ─────────────────────────────────────────────────────────

    class LayerGraphBuilder {
    public:
        LayerGraphBuilder() = default;
        ~LayerGraphBuilder() = default;

        /// レイヤーのエフェクトリストから EffectGraph を構築する。
        ///
        /// - effects は UI 上での追加順（ラック内の並び順）で渡される
        /// - ビルダーが EffectPipelineStage 順にソートして DAG を組み立てる
        /// - 同一ステージ内のエフェクトはチェーン（直列）接続される
        /// - ステージ間は自動で入出力をワイヤリングする
        static EffectGraph build(
            const UniString& layerName,
            const std::vector<ArtifactAbstractEffectPtr>& effects);
    };

}

module;
#include <vector>
#include <unordered_map>
#include <memory>

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>

#include <QString>
#include <QList>
export module Artifact.Engine.DAG.CompositionGraphBuilder;




import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Engine.DAG.Graph;
import Artifact.Engine.DAG.Node;
import Artifact.Engine.DAG.Connection;

export namespace Artifact {

    // ─────────────────────────────────────────────────────────
    //  CompositionGraphBuilder
    //
    //  コンポジション全体のレイヤー、トランスフォーム、エフェクトの
    //  依存関係を解析し、単一の巨大な有向非巡回グラフ (DAG) を構築する。
    //  これにより、DAGExecutor を用いた全自動の並列評価が可能になる。
    // ─────────────────────────────────────────────────────────

    class CompositionGraphBuilder {
    public:
        // コンポジションから評価・レンダリング用グラフを構築
        static std::shared_ptr<EffectGraph> build(ArtifactAbstractComposition* comp);
    };

}

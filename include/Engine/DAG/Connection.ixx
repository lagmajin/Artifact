module;
#include <QString>

export module Artifact.Engine.DAG.Connection;

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



import Utils.Id;
import Artifact.Engine.DAG.Port;

export namespace Artifact {

    using namespace ArtifactCore;

    // ─────────────────────────────────────────────────────────
    //  ConnectionID  –  接続を一意に識別する ID
    // ─────────────────────────────────────────────────────────
    class ConnectionID : public Id {
    public:
        using Id::Id;
    };

    // ─────────────────────────────────────────────────────────
    //  Connection  –  2つのノード間の接続（エッジ）
    //
    //   sourceNode の outputPort → targetNode の inputPort
    //   への1本の接続を表す。
    //   仕様書 4.1 の「レイヤーAの Rasterizer 出力を
    //   レイヤーBの Generator 入力に接続」のような
    //   高度なクロスレイヤールーティングもこの構造で表現する。
    // ─────────────────────────────────────────────────────────

    struct Connection {
        ConnectionID    id;

        // Source (出力側)
        Id              sourceNodeId;
        int             sourcePortIndex = 0;

        // Target (入力側)
        Id              targetNodeId;
        int             targetPortIndex = 0;

        // メタデータ
        bool            enabled = true;

        Connection() = default;
        Connection(const ConnectionID& cid,
                   const Id& srcNode, int srcPort,
                   const Id& tgtNode, int tgtPort)
            : id(cid)
            , sourceNodeId(srcNode), sourcePortIndex(srcPort)
            , targetNodeId(tgtNode), targetPortIndex(tgtPort) {}
    };

}

module;
#include <QString>
#include <QPointF>

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
export module Artifact.Effect.LayerTransform.Transform2D;




import Artifact.Effect.Abstract;
import Utils.String.UniString;

export namespace Artifact {

    using namespace ArtifactCore;

    class LayerTransform2D : public ArtifactAbstractEffect {
    private:
        QPointF position_ = QPointF(0.0, 0.0);
        QPointF anchorPoint_ = QPointF(0.0, 0.0);
        float scale_ = 100.0f;
        float rotation_ = 0.0f;
        float opacity_ = 100.0f;

    public:
        LayerTransform2D() {
            setDisplayName(ArtifactCore::UniString("Layer Transform 2D"));
            setPipelineStage(EffectPipelineStage::LayerTransform);
        }
        virtual ~LayerTransform2D() = default;

        QPointF position() const { return position_; }
        void setPosition(const QPointF& pos) { position_ = pos; }

        QPointF anchorPoint() const { return anchorPoint_; }
        void setAnchorPoint(const QPointF& pos) { anchorPoint_ = pos; }

        float scale() const { return scale_; }
        void setScale(float s) { scale_ = s; }

        float rotation() const { return rotation_; }
        void setRotation(float r) { rotation_ = r; }

        float opacity() const { return opacity_; }
        void setOpacity(float o) { opacity_ = o; }
    };

}

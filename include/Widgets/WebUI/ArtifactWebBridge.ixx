module;
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
#include <wobjectdefs.h>
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
export module Artifact.Widgets.WebBridge;





import Utils.Id;
import Utils.String.UniString;
import Event.Bus;

export namespace Artifact {

    using namespace ArtifactCore;

    // ===================================================================
    // ArtifactWebBridge
    // -------------------------------------------------------------------
    // C++ <-> JavaScript 双方向通信ブリッジ
    //
    // QWebChannel に登録されることで、Webページ側のJSから
    //   bridge.setProperty("Angle", 45.0)
    //   bridge.selectLayer("xxxx-xxxx-xxxx")
    // などの呼び出しが可能になり、逆にC++側から
    //   emit propertyChanged(...)
    // で JS 側に通知を送れます。
    // ===================================================================
    class ArtifactWebBridge : public QObject {
        W_OBJECT(ArtifactWebBridge)
    private:
        ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
        std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

    public:
        explicit ArtifactWebBridge(QObject* parent = nullptr);
        ~ArtifactWebBridge();

        // -------- JS -> C++ (Invokable Methods) --------

        // レイヤー選択
        void selectLayer(const QString& layerId);
        W_INVOKABLE(selectLayer)

        // エフェクトプロパティの変更
        void setEffectProperty(const QString& effectId, const QString& propertyName, const QString& jsonValue);
        W_INVOKABLE(setEffectProperty)

        // プロジェクト情報の取得（JSON文字列で返す）
        QString getProjectInfo();
        W_INVOKABLE(getProjectInfo)

        // 現在選択中のレイヤーのプロパティ一覧を取得（JSON文字列で返す）
        QString getSelectedLayerProperties();
        W_INVOKABLE(getSelectedLayerProperties)

        // -------- C++ -> JS (Signals) --------

        // レイヤーが選択された時にJS側へ通知
        void layerSelectionChanged(const QString& layerId)
            W_SIGNAL(layerSelectionChanged, layerId);

        // プロパティが変更された時にJS側へ通知
        void propertyUpdated(const QString& effectId, const QString& propertyName, const QString& jsonValue)
            W_SIGNAL(propertyUpdated, effectId, propertyName, jsonValue);

        // パイプラインの状態が変わった時にJS側へ通知
        void pipelineStateChanged(const QString& stateJson)
            W_SIGNAL(pipelineStateChanged, stateJson);
    };

}

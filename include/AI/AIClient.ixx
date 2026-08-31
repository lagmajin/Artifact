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
#include <QObject>
#include <QString>
export module AI.Client;


import std;
import Utils.String.UniString;
import Core.AI.CloudAgent;

export namespace Artifact {

using namespace ArtifactCore;

/**
 * @brief AI Client for communication with LLM backends.
 * Refactored to be asynchronous using internal events.
 */
class AIClient : public QObject {
private:
    class Impl;
    Impl* impl_;
public:
    AIClient();
    ~AIClient();

    // singleton
    static AIClient* instance();

    // configuration
    void setApiKey(const UniString& key);
    void setProvider(const UniString& provider);
    // クラウドAIエージェントの設定
    void setCloudAgent(ArtifactCore::ICloudAIAgentPtr agent);
    // クラウドAIのAPIキーを設定
    void setCloudApiKey(const UniString& provider, const UniString& apiKey);

    // 初期化（メニューから明示的に呼び出す）
    bool initialize(const QString& modelPath = QString());
    bool isInitialized() const;
    bool isInitializing() const;
    void shutdown();

    // Send a message to AI synchronously (legacy/wait)
    UniString sendMessage(const UniString& message);

    // Send a message asynchronously
    void postMessage(const UniString& message);

    // Cancel the currently running message generation
    void cancelMessage();

};

}

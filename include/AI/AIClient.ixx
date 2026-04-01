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
#include <QObject>
#include <QString>
#include <wobjectdefs.h>

export module AI.Client;

import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

/**
 * @brief AI Client for communication with LLM backends.
 * Refactored to be asynchronous using Qt signals.
 */
class AIClient : public QObject {
    W_OBJECT(AIClient)
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

    // 初期化（メニューから明示的に呼び出す）
    bool initialize(const QString& modelPath = QString());
    bool isInitialized() const;
    void shutdown();

    // Send a message to AI synchronously (legacy/wait)
    UniString sendMessage(const UniString& message);

    // Send a message asynchronously
    void postMessage(const UniString& message);

    // Signals
    void messageReceived(QString message) W_SIGNAL(messageReceived, message);
    void partialMessageReceived(QString partialText) W_SIGNAL(partialMessageReceived, partialText);
    void errorOccurred(QString error) W_SIGNAL(errorOccurred, error);
};

}

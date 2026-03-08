
module;
#include <QString>
#include <thread>
#include <chrono>
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
#include <wobjectimpl.h>

module AI.Client;

import Utils.String.UniString;
import Core.AI.PromptGenerator;

namespace Artifact {

W_OBJECT_IMPL(AIClient)

class AIClient::Impl {
public:
    UniString apiKey;
    UniString provider = UniString("dummy");
};

AIClient::AIClient(): impl_(new Impl()) {}
AIClient::~AIClient(){ delete impl_; }

AIClient* AIClient::instance() {
    static AIClient inst;
    return &inst;
}

void AIClient::setApiKey(const UniString& key) { impl_->apiKey = key; }
void AIClient::setProvider(const UniString& provider) { impl_->provider = provider; }

UniString AIClient::sendMessage(const UniString& message) {
    // Generate system prompt to show we are using the metadata system
    QString systemPrompt = AIPromptGenerator::generateSystemPrompt(DescriptionLanguage::Japanese);
    std::cout << "AI System Prompt Length: " << systemPrompt.length() << std::endl;

    // Dummy synchronous response
    QString q = message.toQString();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    QString resp = QString("[AI-DUMMY] ") + q;
    return UniString(resp);
}

void AIClient::postMessage(const UniString& message) {
    // Simulation logic: Stream a response back
    std::thread([this, message]() {
        QString input = message.toQString();
        QString fullResponse = QString("[AI-STREAM] Processing: ") + input;
        
        // Simulate thinking
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        // Simulate streaming tokens
        QString current;
        QStringList words = fullResponse.split(" ");
        for (const auto& word : words) {
            current += word + " ";
            Q_EMIT this->partialMessageReceived(current);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        Q_EMIT this->messageReceived(current);
    }).detach();
}

}

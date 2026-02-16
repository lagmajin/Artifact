module;
#include <QString>
#include <thread>
#include <chrono>

module AI.Client;

import std;
import Utils.String.UniString;

namespace Artifact {

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
    // Dummy synchronous response: echo with prefix after a short delay
    QString q = message.toQString();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    QString resp = QString("[AI-DUMMY] ") + q;
    return UniString(resp);
}

}

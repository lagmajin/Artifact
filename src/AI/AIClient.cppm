module;
#include <QString>
#include <wobjectimpl.h>

module AI.Client;

import std;
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
    QString systemPrompt = AIPromptGenerator::generateSystemPrompt(DescriptionLanguage::Japanese);
    std::cout << "AI System Prompt Length: " << systemPrompt.length() << std::endl;

    const QString q = message.toQString();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    return UniString(QString("[AI-DUMMY] ") + q);
}

void AIClient::postMessage(const UniString& message) {
    std::thread([this, message]() {
        const QString input = message.toQString();
        const QString fullResponse = QString("[AI-STREAM] Processing: ") + input;

        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        QString current;
        const QStringList words = fullResponse.split(" ");
        for (const auto& word : words) {
            current += word + " ";
            Q_EMIT this->partialMessageReceived(current);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        Q_EMIT this->messageReceived(current);
    }).detach();
}

}

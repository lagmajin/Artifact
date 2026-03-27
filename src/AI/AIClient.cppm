module;
#include <QString>
#include <QDebug>
#include <wobjectimpl.h>

module AI.Client;

import std;
import Utils.String.UniString;
import Core.AI.PromptGenerator;
import Core.AI.LocalAgent;
import Core.AI.LlamaAgent;

namespace Artifact {

W_OBJECT_IMPL(AIClient)

class AIClient::Impl {
public:
    UniString apiKey;
    UniString provider = UniString("local");  // デフォルトはローカル

    // ローカル AI エージェント
    LocalAIAgentPtr localAgent;

    bool initialized = false;
};

AIClient::AIClient(): impl_(new Impl()) {
    // ローカル AI を初期化
    impl_->localAgent = std::make_shared<LlamaLocalAgent>();

    // モデルパスは設定から取得、またはデフォルト
    QString modelPath = "models/llama-3.2-1b-instruct.q4_k_m.gguf";

    // 初期化を試みる（失敗してもフォールバックするだけ）
    if (impl_->localAgent->initialize(modelPath)) {
        impl_->initialized = true;
        qDebug() << "[AIClient] Local AI initialized successfully";
    } else {
        qWarning() << "[AIClient] Failed to initialize local AI, using fallback responses";
        impl_->initialized = false;
    }
}

AIClient::~AIClient(){ delete impl_; }

AIClient* AIClient::instance() {
    static AIClient inst;
    return &inst;
}

void AIClient::setApiKey(const UniString& key) { impl_->apiKey = key; }

void AIClient::setProvider(const UniString& provider) {
    impl_->provider = provider;
    
    if (provider == UniString("local") || provider == UniString("llama")) {
        // ローカル AI に切り替え
        if (!impl_->initialized) {
            QString modelPath = "models/llama-3.2-1b-instruct.q4_k_m.gguf";
            impl_->initialized = impl_->localAgent->initialize(modelPath);
        }
    }
}

UniString AIClient::sendMessage(const UniString& message) {
    QString systemPrompt = AIPromptGenerator::generateSystemPrompt(DescriptionLanguage::Japanese);
    std::cout << "AI System Prompt Length: " << systemPrompt.length() << std::endl;

    if (impl_->initialized && impl_->localAgent) {
        // ローカル AI で処理
        AIContext context;
        context.setUserPrompt(message.toQString());
        context.setSystemPrompt(systemPrompt);
        
        auto analysis = impl_->localAgent->analyzeUserQuestion(message.toQString(), context);
        
        if (!analysis.requiresCloud) {
            return UniString(analysis.localAnswer);
        } else {
            // クラウドフォールバック（未実装）
            return UniString("[CLOUD] クラウド AI が必要です（未実装）\n\n要約：" + analysis.summarizedContext);
        }
    } else {
        // フォールバック
        const QString q = message.toQString();
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return UniString(QString("[AI-DUMMY] ") + q);
    }
}

void AIClient::postMessage(const UniString& message) {
    std::thread([this, message]() {
        const QString input = message.toQString();
        
        if (impl_->initialized && impl_->localAgent) {
            // ローカル AI で分析
            AIContext context;
            context.setUserPrompt(input);
            context.setSystemPrompt(AIPromptGenerator::generateSystemPrompt(DescriptionLanguage::Japanese));
            
            auto analysis = impl_->localAgent->analyzeUserQuestion(input, context);
            
            if (!analysis.requiresCloud) {
                // ローカルで回答
                QString response = analysis.localAnswer;
                
                // ストリーミング表示
                QString current;
                const QStringList words = response.split(" ");
                for (const auto& word : words) {
                    current += word + " ";
                    Q_EMIT this->partialMessageReceived(current);
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));
                }
                
                Q_EMIT this->messageReceived(response);
            } else {
                // クラウドにフォールバック
                Q_EMIT this->partialMessageReceived("複雑な質問ですね。クラウド AI に確認しています...");
                
                // クラウド AI 実装は別途
                QString cloudResponse = QString("[CLOUD] クラウド AI が必要です\n\n要約された状況：\n%1\n\n※ クラウド AI 機能は現在開発中です")
                    .arg(analysis.summarizedContext);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                Q_EMIT this->messageReceived(cloudResponse);
            }
        } else {
            // フォールバック
            const QString fullResponse = QString("[AI-STREAM] Processing: ") + input;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            
            QString current;
            const QStringList words = fullResponse.split(" ");
            for (const auto& word : words) {
                current += word + " ";
                Q_EMIT this->partialMessageReceived(current);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            Q_EMIT this->messageReceived(fullResponse);
        }
    }).detach();
}

}

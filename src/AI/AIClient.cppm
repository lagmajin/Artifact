module;
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QMetaObject>
#include <QSettings>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
#include <cstdint>
#include <atomic>
#include <mutex>
#include <thread>
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
    UniString provider = UniString("local");
    LocalAIAgentPtr localAgent;
    bool initialized = false;
    bool initializing = false;
    std::uint64_t generation = 0;
    QString modelPath;
    mutable std::mutex mutex;
    std::atomic<bool> cancellationRequested{false};
};

AIClient::AIClient() : impl_(new Impl()) {
    impl_->localAgent = std::make_shared<LlamaLocalAgent>();
}

AIClient::~AIClient() { delete impl_; }

AIClient* AIClient::instance() {
    static AIClient inst;
    return &inst;
}

void AIClient::setApiKey(const UniString& key) { impl_->apiKey = key; }

void AIClient::setProvider(const UniString& provider) {
    impl_->provider = provider;
}

namespace {

void lowerCurrentThreadPriority()
{
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
}

void updateRecentModelPaths(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    QSettings settings;
    QStringList recentPaths = settings.value(QStringLiteral("AI/RecentModelPaths")).toStringList();
    recentPaths.removeAll(trimmed);
    recentPaths.prepend(trimmed);
    while (recentPaths.size() > 10) {
        recentPaths.removeLast();
    }
    settings.setValue(QStringLiteral("AI/RecentModelPaths"), recentPaths);
    settings.setValue(QStringLiteral("AI/ModelPath"), trimmed);
}

} // namespace

bool AIClient::initialize(const QString& modelPath) {
    lowerCurrentThreadPriority();
    QString path = modelPath.trimmed();
    if (path.isEmpty()) {
        path = QStringLiteral("models/llama-3.2-1b-instruct.q4_k_m.gguf");
    }
    LocalAIAgentPtr localAgent;
    std::uint64_t loadGeneration = 0;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->initialized) {
            return true;
        }
        if (impl_->initializing) {
            qInfo() << "[AIClient] Local llama.cpp backend is already initializing:" << path;
            return false;
        }

        if (!impl_->localAgent) {
            impl_->localAgent = std::make_shared<LlamaLocalAgent>();
        }

        impl_->initializing = true;
        impl_->initialized = false;
        impl_->modelPath = path;
        localAgent = impl_->localAgent;
        loadGeneration = ++impl_->generation;
    }

    updateRecentModelPaths(path);
    const bool loaded = localAgent && localAgent->initialize(path);
    bool acceptResult = false;

    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->generation == loadGeneration) {
            acceptResult = true;
            impl_->initializing = false;
            impl_->initialized = loaded;
            if (!loaded) {
                impl_->modelPath.clear();
            }
        }
    }

    if (!acceptResult) {
        qInfo() << "[AIClient] Ignored stale initialization result:" << path;
        return loaded;
    }
    if (loaded) {
        qInfo() << "[AIClient] Local llama.cpp backend initialized:" << path;
    } else {
        QString errorText = QStringLiteral("Failed to initialize local llama.cpp backend.");
        if (const auto* llamaAgent = dynamic_cast<const LlamaLocalAgent*>(localAgent.get())) {
            const QString detail = llamaAgent->lastError();
            if (!detail.isEmpty()) {
                errorText = detail;
            }
        }
        qWarning() << "[AIClient]" << errorText << path;
        Q_EMIT this->errorOccurred(errorText);
    }
    Q_EMIT this->initializationFinished(loaded, path);
    return loaded;
}

bool AIClient::isInitialized() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->initialized;
}

bool AIClient::isInitializing() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->initializing;
}

void AIClient::shutdown() {
    QString previousModelPath;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        ++impl_->generation;
        impl_->cancellationRequested.store(true, std::memory_order_relaxed);
        impl_->initializing = false;
        impl_->initialized = false;
        previousModelPath = impl_->modelPath;
        impl_->localAgent.reset();
        impl_->localAgent = std::make_shared<LlamaLocalAgent>();
        impl_->modelPath.clear();
    }
    qDebug() << "[AIClient] Local AI shut down";
    Q_EMIT this->initializationFinished(false, previousModelPath);
}

UniString AIClient::sendMessage(const UniString& message) {
    const QString systemPrompt = AIPromptGenerator::generateSystemPrompt(DescriptionLanguage::Japanese);
    const QString userPrompt = message.toQString();

    LocalAIAgentPtr localAgent;
    bool initialized = false;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        initialized = impl_->initialized;
        localAgent = impl_->localAgent;
    }

    if (initialized && localAgent) {
        AIContext context;
        context.setUserPrompt(userPrompt);
        context.setSystemPrompt(systemPrompt);

        const QString response = localAgent->generateChatResponse(systemPrompt, userPrompt, context);
        if (!response.isEmpty()) {
            return UniString(response);
        }

        return UniString(QStringLiteral("申し訳ありません。もう少し詳しく教えていただけますか？"));
    }

    return UniString(QStringLiteral("[AI unavailable] ") + userPrompt);
}

void AIClient::cancelMessage() {
    impl_->cancellationRequested.store(true, std::memory_order_relaxed);
}

void AIClient::postMessage(const UniString& message) {
    impl_->cancellationRequested.store(false, std::memory_order_relaxed);
    std::thread([this, message]() {
        lowerCurrentThreadPriority();
        const QString userPrompt = message.toQString();

        LocalAIAgentPtr localAgent;
        bool initialized = false;
        bool initializing = false;
        std::uint64_t currentGeneration = 0;
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            initialized = impl_->initialized;
            initializing = impl_->initializing;
            localAgent = impl_->localAgent;
            currentGeneration = impl_->generation;
        }

        if (initializing && !initialized) {
            Q_EMIT this->errorOccurred(QStringLiteral("Local model is still loading."));
            Q_EMIT this->partialMessageReceived(QStringLiteral("[AI loading]"));
            return;
        }

        if (initialized && localAgent) {
            AIContext context;
            context.setUserPrompt(userPrompt);
            context.setSystemPrompt(AIPromptGenerator::generateSystemPrompt(DescriptionLanguage::Japanese));

            QString accumulated;
            bool wasCancelled = false;
            const QString response = localAgent->generateChatResponseStreaming(
                context.systemPrompt(),
                userPrompt,
                context,
                [this, &accumulated, &wasCancelled, currentGeneration](const QString& piece) -> bool {
                    if (impl_->cancellationRequested.load(std::memory_order_relaxed)) {
                        wasCancelled = true;
                        return false;
                    }
                    accumulated += piece;
                    const QString snapshot = accumulated;
                    QMetaObject::invokeMethod(this, [this, snapshot]() {
                        Q_EMIT this->partialMessageReceived(snapshot);
                    }, Qt::QueuedConnection);
                    std::lock_guard<std::mutex> lock(impl_->mutex);
                    return impl_->generation == currentGeneration;
                });

            if (wasCancelled) {
                QMetaObject::invokeMethod(this, [this]() {
                    Q_EMIT this->messageCancelled();
                }, Qt::QueuedConnection);
                return;
            }

            if (response.isEmpty()) {
                QMetaObject::invokeMethod(this, [this]() {
                    Q_EMIT this->errorOccurred(QStringLiteral("AI response was empty."));
                    Q_EMIT this->messageReceived(QStringLiteral("申し訳ありません。応答を生成できませんでした。"));
                }, Qt::QueuedConnection);
                return;
            }

            QMetaObject::invokeMethod(this, [this, response]() {
                Q_EMIT this->messageReceived(response);
            }, Qt::QueuedConnection);
            return;
        }

        const QString fullResponse = QStringLiteral("[AI unavailable] ") + userPrompt;
        QMetaObject::invokeMethod(this, [this, fullResponse]() {
            Q_EMIT this->partialMessageReceived(fullResponse);
            Q_EMIT this->messageReceived(fullResponse);
        }, Qt::QueuedConnection);
    }).detach();
}

} // namespace Artifact

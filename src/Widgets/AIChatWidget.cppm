module;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QListWidgetItem>
#include <QAbstractItemView>
#include <QVector>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QFileDialog>
#include <QSettings>
#include <QStringList>
#include <QFileInfo>
#include <QDateTime>
#include <QLocale>
#include <QTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <wobjectimpl.h>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

module Widgets.AIChatWidget;

import Utils.String.UniString;
import AI.Client;

namespace Artifact {

W_OBJECT_IMPL(AIChatWidget)

namespace {

void lowerCurrentThreadPriority()
{
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
}

struct ChatMessage {
    QString role;
    QString text;
};

struct ChatSession {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString title = QStringLiteral("New Session");
    QDateTime updatedAt = QDateTime::currentDateTime();
    QVector<ChatMessage> messages;
};

QString displayTitleForSession(const ChatSession& session)
{
    if (session.title.trimmed().isEmpty()) {
        return QStringLiteral("New Session");
    }
    return session.title;
}

QString makeSessionTitleFromUserText(const QString& text)
{
    QString title = text.trimmed().simplified();
    title.replace('\n', ' ');
    if (title.size() > 32) {
        title = title.left(32).trimmed() + QStringLiteral("...");
    }
    return title.isEmpty() ? QStringLiteral("Chat") : title;
}

QString roleLabel(const QString& role)
{
    if (role == QStringLiteral("user")) {
        return QStringLiteral("You");
    }
    if (role == QStringLiteral("assistant")) {
        return QStringLiteral("AI");
    }
    return QStringLiteral("System");
}

QString htmlForMessage(const ChatMessage& message)
{
    const QString escaped = message.text.toHtmlEscaped().replace('\n', QStringLiteral("<br/>"));
    if (message.role == QStringLiteral("assistant")) {
        return QStringLiteral("<b>AI:</b> %1").arg(escaped);
    }
    if (message.role == QStringLiteral("user")) {
        return QStringLiteral("<b>You:</b> %1").arg(escaped);
    }
    return QStringLiteral("<i>%1</i>").arg(escaped);
}

} // namespace

class AIChatWidget::Impl {
public:
    QSplitter* splitter = nullptr;
    QListWidget* sessionList = nullptr;
    QPushButton* newSessionButton = nullptr;
    QTextEdit* history = nullptr;
    QLineEdit* input = nullptr;
    QPushButton* send = nullptr;
    QPushButton* cancel = nullptr;

    // Model loading toolbar
    QComboBox* providerCombo = nullptr;
    QLineEdit* modelPathEdit = nullptr;
    QPushButton* browseButton = nullptr;
    QPushButton* loadButton = nullptr;
    QPushButton* unloadButton = nullptr;
    QLabel* modelStatusLabel = nullptr;

    QVector<ChatSession> sessions;
    int currentSessionIndex = -1;
    int streamingMessageIndex = -1;
    bool isStreaming = false;
    bool isInitializing = false;
    bool streamingUiUpdateScheduled = false;
    QString pendingMessage;
    QString currentReply;

    QString resolveModelPath() const;
    void appendSystemLine(const QString& text);
    void dispatchMessage(const QString& txt, AIClient* client);
    void loadSessions();
    void saveSessions() const;
    void refreshSessionList();
    void renderCurrentSession();
    ChatSession* currentSession();
    int ensureSession();
    void selectSession(int index);
    void createNewSession();
    void appendMessage(const QString& role, const QString& text);
    void updateCurrentAssistantMessage(const QString& text, bool persist = true);
    void updateModelStatus(AIClient* client);
    void updateModelPathPlaceholder();
    void setSendingState(bool sending);
};

QString AIChatWidget::Impl::resolveModelPath() const
{
    QSettings settings;
    QString modelPath = settings.value(QStringLiteral("AI/ModelPath")).toString().trimmed();
    if (!modelPath.isEmpty()) {
        return modelPath;
    }

    const QStringList recentPaths =
        settings.value(QStringLiteral("AI/RecentModelPaths")).toStringList();
    for (const auto& path : recentPaths) {
        const QString trimmed = path.trimmed();
        if (!trimmed.isEmpty()) {
            return trimmed;
        }
    }
    return {};
}

void AIChatWidget::Impl::appendSystemLine(const QString& text)
{
    appendMessage(QStringLiteral("system"), text);
}

void AIChatWidget::Impl::updateModelStatus(AIClient* client)
{
    if (!modelStatusLabel || !loadButton || !unloadButton) {
        return;
    }
    if (client->isInitializing()) {
        modelStatusLabel->setText(QStringLiteral("Loading…"));
        loadButton->setEnabled(false);
        unloadButton->setEnabled(false);
        if (browseButton) {
            browseButton->setEnabled(false);
        }
    } else if (client->isInitialized()) {
        modelStatusLabel->setText(QStringLiteral("✓ Loaded"));
        loadButton->setEnabled(false);
        unloadButton->setEnabled(true);
        if (browseButton) {
            browseButton->setEnabled(true);
        }
    } else {
        modelStatusLabel->setText(QStringLiteral("Not loaded"));
        loadButton->setEnabled(true);
        unloadButton->setEnabled(false);
        if (browseButton) {
            browseButton->setEnabled(true);
        }
    }
}

void AIChatWidget::Impl::updateModelPathPlaceholder()
{
    if (!modelPathEdit || !providerCombo) {
        return;
    }
    const QString provider = providerCombo->currentData().toString().trimmed().toLower();
    if (provider == QStringLiteral("onnx-dml") || provider == QStringLiteral("onnx")) {
        modelPathEdit->setPlaceholderText(QStringLiteral("Path to .onnx model or model directory…"));
    } else {
        modelPathEdit->setPlaceholderText(QStringLiteral("Path to .gguf model…"));
    }
}

void AIChatWidget::Impl::setSendingState(bool sending)
{
    if (send) {
        send->setVisible(!sending);
        send->setEnabled(!sending);
    }
    if (unloadButton) {
        unloadButton->setEnabled(!sending && !isInitializing && AIClient::instance()->isInitialized());
    }
    if (cancel) {
        cancel->setVisible(sending);
    }
    if (input) {
        input->setEnabled(!sending);
    }
}

void AIChatWidget::Impl::loadSessions()
{
    sessions.clear();

    QSettings settings;
    const QByteArray raw = settings.value(QStringLiteral("AI/ChatSessionsV2")).toByteArray();
    if (!raw.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (doc.isArray()) {
            const QJsonArray array = doc.array();
            for (const auto& value : array) {
                if (!value.isObject()) {
                    continue;
                }
                const QJsonObject obj = value.toObject();
                ChatSession session;
                session.id = obj.value(QStringLiteral("id")).toString(session.id);
                session.title = obj.value(QStringLiteral("title")).toString(session.title);
                session.updatedAt = QDateTime::fromString(
                    obj.value(QStringLiteral("updatedAt")).toString(),
                    Qt::ISODate);
                if (!session.updatedAt.isValid()) {
                    session.updatedAt = QDateTime::currentDateTime();
                }

                const QJsonArray messages = obj.value(QStringLiteral("messages")).toArray();
                for (const auto& msgValue : messages) {
                    if (!msgValue.isObject()) {
                        continue;
                    }
                    const QJsonObject msgObj = msgValue.toObject();
                    ChatMessage message;
                    message.role = msgObj.value(QStringLiteral("role")).toString();
                    message.text = msgObj.value(QStringLiteral("text")).toString();
                    if (!message.role.isEmpty() || !message.text.isEmpty()) {
                        session.messages.push_back(message);
                    }
                }
                sessions.push_back(session);
            }
        }
    }

    if (sessions.isEmpty()) {
        sessions.push_back(ChatSession{});
    }

    const QString currentId = settings.value(QStringLiteral("AI/ChatCurrentSessionId")).toString().trimmed();
    currentSessionIndex = 0;
    for (int i = 0; i < sessions.size(); ++i) {
        if (sessions[i].id == currentId) {
            currentSessionIndex = i;
            break;
        }
    }
}

void AIChatWidget::Impl::saveSessions() const
{
    QJsonArray array;
    for (const auto& session : sessions) {
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), session.id);
        obj.insert(QStringLiteral("title"), session.title);
        obj.insert(QStringLiteral("updatedAt"), session.updatedAt.toString(Qt::ISODate));
        QJsonArray messages;
        for (const auto& message : session.messages) {
            QJsonObject msgObj;
            msgObj.insert(QStringLiteral("role"), message.role);
            msgObj.insert(QStringLiteral("text"), message.text);
            messages.append(msgObj);
        }
        obj.insert(QStringLiteral("messages"), messages);
        array.append(obj);
    }

    QSettings settings;
    const QJsonDocument doc(array);
    settings.setValue(QStringLiteral("AI/ChatSessionsV2"), doc.toJson(QJsonDocument::Compact));
    if (currentSessionIndex >= 0 && currentSessionIndex < sessions.size()) {
        settings.setValue(QStringLiteral("AI/ChatCurrentSessionId"), sessions[currentSessionIndex].id);
    }
}

void AIChatWidget::Impl::refreshSessionList()
{
    if (!sessionList) {
        return;
    }

    sessionList->blockSignals(true);
    sessionList->clear();
    for (const auto& session : sessions) {
        auto* item = new QListWidgetItem(displayTitleForSession(session), sessionList);
        item->setData(Qt::UserRole, session.id);
        item->setToolTip(QLocale::system().toString(session.updatedAt, QLocale::LongFormat));
    }
    sessionList->setCurrentRow(std::clamp(currentSessionIndex, 0, std::max(0, sessionList->count() - 1)));
    sessionList->blockSignals(false);
}

ChatSession* AIChatWidget::Impl::currentSession()
{
    if (currentSessionIndex < 0 || currentSessionIndex >= sessions.size()) {
        return nullptr;
    }
    return &sessions[currentSessionIndex];
}

int AIChatWidget::Impl::ensureSession()
{
    if (sessions.isEmpty()) {
        sessions.push_back(ChatSession{});
        currentSessionIndex = 0;
        return 0;
    }
    if (currentSessionIndex < 0 || currentSessionIndex >= sessions.size()) {
        currentSessionIndex = 0;
    }
    return currentSessionIndex;
}

void AIChatWidget::Impl::renderCurrentSession()
{
    if (!history) {
        return;
    }
    history->clear();
    const ChatSession* session = currentSession();
    if (!session) {
        history->append(QStringLiteral("<i>No session selected.</i>"));
        return;
    }
    if (session->messages.isEmpty()) {
        history->append(QStringLiteral("<i>Start a new conversation.</i>"));
        return;
    }
    for (const auto& message : session->messages) {
        history->append(htmlForMessage(message));
    }
    history->ensureCursorVisible();
}

void AIChatWidget::Impl::selectSession(int index)
{
    if (index < 0 || index >= sessions.size()) {
        return;
    }
    currentSessionIndex = index;
    renderCurrentSession();
    saveSessions();
}

void AIChatWidget::Impl::createNewSession()
{
    sessions.push_back(ChatSession{});
    currentSessionIndex = sessions.size() - 1;
    refreshSessionList();
    renderCurrentSession();
    saveSessions();
}

void AIChatWidget::Impl::appendMessage(const QString& role, const QString& text)
{
    const int idx = ensureSession();
    if (idx < 0 || idx >= sessions.size()) {
        return;
    }
    auto& session = sessions[idx];
    session.messages.push_back(ChatMessage{role, text});
    session.updatedAt = QDateTime::currentDateTime();
    if (role == QStringLiteral("user") && session.title == QStringLiteral("New Session")) {
        session.title = makeSessionTitleFromUserText(text);
    }
    refreshSessionList();
    renderCurrentSession();
    saveSessions();
}

void AIChatWidget::Impl::updateCurrentAssistantMessage(const QString& text, bool persist)
{
    const int idx = ensureSession();
    if (idx < 0 || idx >= sessions.size()) {
        return;
    }
    auto& session = sessions[idx];
    if (streamingMessageIndex < 0 || streamingMessageIndex >= session.messages.size()) {
        session.messages.push_back(ChatMessage{QStringLiteral("assistant"), text});
        streamingMessageIndex = session.messages.size() - 1;
    } else {
        session.messages[streamingMessageIndex].text = text;
    }
    session.updatedAt = QDateTime::currentDateTime();
    renderCurrentSession();
    if (persist) {
        saveSessions();
    }
}

void AIChatWidget::Impl::dispatchMessage(const QString& txt, AIClient* client)
{
    const QString trimmed = txt.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    appendMessage(QStringLiteral("user"), trimmed);
    if (input) {
        input->clear();
    }

    if (!client) {
        appendSystemLine(QStringLiteral("AI client is unavailable."));
        return;
    }

    if (client->isInitializing()) {
        appendSystemLine(QStringLiteral("Model is still loading. Your message will be sent after initialization finishes."));
        pendingMessage = trimmed;
        setSendingState(true);
        return;
    }

    if (client->isInitialized()) {
        isStreaming = true;
        streamingMessageIndex = -1;
        currentReply.clear();
        setSendingState(true);
        client->postMessage(UniString(trimmed));
        return;
    }

    const QString modelPath = resolveModelPath();
    if (modelPath.isEmpty()) {
        appendSystemLine(QStringLiteral("Model path is not configured. Use the Browse button above to select a local model."));
        return;
    }
    if (!QFileInfo::exists(modelPath)) {
        appendSystemLine(QStringLiteral("Model file not found: %1").arg(modelPath));
        return;
    }

    isInitializing = true;
    pendingMessage = trimmed;
    setSendingState(true);
    appendSystemLine(QStringLiteral("Loading local AI model in background: %1").arg(modelPath));

    std::thread([client, modelPath]() {
        client->initialize(modelPath);
    }).detach();
}

AIChatWidget::AIChatWidget(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
    impl_->splitter = new QSplitter(this);

    auto* leftPanel = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    impl_->newSessionButton = new QPushButton(QStringLiteral("New Session"), leftPanel);
    impl_->sessionList = new QListWidget(leftPanel);
    impl_->sessionList->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(impl_->newSessionButton);
    leftLayout->addWidget(impl_->sessionList, 1);

    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setSpacing(4);

    // Model loading toolbar
    auto* modelBar = new QWidget(rightPanel);
    auto* modelBarLayout = new QHBoxLayout(modelBar);
    modelBarLayout->setContentsMargins(0, 0, 0, 0);
    modelBarLayout->setSpacing(4);
    impl_->providerCombo = new QComboBox(modelBar);
    impl_->providerCombo->addItem(QStringLiteral("GGUF / llama.cpp"), QStringLiteral("local"));
    impl_->providerCombo->addItem(QStringLiteral("ONNX + DirectML"), QStringLiteral("onnx-dml"));
    impl_->providerCombo->setFixedWidth(124);
    impl_->providerCombo->setToolTip(QStringLiteral("Select the local AI backend."));
    impl_->modelPathEdit = new QLineEdit(modelBar);
    impl_->modelPathEdit->setReadOnly(true);
    impl_->browseButton = new QPushButton(QStringLiteral("Browse…"), modelBar);
    impl_->browseButton->setFixedWidth(70);
    impl_->loadButton = new QPushButton(QStringLiteral("Load"), modelBar);
    impl_->loadButton->setFixedWidth(50);
    impl_->unloadButton = new QPushButton(QStringLiteral("Unload"), modelBar);
    impl_->unloadButton->setFixedWidth(60);
    impl_->modelStatusLabel = new QLabel(QStringLiteral("Not loaded"), modelBar);
    impl_->modelStatusLabel->setMinimumWidth(70);
    modelBarLayout->addWidget(impl_->providerCombo);
    modelBarLayout->addWidget(impl_->modelPathEdit, 1);
    modelBarLayout->addWidget(impl_->browseButton);
    modelBarLayout->addWidget(impl_->loadButton);
    modelBarLayout->addWidget(impl_->unloadButton);
    modelBarLayout->addWidget(impl_->modelStatusLabel);
    rightLayout->addWidget(modelBar);

    impl_->history = new QTextEdit(rightPanel);
    impl_->history->setReadOnly(true);
    impl_->input = new QLineEdit(rightPanel);
    impl_->input->setPlaceholderText(QStringLiteral("Type a message and press Enter"));

    // Send / Cancel row
    auto* sendRow = new QWidget(rightPanel);
    auto* sendRowLayout = new QHBoxLayout(sendRow);
    sendRowLayout->setContentsMargins(0, 0, 0, 0);
    sendRowLayout->setSpacing(4);
    impl_->send = new QPushButton(QStringLiteral("Send"), sendRow);
    impl_->send->setDefault(true);
    impl_->cancel = new QPushButton(QStringLiteral("Cancel"), sendRow);
    impl_->cancel->setVisible(false);
    sendRowLayout->addStretch();
    sendRowLayout->addWidget(impl_->send);
    sendRowLayout->addWidget(impl_->cancel);

    rightLayout->addWidget(impl_->history, 1);
    rightLayout->addWidget(impl_->input);
    rightLayout->addWidget(sendRow);

    impl_->splitter->addWidget(leftPanel);
    impl_->splitter->addWidget(rightPanel);
    impl_->splitter->setStretchFactor(0, 0);
    impl_->splitter->setStretchFactor(1, 1);
    impl_->splitter->setChildrenCollapsible(false);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(impl_->splitter, 1);

    impl_->loadSessions();
    impl_->refreshSessionList();
    impl_->renderCurrentSession();

    auto client = AIClient::instance();

    // Populate model path from settings
    {
        const QString savedPath = impl_->resolveModelPath();
        if (!savedPath.isEmpty()) {
            impl_->modelPathEdit->setText(savedPath);
        }
    }
    {
        QSettings settings;
        QString provider = settings.value(QStringLiteral("AI/Provider"), QStringLiteral("local")).toString().trimmed().toLower();
        if (provider == QStringLiteral("onnx") ||
            provider == QStringLiteral("onnxdml") ||
            provider == QStringLiteral("directml")) {
            provider = QStringLiteral("onnx-dml");
        }
        const int index = impl_->providerCombo->findData(provider.isEmpty() ? QStringLiteral("local") : provider);
        if (index >= 0) {
            impl_->providerCombo->setCurrentIndex(index);
        }
    }
    impl_->updateModelPathPlaceholder();
    impl_->updateModelStatus(client);

    QObject::connect(impl_->providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, client](int) {
        if (!impl_ || !impl_->providerCombo) {
            return;
        }
        const QString provider = impl_->providerCombo->currentData().toString();
        QSettings settings;
        settings.setValue(QStringLiteral("AI/Provider"), provider);
        AIClient::instance()->setProvider(UniString(provider));
        impl_->updateModelPathPlaceholder();
        impl_->updateModelStatus(client);
    });

    QObject::connect(impl_->newSessionButton, &QPushButton::clicked, this, [this]() {
        impl_->createNewSession();
    });

    QObject::connect(impl_->sessionList, &QListWidget::currentRowChanged, this,
                     [this](int row) {
        impl_->selectSession(row);
    });

    // Browse button
    QObject::connect(impl_->browseButton, &QPushButton::clicked, this, [this]() {
        auto* dialog = new QFileDialog(this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(QStringLiteral("Select AI Model"));
        dialog->setFileMode(QFileDialog::ExistingFile);
        const QString provider = impl_->providerCombo ? impl_->providerCombo->currentData().toString().trimmed().toLower() : QStringLiteral("local");
        if (provider == QStringLiteral("onnx-dml") || provider == QStringLiteral("onnx")) {
            dialog->setNameFilter(QStringLiteral("ONNX Model Files (*.onnx *.ort);;All Files (*)"));
        } else {
            dialog->setNameFilter(QStringLiteral("GGUF Model Files (*.gguf);;All Files (*)"));
        }
        dialog->setDirectory(
            impl_->modelPathEdit->text().isEmpty()
                ? QString()
                : QFileInfo(impl_->modelPathEdit->text()).absolutePath());
        QObject::connect(dialog, &QFileDialog::accepted, this, [this, dialog]() {
            if (!impl_ || !dialog) {
                return;
            }
            const QStringList files = dialog->selectedFiles();
            if (files.isEmpty()) {
                return;
            }
            const QString path = files.front().trimmed();
            if (path.isEmpty()) {
                return;
            }
            impl_->modelPathEdit->setText(path);
            QSettings settings;
            settings.setValue(QStringLiteral("AI/ModelPath"), path);
            if (impl_->loadButton) {
                impl_->loadButton->setEnabled(!AIClient::instance()->isInitialized());
            }
        });
        dialog->open();
    });

    // Load button
    QObject::connect(impl_->loadButton, &QPushButton::clicked, this, [this, client]() {
        const QString path = impl_->modelPathEdit->text().trimmed();
        if (path.isEmpty() || !QFileInfo::exists(path)) {
            impl_->appendSystemLine(QStringLiteral("Please browse for a valid model file or directory first."));
            return;
        }
        impl_->isInitializing = true;
        impl_->loadButton->setEnabled(false);
        impl_->modelStatusLabel->setText(QStringLiteral("Loading…"));
        impl_->appendSystemLine(QStringLiteral("Loading model: %1").arg(path));
        std::thread([client, path]() {
            lowerCurrentThreadPriority();
            client->initialize(path);
        }).detach();
    });

    QObject::connect(impl_->unloadButton, &QPushButton::clicked, this, [this, client]() {
        if (!client) {
            return;
        }
        client->shutdown();
        impl_->isInitializing = false;
        impl_->isStreaming = false;
        impl_->currentReply.clear();
        impl_->pendingMessage.clear();
        impl_->streamingMessageIndex = -1;
        impl_->setSendingState(false);
        impl_->appendSystemLine(QStringLiteral("Local model unloaded."));
        impl_->updateModelStatus(client);
    });

    // Cancel button
    QObject::connect(impl_->cancel, &QPushButton::clicked, this, [this, client]() {
        client->cancelMessage();
    });

    QObject::connect(client, &AIClient::partialMessageReceived, this,
                     [this](const QString& partial) {
        if (!impl_) {
            return;
        }
        impl_->isStreaming = true;
        impl_->currentReply = partial;
        if (impl_->streamingUiUpdateScheduled) {
            return;
        }
        impl_->streamingUiUpdateScheduled = true;
        QTimer::singleShot(16, this, [this]() {
            if (!impl_ || !impl_->streamingUiUpdateScheduled) {
                return;
            }
            impl_->streamingUiUpdateScheduled = false;
            impl_->updateCurrentAssistantMessage(impl_->currentReply, false);
        });
    }, Qt::QueuedConnection);

    QObject::connect(client, &AIClient::messageReceived, this,
                     [this, client](const QString& message) {
        if (!impl_) {
            return;
        }
        if (!message.isEmpty()) {
            impl_->updateCurrentAssistantMessage(message);
        }
        impl_->isStreaming = false;
        impl_->currentReply.clear();
        impl_->streamingUiUpdateScheduled = false;
        impl_->pendingMessage.clear();
        impl_->streamingMessageIndex = -1;
        impl_->setSendingState(false);
        impl_->updateModelStatus(client);
    }, Qt::QueuedConnection);

    QObject::connect(client, &AIClient::messageCancelled, this,
                     [this, client]() {
        if (!impl_) {
            return;
        }
        impl_->appendSystemLine(QStringLiteral("Generation cancelled."));
        impl_->isStreaming = false;
        impl_->currentReply.clear();
        impl_->streamingUiUpdateScheduled = false;
        impl_->streamingMessageIndex = -1;
        impl_->setSendingState(false);
        impl_->updateModelStatus(client);
    }, Qt::QueuedConnection);

    QObject::connect(client, &AIClient::errorOccurred, this,
                     [this, client](const QString& error) {
        if (!impl_) {
            return;
        }
        impl_->appendSystemLine(QStringLiteral("AI error: %1").arg(error));
        impl_->isStreaming = false;
        impl_->currentReply.clear();
        impl_->streamingUiUpdateScheduled = false;
        impl_->streamingMessageIndex = -1;
        impl_->setSendingState(false);
        impl_->updateModelStatus(client);
    }, Qt::QueuedConnection);

    QObject::connect(client, &AIClient::initializationFinished, this,
                     [this, client](bool ok, const QString&) {
        if (!impl_) {
            return;
        }

        impl_->updateModelStatus(client);

        const QString pending = impl_->pendingMessage.trimmed();
        const bool shouldHandle = impl_->isInitializing || !pending.isEmpty();
        if (!shouldHandle) {
            return;
        }

        if (!ok && client->isInitializing()) {
            return;
        }

        impl_->isInitializing = false;
        impl_->streamingUiUpdateScheduled = false;

        if (!ok) {
            impl_->appendSystemLine(QStringLiteral("Failed to load local model."));
            impl_->pendingMessage.clear();
            impl_->setSendingState(false);
            return;
        }

        impl_->appendSystemLine(QStringLiteral("Local model loaded."));
        impl_->pendingMessage.clear();
        if (!pending.isEmpty()) {
            impl_->isStreaming = true;
            impl_->streamingMessageIndex = -1;
            impl_->setSendingState(true);
            client->postMessage(UniString(pending));
        } else {
            impl_->setSendingState(false);
        }
    }, Qt::QueuedConnection);

    QObject::connect(impl_->send, &QPushButton::clicked, this, [this, client]() {
        impl_->dispatchMessage(impl_->input ? impl_->input->text() : QString(), client);
    });

    QObject::connect(impl_->input, &QLineEdit::returnPressed, this, [this]() {
        if (impl_->send && impl_->send->isVisible()) {
            impl_->send->click();
        }
    });
}

AIChatWidget::~AIChatWidget() { delete impl_; }

void AIChatWidget::sendUserMessage(const UniString& msg) {
    auto* client = AIClient::instance();
    impl_->dispatchMessage(msg.toQString(), client);
}

void AIChatWidget::setProvider(const UniString& provider) {
    if (impl_ && impl_->providerCombo) {
        QString normalized = provider.toQString().trimmed().toLower();
        if (normalized == QStringLiteral("onnx") ||
            normalized == QStringLiteral("onnxdml") ||
            normalized == QStringLiteral("directml")) {
            normalized = QStringLiteral("onnx-dml");
        }
        const int index = impl_->providerCombo->findData(normalized.isEmpty() ? QStringLiteral("local") : normalized);
        if (index >= 0) {
            impl_->providerCombo->setCurrentIndex(index);
        }
        QSettings settings;
        settings.setValue(QStringLiteral("AI/Provider"), normalized.isEmpty() ? QStringLiteral("local") : normalized);
        impl_->updateModelPathPlaceholder();
    }
    AIClient::instance()->setProvider(provider);
}

} // namespace Artifact

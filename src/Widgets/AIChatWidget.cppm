
module;
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QTextCursor>
#include <wobjectimpl.h>

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

module Widgets.AIChatWidget;

import Utils.String.UniString;
import AI.Client;

namespace Artifact {

 W_OBJECT_IMPL(AIChatWidget)

class AIChatWidget::Impl {
public:
    QVBoxLayout* layout = nullptr;
    QTextEdit* history = nullptr;
    QLineEdit* input = nullptr;
    QPushButton* send = nullptr;
    
    // Tracking current streaming message
    bool isStreaming = false;
    int lastMessagePos = 0;
};

AIChatWidget::AIChatWidget(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
    impl_->layout = new QVBoxLayout(this);
    impl_->history = new QTextEdit(this);
    impl_->history->setReadOnly(true);
    impl_->input = new QLineEdit(this);
    impl_->send = new QPushButton("Send", this);

    impl_->layout->addWidget(impl_->history);
    impl_->layout->addWidget(impl_->input);
    impl_->layout->addWidget(impl_->send);

    // AI Client Signals
    auto client = AIClient::instance();
    
    QObject::connect(client, &AIClient::partialMessageReceived, [this](QString partial) {
        if (!impl_->isStreaming) {
            impl_->isStreaming = true;
            impl_->history->append("<b>AI:</b> ");
            impl_->lastMessagePos = impl_->history->textCursor().position();
        }
        
        // Update the last message
        QTextCursor cursor = impl_->history->textCursor();
        cursor.setPosition(impl_->lastMessagePos, QTextCursor::MoveAnchor);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        cursor.insertText(partial);
        impl_->history->ensureCursorVisible();
    });

    QObject::connect(client, &AIClient::messageReceived, [this](QString final) {
        impl_->isStreaming = false;
        impl_->history->append(""); // New line for next message
    });

    QObject::connect(impl_->send, &QPushButton::clicked, [this, client]() {
        QString txt = impl_->input->text();
        if (txt.isEmpty()) return;
        impl_->history->append(QString("<b>You:</b> %1").arg(txt));
        impl_->input->clear();
        
        // Use non-blocking async message
        client->postMessage(UniString(txt));
    });
}

AIChatWidget::~AIChatWidget() { delete impl_; }

void AIChatWidget::sendUserMessage(const UniString& msg) {
    impl_->history->append(QString("<b>You:</b> %1").arg(msg.toQString()));
    AIClient::instance()->postMessage(msg);
}

void AIChatWidget::setProvider(const UniString& provider) {
    AIClient::instance()->setProvider(provider);
}

}

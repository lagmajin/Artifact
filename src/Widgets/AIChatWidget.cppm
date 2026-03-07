module;
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
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

    QObject::connect(impl_->send, &QPushButton::clicked, [this]() {
        QString txt = impl_->input->text();
        if (txt.isEmpty()) return;
        impl_->history->append(QString("You: %1").arg(txt));
        impl_->input->clear();
        auto resp = AIClient::instance()->sendMessage(UniString(txt));
        impl_->history->append(resp.toQString());
    });
}

AIChatWidget::~AIChatWidget() { delete impl_; }

void AIChatWidget::sendUserMessage(const UniString& msg) {
    auto resp = AIClient::instance()->sendMessage(msg);
    impl_->history->append(msg.toQString());
    impl_->history->append(resp.toQString());
}

void AIChatWidget::setProvider(const UniString& provider) {
    AIClient::instance()->setProvider(provider);
}

}

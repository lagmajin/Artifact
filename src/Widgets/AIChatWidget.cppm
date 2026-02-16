module;
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>

module Widgets.AIChatWidget;

import std;
import Utils.String.UniString;
import AI.Client;

namespace Artifact {

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

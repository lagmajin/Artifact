module;
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QProcess>
#include <wobjectimpl.h>

module Widgets.PowerShellWidget;

import std;
import Utils.String.UniString;

namespace Artifact {

using ArtifactCore::UniString;

W_OBJECT_IMPL(PowerShellWidget)

class PowerShellWidget::Impl {
public:
    QTextEdit* log = nullptr;
    QLineEdit* cmd = nullptr;
    QPushButton* run = nullptr;
    QProcess* proc = nullptr;
};

PowerShellWidget::PowerShellWidget(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
    auto* layout = new QVBoxLayout(this);
    impl_->log = new QTextEdit(this);
    impl_->log->setReadOnly(true);
    impl_->cmd = new QLineEdit(this);
    impl_->run = new QPushButton("Run", this);
    layout->addWidget(impl_->log);
    layout->addWidget(impl_->cmd);
    layout->addWidget(impl_->run);

    impl_->proc = new QProcess(this);
    impl_->proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(impl_->run, &QPushButton::clicked, this, [this]() {
        runCommandAsync(UniString(impl_->cmd->text()));
    });

    connect(impl_->proc, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray out = impl_->proc->readAllStandardOutput();
        appendLog(UniString(QString::fromLocal8Bit(out)));
    });

    connect(impl_->proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus status){
        Q_UNUSED(exitCode);
        Q_UNUSED(status);
        appendLog(UniString("Process finished."));
        // emit signal
        QString full = impl_->log->toPlainText();
        Q_EMIT commandFinished(UniString(full));
    });
}

PowerShellWidget::~PowerShellWidget() {
    if (impl_) {
        if (impl_->proc) impl_->proc->kill();
        delete impl_;
    }
}

UniString PowerShellWidget::runCommand(const UniString& command) {
    QProcess p;
    QString cmd = command.toQString();
#ifdef Q_OS_WIN
    QStringList args = {"-NoProfile","-Command", cmd};
    p.start("powershell.exe", args);
#else
    QStringList args = {"-c", cmd};
    p.start("/bin/sh", args);
#endif
    p.waitForFinished(-1);
    QByteArray out = p.readAllStandardOutput();
    return UniString(QString::fromLocal8Bit(out));
}

void PowerShellWidget::runCommandAsync(const UniString& command) {
    if (impl_->proc->state() != QProcess::NotRunning) {
        appendLog(UniString("Another process is running."));
        return;
    }
#ifdef Q_OS_WIN
    QString program = "powershell.exe";
    QStringList args = {"-NoProfile","-Command", command.toQString()};
#else
    QString program = "/bin/sh";
    QStringList args = {"-c", command.toQString()};
#endif
    impl_->proc->start(program, args);
}

void PowerShellWidget::appendLog(const UniString& text) {
    impl_->log->append(text.toQString());
}

}

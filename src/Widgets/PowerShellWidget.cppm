module;
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QProcess>
#include <QLabel>
#include <QHBoxLayout>
#include <QDir>
#include <QShortcut>
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
module Widgets.PowerShellWidget;




import Utils.String.UniString;

namespace Artifact {

using ArtifactCore::UniString;

W_OBJECT_IMPL(PowerShellWidget)

class PowerShellWidget::Impl {
public:
    QTextEdit* log = nullptr;
    QLineEdit* cmd = nullptr;
    QLineEdit* workingDirectory = nullptr;
    QPushButton* run = nullptr;
    QPushButton* stop = nullptr;
    QPushButton* clear = nullptr;
    QLabel* status = nullptr;
    QProcess* proc = nullptr;
    QStringList history;
    int historyIndex = -1;
};

PowerShellWidget::PowerShellWidget(QWidget* parent) : QWidget(parent), impl_(new Impl()) {
    auto* layout = new QVBoxLayout(this);
    impl_->log = new QTextEdit(this);
    impl_->log->setReadOnly(true);
    impl_->cmd = new QLineEdit(this);
    impl_->cmd->setPlaceholderText(QStringLiteral("Command"));
    impl_->workingDirectory = new QLineEdit(QDir::currentPath(), this);
    impl_->workingDirectory->setPlaceholderText(QStringLiteral("Working directory"));
    impl_->run = new QPushButton(QStringLiteral("Run"), this);
    impl_->stop = new QPushButton(QStringLiteral("Stop"), this);
    impl_->clear = new QPushButton(QStringLiteral("Clear"), this);
    impl_->status = new QLabel(QStringLiteral("Ready"), this);
    layout->addWidget(impl_->log);
    auto *directoryRow = new QHBoxLayout();
    directoryRow->addWidget(new QLabel(QStringLiteral("Working directory:"), this));
    directoryRow->addWidget(impl_->workingDirectory, 1);
    layout->addLayout(directoryRow);
    layout->addWidget(impl_->cmd);
    auto *actionRow = new QHBoxLayout();
    actionRow->addWidget(impl_->run);
    actionRow->addWidget(impl_->stop);
    actionRow->addWidget(impl_->clear);
    actionRow->addWidget(impl_->status, 1);
    layout->addLayout(actionRow);

    impl_->proc = new QProcess(this);
    impl_->proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(impl_->run, &QPushButton::clicked, this, [this]() {
        const QString command = impl_->cmd->text().trimmed();
        if (command.isEmpty()) return;
        impl_->history.removeAll(command);
        impl_->history.append(command);
        impl_->historyIndex = -1;
        runCommandAsync(UniString(command));
    });
    auto browseHistory = [this](const int direction) {
        if (impl_->history.isEmpty()) return;
        if (impl_->historyIndex < 0) {
            impl_->historyIndex = impl_->history.size();
        }
        const int historySize = static_cast<int>(impl_->history.size());
        impl_->historyIndex = std::clamp(
            impl_->historyIndex + direction, 0, historySize);
        impl_->cmd->setText(impl_->historyIndex == impl_->history.size()
                                ? QString{}
                                : impl_->history.at(impl_->historyIndex));
        impl_->cmd->setCursorPosition(impl_->cmd->text().size());
    };
    auto *historyUp = new QShortcut(QKeySequence(Qt::Key_Up), impl_->cmd);
    historyUp->setContext(Qt::WidgetShortcut);
    connect(historyUp, &QShortcut::activated, this,
            [browseHistory]() { browseHistory(-1); });
    auto *historyDown = new QShortcut(QKeySequence(Qt::Key_Down), impl_->cmd);
    historyDown->setContext(Qt::WidgetShortcut);
    connect(historyDown, &QShortcut::activated, this,
            [browseHistory]() { browseHistory(1); });
    connect(impl_->stop, &QPushButton::clicked, this, [this]() {
        if (impl_->proc->state() != QProcess::NotRunning) {
            impl_->proc->kill();
            impl_->status->setText(QStringLiteral("Stopped"));
        }
    });
    connect(impl_->clear, &QPushButton::clicked, this, [this]() {
        impl_->log->clear();
        impl_->status->setText(QStringLiteral("Ready"));
    });

    connect(impl_->proc, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray out = impl_->proc->readAllStandardOutput();
        appendLog(UniString(QString::fromLocal8Bit(out)));
    });

    connect(impl_->proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus processStatus){
        const QString result = processStatus == QProcess::NormalExit
            ? QStringLiteral("Finished (exit code %1)").arg(exitCode)
            : QStringLiteral("Stopped (exit code %1)").arg(exitCode);
        impl_->status->setText(result);
        appendLog(UniString(result));
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
    const QString directory = impl_->workingDirectory->text().trimmed();
    if (!directory.isEmpty() && QDir(directory).exists()) {
        impl_->proc->setWorkingDirectory(QDir(directory).absolutePath());
    }
    impl_->status->setText(QStringLiteral("Running"));
    appendLog(UniString(QStringLiteral("> %1").arg(command.toQString())));
    impl_->proc->start(program, args);
}

void PowerShellWidget::appendLog(const UniString& text) {
    impl_->log->append(text.toQString());
}

}

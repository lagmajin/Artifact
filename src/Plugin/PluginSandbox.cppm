module;

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QString>
#include <QTimer>

module Artifact.Plugin.Sandbox;

import ArtifactCore.Plugin.Common;

static constexpr int kHeartbeatIntervalMs = 500;
static constexpr int kHeartbeatTimeoutMs = 1000;
static constexpr int kMaxCrashCount = 3;
static constexpr int kMaxProtocolLineBytes = 4 * 1024 * 1024;

namespace {

QJsonObject makePingCommand(int id) {
    QJsonObject cmd;
    cmd["cmd"] = QString("ping");
    cmd["id"] = id;
    return cmd;
}

QJsonObject makeShutdownCommand() {
    QJsonObject cmd;
    cmd["cmd"] = QString("shutdown");
    return cmd;
}

QJsonObject makeLoadCommand(const QString& dllPath) {
    QJsonObject cmd;
    cmd["cmd"] = QString("load");
    cmd["pluginPath"] = dllPath;
    return cmd;
}

}

namespace Artifact {

struct ArtifactPluginSandbox::Impl {
    std::string pluginId;
    QString runnerPath;
    QString pluginDllPath;
    QProcess* process = nullptr;
    QTimer* heartbeatTimer = nullptr;

    int heartbeatId = 0;
    int crashes = 0;
    bool expectingPong = false;
    std::chrono::steady_clock::time_point heartbeatSentAt{};
    std::string lastErrorMsg;
    QByteArray stdoutBuffer;
    QByteArray stderrBuffer;

    SandboxCallback responseCallback;
    CrashCallback crashCallback;
    RestartCallback restartCallback;
    FailCallback failCallback;

    void consumeJsonLines(QByteArray* buffer, ArtifactPluginSandbox* sandbox) {
        if (buffer->size() > kMaxProtocolLineBytes &&
            buffer->indexOf('\n') < 0) {
            buffer->clear();
            lastErrorMsg = "Plugin protocol response exceeded maximum line size";
            return;
        }
        while (true) {
            int newline = buffer->indexOf('\n');
            if (newline < 0) break;
            if (newline > kMaxProtocolLineBytes) {
                buffer->remove(0, newline + 1);
                lastErrorMsg = "Plugin protocol response exceeded maximum line size";
                continue;
            }
            QByteArray line = buffer->left(newline).trimmed();
            buffer->remove(0, newline + 1);
            if (line.isEmpty()) continue;

            QJsonParseError err;
            auto doc = QJsonDocument::fromJson(line, &err);
            if (err.error == QJsonParseError::NoError && doc.isObject()) {
                const QJsonObject response = doc.object();
                if (response.value(QStringLiteral("cmd")).toString() == QStringLiteral("pong") &&
                    response.value(QStringLiteral("id")).toInt(-1) == heartbeatId) {
                    expectingPong = false;
                }
                if (responseCallback) {
                    responseCallback(pluginId, response);
                }
            }
        }
    }
};

ArtifactPluginSandbox::ArtifactPluginSandbox(const std::string& pluginId,
                                             const QString& runnerPath,
                                             const QString& pluginDllPath)
    : impl_(std::make_unique<Impl>()) {
    impl_->pluginId = pluginId;
    impl_->runnerPath = runnerPath;
    impl_->pluginDllPath = pluginDllPath;
}

ArtifactPluginSandbox::~ArtifactPluginSandbox() {
    stop();
}

void ArtifactPluginSandbox::setResponseCallback(SandboxCallback cb) { impl_->responseCallback = std::move(cb); }
void ArtifactPluginSandbox::setCrashCallback(CrashCallback cb) { impl_->crashCallback = std::move(cb); }
void ArtifactPluginSandbox::setRestartCallback(RestartCallback cb) { impl_->restartCallback = std::move(cb); }
void ArtifactPluginSandbox::setFailCallback(FailCallback cb) { impl_->failCallback = std::move(cb); }

bool ArtifactPluginSandbox::start() {
    if (impl_->process) return false;

    impl_->stdoutBuffer.clear();
    impl_->stderrBuffer.clear();
    impl_->expectingPong = false;
    impl_->process = new QProcess();
    impl_->process->setProcessChannelMode(QProcess::SeparateChannels);

    QObject::connect(impl_->process, &QProcess::readyReadStandardOutput,
                     [this]() { onReadyReadStdout(); });
    QObject::connect(impl_->process, &QProcess::readyReadStandardError,
                     [this]() { onReadyReadStderr(); });
    QObject::connect(impl_->process,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     [this](int exitCode, QProcess::ExitStatus status) {
                         onProcessFinished(exitCode, static_cast<int>(status));
                     });

    impl_->process->start(impl_->runnerPath, {
        QString("--plugin"), impl_->pluginDllPath
    });

    if (!impl_->process->waitForStarted(10000)) {
        impl_->lastErrorMsg = "Failed to start plugin runner: " + impl_->runnerPath.toStdString();
        delete impl_->process;
        impl_->process = nullptr;
        return false;
    }

    if (!sendCommand(makeLoadCommand(impl_->pluginDllPath))) {
        impl_->lastErrorMsg = "Failed to send plugin load command";
        impl_->process->terminate();
        impl_->process->waitForFinished(1000);
        delete impl_->process;
        impl_->process = nullptr;
        return false;
    }

    impl_->heartbeatTimer = new QTimer();
    QObject::connect(impl_->heartbeatTimer, &QTimer::timeout,
                     [this]() { onHeartbeat(); });
    impl_->heartbeatTimer->start(kHeartbeatIntervalMs);
    impl_->heartbeatId = 0;
    impl_->expectingPong = false;

    return true;
}

void ArtifactPluginSandbox::stop() {
    impl_->expectingPong = false;
    if (impl_->heartbeatTimer) {
        impl_->heartbeatTimer->stop();
        delete impl_->heartbeatTimer;
        impl_->heartbeatTimer = nullptr;
    }

    if (impl_->process && impl_->process->state() != QProcess::NotRunning) {
        sendCommand(makeShutdownCommand());
        if (!impl_->process->waitForFinished(3000)) {
            impl_->process->terminate();
            if (!impl_->process->waitForFinished(3000)) {
                impl_->process->kill();
                impl_->process->waitForFinished(3000);
            }
        }
    }

    delete impl_->process;
    impl_->process = nullptr;
}

bool ArtifactPluginSandbox::sendCommand(const QJsonObject& command) {
    if (!impl_->process || impl_->process->state() != QProcess::Running) {
        return false;
    }
    QJsonDocument doc(command);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    const qint64 written = impl_->process->write(data);
    return written == data.size();
}

bool ArtifactPluginSandbox::isRunning() const {
    return impl_->process && impl_->process->state() == QProcess::Running;
}

int ArtifactPluginSandbox::crashCount() const {
    return impl_->crashes;
}

std::string ArtifactPluginSandbox::lastError() const {
    return impl_->lastErrorMsg;
}

void ArtifactPluginSandbox::onHeartbeat() {
    if (!impl_->process || impl_->process->state() != QProcess::Running) return;

    if (impl_->expectingPong) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - impl_->heartbeatSentAt).count();
        if (elapsed < kHeartbeatTimeoutMs) return;
        impl_->crashes++;
        impl_->expectingPong = false;

        if (impl_->crashes >= kMaxCrashCount) {
            impl_->lastErrorMsg = "Plugin crashed " + std::to_string(impl_->crashes) + " times";
            if (impl_->failCallback) {
                impl_->failCallback(impl_->pluginId, impl_->lastErrorMsg);
            }
            impl_->heartbeatTimer->stop();
            return;
        }

        if (impl_->crashCallback) {
            impl_->crashCallback(impl_->pluginId, impl_->crashes);
        }
        stop();
        if (start()) {
            if (impl_->restartCallback) {
                impl_->restartCallback(impl_->pluginId);
            }
        }
        return;
    }

    ++impl_->heartbeatId;
    if (sendCommand(makePingCommand(impl_->heartbeatId))) {
        impl_->heartbeatSentAt = std::chrono::steady_clock::now();
        impl_->expectingPong = true;
    }
}

void ArtifactPluginSandbox::onReadyReadStdout() {
    if (impl_->process) {
        impl_->stdoutBuffer.append(impl_->process->readAllStandardOutput());
        impl_->consumeJsonLines(&impl_->stdoutBuffer, this);
    }
}

void ArtifactPluginSandbox::onReadyReadStderr() {
    if (impl_->process) {
        impl_->stderrBuffer.append(impl_->process->readAllStandardError());
        if (impl_->stderrBuffer.size() > kMaxProtocolLineBytes) {
            impl_->stderrBuffer.remove(0,
                impl_->stderrBuffer.size() - kMaxProtocolLineBytes);
        }
    }
}

void ArtifactPluginSandbox::onProcessFinished(int exitCode, int status) {
    if (impl_->heartbeatTimer && impl_->heartbeatTimer->isActive()) {
        impl_->lastErrorMsg = "Plugin process exited unexpectedly (code=" +
                              std::to_string(exitCode) + ", status=" +
                              std::to_string(status) + ")";
        QProcess* finishedProcess = impl_->process;
        impl_->process = nullptr;
        if (finishedProcess) finishedProcess->deleteLater();
        impl_->crashes++;
        if (impl_->crashes >= kMaxCrashCount) {
            impl_->lastErrorMsg = "Plugin process crashed " + std::to_string(impl_->crashes) + " times";
            if (impl_->failCallback) {
                impl_->failCallback(impl_->pluginId, impl_->lastErrorMsg);
            }
            impl_->heartbeatTimer->stop();
            return;
        }
        if (impl_->crashCallback) {
            impl_->crashCallback(impl_->pluginId, impl_->crashes);
        }
        if (impl_->heartbeatTimer) {
            impl_->heartbeatTimer->stop();
            delete impl_->heartbeatTimer;
            impl_->heartbeatTimer = nullptr;
        }
        if (start()) {
            if (impl_->restartCallback) {
                impl_->restartCallback(impl_->pluginId);
            }
        }
    }
}

} // namespace Artifact

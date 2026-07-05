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

export module Artifact.Plugin.Sandbox;

import ArtifactCore.Plugin.Common;

export namespace Artifact {

using SandboxCallback = std::function<void(const std::string& pluginId, const QJsonObject& response)>;
using CrashCallback = std::function<void(const std::string& pluginId, int crashCount)>;
using RestartCallback = std::function<void(const std::string& pluginId)>;
using FailCallback = std::function<void(const std::string& pluginId, const std::string& reason)>;

class ArtifactPluginSandbox {
public:
    ArtifactPluginSandbox(const std::string& pluginId,
                          const QString& runnerPath,
                          const QString& pluginDllPath);
    ~ArtifactPluginSandbox();

    bool start();
    void stop();
    bool sendCommand(const QJsonObject& command);
    bool isRunning() const;
    int crashCount() const;
    std::string lastError() const;

    void setResponseCallback(SandboxCallback cb);
    void setCrashCallback(CrashCallback cb);
    void setRestartCallback(RestartCallback cb);
    void setFailCallback(FailCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void onHeartbeat();
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, int status);
};

} // namespace Artifact

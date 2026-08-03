module;
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)
#include <cstdio>
#include <climits>
#include <algorithm>
#include <iostream>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QDir>
#include <QSysInfo>
#include <QThread>

module FarmWorkerMain;

import NetworkRPCClient;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Artifact Farm Worker"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Out-of-process render worker for Artifact Studio"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("host"), QStringLiteral("Master hostname"), QStringLiteral("host"), QStringLiteral("127.0.0.1")});
    parser.addOption({QStringLiteral("port"), QStringLiteral("Master RPC port"), QStringLiteral("port"), QStringLiteral("9876")});
    parser.addOption({QStringLiteral("worker-id"), QStringLiteral("Unique worker identifier"), QStringLiteral("id"), QString()});
    parser.addOption({QStringLiteral("token"), QStringLiteral("Master authentication token"), QStringLiteral("token"), QString()});
    parser.addOption({QStringLiteral("tls"), QStringLiteral("Use TLS for RPC transport")});
    parser.addOption({QStringLiteral("ca-cert"), QStringLiteral("CA certificate PEM file for TLS"), QStringLiteral("file"), QString()});
    parser.addOption({QStringLiteral("gpu-vendor"), QStringLiteral("GPU vendor capability"), QStringLiteral("vendor"), QString()});
    parser.addOption({QStringLiteral("gpu-name"), QStringLiteral("GPU device capability"), QStringLiteral("name"), QString()});
    parser.addOption({QStringLiteral("vram-bytes"), QStringLiteral("GPU VRAM capacity capability"), QStringLiteral("bytes"), QString()});
    parser.addOption({QStringLiteral("ram-bytes"), QStringLiteral("System RAM capacity capability"), QStringLiteral("bytes"), QString()});
    parser.addOption({QStringLiteral("plugins"), QStringLiteral("Comma-separated plugin capability list"), QStringLiteral("names"), QString()});
    parser.addOption({QStringLiteral("plugin-versions"), QStringLiteral("Plugin versions as JSON object"), QStringLiteral("json"), QString()});
    parser.addOption({QStringLiteral("pool"), QStringLiteral("Logical worker pool capability"), QStringLiteral("name"), QString()});
    parser.addOption({QStringLiteral("env"), QStringLiteral("Renderer environment overrides (KEY=VALUE,comma-separated)"), QStringLiteral("values"), QString()});
    parser.addOption({QStringLiteral("maintenance"), QStringLiteral("Register worker in maintenance mode")});
    parser.process(app);

    const QString host = parser.value(QStringLiteral("host"));
    const unsigned short port = parser.value(QStringLiteral("port")).toUShort();
    const QString workerId = parser.value(QStringLiteral("worker-id"));
    const QString authToken = parser.value(QStringLiteral("token"));
    const QString gpuVendor = parser.value(QStringLiteral("gpu-vendor")).trimmed();
    const QString gpuName = parser.value(QStringLiteral("gpu-name")).trimmed();
    const QString pool = parser.value(QStringLiteral("pool")).trimmed();
    const QStringList environmentOverrides = parser.value(QStringLiteral("env")).split(',', Qt::SkipEmptyParts);
    const bool maintenance = parser.isSet(QStringLiteral("maintenance"));
    const qint64 vramBytes = parser.value(QStringLiteral("vram-bytes")).toLongLong();
    const qint64 ramBytes = parser.value(QStringLiteral("ram-bytes")).toLongLong();
    const QString finalId = workerId.isEmpty()
        ? QStringLiteral("worker-%1").arg(QCoreApplication::applicationPid())
        : workerId;

    ArtifactCore::NetworkRPCClient client;
    client.setAuthToken(authToken);
    client.setTlsEnabled(parser.isSet(QStringLiteral("tls")),
                         parser.value(QStringLiteral("ca-cert")));
    QJsonObject capabilities;
    capabilities[QStringLiteral("os")] = QSysInfo::prettyProductName();
    capabilities[QStringLiteral("architecture")] = QSysInfo::currentCpuArchitecture();
    capabilities[QStringLiteral("cpuCount")] = QThread::idealThreadCount();
    if (!gpuVendor.isEmpty()) capabilities[QStringLiteral("gpuVendor")] = gpuVendor;
    if (!gpuName.isEmpty()) capabilities[QStringLiteral("gpuName")] = gpuName;
    if (vramBytes > 0) capabilities[QStringLiteral("vramBytes")] = vramBytes;
    if (ramBytes > 0) capabilities[QStringLiteral("ramBytes")] = ramBytes;
    if (!pool.isEmpty()) capabilities[QStringLiteral("pool")] = pool;
    capabilities[QStringLiteral("maintenance")] = maintenance;
    QJsonArray plugins;
    for (const auto& plugin : parser.value(QStringLiteral("plugins")).split(',', Qt::SkipEmptyParts)) {
        const QString name = plugin.trimmed();
        if (!name.isEmpty() && !plugins.contains(name)) plugins.append(name);
    }
    if (!plugins.isEmpty()) capabilities[QStringLiteral("plugins")] = plugins;
    const QJsonDocument pluginVersionsDoc = QJsonDocument::fromJson(
        parser.value(QStringLiteral("plugin-versions")).toUtf8());
    if (pluginVersionsDoc.isObject()) {
        capabilities[QStringLiteral("pluginVersions")] = pluginVersionsDoc.object();
    }
    client.setCapabilities(capabilities);
    client.setOnJobAssigned([&](const QJsonObject& jobData) {
        int startFrame = jobData[QStringLiteral("startFrame")].toInt(0);
        int endFrame = jobData[QStringLiteral("endFrame")].toInt(0);
        int step = jobData[QStringLiteral("step")].toInt(1);
        int completedFrames = 0;
        int failedFrames = 0;
        qDebug() << "[Worker] Assigned frames" << startFrame << "to" << endFrame << "step" << step;

        const QString renderer = jobData[QStringLiteral("rendererExecutable")].toString().trimmed();
        const QJsonObject payload = jobData[QStringLiteral("renderPayload")].toObject();
        if (renderer.isEmpty() || payload.isEmpty()) {
            const QString message = renderer.isEmpty()
                ? QStringLiteral("No renderer executable was provided for farm assignment")
                : QStringLiteral("No render payload was provided for farm assignment");
            for (int f = startFrame; f < endFrame; f += step) {
                client.sendFrameFailed(f, message);
                ++failedFrames;
                client.sendWorkerProgress(completedFrames, failedFrames, f);
            }
            return;
        }

        {
            QJsonObject renderJob = payload;
            renderJob[QStringLiteral("jobId")] = renderJob[QStringLiteral("jobId")].toString()
                + QStringLiteral("-worker-%1-%2-%3").arg(finalId).arg(startFrame).arg(endFrame);
            QJsonObject composition = renderJob[QStringLiteral("composition")].toObject();
            composition[QStringLiteral("frameStart")] = startFrame;
            composition[QStringLiteral("frameEnd")] = endFrame;
            renderJob[QStringLiteral("composition")] = composition;
            if (!jobData[QStringLiteral("outputPath")].toString().isEmpty()) {
                QJsonObject output = renderJob[QStringLiteral("output")].toObject();
                output[QStringLiteral("path")] = jobData[QStringLiteral("outputPath")].toString();
                renderJob[QStringLiteral("output")] = output;
            }

            QTemporaryDir diagnosticsDir;
            if (diagnosticsDir.isValid()) {
                QJsonObject diagnostics = renderJob[QStringLiteral("diagnostics")].toObject();
                diagnostics[QStringLiteral("summaryFile")] = QDir(diagnosticsDir.path()).filePath(
                    QStringLiteral("summary.json"));
                diagnostics[QStringLiteral("eventLogFile")] = QDir(diagnosticsDir.path()).filePath(
                    QStringLiteral("events.jsonl"));
                diagnostics[QStringLiteral("cancelFile")] = QDir(diagnosticsDir.path()).filePath(
                    QStringLiteral("cancel"));
                renderJob[QStringLiteral("diagnostics")] = diagnostics;
            }

            QTemporaryFile jobFile(QDir::tempPath() + QStringLiteral("/artifact-farm-job-XXXXXX.json"));
            if (!jobFile.open()) {
                for (int f = startFrame; f < endFrame; f += step) {
                    client.sendFrameFailed(f, QStringLiteral("Failed to create renderer job file"));
                    ++failedFrames;
                    client.sendWorkerProgress(completedFrames, failedFrames, f);
                }
                return;
            }
            jobFile.write(QJsonDocument(renderJob).toJson(QJsonDocument::Compact));
            jobFile.flush();

            const int jobTimeoutMs = jobData[QStringLiteral("jobTimeoutMs")].toInt(0);
            const int frameTimeoutMs = jobData[QStringLiteral("frameTimeoutMs")].toInt(0);
            const int frameCount = std::max(1, (endFrame - startFrame + step - 1) / step);
            int timeoutMs = jobTimeoutMs;
            if (frameTimeoutMs > 0) {
                const qint64 sliceTimeout = static_cast<qint64>(frameTimeoutMs) * frameCount;
                timeoutMs = timeoutMs > 0
                    ? static_cast<int>(std::min<qint64>(timeoutMs, sliceTimeout))
                    : static_cast<int>(std::min<qint64>(sliceTimeout, INT_MAX));
            }
            const int maxAttempts = std::clamp(
                jobData[QStringLiteral("retryMaxAttempts")].toInt(1), 1, 32);
            const int initialBackoffMs = std::max(0,
                jobData[QStringLiteral("retryInitialBackoffMs")].toInt(0));
            bool renderSucceeded = false;
            QString error;
            QProcessEnvironment rendererEnvironment = QProcessEnvironment::systemEnvironment();
            for (const auto& overrideValue : environmentOverrides) {
                const int separator = overrideValue.indexOf('=');
                if (separator <= 0) continue;
                const QString key = overrideValue.left(separator).trimmed();
                if (!key.isEmpty()) {
                    rendererEnvironment.insert(key, overrideValue.mid(separator + 1));
                }
            }
            const QJsonObject payloadEnvironment = renderJob[QStringLiteral("environment")].toObject();
            for (auto it = payloadEnvironment.begin(); it != payloadEnvironment.end(); ++it) {
                if (!it.key().trimmed().isEmpty() && it.value().isString()) {
                    rendererEnvironment.insert(it.key(), it.value().toString());
                }
            }
            for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
                if (attempt > 1 && initialBackoffMs > 0) {
                    const int exponent = std::min(attempt - 2, 16);
                    const qint64 backoff = std::min<qint64>(
                        60000, static_cast<qint64>(initialBackoffMs) << exponent);
                    QThread::msleep(static_cast<unsigned long>(backoff));
                }
                QProcess rendererProcess;
                rendererProcess.setProcessEnvironment(rendererEnvironment);
                rendererProcess.start(renderer, {QStringLiteral("--job"), jobFile.fileName()});
                const bool started = rendererProcess.waitForStarted(5000);
                const bool finished = started && rendererProcess.waitForFinished(
                    timeoutMs > 0 ? timeoutMs : -1);
                if (started && !finished && rendererProcess.state() != QProcess::NotRunning)
                    rendererProcess.kill();
                error = QString::fromLocal8Bit(rendererProcess.readAllStandardError()).trimmed();
                if (started && finished && rendererProcess.exitStatus() == QProcess::NormalExit
                    && rendererProcess.exitCode() == 0) {
                    renderSucceeded = true;
                    break;
                }
            }
            if (!renderSucceeded) {
                const QString message = error.isEmpty()
                    ? QStringLiteral("External renderer failed after retries") : error;
                for (int f = startFrame; f < endFrame; f += step) {
                    client.sendFrameFailed(f, message);
                    ++failedFrames;
                    client.sendWorkerProgress(completedFrames, failedFrames, f);
                }
                return;
            }

            const QString outputPath = jobData[QStringLiteral("outputPath")].toString().trimmed();
            const bool sequenceOutput = outputPath.contains('%') || outputPath.contains('*')
                || outputPath.contains(QStringLiteral("####"));
            if (!outputPath.isEmpty() && !sequenceOutput) {
                const QFileInfo outputInfo(outputPath);
                if (!outputInfo.isFile() || outputInfo.size() <= 0) {
                    const QString message = QStringLiteral("External renderer produced no output");
                    for (int f = startFrame; f < endFrame; f += step) {
                        client.sendFrameFailed(f, message);
                        ++failedFrames;
                        client.sendWorkerProgress(completedFrames, failedFrames, f);
                    }
                    return;
                }
            }
            if (!outputPath.isEmpty() && sequenceOutput && !outputPath.contains('*')) {
                const QRegularExpression hashPattern(QStringLiteral("#+"));
                const QRegularExpression printfPattern(QStringLiteral("%0?(\\d*)d"));
                QString missingFrame;
                for (int f = startFrame; f < endFrame; f += step) {
                    QString framePath = outputPath;
                    const auto hashMatch = hashPattern.match(framePath);
                    if (hashMatch.hasMatch()) {
                        framePath.replace(hashMatch.capturedStart(), hashMatch.capturedLength(),
                                          QStringLiteral("%1").arg(f, hashMatch.capturedLength(), 10,
                                                                  QChar('0')));
                    } else {
                        const auto printfMatch = printfPattern.match(framePath);
                        if (!printfMatch.hasMatch()) continue;
                        const int width = printfMatch.captured(1).toInt();
                        framePath.replace(printfMatch.capturedStart(), printfMatch.capturedLength(),
                                          QStringLiteral("%1").arg(f, width, 10, QChar('0')));
                    }
                    const QFileInfo frameInfo(framePath);
                    if (!frameInfo.isFile() || frameInfo.size() <= 0) {
                        missingFrame = QStringLiteral("%1 (frame %2)").arg(framePath).arg(f);
                        break;
                    }
                }
                if (!missingFrame.isEmpty()) {
                    const QString message = QStringLiteral("Missing or empty rendered frame: %1")
                        .arg(missingFrame);
                    for (int f = startFrame; f < endFrame; f += step) {
                        client.sendFrameFailed(f, message);
                        ++failedFrames;
                        client.sendWorkerProgress(completedFrames, failedFrames, f);
                    }
                    return;
                }
            }
        }

        for (int f = startFrame; f < endFrame; f += step) {
            client.sendFrameCompleted(f);
            ++completedFrames;
            client.sendWorkerProgress(completedFrames, failedFrames, f);
            qDebug() << "[Worker] Completed frame" << f;
        }
        qDebug() << "[Worker] Slice done";
    });

    client.setOnDisconnected([&]() {
        qDebug() << "[Worker] Disconnected from master, exiting";
        QCoreApplication::quit();
    });

    qDebug() << "[Worker] Connecting to" << host << ":" << port << "as" << finalId;
    if (!client.connectToServer(host, port, finalId)) {
        qCritical() << "[Worker] Failed to connect to master at" << host << ":" << port;
        return 1;
    }

    qDebug() << "[Worker] Connected, registered as" << finalId;
    return app.exec();
}

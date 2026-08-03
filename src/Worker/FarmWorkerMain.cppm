module;
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)
#include <cstdio>
#include <iostream>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QProcess>
#include <QTemporaryFile>
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
    parser.process(app);

    const QString host = parser.value(QStringLiteral("host"));
    const unsigned short port = parser.value(QStringLiteral("port")).toUShort();
    const QString workerId = parser.value(QStringLiteral("worker-id"));
    const QString authToken = parser.value(QStringLiteral("token"));
    const QString finalId = workerId.isEmpty()
        ? QStringLiteral("worker-%1").arg(QCoreApplication::applicationPid())
        : workerId;

    ArtifactCore::NetworkRPCClient client;
    client.setAuthToken(authToken);
    QJsonObject capabilities;
    capabilities[QStringLiteral("os")] = QSysInfo::prettyProductName();
    capabilities[QStringLiteral("architecture")] = QSysInfo::currentCpuArchitecture();
    capabilities[QStringLiteral("cpuCount")] = QThread::idealThreadCount();
    client.setCapabilities(capabilities);
    client.setOnJobAssigned([&](const QJsonObject& jobData) {
        int startFrame = jobData[QStringLiteral("startFrame")].toInt(0);
        int endFrame = jobData[QStringLiteral("endFrame")].toInt(0);
        int step = jobData[QStringLiteral("step")].toInt(1);
        qDebug() << "[Worker] Assigned frames" << startFrame << "to" << endFrame << "step" << step;

        const QString renderer = jobData[QStringLiteral("rendererExecutable")].toString().trimmed();
        const QJsonObject payload = jobData[QStringLiteral("renderPayload")].toObject();
        if (!renderer.isEmpty() && !payload.isEmpty()) {
            QJsonObject renderJob = payload;
            QJsonObject composition = renderJob[QStringLiteral("composition")].toObject();
            composition[QStringLiteral("frameStart")] = startFrame;
            composition[QStringLiteral("frameEnd")] = endFrame;
            renderJob[QStringLiteral("composition")] = composition;
            if (!jobData[QStringLiteral("outputPath")].toString().isEmpty()) {
                QJsonObject output = renderJob[QStringLiteral("output")].toObject();
                output[QStringLiteral("path")] = jobData[QStringLiteral("outputPath")].toString();
                renderJob[QStringLiteral("output")] = output;
            }

            QTemporaryFile jobFile(QDir::tempPath() + QStringLiteral("/artifact-farm-job-XXXXXX.json"));
            if (!jobFile.open()) {
                for (int f = startFrame; f < endFrame; f += step)
                    client.sendFrameFailed(f, QStringLiteral("Failed to create renderer job file"));
                return;
            }
            jobFile.write(QJsonDocument(renderJob).toJson(QJsonDocument::Compact));
            jobFile.flush();

            QProcess rendererProcess;
            rendererProcess.start(renderer, {QStringLiteral("--job"), jobFile.fileName()});
            const bool started = rendererProcess.waitForStarted(5000);
            const bool finished = started && rendererProcess.waitForFinished(-1);
            const QString error = QString::fromLocal8Bit(rendererProcess.readAllStandardError()).trimmed();
            if (!started || !finished || rendererProcess.exitStatus() != QProcess::NormalExit
                || rendererProcess.exitCode() != 0) {
                const QString message = error.isEmpty()
                    ? QStringLiteral("External renderer failed") : error;
                for (int f = startFrame; f < endFrame; f += step)
                    client.sendFrameFailed(f, message);
                return;
            }
        }

        for (int f = startFrame; f < endFrame; f += step) {
            client.sendFrameCompleted(f);
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

module;
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)
#include <cstdio>
#include <iostream>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QJsonObject>

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
    parser.process(app);

    const QString host = parser.value(QStringLiteral("host"));
    const unsigned short port = parser.value(QStringLiteral("port")).toUShort();
    const QString workerId = parser.value(QStringLiteral("worker-id"));
    const QString finalId = workerId.isEmpty()
        ? QStringLiteral("worker-%1").arg(QCoreApplication::applicationPid())
        : workerId;

    ArtifactCore::NetworkRPCClient client;
    client.setOnJobAssigned([&](const QJsonObject& jobData) {
        int startFrame = jobData[QStringLiteral("startFrame")].toInt(0);
        int endFrame = jobData[QStringLiteral("endFrame")].toInt(0);
        int step = jobData[QStringLiteral("step")].toInt(1);
        qDebug() << "[Worker] Assigned frames" << startFrame << "to" << endFrame << "step" << step;
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

module;
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QProcess>

export module Artifact.AI.Cloud.AICloudSessionController;

import std;
import Artifact.AI.Cloud.AICloudWorkerProtocol;
import Core.AI.Context;

export namespace Artifact {

class AICloudSessionController {
public:
  AICloudSessionController() = default;

  void setContextSnapshot(const ArtifactCore::AIContext &context) {
    context_ = context;
  }

  void setSessionId(const QString &sessionId) {
    sessionId_ = sessionId.trimmed().isEmpty() ? QStringLiteral("default")
                                               : sessionId.trimmed();
  }

  QString sessionId() const { return sessionId_; }

  AICloudChatRequest makeChatRequest(const QString &providerId,
                                     const QString &modelId,
                                     const QString &userPrompt,
                                     const QString &systemPrompt,
                                     const QString &toolTrace,
                                     const QString &baseUrl,
                                     const QString &apiKey,
                                     double temperature = 0.7,
                                     bool stream = true) const {
    return Artifact::makeChatRequest(sessionId_, providerId, modelId,
                                     userPrompt, systemPrompt, toolTrace,
                                     baseUrl, apiKey, context_, temperature,
                                     stream);
  }

  QByteArray serializeChatRequestBody(const AICloudChatRequest &request) const {
    return Artifact::serializeChatRequestBody(request);
  }

  QStringList buildCurlArgs(const AICloudChatRequest &request,
                            const QStringList &extraArgs = {}) const {
    return Artifact::buildCurlJsonRequestArgs(
        request.baseUrl, request.apiKey, serializeChatRequestBody(request),
        extraArgs);
  }

  AICloudChatResponse parseChatResponse(const QByteArray &stdoutBytes,
                                        const QByteArray &stderrBytes,
                                        int exitCode,
                                        QProcess::ExitStatus exitStatus) const {
    return Artifact::parseChatResponse(stdoutBytes, stderrBytes, exitCode,
                                       exitStatus);
  }

private:
  ArtifactCore::AIContext context_;
  QString sessionId_ = QStringLiteral("default");
};

} // namespace Artifact

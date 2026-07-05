module;
#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QString>
#include <QStringList>

export module Artifact.AI.Cloud.AICloudWorkerProtocol;

import std;
import Core.AI.Context;

export namespace Artifact {

struct AICloudError {
  QString code;
  QString message;
  bool retryable = false;

  QJsonObject toJson() const {
    return QJsonObject{
        {QStringLiteral("code"), code},
        {QStringLiteral("message"), message},
        {QStringLiteral("retryable"), retryable},
    };
  }
};

struct AICloudRequestEnvelope {
  int version = 1;
  QString id;
  QString type;
  QString sessionId;
  QString providerId;
  QString modelId;
  QDateTime timestampUtc = QDateTime::currentDateTimeUtc();

  QJsonObject toJson() const {
    return QJsonObject{
        {QStringLiteral("version"), version},
        {QStringLiteral("id"), id},
        {QStringLiteral("type"), type},
        {QStringLiteral("sessionId"), sessionId},
        {QStringLiteral("providerId"), providerId},
        {QStringLiteral("modelId"), modelId},
        {QStringLiteral("timestampUtc"), timestampUtc.toString(Qt::ISODateWithMs)},
    };
  }
};

struct AICloudEventEnvelope {
  int version = 1;
  QString type;
  QString sessionId;
  QString correlationId;
  QDateTime timestampUtc = QDateTime::currentDateTimeUtc();

  QJsonObject toJson() const {
    return QJsonObject{
        {QStringLiteral("version"), version},
        {QStringLiteral("type"), type},
        {QStringLiteral("sessionId"), sessionId},
        {QStringLiteral("correlationId"), correlationId},
        {QStringLiteral("timestampUtc"), timestampUtc.toString(Qt::ISODateWithMs)},
    };
  }
};

struct AICloudConnectionState {
  QString providerId;
  QString baseUrl;
  QString modelId;
  bool hasApiKey = false;
  bool supportsStreaming = true;
  bool supportsRemoteModels = false;

  QJsonObject toJson() const {
    return QJsonObject{
        {QStringLiteral("providerId"), providerId},
        {QStringLiteral("baseUrl"), baseUrl},
        {QStringLiteral("modelId"), modelId},
        {QStringLiteral("hasApiKey"), hasApiKey},
        {QStringLiteral("supportsStreaming"), supportsStreaming},
        {QStringLiteral("supportsRemoteModels"), supportsRemoteModels},
    };
  }
};

struct AICloudSessionState {
  QString sessionId;
  QString providerId;
  QString modelId;
  bool isConnected = false;
  bool isSending = false;
  QString endpoint;
  QString lastErrorCode;
};

struct AICloudWorkerState {
  bool running = false;
  bool ready = false;
  QString processId;
  QString version;
  QString lastError;
};

struct AICloudEventPayload {
  AICloudEventEnvelope envelope;
  QJsonObject data;
  AICloudError error;

  QJsonObject toJson() const {
    QJsonObject root = envelope.toJson();
    if (!data.isEmpty()) {
      root.insert(QStringLiteral("data"), data);
    }
    if (!error.code.isEmpty() || !error.message.isEmpty()) {
      root.insert(QStringLiteral("error"), error.toJson());
    }
    return root;
  }
};

struct AICloudChatRequest {
  AICloudRequestEnvelope envelope;
  QString userPrompt;
  QString systemPrompt;
  QString toolTrace;
  QString baseUrl;
  QString apiKey;
  double temperature = 0.7;
  bool stream = true;
  QJsonArray messages;
  QJsonObject contextSnapshot;

  QJsonObject toBodyJson() const {
    QJsonObject body;
    body.insert(QStringLiteral("model"), envelope.modelId);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("stream"), stream);
    body.insert(QStringLiteral("temperature"), temperature);
    return body;
  }
};

struct AICloudChatResponse {
  AICloudRequestEnvelope envelope;
  bool ok = false;
  int exitCode = 0;
  QProcess::ExitStatus exitStatus = QProcess::NormalExit;
  QByteArray stdoutBytes;
  QByteArray stderrBytes;
  QString errorText;
  QString assistantContent;
  QJsonObject rawJson;
  QJsonObject errorObject;
  bool hasToolCall = false;
  QJsonObject toolCall;
};

inline AICloudChatRequest makeChatRequest(
    const QString &sessionId, const QString &providerId,
    const QString &modelId, const QString &userPrompt,
    const QString &systemPrompt, const QString &toolTrace,
    const QString &baseUrl, const QString &apiKey,
    const ArtifactCore::AIContext &context,
    double temperature = 0.7, bool stream = true) {
  AICloudChatRequest request;
  request.envelope.type = QStringLiteral("chat.send");
  request.envelope.sessionId = sessionId.isEmpty()
                                   ? QStringLiteral("default")
                                   : sessionId.trimmed();
  request.envelope.providerId = providerId.trimmed();
  request.envelope.modelId = modelId.trimmed();
  request.envelope.id = QStringLiteral("chat-%1")
                            .arg(QDateTime::currentMSecsSinceEpoch());
  request.baseUrl = baseUrl.trimmed();
  request.apiKey = apiKey.trimmed();
  request.temperature = temperature;
  request.stream = stream;
  request.userPrompt = userPrompt;
  request.systemPrompt = systemPrompt;
  request.toolTrace = toolTrace;
  request.contextSnapshot = context.toJson();

  request.messages.append(
      QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                  {QStringLiteral("content"), systemPrompt}});
  if (!toolTrace.trimmed().isEmpty()) {
    request.messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("system")},
        {QStringLiteral("content"),
         QStringLiteral(
             "The previous tool execution produced the following result. "
             "Use it to answer the user:\n%1")
             .arg(toolTrace)}});
  }
  request.messages.append(
      QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                  {QStringLiteral("content"), userPrompt}});
  return request;
}

inline QByteArray serializeChatRequestBody(const AICloudChatRequest &request) {
  return QJsonDocument(request.toBodyJson()).toJson(QJsonDocument::Compact);
}

inline QStringList buildCurlJsonRequestArgs(const QString &url,
                                           const QString &apiKey,
                                           const QByteArray &body,
                                           const QStringList &extraArgs = {}) {
  QStringList args;
  args << QStringLiteral("-sS") << QStringLiteral("--fail-with-body")
       << QStringLiteral("--location");
  args.append(extraArgs);
  if (!apiKey.trimmed().isEmpty()) {
    args << QStringLiteral("-H")
         << QStringLiteral("Authorization: Bearer %1").arg(apiKey.trimmed());
  }
  if (!body.isEmpty()) {
    args << QStringLiteral("-H")
         << QStringLiteral("Content-Type: application/json")
         << QStringLiteral("--data-binary") << QStringLiteral("@-");
  }
  args << url;
  return args;
}

inline AICloudChatResponse parseChatResponse(const QByteArray &stdoutBytes,
                                             const QByteArray &stderrBytes,
                                             int exitCode,
                                             QProcess::ExitStatus exitStatus) {
  AICloudChatResponse response;
  response.stdoutBytes = stdoutBytes;
  response.stderrBytes = stderrBytes;
  response.exitCode = exitCode;
  response.exitStatus = exitStatus;

  const QString output = QString::fromUtf8(stdoutBytes).trimmed();
  if (output.isEmpty()) {
    response.errorText = QString::fromUtf8(stderrBytes).trimmed();
    if (response.errorText.isEmpty()) {
      response.errorText = QStringLiteral("Error: empty response");
    }
    return response;
  }

  QJsonParseError parseError{};
  const QJsonDocument doc = QJsonDocument::fromJson(stdoutBytes, &parseError);
  if (doc.isNull() || parseError.error != QJsonParseError::NoError) {
    response.errorText = QStringLiteral("Invalid JSON response");
    return response;
  }

  response.ok = true;
  response.rawJson = doc.object();

  const QJsonObject obj = doc.object();
  if (obj.contains(QStringLiteral("choices")) &&
      obj.value(QStringLiteral("choices")).isArray()) {
    const QJsonArray choices = obj.value(QStringLiteral("choices")).toArray();
    if (!choices.isEmpty()) {
      const QJsonObject choice = choices.first().toObject();
      const QJsonObject message =
          choice.value(QStringLiteral("message")).toObject();
      response.assistantContent = message.value(QStringLiteral("content")).toString();
      if (message.contains(QStringLiteral("tool_calls"))) {
        response.hasToolCall = true;
        response.toolCall = message.value(QStringLiteral("tool_calls"))
                                 .toArray()
                                 .first()
                                 .toObject();
      }
    }
  }

  if (obj.contains(QStringLiteral("error"))) {
    response.ok = false;
    response.errorObject = obj.value(QStringLiteral("error")).toObject();
    response.errorText =
        response.errorObject.value(QStringLiteral("message")).toString();
    if (response.errorText.isEmpty()) {
      response.errorText = QStringLiteral("API Error");
    }
  }

  return response;
}

} // namespace Artifact

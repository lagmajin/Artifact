module;

#include <QJsonObject>
#include <QString>
#include <QStringList>

export module Artifact.AI.AgentApprovalPolicy;
import Core.ArtifactMath;

export namespace Artifact {

/// Shared tool-approval policy for every surface that can execute AI tool
/// calls (the AI Cloud widget and Detached Task execution).
enum class ToolApprovalMode {
  AskEveryTime = 0,
  AutoApprove = 1,
  YOLO = 2,
};

inline ToolApprovalMode toolApprovalModeFromIndex(const int index) {
  switch (ArtifactCore::artifactClamp(index, 0, 2)) {
  case 1:
    return ToolApprovalMode::AutoApprove;
  case 2:
    return ToolApprovalMode::YOLO;
  default:
    return ToolApprovalMode::AskEveryTime;
  }
}

inline int toolApprovalModeToIndex(const ToolApprovalMode mode) {
  switch (mode) {
  case ToolApprovalMode::AutoApprove:
    return 1;
  case ToolApprovalMode::YOLO:
    return 2;
  case ToolApprovalMode::AskEveryTime:
  default:
    return 0;
  }
}

/// Settings location shared by all approval surfaces.  Reading the same key
/// keeps the user-visible approval setting single.
inline QString agentApprovalSettingsOrg() {
  return QStringLiteral("ArtifactStudio");
}
inline QString agentApprovalSettingsGroup() {
  return QStringLiteral("AICloud");
}
inline QString agentApprovalModeKey() {
  return QStringLiteral("toolApprovalMode");
}

/// Method-name based read-only classification for AI tool calls.
///
/// This is a heuristic over the tool method name.  Command-level read-only
/// classification for the CommandIR vocabulary lives in
/// ArtifactCore::CommandIR::isReadOnlyType() and matches exact command types;
/// the two notions are intentionally distinct.
inline bool isReadOnlyToolCall(const QJsonObject &toolCall) {
  const QString method =
      toolCall.value(QStringLiteral("method")).toString().trimmed().toLower();
  if (method.isEmpty()) {
    return false;
  }

  static const QStringList kReadOnlyPrefixes = {
      QStringLiteral("get"),      QStringLiteral("list"),
      QStringLiteral("find"),     QStringLiteral("query"),
      QStringLiteral("inspect"),  QStringLiteral("preview"),
      QStringLiteral("describe"), QStringLiteral("check"),
      QStringLiteral("has"),      QStringLiteral("is"),
      QStringLiteral("count"),    QStringLiteral("current"),
      QStringLiteral("read"),     QStringLiteral("fetch"),
      QStringLiteral("ping")};
  for (const QString &prefix : kReadOnlyPrefixes) {
    if (method.startsWith(prefix)) {
      return true;
    }
  }
  return false;
}

} // namespace Artifact

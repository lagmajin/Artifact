module;

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVariant>

export module Artifact.Test.AIToolBridge;

import Core.AI.Context;
import Core.AI.CommandSandbox;
import Core.AI.Describable;
import Core.AI.McpBridge;
import Core.AI.PromptGenerator;
import Core.AI.ToolBridge;
import Artifact.AI.WorkspaceAutomation;

namespace {

struct TestReport {
    int failures = 0;

    void check(bool condition, const QString& label)
    {
        if (!condition) {
            ++failures;
            qWarning().noquote() << "[AI Test][FAIL]" << label;
        } else {
            qInfo().noquote() << "[AI Test][OK]" << label;
        }
    }
};

class DummyToolHost : public ArtifactCore::IDescribable {
public:
    QString className() const override { return QStringLiteral("DummyToolHost"); }

    ArtifactCore::LocalizedText briefDescription() const override
    {
        return ArtifactCore::IDescribable::loc(
            "A tiny test tool host for AI bridge validation.",
            "AI bridge validation test tool host.",
            {});
    }

    QList<ArtifactCore::MethodDescription> methodDescriptions() const override
    {
        ArtifactCore::MethodDescription method;
        method.name = QStringLiteral("echo");
        method.description = ArtifactCore::IDescribable::loc(
            "Echo the first argument back to the caller.",
            "Echo the first argument back to the caller.",
            {});
        method.returnType = QStringLiteral("QString");
        method.parameterNames = {QStringLiteral("message")};
        method.parameterTypes = {QStringLiteral("QString")};
        method.returnDescription = ArtifactCore::IDescribable::loc(
            "The echoed message.",
            "The echoed message.",
            {});
        return {method};
    }

    QVariant invokeMethod(QStringView name, const QVariantList& args) override
    {
        if (name == QStringLiteral("echo")) {
            if (args.isEmpty()) {
                return QVariant(QStringLiteral("<empty>"));
            }
            return args.first();
        }
        return {};
    }
};

class BrokenToolHost : public ArtifactCore::IDescribable {
public:
    QString className() const override { return QStringLiteral("BrokenToolHost"); }

    ArtifactCore::LocalizedText briefDescription() const override
    {
        return ArtifactCore::IDescribable::loc(
            "An intentionally malformed tool host.",
            "An intentionally malformed tool host.",
            {});
    }

    QList<ArtifactCore::MethodDescription> methodDescriptions() const override
    {
        ArtifactCore::MethodDescription method;
        method.name = QString();
        method.description = ArtifactCore::IDescribable::loc(
            "Broken method entry that should be skipped.",
            "Broken method entry that should be skipped.",
            {});
        method.returnType = QStringLiteral("void");
        return {method};
    }
};

static QJsonObject findTool(const QJsonArray& tools, const QString& component, const QString& method)
{
    for (const QJsonValue& value : tools) {
        const QJsonObject tool = value.toObject();
        if (tool.value(QStringLiteral("component")).toString() == component &&
            tool.value(QStringLiteral("method")).toString() == method) {
            return tool;
        }
    }
    return {};
}

} // namespace

export namespace Artifact {

int runAIToolBridgeTests()
{
    Artifact::WorkspaceAutomation::ensureRegistered();
    static ArtifactCore::AutoRegisterDescribable<DummyToolHost> dummyRegistration(QStringLiteral("DummyToolHost"));
    static ArtifactCore::AutoRegisterDescribable<BrokenToolHost> brokenRegistration(QStringLiteral("BrokenToolHost"));

    TestReport report;

    const QJsonDocument schemaDoc =
        QJsonDocument::fromJson(ArtifactCore::AIPromptGenerator::generateToolSchemaJson());
    report.check(schemaDoc.isObject(), QStringLiteral("schema is valid JSON object"));

    const QJsonObject schema = schemaDoc.object();
    const QJsonArray tools = schema.value(QStringLiteral("tools")).toArray();
    report.check(!tools.isEmpty(), QStringLiteral("schema exposes at least one tool"));

    const QJsonObject dummyTool = findTool(tools, QStringLiteral("DummyToolHost"), QStringLiteral("echo"));
    report.check(!dummyTool.isEmpty(), QStringLiteral("dummy tool is present in schema"));
    report.check(dummyTool.value(QStringLiteral("parameters")).toArray().size() == 1,
                 QStringLiteral("dummy tool has exactly one parameter"));

    const QJsonObject brokenTool = findTool(tools, QStringLiteral("BrokenToolHost"), QString());
    report.check(brokenTool.isEmpty(), QStringLiteral("malformed tool entries are skipped"));

    const QJsonObject workspaceTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("workspaceSnapshot"));
    report.check(!workspaceTool.isEmpty(), QStringLiteral("workspace automation tool is present in schema"));

    const QJsonObject createCompositionTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("createComposition"));
    report.check(!createCompositionTool.isEmpty(), QStringLiteral("workspace automation exposes composition creation"));

    const QJsonObject currentLayersTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("listCurrentCompositionLayers"));
    report.check(!currentLayersTool.isEmpty(), QStringLiteral("workspace automation exposes layer inspection"));

    const QJsonObject addImageLayerTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("addImageLayerToCurrentComposition"));
    report.check(!addImageLayerTool.isEmpty(), QStringLiteral("workspace automation exposes image layer creation"));

    const QJsonObject addSvgLayerTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("addSvgLayerToCurrentComposition"));
    report.check(!addSvgLayerTool.isEmpty(), QStringLiteral("workspace automation exposes svg layer creation"));

    const QJsonObject addAudioLayerTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("addAudioLayerToCurrentComposition"));
    report.check(!addAudioLayerTool.isEmpty(), QStringLiteral("workspace automation exposes audio layer creation"));

    const QJsonObject addTextLayerTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("addTextLayerToCurrentComposition"));
    report.check(!addTextLayerTool.isEmpty(), QStringLiteral("workspace automation exposes text layer creation"));

    const QJsonObject addNullLayerTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("addNullLayerToCurrentComposition"));
    report.check(!addNullLayerTool.isEmpty(), QStringLiteral("workspace automation exposes null layer creation"));

    const QJsonObject addSolidLayerTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("addSolidLayerToCurrentComposition"));
    report.check(!addSolidLayerTool.isEmpty(), QStringLiteral("workspace automation exposes solid layer creation"));

    const QJsonObject setVisibilityTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("setLayerVisibleInCurrentComposition"));
    report.check(!setVisibilityTool.isEmpty(), QStringLiteral("workspace automation exposes layer visibility editing"));

    const QJsonObject replaceSourceTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("replaceLayerSourceInCurrentComposition"));
    report.check(!replaceSourceTool.isEmpty(), QStringLiteral("workspace automation exposes source replacement"));

    const QJsonObject splitLayerTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("splitLayerAtCurrentTime"));
    report.check(!splitLayerTool.isEmpty(), QStringLiteral("workspace automation exposes split-at-time"));

    const QJsonObject duplicateCompositionTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("duplicateComposition"));
    report.check(!duplicateCompositionTool.isEmpty(), QStringLiteral("workspace automation exposes composition duplication"));

    const QJsonObject compositionRemovalMessageTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("compositionRemovalConfirmationMessage"));
    report.check(!compositionRemovalMessageTool.isEmpty(), QStringLiteral("workspace automation exposes composition removal confirmation"));

    const QJsonObject removeCompositionTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("removeCompositionWithRenderQueueCleanup"));
    report.check(!removeCompositionTool.isEmpty(), QStringLiteral("workspace automation exposes composition removal"));

    const QJsonObject removeAssetsTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("removeAllAssets"));
    report.check(!removeAssetsTool.isEmpty(), QStringLiteral("workspace automation exposes asset clearing"));

    const QJsonObject renderQueueJobTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("renderQueueJobByIndex"));
    report.check(!renderQueueJobTool.isEmpty(), QStringLiteral("workspace automation exposes render queue job lookup"));

    const QJsonObject renderQueueJobStatusTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("renderQueueJobStatusAt"));
    report.check(!renderQueueJobStatusTool.isEmpty(), QStringLiteral("workspace automation exposes render queue job status lookup"));

    const QJsonObject renderQueueJobProgressTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("renderQueueJobProgressAt"));
    report.check(!renderQueueJobProgressTool.isEmpty(), QStringLiteral("workspace automation exposes render queue job progress lookup"));

    const QJsonObject renderQueueJobErrorTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("renderQueueJobErrorMessageAt"));
    report.check(!renderQueueJobErrorTool.isEmpty(), QStringLiteral("workspace automation exposes render queue job error lookup"));

    const QJsonObject duplicateQueueTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("duplicateRenderQueueAt"));
    report.check(!duplicateQueueTool.isEmpty(), QStringLiteral("workspace automation exposes render queue duplication"));

    const QJsonObject moveQueueTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("moveRenderQueue"));
    report.check(!moveQueueTool.isEmpty(), QStringLiteral("workspace automation exposes render queue move"));

    const QJsonObject removeQueueAtTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("removeRenderQueueAt"));
    report.check(!removeQueueAtTool.isEmpty(), QStringLiteral("workspace automation exposes render queue removal"));

    const QJsonObject setQueueNameTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("setRenderQueueJobNameAt"));
    report.check(!setQueueNameTool.isEmpty(), QStringLiteral("workspace automation exposes render queue rename"));

    const QJsonObject setQueueOutputTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("setRenderQueueJobOutputPathAt"));
    report.check(!setQueueOutputTool.isEmpty(), QStringLiteral("workspace automation exposes render queue output path editing"));

    const QJsonObject setQueueRangeTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("setRenderQueueJobFrameRangeAt"));
    report.check(!setQueueRangeTool.isEmpty(), QStringLiteral("workspace automation exposes render queue frame range editing"));

    const QJsonObject setQueueOutputSettingsTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("setRenderQueueJobOutputSettingsAt"));
    report.check(!setQueueOutputSettingsTool.isEmpty(), QStringLiteral("workspace automation exposes render queue output settings editing"));

    const QJsonObject setQueueIntegratedTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("setRenderQueueJobIntegratedRenderEnabledAt"));
    report.check(!setQueueIntegratedTool.isEmpty(), QStringLiteral("workspace automation exposes render queue integrated toggle"));

    const QJsonObject setQueueBackendTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("setRenderQueueJobRenderBackendAt"));
    report.check(!setQueueBackendTool.isEmpty(), QStringLiteral("workspace automation exposes render queue backend editing"));

    const QJsonObject setQueueAudioPathTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("setRenderQueueJobAudioSourcePathAt"));
    report.check(!setQueueAudioPathTool.isEmpty(), QStringLiteral("workspace automation exposes render queue audio path editing"));

    const QJsonObject setQueueAudioCodecTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("setRenderQueueJobAudioCodecAt"));
    report.check(!setQueueAudioCodecTool.isEmpty(), QStringLiteral("workspace automation exposes render queue audio codec editing"));

    const QJsonObject setQueueAudioBitrateTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("setRenderQueueJobAudioBitrateKbpsAt"));
    report.check(!setQueueAudioBitrateTool.isEmpty(), QStringLiteral("workspace automation exposes render queue audio bitrate editing"));

    const QJsonObject resetQueueJobTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("resetRenderQueueJobForRerun"));
    report.check(!resetQueueJobTool.isEmpty(), QStringLiteral("workspace automation exposes render queue rerun reset"));

    const QJsonObject resetQueueJobsTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("resetCompletedAndFailedRenderQueueJobsForRerun"));
    report.check(!resetQueueJobsTool.isEmpty(), QStringLiteral("workspace automation exposes render queue batch rerun reset"));

    const QJsonObject startQueueAtTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("startRenderQueueAt"));
    report.check(!startQueueAtTool.isEmpty(), QStringLiteral("workspace automation exposes render queue start by index"));

    const QJsonObject pauseQueueAtTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("pauseRenderQueueAt"));
    report.check(!pauseQueueAtTool.isEmpty(), QStringLiteral("workspace automation exposes render queue pause by index"));

    const QJsonObject cancelQueueAtTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("cancelRenderQueueAt"));
    report.check(!cancelQueueAtTool.isEmpty(), QStringLiteral("workspace automation exposes render queue cancel by index"));

    const QJsonObject findProjectItemTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("findProjectItemById"));
    report.check(!findProjectItemTool.isEmpty(), QStringLiteral("workspace automation exposes project item lookup"));

    const QJsonObject projectItemPathTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("projectItemPathById"));
    report.check(!projectItemPathTool.isEmpty(), QStringLiteral("workspace automation exposes project item path lookup"));

    const QJsonObject projectItemRemovalMessageTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("projectItemRemovalConfirmationMessage"));
    report.check(!projectItemRemovalMessageTool.isEmpty(), QStringLiteral("workspace automation exposes project item removal confirmation"));

    const QJsonObject renameProjectItemTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("renameProjectItemById"));
    report.check(!renameProjectItemTool.isEmpty(), QStringLiteral("workspace automation exposes project item rename"));

    const QJsonObject moveProjectItemTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("moveProjectItemToFolder"));
    report.check(!moveProjectItemTool.isEmpty(), QStringLiteral("workspace automation exposes project item move"));

    const QJsonObject createFolderTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("createFolderInProject"));
    report.check(!createFolderTool.isEmpty(), QStringLiteral("workspace automation exposes project folder creation"));

    const QJsonObject removeProjectItemTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("removeProjectItemById"));
    report.check(!removeProjectItemTool.isEmpty(), QStringLiteral("workspace automation exposes project item removal"));

    const QJsonObject relinkFootageTool = findTool(tools, QStringLiteral("WorkspaceAutomation"), QStringLiteral("relinkFootageByPath"));
    report.check(!relinkFootageTool.isEmpty(), QStringLiteral("workspace automation exposes relink by path"));

    QJsonObject parsedToolCall;
    const bool parsed = ArtifactCore::ToolBridge::tryParseToolCall(
        QStringLiteral("```json\n{\"tool\":{\"class\":\"DummyToolHost\",\"method\":\"echo\",\"arguments\":[\"hello\"]}}\n```"),
        &parsedToolCall);
    report.check(parsed, QStringLiteral("wrapped tool call parses"));
    report.check(parsedToolCall.value(QStringLiteral("class")).toString() == QStringLiteral("DummyToolHost"),
                 QStringLiteral("parsed tool call preserves class"));
    report.check(parsedToolCall.value(QStringLiteral("arguments")).toArray().size() == 1,
                 QStringLiteral("parsed tool call preserves arguments"));

    const ArtifactCore::ToolBridgeResult toolResult =
        ArtifactCore::ToolBridge::executeToolCall(parsedToolCall);
    report.check(toolResult.handled, QStringLiteral("dummy tool executes"));
    report.check(toolResult.value.toString() == QStringLiteral("hello"),
                 QStringLiteral("dummy tool returns echoed value"));

    QJsonObject invalidToolCall;
    invalidToolCall[QStringLiteral("class")] = QStringLiteral("DummyToolHost");
    invalidToolCall[QStringLiteral("method")] = QString();
    const ArtifactCore::ToolBridgeResult invalidResult =
        ArtifactCore::ToolBridge::executeToolCall(invalidToolCall);
    report.check(!invalidResult.handled, QStringLiteral("invalid tool call is rejected"));

    ArtifactCore::AIContext context;
    context.setProjectSummary(QStringLiteral("Project Summary"));
    context.setActiveCompositionId(QStringLiteral("comp-1"));
    context.setActiveCompositionName(QStringLiteral("Main Comp"));
    context.addSelectedLayer(QStringLiteral("Layer A"));
    context.addCompositionName(QStringLiteral("Comp A"));
    context.setCompositionCount(1);
    context.setTotalLayerCount(4);
    context.setTotalEffectCount(2);
    context.setHeavyCompositionCount(0);
    context.addHeavyCompositionName(QStringLiteral("Heavy"));
    context.logUserAction(ArtifactCore::AIContext::ActionType::SelectLayer,
                          QStringLiteral("layer-1"),
                          QStringLiteral("Selected a layer"));

    const ArtifactCore::AIContext restored = ArtifactCore::AIContext::fromJson(context.toJson());
    report.check(restored.projectSummary() == context.projectSummary(),
                 QStringLiteral("AIContext project summary roundtrips"));
    report.check(restored.activeCompositionId() == context.activeCompositionId(),
                 QStringLiteral("AIContext active composition id roundtrips"));
    report.check(restored.selectedLayers().size() == context.selectedLayers().size(),
                 QStringLiteral("AIContext selected layers roundtrip"));
    report.check(!restored.recentActions().empty() &&
                     restored.recentActions().front().timestampMs > 0,
                 QStringLiteral("AIContext action timestamp is preserved"));

    const QJsonObject initRequest{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), 1},
        {QStringLiteral("method"), QStringLiteral("initialize")},
        {QStringLiteral("params"), QJsonObject{{QStringLiteral("context"), context.toJson()}}}
    };
    const QJsonObject initResponse = ArtifactCore::McpBridge::handleRequest(initRequest, context);
    report.check(initResponse.contains(QStringLiteral("result")),
                 QStringLiteral("MCP initialize returns a result"));
    report.check(initResponse.value(QStringLiteral("result")).toObject()
                     .value(QStringLiteral("context")).toObject()
                     .value(QStringLiteral("projectSummary")).toString() == QStringLiteral("Project Summary"),
                 QStringLiteral("MCP initialize echoes context"));

    const QJsonObject listRequest{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), 2},
        {QStringLiteral("method"), QStringLiteral("tools/list")},
        {QStringLiteral("params"), QJsonObject{{QStringLiteral("context"), context.toJson()}}}
    };
    const QJsonObject listResponse = ArtifactCore::McpBridge::handleRequest(listRequest, context);
    report.check(listResponse.contains(QStringLiteral("result")),
                 QStringLiteral("MCP tools/list returns a result"));
    report.check(!listResponse.value(QStringLiteral("result")).toObject()
                      .value(QStringLiteral("tools")).toArray().isEmpty(),
                 QStringLiteral("MCP tools/list exposes tools"));

    const QJsonObject callRequest{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), 3},
        {QStringLiteral("method"), QStringLiteral("tools/call")},
        {QStringLiteral("params"),
         QJsonObject{
             {QStringLiteral("context"), context.toJson()},
             {QStringLiteral("tool"),
              QJsonObject{{QStringLiteral("class"), QStringLiteral("DummyToolHost")},
                          {QStringLiteral("method"), QStringLiteral("echo")},
                          {QStringLiteral("arguments"), QJsonArray{QStringLiteral("bridge")}}}}
         }}
    };
    const QJsonObject callResponse = ArtifactCore::McpBridge::handleRequest(callRequest, context);
    report.check(callResponse.contains(QStringLiteral("result")),
                 QStringLiteral("MCP tools/call returns a result"));
    report.check(callResponse.value(QStringLiteral("result")).toObject()
                     .value(QStringLiteral("handled")).toBool(),
                 QStringLiteral("MCP tools/call executes the tool"));

    const QVariant workspaceSnapshotVariant =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("workspaceSnapshot"), {});
    const QVariantMap workspaceSnapshot = workspaceSnapshotVariant.toMap();
    report.check(workspaceSnapshot.contains(QStringLiteral("project")),
                 QStringLiteral("workspace snapshot includes project state"));

    const QVariant removalMessageVariant =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("compositionRemovalConfirmationMessage"),
                                                     {QStringLiteral("comp-1")});
    report.check(removalMessageVariant.typeId() == QMetaType::QString,
                 QStringLiteral("composition removal confirmation returns text"));

    const QVariant missingItemVariant =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("findProjectItemById"),
                                                     {QStringLiteral("missing-item")});
    report.check(missingItemVariant.typeId() == QMetaType::QVariantMap,
                 QStringLiteral("project item lookup returns a snapshot map"));

    const QVariant missingItemPathVariant =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("projectItemPathById"),
                                                     {QStringLiteral("missing-item")});
    report.check(missingItemPathVariant.typeId() == QMetaType::QVariantList,
                 QStringLiteral("project item path lookup returns a list"));

    const QVariant createFolderResult =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("createFolderInProject"),
                                                     {QStringLiteral("AI Test Folder"), QString()});
    report.check(createFolderResult.typeId() == QMetaType::Bool,
                 QStringLiteral("folder creation returns a boolean"));

    const QVariant queueJobVariant =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("renderQueueJobByIndex"),
                                                     {0});
    report.check(queueJobVariant.typeId() == QMetaType::QVariantMap,
                 QStringLiteral("render queue job lookup returns a snapshot map"));

    const QVariant queueStatusVariant =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("renderQueueJobStatusAt"),
                                                     {0});
    report.check(queueStatusVariant.typeId() == QMetaType::QString,
                 QStringLiteral("render queue job status lookup returns text"));

    const QVariant queueProgressVariant =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("renderQueueJobProgressAt"),
                                                     {0});
    report.check(queueProgressVariant.typeId() == QMetaType::Int,
                 QStringLiteral("render queue job progress lookup returns an int"));

    const QVariant queueErrorVariant =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("renderQueueJobErrorMessageAt"),
                                                     {0});
    report.check(queueErrorVariant.typeId() == QMetaType::QString,
                 QStringLiteral("render queue job error lookup returns text"));

    const QVariant setQueueOutputSettingsResult =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("setRenderQueueJobOutputSettingsAt"),
                                                     {0, QStringLiteral("png"), QStringLiteral("h264"), QStringLiteral("high"), 1920, 1080, 30.0, 8000});
    report.check(setQueueOutputSettingsResult.typeId() == QMetaType::Bool,
                 QStringLiteral("render queue output settings edit returns a boolean"));

    const QVariant setQueueBackendResult =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("setRenderQueueJobRenderBackendAt"),
                                                     {0, QStringLiteral("auto")});
    report.check(setQueueBackendResult.typeId() == QMetaType::Bool,
                 QStringLiteral("render queue backend edit returns a boolean"));

    const QVariant setQueueAudioPathResult =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("setRenderQueueJobAudioSourcePathAt"),
                                                     {0, QStringLiteral("")});
    report.check(setQueueAudioPathResult.typeId() == QMetaType::Bool,
                 QStringLiteral("render queue audio path edit returns a boolean"));

    const QVariant setQueueAudioCodecResult =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("setRenderQueueJobAudioCodecAt"),
                                                     {0, QStringLiteral("aac")});
    report.check(setQueueAudioCodecResult.typeId() == QMetaType::Bool,
                 QStringLiteral("render queue audio codec edit returns a boolean"));

    const QVariant setQueueAudioBitrateResult =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("setRenderQueueJobAudioBitrateKbpsAt"),
                                                     {0, 128});
    report.check(setQueueAudioBitrateResult.typeId() == QMetaType::Bool,
                 QStringLiteral("render queue audio bitrate edit returns a boolean"));

    const QVariant resetQueueJobResult =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("resetRenderQueueJobForRerun"),
                                                     {0});
    report.check(resetQueueJobResult.typeId() == QMetaType::Bool,
                 QStringLiteral("render queue job rerun reset returns a boolean"));

    const QVariant resetQueueJobsResult =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("resetCompletedAndFailedRenderQueueJobsForRerun"),
                                                     {});
    report.check(resetQueueJobsResult.typeId() == QMetaType::Int,
                 QStringLiteral("render queue batch rerun reset returns an int"));

    const QVariant startQueueResult =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("startRenderQueueAt"),
                                                     {0});
    report.check(startQueueResult.typeId() == QMetaType::Bool,
                 QStringLiteral("render queue job start returns a boolean"));

    const QVariant pauseQueueResult =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("pauseRenderQueueAt"),
                                                     {0});
    report.check(pauseQueueResult.typeId() == QMetaType::Bool,
                 QStringLiteral("render queue job pause returns a boolean"));

    const QVariant cancelQueueResult =
        Artifact::WorkspaceAutomation().invokeMethod(QStringLiteral("cancelRenderQueueAt"),
                                                     {0});
    report.check(cancelQueueResult.typeId() == QMetaType::Bool,
                 QStringLiteral("render queue job cancel returns a boolean"));

    ArtifactCore::CommandSandbox& sandbox = ArtifactCore::CommandSandbox::instance();
    sandbox.resetPolicy();

#ifdef Q_OS_WIN
    const QString whereExecutable = QStandardPaths::findExecutable(QStringLiteral("where.exe"));
    report.check(!whereExecutable.isEmpty(), QStringLiteral("where.exe can be resolved"));
    if (!whereExecutable.isEmpty()) {
        sandbox.setAllowedPrograms(QStringList{whereExecutable});

        const QVariant dryRunResult =
            sandbox.dryRun(QStringLiteral("powershell.exe"),
                           QVariantList{QStringLiteral("-NoProfile"),
                                        QStringLiteral("-Command"),
                                        QStringLiteral("Write-Output sandbox")});
        const QVariantMap dryRunMap = dryRunResult.toMap();
        report.check(!dryRunMap.value(QStringLiteral("allowed")).toBool(),
                     QStringLiteral("shell programs are rejected by policy"));

        const QVariant allowedResult =
            sandbox.run(QStringLiteral("where.exe"),
                        QVariantList{QStringLiteral("/?")});
        const QVariantMap allowedMap = allowedResult.toMap();
        report.check(allowedMap.value(QStringLiteral("allowed")).toBool(),
                     QStringLiteral("allowed command passes policy"));
        report.check(allowedMap.value(QStringLiteral("ok")).toBool(),
                     QStringLiteral("allowed command executes successfully"));
        report.check(!allowedMap.value(QStringLiteral("stdout")).toString().trimmed().isEmpty(),
                     QStringLiteral("allowed command captures output"));
    }
#endif

    const QJsonObject pingRequest{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), 4},
        {QStringLiteral("method"), QStringLiteral("ping")}
    };
    const QJsonObject pingResponse = ArtifactCore::McpBridge::handleRequest(pingRequest, context);
    report.check(pingResponse.contains(QStringLiteral("result")),
                 QStringLiteral("MCP ping responds"));

    qInfo().noquote() << "[AI Test] failures:" << report.failures;
    return report.failures;
}

} // namespace Artifact

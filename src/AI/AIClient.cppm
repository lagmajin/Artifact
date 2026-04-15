/*
 * AIClient.cppm
 *
 * 作成: 2026-04-09
 * 説明: AIクライアントの実装
 */

module;
#include <utility>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <wobjectimpl.h>

module AI.Client;
import Utils.String.UniString;
import Core.AI.PromptGenerator;
import Core.AI.Context;
import Core.AI.ToolBridge;
import Core.AI.ToolExecutor;
import Core.AI.LocalAgent;
import Core.AI.LlamaAgent;
import Core.AI.OnnxDmlAgent;
import Core.AI.CloudAgent;
import Core.AI.TieredManager;
import Artifact.AI.WorkspaceAutomation;
import Artifact.AI.MaterialAutomation;
import Artifact.AI.RenderAutomation;
import Artifact.AI.FileAutomation;
import Artifact.Application.Manager;
import Artifact.Project.Statistics;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;

namespace Artifact {

W_OBJECT_IMPL(AIClient)

class AIClient::Impl {
public:
  UniString apiKey;
  UniString provider = UniString("local");
  LocalAIAgentPtr localAgent;
  ICloudAIAgentPtr cloudAgent;
  bool initialized = false;
  bool initializing = false;
  std::uint64_t generation = 0;
  QString modelPath;
  mutable std::mutex mutex;
  std::atomic<bool> cancellationRequested{false};
};

namespace {

LocalAIAgentPtr createLocalAgentForProvider(const UniString &provider) {
  const QString normalized = provider.toQString().trimmed().toLower();
  if (normalized == QStringLiteral("onnx") ||
      normalized == QStringLiteral("onnx-dml") ||
      normalized == QStringLiteral("onnxdml") ||
      normalized == QStringLiteral("directml")) {
    return std::make_shared<OnnxDmlLocalAgent>();
  }
  return std::make_shared<LlamaLocalAgent>();
}

bool backendProviderChanged(const UniString &current, const UniString &next) {
  return current.toQString().trimmed().toLower() !=
         next.toQString().trimmed().toLower();
}

bool looksLikeToolCallText(const QString &text) {
  const QString lower = text.toLower();
  return lower.contains(QStringLiteral("[tool_call]")) ||
         lower.contains(QStringLiteral("\"tool\"")) ||
         lower.contains(QStringLiteral("\"class\"")) ||
         lower.contains(QStringLiteral("\"component\"")) ||
         lower.contains(QStringLiteral("\"method\"")) ||
         lower.contains(QStringLiteral("tool =>")) ||
         lower.contains(QStringLiteral("args =>")) ||
         lower.contains(QStringLiteral("arguments")) ||
         lower.contains(QStringLiteral("workspaceautomation."));
}

} // namespace

AIClient::AIClient() : impl_(new Impl()) {
  impl_->localAgent = std::make_shared<LlamaLocalAgent>();
}

AIClient::~AIClient() { delete impl_; }

AIClient *AIClient::instance() {
  static AIClient inst;
  return &inst;
}

void AIClient::setApiKey(const UniString &key) { impl_->apiKey = key; }

void AIClient::setProvider(const UniString &provider) {
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    changed = backendProviderChanged(impl_->provider, provider);
    impl_->provider = provider;
  }
  if (changed) {
    shutdown();
  }
}

void AIClient::setCloudAgent(ArtifactCore::ICloudAIAgentPtr agent) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->cloudAgent = agent;
}

void AIClient::setCloudApiKey(const UniString &provider,
                              const UniString &apiKey) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const QString normalizedProvider = provider.toQString().trimmed().toLower();
  if (!impl_->cloudAgent ||
      impl_->cloudAgent->providerName().trimmed().toLower() !=
          normalizedProvider) {
    impl_->cloudAgent = CloudAgentFactory::createByName(provider.toQString());
  }
  if (impl_->cloudAgent) {
    impl_->cloudAgent->setApiKey(apiKey.toQString());
  }
}

namespace {

QString summarizeCurrentProjectState() {
  QStringList lines;
  auto *app = ArtifactApplicationManager::instance();
  auto *projectService = app ? app->projectService() : nullptr;
  auto *selectionManager = app ? app->layerSelectionManager() : nullptr;
  auto project =
      projectService ? projectService->getCurrentProjectSharedPtr() : nullptr;

  if (!project) {
    lines << QStringLiteral("Project: (none)");
    return lines.join('\n');
  }

  const auto stats = ArtifactProjectStatistics::collect(project.get());
  const QString projectName = project->settings().projectName().trimmed();
  lines << QStringLiteral("Project: %1")
               .arg(projectName.isEmpty() ? QStringLiteral("(untitled)")
                                          : projectName);
  lines << QStringLiteral("Compositions: %1").arg(stats.compositionCount);
  lines << QStringLiteral("Total layers: %1").arg(stats.totalLayerCount);
  lines << QStringLiteral("Total effects: %1").arg(stats.totalEffectCount);

  int layerHeavyCompositionCount = 0;
  for (const auto &detail : stats.compositionDetails) {
    const QString detailName = detail.name.trimmed().isEmpty()
                                   ? QStringLiteral("(unnamed)")
                                   : detail.name.trimmed();
    lines << QStringLiteral(
                 "Composition: %1 | layers=%2 | effects=%3 | durationFrames=%4")
                 .arg(detailName)
                 .arg(detail.layerCount)
                 .arg(detail.effectCount)
                 .arg(detail.totalDurationFrames);
    if (detail.layerCount >= 10) {
      ++layerHeavyCompositionCount;
    }
  }
  lines << QStringLiteral("Compositions with 10+ layers: %1")
               .arg(layerHeavyCompositionCount);

  if (auto comp = projectService ? projectService->currentComposition().lock()
                                 : ArtifactCompositionPtr{}) {
    const QString compName =
        comp->settings().compositionName().toQString().trimmed();
    lines << QStringLiteral("Active composition: %1 [%2]")
                 .arg(compName.isEmpty() ? QStringLiteral("(unnamed)")
                                         : compName,
                      comp->id().toString());
  }

  if (selectionManager) {
    const auto selected = selectionManager->selectedLayers();
    lines << QStringLiteral("Selected layers: %1").arg(selected.size());
    int index = 0;
    for (const auto &layer : selected) {
      if (!layer) {
        continue;
      }
      const QString layerName = layer->layerName().trimmed();
      lines << QStringLiteral("Selected layer %1: %2 [%3]")
                   .arg(++index)
                   .arg(layerName.isEmpty() ? QStringLiteral("(unnamed)")
                                            : layerName,
                        layer->id().toString());
    }
  }

  lines << QStringLiteral("Composition details:");
  for (const auto &detail : stats.compositionDetails) {
    lines << QStringLiteral("- %1 | layers=%2 | effects=%3 | durationFrames=%4")
                 .arg(detail.name.isEmpty() ? QStringLiteral("(unnamed)")
                                            : detail.name)
                 .arg(detail.layerCount)
                 .arg(detail.effectCount)
                 .arg(detail.totalDurationFrames);
  }

  return lines.join('\n');
}

bool tryHandleToolCallResponse(const QString &responseText,
                               QString *toolTraceOut,
                               QString *errorOut = nullptr) {
  if (!toolTraceOut) {
    return false;
  }

  QJsonObject toolCall;
  if (!ArtifactCore::ToolBridge::tryParseToolCall(responseText, &toolCall)) {
    if (errorOut) {
      *errorOut = QStringLiteral("Failed to parse tool call");
    }
    return false;
  }

  const auto toolResult = ArtifactCore::ToolBridge::executeToolCall(toolCall);
  *toolTraceOut = toolResult.trace;
  if (!toolResult.handled && errorOut) {
    *errorOut = toolResult.trace;
  }
  return true;
}

QString buildCloudSystemPrompt(const AIContext &context) {
  Artifact::WorkspaceAutomation::ensureRegistered();
  Artifact::MaterialAutomation::ensureRegistered();
  Artifact::RenderAutomation::ensureRegistered();
  Artifact::FileAutomation::ensureRegistered();

  QString prompt =
      AIPromptGenerator::generateSystemPrompt(DescriptionLanguage::Japanese);
  prompt += QStringLiteral("\n\n## Tool Schema\n"
                           "If you need a tool, return a single JSON object "
                           "with keys class, method, arguments.\n"
                           "The app also accepts component as an alias for class, and arguments may be an object or an array.\n"
                           "Do not wrap it in markdown fences.\n");
  prompt += ArtifactCore::ToolBridge::toolSchemaJson();
  prompt += QStringLiteral(
      "\n\n## Workspace Automation Quick Actions\n"
      "- コンポジション作成: WorkspaceAutomation.createComposition(name, "
      "width, height)\n"
      "- 画像レイヤー追加: "
      "WorkspaceAutomation.addImageLayerToCurrentComposition(name, path)\n"
      "- SVG レイヤー追加: "
      "WorkspaceAutomation.addSvgLayerToCurrentComposition(name, path)\n"
      "- 音声レイヤー追加: "
      "WorkspaceAutomation.addAudioLayerToCurrentComposition(name, path)\n"
      "- テキストレイヤー追加: "
      "WorkspaceAutomation.addTextLayerToCurrentComposition(name)\n"
      "- Null / Solid レイヤー追加: "
      "WorkspaceAutomation.addNullLayerToCurrentComposition / "
      "addSolidLayerToCurrentComposition\n"
      "- 新規作成や追加を頼まれたら、まずこれらの tool "
      "を優先して使ってください。\n"
      "- project が空なら createProject を先に使ってから composition "
      "を作成してください。\n");
  prompt +=
      QStringLiteral("\n\n## Live Project Context\n"
                     "Use this snapshot as the current ArtifactStudio state.\n"
                     "```json\n%1\n```\n")
          .arg(context.toJsonString());
  prompt += QStringLiteral(
      "\n\n## Cloud Debug Workflow\n"
      "- If the user asks for an operation, use the available tool schema when "
      "possible.\n"
      "- If the task is a bug investigation, answer with hypothesis, evidence, "
      "files to inspect, and next action.\n"
      "- If information is missing, infer from the live project context "
      "first.\n");
  return prompt;
}

ChatResponse runCloudChatWithToolLoop(ICloudAIAgentPtr cloudAgent,
                                      AIContext context,
                                      const QString &userPrompt,
                                      const QString &model = {},
                                      int maxToolDepth = 2) {
  if (!cloudAgent) {
    return {QString(), QString(), 0,
            0,         false,     QStringLiteral("Cloud AI not configured")};
  }

  QString systemPrompt = buildCloudSystemPrompt(context);
  context.setUserPrompt(userPrompt);
  context.setSystemPrompt(systemPrompt);

  ChatResponse response =
      cloudAgent->chat(systemPrompt, userPrompt, context, model);
  if (!response.success) {
    return response;
  }

  for (int depth = 0; depth < maxToolDepth; ++depth) {
    QString toolTrace;
    QString toolError;
    if (tryHandleToolCallResponse(response.content, &toolTrace, &toolError)) {
      systemPrompt =
          buildCloudSystemPrompt(context) +
          QStringLiteral(
              "\n\nThe previous tool execution produced the following result. "
              "Use it to answer the user:\n%1")
              .arg(toolTrace);
      context.setSystemPrompt(systemPrompt);
      response = cloudAgent->chat(systemPrompt, userPrompt, context, model);
      if (!response.success) {
        return response;
      }
      continue;
    }

    if (!toolError.isEmpty() && looksLikeToolCallText(response.content)) {
      systemPrompt =
          buildCloudSystemPrompt(context) +
          QStringLiteral(
              "\n\nThe previous tool call was invalid. "
              "Please repair it and return a corrected tool call or a final answer.\n"
              "- error: %1\n"
              "- raw response: %2\n")
              .arg(toolError, response.content);
      context.setSystemPrompt(systemPrompt);
      response = cloudAgent->chat(systemPrompt, userPrompt, context, model);
      if (!response.success) {
        return response;
      }
      continue;
    }
  }

  return response;
}

AIContext buildCurrentAIContext() {
  AIContext context = TieredAIManager::instance().globalContext();
  const QString snapshot = summarizeCurrentProjectState();
  context.setProjectSummary(snapshot);

  auto *app = ArtifactApplicationManager::instance();
  auto *projectService = app ? app->projectService() : nullptr;
  auto *selectionManager = app ? app->layerSelectionManager() : nullptr;
  auto project =
      projectService ? projectService->getCurrentProjectSharedPtr() : nullptr;
  if (project) {
    const auto stats = ArtifactProjectStatistics::collect(project.get());
    context.setCompositionCount(stats.compositionCount);
    context.setTotalLayerCount(stats.totalLayerCount);
    context.setTotalEffectCount(stats.totalEffectCount);
    context.clearCompositionNames();

    int heavyCount = 0;
    for (const auto &detail : stats.compositionDetails) {
      const QString detailName = detail.name.trimmed().isEmpty()
                                     ? QStringLiteral("(unnamed)")
                                     : detail.name.trimmed();
      context.addCompositionName(detailName);
      if (detail.layerCount >= 10) {
        ++heavyCount;
        context.addHeavyCompositionName(detailName);
      }
    }
    context.setHeavyCompositionCount(heavyCount);
  } else {
    context.setCompositionCount(0);
    context.setTotalLayerCount(0);
    context.setTotalEffectCount(0);
    context.setHeavyCompositionCount(0);
    context.clearCompositionNames();
    context.clearHeavyCompositionNames();
  }

  if (auto comp = projectService ? projectService->currentComposition().lock()
                                 : ArtifactCompositionPtr{}) {
    const QString compName =
        comp->settings().compositionName().toQString().trimmed();
    context.setActiveCompositionId(comp->id().toString());
    context.setActiveCompositionName(
        compName.isEmpty() ? QStringLiteral("(unnamed)") : compName);
  }
  if (selectionManager) {
    for (const auto &layer : selectionManager->selectedLayers()) {
      if (!layer) {
        continue;
      }
      const QString layerName = layer->layerName().trimmed();
      context.addSelectedLayer(layerName.isEmpty() ? layer->id().toString()
                                                   : layerName);
    }
  }
  return context;
}

bool tryAnswerReadOnlyQuestion(const QString &message, const AIContext &context,
                               QString *answerOut) {
  if (!answerOut) {
    return false;
  }

  const QString q = message.trimmed().toLower();
  if (q.isEmpty()) {
    return false;
  }

  const bool asksCompositionCount =
      (q.contains(QStringLiteral("コンポジション")) &&
       q.contains(QStringLiteral("いくつ"))) ||
      q.contains(QStringLiteral("how many compositions")) ||
      q.contains(QStringLiteral("composition count")) ||
      q.contains(QStringLiteral("コンポ数"));

  if (asksCompositionCount) {
    *answerOut = QStringLiteral("コンポジションは %1 個です。")
                     .arg(context.compositionCount());
    return true;
  }

  const bool asksCompositionList =
      (q.contains(QStringLiteral("コンポジション")) &&
       q.contains(QStringLiteral("一覧"))) ||
      q.contains(QStringLiteral("list compositions")) ||
      q.contains(QStringLiteral("composition list"));
  if (asksCompositionList) {
    const auto names = context.compositionNames();
    if (names.isEmpty()) {
      *answerOut = QStringLiteral("コンポジションはありません。");
    } else {
      QStringList lines;
      lines << QStringLiteral("コンポジション一覧 (%1 個):").arg(names.size());
      for (int i = 0; i < names.size(); ++i) {
        lines << QStringLiteral("%1. %2").arg(i + 1).arg(names.at(i));
      }
      *answerOut = lines.join(QStringLiteral("\n"));
    }
    return true;
  }

  const bool asksHeavyCompositions =
      (q.contains(QStringLiteral("10")) &&
       q.contains(QStringLiteral("レイヤー"))) ||
      q.contains(QStringLiteral("10+")) ||
      q.contains(QStringLiteral("10個以上")) ||
      q.contains(QStringLiteral("10 or more layers")) ||
      q.contains(QStringLiteral("10+ layers"));

  if (asksHeavyCompositions) {
    const auto names = context.heavyCompositionNames();
    if (names.isEmpty()) {
      *answerOut = QStringLiteral(
          "10個以上のレイヤーを持つコンポジションはありません。");
    } else {
      *answerOut = QStringLiteral(
                       "10個以上のレイヤーを持つコンポジションは %1 個です: %2")
                       .arg(context.heavyCompositionCount())
                       .arg(names.join(QStringLiteral(", ")));
    }
    return true;
  }

  const bool asksActiveComposition =
      (q.contains(QStringLiteral("アクティブ")) &&
       q.contains(QStringLiteral("コンポジション"))) ||
      q.contains(QStringLiteral("active composition"));
  if (asksActiveComposition) {
    const QString name = context.activeCompositionName().trimmed();
    const QString id = context.activeCompositionId().trimmed();
    if (name.isEmpty() && id.isEmpty()) {
      *answerOut = QStringLiteral("アクティブなコンポジションはありません。");
    } else if (id.isEmpty()) {
      *answerOut =
          QStringLiteral("アクティブなコンポジションは %1 です。").arg(name);
    } else if (name.isEmpty()) {
      *answerOut =
          QStringLiteral("アクティブなコンポジション ID は %1 です。").arg(id);
    } else {
      *answerOut = QStringLiteral("アクティブなコンポジションは %1 [%2] です。")
                       .arg(name, id);
    }
    return true;
  }

  const bool asksSelectedLayers = (q.contains(QStringLiteral("選択")) &&
                                   q.contains(QStringLiteral("レイヤー"))) ||
                                  q.contains(QStringLiteral("selected layers"));
  if (asksSelectedLayers) {
    const auto selected = context.selectedLayers();
    if (selected.empty()) {
      *answerOut = QStringLiteral("選択中のレイヤーはありません。");
    } else {
      QStringList lines;
      lines << QStringLiteral("選択中のレイヤーは %1 個です。")
                   .arg(selected.size());
      for (int i = 0; i < static_cast<int>(selected.size()); ++i) {
        lines << QStringLiteral("%1. %2").arg(i + 1).arg(
            selected[static_cast<size_t>(i)]);
      }
      *answerOut = lines.join(QStringLiteral("\n"));
    }
    return true;
  }

  return false;
}

void lowerCurrentThreadPriority() {
#ifdef _WIN32
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
}

void updateRecentModelPaths(const QString &path) {
  const QString trimmed = path.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }

  QSettings settings;
  QStringList recentPaths =
      settings.value(QStringLiteral("AI/RecentModelPaths")).toStringList();
  recentPaths.removeAll(trimmed);
  recentPaths.prepend(trimmed);
  while (recentPaths.size() > 10) {
    recentPaths.removeLast();
  }
  settings.setValue(QStringLiteral("AI/RecentModelPaths"), recentPaths);
  settings.setValue(QStringLiteral("AI/ModelPath"), trimmed);
}

} // namespace

bool AIClient::initialize(const QString &modelPath) {
  lowerCurrentThreadPriority();
  QString path = modelPath.trimmed();
  if (path.isEmpty()) {
    QString provider;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      provider = impl_->provider.toQString().trimmed().toLower();
    }
    if (provider == QStringLiteral("onnx") ||
        provider == QStringLiteral("onnx-dml") ||
        provider == QStringLiteral("onnxdml") ||
        provider == QStringLiteral("directml")) {
      path = QStringLiteral("models/onnx/model.onnx");
    } else {
      path = QStringLiteral("models/llama-3.2-1b-instruct.q4_k_m.gguf");
    }
  }
  LocalAIAgentPtr localAgent;
  std::uint64_t loadGeneration = 0;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->initialized) {
      return true;
    }
    if (impl_->initializing) {
      qInfo() << "[AIClient] Local AI backend is already initializing:" << path;
      return false;
    }

    impl_->localAgent = createLocalAgentForProvider(impl_->provider);

    if (impl_->cloudAgent) {
      if (!impl_->cloudAgent->initialize()) {
        qWarning() << "[AIClient] Failed to initialize cloud agent";
      }
    }

    impl_->initializing = true;
    impl_->initialized = false;
    impl_->modelPath = path;
    localAgent = impl_->localAgent;
    loadGeneration = ++impl_->generation;
  }

  updateRecentModelPaths(path);
  const bool loaded = localAgent && localAgent->initialize(path);
  bool acceptResult = false;

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->generation == loadGeneration) {
      acceptResult = true;
      impl_->initializing = false;
      impl_->initialized = loaded;
      if (!loaded) {
        impl_->modelPath.clear();
      }
    }
  }

  if (!acceptResult) {
    qInfo() << "[AIClient] Ignored stale initialization result:" << path;
    return loaded;
  }
  if (loaded) {
    qInfo() << "[AIClient] Local AI backend initialized:" << path
            << "provider=" << impl_->provider.toQString();
  } else {
    QString errorText =
        QStringLiteral("Failed to initialize local AI backend.");
    if (localAgent) {
      const QString detail = localAgent->lastError();
      if (!detail.isEmpty()) {
        errorText = detail;
      }
    }
    qWarning() << "[AIClient]" << errorText << path;
    Q_EMIT this->errorOccurred(errorText);
  }
  Q_EMIT this->initializationFinished(loaded, path);
  return loaded;
}

bool AIClient::isInitialized() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->initialized;
}

bool AIClient::isInitializing() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->initializing;
}

void AIClient::shutdown() {
  QString previousModelPath;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    ++impl_->generation;
    impl_->cancellationRequested.store(true, std::memory_order_relaxed);
    impl_->initializing = false;
    impl_->initialized = false;
    previousModelPath = impl_->modelPath;
    impl_->localAgent.reset();
    impl_->localAgent = createLocalAgentForProvider(impl_->provider);
    impl_->modelPath.clear();
  }
  qDebug() << "[AIClient] Local AI shut down";
  Q_EMIT this->initializationFinished(false, previousModelPath);
}

UniString AIClient::sendMessage(const UniString &message) {
  const QString userPrompt = message.toQString();
  AIContext context = buildCurrentAIContext();

  QString readOnlyAnswer;
  if (tryAnswerReadOnlyQuestion(userPrompt, context, &readOnlyAnswer)) {
    return UniString(readOnlyAnswer);
  }

  LocalAIAgentPtr localAgent;
  ICloudAIAgentPtr cloudAgent;
  bool initialized = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    initialized = impl_->initialized;
    localAgent = impl_->localAgent;
    cloudAgent = impl_->cloudAgent;
  }

  if (initialized && localAgent) {
    const QString systemPrompt =
        AIPromptGenerator::generateSystemPrompt(DescriptionLanguage::Japanese);
    context.setUserPrompt(userPrompt);
    context.setSystemPrompt(systemPrompt);

    const QString response =
        localAgent->generateChatResponse(systemPrompt, userPrompt, context);
    if (!response.isEmpty()) {
      return UniString(response);
    }

    return UniString(QStringLiteral(
        "申し訳ありません。もう少し詳しく教えていただけますか？"));
  }

  if (cloudAgent && cloudAgent->isAvailable()) {
    const ChatResponse response =
        runCloudChatWithToolLoop(cloudAgent, context, userPrompt);
    if (response.success && !response.content.trimmed().isEmpty()) {
      return UniString(response.content);
    }
    if (!response.success && !response.errorMessage.trimmed().isEmpty()) {
      return UniString(QStringLiteral("[AI error] ") + response.errorMessage);
    }
  }

  return UniString(QStringLiteral("[AI unavailable] ") + userPrompt);
}

void AIClient::cancelMessage() {
  impl_->cancellationRequested.store(true, std::memory_order_relaxed);
}

void AIClient::postMessage(const UniString &message) {
  impl_->cancellationRequested.store(false, std::memory_order_relaxed);
  const AIContext baseContext = buildCurrentAIContext();
  std::thread([this, message, baseContext]() {
    lowerCurrentThreadPriority();
    const QString userPrompt = message.toQString();
    AIContext context = baseContext;

    QString readOnlyAnswer;
    if (tryAnswerReadOnlyQuestion(userPrompt, context, &readOnlyAnswer)) {
      QMetaObject::invokeMethod(
          this,
          [this, readOnlyAnswer]() {
            Q_EMIT this->messageReceived(readOnlyAnswer);
          },
          Qt::QueuedConnection);
      return;
    }

    LocalAIAgentPtr localAgent;
    ICloudAIAgentPtr cloudAgent;
    bool initialized = false;
    bool initializing = false;
    std::uint64_t currentGeneration = 0;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      initialized = impl_->initialized;
      initializing = impl_->initializing;
      localAgent = impl_->localAgent;
      cloudAgent = impl_->cloudAgent;
      currentGeneration = impl_->generation;
    }

    // クラウドエージェントが利用可能な場合、クラウドAIを使用する
    if (cloudAgent && cloudAgent->isAvailable()) {
      context = baseContext;
      try {
        auto result = runCloudChatWithToolLoop(cloudAgent, context, userPrompt);
        if (result.success) {
          const QString response = result.content;
          QMetaObject::invokeMethod(
              this,
              [this, response]() { Q_EMIT this->messageReceived(response); },
              Qt::QueuedConnection);
        } else {
          QMetaObject::invokeMethod(
              this,
              [this, errorMsg = result.errorMessage]() {
                Q_EMIT this->errorOccurred(
                    QStringLiteral("Cloud AI error: %1").arg(errorMsg));
                Q_EMIT this->messageReceived(QStringLiteral("[AI error] ") +
                                             errorMsg);
              },
              Qt::QueuedConnection);
        }
      } catch (const std::exception &e) {
        QMetaObject::invokeMethod(
            this,
            [this, errorMsg = e.what()]() {
              Q_EMIT this->errorOccurred(
                  QStringLiteral("Exception: %1").arg(errorMsg));
              Q_EMIT this->messageReceived(QStringLiteral("[AI exception] ") +
                                           errorMsg);
            },
            Qt::QueuedConnection);
      }
      return;
    }

    // ローカルAI処理（既存の実装をそのまま使用）
    if (initializing && !initialized) {
      Q_EMIT this->errorOccurred(
          QStringLiteral("Local model is still loading."));
      Q_EMIT this->partialMessageReceived(QStringLiteral("[AI loading]"));
      return;
    }

    if (initialized && localAgent) {
      context = baseContext;
      context.setUserPrompt(userPrompt);
      context.setSystemPrompt(AIPromptGenerator::generateSystemPrompt(
          DescriptionLanguage::Japanese));

      QString accumulated;
      bool wasCancelled = false;
      const QString response = localAgent->generateChatResponseStreaming(
          context.systemPrompt(), userPrompt, context,
          [this, &accumulated, &wasCancelled,
           currentGeneration](const QString &piece) -> bool {
            if (impl_->cancellationRequested.load(std::memory_order_relaxed)) {
              wasCancelled = true;
              return false;
            }
            accumulated += piece;
            const QString snapshot = accumulated;
            QMetaObject::invokeMethod(
                this,
                [this, snapshot]() {
                  Q_EMIT this->partialMessageReceived(snapshot);
                },
                Qt::QueuedConnection);
            std::lock_guard<std::mutex> lock(impl_->mutex);
            return impl_->generation == currentGeneration;
          });

      if (wasCancelled) {
        QMetaObject::invokeMethod(
            this, [this]() { Q_EMIT this->messageCancelled(); },
            Qt::QueuedConnection);
        return;
      }

      if (response.isEmpty()) {
        QMetaObject::invokeMethod(
            this,
            [this]() {
              Q_EMIT this->errorOccurred(
                  QStringLiteral("AI response was empty."));
              Q_EMIT this->messageReceived(QStringLiteral(
                  "申し訳ありません。応答を生成できませんでした。"));
            },
            Qt::QueuedConnection);
        return;
      }

      QMetaObject::invokeMethod(
          this, [this, response]() { Q_EMIT this->messageReceived(response); },
          Qt::QueuedConnection);
      return;
    }

    const QString fullResponse =
        QStringLiteral("[AI unavailable] ") + userPrompt;
    QMetaObject::invokeMethod(
        this,
        [this, fullResponse]() {
          Q_EMIT this->partialMessageReceived(fullResponse);
          Q_EMIT this->messageReceived(fullResponse);
        },
        Qt::QueuedConnection);
  }).detach();
}

} // namespace Artifact

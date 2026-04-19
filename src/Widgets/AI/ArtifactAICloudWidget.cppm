module;
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMouseEvent>
#include <QEvent>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QSslSocket>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <limits>
#include <wobjectimpl.h>

module Artifact.Widgets.AI.ArtifactAICloudWidget;

import Artifact.Widgets.AI.ArtifactAICloudWidget;
import Artifact.Widgets.AI.ArtifactAICloudSettingsWidget;
import std;
import Core.AI.Context;
import Core.AI.PromptGenerator;
import Core.AI.ToolBridge;
import Core.AI.ToolExecutor;
import Core.AI.McpBridge;
import Core.AI.McpTransport;
import Core.AI.CloudAgent;
import Core.AI.TieredManager;
import Widgets.Utils.CSS;
import Artifact.AI.WorkspaceAutomation;
import Artifact.Application.Manager;
import Artifact.Project.Statistics;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;

namespace Artifact {

W_OBJECT_IMPL(ArtifactAICloudWidget)

namespace {

class CloudChatBubble : public QFrame {
public:
  enum class Role { User, Assistant, System };
  enum class Importance { Normal, Important, Critical };

  explicit CloudChatBubble(Role role, QWidget *parent = nullptr)
      : QFrame(parent), role_(role), importance_(Importance::Normal) {
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(0);
    label_ = new QLabel(this);
    label_->setWordWrap(true);
    label_->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                    Qt::TextSelectableByKeyboard);
    label_->setTextFormat(Qt::PlainText);
    label_->setMaximumWidth(580);
    label_->setAttribute(Qt::WA_TranslucentBackground, true);
    label_->setAutoFillBackground(false);
    QFont textFont = label_->font();
    textFont.setHintingPreference(QFont::PreferFullHinting);
    label_->setFont(textFont);
    layout->addWidget(label_);
    refreshLabelAppearance();
  }

  void setText(const QString &text) {
    text_ = text;
    label_->setText(text);
    updateGeometry();
    update();
  }

  void setRole(Role role) {
    role_ = role;
    refreshLabelAppearance();
    update();
  }

  void setImportance(Importance importance) {
    importance_ = importance;
    refreshLabelAppearance();
    update();
  }

  QString text() const { return text_; }
  Role role() const { return role_; }
  Importance importance() const { return importance_; }

protected:
  void paintEvent(QPaintEvent *event) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor surface(theme.secondaryBackgroundColor);
    const QColor background(theme.backgroundColor);
    const QColor accent(theme.accentColor);
    const QColor textColor(theme.textColor);
    const QColor borderBase(theme.borderColor);

    QColor fill;
    QColor border;
    int borderWidth = 1;
    switch (role_) {
    case Role::User:
      fill = surface.lighter(108);
      border = accent;
      break;
    case Role::Assistant:
      fill = surface;
      border = borderBase;
      break;
    case Role::System:
      fill = background.lighter(108);
      border = accent.darker(125);
      break;
    }

    // Adjust border based on importance
    if (importance_ == Importance::Important) {
      border = accent.lighter(140);
      borderWidth = 2;
    } else if (importance_ == Importance::Critical) {
      border = accent;
      borderWidth = 3;
    }

    painter.fillRect(rect(), fill);

    QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(border, borderWidth));
    painter.setBrush(fill);
    painter.drawRoundedRect(r, 10.0, 10.0);
  }

private:
  void refreshLabelAppearance() {
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor surface(theme.secondaryBackgroundColor);
    const QColor background(theme.backgroundColor);
    const QColor accent(theme.accentColor);
    const QColor text(theme.textColor);
    QColor fill;
    QColor textColor;
    QFont font = label_->font();
    switch (role_) {
    case Role::User:
      fill = surface.lighter(108);
      textColor = text;
      break;
    case Role::Assistant:
      fill = surface;
      textColor = text;
      break;
    case Role::System:
      fill = background.lighter(108);
      textColor = accent.lighter(150);
      break;
    }

    // Adjust text style based on importance
    if (importance_ == Importance::Important) {
      font.setBold(true);
    } else if (importance_ == Importance::Critical) {
      font.setBold(true);
      textColor = accent;
    } else {
      font.setBold(false);
    }

    label_->setFont(font);
    QPalette palette = label_->palette();
    palette.setColor(QPalette::Window, fill);
    palette.setColor(QPalette::Base, fill);
    palette.setColor(QPalette::Text, textColor);
    palette.setColor(QPalette::WindowText, textColor);
    label_->setPalette(palette);
  }

  QLabel *label_ = nullptr;
  Role role_ = Role::Assistant;
  Importance importance_ = Importance::Normal;
  QString text_;
};

struct SectionCard {
  QFrame *frame = nullptr;
  QVBoxLayout *body = nullptr;
};

SectionCard makeSectionCard(QWidget *parent, const QString &title,
                            const QString &subtitle = QString()) {
  auto *frame = new QFrame(parent);
  frame->setFrameShape(QFrame::StyledPanel);
  frame->setFrameShadow(QFrame::Plain);

  auto *outer = new QVBoxLayout(frame);
  outer->setContentsMargins(10, 8, 10, 10);
  outer->setSpacing(4);

  auto *titleLabel = new QLabel(title, frame);
  QFont titleFont = titleLabel->font();
  titleFont.setBold(true);
  if (titleFont.pointSize() > 0) {
    titleFont.setPointSize(titleFont.pointSize() + 1);
  }
  titleLabel->setFont(titleFont);
  outer->addWidget(titleLabel);

  if (!subtitle.isEmpty()) {
    auto *subtitleLabel = new QLabel(subtitle, frame);
    subtitleLabel->setWordWrap(true);
    QFont subtitleFont = subtitleLabel->font();
    if (subtitleFont.pointSize() > 0) {
      subtitleFont.setPointSize(std::max(1, subtitleFont.pointSize() - 1));
    }
    subtitleLabel->setFont(subtitleFont);
    outer->addWidget(subtitleLabel);
  }

  auto *body = new QVBoxLayout();
  body->setContentsMargins(0, 0, 0, 0);
  body->setSpacing(6);
  outer->addLayout(body);

  return {frame, body};
}

bool openCloudSettingsDialog(QWidget *parent) {
  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("Cloud AI Settings"));
  dialog.setModal(true);

  auto *layout = new QVBoxLayout(&dialog);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(10);

  auto *settingsWidget = new Artifact::ArtifactAICloudSettingsWidget(&dialog);
  layout->addWidget(settingsWidget, 1);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
  layout->addWidget(buttons);

  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog,
                   &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog,
                   &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }

  settingsWidget->saveSettings();
  return true;
}

struct CurlResult {
  bool ok = false;
  QByteArray stdoutBytes;
  QString errorText;
};

QStringList buildCurlJsonRequestArgs(const QString &url, const QString &apiKey,
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

CurlResult runCurlJsonRequest(const QString &url, const QString &apiKey,
                              const QByteArray &body,
                              const QStringList &extraArgs = {}) {
  CurlResult result;

  QProcess process;
  process.setProgram(QStringLiteral("curl.exe"));
  process.setArguments(buildCurlJsonRequestArgs(url, apiKey, body, extraArgs));
  process.start();
  if (!process.waitForStarted(10000)) {
    result.errorText = QStringLiteral("Failed to start curl.exe");
    return result;
  }
  if (!body.isEmpty()) {
    process.write(body);
    process.closeWriteChannel();
  }
  if (!process.waitForFinished(30000)) {
    process.kill();
    process.waitForFinished(5000);
    result.errorText = QStringLiteral("curl request timed out");
    return result;
  }

  const QByteArray stderrBytes = process.readAllStandardError();
  const QByteArray stdoutBytes = process.readAllStandardOutput();
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    result.errorText =
        QString::fromUtf8(stderrBytes.isEmpty() ? stdoutBytes : stderrBytes)
            .trimmed();
    if (result.errorText.isEmpty()) {
      result.errorText = QStringLiteral("curl failed");
    }
    return result;
  }

  result.ok = true;
  result.stdoutBytes = stdoutBytes;
  return result;
}

QStringList extractModelIdsFromJson(const QJsonDocument &doc) {
  QStringList ids;
  const auto collectFromArray = [&](const QJsonArray &array) {
    for (const auto &value : array) {
      const QJsonObject item = value.toObject();
      QString id = item.value("id").toString();
      if (id.isEmpty()) {
        id = item.value("name").toString();
      }
      if (id.isEmpty()) {
        id = item.value("slug").toString();
      }
      if (id.isEmpty()) {
        id = item.value("model").toString();
      }
      if (id.isEmpty()) {
        id = item.value("display_name").toString();
      }
      if (id.isEmpty()) {
        id = item.value("title").toString();
      }
      if (!id.isEmpty()) {
        ids.push_back(id);
      }
    }
  };

  if (doc.isObject()) {
    const QJsonObject obj = doc.object();
    if (obj.contains("data") && obj.value("data").isArray()) {
      collectFromArray(obj.value("data").toArray());
    } else if (obj.contains("models") && obj.value("models").isArray()) {
      collectFromArray(obj.value("models").toArray());
    }
  } else if (doc.isArray()) {
    collectFromArray(doc.array());
  }
  return ids;
}

int modelPreferenceScore(const QString &modelId) {
  const QString id = modelId.trimmed().toLower();
  if (id.isEmpty()) {
    return -1000;
  }

  int score = 0;
  auto addIfContains = [&](const QString &needle, int delta) {
    if (id.contains(needle)) {
      score += delta;
    }
  };

  addIfContains(QStringLiteral("free"), 1000);
  addIfContains(QStringLiteral(":free"), 1000);
  addIfContains(QStringLiteral("/free"), 1000);
  addIfContains(QStringLiteral("mini"), 120);
  addIfContains(QStringLiteral("flash-lite"), 110);
  addIfContains(QStringLiteral("flash"), 90);
  addIfContains(QStringLiteral("haiku"), 85);
  addIfContains(QStringLiteral("lite"), 80);
  addIfContains(QStringLiteral("nano"), 75);
  addIfContains(QStringLiteral("small"), 60);
  addIfContains(QStringLiteral("compact"), 55);
  addIfContains(QStringLiteral("base"), 35);
  addIfContains(QStringLiteral("334"), 30);

  addIfContains(QStringLiteral("pro"), -140);
  addIfContains(QStringLiteral("max"), -130);
  addIfContains(QStringLiteral("opus"), -125);
  addIfContains(QStringLiteral("sonnet"), -70);
  addIfContains(QStringLiteral("ultra"), -110);
  addIfContains(QStringLiteral("large"), -90);
  addIfContains(QStringLiteral("reasoning"), -30);

  if (id.startsWith(QStringLiteral("gpt-4o-mini"))) {
    score += 140;
  }
  if (id.startsWith(QStringLiteral("gpt-4.1-mini"))) {
    score += 140;
  }
  if (id.startsWith(QStringLiteral("gpt-3.5"))) {
    score += 60;
  }
  if (id.startsWith(QStringLiteral("gemini-2.0-flash"))) {
    score += 120;
  }
  if (id.startsWith(QStringLiteral("claude-3-haiku"))) {
    score += 120;
  }

  return score;
}

QString choosePreferredModelId(const QStringList &modelIds) {
  QString bestId;
  int bestScore = std::numeric_limits<int>::min();
  for (const QString &id : modelIds) {
    const int score = modelPreferenceScore(id);
    if (score > bestScore) {
      bestScore = score;
      bestId = id.trimmed();
    }
  }
  return bestId;
}

AIContext buildCurrentCloudContext() {
  AIContext context = TieredAIManager::instance().globalContext();

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
    context.clearHeavyCompositionNames();

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

QString buildCloudSystemPrompt(const AIContext &context) {
  Artifact::WorkspaceAutomation::ensureRegistered();

  QString prompt = ArtifactCore::AIPromptGenerator::generateSystemPrompt(
      ArtifactCore::DescriptionLanguage::Japanese);
  prompt += QStringLiteral("\n\n## Tool Schema\n"
                           "If you need a tool, return a single JSON object "
                           "with keys class, method, arguments.\n"
                           "The app also accepts component as an alias for class, and arguments may be an object or an array.\n"
                           "Do not wrap it in markdown fences.\n");
  prompt += QString::fromUtf8(
      ArtifactCore::AIPromptGenerator::generateToolSchemaJson());
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
      "- レイヤー合成モード変更: "
      "WorkspaceAutomation.setLayerBlendModeInCurrentComposition(layerId, blendMode)\n"
      "- レイヤー不透明度変更: "
      "WorkspaceAutomation.setLayerOpacityInCurrentComposition(layerId, opacity)\n"
      "- 新規作成や追加を頼まれたら、まずこれらの tool "
      "を優先して使ってください。\n"
      "- project が空なら createProject を先に使ってから composition "
      "を作成してください。\n");
  prompt +=
      QStringLiteral("\n\n追加の前提:\n"
                     "- ArtifactStudio はモーショングラフィックス / 動画編集 / "
                     "コンポジット / レイヤー編集のためのアプリです。\n"
                     "- "
                     "この会話での「コンポジション」はアプリ内のコンポジション"
                     "を指し、音楽の作曲ではありません。\n"
                     "- "
                     "ユーザーが数を尋ねたら、まずプロジェクト内の状態について"
                     "答えてください。\n"
                     "- 文脈が不足していても、まず ArtifactStudio "
                     "のプロジェクト状態を前提に解釈してください。\n");
  prompt += QStringLiteral(
      "\n\n## Cloud Debug Workflow\n"
      "- "
      "不具合調査や曖昧な相談では、最初に最有力仮説、根拠、確認すべきファイルや"
      "値、最小の次アクションを提示してください。\n"
      "- "
      "足りない情報があれば、質問だけで止まらず、現時点の文脈からの推定を先に返"
      "してください。\n"
      "- 可能なら、どの surface / widget / render path "
      "を見ているかを切り分けて説明してください。\n");
  prompt += QStringLiteral("\n\n## Live Project Context\n"
                           "```json\n%1\n```\n")
                .arg(context.toJsonString());
  return prompt;
}

QString buildMcpPreviewText(const AIContext &context) {
  const QJsonObject initializeRequest{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), 1},
      {QStringLiteral("method"), QStringLiteral("initialize")},
      {QStringLiteral("params"),
       QJsonObject{
           {QStringLiteral("clientInfo"),
            QJsonObject{
                {QStringLiteral("name"), QStringLiteral("ArtifactStudio")},
                {QStringLiteral("version"), QStringLiteral("0.9.0")}}}}}};
  const QJsonObject listRequest{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), 2},
      {QStringLiteral("method"), QStringLiteral("tools/list")},
      {QStringLiteral("params"), QJsonObject{}}};
  const QJsonObject callRequest{
      {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
      {QStringLiteral("id"), 3},
      {QStringLiteral("method"), QStringLiteral("tools/call")},
      {QStringLiteral("params"),
       QJsonObject{
           {QStringLiteral("class"), QStringLiteral("ArtifactProjectService")},
           {QStringLiteral("method"), QStringLiteral("currentComposition")},
           {QStringLiteral("arguments"), QJsonArray{}}}}};

  QStringList sections;
  sections << QStringLiteral("MCP transport preview");
  sections << QStringLiteral("initialize frame:\n%1")
                  .arg(QString::fromUtf8(
                      ArtifactCore::McpBridge::encodeFrame(initializeRequest)));
  sections << QStringLiteral("tools/list frame:\n%1")
                  .arg(QString::fromUtf8(
                      ArtifactCore::McpBridge::encodeFrame(listRequest)));
  sections << QStringLiteral("tools/call frame:\n%1")
                  .arg(QString::fromUtf8(
                      ArtifactCore::McpBridge::encodeFrame(callRequest)));
  sections << QStringLiteral("initialize response example:\n%1")
                  .arg(QString::fromUtf8(ArtifactCore::McpBridge::encodeFrame(
                      ArtifactCore::McpBridge::handleRequest(initializeRequest,
                                                             context))));
  sections << QStringLiteral("tools/list response example:\n%1")
                  .arg(QString::fromUtf8(ArtifactCore::McpBridge::encodeFrame(
                      ArtifactCore::McpBridge::handleRequest(listRequest,
                                                             context))));
  sections << QStringLiteral("tools/call response example:\n%1")
                  .arg(QString::fromUtf8(ArtifactCore::McpBridge::encodeFrame(
                      ArtifactCore::McpBridge::handleRequest(callRequest,
                                                             context))));
  return sections.join(QStringLiteral("\n\n"));
}

QString mcpResultToText(const ArtifactCore::McpCallResult &result) {
  if (result.success) {
    return QString::fromUtf8(
        QJsonDocument(result.response).toJson(QJsonDocument::Indented));
  }
  return QStringLiteral("Error: %1").arg(result.errorText);
}

QStringList extractMcpToolNames(const ArtifactCore::McpCallResult &result) {
  if (!result.success) {
    return {};
  }

  const QJsonValue resultValue =
      result.response.value(QStringLiteral("result"));
  if (!resultValue.isObject()) {
    return {};
  }

  const QJsonArray tools =
      resultValue.toObject().value(QStringLiteral("tools")).toArray();
  QStringList names;
  for (const QJsonValue &value : tools) {
    const QJsonObject tool = value.toObject();
    const QString name =
        tool.value(QStringLiteral("name")).toString().trimmed();
    if (!name.isEmpty()) {
      names.push_back(name);
    }
  }
  return names;
}

CloudChatBubble::Importance detectMessageImportance(const QString &text) {
  const QString lower = text.toLower();
  const QStringList importantKeywords = {
      QStringLiteral("error"), QStringLiteral("failed"), QStringLiteral("warning"),
      QStringLiteral("bug"), QStringLiteral("issue"), QStringLiteral("exception"),
      QStringLiteral("alert"), QStringLiteral("urgent"), QStringLiteral("important"),
      QStringLiteral("エラー"), QStringLiteral("失敗"), QStringLiteral("警告"),
      QStringLiteral("バグ"), QStringLiteral("問題"), QStringLiteral("例外"),
      QStringLiteral("アラート"), QStringLiteral("緊急"), QStringLiteral("重要")
  };
  const QStringList criticalKeywords = {
      QStringLiteral("critical"), QStringLiteral("fatal"), QStringLiteral("severe"),
      QStringLiteral("緊急"), QStringLiteral("致命的"), QStringLiteral("重大")
  };

  int importantCount = 0;
  bool hasCritical = false;
  for (const QString &keyword : importantKeywords) {
    if (lower.contains(keyword)) {
      ++importantCount;
    }
  }
  for (const QString &keyword : criticalKeywords) {
    if (lower.contains(keyword)) {
      hasCritical = true;
      break;
    }
  }

  if (hasCritical || importantCount >= 2) {
    return CloudChatBubble::Importance::Critical;
  } else if (importantCount >= 1) {
    return CloudChatBubble::Importance::Important;
  }
  return CloudChatBubble::Importance::Normal;
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

enum class ToolApprovalMode {
  AskEveryTime = 0,
  AutoApprove = 1,
  YOLO = 2,
};

ToolApprovalMode toolApprovalModeFromIndex(const int index) {
  switch (std::clamp(index, 0, 2)) {
  case 1:
    return ToolApprovalMode::AutoApprove;
  case 2:
    return ToolApprovalMode::YOLO;
  default:
    return ToolApprovalMode::AskEveryTime;
  }
}

int toolApprovalModeToIndex(const ToolApprovalMode mode) {
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

bool isReadOnlyToolCall(const QJsonObject &toolCall) {
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

QString formatToolCallSummary(const QJsonObject &toolCall) {
  const QString className =
      toolCall.value(QStringLiteral("class")).toString().trimmed();
  const QString method =
      toolCall.value(QStringLiteral("method")).toString().trimmed();
  const QJsonValue argsValue = toolCall.value(QStringLiteral("arguments"));
  QString argsText;
  if (argsValue.isArray()) {
    argsText = QString::fromUtf8(
        QJsonDocument(argsValue.toArray()).toJson(QJsonDocument::Compact));
  } else if (argsValue.isObject()) {
    argsText = QString::fromUtf8(
        QJsonDocument(argsValue.toObject()).toJson(QJsonDocument::Compact));
  } else if (!argsValue.isUndefined() && !argsValue.isNull()) {
    argsText = argsValue.toVariant().toString();
  }

  return QStringLiteral("%1.%2(%3)")
      .arg(className.isEmpty() ? QStringLiteral("(unknown)") : className,
           method.isEmpty() ? QStringLiteral("(unknown)") : method,
           argsText.isEmpty() ? QStringLiteral("[]") : argsText);
}

bool requestToolExecutionApproval(QWidget *parent, const QJsonObject &toolCall,
                                  const ToolApprovalMode mode) {
  const bool autoAllowed =
      mode == ToolApprovalMode::YOLO ||
      (mode == ToolApprovalMode::AutoApprove && isReadOnlyToolCall(toolCall));
  if (autoAllowed) {
    return true;
  }

  QMessageBox box(parent);
  box.setIcon(QMessageBox::Question);
  box.setWindowTitle(QStringLiteral("Approve tool execution"));
  box.setText(QStringLiteral("Run this tool call?"));
  box.setInformativeText(formatToolCallSummary(toolCall));
  box.setDetailedText(QString::fromUtf8(
      QJsonDocument(toolCall).toJson(QJsonDocument::Indented)));
  box.setStandardButtons(QMessageBox::NoButton);
  box.setTextInteractionFlags(Qt::TextSelectableByMouse |
                              Qt::TextSelectableByKeyboard);

  auto *approveButton = box.addButton(QStringLiteral("Approve once"),
                                      QMessageBox::AcceptRole);
  auto *autoApproveButton =
      box.addButton(QStringLiteral("Auto-Approve"), QMessageBox::ActionRole);
  auto *yoloButton =
      box.addButton(QStringLiteral("YOLO"), QMessageBox::DestructiveRole);
  auto *denyButton =
      box.addButton(QStringLiteral("Deny"), QMessageBox::RejectRole);

  box.setDefaultButton(approveButton);
  box.exec();

  if (box.clickedButton() == approveButton) {
    return true;
  }
  if (box.clickedButton() == autoApproveButton) {
    QSettings settings(QStringLiteral("ArtifactStudio"),
                       QStringLiteral("AICloud"));
    settings.setValue(QStringLiteral("toolApprovalMode"),
                      toolApprovalModeToIndex(ToolApprovalMode::AutoApprove));
    return true;
  }
  if (box.clickedButton() == yoloButton) {
    QSettings settings(QStringLiteral("ArtifactStudio"),
                       QStringLiteral("AICloud"));
    settings.setValue(QStringLiteral("toolApprovalMode"),
                      toolApprovalModeToIndex(ToolApprovalMode::YOLO));
    return true;
  }
  Q_UNUSED(denyButton);
  return false;
}

} // namespace

void Artifact::ArtifactAICloudWidget::startChatRequest(
    const QString &userPrompt, const QString &systemPrompt,
    const QString &toolTrace) {
  if (!sendProcess_) {
    replaceLastAssistantMessage(
        QStringLiteral("Error: send process unavailable"));
    return;
  }

  if (sendProcess_->state() != QProcess::NotRunning) {
    sendProcess_->kill();
    sendProcess_->waitForFinished(1000);
  }

  pendingUserPrompt_ = userPrompt;
  pendingSystemPrompt_ = systemPrompt;
  pendingToolTrace_ = toolTrace;
  isSending_ = true;
  sendCanceled_ = false;
  updateSendButtonState();

  const QString modelId =
      modelCombo_ ? modelCombo_->currentData().toString().trimmed() : QString();
  QSettings settings(QStringLiteral("ArtifactStudio"),
                     QStringLiteral("AICloud"));
  const QString apiKey =
      settings.value(QStringLiteral("apiKey")).toString().trimmed();
  const QString baseUrl = currentChatCompletionsUrl();

  QJsonObject requestObj;
  requestObj["model"] = modelId;
  QJsonArray messages;
  messages.append(QJsonObject{{"role", "system"}, {"content", systemPrompt}});
  if (!toolTrace.isEmpty()) {
    messages.append(QJsonObject{
        {"role", "system"},
        {"content",
         QStringLiteral(
             "The previous tool execution produced the following result. "
             "Use it to answer the user:\n%1")
             .arg(toolTrace)}});
  }
  messages.append(QJsonObject{{"role", "user"}, {"content", userPrompt}});
  requestObj["messages"] = messages;
  requestObj["stream"] = false;
  requestObj["temperature"] = 0.7;

  const QByteArray body =
      QJsonDocument(requestObj).toJson(QJsonDocument::Compact);

  sendProcess_->setProgram(QStringLiteral("curl.exe"));
  sendProcess_->setArguments(buildCurlJsonRequestArgs(baseUrl, apiKey, body));
  sendProcess_->start();
  if (sendProcess_->state() == QProcess::NotRunning) {
    replaceLastAssistantMessage(
        QStringLiteral("Error: Failed to start curl.exe"));
    isSending_ = false;
    updateSendButtonState();
    return;
  }
  if (!body.isEmpty()) {
    sendProcess_->write(body);
    sendProcess_->closeWriteChannel();
  }
  scrollTranscriptToBottom();
}

bool Artifact::ArtifactAICloudWidget::tryHandleToolCallResponse(
    const QString &responseText, QString *toolTraceOut, QString *errorOut,
    bool *blockedByApprovalOut) {
  if (!toolTraceOut) {
    return false;
  }
  if (blockedByApprovalOut) {
    *blockedByApprovalOut = false;
  }

  QJsonObject toolCall;
  if (!ArtifactCore::ToolBridge::tryParseToolCall(responseText, &toolCall)) {
    if (errorOut) {
      *errorOut = QStringLiteral("Failed to parse tool call");
    }
    return false;
  }

  const ToolApprovalMode approvalMode = toolApprovalModeFromIndex(
      toolApprovalModeCombo_ ? toolApprovalModeCombo_->currentIndex() : 0);
  if (!requestToolExecutionApproval(this, toolCall, approvalMode)) {
    *toolTraceOut = QStringLiteral("Tool call denied by user.");
    if (blockedByApprovalOut) {
      *blockedByApprovalOut = true;
    }
    return true;
  }

  if (toolApprovalModeCombo_) {
    QSettings settings(QStringLiteral("ArtifactStudio"),
                       QStringLiteral("AICloud"));
    const int savedMode =
        settings.value(QStringLiteral("toolApprovalMode"),
                       toolApprovalModeCombo_->currentIndex())
            .toInt();
    const int normalized = std::clamp(savedMode, 0, 2);
    if (toolApprovalModeCombo_->currentIndex() != normalized) {
      toolApprovalModeCombo_->setCurrentIndex(normalized);
    }
  }

  const auto toolResult = ArtifactCore::ToolBridge::executeToolCall(toolCall);
  *toolTraceOut = toolResult.handled
                      ? ArtifactCore::ToolBridge::buildToolTraceMessage(
                            toolCall, toolResult.value)
                      : toolResult.trace;
  if (!toolResult.handled && errorOut) {
    *errorOut = toolResult.trace;
  }
  return true;
}

Artifact::ArtifactAICloudWidget::ArtifactAICloudWidget(QWidget *parent)
    : QWidget(parent), networkManager_(new QNetworkAccessManager(this)),
      modelsNetworkManager_(new QNetworkAccessManager(this)),
      sendProcess_(new QProcess(this)) {
  qDebug() << "[AICloud] supportsSsl=" << QSslSocket::supportsSsl()
           << "backends=" << QSslSocket::availableBackends()
           << "active=" << QSslSocket::activeBackend();

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(10);

  auto *splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setChildrenCollapsible(false);

  auto *leftPanel = new QWidget(splitter);
  auto *leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(10);

  auto *headerFrame = new QFrame(leftPanel);
  headerFrame->setFrameShape(QFrame::StyledPanel);
  headerFrame->setFrameShadow(QFrame::Plain);
  auto *headerLayout = new QVBoxLayout(headerFrame);
  headerLayout->setContentsMargins(10, 8, 10, 8);
  headerLayout->setSpacing(2);
  auto *headerTitle = new QLabel(QStringLiteral("AI Cloud"), headerFrame);
  QFont headerFont = headerTitle->font();
  headerFont.setBold(true);
  if (headerFont.pointSize() > 0) {
    headerFont.setPointSize(headerFont.pointSize() + 2);
  }
  headerTitle->setFont(headerFont);
  auto *headerSubtitle = new QLabel(
      QStringLiteral("VSCode-style cloud assistant panel"), headerFrame);
  headerSubtitle->setWordWrap(true);
  auto *headerHint = new QLabel(
      QStringLiteral(
          "OpenRouter, Kilo Gateway, and OpenAI-compatible endpoints"),
      headerFrame);
  headerHint->setWordWrap(true);
  headerLayout->addWidget(headerTitle);
  headerLayout->addWidget(headerSubtitle);
  headerLayout->addWidget(headerHint);
  leftLayout->addWidget(headerFrame);

  providerCombo_ = new QComboBox(leftPanel);
  providerCombo_->addItem(QStringLiteral("OpenAI"),
                          static_cast<int>(AIProvider::OpenAI));
  providerCombo_->addItem(QStringLiteral("Grok"),
                          static_cast<int>(AIProvider::Grok));
  providerCombo_->addItem(QStringLiteral("OpenRouter"),
                          static_cast<int>(AIProvider::OpenRouter));
  providerCombo_->addItem(QStringLiteral("KiloGateway"),
                          static_cast<int>(AIProvider::KiloGateway));
  providerCombo_->addItem(QStringLiteral("Custom"),
                          static_cast<int>(AIProvider::Custom));
  providerCombo_->setVisible(false);

  baseUrlLabel_ = new QLabel(QStringLiteral("Base URL"), leftPanel);
  baseUrlEdit_ = new QLineEdit(leftPanel);
  baseUrlEdit_->setPlaceholderText(QStringLiteral("https://api.openai.com/v1"));
  baseUrlLabel_->setVisible(false);
  baseUrlEdit_->setVisible(false);

  apiKeyEdit_ = new QLineEdit(leftPanel);
  apiKeyEdit_->setEchoMode(QLineEdit::Password);
  apiKeyEdit_->setPlaceholderText(QStringLiteral("API key / bearer token"));
  apiKeyEdit_->setVisible(false);

  modelFilterEdit_ = new QLineEdit(leftPanel);
  modelFilterEdit_->setPlaceholderText(QStringLiteral("Filter models..."));
  modelFilterEdit_->setClearButtonEnabled(true);
  modelFilterEdit_->setVisible(false);

  modelCombo_ = new QComboBox(leftPanel);
  modelCombo_->setVisible(false);

  auto *advancedToggleRow = new QHBoxLayout();
  advancedToggleRow->setContentsMargins(0, 0, 0, 0);
  advancedToggleRow->setSpacing(6);
  auto *advancedToggle = new QPushButton(QStringLiteral("More"), leftPanel);
  advancedToggle->setCheckable(true);
  advancedToggle->setChecked(false);
  advancedToggle->setFlat(true);
  advancedToggle->setToolTip(QStringLiteral("Show advanced cloud controls"));
  advancedToggle->setMaximumWidth(72);
  advancedToggleRow->addWidget(advancedToggle);
  advancedToggleRow->addStretch();
  leftLayout->addLayout(advancedToggleRow);

  auto *advancedPanel = new QWidget(leftPanel);
  auto *advancedLayout = new QVBoxLayout(advancedPanel);
  advancedLayout->setContentsMargins(0, 0, 0, 0);
  advancedLayout->setSpacing(10);
  advancedPanel->setVisible(false);

  const auto approvalCard = makeSectionCard(
      leftPanel, QStringLiteral("Command Approval"),
      QStringLiteral(
          "Ask before running tool calls, auto-approve read-only calls, or run everything."));
  auto *approvalForm = new QFormLayout();
  approvalForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  approvalForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  approvalForm->setHorizontalSpacing(8);
  approvalForm->setVerticalSpacing(6);
  toolApprovalModeCombo_ = new QComboBox(leftPanel);
  toolApprovalModeCombo_->addItem(QStringLiteral("Ask Every Time"));
  toolApprovalModeCombo_->addItem(QStringLiteral("Auto-Approve"));
  toolApprovalModeCombo_->addItem(QStringLiteral("YOLO"));
  toolApprovalModeCombo_->installEventFilter(this);
  {
    QSettings settings(QStringLiteral("ArtifactStudio"),
                       QStringLiteral("AICloud"));
    const int savedMode =
        settings.value(QStringLiteral("toolApprovalMode"), 0).toInt();
    toolApprovalModeCombo_->setCurrentIndex(std::clamp(savedMode, 0, 2));
  }
  approvalForm->addRow(QStringLiteral("Mode"), toolApprovalModeCombo_);
  approvalCard.body->addLayout(approvalForm);
  auto *approvalHint = new QLabel(
      QStringLiteral(
          "Auto-Approve allows read-only tool calls. YOLO skips confirmation."),
      leftPanel);
  approvalHint->setWordWrap(true);
  approvalCard.body->addWidget(approvalHint);
  advancedLayout->addWidget(approvalCard.frame);

  const auto toolsCard = makeSectionCard(
      leftPanel, QStringLiteral("Tools"),
      QStringLiteral("Registry-derived tool schema and execution hints."));
  auto *toolHeaderRow = new QHBoxLayout();
  toolHeaderRow->setContentsMargins(0, 0, 0, 0);
  toolHeaderRow->setSpacing(6);
  toolCountLabel_ = new QLabel(QStringLiteral("0 tools"), leftPanel);
  toolCountLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  auto *refreshToolsButton =
      new QPushButton(QStringLiteral("Refresh"), leftPanel);
  refreshToolsButton->setFixedWidth(82);
  toolHeaderRow->addWidget(toolCountLabel_);
  toolHeaderRow->addStretch();
  toolHeaderRow->addWidget(refreshToolsButton);
  toolsCard.body->addLayout(toolHeaderRow);

  toolSchemaPreview_ = new QTextEdit(leftPanel);
  toolSchemaPreview_->setReadOnly(true);
  toolSchemaPreview_->setAcceptRichText(false);
  toolSchemaPreview_->setMinimumHeight(180);
  toolSchemaPreview_->setFont(
      QFontDatabase::systemFont(QFontDatabase::FixedFont));
  toolSchemaPreview_->setPlaceholderText(
      QStringLiteral("No tool schema loaded."));
  toolsCard.body->addWidget(toolSchemaPreview_);

  auto *toolLogHeaderRow = new QHBoxLayout();
  toolLogHeaderRow->setContentsMargins(0, 0, 0, 0);
  toolLogHeaderRow->setSpacing(6);
  auto *toolLogLabel = new QLabel(QStringLiteral("Tool Log"), leftPanel);
  auto *clearToolLogButton =
      new QPushButton(QStringLiteral("Clear"), leftPanel);
  clearToolLogButton->setFixedWidth(72);
  toolLogHeaderRow->addWidget(toolLogLabel);
  toolLogHeaderRow->addStretch();
  toolLogHeaderRow->addWidget(clearToolLogButton);
  toolsCard.body->addLayout(toolLogHeaderRow);

  toolLogView_ = new QTextEdit(leftPanel);
  toolLogView_->setReadOnly(true);
  toolLogView_->setAcceptRichText(false);
  toolLogView_->setMinimumHeight(160);
  toolLogView_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  toolLogView_->setPlaceholderText(
      QStringLiteral("Tool execution log will appear here."));
  toolsCard.body->addWidget(toolLogView_);
  advancedLayout->addWidget(toolsCard.frame);

  const auto mcpCard = makeSectionCard(
      leftPanel, QStringLiteral("MCP"),
      QStringLiteral(
          "JSON-RPC framed preview for external transport integration."));
  auto *mcpHeaderRow = new QHBoxLayout();
  mcpHeaderRow->setContentsMargins(0, 0, 0, 0);
  mcpHeaderRow->setSpacing(6);
  auto *mcpLabel = new QLabel(QStringLiteral("Bridge"), leftPanel);
  auto *refreshMcpButton =
      new QPushButton(QStringLiteral("Refresh"), leftPanel);
  refreshMcpButton->setFixedWidth(82);
  mcpHeaderRow->addWidget(mcpLabel);
  mcpHeaderRow->addStretch();
  mcpHeaderRow->addWidget(refreshMcpButton);
  mcpCard.body->addLayout(mcpHeaderRow);

  mcpPreview_ = new QTextEdit(leftPanel);
  mcpPreview_->setReadOnly(true);
  mcpPreview_->setAcceptRichText(false);
  mcpPreview_->setMinimumHeight(220);
  mcpPreview_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  mcpPreview_->setPlaceholderText(
      QStringLiteral("MCP frames will appear here."));
  mcpCard.body->addWidget(mcpPreview_);
  advancedLayout->addWidget(mcpCard.frame);

  const auto transportCard = makeSectionCard(
      leftPanel, QStringLiteral("Transport"),
      QStringLiteral(
          "External stdio process for future MCP-compatible tools."));
  auto *transportForm = new QFormLayout();
  transportForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  transportForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  transportForm->setHorizontalSpacing(8);
  transportForm->setVerticalSpacing(6);
  transportCard.body->addLayout(transportForm);

  mcpProgramEdit_ = new QLineEdit(leftPanel);
  mcpProgramEdit_->setPlaceholderText(QStringLiteral("mcp-server.exe"));
  transportForm->addRow(QStringLiteral("Program"), mcpProgramEdit_);

  mcpArgsEdit_ = new QLineEdit(leftPanel);
  mcpArgsEdit_->setPlaceholderText(QStringLiteral("--stdio"));
  transportForm->addRow(QStringLiteral("Args"), mcpArgsEdit_);
  mcpProgramEdit_->setText(QCoreApplication::applicationFilePath());
  mcpArgsEdit_->setText(QStringLiteral("--mcp-server"));

  mcpStatusLabel_ = new QLabel(QStringLiteral("Stopped"), leftPanel);
  mcpStatusLabel_->setWordWrap(true);
  transportCard.body->addWidget(mcpStatusLabel_);

  auto *transportButtons = new QHBoxLayout();
  transportButtons->setContentsMargins(0, 0, 0, 0);
  transportButtons->setSpacing(6);
  mcpStartButton_ = new QPushButton(QStringLiteral("Start"), leftPanel);
  mcpStopButton_ = new QPushButton(QStringLiteral("Stop"), leftPanel);
  mcpInitializeButton_ =
      new QPushButton(QStringLiteral("Initialize"), leftPanel);
  mcpListToolsButton_ =
      new QPushButton(QStringLiteral("List Tools"), leftPanel);
  mcpPingButton_ = new QPushButton(QStringLiteral("Ping"), leftPanel);
  transportButtons->addWidget(mcpStartButton_);
  transportButtons->addWidget(mcpStopButton_);
  transportButtons->addWidget(mcpInitializeButton_);
  transportButtons->addWidget(mcpListToolsButton_);
  transportButtons->addWidget(mcpPingButton_);
  transportCard.body->addLayout(transportButtons);

  auto *mcpToolForm = new QFormLayout();
  mcpToolForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  mcpToolForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  mcpToolForm->setHorizontalSpacing(8);
  mcpToolForm->setVerticalSpacing(6);
  transportCard.body->addLayout(mcpToolForm);

  mcpToolClassEdit_ = new QLineEdit(leftPanel);
  mcpToolClassEdit_->setPlaceholderText(
      QStringLiteral("ArtifactProjectService"));
  mcpToolForm->addRow(QStringLiteral("Tool Class"), mcpToolClassEdit_);

  mcpToolMethodEdit_ = new QLineEdit(leftPanel);
  mcpToolMethodEdit_->setPlaceholderText(QStringLiteral("currentComposition"));
  mcpToolForm->addRow(QStringLiteral("Tool Method"), mcpToolMethodEdit_);

  mcpToolArgsEdit_ = new QTextEdit(leftPanel);
  mcpToolArgsEdit_->setAcceptRichText(false);
  mcpToolArgsEdit_->setFont(
      QFontDatabase::systemFont(QFontDatabase::FixedFont));
  mcpToolArgsEdit_->setPlaceholderText(
      QStringLiteral("[] or JSON array of arguments"));
  mcpToolArgsEdit_->setMinimumHeight(96);
  mcpToolForm->addRow(QStringLiteral("Arguments"), mcpToolArgsEdit_);

  auto *mcpToolCallRow = new QHBoxLayout();
  mcpToolCallRow->setContentsMargins(0, 0, 0, 0);
  mcpToolCallRow->setSpacing(6);
  mcpToolCallButton_ = new QPushButton(QStringLiteral("Call Tool"), leftPanel);
  mcpToolCallRow->addStretch();
  mcpToolCallRow->addWidget(mcpToolCallButton_);
  transportCard.body->addLayout(mcpToolCallRow);

  auto *mcpSelectorRow = new QHBoxLayout();
  mcpSelectorRow->setContentsMargins(0, 0, 0, 0);
  mcpSelectorRow->setSpacing(6);
  mcpToolSelector_ = new QComboBox(leftPanel);
  mcpToolSelector_->setEditable(true);
  mcpToolSelector_->setInsertPolicy(QComboBox::NoInsert);
  mcpToolSelector_->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
  mcpSelectorRow->addWidget(mcpToolSelector_, 1);
  transportCard.body->addLayout(mcpSelectorRow);

  mcpLogView_ = new QTextEdit(leftPanel);
  mcpLogView_->setReadOnly(true);
  mcpLogView_->setAcceptRichText(false);
  mcpLogView_->setMinimumHeight(140);
  mcpLogView_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  mcpLogView_->setPlaceholderText(
      QStringLiteral("Transport log appears here."));
  transportCard.body->addWidget(mcpLogView_);
  advancedLayout->addWidget(transportCard.frame);
  advancedLayout->addStretch();
  leftLayout->addWidget(advancedPanel, 1);

  connect(advancedToggle, &QPushButton::toggled, this,
          [advancedPanel, advancedToggle](bool checked) {
            advancedPanel->setVisible(checked);
            advancedToggle->setText(checked ? QStringLiteral("Less")
                                            : QStringLiteral("More"));
          });

  auto *rightPanel = new QWidget(splitter);
  auto *rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(10);

  auto *panelControlRow = new QHBoxLayout();
  panelControlRow->setContentsMargins(0, 0, 0, 0);
  panelControlRow->setSpacing(6);
  auto *panelToggleButton =
      new QPushButton(QStringLiteral("Show Cloud Panel"), rightPanel);
  panelToggleButton->setToolTip(
      QStringLiteral("Show or hide the left-side cloud controls panel"));
  panelToggleButton->setCheckable(true);
  panelToggleButton->setChecked(false);
  panelControlRow->addWidget(panelToggleButton);
  panelControlRow->addStretch();
  openSettingsButton_ = new QPushButton(QStringLiteral("Cloud Settings..."),
                                        rightPanel);
  panelControlRow->addWidget(openSettingsButton_);
  rightLayout->addLayout(panelControlRow);

  const auto transcriptCard = makeSectionCard(
      rightPanel, QStringLiteral("Conversation"),
      QStringLiteral(
          "User messages appear on the right, assistant replies on the left."));
  auto *transcriptToolbar = new QHBoxLayout();
  transcriptToolbar->setContentsMargins(0, 0, 0, 0);
  transcriptToolbar->setSpacing(6);
  requestStatusLabel_ =
      new QLabel(QStringLiteral("API request ready"), rightPanel);
  requestStatusLabel_->setWordWrap(true);
  requestStatusLabel_->setVisible(false);
  transcriptToolbar->addWidget(requestStatusLabel_, 1);
  auto *transcriptHint = new QLabel(
      QStringLiteral("Copy the full conversation as plain text."), rightPanel);
  transcriptHint->setWordWrap(true);
  transcriptToolbar->addWidget(transcriptHint, 1);
  transcriptToolbar->addStretch();
  copyTranscriptButton_ =
      new QPushButton(QStringLiteral("Copy Conversation"), rightPanel);
  transcriptToolbar->addWidget(copyTranscriptButton_);
  transcriptCard.body->addLayout(transcriptToolbar);

  transcriptScrollArea_ = new QScrollArea(rightPanel);
  transcriptScrollArea_->setWidgetResizable(true);
  transcriptScrollArea_->setFrameShape(QFrame::StyledPanel);
  transcriptScrollArea_->setFrameShadow(QFrame::Plain);
  transcriptContent_ = new QWidget(transcriptScrollArea_);
  transcriptContent_->setAutoFillBackground(true);
  {
    QPalette tp = transcriptContent_->palette();
    tp.setColor(QPalette::Window, palette().color(QPalette::Window));
    transcriptContent_->setPalette(tp);
  }
  transcriptLayout_ = new QVBoxLayout(transcriptContent_);
  transcriptLayout_->setContentsMargins(8, 8, 8, 8);
  transcriptLayout_->setSpacing(8);
  transcriptLayout_->addStretch(1);
  transcriptScrollArea_->setWidget(transcriptContent_);
  transcriptCard.body->addWidget(transcriptScrollArea_, 1);
  rightLayout->addWidget(transcriptCard.frame, 1);

  loadApiKey();

  const auto promptCard = makeSectionCard(
      rightPanel, QStringLiteral("Composer"),
      QStringLiteral(
          "Write a prompt, then send or cancel from the same button."));
  promptEdit_ = new QTextEdit(rightPanel);
  promptEdit_->setPlaceholderText(QStringLiteral("Enter your prompt here..."));
  promptEdit_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  promptEdit_->setMinimumHeight(140);
  promptEdit_->setAcceptRichText(false);
  promptCard.body->addWidget(promptEdit_);

  auto *sendRow = new QHBoxLayout();
  sendRow->setContentsMargins(0, 0, 0, 0);
  sendRow->setSpacing(6);
  auto *sendHint = new QLabel(
      QStringLiteral("Enter to send, Esc to cancel while generating"),
      rightPanel);
  sendHint->setWordWrap(true);
  sendRow->addWidget(sendHint, 1);
  sendRow->addStretch();
  sendButton_ = new QPushButton(QStringLiteral("Send to AI"), rightPanel);
  sendButton_->setMinimumWidth(120);
  sendButton_->setMinimumHeight(30);
  sendButton_->setEnabled(false);
  connect(sendButton_, &QPushButton::clicked, this,
          &ArtifactAICloudWidget::onSendClicked);
  sendRow->addWidget(sendButton_);
  promptCard.body->addLayout(sendRow);

  auto *modelPickRow = new QHBoxLayout();
  modelPickRow->setContentsMargins(0, 0, 0, 0);
  modelPickRow->setSpacing(6);
  modelPickRow->addStretch();
  modelSelectionLabel_ = new QLabel(rightPanel);
  modelSelectionLabel_->setCursor(Qt::PointingHandCursor);
  modelSelectionLabel_->setTextFormat(Qt::RichText);
  modelSelectionLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
  modelSelectionLabel_->setToolTip(QStringLiteral("Click to choose a model"));
  modelSelectionLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  QFont modelFont = modelSelectionLabel_->font();
  if (modelFont.pointSize() > 0) {
    modelFont.setPointSize(std::max(1, modelFont.pointSize() - 1));
  }
  modelSelectionLabel_->setFont(modelFont);
  QPalette modelPalette = modelSelectionLabel_->palette();
  modelPalette.setColor(QPalette::WindowText, QColor(191, 224, 255));
  modelSelectionLabel_->setPalette(modelPalette);
  modelSelectionLabel_->installEventFilter(this);
  modelPickRow->addWidget(modelSelectionLabel_);
  promptCard.body->addLayout(modelPickRow);
  updateModelSelectionLabel();

  rightLayout->addWidget(promptCard.frame);

  splitter->addWidget(leftPanel);
  splitter->addWidget(rightPanel);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  leftPanel->setVisible(false);
  splitter->setSizes({0, 1200});
  layout->addWidget(splitter, 1);

  connect(panelToggleButton, &QPushButton::toggled, this,
          [splitter, leftPanel, panelToggleButton](bool checked) {
            QSettings settings(QStringLiteral("ArtifactStudio"),
                               QStringLiteral("AICloud"));
            settings.setValue(QStringLiteral("cloudPanelVisible"), checked);
            leftPanel->setVisible(checked);
            panelToggleButton->setText(checked ? QStringLiteral("Hide Cloud Panel")
                                               : QStringLiteral("Show Cloud Panel"));
            splitter->setSizes(checked ? QList<int>{320, 880}
                                       : QList<int>{0, 1200});
          });
  connect(openSettingsButton_, &QPushButton::clicked, this, [this]() {
    if (openCloudSettingsDialog(this)) {
      loadApiKey();
      updateConnectionSummary();
      refreshModelList();
      updateSendButtonState();
    }
  });

  {
    QSettings settings(QStringLiteral("ArtifactStudio"),
                       QStringLiteral("AICloud"));
    const bool showCloudPanel =
        settings.value(QStringLiteral("cloudPanelVisible"), false).toBool();
    panelToggleButton->setChecked(showCloudPanel);
  }

  connect(apiKeyEdit_, &QLineEdit::textChanged, this,
          [this]() { updateSendButtonState(); });
  connect(baseUrlEdit_, &QLineEdit::textChanged, this,
          [this]() { updateSendButtonState(); });
  connect(promptEdit_, &QTextEdit::textChanged, this,
          [this]() { updateSendButtonState(); });
  connect(copyTranscriptButton_, &QPushButton::clicked, this,
          [this]() { copyTranscriptToClipboard(); });
  connect(modelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this]() {
            updateSendButtonState();
            updateModelSelectionLabel();
          });
  connect(modelFilterEdit_, &QLineEdit::textChanged, this,
          [this](const QString &) { applyModelFilter(); });
  connect(promptEdit_, &QTextEdit::textChanged, this,
          [this]() { updateSendButtonState(); });

  promptEdit_->installEventFilter(this);
  modelFilterEdit_->installEventFilter(this);

  connect(providerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ArtifactAICloudWidget::onProviderChanged);

  connect(networkManager_, &QNetworkAccessManager::finished, this,
          &ArtifactAICloudWidget::onApiReply);

  connect(modelsNetworkManager_, &QNetworkAccessManager::finished, this,
          &ArtifactAICloudWidget::onModelsReply);
  connect(refreshToolsButton, &QPushButton::clicked, this, [this]() {
    updateToolSchemaPreview();
    refreshMcpToolSelector();
  });
  connect(refreshMcpButton, &QPushButton::clicked, this, [this]() {
    updateMcpPreview();
    refreshMcpToolSelector();
  });
  connect(mcpStartButton_, &QPushButton::clicked, this, [this]() {
    mcpSession_.setProgram(mcpProgramEdit_ ? mcpProgramEdit_->text().trimmed()
                                           : QString());
    mcpSession_.setArguments(
        mcpArgsEdit_ ? QProcess::splitCommand(mcpArgsEdit_->text().trimmed())
                     : QStringList{});
    if (mcpSession_.start()) {
      appendMcpLog(
          QStringLiteral("Transport started: %1 %2")
              .arg(mcpSession_.program(), mcpSession_.arguments().join(' ')));
      if (mcpStatusLabel_) {
        mcpStatusLabel_->setText(QStringLiteral("Running"));
      }
      const AIContext context = buildCurrentCloudContext();
      const auto initializeResult = mcpSession_.initialize(context);
      appendMcpLog(QStringLiteral("Initialize:\n%1")
                       .arg(mcpResultToText(initializeResult)));
      const auto listResult = mcpSession_.listTools(context);
      appendMcpLog(
          QStringLiteral("tools/list:\n%1").arg(mcpResultToText(listResult)));
      updateMcpPreview();
    } else {
      appendMcpLog(
          QStringLiteral("Start failed: %1").arg(mcpSession_.lastError()));
      if (mcpStatusLabel_) {
        mcpStatusLabel_->setText(
            QStringLiteral("Failed: %1").arg(mcpSession_.lastError()));
      }
    }
  });
  connect(mcpStopButton_, &QPushButton::clicked, this, [this]() {
    mcpSession_.stop();
    appendMcpLog(QStringLiteral("Transport stopped."));
    if (mcpStatusLabel_) {
      mcpStatusLabel_->setText(QStringLiteral("Stopped"));
    }
  });
  connect(mcpInitializeButton_, &QPushButton::clicked, this, [this]() {
    const auto result = mcpSession_.initialize(buildCurrentCloudContext());
    appendMcpLog(
        QStringLiteral("Initialize:\n%1").arg(mcpResultToText(result)));
    if (mcpStatusLabel_) {
      mcpStatusLabel_->setText(
          result.success
              ? QStringLiteral("Initialized")
              : QStringLiteral("Initialize failed: %1").arg(result.errorText));
    }
  });
  connect(mcpListToolsButton_, &QPushButton::clicked, this, [this]() {
    const auto result = mcpSession_.listTools(buildCurrentCloudContext());
    appendMcpLog(
        QStringLiteral("tools/list:\n%1").arg(mcpResultToText(result)));
    const QStringList names = extractMcpToolNames(result);
    if (!names.isEmpty()) {
      refreshMcpToolSelector(names);
    }
    if (mcpStatusLabel_) {
      mcpStatusLabel_->setText(
          result.success
              ? QStringLiteral("Tools listed")
              : QStringLiteral("tools/list failed: %1").arg(result.errorText));
    }
  });
  connect(mcpPingButton_, &QPushButton::clicked, this, [this]() {
    const auto result = mcpSession_.call(
        QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), 99},
                    {QStringLiteral("method"), QStringLiteral("ping")},
                    {QStringLiteral("params"), QJsonObject{}}},
        buildCurrentCloudContext());
    appendMcpLog(QStringLiteral("Ping:\n%1").arg(mcpResultToText(result)));
    if (mcpStatusLabel_) {
      mcpStatusLabel_->setText(
          result.success
              ? QStringLiteral("Running")
              : QStringLiteral("Ping failed: %1").arg(result.errorText));
    }
  });
  connect(mcpToolCallButton_, &QPushButton::clicked, this, [this]() {
    const QString className =
        mcpToolClassEdit_ ? mcpToolClassEdit_->text().trimmed() : QString();
    const QString methodName =
        mcpToolMethodEdit_ ? mcpToolMethodEdit_->text().trimmed() : QString();
    const QString argsText = mcpToolArgsEdit_
                                 ? mcpToolArgsEdit_->toPlainText().trimmed()
                                 : QString();
    QJsonArray argsArray;
    if (!argsText.isEmpty()) {
      QJsonParseError parseError;
      const QJsonDocument argsDoc =
          QJsonDocument::fromJson(argsText.toUtf8(), &parseError);
      if (parseError.error == QJsonParseError::NoError) {
        if (argsDoc.isArray()) {
          argsArray = argsDoc.array();
        } else if (argsDoc.isObject()) {
          argsArray = QJsonArray{argsDoc.object()};
        }
      }
    }
    QJsonObject toolCall{{QStringLiteral("class"), className},
                         {QStringLiteral("method"), methodName},
                         {QStringLiteral("arguments"), argsArray}};
    // Check if this is an external MCP tool call or internal execution
    const bool isExternalMcp = !mcpSession_.program().isEmpty();
    QVariant executionResult;
    if (isExternalMcp) {
      const auto result =
          mcpSession_.callTool(toolCall, buildCurrentCloudContext());
      executionResult = result.success ? QVariant(result.response)
                                       : QVariant(result.errorText);
    } else {
      // Internal execution: execute directly using AIToolExecutor
      executionResult =
          ArtifactCore::AIToolExecutor::instance().execute(toolCall);
    }
    const QJsonValue executionJson = QJsonValue::fromVariant(executionResult);
    QString executionText;
    if (executionJson.isObject()) {
      executionText = QString::fromUtf8(
          QJsonDocument(executionJson.toObject()).toJson(QJsonDocument::Compact));
    } else if (executionJson.isArray()) {
      executionText = QString::fromUtf8(
          QJsonDocument(executionJson.toArray()).toJson(QJsonDocument::Compact));
    } else {
      executionText = executionResult.toString().trimmed();
    }
    appendMcpLog(QStringLiteral("tools/call:\n%1")
                     .arg(executionText.isEmpty() ? QStringLiteral("(empty)")
                                                  : executionText));
    if (mcpStatusLabel_) {
      mcpStatusLabel_->setText(QStringLiteral("tools/call executed"));
    }
  });
  connect(mcpToolSelector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) {
            if (mcpToolSelector_) {
              applySelectedMcpTool(mcpToolSelector_->currentText());
            }
          });
  connect(clearToolLogButton, &QPushButton::clicked, this, [this]() {
    toolLogEntries_.clear();
    if (toolLogView_) {
      toolLogView_->clear();
    }
  });
  connect(sendProcess_, &QProcess::finished, this,
          &ArtifactAICloudWidget::onSendProcessFinished);
  connect(
      sendProcess_, &QProcess::errorOccurred, this,
      [this](QProcess::ProcessError) {
        if (!isSending_) {
          return;
        }
        const QString errorText = sendProcess_
                                      ? sendProcess_->errorString()
                                      : QStringLiteral("Unknown process error");
        replaceLastAssistantMessage(QStringLiteral("Error: %1").arg(errorText));
        isSending_ = false;
        sendCanceled_ = false;
        updateSendButtonState();
      });

  // Initialize model list based on default provider
  updateModelList();
  updateToolSchemaPreview();
  refreshMcpToolSelector();
  updateMcpPreview();
  updateTransportPreview();
  onProviderChanged(providerCombo_->currentIndex());
  updateSendButtonState();
  if (mcpSession_.isRunning()) {
    updateTransportPreview();
  }
}

AIProvider Artifact::ArtifactAICloudWidget::currentProvider() const {
  QSettings settings(QStringLiteral("ArtifactStudio"),
                     QStringLiteral("AICloud"));
  const QString providerName =
      settings.value(QStringLiteral("provider"), QStringLiteral("OpenAI"))
          .toString();
  if (providerName == QStringLiteral("Grok")) {
    return AIProvider::Grok;
  }
  if (providerName == QStringLiteral("OpenRouter")) {
    return AIProvider::OpenRouter;
  }
  if (providerName == QStringLiteral("KiloGateway")) {
    return AIProvider::KiloGateway;
  }
  if (providerName == QStringLiteral("Custom")) {
    return AIProvider::Custom;
  }
  return AIProvider::OpenAI;
}

QString Artifact::ArtifactAICloudWidget::currentBaseUrl() const {
  QSettings settings(QStringLiteral("ArtifactStudio"),
                     QStringLiteral("AICloud"));
  const QString configuredBaseUrl =
      settings.value(QStringLiteral("baseUrl")).toString().trimmed();
  switch (currentProvider()) {
  case AIProvider::OpenAI:
    return QStringLiteral("https://api.openai.com/v1/chat/completions");
  case AIProvider::Grok:
    return QStringLiteral("https://api.x.ai/v1/chat/completions");
  case AIProvider::OpenRouter:
    return QStringLiteral("https://openrouter.ai/api/v1/chat/completions");
  case AIProvider::KiloGateway:
    return configuredBaseUrl;
  case AIProvider::Custom:
    return configuredBaseUrl;
  }
  return QString();
}

QString Artifact::ArtifactAICloudWidget::currentChatCompletionsUrl() const {
  switch (currentProvider()) {
  case AIProvider::OpenAI:
    return QStringLiteral("https://api.openai.com/v1/chat/completions");
  case AIProvider::Grok:
    return QStringLiteral("https://api.x.ai/v1/chat/completions");
  case AIProvider::OpenRouter:
    return QStringLiteral("https://openrouter.ai/api/v1/chat/completions");
  case AIProvider::KiloGateway: {
    const QString base = currentBaseUrl();
    if (base.isEmpty()) {
      return QString();
    }
    QString normalized = base;
    while (normalized.endsWith('/')) {
      normalized.chop(1);
    }
    if (normalized.endsWith(QStringLiteral("/chat/completions"))) {
      return normalized;
    }
    return normalized + QStringLiteral("/chat/completions");
  }
  case AIProvider::Custom:
    return currentBaseUrl();
  }
  return QString();
}

QString Artifact::ArtifactAICloudWidget::currentModelsUrl() const {
  switch (currentProvider()) {
  case AIProvider::OpenRouter:
    return QStringLiteral("https://openrouter.ai/api/v1/models");
  case AIProvider::KiloGateway: {
    const QString base = currentBaseUrl();
    if (base.isEmpty()) {
      return QString();
    }
    QString normalized = base;
    if (normalized.endsWith(QStringLiteral("/chat/completions"))) {
      normalized.chop(QStringLiteral("/chat/completions").size());
    }
    while (normalized.endsWith('/')) {
      normalized.chop(1);
    }
    return normalized + QStringLiteral("/models");
  }
  default:
    return QString();
  }
}

bool Artifact::ArtifactAICloudWidget::providerSupportsRemoteModelList() const {
  switch (currentProvider()) {
  case AIProvider::OpenRouter:
  case AIProvider::KiloGateway:
    return true;
  default:
    return false;
  }
}

void Artifact::ArtifactAICloudWidget::updateModelList() {
  availableModelIds_.clear();

  switch (currentProvider()) {
  case AIProvider::OpenAI:
    availableModelIds_ << QStringLiteral("gpt-4o")
                       << QStringLiteral("gpt-4o-mini")
                       << QStringLiteral("gpt-4-turbo");
    break;
  case AIProvider::Grok:
    availableModelIds_ << QStringLiteral("grok-beta")
                       << QStringLiteral("grok-2")
                       << QStringLiteral("grok-2-mini");
    break;
  case AIProvider::OpenRouter:
    availableModelIds_ << QStringLiteral("anthropic/claude-3.5-sonnet")
                       << QStringLiteral("anthropic/claude-3-sonnet")
                       << QStringLiteral("openai/gpt-4o")
                       << QStringLiteral("openai/gpt-4o-mini")
                       << QStringLiteral("google/gemini-pro-1.5")
                       << QStringLiteral("meta-llama/llama-3-8b-instruct");
    break;
  case AIProvider::KiloGateway:
  case AIProvider::Custom:
    availableModelIds_ << QStringLiteral("custom-model");
    break;
  }
  std::sort(availableModelIds_.begin(), availableModelIds_.end(),
            [](const QString &a, const QString &b) {
              const int sa = modelPreferenceScore(a);
              const int sb = modelPreferenceScore(b);
              if (sa != sb) {
                return sa > sb;
              }
              return a.compare(b, Qt::CaseInsensitive) < 0;
            });
  applyModelFilter();
}

void Artifact::ArtifactAICloudWidget::populateModelList(
    const QStringList &modelIds, const QString &preferredModel) {
  availableModelIds_ = modelIds;
  std::sort(availableModelIds_.begin(), availableModelIds_.end(),
            [](const QString &a, const QString &b) {
              const int sa = modelPreferenceScore(a);
              const int sb = modelPreferenceScore(b);
              if (sa != sb) {
                return sa > sb;
              }
              return a.compare(b, Qt::CaseInsensitive) < 0;
            });
  applyModelFilter(preferredModel);
}

void Artifact::ArtifactAICloudWidget::applyModelFilter(
    const QString &preferredModel) {
  if (!modelCombo_) {
    return;
  }

  const QString filter = modelFilterEdit_
                             ? modelFilterEdit_->text().trimmed().toLower()
                             : QString();
  const QString previous = preferredModel.isEmpty()
                               ? modelCombo_->currentData().toString()
                               : preferredModel;
  const QString preferred =
      previous.isEmpty() ? choosePreferredModelId(availableModelIds_) : previous;
  const int total = availableModelIds_.size();
  int shown = 0;

  modelCombo_->blockSignals(true);
  modelCombo_->clear();

  for (const auto &id : availableModelIds_) {
    const QString trimmed = id.trimmed();
    if (trimmed.isEmpty()) {
      continue;
    }
    if (!filter.isEmpty() && !trimmed.toLower().contains(filter)) {
      continue;
    }
    modelCombo_->addItem(trimmed, trimmed);
    ++shown;
  }

  if (modelCombo_->count() == 0) {
    const QString label =
        filter.isEmpty()
            ? QStringLiteral("(No models available)")
            : QStringLiteral("(No models match \"%1\")").arg(filter);
    modelCombo_->addItem(label, QString());
    modelCombo_->setCurrentIndex(0);
  } else {
    int idx = modelCombo_->findData(preferred);
    if (idx < 0) {
      idx = 0;
    }
    modelCombo_->setCurrentIndex(idx);
  }

  modelCombo_->blockSignals(false);

  if (modelCountLabel_) {
    if (total <= 0) {
      modelCountLabel_->setText(QStringLiteral("0 models"));
    } else if (filter.isEmpty()) {
      modelCountLabel_->setText(QStringLiteral("%1 models").arg(shown));
    } else {
      modelCountLabel_->setText(
          QStringLiteral("%1 / %2").arg(shown).arg(total));
    }
  }

  updateModelSelectionLabel();
  updateSendButtonState();
}

void Artifact::ArtifactAICloudWidget::onProviderChanged(int index) {
  Q_UNUSED(index);
  updateModelList();

  const auto provider = currentProvider();

  // Show/hide base URL input based on provider
  const bool showBaseUrl =
      (provider == AIProvider::KiloGateway || provider == AIProvider::Custom);
  baseUrlEdit_->setVisible(showBaseUrl);
  baseUrlLabel_->setVisible(showBaseUrl);

  // Set appropriate placeholder for base URL
  if (provider == AIProvider::KiloGateway) {
    baseUrlEdit_->setPlaceholderText(
        "https://your-kilogateway-instance.com/v1");
  } else if (provider == AIProvider::Custom) {
    baseUrlEdit_->setPlaceholderText(
        "https://api.example.com/v1/chat/completions");
  }

  updateConnectionSummary();
  refreshModelList();
}

void Artifact::ArtifactAICloudWidget::loadApiKey() {
  QSettings settings("ArtifactStudio", "AICloud");
  apiKeyEdit_->setText(settings.value("apiKey").toString());

  // Load provider
  const QString providerName = settings.value("provider", "OpenAI").toString();
  const int idx = providerCombo_->findText(providerName);
  if (idx >= 0) {
    providerCombo_->setCurrentIndex(idx);
  }

  // Load base URL for custom providers
  baseUrlEdit_->setText(settings.value("baseUrl").toString());
  updateConnectionSummary();
  updateModelSelectionLabel();
}

void Artifact::ArtifactAICloudWidget::saveApiKey() {
  QSettings settings("ArtifactStudio", "AICloud");
  settings.setValue("apiKey", apiKeyEdit_->text());
  settings.setValue("provider", providerCombo_->currentText());
  settings.setValue("baseUrl", baseUrlEdit_->text());
  updateConnectionSummary();
  updateModelSelectionLabel();
}

void Artifact::ArtifactAICloudWidget::updateConnectionSummary() {
  QSettings settings(QStringLiteral("ArtifactStudio"),
                     QStringLiteral("AICloud"));
  const QString providerName =
      settings.value(QStringLiteral("provider"), QStringLiteral("OpenAI"))
          .toString();
  const QString apiKey =
      settings.value(QStringLiteral("apiKey")).toString().trimmed();
  if (connectionProviderLabel_) {
    connectionProviderLabel_->setText(
        QStringLiteral("Provider: %1").arg(providerName));
  }
  if (connectionEndpointLabel_) {
    QString endpoint = currentChatCompletionsUrl();
    if (endpoint.isEmpty()) {
      endpoint = QStringLiteral("(no endpoint)");
    }
    connectionEndpointLabel_->setText(
        QStringLiteral("Endpoint: %1").arg(endpoint));
  }
  if (connectionApiKeyLabel_) {
    const bool hasKey = !apiKey.isEmpty();
    connectionApiKeyLabel_->setText(QStringLiteral("API key: %1")
                                        .arg(hasKey
                                                 ? QStringLiteral("configured")
                                                 : QStringLiteral("not set")));
  }
}

void Artifact::ArtifactAICloudWidget::updateSendButtonState() {
  if (!sendButton_) {
    return;
  }

  if (isSending_) {
    sendButton_->setText(QStringLiteral("Cancel"));
    sendButton_->setEnabled(true);
    if (requestStatusLabel_) {
      requestStatusLabel_->setText(QStringLiteral("API request in progress..."));
      requestStatusLabel_->setVisible(true);
    }
    return;
  }

  QSettings settings(QStringLiteral("ArtifactStudio"),
                     QStringLiteral("AICloud"));
  const bool hasApiKey =
      !settings.value(QStringLiteral("apiKey")).toString().trimmed().isEmpty();
  const bool hasPrompt = !promptEdit_->toPlainText().trimmed().isEmpty();
  const bool hasModel =
      !modelCombo_->currentData().toString().trimmed().isEmpty();
  bool canSend = hasApiKey && hasPrompt && hasModel;
  if (currentProvider() == AIProvider::KiloGateway ||
      currentProvider() == AIProvider::Custom) {
    canSend = canSend && !currentChatCompletionsUrl().isEmpty();
  }
  sendButton_->setEnabled(canSend);
  sendButton_->setText(QStringLiteral("Send to AI"));
  if (requestStatusLabel_) {
    requestStatusLabel_->setVisible(false);
  }
}

void Artifact::ArtifactAICloudWidget::updateToolSchemaPreview() {
  if (toolSchemaPreview_) {
    const QString preview = ArtifactCore::ToolBridge::toolSchemaSummary();
    toolSchemaPreview_->setPlainText(preview);
    toolSchemaPreview_->moveCursor(QTextCursor::Start);
  }

  const QByteArray schemaBytes =
      ArtifactCore::AIPromptGenerator::generateToolSchemaJson();
  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(schemaBytes, &error);
  int count = 0;
  if (error.error == QJsonParseError::NoError && doc.isObject()) {
    count = doc.object().value(QStringLiteral("tools")).toArray().size();
  }

  if (toolCountLabel_) {
    toolCountLabel_->setText(QStringLiteral("%1 tools").arg(count));
  }
}

void Artifact::ArtifactAICloudWidget::updateMcpPreview() {
  if (!mcpPreview_) {
    return;
  }

  const AIContext context = buildCurrentCloudContext();
  mcpPreview_->setPlainText(buildMcpPreviewText(context));
  mcpPreview_->moveCursor(QTextCursor::Start);
}

void Artifact::ArtifactAICloudWidget::refreshMcpToolSelector(
    const QStringList &toolNames) {
  if (!mcpToolSelector_) {
    return;
  }

  QStringList names = toolNames;
  if (names.isEmpty()) {
    const QByteArray schemaBytes =
        ArtifactCore::ToolBridge::toolSchemaJson().toUtf8();
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(schemaBytes, &error);
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
      const QJsonArray tools =
          doc.object().value(QStringLiteral("tools")).toArray();
      for (const QJsonValue &value : tools) {
        const QJsonObject tool = value.toObject();
        const QString toolName = QStringLiteral("%1.%2").arg(
            tool.value(QStringLiteral("component")).toString(),
            tool.value(QStringLiteral("method")).toString());
        if (!toolName.trimmed().isEmpty()) {
          names.push_back(toolName);
        }
      }
    }
  }

  const QString previous = mcpToolSelector_->currentText().trimmed();
  mcpToolSelector_->blockSignals(true);
  mcpToolSelector_->clear();
  for (const QString &name : names) {
    const QString trimmed = name.trimmed();
    if (!trimmed.isEmpty()) {
      mcpToolSelector_->addItem(trimmed);
    }
  }
  if (!previous.isEmpty()) {
    const int idx = mcpToolSelector_->findText(previous);
    if (idx >= 0) {
      mcpToolSelector_->setCurrentIndex(idx);
    } else if (mcpToolSelector_->count() > 0) {
      mcpToolSelector_->setCurrentIndex(0);
    }
  } else if (mcpToolSelector_->count() > 0) {
    mcpToolSelector_->setCurrentIndex(0);
  }
  mcpToolSelector_->blockSignals(false);

  if (mcpToolSelector_->count() > 0) {
    applySelectedMcpTool(mcpToolSelector_->currentText());
  }
}

void Artifact::ArtifactAICloudWidget::applySelectedMcpTool(
    const QString &toolName) {
  const QString trimmed = toolName.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }

  const QByteArray schemaBytes =
      ArtifactCore::ToolBridge::toolSchemaJson().toUtf8();
  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(schemaBytes, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    return;
  }

  const QJsonArray tools =
      doc.object().value(QStringLiteral("tools")).toArray();
  for (const QJsonValue &value : tools) {
    const QJsonObject tool = value.toObject();
    const QString candidateName = QStringLiteral("%1.%2").arg(
        tool.value(QStringLiteral("component")).toString(),
        tool.value(QStringLiteral("method")).toString());
    if (candidateName != trimmed) {
      continue;
    }

    const QString component =
        tool.value(QStringLiteral("component")).toString();
    const QString method = tool.value(QStringLiteral("method")).toString();
    if (mcpToolClassEdit_) {
      mcpToolClassEdit_->setText(component);
    }
    if (mcpToolMethodEdit_) {
      mcpToolMethodEdit_->setText(method);
    }
    if (mcpToolArgsEdit_) {
      mcpToolArgsEdit_->setPlainText(
          ArtifactCore::ToolBridge::buildToolArgumentsTemplate(tool));
    }
    if (mcpStatusLabel_) {
      const QString description =
          tool.value(QStringLiteral("description")).toString().trimmed();
      mcpStatusLabel_->setText(
          description.isEmpty()
              ? QStringLiteral("Selected: %1").arg(candidateName)
              : QStringLiteral("Selected: %1 - %2")
                    .arg(candidateName, description));
    }
    return;
  }
}

void Artifact::ArtifactAICloudWidget::updateTransportPreview() {
  if (!mcpStatusLabel_) {
    return;
  }

  if (mcpSession_.isRunning()) {
    mcpStatusLabel_->setText(
        QStringLiteral("Running: %1 %2")
            .arg(mcpSession_.program(), mcpSession_.arguments().join(' ')));
  } else {
    mcpStatusLabel_->setText(QStringLiteral("Stopped"));
  }
}

void Artifact::ArtifactAICloudWidget::appendToolExecutionLog(
    const QString &entry) {
  const QString timestamp = QDateTime::currentDateTime().toString(
      QStringLiteral("yyyy-MM-dd hh:mm:ss"));
  const QString line =
      QStringLiteral("[%1]\n%2").arg(timestamp, entry.trimmed());
  toolLogEntries_.append(line);
  while (toolLogEntries_.size() > 50) {
    toolLogEntries_.removeFirst();
  }
  if (toolLogView_) {
    toolLogView_->setPlainText(
        toolLogEntries_.join(QStringLiteral("\n\n---\n\n")));
    toolLogView_->moveCursor(QTextCursor::End);
  }
}

void Artifact::ArtifactAICloudWidget::appendMcpLog(const QString &entry) {
  const QString timestamp = QDateTime::currentDateTime().toString(
      QStringLiteral("yyyy-MM-dd hh:mm:ss"));
  const QString line =
      QStringLiteral("[%1] %2").arg(timestamp, entry.trimmed());
  mcpLogEntries_.append(line);
  while (mcpLogEntries_.size() > 50) {
    mcpLogEntries_.removeFirst();
  }
  if (mcpLogView_) {
    mcpLogView_->setPlainText(mcpLogEntries_.join(QStringLiteral("\n")));
    mcpLogView_->moveCursor(QTextCursor::End);
  }
}

void Artifact::ArtifactAICloudWidget::appendTranscriptMessage(
    const QString &role, const QString &text) {
  if (!transcriptLayout_ || !transcriptContent_) {
    return;
  }

  auto *row = new QWidget(transcriptContent_);
  auto *rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->setSpacing(0);

  const QString normalizedRole = role.trimmed().toLower();
  CloudChatBubble::Role bubbleRole = CloudChatBubble::Role::Assistant;
  if (normalizedRole == QStringLiteral("user")) {
    bubbleRole = CloudChatBubble::Role::User;
  } else if (normalizedRole == QStringLiteral("system")) {
    bubbleRole = CloudChatBubble::Role::System;
  }

  auto *bubble = new CloudChatBubble(bubbleRole, row);
  bubble->setText(text);
  // Detect importance for assistant messages
  if (normalizedRole == QStringLiteral("assistant")) {
    const auto importance = detectMessageImportance(text);
    bubble->setImportance(importance);
  }

  if (normalizedRole == QStringLiteral("user")) {
    rowLayout->addStretch(1);
    rowLayout->addWidget(bubble, 0, Qt::AlignRight);
  } else if (normalizedRole == QStringLiteral("system")) {
    rowLayout->addStretch(1);
    rowLayout->addWidget(bubble, 0, Qt::AlignHCenter);
    rowLayout->addStretch(1);
  } else {
    rowLayout->addWidget(bubble, 0, Qt::AlignLeft);
    rowLayout->addStretch(1);
  }

  const int insertIndex = std::max(0, transcriptLayout_->count() - 1);
  transcriptLayout_->insertWidget(insertIndex, row);
  transcriptRows_.push_back(row);
  transcriptBubbles_.push_back(bubble);
  scrollTranscriptToBottom();
}

QString Artifact::ArtifactAICloudWidget::buildTranscriptText() const {
  QStringList lines;
  for (const QWidget *widget : transcriptBubbles_) {
    const auto *bubble = dynamic_cast<const CloudChatBubble *>(widget);
    if (!bubble) {
      continue;
    }
    QString prefix;
    switch (bubble->role()) {
    case CloudChatBubble::Role::User:
      prefix = QStringLiteral("User");
      break;
    case CloudChatBubble::Role::Assistant:
      prefix = QStringLiteral("Assistant");
      break;
    case CloudChatBubble::Role::System:
      prefix = QStringLiteral("System");
      break;
    }
    lines << QStringLiteral("[%1]").arg(prefix) << bubble->text();
    lines << QString();
  }
  return lines.join(QStringLiteral("\n")).trimmed();
}

void Artifact::ArtifactAICloudWidget::copyTranscriptToClipboard() {
  const QString transcript = buildTranscriptText();
  if (transcript.isEmpty()) {
    return;
  }
  if (auto *clipboard = QGuiApplication::clipboard()) {
    clipboard->setText(transcript);
  }
  if (requestStatusLabel_) {
    requestStatusLabel_->setText(QStringLiteral("Conversation copied."));
    requestStatusLabel_->setVisible(true);
  }
}

void Artifact::ArtifactAICloudWidget::updateModelSelectionLabel() {
  if (!modelSelectionLabel_) {
    return;
  }

  QString text = QStringLiteral("<u>Model: ");
  const QString currentModel =
      modelCombo_ ? modelCombo_->currentData().toString().trimmed() : QString();
  if (currentModel.isEmpty()) {
    text += QStringLiteral("Choose...");
  } else {
    text += currentModel.toHtmlEscaped();
  }
  text += QStringLiteral("</u>");
  modelSelectionLabel_->setText(text);
}

void Artifact::ArtifactAICloudWidget::openModelSelectionPopup() {
  if (!modelCombo_ || !modelSelectionLabel_) {
    return;
  }

  auto *popup = new QFrame(this, Qt::Popup | Qt::FramelessWindowHint);
  popup->setAttribute(Qt::WA_DeleteOnClose, true);
  popup->setObjectName(QStringLiteral("ArtifactModelPopup"));
  auto *layout = new QVBoxLayout(popup);
  layout->setContentsMargins(10, 10, 10, 10);
  layout->setSpacing(6);

  auto *combo = new QComboBox(popup);
  combo->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
  combo->setMinimumWidth(std::max(260, modelSelectionLabel_->width() + 36));
  layout->addWidget(combo);

  const QString currentModel =
      modelCombo_->currentData().toString().trimmed();
  for (const QString &id : availableModelIds_) {
    const QString trimmed = id.trimmed();
    if (!trimmed.isEmpty()) {
      combo->addItem(trimmed, trimmed);
    }
  }
  if (combo->count() == 0) {
    combo->addItem(QStringLiteral("(No models available)"), QString());
  } else {
    const int idx = combo->findData(currentModel);
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
  }

  QObject::connect(combo, QOverload<int>::of(&QComboBox::activated), popup,
                   [this, popup, combo](int) {
                     if (!modelCombo_ || !combo) {
                       if (popup) {
                         popup->close();
                       }
                       return;
                     }
                     const QString chosen = combo->currentData().toString().trimmed();
                     if (chosen.isEmpty()) {
                       if (popup) {
                         popup->close();
                       }
                       return;
                     }
                     const int idx = modelCombo_->findData(chosen);
                     if (idx >= 0) {
                       modelCombo_->setCurrentIndex(idx);
                     }
                     updateModelSelectionLabel();
                     updateSendButtonState();
                     popup->close();
                   });

  const QPoint anchor = modelSelectionLabel_->mapToGlobal(
      QPoint(0, modelSelectionLabel_->height() + 2));
  popup->move(anchor);
  popup->show();
  combo->setFocus(Qt::PopupFocusReason);
  combo->showPopup();
}

void Artifact::ArtifactAICloudWidget::replaceLastAssistantMessage(
    const QString &text) {
  if (activeAssistantBubbleIndex_ < 0 ||
      activeAssistantBubbleIndex_ >= transcriptBubbles_.size()) {
    appendTranscriptMessage(QStringLiteral("assistant"), text);
    activeAssistantBubbleIndex_ = transcriptBubbles_.size() - 1;
    return;
  }

  if (auto *bubble = static_cast<CloudChatBubble *>(
          transcriptBubbles_.at(activeAssistantBubbleIndex_))) {
    bubble->setText(text);
    const auto importance = detectMessageImportance(text);
    bubble->setImportance(importance);
    scrollTranscriptToBottom();
    return;
  }

  appendTranscriptMessage(QStringLiteral("assistant"), text);
  activeAssistantBubbleIndex_ = transcriptBubbles_.size() - 1;
}

void Artifact::ArtifactAICloudWidget::scrollTranscriptToBottom() {
  if (!transcriptScrollArea_) {
    return;
  }
  if (auto *bar = transcriptScrollArea_->verticalScrollBar()) {
    bar->setValue(bar->maximum());
  }
  if (transcriptScrollArea_) {
    QTimer::singleShot(0, this, [this]() {
      if (!transcriptScrollArea_) {
        return;
      }
      if (auto *bar = transcriptScrollArea_->verticalScrollBar()) {
        bar->setValue(bar->maximum());
      }
    });
  }
}

void Artifact::ArtifactAICloudWidget::refreshModelList() {
  if (!providerSupportsRemoteModelList()) {
    updateModelList();
    return;
  }

  const QString endpoint = currentModelsUrl();
  QSettings settings(QStringLiteral("ArtifactStudio"),
                     QStringLiteral("AICloud"));
  const QString apiKey =
      settings.value(QStringLiteral("apiKey")).toString().trimmed();
  if (endpoint.isEmpty()) {
    updateModelList();
    return;
  }

  appendTranscriptMessage(QStringLiteral("system"),
                          QStringLiteral("Loading model list..."));
  const CurlResult result = runCurlJsonRequest(endpoint, apiKey, {});
  if (!result.ok) {
    appendTranscriptMessage(
        QStringLiteral("system"),
        QStringLiteral("Model list error: %1").arg(result.errorText));
    updateModelList();
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(result.stdoutBytes);
  if (doc.isNull()) {
    appendTranscriptMessage(QStringLiteral("system"),
                            QStringLiteral("Model list error: invalid JSON"));
    updateModelList();
    return;
  }

  const QStringList ids = extractModelIdsFromJson(doc);
  if (ids.isEmpty()) {
    appendTranscriptMessage(
        QStringLiteral("system"),
        QStringLiteral("Model list loaded, but no models were returned"));
    updateModelList();
    return;
  }

  populateModelList(ids);
  appendTranscriptMessage(QStringLiteral("system"),
                          QStringLiteral("Loaded %1 models").arg(ids.size()));
}

void Artifact::ArtifactAICloudWidget::onSendClicked() {
  qDebug() << "[AICloud] onSendClicked";
  if (isSending_) {
    cancelCurrentSend();
    return;
  }

  QString modelId = modelCombo_->currentData().toString();
  QSettings settings(QStringLiteral("ArtifactStudio"),
                     QStringLiteral("AICloud"));
  QString apiKey =
      settings.value(QStringLiteral("apiKey")).toString().trimmed();
  QString prompt = promptEdit_->toPlainText();
  const AIContext context = buildCurrentCloudContext();
  const QString systemPrompt = buildCloudSystemPrompt(context);
  const QString baseUrl = currentChatCompletionsUrl();

  if (apiKey.isEmpty()) {
    QMessageBox::warning(this, "Error", "API Key is required");
    return;
  }

  if (baseUrl.isEmpty() && (currentProvider() == AIProvider::KiloGateway ||
                            currentProvider() == AIProvider::Custom)) {
    QMessageBox::warning(this, "Error",
                         "Chat completions URL is required for this provider");
    return;
  }

  toolLoopDepth_ = 0;
  appendTranscriptMessage(QStringLiteral("user"), prompt.trimmed());
  promptEdit_->clear();
  appendTranscriptMessage(QStringLiteral("system"),
                          QStringLiteral("API request in progress..."));
  appendTranscriptMessage(QStringLiteral("assistant"),
                          QStringLiteral("API request in progress..."));
  activeAssistantBubbleIndex_ = transcriptBubbles_.size() - 1;
  if (requestStatusLabel_) {
    requestStatusLabel_->setText(QStringLiteral("API request in progress..."));
    requestStatusLabel_->setVisible(true);
  }

  isSending_ = true;
  sendCanceled_ = false;
  updateSendButtonState();
  startChatRequest(prompt, systemPrompt);
}

void Artifact::ArtifactAICloudWidget::cancelCurrentSend() {
  if (!isSending_) {
    return;
  }

  sendCanceled_ = true;
  if (sendProcess_ && sendProcess_->state() != QProcess::NotRunning) {
    sendProcess_->kill();
  }
  if (requestStatusLabel_) {
    requestStatusLabel_->setText(QStringLiteral("Cancelling API request..."));
    requestStatusLabel_->setVisible(true);
  }
  appendTranscriptMessage(QStringLiteral("system"),
                          QStringLiteral("API request canceled."));
  replaceLastAssistantMessage(QStringLiteral("Request canceled."));
}

bool Artifact::ArtifactAICloudWidget::eventFilter(QObject *watched,
                                                  QEvent *event) {
  if (watched == modelSelectionLabel_ && event) {
    if (event->type() == QEvent::MouseButtonRelease) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() == Qt::LeftButton) {
        openModelSelectionPopup();
        return true;
      }
    }
  }

  if (watched == promptEdit_ && event && event->type() == QEvent::KeyPress) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Escape && isSending_) {
      cancelCurrentSend();
      return true;
    }
    if (!isSending_ &&
        (keyEvent->key() == Qt::Key_Return ||
         keyEvent->key() == Qt::Key_Enter) &&
        !(keyEvent->modifiers() & Qt::ShiftModifier)) {
      onSendClicked();
      return true;
    }
  }

  if (watched == modelFilterEdit_ && event &&
      event->type() == QEvent::KeyPress) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Escape) {
      modelFilterEdit_->clear();
      return true;
    }
  }

  if (watched == toolApprovalModeCombo_ && event &&
      (event->type() == QEvent::FocusOut || event->type() == QEvent::Hide)) {
    QSettings settings(QStringLiteral("ArtifactStudio"),
                       QStringLiteral("AICloud"));
    settings.setValue(QStringLiteral("toolApprovalMode"),
                      toolApprovalModeCombo_
                          ? toolApprovalModeCombo_->currentIndex()
                          : 0);
  }

  return QWidget::eventFilter(watched, event);
}

void Artifact::ArtifactAICloudWidget::onSendProcessFinished(
    int exitCode, QProcess::ExitStatus exitStatus) {
  Q_UNUSED(exitCode);
  Q_UNUSED(exitStatus);

  const QByteArray stderrBytes = sendProcess_->readAllStandardError();
  const QByteArray stdoutBytes = sendProcess_->readAllStandardOutput();
  isSending_ = false;
  updateSendButtonState();

  if (sendCanceled_) {
    sendCanceled_ = false;
    return;
  }

  const QString output = QString::fromUtf8(stdoutBytes).trimmed();
  if (output.isEmpty()) {
    const QString errorText = QString::fromUtf8(stderrBytes).trimmed();
    replaceLastAssistantMessage(
        errorText.isEmpty() ? QStringLiteral("Error: empty response")
                            : QStringLiteral("Error: %1").arg(errorText));
    pendingToolTrace_.clear();
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(stdoutBytes);
  if (doc.isNull()) {
    replaceLastAssistantMessage(QStringLiteral("Invalid JSON response"));
    pendingToolTrace_.clear();
    return;
  }

  QJsonObject obj = doc.object();
  if (obj.contains("choices") && !obj["choices"].toArray().isEmpty()) {
    QJsonObject choice =
        obj["choices"].toArray()[0].toObject()["message"].toObject();
    QString content = choice["content"].toString();
    const QString finalContent =
        content.isEmpty() ? QStringLiteral("No response content") : content;

    QString toolTrace;
    QString toolError;
    bool toolBlockedByApproval = false;
    if (toolLoopDepth_ < 2 &&
        tryHandleToolCallResponse(finalContent, &toolTrace, &toolError,
                                  &toolBlockedByApproval)) {
      if (toolBlockedByApproval) {
        appendTranscriptMessage(QStringLiteral("system"), toolTrace);
        appendToolExecutionLog(toolTrace);
        replaceLastAssistantMessage(QStringLiteral("Tool call denied."));
        pendingToolTrace_.clear();
        return;
      }
      ++toolLoopDepth_;
      pendingToolTrace_ = toolTrace;
      appendTranscriptMessage(QStringLiteral("system"), toolTrace);
      appendToolExecutionLog(toolTrace);
      replaceLastAssistantMessage(
          QStringLiteral("Tool executed, asking for final answer..."));
      QTimer::singleShot(0, this, [this]() {
        startChatRequest(pendingUserPrompt_, pendingSystemPrompt_,
                         pendingToolTrace_);
      });
      return;
    }

    if (toolLoopDepth_ < 2 && !toolError.isEmpty() &&
        looksLikeToolCallText(finalContent)) {
      ++toolLoopDepth_;
      const QString repairTrace = QStringLiteral(
          "Tool call error:\n- error: %1\n- raw response: %2")
                                     .arg(toolError, finalContent);
      pendingToolTrace_ = repairTrace;
      appendTranscriptMessage(QStringLiteral("system"), repairTrace);
      appendToolExecutionLog(repairTrace);
      replaceLastAssistantMessage(
          QStringLiteral("Tool call invalid, asking for repair..."));
      QTimer::singleShot(0, this, [this]() {
        startChatRequest(pendingUserPrompt_, pendingSystemPrompt_,
                         pendingToolTrace_);
      });
      return;
    }

    replaceLastAssistantMessage(finalContent);
    pendingToolTrace_.clear();
  } else if (obj.contains("error")) {
    QString errorMsg = obj["error"].toObject()["message"].toString();
    replaceLastAssistantMessage("API Error: " + errorMsg);
    pendingToolTrace_.clear();
  } else {
    replaceLastAssistantMessage("No response content");
    pendingToolTrace_.clear();
  }
}

void Artifact::ArtifactAICloudWidget::onApiReply(QNetworkReply *reply) {
  updateSendButtonState();
  if (reply) {
    reply->deleteLater();
  }
}

void Artifact::ArtifactAICloudWidget::onModelsReply(QNetworkReply *reply) {
  if (!reply) {
    return;
  }

  if (reply->error() != QNetworkReply::NoError) {
    appendTranscriptMessage(
        QStringLiteral("system"),
        QStringLiteral("Model list error: %1").arg(reply->errorString()));
    reply->deleteLater();
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
  if (doc.isNull()) {
    appendTranscriptMessage(QStringLiteral("system"),
                            QStringLiteral("Model list error: invalid JSON"));
    reply->deleteLater();
    return;
  }

  QStringList ids;
  const QJsonObject obj = doc.object();
  const auto collectFromArray = [&](const QJsonArray &array) {
    for (const auto &value : array) {
      const QJsonObject item = value.toObject();
      QString id = item.value("id").toString();
      if (id.isEmpty()) {
        id = item.value("name").toString();
      }
      if (id.isEmpty()) {
        id = item.value("slug").toString();
      }
      if (id.isEmpty()) {
        id = item.value("model").toString();
      }
      if (id.isEmpty()) {
        id = item.value("display_name").toString();
      }
      if (id.isEmpty()) {
        id = item.value("title").toString();
      }
      if (!id.isEmpty()) {
        ids.push_back(id);
      }
    }
  };

  if (obj.contains("data") && obj.value("data").isArray()) {
    collectFromArray(obj.value("data").toArray());
  } else if (obj.contains("models") && obj.value("models").isArray()) {
    collectFromArray(obj.value("models").toArray());
  } else if (doc.isArray()) {
    collectFromArray(doc.array());
  }

  if (ids.isEmpty()) {
    appendTranscriptMessage(
        QStringLiteral("system"),
        QStringLiteral("Model list loaded, but no models were returned"));
    updateModelList();
  } else {
    populateModelList(ids);
    appendTranscriptMessage(QStringLiteral("system"),
                            QStringLiteral("Loaded %1 models").arg(ids.size()));
  }

  reply->deleteLater();
}

} // namespace Artifact

module;
#include <QComboBox>
#include <QApplication>
#include <QEvent>
#include <QScrollBar>
#include <QPainter>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFontDatabase>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QKeyEvent>
#include <QPushButton>
#include <QSslSocket>
#include <QSettings>
#include <QSplitter>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <wobjectimpl.h>


module Artifact.Widgets.AI.ArtifactAICloudWidget;

import Artifact.Widgets.AI.ArtifactAICloudWidget;
import std;
import Core.AI.PromptGenerator;
import Core.AI.CloudAgent;

namespace Artifact {

W_OBJECT_IMPL(ArtifactAICloudWidget)

namespace {

class CloudChatBubble : public QFrame {
public:
  enum class Role { User, Assistant, System };

  explicit CloudChatBubble(Role role, QWidget *parent = nullptr)
      : QFrame(parent), role_(role) {
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(0);
    label_ = new QLabel(this);
    label_->setWordWrap(true);
    label_->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                    Qt::TextSelectableByKeyboard);
    label_->setTextFormat(Qt::PlainText);
    label_->setMaximumWidth(580);
    layout->addWidget(label_);
  }

  void setText(const QString &text) {
    text_ = text;
    label_->setText(text);
    updateGeometry();
    update();
  }

  void setRole(Role role) {
    role_ = role;
    update();
  }

  QString text() const { return text_; }

protected:
  void paintEvent(QPaintEvent *event) override {
    QFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QRectF r = rect().adjusted(1, 1, -1, -1);

    QColor fill;
    QColor border;
    switch (role_) {
    case Role::User:
      fill = QColor("#1f4e79");
      border = QColor("#4aa3ff");
      break;
    case Role::Assistant:
      fill = QColor("#23272f");
      border = QColor("#3b424d");
      break;
    case Role::System:
      fill = QColor("#35302a");
      border = QColor("#6a5d3d");
      break;
    }

    painter.setPen(QPen(border, 1));
    painter.setBrush(fill);
    painter.drawRoundedRect(r, 10.0, 10.0);
  }

private:
  QLabel *label_ = nullptr;
  Role role_ = Role::Assistant;
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
    result.errorText = QString::fromUtf8(stderrBytes.isEmpty()
                                            ? stdoutBytes
                                            : stderrBytes)
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

QString buildCloudSystemPrompt()
{
  QString prompt = ArtifactCore::AIPromptGenerator::generateSystemPrompt(
      ArtifactCore::DescriptionLanguage::Japanese);
  prompt += QStringLiteral(
      "\n\n追加の前提:\n"
      "- ArtifactStudio はモーショングラフィックス / 動画編集 / コンポジット / レイヤー編集のためのアプリです。\n"
      "- この会話での「コンポジション」はアプリ内のコンポジションを指し、音楽の作曲ではありません。\n"
      "- ユーザーが数を尋ねたら、まずプロジェクト内の状態について答えてください。\n"
      "- 文脈が不足していても、まず ArtifactStudio のプロジェクト状態を前提に解釈してください。\n");
  return prompt;
}

} // namespace

Artifact::ArtifactAICloudWidget::ArtifactAICloudWidget(QWidget *parent)
    : QWidget(parent),
      networkManager_(new QNetworkAccessManager(this)),
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
  auto *headerSubtitle =
      new QLabel(QStringLiteral("VSCode-style cloud assistant panel"),
                 headerFrame);
  headerSubtitle->setWordWrap(true);
  auto *headerHint = new QLabel(
      QStringLiteral("OpenRouter, Kilo Gateway, and OpenAI-compatible endpoints"),
      headerFrame);
  headerHint->setWordWrap(true);
  headerLayout->addWidget(headerTitle);
  headerLayout->addWidget(headerSubtitle);
  headerLayout->addWidget(headerHint);
  leftLayout->addWidget(headerFrame);

  const auto connectionCard = makeSectionCard(
      leftPanel, QStringLiteral("Connection"),
      QStringLiteral("Provider, endpoint, and API key settings."));
  auto *connectionForm = new QFormLayout();
  connectionForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  connectionForm->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  connectionForm->setHorizontalSpacing(8);
  connectionForm->setVerticalSpacing(6);
  connectionCard.body->addLayout(connectionForm);

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
  connectionForm->addRow(QStringLiteral("Provider"), providerCombo_);

  baseUrlLabel_ = new QLabel(QStringLiteral("Base URL"), leftPanel);
  baseUrlEdit_ = new QLineEdit(leftPanel);
  baseUrlEdit_->setPlaceholderText(QStringLiteral("https://api.openai.com/v1"));
  connectionForm->addRow(baseUrlLabel_, baseUrlEdit_);

  apiKeyEdit_ = new QLineEdit(leftPanel);
  apiKeyEdit_->setEchoMode(QLineEdit::Password);
  apiKeyEdit_->setPlaceholderText(QStringLiteral("API key / bearer token"));
  connectionForm->addRow(QStringLiteral("API Key"), apiKeyEdit_);

  leftLayout->addWidget(connectionCard.frame);

  const auto modelCard = makeSectionCard(
      leftPanel, QStringLiteral("Model"),
      QStringLiteral("Search and pick a remote model from the list."));
  auto *filterRow = new QHBoxLayout();
  filterRow->setContentsMargins(0, 0, 0, 0);
  filterRow->setSpacing(6);
  modelFilterEdit_ = new QLineEdit(leftPanel);
  modelFilterEdit_->setPlaceholderText(QStringLiteral("Filter models..."));
  modelFilterEdit_->setClearButtonEnabled(true);
  modelCountLabel_ = new QLabel(QStringLiteral("0 models"), leftPanel);
  modelCountLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  filterRow->addWidget(modelFilterEdit_, 1);
  filterRow->addWidget(modelCountLabel_);
  modelCard.body->addLayout(filterRow);

  auto *modelRow = new QHBoxLayout();
  modelRow->setContentsMargins(0, 0, 0, 0);
  modelRow->setSpacing(6);
  modelCombo_ = new QComboBox(leftPanel);
  modelRow->addWidget(modelCombo_, 1);
  auto *refreshModelsButton = new QPushButton(QStringLiteral("Reload"), leftPanel);
  refreshModelsButton->setFixedWidth(74);
  modelRow->addWidget(refreshModelsButton);
  modelCard.body->addLayout(modelRow);
  leftLayout->addWidget(modelCard.frame);

  leftLayout->addStretch();

  auto *rightPanel = new QWidget(splitter);
  auto *rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(10);

  const auto transcriptCard = makeSectionCard(
      rightPanel, QStringLiteral("Conversation"),
      QStringLiteral("User messages appear on the right, assistant replies on the left."));
  transcriptScrollArea_ = new QScrollArea(rightPanel);
  transcriptScrollArea_->setWidgetResizable(true);
  transcriptScrollArea_->setFrameShape(QFrame::StyledPanel);
  transcriptScrollArea_->setFrameShadow(QFrame::Plain);
  transcriptContent_ = new QWidget(transcriptScrollArea_);
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
      QStringLiteral("Write a prompt, then send or cancel from the same button."));
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
      QStringLiteral("Enter to send, Esc to cancel while generating"), rightPanel);
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
  rightLayout->addWidget(promptCard.frame);

  splitter->addWidget(leftPanel);
  splitter->addWidget(rightPanel);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({360, 840});
  layout->addWidget(splitter, 1);

  connect(apiKeyEdit_, &QLineEdit::textChanged, this, [this]() {
    updateSendButtonState();
  });
  connect(baseUrlEdit_, &QLineEdit::textChanged, this, [this]() {
    updateSendButtonState();
  });
  connect(promptEdit_, &QTextEdit::textChanged, this, [this]() {
    updateSendButtonState();
  });
  connect(modelCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this]() { updateSendButtonState(); });
  connect(modelFilterEdit_, &QLineEdit::textChanged, this,
          [this](const QString &) { applyModelFilter(); });
  connect(promptEdit_, &QTextEdit::textChanged, this, [this]() {
    updateSendButtonState();
  });

  promptEdit_->installEventFilter(this);
  modelFilterEdit_->installEventFilter(this);

  connect(providerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ArtifactAICloudWidget::onProviderChanged);

  connect(networkManager_, &QNetworkAccessManager::finished, this,
          &ArtifactAICloudWidget::onApiReply);

  connect(modelsNetworkManager_, &QNetworkAccessManager::finished, this,
          &ArtifactAICloudWidget::onModelsReply);
  connect(refreshModelsButton, &QPushButton::clicked, this,
          &ArtifactAICloudWidget::refreshModelList);
  connect(sendProcess_, &QProcess::finished, this,
          &ArtifactAICloudWidget::onSendProcessFinished);
  connect(sendProcess_, &QProcess::errorOccurred, this,
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
  onProviderChanged(providerCombo_->currentIndex());
  updateSendButtonState();
}

AIProvider Artifact::ArtifactAICloudWidget::currentProvider() const {
  return static_cast<AIProvider>(providerCombo_->currentData().toInt());
}

QString Artifact::ArtifactAICloudWidget::currentBaseUrl() const {
  switch (currentProvider()) {
  case AIProvider::OpenAI:
    return QStringLiteral("https://api.openai.com/v1/chat/completions");
  case AIProvider::Grok:
    return QStringLiteral("https://api.x.ai/v1/chat/completions");
  case AIProvider::OpenRouter:
    return QStringLiteral("https://openrouter.ai/api/v1/chat/completions");
  case AIProvider::KiloGateway:
    return baseUrlEdit_->text().trimmed();
  case AIProvider::Custom:
    return baseUrlEdit_->text().trimmed();
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
    return baseUrlEdit_->text().trimmed();
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
  applyModelFilter();
}

void Artifact::ArtifactAICloudWidget::populateModelList(const QStringList &modelIds,
                                                       const QString &preferredModel) {
  availableModelIds_ = modelIds;
  applyModelFilter(preferredModel);
}

void Artifact::ArtifactAICloudWidget::applyModelFilter(const QString &preferredModel) {
  if (!modelCombo_) {
    return;
  }

  const QString filter = modelFilterEdit_ ? modelFilterEdit_->text().trimmed().toLower()
                                          : QString();
  const QString previous = preferredModel.isEmpty()
                               ? modelCombo_->currentData().toString()
                               : preferredModel;
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
    const QString label = filter.isEmpty()
                              ? QStringLiteral("(No models available)")
                              : QStringLiteral("(No models match \"%1\")").arg(filter);
    modelCombo_->addItem(label, QString());
    modelCombo_->setCurrentIndex(0);
  } else {
    int idx = modelCombo_->findData(previous);
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
      modelCountLabel_->setText(QStringLiteral("%1 / %2").arg(shown).arg(total));
    }
  }

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
}

void Artifact::ArtifactAICloudWidget::saveApiKey() {
  QSettings settings("ArtifactStudio", "AICloud");
  settings.setValue("apiKey", apiKeyEdit_->text());
  settings.setValue("provider", providerCombo_->currentText());
  settings.setValue("baseUrl", baseUrlEdit_->text());
}

void Artifact::ArtifactAICloudWidget::updateSendButtonState() {
  if (!sendButton_) {
    return;
  }

  if (isSending_) {
    sendButton_->setText(QStringLiteral("Cancel"));
    sendButton_->setEnabled(true);
    return;
  }

  const bool hasApiKey = !apiKeyEdit_->text().trimmed().isEmpty();
  const bool hasPrompt = !promptEdit_->toPlainText().trimmed().isEmpty();
  const bool hasModel = !modelCombo_->currentData().toString().trimmed().isEmpty();
  bool canSend = hasApiKey && hasPrompt && hasModel;
  if (currentProvider() == AIProvider::KiloGateway ||
      currentProvider() == AIProvider::Custom) {
    canSend = canSend && !currentChatCompletionsUrl().isEmpty();
  }
  sendButton_->setEnabled(canSend);
  sendButton_->setText(QStringLiteral("Send to AI"));
}

void Artifact::ArtifactAICloudWidget::appendTranscriptMessage(const QString &role,
                                                             const QString &text) {
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

void Artifact::ArtifactAICloudWidget::replaceLastAssistantMessage(const QString &text) {
  if (activeAssistantBubbleIndex_ < 0 ||
      activeAssistantBubbleIndex_ >= transcriptBubbles_.size()) {
    appendTranscriptMessage(QStringLiteral("assistant"), text);
    activeAssistantBubbleIndex_ = transcriptBubbles_.size() - 1;
    return;
  }

  if (auto *bubble = static_cast<CloudChatBubble *>(
          transcriptBubbles_.at(activeAssistantBubbleIndex_))) {
    bubble->setText(text);
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
}

void Artifact::ArtifactAICloudWidget::refreshModelList() {
  if (!providerSupportsRemoteModelList()) {
    updateModelList();
    return;
  }

  const QString endpoint = currentModelsUrl();
  const QString apiKey = apiKeyEdit_->text().trimmed();
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
    appendTranscriptMessage(QStringLiteral("system"),
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
  saveApiKey();

  QString modelId = modelCombo_->currentData().toString();
  QString apiKey = apiKeyEdit_->text();
  QString prompt = promptEdit_->toPlainText();
  QString baseUrl = currentChatCompletionsUrl();

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

  QJsonObject requestObj;
  requestObj["model"] = modelId;
  QJsonArray messages;
  messages.append(QJsonObject{{"role", "system"},
                              {"content", buildCloudSystemPrompt()}});
  messages.append(QJsonObject{{"role", "user"}, {"content", prompt}});
  requestObj["messages"] = messages;
  requestObj["stream"] = false;
  requestObj["temperature"] = 0.7;

  const QByteArray body = QJsonDocument(requestObj).toJson(QJsonDocument::Compact);
  appendTranscriptMessage(QStringLiteral("user"), prompt.trimmed());
  promptEdit_->clear();
  appendTranscriptMessage(QStringLiteral("assistant"), QStringLiteral("Thinking..."));
  activeAssistantBubbleIndex_ = transcriptBubbles_.size() - 1;

  isSending_ = true;
  sendCanceled_ = false;
  updateSendButtonState();

  sendProcess_->setProgram(QStringLiteral("curl.exe"));
  sendProcess_->setArguments(
      buildCurlJsonRequestArgs(baseUrl, apiKey, body));
  sendProcess_->start();
  if (sendProcess_->state() == QProcess::NotRunning) {
    replaceLastAssistantMessage(QStringLiteral("Error: Failed to start curl.exe"));
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

void Artifact::ArtifactAICloudWidget::cancelCurrentSend() {
  if (!isSending_) {
    return;
  }

  sendCanceled_ = true;
  if (sendProcess_ && sendProcess_->state() != QProcess::NotRunning) {
    sendProcess_->kill();
  }
  replaceLastAssistantMessage(QStringLiteral("Request canceled."));
}

bool Artifact::ArtifactAICloudWidget::eventFilter(QObject *watched,
                                                  QEvent *event) {
  if (watched == promptEdit_ && event && event->type() == QEvent::KeyPress) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Escape && isSending_) {
      cancelCurrentSend();
      return true;
    }
    if (!isSending_ &&
        (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
        !(keyEvent->modifiers() & Qt::ShiftModifier)) {
      onSendClicked();
      return true;
    }
  }

  if (watched == modelFilterEdit_ && event && event->type() == QEvent::KeyPress) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Escape) {
      modelFilterEdit_->clear();
      return true;
    }
  }

  return QWidget::eventFilter(watched, event);
}

void Artifact::ArtifactAICloudWidget::onSendProcessFinished(int exitCode,
                                                           QProcess::ExitStatus exitStatus) {
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
    replaceLastAssistantMessage(errorText.isEmpty()
                                    ? QStringLiteral("Error: empty response")
                                    : QStringLiteral("Error: %1").arg(errorText));
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(stdoutBytes);
  if (doc.isNull()) {
    replaceLastAssistantMessage(QStringLiteral("Invalid JSON response"));
    return;
  }

  QJsonObject obj = doc.object();
  if (obj.contains("choices") && !obj["choices"].toArray().isEmpty()) {
    QJsonObject choice =
        obj["choices"].toArray()[0].toObject()["message"].toObject();
    QString content = choice["content"].toString();
    replaceLastAssistantMessage(content.isEmpty()
                                    ? QStringLiteral("No response content")
                                    : content);
  } else if (obj.contains("error")) {
    QString errorMsg = obj["error"].toObject()["message"].toString();
    replaceLastAssistantMessage("API Error: " + errorMsg);
  } else {
    replaceLastAssistantMessage("No response content");
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
    appendTranscriptMessage(QStringLiteral("system"),
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
    appendTranscriptMessage(QStringLiteral("system"),
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

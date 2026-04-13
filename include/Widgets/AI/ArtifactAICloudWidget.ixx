module;
#include <QComboBox>
#include <QHBoxLayout>
#include <QFrame>
#include <QDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QProcess>
#include <QSettings>
#include <QScrollArea>
#include <QVector>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QStringList>
#include <wobjectdefs.h>

export module Artifact.Widgets.AI.ArtifactAICloudWidget;

import std;
import Core.AI.McpTransport;

export namespace Artifact {

enum class AIProvider { OpenAI, Grok, OpenRouter, KiloGateway, Custom };

class ArtifactAICloudWidget : public QWidget {
  W_OBJECT(ArtifactAICloudWidget)
public:
  ArtifactAICloudWidget(QWidget *parent = nullptr);

private:
  void onSendClicked();
  void onSendProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void onApiReply(QNetworkReply *reply);
  void onModelsReply(QNetworkReply *reply);
  void onProviderChanged(int index);
  void refreshModelList();
  void cancelCurrentSend();
  void startChatRequest(const QString &userPrompt, const QString &systemPrompt,
                        const QString &toolTrace = QString());
  bool tryHandleToolCallResponse(const QString &responseText,
                                 QString *toolTraceOut,
                                 QString *errorOut = nullptr);
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void loadApiKey();
  void saveApiKey();
  void updateModelList();
  void updateSendButtonState();
  void updateToolSchemaPreview();
  void updateMcpPreview();
  void updateTransportPreview();
  void updateConnectionSummary();
  void refreshMcpToolSelector(const QStringList &toolNames = QStringList());
  void applySelectedMcpTool(const QString &toolName);
  void appendToolExecutionLog(const QString &entry);
  void appendMcpLog(const QString &entry);
  void copyTranscriptToClipboard();
  void openModelSelectionPopup();
  QString buildTranscriptText() const;
  void applyModelFilter(const QString &preferredModel = QString());
  void populateModelList(const QStringList &modelIds, const QString &preferredModel = QString());
  void updateModelSelectionLabel();
  void appendTranscriptMessage(const QString &role, const QString &text);
  void replaceLastAssistantMessage(const QString &text);
  void scrollTranscriptToBottom();
  AIProvider currentProvider() const;
  QString currentBaseUrl() const;
  QString currentChatCompletionsUrl() const;
  QString currentModelsUrl() const;
  bool providerSupportsRemoteModelList() const;

  QComboBox *providerCombo_;
  QComboBox *modelCombo_;
  QLineEdit *modelFilterEdit_;
  QLineEdit *apiKeyEdit_;
  QLineEdit *baseUrlEdit_;
  QLabel *baseUrlLabel_;
  QLabel *modelCountLabel_ = nullptr;
  QLabel *modelSelectionLabel_ = nullptr;
  QLabel *connectionProviderLabel_ = nullptr;
  QLabel *connectionEndpointLabel_ = nullptr;
  QLabel *connectionApiKeyLabel_ = nullptr;
  QLabel *toolCountLabel_ = nullptr;
  QPushButton *openSettingsButton_ = nullptr;
  QScrollArea *transcriptScrollArea_ = nullptr;
  QWidget *transcriptContent_ = nullptr;
  QVBoxLayout *transcriptLayout_ = nullptr;
  QTextEdit *toolSchemaPreview_ = nullptr;
  QTextEdit *toolLogView_ = nullptr;
  QTextEdit *mcpPreview_ = nullptr;
  QLineEdit *mcpProgramEdit_ = nullptr;
  QLineEdit *mcpArgsEdit_ = nullptr;
  QLabel *mcpStatusLabel_ = nullptr;
  QTextEdit *mcpLogView_ = nullptr;
  QPushButton *mcpStartButton_ = nullptr;
  QPushButton *mcpStopButton_ = nullptr;
  QPushButton *mcpInitializeButton_ = nullptr;
  QPushButton *mcpListToolsButton_ = nullptr;
  QPushButton *mcpPingButton_ = nullptr;
  QComboBox *mcpToolSelector_ = nullptr;
  QLineEdit *mcpToolClassEdit_ = nullptr;
  QLineEdit *mcpToolMethodEdit_ = nullptr;
  QTextEdit *mcpToolArgsEdit_ = nullptr;
  QPushButton *mcpToolCallButton_ = nullptr;
  QTextEdit *promptEdit_ = nullptr;
  QPushButton *sendButton_ = nullptr;
  QLabel *requestStatusLabel_ = nullptr;
  QPushButton *copyTranscriptButton_ = nullptr;
  bool isSending_ = false;
  bool sendCanceled_ = false;
  int toolLoopDepth_ = 0;
  int activeAssistantBubbleIndex_ = -1;
  QString pendingUserPrompt_;
  QString pendingSystemPrompt_;
  QString pendingToolTrace_;
  QVector<QWidget *> transcriptRows_;
  QVector<QWidget *> transcriptBubbles_;
  QProcess *sendProcess_;
  QStringList availableModelIds_;
  QStringList toolLogEntries_;
  QStringList mcpLogEntries_;
  QStringList mcpToolNames_;
  ArtifactCore::McpTransportSession mcpSession_;
  QNetworkAccessManager *networkManager_;
  QNetworkAccessManager *modelsNetworkManager_;
};

} // namespace Artifact

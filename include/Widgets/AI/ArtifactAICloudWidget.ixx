module;
#include <QComboBox>
#include <QHBoxLayout>
#include <QFrame>
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
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void loadApiKey();
  void saveApiKey();
  void updateModelList();
  void updateSendButtonState();
  void applyModelFilter(const QString &preferredModel = QString());
  void populateModelList(const QStringList &modelIds, const QString &preferredModel = QString());
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
  QLabel *modelCountLabel_;
  QScrollArea *transcriptScrollArea_;
  QWidget *transcriptContent_;
  QVBoxLayout *transcriptLayout_;
  QTextEdit *promptEdit_;
  QPushButton *sendButton_;
  bool isSending_ = false;
  bool sendCanceled_ = false;
  int activeAssistantBubbleIndex_ = -1;
  QVector<QWidget *> transcriptRows_;
  QVector<QWidget *> transcriptBubbles_;
  QProcess *sendProcess_;
  QStringList availableModelIds_;
  QNetworkAccessManager *networkManager_;
  QNetworkAccessManager *modelsNetworkManager_;
};

} // namespace Artifact

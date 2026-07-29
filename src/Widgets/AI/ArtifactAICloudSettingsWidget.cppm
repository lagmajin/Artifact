module;
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QSettings>

module Artifact.Widgets.AI.ArtifactAICloudSettingsWidget;

import std;

namespace Artifact {

ArtifactAICloudSettingsWidget::ArtifactAICloudSettingsWidget(QWidget *parent)
    : QWidget(parent) {
  setAccessibleName(QStringLiteral("Cloud AI settings"));
  setAccessibleDescription(QStringLiteral("Configure the cloud AI provider, endpoint, and API key"));
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(10);

  auto *card = new QGroupBox(QStringLiteral("Cloud AI Settings"), this);
  auto *cardLayout = new QVBoxLayout(card);

  auto *hint = new QLabel(
      QStringLiteral("Store provider, endpoint, and API key separately from the chat view."),
      card);
  hint->setAccessibleName(QStringLiteral("Cloud AI settings description"));
  hint->setWordWrap(true);
  cardLayout->addWidget(hint);

  auto *form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  form->setHorizontalSpacing(8);
  form->setVerticalSpacing(6);
  cardLayout->addLayout(form);

  providerCombo_ = new QComboBox(card);
  providerCombo_->setAccessibleName(QStringLiteral("Cloud AI provider"));
  providerCombo_->setAccessibleDescription(QStringLiteral("Choose the cloud AI service provider"));
  providerCombo_->addItem(QStringLiteral("OpenAI"), static_cast<int>(AICloudProvider::OpenAI));
  providerCombo_->addItem(QStringLiteral("Grok"), static_cast<int>(AICloudProvider::Grok));
  providerCombo_->addItem(QStringLiteral("OpenRouter"), static_cast<int>(AICloudProvider::OpenRouter));
  providerCombo_->addItem(QStringLiteral("KiloGateway"), static_cast<int>(AICloudProvider::KiloGateway));
  providerCombo_->addItem(QStringLiteral("Custom"), static_cast<int>(AICloudProvider::Custom));
  form->addRow(QStringLiteral("Provider"), providerCombo_);

  baseUrlLabel_ = new QLabel(QStringLiteral("Base URL"), card);
  baseUrlEdit_ = new QLineEdit(card);
  baseUrlEdit_->setAccessibleName(QStringLiteral("Cloud AI base URL"));
  baseUrlEdit_->setAccessibleDescription(QStringLiteral("Endpoint URL for custom or gateway providers"));
  baseUrlEdit_->setPlaceholderText(QStringLiteral("https://api.openai.com/v1"));
  form->addRow(baseUrlLabel_, baseUrlEdit_);

  apiKeyEdit_ = new QLineEdit(card);
  apiKeyEdit_->setAccessibleName(QStringLiteral("Cloud AI API key"));
  apiKeyEdit_->setAccessibleDescription(QStringLiteral("API key or bearer token used for cloud AI requests"));
  apiKeyEdit_->setEchoMode(QLineEdit::Password);
  apiKeyEdit_->setPlaceholderText(QStringLiteral("API key / bearer token"));
  form->addRow(QStringLiteral("API Key"), apiKeyEdit_);

  cardLayout->addWidget(new QLabel(
      QStringLiteral("OpenAI / Grok / OpenRouter can keep their defaults; KiloGateway and Custom expose the base URL field."),
      card));

  mainLayout->addWidget(card);
  mainLayout->addStretch(1);

  connect(providerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ArtifactAICloudSettingsWidget::onProviderChanged);

  loadSettings();
  updateBaseUrlVisibility();
}

AICloudProvider ArtifactAICloudSettingsWidget::provider() const {
  return static_cast<AICloudProvider>(providerCombo_->currentData().toInt());
}

QString ArtifactAICloudSettingsWidget::providerName() const {
  switch (provider()) {
  case AICloudProvider::OpenAI:
    return QStringLiteral("OpenAI");
  case AICloudProvider::Grok:
    return QStringLiteral("Grok");
  case AICloudProvider::OpenRouter:
    return QStringLiteral("OpenRouter");
  case AICloudProvider::KiloGateway:
    return QStringLiteral("KiloGateway");
  case AICloudProvider::Custom:
    return QStringLiteral("Custom");
  }
  return QStringLiteral("OpenAI");
}

QString ArtifactAICloudSettingsWidget::apiKey() const {
  return apiKeyEdit_ ? apiKeyEdit_->text().trimmed() : QString();
}

QString ArtifactAICloudSettingsWidget::baseUrl() const {
  return baseUrlEdit_ ? baseUrlEdit_->text().trimmed() : QString();
}

void ArtifactAICloudSettingsWidget::loadSettings() {
  QSettings settings(QStringLiteral("ArtifactStudio"), QStringLiteral("AICloud"));
  apiKeyEdit_->setText(settings.value(QStringLiteral("apiKey")).toString());
  baseUrlEdit_->setText(settings.value(QStringLiteral("baseUrl")).toString());

  const QString providerNameSetting =
      settings.value(QStringLiteral("provider"), QStringLiteral("OpenAI")).toString();
  const int idx = providerCombo_->findText(providerNameSetting);
  if (idx >= 0) {
    providerCombo_->setCurrentIndex(idx);
  }
}

void ArtifactAICloudSettingsWidget::saveSettings() {
  QSettings settings(QStringLiteral("ArtifactStudio"), QStringLiteral("AICloud"));
  settings.setValue(QStringLiteral("apiKey"), apiKeyEdit_->text().trimmed());
  settings.setValue(QStringLiteral("baseUrl"), baseUrlEdit_->text().trimmed());
  settings.setValue(QStringLiteral("provider"), providerName());
}

void ArtifactAICloudSettingsWidget::onProviderChanged(int index) {
  Q_UNUSED(index);
  updateBaseUrlVisibility();
}

void ArtifactAICloudSettingsWidget::updateBaseUrlVisibility() {
  const bool showBaseUrl = provider() == AICloudProvider::KiloGateway ||
                           provider() == AICloudProvider::Custom;
  if (baseUrlLabel_) {
    baseUrlLabel_->setVisible(showBaseUrl);
  }
  if (baseUrlEdit_) {
    baseUrlEdit_->setVisible(showBaseUrl);
  }

  if (!baseUrlEdit_) {
    return;
  }
  if (provider() == AICloudProvider::KiloGateway) {
    baseUrlEdit_->setPlaceholderText(QStringLiteral("https://your-kilogateway-instance.com/v1"));
  } else if (provider() == AICloudProvider::Custom) {
    baseUrlEdit_->setPlaceholderText(QStringLiteral("https://api.example.com/v1"));
  } else {
    baseUrlEdit_->setPlaceholderText(QStringLiteral("https://api.openai.com/v1"));
  }
}

} // namespace Artifact

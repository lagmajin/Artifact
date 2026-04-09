module;
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <QWidget>

export module Artifact.Widgets.AI.ArtifactAICloudSettingsWidget;

import std;

export namespace Artifact {

enum class AICloudProvider { OpenAI, Grok, OpenRouter, KiloGateway, Custom };

class ArtifactAICloudSettingsWidget : public QWidget {
public:
  explicit ArtifactAICloudSettingsWidget(QWidget *parent = nullptr);

  AICloudProvider provider() const;
  QString providerName() const;
  QString apiKey() const;
  QString baseUrl() const;
  void loadSettings();
  void saveSettings();

private:
  void onProviderChanged(int index);
  void updateBaseUrlVisibility();

  QComboBox *providerCombo_ = nullptr;
  QLineEdit *apiKeyEdit_ = nullptr;
  QLineEdit *baseUrlEdit_ = nullptr;
  QLabel *baseUrlLabel_ = nullptr;
};

} // namespace Artifact

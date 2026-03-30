module;

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <wobjectimpl.h>


import Color.ScienceManager;

module Artifact.Widgets.ColorSciencePanel;

namespace Artifact {

class ArtifactColorSciencePanel::Impl {
public:
  ArtifactColorScienceManager *manager_ = nullptr;

  // UI elements
  QComboBox *inputSpaceCombo_ = nullptr;
  QComboBox *workingSpaceCombo_ = nullptr;
  QComboBox *outputSpaceCombo_ = nullptr;
  QComboBox *lutCombo_ = nullptr;
  QSlider *lutIntensitySlider_ = nullptr;
  QLabel *lutIntensityLabel_ = nullptr;
  QPushButton *loadLUTButton_ = nullptr;
  QPushButton *clearLUTButton_ = nullptr;
  QCheckBox *hdrCheckBox_ = nullptr;

  void setupUI(QWidget *parent);
  void updateUI();
  void connectSignals();
};

ArtifactColorSciencePanel::ArtifactColorSciencePanel(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  impl_->manager_ = new ArtifactColorScienceManager();
  impl_->setupUI(this);
  impl_->connectSignals();
  impl_->updateUI();
}

ArtifactColorSciencePanel::~ArtifactColorSciencePanel() { delete impl_; }

void ArtifactColorSciencePanel::Impl::setupUI(QWidget *parent) {
  auto *layout = new QVBoxLayout(parent);

  // Color Space Group
  auto *colorSpaceGroup = new QGroupBox("Color Spaces");
  auto *colorSpaceLayout = new QFormLayout(colorSpaceGroup);

  inputSpaceCombo_ = new QComboBox();
  workingSpaceCombo_ = new QComboBox();
  outputSpaceCombo_ = new QComboBox();

  colorSpaceLayout->addRow("Input:", inputSpaceCombo_);
  colorSpaceLayout->addRow("Working:", workingSpaceCombo_);
  colorSpaceLayout->addRow("Output:", outputSpaceCombo_);

  layout->addWidget(colorSpaceGroup);

  // LUT Group
  auto *lutGroup = new QGroupBox("LUT");
  auto *lutLayout = new QVBoxLayout(lutGroup);

  lutCombo_ = new QComboBox();
  lutCombo_->setEditable(false);

  auto *lutControlsLayout = new QHBoxLayout();
  lutIntensitySlider_ = new QSlider(Qt::Horizontal);
  lutIntensitySlider_->setRange(0, 100);
  lutIntensitySlider_->setValue(100);
  lutIntensityLabel_ = new QLabel("100%");

  lutControlsLayout->addWidget(new QLabel("Intensity:"));
  lutControlsLayout->addWidget(lutIntensitySlider_);
  lutControlsLayout->addWidget(lutIntensityLabel_);

  auto *lutButtonsLayout = new QHBoxLayout();
  loadLUTButton_ = new QPushButton("Load LUT...");
  clearLUTButton_ = new QPushButton("Clear");

  lutButtonsLayout->addWidget(loadLUTButton_);
  lutButtonsLayout->addWidget(clearLUTButton_);

  lutLayout->addWidget(lutCombo_);
  lutLayout->addLayout(lutControlsLayout);
  lutLayout->addLayout(lutButtonsLayout);

  layout->addWidget(lutGroup);

  // HDR Group
  auto *hdrGroup = new QGroupBox("HDR");
  auto *hdrLayout = new QVBoxLayout(hdrGroup);

  hdrCheckBox_ = new QCheckBox("Enable HDR processing");
  hdrLayout->addWidget(hdrCheckBox_);

  layout->addWidget(hdrGroup);

  layout->addStretch();
}

void ArtifactColorSciencePanel::Impl::updateUI() {
  if (!manager_)
    return;

  auto settings = manager_->getSettings();

  // Populate color space combos
  auto spaces = manager_->getSupportedColorSpaces();
  QStringList spaceNames;
  for (auto space : spaces) {
    switch (space) {
    case ColorSpace::sRGB:
      spaceNames << "sRGB";
      break;
    case ColorSpace::Rec709:
      spaceNames << "Rec.709";
      break;
    case ColorSpace::Rec2020:
      spaceNames << "Rec.2020";
      break;
    case ColorSpace::P3:
      spaceNames << "DCI-P3";
      break;
    case ColorSpace::ACES_AP0:
      spaceNames << "ACEScg";
      break;
    case ColorSpace::ACES_AP1:
      spaceNames << "ACEScct";
      break;
    default:
      spaceNames << "Custom";
      break;
    }
  }

  inputSpaceCombo_->clear();
  inputSpaceCombo_->addItems(spaceNames);
  inputSpaceCombo_->setCurrentIndex(static_cast<int>(settings.inputSpace));

  workingSpaceCombo_->clear();
  workingSpaceCombo_->addItems(spaceNames);
  workingSpaceCombo_->setCurrentIndex(static_cast<int>(settings.workingSpace));

  outputSpaceCombo_->clear();
  outputSpaceCombo_->addItems(spaceNames);
  outputSpaceCombo_->setCurrentIndex(static_cast<int>(settings.outputSpace));

  // LUT combo
  lutCombo_->clear();
  auto availableLUTs = manager_->getAvailableLUTs();
  for (const auto &lut : availableLUTs) {
    QFileInfo info(QString::fromStdString(lut));
    lutCombo_->addItem(info.baseName(), QString::fromStdString(lut));
  }

  // LUT intensity
  int intensityPercent = static_cast<int>(manager_->getLUTIntensity() * 100);
  lutIntensitySlider_->setValue(intensityPercent);
  lutIntensityLabel_->setText(QString("%1%").arg(intensityPercent));

  // HDR
  hdrCheckBox_->setChecked(manager_->isHDREnabled());
}

void ArtifactColorSciencePanel::Impl::connectSignals() {
  if (!manager_)
    return;

  // Color space changes
  connect(inputSpaceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int index) {
            auto settings = manager_->getSettings();
            settings.inputSpace = static_cast<ColorSpace>(index);
            manager_->setSettings(settings);
          });

  connect(workingSpaceCombo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int index) {
            auto settings = manager_->getSettings();
            settings.workingSpace = static_cast<ColorSpace>(index);
            manager_->setSettings(settings);
          });

  connect(outputSpaceCombo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int index) {
            auto settings = manager_->getSettings();
            settings.outputSpace = static_cast<ColorSpace>(index);
            manager_->setSettings(settings);
          });

  // LUT controls
  connect(lutIntensitySlider_, &QSlider::valueChanged, [this](int value) {
    manager_->setLUTIntensity(value / 100.0f);
    lutIntensityLabel_->setText(QString("%1%").arg(value));
  });

  connect(loadLUTButton_, &QPushButton::clicked, [this]() {
    QString fileName = QFileDialog::getOpenFileName(
        nullptr, "Load LUT", QString(),
        "LUT files (*.cube *.3dl *.lut);;All files (*.*)");
    if (!fileName.isEmpty()) {
      if (manager_->loadLUT(fileName.toStdString())) {
        updateUI();
      }
    }
  });

  connect(clearLUTButton_, &QPushButton::clicked, [this]() {
    manager_->clearLUT();
    updateUI();
  });

  // HDR
  connect(hdrCheckBox_, &QCheckBox::toggled,
          [this](bool checked) { manager_->setHDREnabled(checked); });
}

ArtifactColorScienceManager *
ArtifactColorSciencePanel::colorScienceManager() const {
  return impl_->manager_;
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactColorSciencePanel)

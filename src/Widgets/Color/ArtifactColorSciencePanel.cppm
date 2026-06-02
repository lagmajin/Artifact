module;
#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QStringList>
#include <wobjectimpl.h>

import Analyze.Histogram;
import Color.AutoMatch;
import Color.Float;
import Image.ImageF32x4_RGBA;
import Artifact.Service.Playback;

module Artifact.Widgets.ColorSciencePanel;
import Color.ScienceManager;

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
  QPushButton *analyzeFrameButton_ = nullptr;
  QPushButton *clearAnalysisButton_ = nullptr;
  QLabel *analysisSummaryLabel_ = nullptr;
  QPlainTextEdit *analysisReport_ = nullptr;

  void setupUI(QWidget *parent);
  void updateUI();
  void connectSignals();
  void clearAnalysis();
  void analyzeCurrentFrame();
  void setAnalysisReport(const QString &summary, const QStringList &lines);
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

  // Auto Grading Group
  auto *analysisGroup = new QGroupBox("Auto Grading");
  auto *analysisLayout = new QVBoxLayout(analysisGroup);
  auto *analysisButtonRow = new QHBoxLayout();
  analyzeFrameButton_ = new QPushButton("Analyze Current Frame");
  clearAnalysisButton_ = new QPushButton("Clear");
  analysisButtonRow->addWidget(analyzeFrameButton_);
  analysisButtonRow->addWidget(clearAnalysisButton_);
  analysisButtonRow->addStretch();

  analysisSummaryLabel_ =
      new QLabel("Open a project and analyze the current preview frame.");
  analysisSummaryLabel_->setWordWrap(true);
  analysisReport_ = new QPlainTextEdit();
  analysisReport_->setReadOnly(true);
  analysisReport_->setPlaceholderText("No analysis yet.");
  analysisReport_->setMinimumHeight(180);

  analysisLayout->addLayout(analysisButtonRow);
  analysisLayout->addWidget(analysisSummaryLabel_);
  analysisLayout->addWidget(analysisReport_);

  layout->addWidget(analysisGroup);

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

  connect(analyzeFrameButton_, &QPushButton::clicked, [this]() {
    analyzeCurrentFrame();
  });

  connect(clearAnalysisButton_, &QPushButton::clicked, [this]() {
    clearAnalysis();
  });
}

void ArtifactColorSciencePanel::Impl::clearAnalysis() {
  if (analysisSummaryLabel_) {
    analysisSummaryLabel_->setText(
        QStringLiteral("Open a project and analyze the current preview frame."));
  }
  if (analysisReport_) {
    analysisReport_->clear();
  }
}

void ArtifactColorSciencePanel::Impl::setAnalysisReport(
    const QString &summary, const QStringList &lines) {
  if (analysisSummaryLabel_) {
    analysisSummaryLabel_->setText(summary);
  }
  if (analysisReport_) {
    analysisReport_->setPlainText(lines.join(QStringLiteral("\n")));
  }
}

void ArtifactColorSciencePanel::Impl::analyzeCurrentFrame() {
  auto *playback = ArtifactPlaybackService::instance();
  if (!playback) {
    setAnalysisReport(
        QStringLiteral("Playback service is unavailable."),
        {QStringLiteral("No playback service instance was found.")});
    return;
  }

  const auto framePos = playback->currentFrame().framePosition();
  ArtifactCore::ImageF32x4_RGBA frame;
  if (!playback->tryGetRamPreviewFrameImage(framePos, frame) || frame.isEmpty()) {
    setAnalysisReport(
        QStringLiteral("No RAM preview frame is ready for frame %1.")
            .arg(framePos),
        {QStringLiteral("Build RAM preview or play the composition first."),
         QStringLiteral("This window reads the current frame from preview memory.")});
    return;
  }

  const int width = frame.width();
  const int height = frame.height();
  const float *pixels = frame.rgba32fData();
  if (!pixels || width <= 0 || height <= 0) {
    setAnalysisReport(QStringLiteral("Current frame data is empty."),
                      {QStringLiteral("The preview frame did not provide pixel data.")});
    return;
  }

  const auto stats = ArtifactCore::ImageAnalyzer::analyze(pixels, width, height);
  const float exposureEV =
      ArtifactCore::ImageAnalyzer::autoExposureEV(pixels, width, height);
  const auto whiteBalance =
      ArtifactCore::ImageAnalyzer::autoWhiteBalance(pixels, width, height);
  const float contrastRatio =
      ArtifactCore::ImageAnalyzer::contrastRatio(pixels, width, height);
  const float dynamicRange =
      ArtifactCore::ImageAnalyzer::dynamicRange(pixels, width, height);

  ArtifactCore::ImageF32x4_RGBA neutral;
  neutral.resize(width, height);
  neutral.fill(ArtifactCore::FloatRGBA(0.18f, 0.18f, 0.18f, 1.0f));
  const auto neutralMatch = ArtifactCore::AutoColorMatcher::computeMatch(
      pixels, neutral.rgba32fData(), width, height, width, height,
      ArtifactCore::AutoColorMatcher::Method::Reinhard);

  QStringList lines;
  lines << QStringLiteral("frame %1 | size %2x%3")
               .arg(framePos)
               .arg(width)
               .arg(height);
  lines << QStringLiteral("luma mean %1 | median %2 | p5 %3 | p95 %4")
               .arg(QString::number(stats.luminance.mean, 'f', 3))
               .arg(QString::number(stats.luminance.median, 'f', 3))
               .arg(QString::number(stats.luminance.percentile5, 'f', 3))
               .arg(QString::number(stats.luminance.percentile95, 'f', 3));
  lines << QStringLiteral("exposure EV %1")
               .arg(QString::number(exposureEV, 'f', 2));
  lines << QStringLiteral("white balance R %1 | G %2 | B %3")
               .arg(QString::number(whiteBalance[0], 'f', 2))
               .arg(QString::number(whiteBalance[1], 'f', 2))
               .arg(QString::number(whiteBalance[2], 'f', 2));
  lines << QStringLiteral("contrast %1 | dynamic range %2 EV")
               .arg(QString::number(contrastRatio, 'f', 2))
               .arg(QString::number(dynamicRange, 'f', 2));
  lines << QStringLiteral("neutral match scale R %1 G %2 B %3")
               .arg(QString::number(neutralMatch.scaleR, 'f', 2))
               .arg(QString::number(neutralMatch.scaleG, 'f', 2))
               .arg(QString::number(neutralMatch.scaleB, 'f', 2));
  lines << QStringLiteral("neutral match offset R %1 G %2 B %3 | confidence %4")
               .arg(QString::number(neutralMatch.offsetR, 'f', 3))
               .arg(QString::number(neutralMatch.offsetG, 'f', 3))
               .arg(QString::number(neutralMatch.offsetB, 'f', 3))
               .arg(QString::number(neutralMatch.confidence * 100.0f, 'f', 0) +
                    QStringLiteral("%"));

  QStringList summaryParts;
  summaryParts << QStringLiteral("Analyzed frame %1.").arg(framePos);
  if (exposureEV > 0.35f) {
    summaryParts << QStringLiteral("Frame is dark; exposure lift is likely useful.");
  } else if (exposureEV < -0.35f) {
    summaryParts << QStringLiteral("Frame is bright; exposure reduction may help.");
  } else {
    summaryParts << QStringLiteral("Exposure is near target middle gray.");
  }

  if (whiteBalance[0] > 1.08f || whiteBalance[2] > 1.08f) {
    summaryParts << QStringLiteral("White balance skew is visible.");
  } else {
    summaryParts << QStringLiteral("White balance looks fairly neutral.");
  }

  if (contrastRatio < 1.4f) {
    summaryParts << QStringLiteral("Low contrast: consider a stronger grade.");
  } else if (contrastRatio > 3.5f) {
    summaryParts << QStringLiteral("High contrast: preserve highlight detail when grading.");
  }

  setAnalysisReport(summaryParts.join(QStringLiteral(" ")), lines);
}

ArtifactColorScienceManager *
ArtifactColorSciencePanel::colorScienceManager() const {
  return impl_->manager_;
}

void ArtifactColorSciencePanel::analyzeCurrentFrame() {
  impl_->analyzeCurrentFrame();
}

void ArtifactColorSciencePanel::clearAnalysis() {
  impl_->clearAnalysis();
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactColorSciencePanel)

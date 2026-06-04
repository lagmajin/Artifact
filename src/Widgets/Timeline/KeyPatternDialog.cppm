module;

#include <QComboBox>
#include <QDialogButtonBox>
#include <QAbstractItemView>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVariant>
#include <limits>
#include <wobjectimpl.h>

module Artifact.Widgets.Timeline.KeyPatternDialog;

import Animation.KeyframePatternGenerator;

namespace Artifact {

namespace {

static QString valueTextFor(const QVariant &value) {
  if (!value.isValid()) {
    return QStringLiteral("-");
  }
  return value.toString();
}

static QString frameTextFor(const ArtifactCore::KeyFrame &keyframe) {
  return QStringLiteral("%1").arg(QString::number(keyframe.time.value()));
}

static int patternIndexForPreset(ArtifactCore::KeyframePatternPreset preset) {
  return static_cast<int>(preset);
}

static ArtifactCore::KeyframePatternPreset presetForIndex(int index) {
  switch (index) {
  case 0: return ArtifactCore::KeyframePatternPreset::Stagger;
  case 1: return ArtifactCore::KeyframePatternPreset::Pulse;
  case 2: return ArtifactCore::KeyframePatternPreset::Bounce;
  case 3: return ArtifactCore::KeyframePatternPreset::Shake;
  case 4: return ArtifactCore::KeyframePatternPreset::Loop;
  case 5: return ArtifactCore::KeyframePatternPreset::Ramp;
  case 6: return ArtifactCore::KeyframePatternPreset::Wave;
  case 7: return ArtifactCore::KeyframePatternPreset::Step;
  case 8: return ArtifactCore::KeyframePatternPreset::RandomHold;
  case 9: return ArtifactCore::KeyframePatternPreset::Overshoot;
  case 10: return ArtifactCore::KeyframePatternPreset::Settle;
  case 11: return ArtifactCore::KeyframePatternPreset::BeatSync;
  default: return ArtifactCore::KeyframePatternPreset::Ramp;
  }
}

} // namespace

W_OBJECT_IMPL(KeyPatternDialog)

class KeyPatternDialog::Impl {
public:
  std::function<void(const ArtifactCore::KeyframePatternRequest &)> applyCallback;
  ArtifactCore::KeyframePatternRequest request;
  QComboBox *presetCombo = nullptr;
  QDoubleSpinBox *startFrameSpin = nullptr;
  QDoubleSpinBox *endFrameSpin = nullptr;
  QDoubleSpinBox *baseValueSpin = nullptr;
  QDoubleSpinBox *targetValueSpin = nullptr;
  QDoubleSpinBox *amplitudeSpin = nullptr;
  QDoubleSpinBox *cyclesSpin = nullptr;
  QDoubleSpinBox *phaseSpin = nullptr;
  QDoubleSpinBox *delaySpin = nullptr;
  QDoubleSpinBox *bpmSpin = nullptr;
  QDoubleSpinBox *dampingSpin = nullptr;
  QDoubleSpinBox *oscillationSpin = nullptr;
  QSpinBox *stepCountSpin = nullptr;
  QSpinBox *sampleCountSpin = nullptr;
  QSpinBox *seedSpin = nullptr;
  QLabel *warningLabel = nullptr;
  QLabel *summaryLabel = nullptr;
  QListWidget *previewList = nullptr;
  QDialogButtonBox *buttonBox = nullptr;
};

KeyPatternDialog::KeyPatternDialog(
    QWidget *parent,
    std::function<void(const ArtifactCore::KeyframePatternRequest &)> applyCallback,
    const ArtifactCore::KeyframePatternRequest &initialRequest)
    : QDialog(parent), impl_(new Impl()) {
  setWindowTitle(QStringLiteral("Key Pattern Dialog"));
  resize(900, 640);

  impl_->applyCallback = std::move(applyCallback);
  impl_->request = initialRequest;

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(14, 14, 14, 14);
  root->setSpacing(10);

  auto *summary = impl_->summaryLabel = new QLabel(this);
  summary->setWordWrap(true);
  summary->setText(QStringLiteral("Generate a keyframe pattern for the selected property targets."));
  root->addWidget(summary);

  auto *form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  form->setFormAlignment(Qt::AlignTop);
  form->setSpacing(8);
  root->addLayout(form);

  impl_->presetCombo = new QComboBox(this);
  for (int i = 0; i <= static_cast<int>(ArtifactCore::KeyframePatternPreset::BeatSync); ++i) {
    impl_->presetCombo->addItem(
        ArtifactCore::KeyframePatternGenerator::presetLabel(presetForIndex(i)),
        i);
  }
  form->addRow(QStringLiteral("Pattern"), impl_->presetCombo);

  impl_->startFrameSpin = new QDoubleSpinBox(this);
  impl_->startFrameSpin->setRange(-999999.0, 999999.0);
  impl_->startFrameSpin->setDecimals(2);
  impl_->startFrameSpin->setSingleStep(1.0);
  form->addRow(QStringLiteral("Start Frame"), impl_->startFrameSpin);

  impl_->endFrameSpin = new QDoubleSpinBox(this);
  impl_->endFrameSpin->setRange(-999999.0, 999999.0);
  impl_->endFrameSpin->setDecimals(2);
  impl_->endFrameSpin->setSingleStep(1.0);
  form->addRow(QStringLiteral("End Frame"), impl_->endFrameSpin);

  impl_->baseValueSpin = new QDoubleSpinBox(this);
  impl_->baseValueSpin->setRange(-999999.0, 999999.0);
  impl_->baseValueSpin->setDecimals(3);
  impl_->baseValueSpin->setSingleStep(1.0);
  form->addRow(QStringLiteral("Base Value"), impl_->baseValueSpin);

  impl_->targetValueSpin = new QDoubleSpinBox(this);
  impl_->targetValueSpin->setRange(-999999.0, 999999.0);
  impl_->targetValueSpin->setDecimals(3);
  impl_->targetValueSpin->setSingleStep(1.0);
  form->addRow(QStringLiteral("Target Value"), impl_->targetValueSpin);

  impl_->amplitudeSpin = new QDoubleSpinBox(this);
  impl_->amplitudeSpin->setRange(0.0, 999999.0);
  impl_->amplitudeSpin->setDecimals(3);
  impl_->amplitudeSpin->setSingleStep(1.0);
  form->addRow(QStringLiteral("Amplitude"), impl_->amplitudeSpin);

  impl_->cyclesSpin = new QDoubleSpinBox(this);
  impl_->cyclesSpin->setRange(0.1, 64.0);
  impl_->cyclesSpin->setDecimals(3);
  impl_->cyclesSpin->setSingleStep(0.5);
  form->addRow(QStringLiteral("Cycles"), impl_->cyclesSpin);

  impl_->phaseSpin = new QDoubleSpinBox(this);
  impl_->phaseSpin->setRange(-1000.0, 1000.0);
  impl_->phaseSpin->setDecimals(3);
  impl_->phaseSpin->setSingleStep(0.25);
  form->addRow(QStringLiteral("Phase"), impl_->phaseSpin);

  impl_->delaySpin = new QDoubleSpinBox(this);
  impl_->delaySpin->setRange(0.0, 999999.0);
  impl_->delaySpin->setDecimals(2);
  impl_->delaySpin->setSingleStep(1.0);
  form->addRow(QStringLiteral("Delay Frames"), impl_->delaySpin);

  impl_->bpmSpin = new QDoubleSpinBox(this);
  impl_->bpmSpin->setRange(1.0, 999.0);
  impl_->bpmSpin->setDecimals(3);
  impl_->bpmSpin->setSingleStep(1.0);
  form->addRow(QStringLiteral("BPM"), impl_->bpmSpin);

  impl_->dampingSpin = new QDoubleSpinBox(this);
  impl_->dampingSpin->setRange(0.0, 32.0);
  impl_->dampingSpin->setDecimals(3);
  impl_->dampingSpin->setSingleStep(0.5);
  form->addRow(QStringLiteral("Damping"), impl_->dampingSpin);

  impl_->oscillationSpin = new QDoubleSpinBox(this);
  impl_->oscillationSpin->setRange(0.0, 16.0);
  impl_->oscillationSpin->setDecimals(3);
  impl_->oscillationSpin->setSingleStep(0.5);
  form->addRow(QStringLiteral("Oscillation"), impl_->oscillationSpin);

  impl_->stepCountSpin = new QSpinBox(this);
  impl_->stepCountSpin->setRange(1, 128);
  impl_->stepCountSpin->setSingleStep(1);
  form->addRow(QStringLiteral("Step Count"), impl_->stepCountSpin);

  impl_->sampleCountSpin = new QSpinBox(this);
  impl_->sampleCountSpin->setRange(2, 256);
  impl_->sampleCountSpin->setSingleStep(1);
  form->addRow(QStringLiteral("Sample Count"), impl_->sampleCountSpin);

  impl_->seedSpin = new QSpinBox(this);
  impl_->seedSpin->setRange(0, std::numeric_limits<int>::max());
  impl_->seedSpin->setSingleStep(1);
  form->addRow(QStringLiteral("Seed"), impl_->seedSpin);

  impl_->warningLabel = new QLabel(this);
  impl_->warningLabel->setWordWrap(true);
  impl_->warningLabel->setVisible(false);
  root->addWidget(impl_->warningLabel);

  impl_->previewList = new QListWidget(this);
  impl_->previewList->setAlternatingRowColors(false);
  impl_->previewList->setSelectionMode(QAbstractItemView::NoSelection);
  impl_->previewList->setMinimumHeight(220);
  root->addWidget(impl_->previewList, 1);

  impl_->buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
  if (auto *applyButton = impl_->buttonBox->button(QDialogButtonBox::Ok)) {
    applyButton->setText(QStringLiteral("Apply"));
    applyButton->setEnabled(static_cast<bool>(impl_->applyCallback));
  }
  root->addWidget(impl_->buttonBox);

  QObject::connect(impl_->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
    applyCurrentRequest();
  });
  QObject::connect(impl_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto connectPreviewUpdate = [this](auto *widget) {
    QObject::connect(widget, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                     [this](double) { refreshPreview(); });
  };
  connectPreviewUpdate(impl_->startFrameSpin);
  connectPreviewUpdate(impl_->endFrameSpin);
  connectPreviewUpdate(impl_->baseValueSpin);
  connectPreviewUpdate(impl_->targetValueSpin);
  connectPreviewUpdate(impl_->amplitudeSpin);
  connectPreviewUpdate(impl_->cyclesSpin);
  connectPreviewUpdate(impl_->phaseSpin);
  connectPreviewUpdate(impl_->delaySpin);
  connectPreviewUpdate(impl_->bpmSpin);
  connectPreviewUpdate(impl_->dampingSpin);
  connectPreviewUpdate(impl_->oscillationSpin);
  QObject::connect(impl_->stepCountSpin, qOverload<int>(&QSpinBox::valueChanged), this,
                   [this](int) { refreshPreview(); });
  QObject::connect(impl_->sampleCountSpin, qOverload<int>(&QSpinBox::valueChanged), this,
                   [this](int) { refreshPreview(); });
  QObject::connect(impl_->seedSpin, qOverload<int>(&QSpinBox::valueChanged), this,
                   [this](int) { refreshPreview(); });
  QObject::connect(impl_->presetCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                   [this](int) { refreshPreview(); });

  setRequest(initialRequest);
  refreshPreview();
}

KeyPatternDialog::~KeyPatternDialog() {
  delete impl_;
}

void KeyPatternDialog::setRequest(const ArtifactCore::KeyframePatternRequest &request) {
  if (!impl_) {
    return;
  }
  impl_->request = request;
  impl_->presetCombo->setCurrentIndex(patternIndexForPreset(request.preset));
  impl_->startFrameSpin->setValue(request.startFrame);
  impl_->endFrameSpin->setValue(request.endFrame);
  impl_->baseValueSpin->setValue(request.baseValue.toDouble());
  impl_->targetValueSpin->setValue(request.targetValue.isValid() ? request.targetValue.toDouble()
                                                                 : request.baseValue.toDouble());
  impl_->amplitudeSpin->setValue(request.amplitude);
  impl_->cyclesSpin->setValue(request.cycles);
  impl_->phaseSpin->setValue(request.phase);
  impl_->delaySpin->setValue(request.delayFrames);
  impl_->bpmSpin->setValue(request.bpm);
  impl_->dampingSpin->setValue(request.damping);
  impl_->oscillationSpin->setValue(request.settleOscillation);
  impl_->stepCountSpin->setValue(request.stepCount);
  impl_->sampleCountSpin->setValue(request.sampleCount);
  impl_->seedSpin->setValue(static_cast<int>(request.seed));
}

ArtifactCore::KeyframePatternRequest KeyPatternDialog::request() const {
  if (!impl_) {
    return {};
  }
  ArtifactCore::KeyframePatternRequest request = impl_->request;
  request.preset = presetForIndex(impl_->presetCombo->currentIndex());
  request.startFrame = impl_->startFrameSpin->value();
  request.endFrame = impl_->endFrameSpin->value();
  request.baseValue = impl_->baseValueSpin->value();
  request.targetValue = impl_->targetValueSpin->value();
  request.amplitude = impl_->amplitudeSpin->value();
  request.cycles = impl_->cyclesSpin->value();
  request.phase = impl_->phaseSpin->value();
  request.delayFrames = impl_->delaySpin->value();
  request.bpm = impl_->bpmSpin->value();
  request.damping = impl_->dampingSpin->value();
  request.settleOscillation = impl_->oscillationSpin->value();
  request.stepCount = impl_->stepCountSpin->value();
  request.sampleCount = impl_->sampleCountSpin->value();
  request.seed = static_cast<quint32>(std::max(0, impl_->seedSpin->value()));
  return request;
}

void KeyPatternDialog::refreshPreview() {
  if (!impl_) {
    return;
  }
  const auto request = this->request();
  const auto result = ArtifactCore::KeyframePatternGenerator::generate(request);

  impl_->previewList->clear();
  for (const auto &keyframe : result.keyframes) {
    impl_->previewList->addItem(
        QStringLiteral("Frame %1  |  Value %2  |  %3")
            .arg(frameTextFor(keyframe), valueTextFor(keyframe.value),
                 ArtifactCore::KeyframePatternGenerator::presetLabel(request.preset)));
  }

  if (result.warning.isEmpty()) {
    impl_->warningLabel->setVisible(false);
    impl_->warningLabel->clear();
  } else {
    impl_->warningLabel->setText(result.warning);
    impl_->warningLabel->setVisible(true);
  }

  if (impl_->summaryLabel) {
    impl_->summaryLabel->setText(
        QStringLiteral("%1 preview: %2 keyframes")
            .arg(ArtifactCore::KeyframePatternGenerator::presetLabel(request.preset))
            .arg(result.keyframes.size()));
  }
}

void KeyPatternDialog::applyCurrentRequest() {
  if (!impl_ || !impl_->applyCallback) {
    reject();
    return;
  }
  const auto currentRequest = request();
  impl_->applyCallback(currentRequest);
  accept();
}

} // namespace Artifact

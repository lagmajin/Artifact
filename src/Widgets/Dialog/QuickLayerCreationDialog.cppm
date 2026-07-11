module;
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSettings>
#include <QVBoxLayout>
#include <wobjectimpl.h>
module Artifact.Widgets.QuickLayerCreationDialog;

import Artifact.Layer.InitParams;

namespace Artifact {

W_OBJECT_IMPL(QuickLayerCreationDialog)

class QuickLayerCreationDialog::Impl {
public:
  QLineEdit* name = nullptr;
  QSpinBox* width = nullptr;
  QSpinBox* height = nullptr;
  QComboBox* mask = nullptr;
  QDoubleSpinBox* feather = nullptr;
  QCheckBox* entry = nullptr;
  QCheckBox* exit = nullptr;
  QComboBox* timing = nullptr;
  QComboBox* curve = nullptr;
  QSpinBox* frames = nullptr;
};

QuickLayerCreationDialog::QuickLayerCreationDialog(QWidget* parent)
    : QDialog(parent), impl_(new Impl()) {
  setWindowTitle(QStringLiteral("クイックレイヤー作成"));
  resize(460, 560);

  auto* root = new QVBoxLayout(this);
  auto* basic = new QGroupBox(QStringLiteral("平面 / 素材"), this);
  auto* form = new QFormLayout(basic);
  impl_->name = new QLineEdit(QStringLiteral("平面 1"), basic);
  impl_->width = new QSpinBox(basic);
  impl_->height = new QSpinBox(basic);
  impl_->width->setRange(1, 16384);
  impl_->height->setRange(1, 16384);
  impl_->width->setValue(1920);
  impl_->height->setValue(1080);
  form->addRow(QStringLiteral("名前"), impl_->name);
  form->addRow(QStringLiteral("幅"), impl_->width);
  form->addRow(QStringLiteral("高さ"), impl_->height);
  root->addWidget(basic);

  auto* maskBox = new QGroupBox(QStringLiteral("マスク"), this);
  auto* maskForm = new QFormLayout(maskBox);
  impl_->mask = new QComboBox(maskBox);
  impl_->mask->addItem(QStringLiteral("なし"), static_cast<int>(QuickLayerMaskShape::None));
  impl_->mask->addItem(QStringLiteral("長方形"), static_cast<int>(QuickLayerMaskShape::Rectangle));
  impl_->mask->addItem(QStringLiteral("楕円"), static_cast<int>(QuickLayerMaskShape::Ellipse));
  impl_->feather = new QDoubleSpinBox(maskBox);
  impl_->feather->setRange(0.0, 2048.0);
  impl_->feather->setSuffix(QStringLiteral(" px"));
  maskForm->addRow(QStringLiteral("形状"), impl_->mask);
  maskForm->addRow(QStringLiteral("Feather"), impl_->feather);
  root->addWidget(maskBox);

  auto* envelopeBox = new QGroupBox(QStringLiteral("入場 / 退場 Envelope"), this);
  auto* envelopeForm = new QFormLayout(envelopeBox);
  impl_->entry = new QCheckBox(QStringLiteral("入場"), envelopeBox);
  impl_->exit = new QCheckBox(QStringLiteral("退場"), envelopeBox);
  auto* directions = new QWidget(envelopeBox);
  auto* directionLayout = new QHBoxLayout(directions);
  directionLayout->setContentsMargins(0, 0, 0, 0);
  directionLayout->addWidget(impl_->entry);
  directionLayout->addWidget(impl_->exit);
  impl_->timing = new QComboBox(envelopeBox);
  impl_->timing->addItem(QStringLiteral("同時"), static_cast<int>(QuickLayerEnvelopeTiming::Simultaneous));
  impl_->timing->addItem(QStringLiteral("透明度先行"), static_cast<int>(QuickLayerEnvelopeTiming::OpacityLead));
  impl_->timing->addItem(QStringLiteral("エフェクト先行"), static_cast<int>(QuickLayerEnvelopeTiming::EffectLead));
  impl_->curve = new QComboBox(envelopeBox);
  impl_->curve->addItem(QStringLiteral("Linear"), static_cast<int>(LayerEnvelopeCurve::Linear));
  impl_->curve->addItem(QStringLiteral("Ease In"), static_cast<int>(LayerEnvelopeCurve::EaseIn));
  impl_->curve->addItem(QStringLiteral("Ease Out"), static_cast<int>(LayerEnvelopeCurve::EaseOut));
  impl_->curve->addItem(QStringLiteral("Ease In-Out"), static_cast<int>(LayerEnvelopeCurve::EaseInOut));
  impl_->curve->addItem(QStringLiteral("Step"), static_cast<int>(LayerEnvelopeCurve::Step));
  impl_->frames = new QSpinBox(envelopeBox);
  impl_->frames->setRange(1, 240);
  impl_->frames->setValue(8);
  impl_->frames->setSuffix(QStringLiteral(" frames"));
  envelopeForm->addRow(QStringLiteral("方向"), directions);
  envelopeForm->addRow(QStringLiteral("追従"), impl_->timing);
  envelopeForm->addRow(QStringLiteral("カーブ"), impl_->curve);
  envelopeForm->addRow(QStringLiteral("長さ"), impl_->frames);
  root->addWidget(envelopeBox);
  root->addStretch();

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("作成"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("キャンセル"));
  QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(buttons);

  QSettings settings;
  settings.beginGroup(QStringLiteral("QuickLayerCreationDialog"));
  impl_->width->setValue(settings.value(QStringLiteral("width"), impl_->width->value()).toInt());
  impl_->height->setValue(settings.value(QStringLiteral("height"), impl_->height->value()).toInt());
  impl_->mask->setCurrentIndex(settings.value(QStringLiteral("mask"), 0).toInt());
  impl_->feather->setValue(settings.value(QStringLiteral("feather"), 0.0).toDouble());
  impl_->entry->setChecked(settings.value(QStringLiteral("entry"), false).toBool());
  impl_->exit->setChecked(settings.value(QStringLiteral("exit"), false).toBool());
  impl_->timing->setCurrentIndex(settings.value(QStringLiteral("timing"), 0).toInt());
  impl_->curve->setCurrentIndex(settings.value(QStringLiteral("curve"), 0).toInt());
  impl_->frames->setValue(settings.value(QStringLiteral("frames"), 8).toInt());
  settings.endGroup();
}

QuickLayerCreationDialog::~QuickLayerCreationDialog() { delete impl_; }

QuickLayerCreationOptions QuickLayerCreationDialog::submittedOptions() const {
  QuickLayerCreationOptions options;
  options.solidParams = ArtifactSolidLayerInitParams(impl_->name->text());
  options.solidParams.setWidth(impl_->width->value());
  options.solidParams.setHeight(impl_->height->value());
  options.maskShape = static_cast<QuickLayerMaskShape>(impl_->mask->currentData().toInt());
  options.maskFeather = static_cast<float>(impl_->feather->value());
  options.entryEnvelope = impl_->entry->isChecked();
  options.exitEnvelope = impl_->exit->isChecked();
  options.envelopeTiming = static_cast<QuickLayerEnvelopeTiming>(impl_->timing->currentData().toInt());
  options.envelopeCurve = static_cast<LayerEnvelopeCurve>(impl_->curve->currentData().toInt());
  options.envelopeFrames = impl_->frames->value();
  options.envelope.enabled = options.entryEnvelope || options.exitEnvelope;
  options.envelope.entry = options.entryEnvelope;
  options.envelope.exit = options.exitEnvelope;
  options.envelope.durationFrames = options.envelopeFrames;
  options.envelope.curve = options.envelopeCurve;

  QSettings settings;
  settings.beginGroup(QStringLiteral("QuickLayerCreationDialog"));
  settings.setValue(QStringLiteral("width"), impl_->width->value());
  settings.setValue(QStringLiteral("height"), impl_->height->value());
  settings.setValue(QStringLiteral("mask"), impl_->mask->currentIndex());
  settings.setValue(QStringLiteral("feather"), impl_->feather->value());
  settings.setValue(QStringLiteral("entry"), impl_->entry->isChecked());
  settings.setValue(QStringLiteral("exit"), impl_->exit->isChecked());
  settings.setValue(QStringLiteral("timing"), impl_->timing->currentIndex());
  settings.setValue(QStringLiteral("curve"), impl_->curve->currentIndex());
  settings.setValue(QStringLiteral("frames"), impl_->frames->value());
  settings.endGroup();
  switch (options.envelopeTiming) {
  case QuickLayerEnvelopeTiming::OpacityLead:
    options.envelope.timing = LayerEnvelopeTiming::OpacityLead;
    break;
  case QuickLayerEnvelopeTiming::EffectLead:
    options.envelope.timing = LayerEnvelopeTiming::EffectLead;
    break;
  case QuickLayerEnvelopeTiming::Simultaneous:
    options.envelope.timing = LayerEnvelopeTiming::Simultaneous;
    break;
  }
  return options;
}

}

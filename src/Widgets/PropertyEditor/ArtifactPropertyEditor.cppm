module;

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QSignalBlocker>

module Artifact.Widgets.PropertyEditor;

import std;

namespace Artifact {

ArtifactAbstractPropertyEditor::ArtifactAbstractPropertyEditor(QWidget* parent)
    : QWidget(parent) {}

ArtifactAbstractPropertyEditor::~ArtifactAbstractPropertyEditor() = default;

void ArtifactAbstractPropertyEditor::setCommitHandler(CommitHandler handler)
{
    commitHandler_ = std::move(handler);
}

void ArtifactAbstractPropertyEditor::commitValue(const QVariant& value) const
{
    if (commitHandler_) {
        commitHandler_(value);
    }
}

namespace {

QColor propertyColor(const ArtifactCore::AbstractProperty& property)
{
    QColor color = property.getColorValue();
    const QVariant currentValue = property.getValue();
    if (!color.isValid() && currentValue.canConvert<QColor>()) {
        color = currentValue.value<QColor>();
    }
    if (!color.isValid()) {
        color = QColor(QStringLiteral("#000000"));
    }
    return color;
}

int floatToSliderPosition(const double value, const double minValue, const double maxValue)
{
    if (maxValue <= minValue) {
        return 0;
    }
    const double normalized = std::clamp((value - minValue) / (maxValue - minValue), 0.0, 1.0);
    return static_cast<int>(std::lround(normalized * 1000.0));
}

double sliderPositionToFloat(const int sliderValue, const double minValue, const double maxValue)
{
    if (maxValue <= minValue) {
        return minValue;
    }
    const double normalized = static_cast<double>(sliderValue) / 1000.0;
    return minValue + (maxValue - minValue) * normalized;
}

} // namespace

ArtifactFloatPropertyEditor::ArtifactFloatPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent)
{
    spinBox_ = new QDoubleSpinBox(this);
    slider_ = new QSlider(Qt::Horizontal, this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(spinBox_, 1);
    layout->addWidget(slider_, 1);

    const auto meta = property.metadata();
    const double hardMin = meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1e6;
    const double hardMax = meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1e6;
    softMin_ = meta.softMin.isValid() ? meta.softMin.toDouble() : hardMin;
    softMax_ = meta.softMax.isValid() ? meta.softMax.toDouble() : hardMax;
    if (softMax_ <= softMin_) {
        softMin_ = hardMin;
        softMax_ = hardMax;
    }

    spinBox_->setRange(
        meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1e6,
        meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1e6);
    spinBox_->setValue(property.getValue().toDouble());
    if (meta.step.isValid()) {
        spinBox_->setSingleStep(meta.step.toDouble());
    }
    if (!meta.unit.isEmpty()) {
        spinBox_->setSuffix(QStringLiteral(" ") + meta.unit);
    }

    slider_->setRange(0, 1000);
    slider_->setValue(floatToSliderPosition(property.getValue().toDouble(), softMin_, softMax_));

    QObject::connect(spinBox_, &QDoubleSpinBox::valueChanged, this, [this](const double nextValue) {
        const QSignalBlocker blocker(slider_);
        slider_->setValue(floatToSliderPosition(nextValue, softMin_, softMax_));
    });
    QObject::connect(spinBox_, &QDoubleSpinBox::editingFinished, this, [this]() {
        commitValue(spinBox_->value());
    });
    QObject::connect(slider_, &QSlider::valueChanged, this, [this](const int sliderValue) {
        const double nextValue = sliderPositionToFloat(sliderValue, softMin_, softMax_);
        const QSignalBlocker blocker(spinBox_);
        spinBox_->setValue(nextValue);
        commitValue(nextValue);
    });
}

QVariant ArtifactFloatPropertyEditor::value() const
{
    return spinBox_ ? QVariant(spinBox_->value()) : QVariant();
}

void ArtifactFloatPropertyEditor::setValueFromVariant(const QVariant& value)
{
    if (!spinBox_ || !slider_) {
        return;
    }
    const double nextValue = value.toDouble();
    {
        const QSignalBlocker spinBlocker(spinBox_);
        spinBox_->setValue(nextValue);
    }
    const QSignalBlocker sliderBlocker(slider_);
    slider_->setValue(floatToSliderPosition(nextValue, softMin_, softMax_));
}

ArtifactIntPropertyEditor::ArtifactIntPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent)
{
    spinBox_ = new QSpinBox(this);
    slider_ = new QSlider(Qt::Horizontal, this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(spinBox_, 1);
    layout->addWidget(slider_, 1);

    const auto meta = property.metadata();
    const int hardMin = meta.hardMin.isValid() ? meta.hardMin.toInt() : -1000000;
    const int hardMax = meta.hardMax.isValid() ? meta.hardMax.toInt() : 1000000;
    softMin_ = meta.softMin.isValid() ? meta.softMin.toInt() : hardMin;
    softMax_ = meta.softMax.isValid() ? meta.softMax.toInt() : hardMax;
    if (softMax_ <= softMin_) {
        softMin_ = hardMin;
        softMax_ = hardMax;
    }

    spinBox_->setRange(
        meta.hardMin.isValid() ? meta.hardMin.toInt() : -1000000,
        meta.hardMax.isValid() ? meta.hardMax.toInt() : 1000000);
    spinBox_->setValue(property.getValue().toInt());
    if (meta.step.isValid()) {
        spinBox_->setSingleStep(meta.step.toInt());
    }
    if (!meta.unit.isEmpty()) {
        spinBox_->setSuffix(QStringLiteral(" ") + meta.unit);
    }

    slider_->setRange(softMin_, softMax_);
    slider_->setValue(std::clamp(property.getValue().toInt(), softMin_, softMax_));

    QObject::connect(spinBox_, &QSpinBox::valueChanged, this, [this](const int nextValue) {
        const QSignalBlocker blocker(slider_);
        slider_->setValue(std::clamp(nextValue, softMin_, softMax_));
    });
    QObject::connect(spinBox_, &QSpinBox::editingFinished, this, [this]() {
        commitValue(spinBox_->value());
    });
    QObject::connect(slider_, &QSlider::valueChanged, this, [this](const int sliderValue) {
        const QSignalBlocker blocker(spinBox_);
        spinBox_->setValue(sliderValue);
        commitValue(sliderValue);
    });
}

QVariant ArtifactIntPropertyEditor::value() const
{
    return spinBox_ ? QVariant(spinBox_->value()) : QVariant();
}

void ArtifactIntPropertyEditor::setValueFromVariant(const QVariant& value)
{
    if (!spinBox_ || !slider_) {
        return;
    }
    const int nextValue = value.toInt();
    {
        const QSignalBlocker spinBlocker(spinBox_);
        spinBox_->setValue(nextValue);
    }
    const QSignalBlocker sliderBlocker(slider_);
    slider_->setValue(std::clamp(nextValue, softMin_, softMax_));
}

ArtifactBoolPropertyEditor::ArtifactBoolPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent)
{
    checkBox_ = new QCheckBox(this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(checkBox_);
    layout->addStretch();

    checkBox_->setChecked(property.getValue().toBool());
    QObject::connect(checkBox_, &QCheckBox::toggled, this, [this](const bool checked) {
        commitValue(checked);
    });
}

QVariant ArtifactBoolPropertyEditor::value() const
{
    return checkBox_ ? QVariant(checkBox_->isChecked()) : QVariant();
}

void ArtifactBoolPropertyEditor::setValueFromVariant(const QVariant& value)
{
    if (!checkBox_) {
        return;
    }
    const QSignalBlocker blocker(checkBox_);
    checkBox_->setChecked(value.toBool());
}

ArtifactStringPropertyEditor::ArtifactStringPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent)
{
    lineEdit_ = new QLineEdit(property.getValue().toString(), this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(lineEdit_);

    QObject::connect(lineEdit_, &QLineEdit::editingFinished, this, [this]() {
        commitValue(lineEdit_->text());
    });
}

QVariant ArtifactStringPropertyEditor::value() const
{
    return lineEdit_ ? QVariant(lineEdit_->text()) : QVariant();
}

void ArtifactStringPropertyEditor::setValueFromVariant(const QVariant& value)
{
    if (!lineEdit_) {
        return;
    }
    const QSignalBlocker blocker(lineEdit_);
    lineEdit_->setText(value.toString());
}

ArtifactColorPropertyEditor::ArtifactColorPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent)
{
    button_ = new QPushButton(QStringLiteral(" "), this);
    valueLabel_ = new QLabel(this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(button_, 0);
    layout->addWidget(valueLabel_, 1);

    button_->setFixedWidth(36);
    currentColor_ = propertyColor(property);
    applyColor(currentColor_);
    QObject::connect(button_, &QPushButton::clicked, this, [this]() {
        const QColor nextColor = QColorDialog::getColor(currentColor_, button_, QStringLiteral("Select Color"));
        if (!nextColor.isValid()) {
            return;
        }
        applyColor(nextColor);
        commitValue(nextColor);
    });
}

QVariant ArtifactColorPropertyEditor::value() const
{
    return QVariant(currentColor_);
}

void ArtifactColorPropertyEditor::setValueFromVariant(const QVariant& value)
{
    QColor nextColor;
    if (value.canConvert<QColor>()) {
        nextColor = value.value<QColor>();
    }
    if (nextColor.isValid()) {
        applyColor(nextColor);
    }
}

void ArtifactColorPropertyEditor::applyColor(const QColor& color)
{
    currentColor_ = color;
    if (button_) {
        button_->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #666;").arg(color.name()));
    }
    if (valueLabel_) {
        valueLabel_->setText(color.name(QColor::HexArgb).toUpper());
    }
}

ArtifactPropertyEditorRowWidget::ArtifactPropertyEditorRowWidget(
    const QString& labelText,
    ArtifactAbstractPropertyEditor* editor,
    const QString& propertyName,
    QWidget* parent)
    : QWidget(parent),
      label_(new QLabel(labelText, this)),
      editor_(editor),
      keyframeButton_(new QPushButton(QStringLiteral("K"), this)),
      resetButton_(new QPushButton(QStringLiteral("R"), this)),
      expressionButton_(new QPushButton(QString::fromUtf8("fx"), this))
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    label_->setMinimumWidth(96);
    editor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    keyframeButton_->setToolTip(QStringLiteral("Keyframe: %1").arg(propertyName));
    keyframeButton_->setFixedSize(22, 22);
    keyframeButton_->setCheckable(true);
    keyframeButton_->setChecked(false);
    keyframeButton_->setEnabled(false);
    resetButton_->setToolTip(QStringLiteral("Reset: %1").arg(propertyName));
    resetButton_->setFixedSize(22, 22);
    expressionButton_->setToolTip(QStringLiteral("Expression Copilot: %1").arg(propertyName));
    expressionButton_->setFixedSize(24, 24);

    layout->addWidget(label_);
    layout->addWidget(editor_, 1);
    layout->addWidget(keyframeButton_);
    layout->addWidget(resetButton_);
    layout->addWidget(expressionButton_);

    QObject::connect(resetButton_, &QPushButton::clicked, this, [this]() {
        if (resetHandler_) {
            resetHandler_();
        }
    });
    QObject::connect(expressionButton_, &QPushButton::clicked, this, [this]() {
        if (expressionHandler_) {
            expressionHandler_();
        }
    });
}

ArtifactPropertyEditorRowWidget::~ArtifactPropertyEditorRowWidget() = default;

QLabel* ArtifactPropertyEditorRowWidget::label() const
{
    return label_;
}

ArtifactAbstractPropertyEditor* ArtifactPropertyEditorRowWidget::editor() const
{
    return editor_;
}

void ArtifactPropertyEditorRowWidget::setExpressionHandler(std::function<void()> handler)
{
    expressionHandler_ = std::move(handler);
}

void ArtifactPropertyEditorRowWidget::setResetHandler(std::function<void()> handler)
{
    resetHandler_ = std::move(handler);
}

void ArtifactPropertyEditorRowWidget::setEditorToolTip(const QString& tooltip)
{
    label_->setToolTip(tooltip);
    editor_->setToolTip(tooltip);
    keyframeButton_->setToolTip(tooltip);
    resetButton_->setToolTip(tooltip);
    expressionButton_->setToolTip(tooltip);
}

void ArtifactPropertyEditorRowWidget::setShowExpressionButton(const bool visible)
{
    expressionButton_->setVisible(visible);
}

void ArtifactPropertyEditorRowWidget::setShowResetButton(const bool visible)
{
    resetButton_->setVisible(visible);
}

void ArtifactPropertyEditorRowWidget::setShowKeyframeButton(const bool visible)
{
    keyframeButton_->setVisible(visible);
    keyframeButton_->setEnabled(visible);
}

ArtifactAbstractPropertyEditor* createPropertyEditorWidget(const ArtifactCore::AbstractProperty& property, QWidget* parent)
{
    switch (property.getType()) {
    case ArtifactCore::PropertyType::Float:
        return new ArtifactFloatPropertyEditor(property, parent);
    case ArtifactCore::PropertyType::Integer:
        return new ArtifactIntPropertyEditor(property, parent);
    case ArtifactCore::PropertyType::Boolean:
        return new ArtifactBoolPropertyEditor(property, parent);
    case ArtifactCore::PropertyType::Color:
        return new ArtifactColorPropertyEditor(property, parent);
    case ArtifactCore::PropertyType::String:
        return new ArtifactStringPropertyEditor(property, parent);
    default:
        return nullptr;
    }
}

} // namespace Artifact

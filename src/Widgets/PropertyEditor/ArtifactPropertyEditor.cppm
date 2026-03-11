module;

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

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

} // namespace

ArtifactFloatPropertyEditor::ArtifactFloatPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent)
{
    auto* spin = new QDoubleSpinBox(this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(spin);

    const auto meta = property.metadata();
    spin->setRange(
        meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1e6,
        meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1e6);
    spin->setValue(property.getValue().toDouble());
    if (meta.step.isValid()) {
        spin->setSingleStep(meta.step.toDouble());
    }
    if (!meta.unit.isEmpty()) {
        spin->setSuffix(QStringLiteral(" ") + meta.unit);
    }

    QObject::connect(spin, &QDoubleSpinBox::editingFinished, this, [this, spin]() {
        commitValue(spin->value());
    });
}

ArtifactIntPropertyEditor::ArtifactIntPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent)
{
    auto* spin = new QSpinBox(this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(spin);

    const auto meta = property.metadata();
    spin->setRange(
        meta.hardMin.isValid() ? meta.hardMin.toInt() : -1000000,
        meta.hardMax.isValid() ? meta.hardMax.toInt() : 1000000);
    spin->setValue(property.getValue().toInt());
    if (meta.step.isValid()) {
        spin->setSingleStep(meta.step.toInt());
    }
    if (!meta.unit.isEmpty()) {
        spin->setSuffix(QStringLiteral(" ") + meta.unit);
    }

    QObject::connect(spin, &QSpinBox::editingFinished, this, [this, spin]() {
        commitValue(spin->value());
    });
}

ArtifactBoolPropertyEditor::ArtifactBoolPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent)
{
    auto* checkBox = new QCheckBox(this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(checkBox);
    layout->addStretch();

    checkBox->setChecked(property.getValue().toBool());
    QObject::connect(checkBox, &QCheckBox::toggled, this, [this](const bool checked) {
        commitValue(checked);
    });
}

ArtifactStringPropertyEditor::ArtifactStringPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent)
{
    auto* lineEdit = new QLineEdit(property.getValue().toString(), this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(lineEdit);

    QObject::connect(lineEdit, &QLineEdit::editingFinished, this, [this, lineEdit]() {
        commitValue(lineEdit->text());
    });
}

ArtifactColorPropertyEditor::ArtifactColorPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent)
{
    auto* button = new QPushButton(QStringLiteral(" "), this);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(button);

    QColor color = propertyColor(property);
    const auto applyColor = [button](const QColor& nextColor) {
        button->setStyleSheet(QStringLiteral("background-color: %1;").arg(nextColor.name()));
    };

    applyColor(color);
    QObject::connect(button, &QPushButton::clicked, this, [this, button, color, applyColor]() mutable {
        const QColor nextColor = QColorDialog::getColor(color, button, QStringLiteral("Select Color"));
        if (!nextColor.isValid()) {
            return;
        }
        color = nextColor;
        applyColor(color);
        commitValue(color);
    });
}

ArtifactPropertyEditorRowWidget::ArtifactPropertyEditorRowWidget(
    const QString& labelText,
    ArtifactAbstractPropertyEditor* editor,
    const QString& propertyName,
    QWidget* parent)
    : QWidget(parent),
      label_(new QLabel(labelText, this)),
      editor_(editor),
      expressionButton_(new QPushButton(QString::fromUtf8("fx"), this))
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    label_->setMinimumWidth(96);
    editor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    expressionButton_->setToolTip(QStringLiteral("Expression Copilot: %1").arg(propertyName));
    expressionButton_->setFixedSize(24, 24);

    layout->addWidget(label_);
    layout->addWidget(editor_, 1);
    layout->addWidget(expressionButton_);

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

void ArtifactPropertyEditorRowWidget::setEditorToolTip(const QString& tooltip)
{
    label_->setToolTip(tooltip);
    editor_->setToolTip(tooltip);
    expressionButton_->setToolTip(tooltip);
}

void ArtifactPropertyEditorRowWidget::setShowExpressionButton(const bool visible)
{
    expressionButton_->setVisible(visible);
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

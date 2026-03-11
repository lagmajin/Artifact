module;

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVariant>
#include <QColor>

export module Artifact.Widgets.PropertyEditor;

import std;

import Property.Abstract;

export namespace Artifact {

class ArtifactAbstractPropertyEditor : public QWidget {
public:
    using CommitHandler = std::function<void(const QVariant&)>;

    explicit ArtifactAbstractPropertyEditor(QWidget* parent = nullptr);
    ~ArtifactAbstractPropertyEditor() override;

    void setCommitHandler(CommitHandler handler);
    virtual QVariant value() const = 0;
    virtual void setValueFromVariant(const QVariant& value) = 0;

protected:
    void commitValue(const QVariant& value) const;

private:
    CommitHandler commitHandler_;
};

class ArtifactFloatPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactFloatPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    class QDoubleSpinBox* spinBox_ = nullptr;
    class QSlider* slider_ = nullptr;
    double softMin_ = 0.0;
    double softMax_ = 1.0;
};

class ArtifactIntPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactIntPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    class QSpinBox* spinBox_ = nullptr;
    class QSlider* slider_ = nullptr;
    int softMin_ = 0;
    int softMax_ = 100;
};

class ArtifactBoolPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactBoolPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    class QCheckBox* checkBox_ = nullptr;
};

class ArtifactStringPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactStringPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    class QLineEdit* lineEdit_ = nullptr;
};

class ArtifactColorPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactColorPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    void applyColor(const class QColor& color);

    class QPushButton* button_ = nullptr;
    class QLabel* valueLabel_ = nullptr;
    class QColor currentColor_;
};

class ArtifactPropertyEditorRowWidget final : public QWidget {
public:
    explicit ArtifactPropertyEditorRowWidget(
        const QString& labelText,
        ArtifactAbstractPropertyEditor* editor,
        const QString& propertyName,
        QWidget* parent = nullptr);
    ~ArtifactPropertyEditorRowWidget() override;

    QLabel* label() const;
    ArtifactAbstractPropertyEditor* editor() const;
    void setExpressionHandler(std::function<void()> handler);
    void setResetHandler(std::function<void()> handler);
    void setEditorToolTip(const QString& tooltip);
    void setShowExpressionButton(bool visible);
    void setShowResetButton(bool visible);
    void setShowKeyframeButton(bool visible);

private:
    QLabel* label_ = nullptr;
    ArtifactAbstractPropertyEditor* editor_ = nullptr;
    QPushButton* keyframeButton_ = nullptr;
    QPushButton* resetButton_ = nullptr;
    QPushButton* expressionButton_ = nullptr;
    std::function<void()> expressionHandler_;
    std::function<void()> resetHandler_;
};

ArtifactAbstractPropertyEditor* createPropertyEditorWidget(
    const ArtifactCore::AbstractProperty& property,
    QWidget* parent = nullptr);

} // namespace Artifact

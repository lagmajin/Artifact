module;

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVariant>
#include <QColor>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>

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
    virtual bool supportsScrub() const;
    virtual void scrubByPixels(int deltaPixels, bool fineAdjust);

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
    QDoubleSpinBox* spinBox_ = nullptr;
    QSlider* slider_ = nullptr;
    double softMin_ = 0.0;
    double softMax_ = 1.0;
};

class ArtifactIntPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactIntPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    QSpinBox* spinBox_ = nullptr;
    QSlider* slider_ = nullptr;
    int softMin_ = 0;
    int softMax_ = 100;
};

class ArtifactBoolPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactBoolPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    QCheckBox* checkBox_ = nullptr;
};

class ArtifactStringPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactStringPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    QLineEdit* lineEdit_ = nullptr;
};

class ArtifactFontFamilyPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactFontFamilyPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    QFontComboBox* comboBox_ = nullptr;
};

class ArtifactPathPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactPathPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    QLineEdit* lineEdit_ = nullptr;
    QPushButton* browseButton_ = nullptr;
};

class ArtifactEnumPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    using OptionList = std::vector<std::pair<int, QString>>;

    ArtifactEnumPropertyEditor(const ArtifactCore::AbstractProperty& property, OptionList options, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    QComboBox* comboBox_ = nullptr;
    OptionList options_;
};

class ArtifactColorPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactColorPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    void applyColor(const QColor& color);

    QPushButton* button_ = nullptr;
    QLabel* valueLabel_ = nullptr;
    QColor currentColor_;
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
    bool scrubbing_ = false;
    int scrubStartX_ = 0;
    QVariant scrubStartValue_;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

ArtifactAbstractPropertyEditor* createPropertyEditorWidget(
    const ArtifactCore::AbstractProperty& property,
    QWidget* parent = nullptr);

} // namespace Artifact

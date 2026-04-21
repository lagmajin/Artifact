module;
#include <utility>

#include <functional>
#include <vector>

#include <QWidget>
#include <QAbstractButton>
#include <QLabel>
#include <QPaintEvent>
#include <QPushButton>
#include <QVariant>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QTextEdit>
export module Artifact.Widgets.PropertyEditor;

import Property.Abstract;
import Artifact.Widgets.FontPicker;
import Event.Bus;

export namespace Artifact {

enum class ArtifactPropertyRowLayoutMode {
    LabelThenEditor = 0,
    EditorThenLabel
};

enum class ArtifactNumericEditorLayoutMode {
    ValueThenSlider = 0,
    SliderThenValue
};

class ArtifactAbstractPropertyEditor : public QWidget {
public:
    using CommitHandler = std::function<void(const QVariant&)>;
    using PreviewHandler = std::function<void(const QVariant&)>;

    explicit ArtifactAbstractPropertyEditor(QWidget* parent = nullptr);
    ~ArtifactAbstractPropertyEditor() override;

    void setCommitHandler(CommitHandler handler);
    void setPreviewHandler(PreviewHandler handler);
    void previewCurrentValue() const;
    void previewValueFromVariant(const QVariant& value) const;
    void commitCurrentValue() const;
    virtual QVariant value() const = 0;
    virtual void setValueFromVariant(const QVariant& value) = 0;
    virtual bool supportsScrub() const;
    virtual void scrubByPixels(int deltaPixels, Qt::KeyboardModifiers modifiers);

protected:
    void commitValue(const QVariant& value) const;
    void previewValue(const QVariant& value) const;

private:
    CommitHandler commitHandler_;
    PreviewHandler previewHandler_;
};

ArtifactNumericEditorLayoutMode globalNumericEditorLayoutMode();
void setGlobalNumericEditorLayoutMode(ArtifactNumericEditorLayoutMode mode);
bool artifactShouldShowPropertyResetButtons();
void artifactSetShowPropertyResetButtons(bool show);

class ArtifactFloatPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactFloatPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr, bool showSlider = true);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;
    bool supportsScrub() const override;
    void scrubByPixels(int deltaPixels, Qt::KeyboardModifiers modifiers) override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void resetToDefaultValue(const ArtifactCore::AbstractProperty& property);
    int floatToSliderPosition(double val, double min, double max) const;
    double sliderPositionToFloat(int pos, double min, double max) const;

private:
    QDoubleSpinBox* spinBox_ = nullptr;
    QSlider* slider_ = nullptr;
    QPushButton* resetButton_ = nullptr;
    double softMin_ = 0.0;
    double softMax_ = 1.0;
    bool sliderInteracting_ = false;
};

class ArtifactIntPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactIntPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr, bool showSlider = true);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;
    bool supportsScrub() const override;
    void scrubByPixels(int deltaPixels, Qt::KeyboardModifiers modifiers) override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    int intToSliderPosition(int value, int min, int max) const;
    int sliderPositionToInt(int pos, int min, int max) const;

private:
    QSpinBox* spinBox_ = nullptr;
    QSlider* slider_ = nullptr;
    int softMin_ = 0;
    int softMax_ = 100;
    bool sliderInteracting_ = false;
};

class ArtifactBoolPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactBoolPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    QAbstractButton* toggleSwitch_ = nullptr;
};

class ArtifactStringPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactStringPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    QLineEdit* lineEdit_ = nullptr;
};

class ArtifactMultilineStringPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactMultilineStringPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QTextEdit* textEdit_ = nullptr;
};

class ArtifactFontFamilyPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactFontFamilyPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    FontPickerWidget* fontPicker_ = nullptr;
    ArtifactCore::EventBus::Subscription fontChangeSubscription_;
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

class ArtifactObjectReferencePropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactObjectReferencePropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private slots:
    void onReferencePicked();
    void onReferenceChanged(qint64 newId);

private:
    QWidget* referenceWidget_ = nullptr;
    qint64 currentId_ = -1;
};

class ArtifactPropertyEditorRowWidget final : public QWidget {
public:
    using KeyFrameHandler = std::function<void(bool)>;
    using NavigationHandler = std::function<void(int)>; // -1 for prev, 1 for next

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
    void setKeyframeHandler(KeyFrameHandler handler);
    void setNavigationHandler(NavigationHandler handler);
    
    void setEditorToolTip(const QString& tooltip);
    void setSupplementaryText(const QString& text);
    void setShowExpressionButton(bool visible);
    void setShowResetButton(bool visible);
    void setShowKeyframeButton(bool visible);
    
    void setKeyframeChecked(bool checked);
    void setKeyframeEnabled(bool enabled);
    void setNavigationEnabled(bool enabled);
    static void setGlobalLayoutMode(ArtifactPropertyRowLayoutMode mode);
    static ArtifactPropertyRowLayoutMode globalLayoutMode();

private:
    void finishScrub(bool commitChanges);
    void updateKeyframeButtonIcon();

    QLabel* label_ = nullptr;
    QLabel* scrubHandle_ = nullptr;
    QLabel* supplementaryLabel_ = nullptr;
    ArtifactAbstractPropertyEditor* editor_ = nullptr;
    QPushButton* keyframeButton_ = nullptr;
    QPushButton* resetButton_ = nullptr;
    QPushButton* expressionButton_ = nullptr;
    QPushButton* prevKeyBtn_ = nullptr;
    QPushButton* nextKeyBtn_ = nullptr;
    
    std::function<void()> expressionHandler_;
    std::function<void()> resetHandler_;
    KeyFrameHandler keyframeHandler_;
    NavigationHandler navigationHandler_;
    
    bool scrubbing_ = false;
    bool scrubStarted_ = false;
    int scrubStartX_ = 0;
    int scrubThreshold_ = 4;
    QVariant scrubStartValue_;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void updateAuxControlVisibility();

private:
    QString propertyName_;
};

ArtifactAbstractPropertyEditor* createPropertyEditorWidget(
    const ArtifactCore::AbstractProperty& property,
    QWidget* parent = nullptr);

} // namespace Artifact

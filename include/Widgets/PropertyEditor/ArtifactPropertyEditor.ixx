module;
#include <utility>

#include <functional>
#include <optional>
#include <vector>

#include <QWidget>
#include <QAbstractButton>
#include <QLabel>
#include <QPaintEvent>
#include <QPoint>
#include <QPushButton>
#include <QVariant>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QTextEdit>
#include <QWheelEvent>
#include <wobjectdefs.h>
export module Artifact.Widgets.PropertyEditor;

import Property.Abstract;
import Artifact.Layer.Text;
import Artifact.Widgets.FontPicker;
import Event.Bus;

export namespace Artifact {
namespace detail {
class PropertyComboBox final : public QComboBox {
public:
    explicit PropertyComboBox(QWidget* parent = nullptr);
protected:
    void wheelEvent(QWheelEvent* event) override;
};

class PropertySliderWidget final : public QSlider {
public:
    explicit PropertySliderWidget(QWidget* parent = nullptr);
    void setDisplayText(QString text);
protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
private:
    QString displayText_;
};

class PropertyCallbackButton final : public QPushButton {
public:
    using Callback = std::function<void()>;

    explicit PropertyCallbackButton(const QString& text, QWidget* parent = nullptr);
    void setCallback(Callback callback);
protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
private:
    Callback callback_;
};

class PropertyRotationKnobWidget final : public QWidget {
public:
    using ValueHandler = std::function<void(double)>;
    explicit PropertyRotationKnobWidget(QWidget* parent = nullptr);
    void setValue(double value);
    double value() const;
    void setRange(double minimum, double maximum);
    void setPreviewHandler(ValueHandler handler);
    void setCommitHandler(ValueHandler handler);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;
private:
    double angleFromPosition(const QPointF& position) const;
    double value_ = 0.0;
    double minimum_ = 0.0;
    double maximum_ = 360.0;
    double lastAngle_ = 0.0;
    bool dragging_ = false;
    ValueHandler previewHandler_;
    ValueHandler commitHandler_;
};

class ArtifactToggleSwitch final : public QAbstractButton {
public:
    explicit ArtifactToggleSwitch(QWidget* parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
protected:
    bool hitButton(const QPoint& pos) const override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
};

void applyPropertyFieldPalette(QWidget* widget, bool elevated = false);
void applyPropertyButtonPalette(QAbstractButton* button, bool accent = false);
void applyPropertyLabelPalette(QLabel* label, bool prominent = false);
void applyThemeTextPalette(QWidget* widget, int shade = 100);
QColor themeColor(const QString& value, const QColor& fallback);
QColor blendColor(const QColor& a, const QColor& b, qreal t);
QColor propertySurfaceColor(bool elevated = false);
QString fileDialogFilterForProperty(const QString& propertyName);
QColor propertyColor(const ArtifactCore::AbstractProperty& property);
bool isPathProperty(const ArtifactCore::AbstractProperty& property);
bool isFontFamilyProperty(const ArtifactCore::AbstractProperty& property);
bool isMultilineTextProperty(const ArtifactCore::AbstractProperty& property);
bool shouldShowNumericSlider(const ArtifactCore::AbstractProperty& property);
int intToSliderPosition(int value, int min, int max);
int sliderPositionToInt(int pos, int min, int max);
std::optional<std::vector<std::pair<int, QString>>>
enumOptionsForProperty(const ArtifactCore::AbstractProperty& property);
bool artifactShouldShowPropertyResetButtonsImpl();
void artifactSetShowPropertyResetButtonsImpl(bool show);
}

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
    virtual QWidget* scrubTargetWidget() const;

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
QVariant getPropertyDefaultValue(const ArtifactCore::AbstractProperty& property);

class ArtifactFloatPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactFloatPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr, bool showSlider = true);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;
    bool supportsScrub() const override;
    void scrubByPixels(int deltaPixels, Qt::KeyboardModifiers modifiers) override;
    QWidget* scrubTargetWidget() const override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void resetToDefaultValue(const ArtifactCore::AbstractProperty& property);
    int floatToSliderPosition(double val, double min, double max) const;
    double sliderPositionToFloat(int pos, double min, double max) const;

private:
    QDoubleSpinBox* spinBox_ = nullptr;
    QSlider* slider_ = nullptr;
    QWidget* knob_ = nullptr;
    QPushButton* resetButton_ = nullptr;
    double softMin_ = 0.0;
    double softMax_ = 1.0;
    bool sliderInteracting_ = false;
    bool sliderDragArmed_ = false;
    bool sliderDragActive_ = false;
    QPoint sliderDragStartPos_;
    int sliderDragStartValue_ = 0;
};

class ArtifactIntPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactIntPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr, bool showSlider = true);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;
    bool supportsScrub() const override;
    void scrubByPixels(int deltaPixels, Qt::KeyboardModifiers modifiers) override;
    QWidget* scrubTargetWidget() const override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    int intToSliderPosition(int value, int min, int max) const;
    int sliderPositionToInt(int pos, int min, int max) const;

private:
    QSpinBox* spinBox_ = nullptr;
    QSlider* slider_ = nullptr;
    QWidget* knob_ = nullptr;
    int softMin_ = 0;
    int softMax_ = 100;
    bool sliderInteracting_ = false;
    bool sliderDragArmed_ = false;
    bool sliderDragActive_ = false;
    QPoint sliderDragStartPos_;
    int sliderDragStartValue_ = 0;
};

class ArtifactAnimatorCountPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactAnimatorCountPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    void stepCount(int delta);
    void syncUi();

private:
    QLabel* countLabel_ = nullptr;
    detail::PropertyCallbackButton* removeButton_ = nullptr;
    detail::PropertyCallbackButton* addButton_ = nullptr;
    int currentCount_ = 0;
    int minCount_ = 0;
    int maxCount_ = 16;
};

class ArtifactDashPatternPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactDashPatternPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private:
    void applyPreset(const QString& pattern);
    QComboBox* presetCombo_ = nullptr;
    QLineEdit* customEdit_ = nullptr;
};

class ArtifactRotationPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactRotationPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;
    bool supportsScrub() const override;
    void scrubByPixels(int deltaPixels, Qt::KeyboardModifiers modifiers) override;
    QWidget* scrubTargetWidget() const override;

private:
    QDoubleSpinBox* spinBox_ = nullptr;
    QWidget* knob_ = nullptr;
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

class ArtifactTextAnimatorColorEditor final : public ArtifactAbstractPropertyEditor {
    W_OBJECT(ArtifactTextAnimatorColorEditor)
public:
    explicit ArtifactTextAnimatorColorEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

    void setLayer(ArtifactTextLayer* layer) { layer_ = layer; }

public:
    void colorApplied(int start, int end, QColor color) W_SIGNAL(colorApplied, start, end, color);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void onSelectionChanged();
    void onColorPicked();

    QTextEdit* textEdit_ = nullptr;
    QPushButton* colorButton_ = nullptr;
    ArtifactTextLayer* layer_ = nullptr;
};

class ArtifactObjectReferencePropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactObjectReferencePropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
    QVariant value() const override;
    void setValueFromVariant(const QVariant& value) override;

private slots:
    void onReferencePicked();
    void onReferenceChanged(qint64 newId);
    void updateReferenceDisplay();

private:
    QWidget* referenceWidget_ = nullptr;
    QLabel* valueLabel_ = nullptr;
    QPushButton* pickButton_ = nullptr;
    QPushButton* clearButton_ = nullptr;
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
    QString propertyName() const;
    void setExpressionHandler(std::function<void()> handler);
    void setResetHandler(std::function<void()> handler);
    void setAuxAction(std::function<void()> handler, const QString& label);
    void setKeyframeHandler(KeyFrameHandler handler);
    void setNavigationHandler(NavigationHandler handler);
    
    void setEditorToolTip(const QString& tooltip);
    void setSupplementaryText(const QString& text);
    void setShowExpressionButton(bool visible);
    void setShowResetButton(bool visible);
    void setShowKeyframeButton(bool visible);
    void setShowFavoriteButton(bool visible);
    void setFavoriteChecked(bool checked);
    bool isFavoriteChecked() const;
    void setFavoriteHandler(std::function<void(bool)> handler);
    
    void setKeyframeChecked(bool checked);
    void setKeyframeModeEnabled(bool enabled);
    void setKeyframeAnchorHandler(std::function<void(ArtifactCore::KeyFrame::Anchor)> handler);
    void setKeyframeColorLabelHandler(std::function<void(ArtifactCore::KeyFrame::ColorLabel)> handler);
    bool isKeyframeModeEnabled() const;
    void setKeyframeEnabled(bool enabled);
    void setNavigationEnabled(bool enabled);
    static void setGlobalLayoutMode(ArtifactPropertyRowLayoutMode mode);
    static ArtifactPropertyRowLayoutMode globalLayoutMode();

private:
    void finishScrub(bool commitChanges);
    void updateKeyframeButtonIcon();

    QLabel* label_ = nullptr;
    QWidget* scrubTarget_ = nullptr;
    QLabel* supplementaryLabel_ = nullptr;
    ArtifactAbstractPropertyEditor* editor_ = nullptr;
    QPushButton* keyframeButton_ = nullptr;
    QPushButton* resetButton_ = nullptr;
    QPushButton* expressionButton_ = nullptr;
    QPushButton* favoriteButton_ = nullptr;
    QPushButton* prevKeyBtn_ = nullptr;
    QPushButton* nextKeyBtn_ = nullptr;
    
    std::function<void()> expressionHandler_;
    std::function<void()> resetHandler_;
    std::function<void()> auxActionHandler_;
    std::function<void(bool)> favoriteHandler_;
    KeyFrameHandler keyframeHandler_;
    NavigationHandler navigationHandler_;
    std::function<void(ArtifactCore::KeyFrame::Anchor)> keyframeAnchorHandler_;
    std::function<void(ArtifactCore::KeyFrame::ColorLabel)> keyframeColorLabelHandler_;
    bool currentFrameKeyframed_ = false;
    QString auxActionLabel_;
    
    bool scrubCandidate_ = false;
    bool scrubbing_ = false;
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

module;
#include <utility>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEnterEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFont>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QHash>
#include <QLocale>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPalette>
#include <QPushButton>
#include <QFrame>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QWheelEvent>
#include <QStyleOptionSlider>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtSVG/QSvgRenderer>
#include <wobjectimpl.h>

module Artifact.Widgets.PropertyEditor;

import std;
import Utils.Path;
import Color.Float;
import Font.FreeFont;
import Artifact.Widgets.FontPicker;
import Artifact.Widgets.ObjectPicker;
import Artifact.Widgets.RelativeSpinBox;
import Artifact.Event.Types;
import Artifact.Widgets.ExpressionCopilotWidget;
import Artifact.Service.Playback;
import Artifact.Service.Project;
import Artifact.Widgets.Timeline.KeyframeIconHelper;
import Time.Rational;
import FloatColorPickerDialog;
import Artifact.Widgets.Dialog.FloatColorPickerHooks;
import Widgets.Utils.CSS;
import Artifact.Layer.Text;

namespace Artifact {

W_OBJECT_IMPL(ArtifactTextAnimatorColorEditor)

using namespace detail;

bool artifactShouldShowPropertyResetButtons() {
  return artifactShouldShowPropertyResetButtonsImpl();
}

void artifactSetShowPropertyResetButtons(bool show) {
  artifactSetShowPropertyResetButtonsImpl(show);
}

ArtifactEnumPropertyEditor::ArtifactEnumPropertyEditor(
ArtifactPropertyEditorRowWidget::ArtifactPropertyEditorRowWidget(
    const QString &labelText, ArtifactAbstractPropertyEditor *editor,
    const QString &propertyName, QWidget *parent)
    : QWidget(parent), label_(new QLabel(labelText, this)),
      editor_(editor), keyframeButton_(new QPushButton(this)),
      resetButton_(new QPushButton(this)),
      expressionButton_(new QPushButton(this)),
      prevKeyBtn_(new QPushButton(this)), nextKeyBtn_(new QPushButton(this)),
      propertyName_(propertyName) {
  setObjectName(QStringLiteral("propertyRow"));
  setFocusPolicy(Qt::StrongFocus);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setMinimumHeight(kPropertyRowMinHeight);


std::optional<ArtifactEnumPropertyEditor::OptionList>
parseTooltipEnumOptions(const QString &tooltip) {
  ArtifactEnumPropertyEditor::OptionList options;
  const auto entries = tooltip.split(',', Qt::SkipEmptyParts);
  for (const QString &rawEntry : entries) {
    const QString entry = rawEntry.trimmed();
    const int equalIndex = entry.indexOf('=');
    if (equalIndex <= 0 || equalIndex + 1 >= entry.size()) {
      return std::nullopt;
    }
    bool ok = false;
    const int value = entry.left(equalIndex).trimmed().toInt(&ok);
    if (!ok) {
      return std::nullopt;
    }
    const QString label = entry.mid(equalIndex + 1).trimmed();
    if (label.isEmpty()) {
      return std::nullopt;
    }
    options.emplace_back(value, label);
  }
  if (options.empty()) {
    return std::nullopt;
  }
  return options;
}

std::optional<ArtifactEnumPropertyEditor::OptionList>
enumOptionsForProperty(const ArtifactCore::AbstractProperty &property) {
  if (property.getType() != ArtifactCore::PropertyType::Integer) {
    return std::nullopt;
  }

  const auto meta = property.metadata();
  if (!meta.tooltip.isEmpty()) {
    if (const auto parsed = parseTooltipEnumOptions(meta.tooltip)) {
      return parsed;
    }
  }

  const QString name = property.getName();
  if (name == QStringLiteral("waveType")) {
    return ArtifactEnumPropertyEditor::OptionList{
        {0, QStringLiteral("Sine")}, {1, QStringLiteral("Cosine")}};
  }
  if (name == QStringLiteral("text.animatorPreset")) {
    return ArtifactEnumPropertyEditor::OptionList{
        {0, QStringLiteral("None")},
        {1, QStringLiteral("Typewriter")},
        {2, QStringLiteral("Slide Up")},
        {3, QStringLiteral("Scale In")},
        {4, QStringLiteral("Rotation In")},
        {5, QStringLiteral("Tracking Fade")},
        {6, QStringLiteral("Wiggly Position")},
        {7, QStringLiteral("Blur Reveal")}};
  }
  if (name == QStringLiteral("layer.cachePolicy")) {
    return ArtifactEnumPropertyEditor::OptionList{
        {0, QStringLiteral("Default")},
        {1, QStringLiteral("Enabled")},
        {2, QStringLiteral("Disabled")}};
  }
  if (name == QStringLiteral("component.cloner.mode")) {
    return ArtifactEnumPropertyEditor::OptionList{
        {0, QStringLiteral("Linear")},
        {1, QStringLiteral("Linear Jitter")},
        {2, QStringLiteral("Curve")},
        {3, QStringLiteral("Random")},
        {4, QStringLiteral("Spline")},
        {5, QStringLiteral("Grid")},
        {6, QStringLiteral("Radial")}};
  }
  if (name == QStringLiteral("orientation")) {
    return ArtifactEnumPropertyEditor::OptionList{
        {0, QStringLiteral("Horizontal")}, {1, QStringLiteral("Vertical")}};
  }
  if (name.startsWith(QStringLiteral("mask."), Qt::CaseInsensitive) &&
      name.endsWith(QStringLiteral(".mode"), Qt::CaseInsensitive)) {
    return ArtifactEnumPropertyEditor::OptionList{
        {0, QStringLiteral("Add")},
        {1, QStringLiteral("Subtract")},
        {2, QStringLiteral("Intersect")},
        {3, QStringLiteral("Difference")}};
  }
  if (name == QStringLiteral("shape.strokeCap")) {
    return ArtifactEnumPropertyEditor::OptionList{
        {0, QStringLiteral("Flat")},
        {1, QStringLiteral("Round")},
        {2, QStringLiteral("Square")}};
  }
  if (name == QStringLiteral("shape.strokeJoin")) {
    return ArtifactEnumPropertyEditor::OptionList{
        {0, QStringLiteral("Miter")},
        {1, QStringLiteral("Round")},
        {2, QStringLiteral("Bevel")}};
  }
  if (name == QStringLiteral("shape.strokeAlign")) {
    return ArtifactEnumPropertyEditor::OptionList{
        {0, QStringLiteral("Center")},
        {1, QStringLiteral("Inside")},
        {2, QStringLiteral("Outside")}};
  }
  if (name == QStringLiteral("shape.fillType") ||
      name == QStringLiteral("solid.fillType")) {
    return ArtifactEnumPropertyEditor::OptionList{
        {0, QStringLiteral("Solid")},
        {1, QStringLiteral("Linear Gradient")},
        {2, QStringLiteral("Radial Gradient")},
        {3, QStringLiteral("Conical Gradient")}};
  }

  return std::nullopt;
}

std::pair<double, double>
resolveFloatSoftRange(const ArtifactCore::AbstractProperty &property,
                      const ArtifactCore::PropertyMetadata &meta,
                      const double hardMin, const double hardMax) {
  if (meta.softMin.isValid() && meta.softMax.isValid()) {
    const double softMin = meta.softMin.toDouble();
    const double softMax = meta.softMax.toDouble();
    if (softMax > softMin) {
      return {softMin, softMax};
    }
  }

  // If hard range is explicit, reuse it for slider range.
  if (meta.hardMin.isValid() && meta.hardMax.isValid() && hardMax > hardMin) {
    return {hardMin, hardMax};
  }

  // Adaptive fallback around current value.
  const double currentValue = property.getValue().toDouble();
  const double step =
      std::abs(meta.step.isValid() ? meta.step.toDouble() : 0.0);
  const double halfSpan =
      std::max({std::abs(currentValue) * 0.5, step * 50.0, 1.0});
  double softMin = currentValue - halfSpan;
  double softMax = currentValue + halfSpan;
  if (hardMax > hardMin) {
    softMin = std::clamp(softMin, hardMin, hardMax);
    softMax = std::clamp(softMax, hardMin, hardMax);
  }
  if (softMax <= softMin) {
    return {hardMin, hardMax};
  }
  return {softMin, softMax};
}

std::pair<int, int>
resolveIntSoftRange(const ArtifactCore::AbstractProperty &property,
                    const ArtifactCore::PropertyMetadata &meta,
                    const int hardMin, const int hardMax) {
  if (meta.softMin.isValid() && meta.softMax.isValid()) {
    const int softMin = meta.softMin.toInt();
    const int softMax = meta.softMax.toInt();
    if (softMax > softMin) {
      return {softMin, softMax};
    }
  }

  // If hard range is explicit, reuse it for slider range.
  if (meta.hardMin.isValid() && meta.hardMax.isValid() && hardMax > hardMin) {
    return {hardMin, hardMax};

  }
  auto *spinBox = static_cast<ArtifactRelativeSpinBox *>(spinBox_);
  if (auto *lineEdit = spinBox->scrubLineEdit()) {
    return lineEdit;
  }
  return ArtifactAbstractPropertyEditor::scrubTargetWidget();
}

bool ArtifactIntPropertyEditor::eventFilter(QObject *watched, QEvent *event) {
  if (!slider_ || watched != slider_) {
    return ArtifactAbstractPropertyEditor::eventFilter(watched, event);
  }

  auto sliderValueForX = [this](const int x) {
    const double ratio =
        static_cast<double>(x) / static_cast<double>(std::max(1, slider_->width()));
    return static_cast<int>(std::round(std::clamp(ratio, 0.0, 1.0) *
                                       static_cast<double>(slider_->maximum())));
  };

  if (event->type() == QEvent::MouseButtonPress) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      sliderDragArmed_ = true;
      sliderDragActive_ = false;
      sliderInteracting_ = true;
      sliderDragStartPos_ = mouseEvent->pos();
      sliderDragStartValue_ = slider_->value();
      slider_->setSliderDown(true);
      return true;
    }
  }
  if (event->type() == QEvent::MouseMove && sliderDragArmed_) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (!(mouseEvent->buttons() & Qt::LeftButton)) {
      sliderDragArmed_ = false;
      sliderDragActive_ = false;
      sliderInteracting_ = false;
      slider_->setSliderDown(false);
      return true;
    }
    if (!sliderDragActive_ &&
        (mouseEvent->pos() - sliderDragStartPos_).manhattanLength() <
            QApplication::startDragDistance()) {
      return true;
    }
    sliderDragActive_ = true;
    slider_->setValue(sliderValueForX(mouseEvent->pos().x()));
    return true;
  }
  if (event->type() == QEvent::MouseButtonRelease && sliderDragArmed_) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      if (sliderDragActive_) {
        slider_->setValue(sliderValueForX(mouseEvent->pos().x()));
      } else {
        const QSignalBlocker blocker(slider_);
        slider_->setValue(sliderDragStartValue_);
      }
      sliderDragArmed_ = false;
      const bool shouldCommit = sliderDragActive_;
      sliderDragActive_ = false;
      sliderInteracting_ = false;
      slider_->setSliderDown(false);
      if (shouldCommit) {
        commitCurrentValue();
      }
      return true;
    }
  }
  return ArtifactAbstractPropertyEditor::eventFilter(watched, event);
}

int ArtifactIntPropertyEditor::intToSliderPosition(int value, int min,
                                                   int max) const {
  return detail::intToSliderPosition(value, min, max);
}

int ArtifactIntPropertyEditor::sliderPositionToInt(int pos, int min,
                                                   int max) const {
  return detail::sliderPositionToInt(pos, min, max);
}

void ArtifactIntPropertyEditor::scrubByPixels(
    const int deltaPixels, const Qt::KeyboardModifiers modifiers) {
  if (!spinBox_) {
    return;
  }
  const int baseStep = std::max(1, spinBox_->singleStep());
  double scaledStep = static_cast<double>(baseStep);
  if (modifiers.testFlag(Qt::ShiftModifier)) {
    scaledStep *= 0.25;
  }
  if (modifiers.testFlag(Qt::ControlModifier)) {
    scaledStep *= 5.0;
  }
  const int nextValue = static_cast<int>(
      std::llround(static_cast<double>(spinBox_->value()) +
                   static_cast<double>(deltaPixels) * scaledStep));
  spinBox_->setValue(nextValue);
}

ArtifactAnimatorCountPropertyEditor::ArtifactAnimatorCountPropertyEditor(

QLabel *ArtifactPropertyEditorRowWidget::label() const { return label_; }

ArtifactAbstractPropertyEditor *
ArtifactPropertyEditorRowWidget::editor() const {
  return editor_;
}

QString ArtifactPropertyEditorRowWidget::propertyName() const { return propertyName_; }

void ArtifactPropertyEditorRowWidget::setGlobalLayoutMode(
    const ArtifactPropertyRowLayoutMode mode) {
  g_propertyRowLayoutMode = mode;
}

ArtifactPropertyRowLayoutMode
ArtifactPropertyEditorRowWidget::globalLayoutMode() {
  return g_propertyRowLayoutMode;
}

void setGlobalNumericEditorLayoutMode(
    const ArtifactNumericEditorLayoutMode mode) {
  g_numericEditorLayoutMode = mode;
}

ArtifactNumericEditorLayoutMode globalNumericEditorLayoutMode() {
  return g_numericEditorLayoutMode;
}

void ArtifactPropertyEditorRowWidget::setExpressionHandler(
    std::function<void()> handler) {
  expressionHandler_ = std::move(handler);
}

void ArtifactPropertyEditorRowWidget::setResetHandler(
    std::function<void()> handler) {
  resetHandler_ = std::move(handler);
}

void ArtifactPropertyEditorRowWidget::setKeyframeHandler(
    KeyFrameHandler handler) {
  keyframeHandler_ = std::move(handler);
}

void ArtifactPropertyEditorRowWidget::setKeyframeAnchorHandler(
    std::function<void(ArtifactCore::KeyFrame::Anchor)> handler) {
  keyframeAnchorHandler_ = std::move(handler);
}

void ArtifactPropertyEditorRowWidget::setKeyframeColorLabelHandler(
    std::function<void(ArtifactCore::KeyFrame::ColorLabel)> handler) {
  keyframeColorLabelHandler_ = std::move(handler);
}

void ArtifactPropertyEditorRowWidget::setNavigationHandler(
    NavigationHandler handler) {
  navigationHandler_ = std::move(handler);
}

void ArtifactPropertyEditorRowWidget::setEditorToolTip(const QString &tooltip) {
  label_->setToolTip(tooltip);
  editor_->setToolTip(tooltip);
  if (scrubTarget_) {
    scrubTarget_->setToolTip(
        editor_ && editor_->supportsScrub()
            ? tooltip +
                  QStringLiteral(
                      "\nDrag to scrub. Shift=fine, Ctrl=coarse, Esc=cancel.")
            : tooltip);
  }
  if (supplementaryLabel_) {
    supplementaryLabel_->setToolTip(tooltip);
  }
  keyframeButton_->setToolTip(
      QStringLiteral("Toggle keyframe: %1").arg(propertyName_));
  prevKeyBtn_->setToolTip(
      QStringLiteral("Previous keyframe: %1").arg(propertyName_));
  nextKeyBtn_->setToolTip(
      QStringLiteral("Next keyframe: %1").arg(propertyName_));
  resetButton_->setToolTip(QStringLiteral("Reset: %1").arg(propertyName_));
  expressionButton_->setToolTip(
      QStringLiteral("Expression: %1").arg(propertyName_));
}

void ArtifactPropertyEditorRowWidget::setSupplementaryText(
    const QString &text) {
  if (!supplementaryLabel_) {
    return;
  }
  const QString trimmed = text.trimmed();
  supplementaryLabel_->setText(trimmed);
  supplementaryLabel_->setVisible(!trimmed.isEmpty());
}

void ArtifactPropertyEditorRowWidget::setShowExpressionButton(
    const bool visible) {
  expressionButton_->setProperty("baseVisible", visible);
  updateAuxControlVisibility();
  update();
}

void ArtifactPropertyEditorRowWidget::setShowResetButton(const bool visible) {
  resetButton_->setProperty("baseVisible", visible);
  updateAuxControlVisibility();
  update();
}

void ArtifactPropertyEditorRowWidget::setShowKeyframeButton(
    const bool visible) {
  keyframeButton_->setVisible(visible);
  updateKeyframeButtonIcon();
  updateAuxControlVisibility();
  update();
}

void ArtifactPropertyEditorRowWidget::setShowFavoriteButton(const bool visible) {
  favoriteButton_->setProperty("baseVisible", visible);
  updateAuxControlVisibility();
  update();
}

void ArtifactPropertyEditorRowWidget::setFavoriteChecked(const bool checked) {
  favoriteButton_->setChecked(checked);
  favoriteButton_->setText(checked
      ? QStringLiteral("\u2605")  // ★
      : QStringLiteral("\u2606")); // ☆
}

bool ArtifactPropertyEditorRowWidget::isFavoriteChecked() const {
  return favoriteButton_->isChecked();
}

void ArtifactPropertyEditorRowWidget::setFavoriteHandler(
    std::function<void(bool)> handler) {
  favoriteHandler_ = std::move(handler);
}

void ArtifactPropertyEditorRowWidget::updateKeyframeButtonIcon() {
  if (!keyframeButton_) {
    return;
  }
  const bool modeEnabled = keyframeButton_->isChecked();
  const bool hasCurrentFrameKey = currentFrameKeyframed_;
  const bool enabled = keyframeButton_->isEnabled();
  QColor fillColor = QColor(QStringLiteral("#6E7681"));
  QColor outlineColor = QColor(QStringLiteral("#B6C0CD"));
  KeyframeIconState state = KeyframeIconState::Normal;
  if (modeEnabled) {
    fillColor = QColor(QStringLiteral("#A786FF"));
    outlineColor = QColor(QStringLiteral("#D9CCFF"));
    state = KeyframeIconState::Selected;
  }
  if (hasCurrentFrameKey) {
    fillColor = QColor(QStringLiteral("#FFD84D"));
    outlineColor = QColor(QStringLiteral("#FFF1A8"));
    state = KeyframeIconState::Selected;
  }
  if (!enabled) {
    fillColor.setAlpha(110);
    outlineColor.setAlpha(130);
  }
  KeyframeIconStyle style;
  style.size = QSize(14, 14);
  style.fillColor = fillColor;
  style.outlineColor = outlineColor;
  style.state = state;
  keyframeButton_->setIcon(cachedKeyframeIcon(style));
  keyframeButton_->setToolTip(
      hasCurrentFrameKey
          ? QStringLiteral("Current frame has a keyframe: %1").arg(propertyName_)
          : modeEnabled
                ? QStringLiteral("Animation enabled for property: %1").arg(propertyName_)
                : QStringLiteral("Toggle keyframe at current frame: %1")
                      .arg(propertyName_));
}

void ArtifactPropertyEditorRowWidget::setKeyframeChecked(const bool checked) {
  currentFrameKeyframed_ = checked;
  updateKeyframeButtonIcon();
  update();
}

void ArtifactPropertyEditorRowWidget::setKeyframeModeEnabled(
    const bool enabled) {
  const QSignalBlocker blocker(keyframeButton_);
  keyframeButton_->setChecked(enabled);
  updateKeyframeButtonIcon();
  update();
}

bool ArtifactPropertyEditorRowWidget::isKeyframeModeEnabled() const {
  return keyframeButton_ && keyframeButton_->isChecked();
}

void ArtifactPropertyEditorRowWidget::setKeyframeEnabled(const bool enabled) {
  keyframeButton_->setEnabled(enabled);
  updateKeyframeButtonIcon();
  update();
}

void ArtifactPropertyEditorRowWidget::setNavigationEnabled(const bool enabled) {
  prevKeyBtn_->setProperty("baseVisible", enabled);
  nextKeyBtn_->setProperty("baseVisible", enabled);
  prevKeyBtn_->setEnabled(enabled);
  nextKeyBtn_->setEnabled(enabled);
  const QString prevToolTip = enabled
                                  ? QStringLiteral("Previous keyframe: %1")
                                        .arg(propertyName_)
                                  : QStringLiteral("No previous keyframe available: %1")
                                        .arg(propertyName_);
  const QString nextToolTip = enabled
                                  ? QStringLiteral("Next keyframe: %1").arg(propertyName_)
                                  : QStringLiteral("No next keyframe available: %1")
                                        .arg(propertyName_);
  prevKeyBtn_->setToolTip(prevToolTip);
  nextKeyBtn_->setToolTip(nextToolTip);
  updateAuxControlVisibility();
  update();
}

void ArtifactPropertyEditorRowWidget::updateAuxControlVisibility() {
  const bool hover = underMouse();
  const bool keyVisible = keyframeButton_->isVisible();
  const bool resetVisible = resetButton_->property("baseVisible").toBool();
  const bool exprVisible = expressionButton_->property("baseVisible").toBool();
  const bool navVisible = prevKeyBtn_->property("baseVisible").toBool() &&
                          nextKeyBtn_->property("baseVisible").toBool();
  const bool favVisible = favoriteButton_->property("baseVisible").toBool();

  resetButton_->setVisible(resetVisible && hover);
  expressionButton_->setVisible(exprVisible && hover);
  favoriteButton_->setVisible(favVisible && (hover || favoriteButton_->isChecked()));
  prevKeyBtn_->setVisible(keyVisible && navVisible);
  nextKeyBtn_->setVisible(keyVisible && navVisible);

  resetButton_->setEnabled(resetVisible && hover);
  expressionButton_->setEnabled(exprVisible && hover);
  favoriteButton_->setEnabled(favVisible && hover);
  prevKeyBtn_->setEnabled(navVisible);
  nextKeyBtn_->setEnabled(navVisible);
}

void ArtifactPropertyEditorRowWidget::enterEvent(QEnterEvent *event) {
  QWidget::enterEvent(event);
  updateAuxControlVisibility();
  update();
}

void ArtifactPropertyEditorRowWidget::leaveEvent(QEvent *event) {
  QWidget::leaveEvent(event);
  updateAuxControlVisibility();
  update();
}

void ArtifactPropertyEditorRowWidget::contextMenuEvent(
    QContextMenuEvent *event) {
  if (!event) {
    return;
  }

  QMenu menu(this);
  QAction *copyAction = menu.addAction(QStringLiteral("Copy Value"));
  QAction *pasteAction = menu.addAction(QStringLiteral("Paste Value"));
  QAction *resetAction = menu.addAction(QStringLiteral("Reset Value"));
  QAction *copyNameAction =
      menu.addAction(QStringLiteral("Copy Property Name"));
  QMenu *anchorMenu = nullptr;
  QAction *anchorAbsoluteAction = nullptr;
  QAction *anchorLockToInAction = nullptr;
  QAction *anchorLockToOutAction = nullptr;
  QAction *anchorStretchAction = nullptr;
  QMenu *colorMenu = nullptr;
  QAction *colorNoneAction = nullptr;
  QAction *colorRedAction = nullptr;
  QAction *colorBlueAction = nullptr;
  QAction *colorYellowAction = nullptr;
  QAction *colorGreenAction = nullptr;
  QAction *colorPurpleAction = nullptr;
  QAction *colorGrayAction = nullptr;
  if (keyframeAnchorHandler_) {
    anchorMenu = menu.addMenu(QStringLiteral("Keyframe Anchor"));
    anchorAbsoluteAction = anchorMenu->addAction(QStringLiteral("Absolute"));
    anchorLockToInAction = anchorMenu->addAction(QStringLiteral("Lock to In Point"));
    anchorLockToOutAction = anchorMenu->addAction(QStringLiteral("Lock to Out Point"));
    anchorStretchAction = anchorMenu->addAction(QStringLiteral("Stretch with Layer"));
  }
  if (keyframeColorLabelHandler_) {
    colorMenu = menu.addMenu(QStringLiteral("Keyframe Color Label"));
    colorNoneAction = colorMenu->addAction(QStringLiteral("None"));
    colorRedAction = colorMenu->addAction(QStringLiteral("Red"));
    colorBlueAction = colorMenu->addAction(QStringLiteral("Blue"));
    colorYellowAction = colorMenu->addAction(QStringLiteral("Yellow"));
    colorGreenAction = colorMenu->addAction(QStringLiteral("Green"));
    colorPurpleAction = colorMenu->addAction(QStringLiteral("Purple"));
    colorGrayAction = colorMenu->addAction(QStringLiteral("Gray"));
  }

  copyAction->setEnabled(editor_ != nullptr);
  pasteAction->setEnabled(editor_ != nullptr);
  resetAction->setEnabled(resetButton_ &&
                          resetButton_->property("baseVisible").toBool());

  QAction *chosen = menu.exec(event->globalPos());
  if (!chosen) {
    event->accept();
    return;
  }

  if (chosen == copyAction && editor_) {
    QApplication::clipboard()->setText(editor_->value().toString());
  } else if (chosen == pasteAction && editor_) {
    const QString clipboardText = QApplication::clipboard()->text().trimmed();
    if (!clipboardText.isEmpty()) {
      editor_->setValueFromVariant(clipboardText);
      editor_->commitCurrentValue();
    }
  } else if (chosen == resetAction && resetButton_ &&
             resetButton_->isVisible()) {
    resetButton_->click();
  } else if (chosen == copyNameAction) {
    QApplication::clipboard()->setText(propertyName_);
  } else if (anchorMenu &&
             (chosen == anchorAbsoluteAction || chosen == anchorLockToInAction ||
              chosen == anchorLockToOutAction || chosen == anchorStretchAction)) {
    if (keyframeAnchorHandler_) {
      const auto anchor =
          (chosen == anchorAbsoluteAction)   ? ArtifactCore::KeyFrame::Anchor::Absolute
          : (chosen == anchorLockToInAction) ? ArtifactCore::KeyFrame::Anchor::LockToIn
          : (chosen == anchorLockToOutAction) ? ArtifactCore::KeyFrame::Anchor::LockToOut
                                             : ArtifactCore::KeyFrame::Anchor::StretchWithLayer;
      keyframeAnchorHandler_(anchor);
    }
  } else if (colorMenu &&
             (chosen == colorNoneAction || chosen == colorRedAction ||
              chosen == colorBlueAction || chosen == colorYellowAction ||
              chosen == colorGreenAction || chosen == colorPurpleAction ||
              chosen == colorGrayAction)) {
    if (keyframeColorLabelHandler_) {
      const auto label =
          (chosen == colorNoneAction)    ? ArtifactCore::KeyFrame::ColorLabel::None
          : (chosen == colorRedAction)   ? ArtifactCore::KeyFrame::ColorLabel::Red
          : (chosen == colorBlueAction)  ? ArtifactCore::KeyFrame::ColorLabel::Blue
          : (chosen == colorYellowAction) ? ArtifactCore::KeyFrame::ColorLabel::Yellow
          : (chosen == colorGreenAction)  ? ArtifactCore::KeyFrame::ColorLabel::Green
          : (chosen == colorPurpleAction) ? ArtifactCore::KeyFrame::ColorLabel::Purple
                                         : ArtifactCore::KeyFrame::ColorLabel::Gray;
      keyframeColorLabelHandler_(label);
    }
  }
  event->accept();
}

void ArtifactPropertyEditorRowWidget::paintEvent(QPaintEvent *event) {
  QWidget::paintEvent(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor accent =
      themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
  const QColor selection =
      themeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
  const QColor border =
      themeColor(theme.borderColor, QColor(QStringLiteral("#404754")));

  const bool hovered = underMouse();
  const bool focused = editor_ && editor_->hasFocus();
  const bool keyframed = currentFrameKeyframed_;

  const QRectF frame = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  const qreal radius = 7.0;
  QPainterPath path;
  path.addRoundedRect(frame, radius, radius);

  if (hovered || focused) {
    QColor fill = propertySurfaceColor(false);
    fill = focused ? blendColor(fill, selection, 0.18)
                   : blendColor(fill, accent, 0.025);
    painter.fillPath(path, fill);

    const QColor line =
        focused ? blendColor(border, selection, 0.40)
                : blendColor(border, accent, 0.10);
    painter.setPen(QPen(line, 1.0));
    painter.drawPath(path);
  }

  if (keyframed) {
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawRoundedRect(QRectF(1.0, 3.0, 4.0, height() - 6.0), 2.0, 2.0);
    painter.restore();
  }
}

bool ArtifactPropertyEditorRowWidget::eventFilter(QObject *watched,
                                                  QEvent *event) {
  if (!editor_ || !editor_->supportsScrub()) {
    return QWidget::eventFilter(watched, event);
  }

  switch (event->type()) {
  case QEvent::MouseButtonPress: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() != Qt::LeftButton) {
      break;
    }
    if (watched == scrubTarget_) {
      scrubCandidate_ = true;
      scrubbing_ = false;
      scrubStartX_ = mouseEvent->globalPosition().toPoint().x();
      scrubStartValue_ = editor_->value();
      setFocus(Qt::MouseFocusReason);
      return false;
    }
    break;
  }
  case QEvent::MouseMove: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (watched == scrubTarget_) {
      if (!scrubCandidate_ && !scrubbing_) {
        break;
      }
      if (!mouseEvent->buttons().testFlag(Qt::LeftButton)) {
        scrubCandidate_ = false;
        break;
      }
      const int currentX = mouseEvent->globalPosition().toPoint().x();
      const int deltaPixels = currentX - scrubStartX_;
      if (!scrubbing_) {
        if (std::abs(deltaPixels) < scrubThreshold_) {
          break;
        }
        scrubbing_ = true;
        scrubTarget_->grabMouse();
        grabKeyboard();
        scrubTarget_->setCursor(Qt::SizeHorCursor);
      }
      editor_->setValueFromVariant(scrubStartValue_);
      editor_->scrubByPixels(deltaPixels, mouseEvent->modifiers());
      editor_->previewCurrentValue();
      return true;
    }
    break;
  }
  case QEvent::MouseButtonRelease: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (watched == scrubTarget_ && mouseEvent->button() == Qt::LeftButton) {
      if (scrubbing_) {
        finishScrub(true);
        return true;
      }
      scrubCandidate_ = false;
    }
    break;
  }
  default:
    break;
  }

  return QWidget::eventFilter(watched, event);
}

void ArtifactPropertyEditorRowWidget::keyPressEvent(QKeyEvent *event) {
  if ((scrubbing_ || scrubCandidate_) && event->key() == Qt::Key_Escape) {
    finishScrub(false);
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

void ArtifactPropertyEditorRowWidget::finishScrub(const bool commitChanges) {
  if (!scrubbing_ && !scrubCandidate_) {
    return;
  }
  if (scrubbing_ && scrubTarget_) {
    scrubTarget_->releaseMouse();
    releaseKeyboard();
  }
  if (scrubTarget_) {
    scrubTarget_->setCursor(editor_ && editor_->supportsScrub()
                                ? Qt::SizeHorCursor
                                : Qt::ArrowCursor);
  }

  if (!editor_) {
    scrubCandidate_ = false;
    scrubbing_ = false;
    return;
  }

  if (commitChanges && scrubbing_) {
    editor_->commitCurrentValue();
  } else {
    editor_->setValueFromVariant(scrubStartValue_);
    editor_->previewValueFromVariant(scrubStartValue_);
  }
  scrubCandidate_ = false;
  scrubbing_ = false;
}

// ---------------------------------------------------------------------------
// ArtifactTextAnimatorColorEditor
// ---------------------------------------------------------------------------
ArtifactTextAnimatorColorEditor::ArtifactTextAnimatorColorEditor(
} // namespace Artifact

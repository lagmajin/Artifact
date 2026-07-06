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
#include <QSizePolicy>
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

namespace {
constexpr int kPropertyRowMinHeight = 34;
constexpr int kPropertyRowLabelMinHeight = 24;
constexpr int kPropertyRowLabelWidth = 132;
constexpr int kPropertyRowMarginH = 10;
constexpr int kPropertyRowMarginV = 4;
constexpr int kPropertyRowSpacing = 8;
constexpr int kPropertyActionSpacing = 4;
constexpr int kPropertyNavButtonWidth = 14;
constexpr int kPropertyNavButtonHeight = 22;
constexpr int kPropertyKeyButtonSize = 22;
constexpr int kPropertyResetButtonSize = 24;
constexpr int kPropertyExprButtonWidth = 26;
constexpr int kPropertyExprButtonHeight = 24;
constexpr int kAuxButtonAreaWidth =
    kPropertyNavButtonWidth + kPropertyActionSpacing +
    kPropertyKeyButtonSize + kPropertyActionSpacing +
    kPropertyNavButtonWidth + kPropertyActionSpacing +
    kPropertyResetButtonSize + kPropertyActionSpacing +
    kPropertyExprButtonWidth + kPropertyActionSpacing +
    kPropertyKeyButtonSize;
}

namespace Artifact {

W_OBJECT_IMPL(ArtifactTextAnimatorColorEditor)

using namespace detail;

namespace detail {
extern ArtifactPropertyRowLayoutMode g_propertyRowLayoutMode;
extern ArtifactNumericEditorLayoutMode g_numericEditorLayoutMode;
bool artifactShouldShowPropertyResetButtonsImpl();
void artifactSetShowPropertyResetButtonsImpl(bool show);
int intToSliderPosition(int value, int min, int max);
int sliderPositionToInt(int pos, int min, int max);
QIcon loadPropertyIcon(const QString &resourceRelativePath,
                       const QString &fallbackFileName);
}

bool artifactShouldShowPropertyResetButtons() {
  return artifactShouldShowPropertyResetButtonsImpl();
}

void artifactSetShowPropertyResetButtons(bool show) {
  artifactSetShowPropertyResetButtonsImpl(show);
}

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
  setAttribute(Qt::WA_Hover, true);
  applyPropertyFieldPalette(this, false);
  setAutoFillBackground(false);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(kPropertyRowMarginH, kPropertyRowMarginV,
                             kPropertyRowMarginH, kPropertyRowMarginV);
  layout->setSpacing(kPropertyRowSpacing);

  label_->setObjectName(QStringLiteral("propertyRowLabel"));
  label_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  label_->setMinimumHeight(kPropertyRowLabelMinHeight);
  label_->setFixedWidth(kPropertyRowLabelWidth);
  label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  label_->setAutoFillBackground(false);
  applyPropertyLabelPalette(label_);

  supplementaryLabel_ = new QLabel(this);
  supplementaryLabel_->setObjectName(
      QStringLiteral("propertySupplementaryLabel"));
  supplementaryLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  supplementaryLabel_->setAutoFillBackground(false);
  supplementaryLabel_->setVisible(false);
  supplementaryLabel_->setSizePolicy(QSizePolicy::Minimum,
                                     QSizePolicy::Fixed);
  applyPropertyLabelPalette(supplementaryLabel_, true);

  editor_->setParent(this);
  editor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  applyPropertyFieldPalette(editor_);

  const QIcon prevIcon = loadPropertyIcon(
      QStringLiteral("Studio/property_key_previous.svg"),
      QStringLiteral("Studio/property_key_previous.svg"));
  const QIcon nextIcon = loadPropertyIcon(
      QStringLiteral("Studio/property_key_next.svg"),
      QStringLiteral("Studio/property_key_next.svg"));
  const QIcon resetIcon =
      loadPropertyIcon(QStringLiteral("MaterialVS/neutral/undo.svg"),
                       QString());
  const QIcon expressionIcon =
      loadPropertyIcon(QStringLiteral("MaterialVS/blue/code.svg"),
                       QString());

  prevKeyBtn_->setFixedSize(kPropertyNavButtonWidth, kPropertyNavButtonHeight);
  nextKeyBtn_->setFixedSize(kPropertyNavButtonWidth, kPropertyNavButtonHeight);
  prevKeyBtn_->setIcon(prevIcon);
  nextKeyBtn_->setIcon(nextIcon);
  prevKeyBtn_->setIconSize(QSize(10, 10));
  nextKeyBtn_->setIconSize(QSize(10, 10));
  prevKeyBtn_->setObjectName(QStringLiteral("propertyPrevKeyButton"));
  nextKeyBtn_->setObjectName(QStringLiteral("propertyNextKeyButton"));
  prevKeyBtn_->setToolTip(
      QStringLiteral("Previous keyframe: %1").arg(propertyName));
  nextKeyBtn_->setToolTip(
      QStringLiteral("Next keyframe: %1").arg(propertyName));
  prevKeyBtn_->setFlat(true);
  nextKeyBtn_->setFlat(true);
  prevKeyBtn_->setVisible(false);
  nextKeyBtn_->setVisible(false);
  prevKeyBtn_->setFocusPolicy(Qt::NoFocus);
  nextKeyBtn_->setFocusPolicy(Qt::NoFocus);
  applyPropertyButtonPalette(prevKeyBtn_);
  applyPropertyButtonPalette(nextKeyBtn_);

  keyframeButton_->setObjectName(QStringLiteral("propertyKeyButton"));
  keyframeButton_->setToolTip(
      QStringLiteral("Toggle Keyframe: %1").arg(propertyName));
  keyframeButton_->setFixedSize(kPropertyKeyButtonSize,
                                kPropertyKeyButtonSize);
  keyframeButton_->setCheckable(true);
  keyframeButton_->setIconSize(QSize(14, 14));
  keyframeButton_->setFlat(true);
  keyframeButton_->setFocusPolicy(Qt::NoFocus);
  applyPropertyButtonPalette(keyframeButton_);
  updateKeyframeButtonIcon();

  resetButton_->setObjectName(QStringLiteral("propertyResetButton"));
  resetButton_->setToolTip(QStringLiteral("Reset: %1").arg(propertyName));
  resetButton_->setFixedSize(kPropertyResetButtonSize,
                             kPropertyResetButtonSize);
  resetButton_->setIcon(resetIcon);
  resetButton_->setIconSize(QSize(14, 14));
  resetButton_->setFlat(true);
  resetButton_->setVisible(false);
  resetButton_->setFocusPolicy(Qt::NoFocus);
  applyPropertyButtonPalette(resetButton_);

  expressionButton_->setObjectName(QStringLiteral("propertyExprButton"));
  expressionButton_->setToolTip(
      QStringLiteral("Expression: %1").arg(propertyName));
  expressionButton_->setFixedSize(kPropertyExprButtonWidth,
                                  kPropertyExprButtonHeight);
  expressionButton_->setIcon(expressionIcon);
  expressionButton_->setIconSize(QSize(14, 14));
  expressionButton_->setFlat(true);
  expressionButton_->setVisible(false);
  expressionButton_->setFocusPolicy(Qt::NoFocus);
  applyPropertyButtonPalette(expressionButton_);

  favoriteButton_ = new QPushButton(this);
  favoriteButton_->setObjectName(QStringLiteral("propertyFavButton"));
  favoriteButton_->setToolTip(
      QStringLiteral("Favorite: %1").arg(propertyName));
  favoriteButton_->setFixedSize(kPropertyKeyButtonSize,
                                kPropertyKeyButtonSize);
  favoriteButton_->setCheckable(true);
  favoriteButton_->setText(QStringLiteral("\u2606"));
  QFont starFont = favoriteButton_->font();
  starFont.setPointSize(11);
  favoriteButton_->setFont(starFont);
  favoriteButton_->setFlat(true);
  favoriteButton_->setVisible(false);
  favoriteButton_->setFocusPolicy(Qt::NoFocus);
  applyPropertyButtonPalette(favoriteButton_);

  auto *auxContainer = new QWidget(this);
  auxContainer->setMaximumWidth(kAuxButtonAreaWidth);
  auxContainer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
  auxContainer->setAutoFillBackground(false);
  auto *auxLayout = new QHBoxLayout(auxContainer);
  auxLayout->setContentsMargins(0, 0, 0, 0);
  auxLayout->setSpacing(kPropertyActionSpacing);
  auxLayout->addWidget(prevKeyBtn_);
  auxLayout->addWidget(keyframeButton_);
  auxLayout->addWidget(nextKeyBtn_);
  auxLayout->addWidget(resetButton_);
  auxLayout->addWidget(expressionButton_);
  auxLayout->addWidget(favoriteButton_);

  scrubTarget_ = editor_->scrubTargetWidget();
  if (!scrubTarget_) {
    scrubTarget_ = editor_;
  }
  scrubTarget_->installEventFilter(this);
  label_->setCursor(Qt::ArrowCursor);
  editor_->setCursor(Qt::ArrowCursor);
  scrubTarget_->setCursor(editor_->supportsScrub() ? Qt::SizeHorCursor
                                                   : Qt::ArrowCursor);

  if (g_propertyRowLayoutMode ==
      ArtifactPropertyRowLayoutMode::EditorThenLabel) {
    layout->addWidget(editor_, 1);
    layout->addWidget(label_, 0);
  } else {
    layout->addWidget(label_, 0);
    layout->addWidget(editor_, 1);
  }
  layout->addWidget(supplementaryLabel_, 0);
  layout->addWidget(auxContainer, 0);

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
  QObject::connect(favoriteButton_, &QPushButton::toggled, this,
                   [this](const bool checked) {
                     if (favoriteButton_) {
                       favoriteButton_->setText(
                           checked ? QStringLiteral("\u2605")
                                   : QStringLiteral("\u2606"));
                     }
                     if (favoriteHandler_) {
                       favoriteHandler_(checked);
                     }
                   });
  QObject::connect(keyframeButton_, &QPushButton::toggled, this,
                   [this](const bool checked) {
                     if (keyframeHandler_) {
                       keyframeHandler_(checked);
                     }
                     updateKeyframeButtonIcon();
                   });
  QObject::connect(prevKeyBtn_, &QPushButton::clicked, this, [this]() {
    if (navigationHandler_) {
      navigationHandler_(-1);
    }
  });
  QObject::connect(nextKeyBtn_, &QPushButton::clicked, this, [this]() {
    if (navigationHandler_) {
      navigationHandler_(1);
    }
  });
}

ArtifactPropertyEditorRowWidget::~ArtifactPropertyEditorRowWidget() = default;

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
  return {hardMin, hardMax};
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

void ArtifactPropertyEditorRowWidget::setAuxAction(
    std::function<void()> handler, const QString &label) {
  auxActionHandler_ = std::move(handler);
  auxActionLabel_ = label;
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
  if (!favoriteButton_) {
    return;
  }
  favoriteButton_->setProperty("baseVisible", visible);
  updateAuxControlVisibility();
  update();
}

void ArtifactPropertyEditorRowWidget::setFavoriteChecked(const bool checked) {
  if (!favoriteButton_) {
    return;
  }
  favoriteButton_->setChecked(checked);
  favoriteButton_->setText(checked
      ? QStringLiteral("\u2605")  // ★
      : QStringLiteral("\u2606")); // ☆
}

bool ArtifactPropertyEditorRowWidget::isFavoriteChecked() const {
  return favoriteButton_ && favoriteButton_->isChecked();
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
  if (!keyframeButton_ || !resetButton_ || !expressionButton_ ||
      !prevKeyBtn_ || !nextKeyBtn_) {
    return;
  }
  const bool hover = underMouse();
  const bool keyVisible = keyframeButton_->isVisible();
  const bool resetVisible = resetButton_->property("baseVisible").toBool();
  const bool exprVisible = expressionButton_->property("baseVisible").toBool();
  const bool navVisible = prevKeyBtn_->property("baseVisible").toBool() &&
                          nextKeyBtn_->property("baseVisible").toBool();
  const bool favVisible =
      favoriteButton_ && favoriteButton_->property("baseVisible").toBool();

  resetButton_->setVisible(resetVisible && hover);
  expressionButton_->setVisible(exprVisible && hover);
  if (favoriteButton_) {
    favoriteButton_->setVisible(
        favVisible && (hover || favoriteButton_->isChecked()));
  }
  prevKeyBtn_->setVisible(keyVisible && navVisible);
  nextKeyBtn_->setVisible(keyVisible && navVisible);

  resetButton_->setEnabled(resetVisible && hover);
  expressionButton_->setEnabled(exprVisible && hover);
  if (favoriteButton_) {
    favoriteButton_->setEnabled(favVisible && hover);
  }
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
  QAction *auxAction = nullptr;
  if (auxActionHandler_ && !auxActionLabel_.isEmpty()) {
    auxAction = menu.addAction(auxActionLabel_);
  }
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
  } else if (chosen == auxAction && auxActionHandler_) {
    auxActionHandler_();
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

} // namespace Artifact

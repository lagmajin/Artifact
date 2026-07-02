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

namespace {
constexpr int kPropertyRowMinHeight = 32;
constexpr int kPropertyRowLabelMinHeight = 24;
constexpr int kPropertyRowLabelWidth = 132;
constexpr int kPropertyRowMarginH = 10;
constexpr int kPropertyRowMarginV = 5;
constexpr int kPropertyRowSpacing = 8;
constexpr int kPropertyActionSpacing = 4;
constexpr int kPropertyNavButtonWidth = 14;
constexpr int kPropertyNavButtonHeight = 22;
constexpr int kPropertyKeyButtonSize = 22;
constexpr int kPropertyResetButtonSize = 24;
constexpr int kPropertyExprButtonWidth = 26;
constexpr int kPropertyExprButtonHeight = 24;
constexpr int kNumericEditorValueWidth = 110;
// Fixed width for the aux-button container so the row doesn't resize on hover.
constexpr int kAuxButtonAreaWidth =
    kPropertyNavButtonWidth + kPropertyActionSpacing +
    kPropertyKeyButtonSize  + kPropertyActionSpacing +
    kPropertyNavButtonWidth + kPropertyActionSpacing +
    kPropertyResetButtonSize + kPropertyActionSpacing +
    kPropertyExprButtonWidth; // 14+4+22+4+14+4+24+4+26 = 116

QColor themeColor(const QString &value, const QColor &fallback) {
  const QColor color(value);
  return color.isValid() ? color : fallback;
}

class PropertySliderWidget final : public QSlider {
public:
  explicit PropertySliderWidget(QWidget *parent = nullptr)
      : QSlider(Qt::Horizontal, parent) {}

  void setDisplayText(QString text) {
    if (displayText_ == text) {
      return;
    }
    displayText_ = std::move(text);
    update();
  }

protected:
  void wheelEvent(QWheelEvent *event) override {
    event->ignore();
  }

  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor surface = palette().color(QPalette::Window);
    const QColor trackBase = palette().color(QPalette::Base);
    const QColor trackBorder = palette().color(QPalette::Dark);
    const QColor trackFill = palette().color(QPalette::Highlight);
    const QColor handleFill = palette().color(QPalette::Button);
    const QColor handleBorder = palette().color(QPalette::Mid);
    const QColor textColor = palette().color(QPalette::Text);

    painter.fillRect(rect(), surface);

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    const QRect grooveRect =
        style()->subControlRect(QStyle::CC_Slider, &opt,
                                QStyle::SC_SliderGroove, this);
    const QRect handleRect =
        style()->subControlRect(QStyle::CC_Slider, &opt,
                                QStyle::SC_SliderHandle, this);

    QRect trackRect = rect().adjusted(1, 1, -2, -2);
    trackRect.setHeight(std::max(18, rect().height() - 4));
    trackRect.moveCenter(QPoint(trackRect.center().x(), rect().center().y()));

    painter.setPen(Qt::NoPen);
    painter.setBrush(trackBase);
    painter.drawRoundedRect(trackRect, 5.0, 5.0);

    QRect fillRect = trackRect;
    const bool reversed =
        (opt.upsideDown && opt.orientation == Qt::Horizontal) ||
        (!opt.upsideDown && opt.orientation == Qt::Vertical);
    if (opt.orientation == Qt::Horizontal) {
      const int handleCenter = handleRect.center().x();
      if (reversed) {
        fillRect.setLeft(handleCenter);
      } else {
        fillRect.setRight(handleCenter);
      }
    } else {
      const int handleCenter = handleRect.center().y();
      if (reversed) {
        fillRect.setTop(handleCenter);
      } else {
        fillRect.setBottom(handleCenter);
      }
    }

    painter.setBrush(trackFill);
    painter.drawRoundedRect(fillRect.intersected(trackRect), 5.0, 5.0);
    painter.setPen(QPen(trackBorder, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(trackRect, 5.0, 5.0);

    QRect ghostHandleRect = handleRect.adjusted(2, 3, -2, -3);
    if (ghostHandleRect.width() > 2 && ghostHandleRect.height() > 2) {
      QColor ghostFill = handleFill;
      ghostFill.setAlpha(70);
      QColor ghostBorder = handleBorder;
      ghostBorder.setAlpha(110);
      painter.setPen(QPen(ghostBorder, 1.0));
      painter.setBrush(ghostFill);
      painter.drawRoundedRect(ghostHandleRect, 3.0, 3.0);
    }

    if (!displayText_.isEmpty()) {
      QFont textFont = font();
      textFont.setPointSize(std::max(10, textFont.pointSize()));
      textFont.setWeight(QFont::DemiBold);
      painter.setFont(textFont);
      painter.setPen(textColor);
      painter.drawText(trackRect.adjusted(10, 0, -10, 0),
                       Qt::AlignCenter, displayText_);
    }
  }

private:
  QString displayText_;
};

class PropertyComboBox final : public QComboBox {
public:
  explicit PropertyComboBox(QWidget *parent = nullptr) : QComboBox(parent) {}

protected:
  void wheelEvent(QWheelEvent *event) override {
    event->ignore();
  }
};

class PropertyCallbackButton final : public QPushButton {
public:
  using Callback = std::function<void()>;

  explicit PropertyCallbackButton(const QString &text, QWidget *parent = nullptr)
      : QPushButton(text, parent) {}

  void setCallback(Callback callback) { callback_ = std::move(callback); }

protected:
  void mouseReleaseEvent(QMouseEvent *event) override {
    QPushButton::mouseReleaseEvent(event);
    if (!isEnabled() || !callback_ || !event ||
        event->button() != Qt::LeftButton || !rect().contains(event->pos())) {
      return;
    }
    callback_();
  }

private:
  Callback callback_;
};

QColor blendColor(const QColor &a, const QColor &b, const qreal t) {
  const qreal clamped = std::clamp(t, 0.0, 1.0);
  return QColor::fromRgbF(a.redF() * (1.0 - clamped) + b.redF() * clamped,
                          a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
                          a.blueF() * (1.0 - clamped) + b.blueF() * clamped,
                          a.alphaF() * (1.0 - clamped) + b.alphaF() * clamped);
}

QColor propertySurfaceColor(const bool elevated = false) {
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor background =
      themeColor(theme.backgroundColor, QColor(QStringLiteral("#20242A")));
  const QColor surface = themeColor(theme.secondaryBackgroundColor,
                                    QColor(QStringLiteral("#2B3038")));
  return blendColor(background, surface, elevated ? 0.64 : 0.54);
}

QString formatNumericSliderText(const double value, const QString &unit,
                                const int decimals = 3) {
  QString text = QLocale::system().toString(value, 'f', decimals);
  while (text.contains(QLatin1Char('.')) &&
         (text.endsWith(QLatin1Char('0')) || text.endsWith(QLatin1Char('.')))) {
    text.chop(1);
    if (text.endsWith(QLatin1Char('.'))) {
      text.chop(1);
      break;
    }
  }
  if (!unit.isEmpty()) {
    text += QStringLiteral(" ") + unit;
  }
  return text;
}

void applyThemeTextPalette(QWidget *widget, int shade = 100) {
  if (!widget) {
    return;
  }
  const QColor textColor = themeColor(ArtifactCore::currentDCCTheme().textColor,
                                      QColor(QStringLiteral("#E3E7EC")));
  QPalette pal = widget->palette();
  pal.setColor(QPalette::WindowText, textColor.darker(shade));
  pal.setColor(QPalette::Text, textColor.darker(shade));
  widget->setPalette(pal);
}

void applyPropertyFieldPalette(QWidget *widget, const bool elevated = false) {
  if (!widget) {
    return;
  }
  // Ensure the Qt style polish runs first so our palette takes precedence.
  // CommonStyle::polish() sets raw Window/Base from the app theme for QSpinBox/QSlider;
  // calling ensurePolished() here means our blended surface color is set AFTER polish,
  // not before — preventing polish from silently overwriting property-row colors.
  widget->ensurePolished();
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor background =
      themeColor(theme.backgroundColor, QColor(QStringLiteral("#20242A")));
  const QColor text =
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor selection =
      themeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
  const QColor border =
      themeColor(theme.borderColor, QColor(QStringLiteral("#404754")));
  const QColor accent =
      themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));

  widget->setAttribute(Qt::WA_StyledBackground, true);
  widget->setAutoFillBackground(true);
  QPalette pal = widget->palette();
  const QColor window = propertySurfaceColor(elevated);
  const QColor mid = blendColor(window, border, 0.58);
  const QColor dark = blendColor(window, border, 0.42);
  const QColor shadow = blendColor(window, background, 0.28);
  const QColor midlight = blendColor(window, text, 0.06);
  pal.setColor(QPalette::Window, window);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Base, window);
  pal.setColor(QPalette::AlternateBase, blendColor(window, text, 0.05));
  pal.setColor(QPalette::Text, text);
  pal.setColor(QPalette::Button, window);
  pal.setColor(QPalette::ButtonText, text);
  pal.setColor(QPalette::Highlight, selection);
  pal.setColor(QPalette::HighlightedText, background);
  pal.setColor(QPalette::Midlight, midlight);
  pal.setColor(QPalette::Mid, border);
  pal.setColor(QPalette::Dark, mid);
  pal.setColor(QPalette::Shadow, shadow);
  pal.setColor(QPalette::Light, accent.lighter(120));
  widget->setPalette(pal);
}

void applyPropertyButtonPalette(QAbstractButton *button,
                                const bool accent = false) {
  if (!button) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor background =
      themeColor(theme.backgroundColor, QColor(QStringLiteral("#20242A")));
  const QColor text =
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor selection =
      themeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
  const QColor border =
      themeColor(theme.borderColor, QColor(QStringLiteral("#404754")));
  const QColor fill =
      accent ? themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")))
             : propertySurfaceColor(false);
  const QColor contrast = accent ? background : text;

  button->setAttribute(Qt::WA_StyledBackground, true);
  button->setAutoFillBackground(true);
  QPalette pal = button->palette();
  pal.setColor(QPalette::Button, fill);
  pal.setColor(QPalette::ButtonText, contrast);
  pal.setColor(QPalette::Window, fill);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Highlight, selection);
  pal.setColor(QPalette::HighlightedText, background);
  pal.setColor(QPalette::Mid, border);
  button->setPalette(pal);
}

void applyPropertyLabelPalette(QLabel *label, const bool prominent = false) {
  if (!label) {
    return;
  }
  const auto &theme = ArtifactCore::currentDCCTheme();
  const QColor text =
      themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
  const QColor accent =
      themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));

  QPalette pal = label->palette();
  pal.setColor(QPalette::WindowText, prominent ? accent : text);
  pal.setColor(QPalette::Window, propertySurfaceColor(false));
  // Ensure the label actually paints its background from the palette.
  label->setAutoFillBackground(false);
  label->setPalette(pal);
}
} // namespace

ArtifactAbstractPropertyEditor::ArtifactAbstractPropertyEditor(QWidget *parent)
    : QWidget(parent) {}

ArtifactAbstractPropertyEditor::~ArtifactAbstractPropertyEditor() = default;

void ArtifactAbstractPropertyEditor::setCommitHandler(CommitHandler handler) {
  commitHandler_ = std::move(handler);
}

void ArtifactAbstractPropertyEditor::setPreviewHandler(PreviewHandler handler) {
  previewHandler_ = std::move(handler);
}

void ArtifactAbstractPropertyEditor::previewCurrentValue() const {
  previewValue(value());
}

void ArtifactAbstractPropertyEditor::previewValueFromVariant(
    const QVariant &value) const {
  previewValue(value);
}

void ArtifactAbstractPropertyEditor::commitCurrentValue() const {
  commitValue(value());
}

void ArtifactAbstractPropertyEditor::commitValue(const QVariant &value) const {
  if (commitHandler_) {
    commitHandler_(value);
  }
}

void ArtifactAbstractPropertyEditor::previewValue(const QVariant &value) const {
  if (previewHandler_) {
    previewHandler_(value);
  }
}

bool ArtifactAbstractPropertyEditor::supportsScrub() const { return false; }

void ArtifactAbstractPropertyEditor::scrubByPixels(
    int deltaPixels, Qt::KeyboardModifiers modifiers) {
  Q_UNUSED(deltaPixels);
  Q_UNUSED(modifiers);
}

QWidget *ArtifactAbstractPropertyEditor::scrubTargetWidget() const {
  return const_cast<ArtifactAbstractPropertyEditor *>(this);
}

namespace {

QColor propertyColor(const ArtifactCore::AbstractProperty &property) {
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

int floatToSliderPosition(const double value, const double minValue,
                          const double maxValue) {
  if (maxValue <= minValue) {
    return 0;
  }
  const double normalized =
      std::clamp((value - minValue) / (maxValue - minValue), 0.0, 1.0);
  return static_cast<int>(std::lround(normalized * 10000.0));
}

double sliderPositionToFloat(const int sliderValue, const double minValue,
                             const double maxValue) {
  if (maxValue <= minValue) {
    return minValue;
  }
  const double normalized = static_cast<double>(sliderValue) / 10000.0;
  return minValue + (maxValue - minValue) * normalized;
}

int intToSliderPosition(const int value, const int minValue,
                        const int maxValue) {
  if (maxValue <= minValue) {
    return 0;
  }
  const double normalized =
      std::clamp(static_cast<double>(value - minValue) /
                     static_cast<double>(maxValue - minValue),
                 0.0, 1.0);
  return static_cast<int>(std::lround(normalized * 10000.0));
}

int sliderPositionToInt(const int sliderValue, const int minValue,
                        const int maxValue) {
  if (maxValue <= minValue) {
    return minValue;
  }
  const double normalized = static_cast<double>(sliderValue) / 10000.0;
  return static_cast<int>(
      std::lround(static_cast<double>(minValue) +
                  static_cast<double>(maxValue - minValue) * normalized));
}

QString fileDialogFilterForProperty(const QString &propertyName) {
  if (propertyName.contains(QStringLiteral("model"), Qt::CaseInsensitive) ||
      propertyName.contains(QStringLiteral("3d"), Qt::CaseInsensitive)) {
    return QStringLiteral(
        "3D Models (*.obj *.fbx *.gltf *.glb *.stl *.dae *.abc *.usd *.usda *.usdc *.usdz *.pmd *.pmx);;All Files (*.*)");
  }
  if (propertyName.contains(QStringLiteral("video"), Qt::CaseInsensitive) ||
      propertyName.contains(QStringLiteral("media"), Qt::CaseInsensitive)) {
    return QStringLiteral("Media Files (*.mp4 *.mov *.avi *.mkv *.webm *.mp3 "
                          "*.wav *.flac);;All Files (*.*)");
  }
  if (propertyName.contains(QStringLiteral("audio"), Qt::CaseInsensitive)) {
    return QStringLiteral(
        "Audio Files (*.wav *.mp3 *.flac *.ogg *.m4a);;All Files (*.*)");
  }
  if (propertyName.contains(QStringLiteral("image"), Qt::CaseInsensitive) ||
      propertyName.endsWith(QStringLiteral("sourcePath"),
                            Qt::CaseInsensitive)) {
    return QStringLiteral("Image Files (*.png *.jpg *.jpeg *.bmp *.tif *.tiff "
                          "*.webp *.exr);;All Files (*.*)");
  }
  return QStringLiteral("All Files (*.*)");
}

bool isPathProperty(const ArtifactCore::AbstractProperty &property) {
  if (property.getType() != ArtifactCore::PropertyType::String) {
    return false;
  }
  const QString name = property.getName();
  return name.compare(QStringLiteral("video.sourcePath"),
                      Qt::CaseInsensitive) == 0 ||
         name.compare(QStringLiteral("model.sourcePath"),
                      Qt::CaseInsensitive) == 0 ||
         name.endsWith(QStringLiteral(".sourcePath"), Qt::CaseInsensitive) ||
         name.compare(QStringLiteral("sourcePath"), Qt::CaseInsensitive) == 0;
}

bool isFontFamilyProperty(const ArtifactCore::AbstractProperty &property) {
  if (property.getType() != ArtifactCore::PropertyType::String) {
    return false;
  }
  const QString name = property.getName();
  return name.compare(QStringLiteral("text.fontFamily"), Qt::CaseInsensitive) ==
             0 ||
         name.endsWith(QStringLiteral(".fontFamily"), Qt::CaseInsensitive) ||
         name.compare(QStringLiteral("fontFamily"), Qt::CaseInsensitive) == 0;
}

bool isMultilineTextProperty(const ArtifactCore::AbstractProperty &property) {
  if (property.getType() != ArtifactCore::PropertyType::String) {
    return false;
  }
  const QString name = property.getName();
  return name.compare(QStringLiteral("text.value"), Qt::CaseInsensitive) == 0;
}

bool shouldShowNumericSlider(const ArtifactCore::AbstractProperty &property) {
  const QString name = property.getName();
  if (name.isEmpty()) {
    return true;
  }
  // Size/position style fields are easier to edit precisely without the
  // extra slider strip.
  if (name == QStringLiteral("size") ||
      name.endsWith(QStringLiteral(".size"), Qt::CaseInsensitive) ||
      name == QStringLiteral("shape.width") ||
      name == QStringLiteral("solid.gradientAngleDegrees") ||
      name == QStringLiteral("solid.gradientCenterX") ||
      name == QStringLiteral("solid.gradientCenterY") ||
      name == QStringLiteral("solid.gradientScale") ||
      name == QStringLiteral("solid.gradientOffset") ||
      name == QStringLiteral("shape.height") ||
      name.startsWith(QStringLiteral("transform.position"),
                      Qt::CaseInsensitive) ||
      name.startsWith(QStringLiteral("transform.size"),
                      Qt::CaseInsensitive)) {
    return false;
  }
  return true;
}

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

  // Adaptive fallback around current value.
  const int currentValue = property.getValue().toInt();
  const int step =
      std::max(1, std::abs(meta.step.isValid() ? meta.step.toInt() : 1));
  const int halfSpan = std::max({std::abs(currentValue) / 2, step * 50, 10});
  int softMin = currentValue - halfSpan;
  int softMax = currentValue + halfSpan;
  if (hardMax > hardMin) {
    softMin = std::clamp(softMin, hardMin, hardMax);
    softMax = std::clamp(softMax, hardMin, hardMax);
  }
  if (softMax <= softMin) {
    return {hardMin, hardMax};
  }
  return {softMin, softMax};
}

ArtifactPropertyRowLayoutMode g_propertyRowLayoutMode =
    ArtifactPropertyRowLayoutMode::EditorThenLabel;

ArtifactNumericEditorLayoutMode g_numericEditorLayoutMode =
    ArtifactNumericEditorLayoutMode::ValueThenSlider;

// ✅ プロパティリセットボタンを表示するかどうかの設定
bool g_showPropertyResetButtons = false;

bool artifactShouldShowPropertyResetButtonsImpl() {
  return g_showPropertyResetButtons;
}

void artifactSetShowPropertyResetButtonsImpl(bool show) {
  g_showPropertyResetButtons = show;
}

// ✅ プロパティのデフォルト値を取得するユーティリティ関数
QVariant
getPropertyDefaultValue(const ArtifactCore::AbstractProperty &property) {
  // プロパティのメタデータからデフォルト値を取得
  const auto meta = property.metadata();
  if (property.getDefaultValue().isValid()) {
    return property.getDefaultValue();
  }

  // 型別のデフォルト値フォールバック
  switch (property.getType()) {
  case ArtifactCore::PropertyType::Float:
    return QVariant(0.0);
  case ArtifactCore::PropertyType::Integer:
    return QVariant(0);
  case ArtifactCore::PropertyType::Boolean:
    return QVariant(false);
  case ArtifactCore::PropertyType::String:
    return QVariant(QString());
  case ArtifactCore::PropertyType::Color:
    return QVariant(QColor(Qt::black));
  default:
    return QVariant();
  }
}

// --- Icon Loading Helpers ---

QIcon loadSvgAsIcon(const QString &path, int size = 16) {
  if (path.isEmpty())
    return QIcon();
  if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
    QSvgRenderer renderer(path);
    if (renderer.isValid()) {
      QPixmap pixmap(size, size);
      pixmap.fill(Qt::transparent);
      QPainter painter(&pixmap);
      renderer.render(&painter);
      painter.end();
      if (!pixmap.isNull())
        return QIcon(pixmap);
    }
    return QIcon();
  }
  return QIcon(path);
}

QIcon loadPropertyIcon(const QString &resourceRelativePath,
                       const QString &fallbackFileName = {}) {
  using namespace ArtifactCore;
  static QHash<QString, QIcon> iconCache;
  const QString cacheKey =
      resourceRelativePath + QStringLiteral("|") + fallbackFileName;
  auto it = iconCache.constFind(cacheKey);
  if (it != iconCache.constEnd())
    return it.value();
  QIcon icon = loadSvgAsIcon(resolveIconResourcePath(resourceRelativePath));
  if (!icon.isNull()) {
    iconCache.insert(cacheKey, icon);
    return icon;
  }
  if (!fallbackFileName.isEmpty()) {
    icon = loadSvgAsIcon(resolveIconPath(fallbackFileName));
  }
  iconCache.insert(cacheKey, icon);
  return icon;
}

// --- Relative Input Support ---
// ArtifactRelativeDoubleSpinBox / ArtifactRelativeSpinBox は
// Artifact.Widgets.RelativeSpinBox モジュールに切り出した（Dialog 系と共有するため）。

class ArtifactToggleSwitch final : public QAbstractButton {
public:
  explicit ArtifactToggleSwitch(QWidget *parent = nullptr)
      : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  }

  QSize sizeHint() const override { return {48, 26}; }
  QSize minimumSizeHint() const override { return {42, 24}; }

protected:
  bool hitButton(const QPoint &pos) const override {
    return rect().contains(pos);
  }

  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF trackRect = rect().adjusted(1, 1, -1, -1);
    const qreal radius = trackRect.height() * 0.5;

    const auto &theme = ArtifactCore::currentDCCTheme();
    const QColor accent =
        themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
    const QColor selection =
        themeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
    const QColor border =
        themeColor(theme.borderColor, QColor(QStringLiteral("#404754")));
    const QColor text =
        themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));

    QColor trackColor = isChecked() ? blendColor(propertySurfaceColor(true), accent, 0.24)
                                    : propertySurfaceColor(false);
    QColor borderColor =
        isChecked() ? blendColor(border, accent, 0.36) : border;
    QColor knobColor = isChecked() ? accent.lighter(175) : text;
    if (!isEnabled()) {
      trackColor = trackColor.darker(135);
      borderColor = borderColor.darker(135);
      knobColor = knobColor.darker(120);
    }

    painter.setPen(QPen(borderColor, 1.0));
    painter.setBrush(trackColor);
    painter.drawRoundedRect(trackRect, radius, radius);

    const qreal knobMargin = 2.0;
    const qreal knobDiameter = trackRect.height() - knobMargin * 2.0;
    const qreal knobX = isChecked()
                            ? trackRect.right() - knobMargin - knobDiameter
                            : trackRect.left() + knobMargin;
    const QRectF knobRect(knobX, trackRect.top() + knobMargin, knobDiameter,
                          knobDiameter);

    painter.setPen(Qt::NoPen);
    painter.setBrush(knobColor);
    painter.drawEllipse(knobRect);

    if (hasFocus()) {
      QPen focusPen(accent.lighter(125), 1.0);
      painter.setPen(focusPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRoundedRect(trackRect.adjusted(1, 1, -1, -1), radius, radius);
    }
  }
};

class PropertyNumericKnobWidget final : public QWidget {
public:
  using ValueHandler = std::function<void(double)>;

  explicit PropertyNumericKnobWidget(QWidget *parent = nullptr)
      : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::OpenHandCursor);
    setFixedSize(34, 34);
    setToolTip(QStringLiteral("Drag vertically to adjust. Shift = fine, Ctrl = coarse."));
  }

  QSize sizeHint() const override { return {34, 34}; }
  QSize minimumSizeHint() const override { return {30, 30}; }

  void setRange(const double minValue, const double maxValue) {
    min_ = minValue;
    max_ = maxValue > minValue ? maxValue : minValue + 1.0;
    setValue(value_);
  }

  void setValue(const double value) {
    const double nextValue = std::clamp(value, min_, max_);
    if (std::abs(value_ - nextValue) < 0.0001) {
      return;
    }
    value_ = nextValue;
    update();
  }

  void setPreviewHandler(ValueHandler handler) {
    previewHandler_ = std::move(handler);
  }

  void setCommitHandler(ValueHandler handler) {
    commitHandler_ = std::move(handler);
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(event);
      return;
    }
    dragging_ = true;
    dragStartY_ = event->position().y();
    dragStartValue_ = value_;
    grabMouse();
    setFocus(Qt::MouseFocusReason);
    setCursor(Qt::ClosedHandCursor);
    event->accept();
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (!dragging_) {
      QWidget::mouseMoveEvent(event);
      return;
    }
    const double range = std::max(1.0, max_ - min_);
    double sensitivity = range / 160.0;
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
      sensitivity *= 0.15;
    }
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
      sensitivity *= 4.0;
    }
    setValue(dragStartValue_ + (dragStartY_ - event->position().y()) * sensitivity);
    if (previewHandler_) {
      previewHandler_(value_);
    }
    event->accept();
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (!dragging_ || event->button() != Qt::LeftButton) {
      QWidget::mouseReleaseEvent(event);
      return;
    }
    dragging_ = false;
    releaseMouse();
    setCursor(Qt::OpenHandCursor);
    if (commitHandler_) {
      commitHandler_(value_);
    }
    event->accept();
  }

  void wheelEvent(QWheelEvent *event) override {
    const double range = std::max(1.0, max_ - min_);
    double step = range / 120.0;
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
      step *= 0.15;
    }
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
      step *= 4.0;
    }
    setValue(value_ + (event->angleDelta().y() >= 0 ? step : -step));
    if (previewHandler_) {
      previewHandler_(value_);
    }
    if (commitHandler_) {
      commitHandler_(value_);
    }
    event->accept();
  }

  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor window = palette().color(QPalette::Window);
    const QColor text = palette().color(QPalette::Text);
    const QColor border = palette().color(QPalette::Mid);
    const QColor shadow = palette().color(QPalette::Shadow);
    const QColor highlight = palette().color(QPalette::Highlight);
    const QColor light = palette().color(QPalette::Light);

    const QRectF knobRect = rect().adjusted(3, 3, -3, -3);
    const QPointF center = knobRect.center();
    const qreal radius = std::min(knobRect.width(), knobRect.height()) * 0.5;

    QColor outer = blendColor(window, text, 0.05);
    QColor inner = blendColor(window, shadow, 0.18);
    if (!isEnabled()) {
      outer = outer.darker(125);
      inner = inner.darker(125);
    }

    painter.setPen(QPen(blendColor(border, shadow, 0.25), 1.0));
    painter.setBrush(outer);
    painter.drawEllipse(knobRect);

    const QRectF innerRect = knobRect.adjusted(5, 5, -5, -5);
    painter.setPen(QPen(blendColor(border, text, 0.18), 1.0));
    painter.setBrush(inner);
    painter.drawEllipse(innerRect);

    const QRectF arcRect = knobRect.adjusted(2.5, 2.5, -2.5, -2.5);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(blendColor(border, window, 0.25), 2.2, Qt::SolidLine,
                        Qt::RoundCap));
    painter.drawArc(arcRect, 225 * 16, -270 * 16);

    const double normalized =
        max_ <= min_ ? 0.0 : std::clamp((value_ - min_) / (max_ - min_), 0.0, 1.0);
    QColor accent = blendColor(highlight, light, 0.22);
    if (!isEnabled()) {
      accent = accent.darker(140);
    }
    painter.setPen(QPen(accent, 2.4, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(arcRect, 225 * 16,
                    static_cast<int>(-270.0 * normalized * 16.0));

    const double indicatorDegrees = 225.0 - 270.0 * normalized;
    const double radians = indicatorDegrees * std::numbers::pi / 180.0;
    const QPointF indicator(center.x() + std::cos(radians) * radius * 0.58,
                            center.y() - std::sin(radians) * radius * 0.58);
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawEllipse(indicator, 2.2, 2.2);

    if (hasFocus()) {
      painter.setPen(QPen(accent.lighter(125), 1.0, Qt::DashLine));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(knobRect.adjusted(1, 1, -1, -1));
    }
  }

private:
  double min_ = 0.0;
  double max_ = 1.0;
  double value_ = 0.0;
  double dragStartY_ = 0.0;
  double dragStartValue_ = 0.0;
  bool dragging_ = false;
  ValueHandler previewHandler_;
  ValueHandler commitHandler_;
};

class PropertyRotationKnobWidget final : public QWidget {
public:
  using ValueHandler = std::function<void(double)>;

  explicit PropertyRotationKnobWidget(QWidget *parent = nullptr)
      : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::OpenHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  }

  QSize sizeHint() const override { return {36, 36}; }
  QSize minimumSizeHint() const override { return {30, 30}; }

  void setValue(const double value) {
    if (std::abs(value_ - value) < 0.0001) {
      return;
    }
    value_ = value;
    update();
  }

  void setPreviewHandler(ValueHandler handler) {
    previewHandler_ = std::move(handler);
  }

  void setCommitHandler(ValueHandler handler) {
    commitHandler_ = std::move(handler);
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() != Qt::LeftButton) {
      QWidget::mousePressEvent(event);
      return;
    }
    dragging_ = true;
    lastAngle_ = angleFromPosition(event->position());
    grabMouse();
    setFocus(Qt::MouseFocusReason);
    setCursor(Qt::ClosedHandCursor);
    event->accept();
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (!dragging_) {
      QWidget::mouseMoveEvent(event);
      return;
    }
    double delta = angleFromPosition(event->position()) - lastAngle_;
    if (delta > 180.0) {
      delta -= 360.0;
    } else if (delta < -180.0) {
      delta += 360.0;
    }

    double sensitivity = 1.0;
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
      sensitivity *= 0.2;
    }
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
      sensitivity *= 4.0;
    }

    value_ += delta * sensitivity;
    lastAngle_ = angleFromPosition(event->position());
    update();
    if (previewHandler_) {
      previewHandler_(value_);
    }
    event->accept();
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (!dragging_ || event->button() != Qt::LeftButton) {
      QWidget::mouseReleaseEvent(event);
      return;
    }
    dragging_ = false;
    releaseMouse();
    setCursor(Qt::OpenHandCursor);
    if (commitHandler_) {
      commitHandler_(value_);
    }
    event->accept();
  }

  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor window = palette().color(QPalette::Window);
    const QColor text = palette().color(QPalette::Text);
    const QColor border = palette().color(QPalette::Mid);
    const QColor highlight = palette().color(QPalette::Highlight);

    const QRectF knobRect = rect().adjusted(2, 2, -2, -2);
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(window);
    painter.drawEllipse(knobRect);

    painter.setPen(QPen(blendColor(border, text, 0.35), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(knobRect.adjusted(3, 3, -3, -3));

    const QPointF center = knobRect.center();
    const qreal radius = std::min(knobRect.width(), knobRect.height()) * 0.32;
    const double radians = (value_ - 90.0) * std::numbers::pi / 180.0;
    const QPointF tip(center.x() + std::cos(radians) * radius,
                      center.y() + std::sin(radians) * radius);

    painter.setPen(QPen(highlight, 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center, tip);
    painter.setBrush(highlight);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(center, 1.7, 1.7);

    if (hasFocus()) {
      QPen focusPen(highlight.lighter(125), 1.0, Qt::DashLine);
      painter.setPen(focusPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(knobRect.adjusted(1, 1, -1, -1));
    }
  }

private:
  double angleFromPosition(const QPointF &position) const {
    const QPointF center = rect().center();
    const QPointF delta = position - center;
    return std::atan2(delta.y(), delta.x()) * 180.0 / std::numbers::pi + 90.0;
  }

  double value_ = 0.0;
  double lastAngle_ = 0.0;
  bool dragging_ = false;
  ValueHandler previewHandler_;
  ValueHandler commitHandler_;
};

} // namespace

bool artifactShouldShowPropertyResetButtons() {
  return artifactShouldShowPropertyResetButtonsImpl();
}

void artifactSetShowPropertyResetButtons(bool show) {
  artifactSetShowPropertyResetButtonsImpl(show);
}

ArtifactFloatPropertyEditor::ArtifactFloatPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent,
    const bool showSlider)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyFloatEditor"));
  spinBox_ = new ArtifactRelativeDoubleSpinBox(this);
  if (showSlider) {
    slider_ = new PropertySliderWidget(this);
    applyPropertyFieldPalette(slider_);
    knob_ = new PropertyNumericKnobWidget(this);
    applyPropertyFieldPalette(knob_, true);
  }
  QPushButton *resetButton = nullptr;
  if (::Artifact::artifactShouldShowPropertyResetButtons()) {
    resetButton = new QPushButton(QStringLiteral("⟲"), this);
    resetButton->setObjectName(QStringLiteral("propertyResetButton"));
    resetButton->setFixedSize(20, 20);
    resetButton->setToolTip(QStringLiteral("Reset to default"));
    resetButton->setFlat(true);
    applyPropertyButtonPalette(resetButton);

    QObject::connect(resetButton, &QPushButton::clicked, this,
                     [this, property]() {
                       const QVariant defaultValue =
                           getPropertyDefaultValue(property);
                       const double defaultNumericValue = defaultValue.toDouble();
                       if (spinBox_) {
                         spinBox_->setValue(defaultNumericValue);
                       }
                       if (slider_) {
                         const QSignalBlocker blocker(slider_);
                         slider_->setValue(floatToSliderPosition(
                             defaultNumericValue, softMin_, softMax_));
                         if (auto *propertySlider =
                                 static_cast<PropertySliderWidget *>(slider_)) {
                           propertySlider->setDisplayText(
                               formatNumericSliderText(
                                   defaultNumericValue,
                                   spinBox_ ? spinBox_->suffix().trimmed()
                                            : QString(),
                                   3));
                         }
                       }
                       if (auto *propertyKnob =
                               static_cast<PropertyNumericKnobWidget *>(knob_)) {
                         propertyKnob->setValue(defaultNumericValue);
                       }
                       commitCurrentValue();
                     });
  }

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  spinBox_->setMinimumWidth(kNumericEditorValueWidth);
  spinBox_->setMaximumWidth(kNumericEditorValueWidth);
  spinBox_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  if (slider_) {
    if (g_numericEditorLayoutMode ==
        ArtifactNumericEditorLayoutMode::SliderThenValue) {
      layout->addWidget(slider_, 3);
      if (knob_) {
        layout->addWidget(knob_, 0);
      }
      layout->addWidget(spinBox_, 1);
    } else {
      if (knob_) {
        layout->addWidget(knob_, 0);
      }
      layout->addWidget(spinBox_, 1);
      layout->addWidget(slider_, 3);
    }
  } else {
    layout->addWidget(spinBox_, 1);
  }

  const auto meta = property.metadata();
  const double hardMin =
      meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1e6;
  const double hardMax = meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1e6;
  const auto [resolvedSoftMin, resolvedSoftMax] =
      resolveFloatSoftRange(property, meta, hardMin, hardMax);
  softMin_ = resolvedSoftMin;
  softMax_ = resolvedSoftMax;
  if (softMax_ <= softMin_) {
    softMin_ = hardMin;
    softMax_ = hardMax;
  }
  if (auto *propertyKnob = static_cast<PropertyNumericKnobWidget *>(knob_)) {
    propertyKnob->setRange(softMin_, softMax_);
    propertyKnob->setValue(property.getValue().toDouble());
    propertyKnob->setPreviewHandler([this](const double nextValue) {
      if (!spinBox_) {
        return;
      }
      spinBox_->setValue(nextValue);
      previewValue(spinBox_->value());
    });
    propertyKnob->setCommitHandler([this](const double nextValue) {
      if (!spinBox_) {
        return;
      }
      spinBox_->setValue(nextValue);
      commitValue(spinBox_->value());
    });
  }

  spinBox_->setRange(meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1e6,
                     meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1e6);
  spinBox_->setValue(property.getValue().toDouble());
  {
    QFont font = spinBox_->font();
    font.setPointSize(11);
    font.setWeight(QFont::DemiBold);
    spinBox_->setFont(font);
    applyPropertyFieldPalette(spinBox_);
    applyThemeTextPalette(spinBox_);
  }
  if (meta.step.isValid()) {
    spinBox_->setSingleStep(meta.step.toDouble());
  }
  if (!meta.unit.isEmpty()) {
    spinBox_->setSuffix(QStringLiteral(" ") + meta.unit);
  }
  const QString sliderUnit = meta.unit;
  spinBox_->setMinimumHeight(22);
  spinBox_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spinBox_->setFrame(false);

  if (slider_) {
    slider_->setRange(0, 10000); // 精度を向上
    slider_->setMinimumHeight(24);
    slider_->setTracking(true); // ドラッグ中の追従を有効化
    slider_->setValue(floatToSliderPosition(property.getValue().toDouble(),
                                            softMin_, softMax_));
    if (auto *propertySlider = static_cast<PropertySliderWidget *>(slider_)) {
      propertySlider->setDisplayText(
          formatNumericSliderText(property.getValue().toDouble(), meta.unit, 3));
    }
  }

  QObject::connect(spinBox_, &QDoubleSpinBox::valueChanged, this,
                   [this, sliderUnit](const double nextValue) {
                     if (slider_) {
                       const QSignalBlocker blocker(slider_);
                       slider_->setValue(
                           floatToSliderPosition(nextValue, softMin_, softMax_));
                       if (auto *propertySlider =
                               static_cast<PropertySliderWidget *>(slider_)) {
                         propertySlider->setDisplayText(
                             formatNumericSliderText(nextValue, sliderUnit, 3));
                       }
                     }
                     if (auto *propertyKnob =
                             static_cast<PropertyNumericKnobWidget *>(knob_)) {
                       propertyKnob->setValue(nextValue);
                     }
                     if (spinBox_->hasFocus() && !sliderInteracting_) {
                       previewValue(nextValue);
                     }
                   });
  QObject::connect(spinBox_, &QDoubleSpinBox::editingFinished, this,
                   [this]() { commitValue(spinBox_->value()); });
  if (slider_) {
    QObject::connect(slider_, &QSlider::sliderPressed, this, [this]() {
      sliderInteracting_ = true;
      previewCurrentValue();
    });

    QObject::connect(
        slider_, &QSlider::valueChanged, this, [this, sliderUnit](const int sliderValue) {
          const double nextValue =
              this->sliderPositionToFloat(sliderValue, softMin_, softMax_);
          const QSignalBlocker blocker(spinBox_);
          spinBox_->setValue(nextValue);
          if (auto *propertyKnob = static_cast<PropertyNumericKnobWidget *>(knob_)) {
            propertyKnob->setValue(nextValue);
          }
          if (auto *propertySlider = static_cast<PropertySliderWidget *>(slider_)) {
            propertySlider->setDisplayText(
                formatNumericSliderText(nextValue, sliderUnit, 3));
          }
          if (sliderInteracting_) {
            previewValue(nextValue);
          }
        });
    QObject::connect(slider_, &QSlider::sliderReleased, this, [this]() {
      if (!sliderInteracting_) {
        return;
      }
      sliderInteracting_ = false;
      commitCurrentValue();
    });

    Artifact::installSliderJumpBehavior(this);
  }
}

bool ArtifactFloatPropertyEditor::eventFilter(QObject *watched, QEvent *event) {
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

int ArtifactFloatPropertyEditor::floatToSliderPosition(double val, double min,
                                                       double max) const {
  if (std::abs(max - min) < 1e-7)
    return 0;
  double ratio = (val - min) / (max - min);
  return static_cast<int>(std::clamp(ratio, 0.0, 1.0) * 10000.0);
}

double ArtifactFloatPropertyEditor::sliderPositionToFloat(int pos, double min,
                                                          double max) const {
  double ratio = static_cast<double>(pos) / 10000.0;
  return min + ratio * (max - min);
}

QVariant ArtifactFloatPropertyEditor::value() const {
  return spinBox_ ? QVariant(spinBox_->value()) : QVariant();
}

void ArtifactFloatPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!spinBox_) {
    return;
  }
  const double nextValue = value.toDouble();
  {
    const QSignalBlocker spinBlocker(spinBox_);
    spinBox_->setValue(nextValue);
  }
  if (slider_) {
    const QSignalBlocker sliderBlocker(slider_);
    slider_->setValue(this->floatToSliderPosition(nextValue, softMin_, softMax_));
    if (auto *propertySlider = static_cast<PropertySliderWidget *>(slider_)) {
      propertySlider->setDisplayText(
          formatNumericSliderText(nextValue, spinBox_->suffix().trimmed(), 3));
    }
  }
  if (auto *propertyKnob = static_cast<PropertyNumericKnobWidget *>(knob_)) {
    propertyKnob->setValue(nextValue);
  }
}

bool ArtifactFloatPropertyEditor::supportsScrub() const { return true; }

QWidget *ArtifactFloatPropertyEditor::scrubTargetWidget() const {
  if (!spinBox_) {
    return ArtifactAbstractPropertyEditor::scrubTargetWidget();
  }
  auto *spinBox = static_cast<ArtifactRelativeDoubleSpinBox *>(spinBox_);
  if (auto *lineEdit = spinBox->scrubLineEdit()) {
    return lineEdit;
  }
  return ArtifactAbstractPropertyEditor::scrubTargetWidget();
}

void ArtifactFloatPropertyEditor::scrubByPixels(
    const int deltaPixels, const Qt::KeyboardModifiers modifiers) {
  if (!spinBox_) {
    return;
  }

  double range = std::abs(softMax_ - softMin_);
  if (range < 1e-5)
    range = 100.0;

  double sensitivity = range / 500.0;
  if (modifiers.testFlag(Qt::ShiftModifier)) {
    sensitivity *= 0.1;
  }
  if (modifiers.testFlag(Qt::ControlModifier)) {
    sensitivity *= 5.0;
  }

  const double nextValue =
      spinBox_->value() + static_cast<double>(deltaPixels) * sensitivity;
  spinBox_->setValue(nextValue);
}

ArtifactIntPropertyEditor::ArtifactIntPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent,
    const bool showSlider)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyIntEditor"));
  spinBox_ = new ArtifactRelativeSpinBox(this);
  if (showSlider) {
    slider_ = new PropertySliderWidget(this);
    applyPropertyFieldPalette(slider_);
    knob_ = new PropertyNumericKnobWidget(this);
    applyPropertyFieldPalette(knob_, true);
  }
  QPushButton *resetButton = nullptr;
  if (::Artifact::artifactShouldShowPropertyResetButtons()) {
    resetButton = new QPushButton(QStringLiteral("⟲"), this);
    resetButton->setObjectName(QStringLiteral("propertyResetButton"));
    resetButton->setFixedSize(20, 20);
    resetButton->setToolTip(QStringLiteral("Reset to default"));
    resetButton->setFlat(true);
    applyPropertyButtonPalette(resetButton);
  }
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  spinBox_->setMinimumWidth(kNumericEditorValueWidth);
  spinBox_->setMaximumWidth(kNumericEditorValueWidth);
  spinBox_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  if (slider_) {
    if (g_numericEditorLayoutMode ==
        ArtifactNumericEditorLayoutMode::SliderThenValue) {
      layout->addWidget(slider_, 3);
      if (knob_) {
        layout->addWidget(knob_, 0);
      }
      layout->addWidget(spinBox_, 1);
    } else {
      if (knob_) {
        layout->addWidget(knob_, 0);
      }
      layout->addWidget(spinBox_, 1);
      layout->addWidget(slider_, 3);
    }
  } else {
    layout->addWidget(spinBox_, 1);
  }

  // ✅ リセットボタンを一番右に追加
  if (resetButton) {
    layout->addWidget(resetButton, 0);
  }

  const auto meta = property.metadata();
  const int hardMin = meta.hardMin.isValid() ? meta.hardMin.toInt() : -1000000;
  const int hardMax = meta.hardMax.isValid() ? meta.hardMax.toInt() : 1000000;
  const auto [resolvedSoftMin, resolvedSoftMax] =
      resolveIntSoftRange(property, meta, hardMin, hardMax);
  softMin_ = resolvedSoftMin;
  softMax_ = resolvedSoftMax;
  if (softMax_ <= softMin_) {
    softMin_ = hardMin;
    softMax_ = hardMax;
  }
  if (auto *propertyKnob = static_cast<PropertyNumericKnobWidget *>(knob_)) {
    propertyKnob->setRange(static_cast<double>(softMin_),
                           static_cast<double>(softMax_));
    propertyKnob->setValue(static_cast<double>(property.getValue().toInt()));
    propertyKnob->setPreviewHandler([this](const double nextValue) {
      if (!spinBox_) {
        return;
      }
      spinBox_->setValue(static_cast<int>(std::llround(nextValue)));
      previewValue(spinBox_->value());
    });
    propertyKnob->setCommitHandler([this](const double nextValue) {
      if (!spinBox_) {
        return;
      }
      spinBox_->setValue(static_cast<int>(std::llround(nextValue)));
      commitValue(spinBox_->value());
    });
  }

  spinBox_->setRange(meta.hardMin.isValid() ? meta.hardMin.toInt() : -1000000,
                     meta.hardMax.isValid() ? meta.hardMax.toInt() : 1000000);
  spinBox_->setValue(property.getValue().toInt());
  {
    QFont font = spinBox_->font();
    font.setPointSize(11);
    font.setWeight(QFont::DemiBold);
    spinBox_->setFont(font);
    applyPropertyFieldPalette(spinBox_);
    applyThemeTextPalette(spinBox_);
  }
  if (meta.step.isValid()) {
    spinBox_->setSingleStep(meta.step.toInt());
  }
  if (!meta.unit.isEmpty()) {
    spinBox_->setSuffix(QStringLiteral(" ") + meta.unit);
  }
  const QString sliderUnit = meta.unit;
  spinBox_->setMinimumHeight(22);
  spinBox_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spinBox_->setFrame(false);

  if (slider_) {
    slider_->setRange(0, 10000); // 精度を向上
    slider_->setMinimumHeight(24);
    slider_->setTracking(true);
    slider_->setValue(
        intToSliderPosition(property.getValue().toInt(), softMin_, softMax_));
          if (auto *propertySlider = static_cast<PropertySliderWidget *>(slider_)) {
      propertySlider->setDisplayText(
          formatNumericSliderText(property.getValue().toInt(), meta.unit, 0));
    }
  }

  QObject::connect(
      spinBox_, &QSpinBox::valueChanged, this, [this, sliderUnit](const int nextValue) {
        if (slider_) {
          const QSignalBlocker blocker(slider_);
          slider_->setValue(intToSliderPosition(nextValue, softMin_, softMax_));
          if (auto *propertySlider =
                  static_cast<PropertySliderWidget *>(slider_)) {
            propertySlider->setDisplayText(
                formatNumericSliderText(nextValue, sliderUnit, 0));
          }
        }
        if (auto *propertyKnob = static_cast<PropertyNumericKnobWidget *>(knob_)) {
          propertyKnob->setValue(static_cast<double>(nextValue));
        }
        if (spinBox_->hasFocus() && !sliderInteracting_) {
          previewValue(nextValue);
        }
      });
  QObject::connect(spinBox_, &QSpinBox::editingFinished, this,
                   [this]() { commitValue(spinBox_->value()); });
  if (slider_) {
    QObject::connect(slider_, &QSlider::sliderPressed, this, [this]() {
      sliderInteracting_ = true;
      previewCurrentValue();
    });
    QObject::connect(slider_, &QSlider::valueChanged, this,
                     [this, sliderUnit](const int sliderValue) {
                       const int nextValue =
                           sliderPositionToInt(sliderValue, softMin_, softMax_);
                       const QSignalBlocker blocker(spinBox_);
                       spinBox_->setValue(nextValue);
                       if (auto *propertyKnob =
                               static_cast<PropertyNumericKnobWidget *>(knob_)) {
                         propertyKnob->setValue(static_cast<double>(nextValue));
                       }
                       if (auto *propertySlider =
                               static_cast<PropertySliderWidget *>(slider_)) {
                         propertySlider->setDisplayText(
                             formatNumericSliderText(nextValue, sliderUnit, 0));
                       }
                       if (sliderInteracting_) {
                         previewValue(nextValue);
                       }
                     });
    QObject::connect(slider_, &QSlider::sliderReleased, this, [this]() {
      if (!sliderInteracting_) {
        return;
      }
      sliderInteracting_ = false;
      commitCurrentValue();
    });
    Artifact::installSliderJumpBehavior(this);
  }
}

QVariant ArtifactIntPropertyEditor::value() const {
  return spinBox_ ? QVariant(spinBox_->value()) : QVariant();
}

void ArtifactIntPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!spinBox_) {
    return;
  }
  const int nextValue = value.toInt();
  {
    const QSignalBlocker spinBlocker(spinBox_);
    spinBox_->setValue(nextValue);
  }
  if (slider_) {
    const QSignalBlocker sliderBlocker(slider_);
    slider_->setValue(intToSliderPosition(nextValue, softMin_, softMax_));
    if (auto *propertySlider = static_cast<PropertySliderWidget *>(slider_)) {
      propertySlider->setDisplayText(
          formatNumericSliderText(nextValue, spinBox_->suffix().trimmed(), 0));
    }
  }
  if (auto *propertyKnob = static_cast<PropertyNumericKnobWidget *>(knob_)) {
    propertyKnob->setValue(static_cast<double>(nextValue));
  }
}

bool ArtifactIntPropertyEditor::supportsScrub() const { return true; }

QWidget *ArtifactIntPropertyEditor::scrubTargetWidget() const {
  if (!spinBox_) {
    return ArtifactAbstractPropertyEditor::scrubTargetWidget();
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
  return ::Artifact::intToSliderPosition(value, min, max);
}

int ArtifactIntPropertyEditor::sliderPositionToInt(int pos, int min,
                                                   int max) const {
  return ::Artifact::sliderPositionToInt(pos, min, max);
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
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyAnimatorCountEditor"));

  const auto meta = property.metadata();
  minCount_ = meta.hardMin.isValid() ? meta.hardMin.toInt() : 0;
  maxCount_ = meta.hardMax.isValid() ? meta.hardMax.toInt() : 16;
  if (maxCount_ < minCount_) {
    std::swap(maxCount_, minCount_);
  }

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  removeButton_ = new PropertyCallbackButton(QStringLiteral("-"), this);
  removeButton_->setToolTip(QStringLiteral("Remove last animator"));
  removeButton_->setFixedHeight(24);
  removeButton_->setMinimumWidth(28);
  applyPropertyButtonPalette(removeButton_, false);
  static_cast<PropertyCallbackButton *>(removeButton_)->setCallback(
      [this]() { stepCount(-1); });

  countLabel_ = new QLabel(this);
  countLabel_->setAlignment(Qt::AlignCenter);
  countLabel_->setMinimumHeight(24);
  countLabel_->setMinimumWidth(92);
  countLabel_->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
  applyPropertyFieldPalette(countLabel_, true);
  applyPropertyLabelPalette(countLabel_, true);

  addButton_ = new PropertyCallbackButton(QStringLiteral("+"), this);
  addButton_->setToolTip(QStringLiteral("Add animator (Click to select type)"));
  addButton_->setFixedHeight(24);
  addButton_->setMinimumWidth(28);
  applyPropertyButtonPalette(addButton_, true);
  static_cast<PropertyCallbackButton *>(addButton_)->setCallback(
      [this]() {
        QMenu menu(this);
        QAction *defaultAct = menu.addAction(QStringLiteral("Default Animator"));
        menu.addSeparator();
        QAction *typewriterAct = menu.addAction(QStringLiteral("Typewriter Preset"));
        QAction *slideUpAct = menu.addAction(QStringLiteral("Slide Up Preset"));
        QAction *scaleInAct = menu.addAction(QStringLiteral("Scale In Preset"));
        QAction *rotationInAct = menu.addAction(QStringLiteral("Rotation In Preset"));
        QAction *trackingFadeAct = menu.addAction(QStringLiteral("Tracking Fade Preset"));
        QAction *wigglyPositionAct = menu.addAction(QStringLiteral("Wiggly Position Preset"));
        QAction *blurRevealAct = menu.addAction(QStringLiteral("Blur Reveal Preset"));

        defaultAct->setToolTip(QStringLiteral("Standard animator with blank settings"));
        typewriterAct->setToolTip(QStringLiteral("Typewriter animation: scale, opacity, tracking, blur"));
        slideUpAct->setToolTip(QStringLiteral("Slide up animation: position, opacity"));
        scaleInAct->setToolTip(QStringLiteral("Scale in animation: scale, opacity"));
        rotationInAct->setToolTip(QStringLiteral("Rotation in animation: scale, rotation, opacity"));
        trackingFadeAct->setToolTip(QStringLiteral("Tracking fade animation: tracking, scale, opacity"));
        wigglyPositionAct->setToolTip(QStringLiteral("Wiggly selector: smooth position and rotation wiggles"));
        blurRevealAct->setToolTip(QStringLiteral("Blur reveal animation: scale, opacity, blur"));

        QAction *chosen = menu.exec(addButton_->mapToGlobal(QPoint(0, addButton_->height())));
        if (!chosen) {
          return;
        }

        const int nextCount = std::clamp(currentCount_ + 1, minCount_, maxCount_);
        if (nextCount == currentCount_) {
          return;
        }
        currentCount_ = nextCount;
        syncUi();

        int presetId = 0;
        if (chosen == typewriterAct) presetId = 1;
        else if (chosen == slideUpAct) presetId = 2;
        else if (chosen == scaleInAct) presetId = 3;
        else if (chosen == rotationInAct) presetId = 4;
        else if (chosen == trackingFadeAct) presetId = 5;
        else if (chosen == wigglyPositionAct) presetId = 6;
        else if (chosen == blurRevealAct) presetId = 7;

        if (presetId > 0) {
          commitValue((presetId * 100) + currentCount_);
        } else {
          commitValue(currentCount_);
        }
      });

  layout->addWidget(removeButton_, 0);
  layout->addWidget(countLabel_, 1);
  layout->addWidget(addButton_, 0);

  setValueFromVariant(property.getValue());
}

QVariant ArtifactAnimatorCountPropertyEditor::value() const {
  return currentCount_;
}

void ArtifactAnimatorCountPropertyEditor::setValueFromVariant(
    const QVariant &value) {
  currentCount_ = std::clamp(value.toInt(), minCount_, maxCount_);
  syncUi();
}

void ArtifactAnimatorCountPropertyEditor::stepCount(const int delta) {
  const int nextCount =
      std::clamp(currentCount_ + delta, minCount_, maxCount_);
  if (nextCount == currentCount_) {
    return;
  }
  currentCount_ = nextCount;
  syncUi();
  commitValue(currentCount_);
}

void ArtifactAnimatorCountPropertyEditor::syncUi() {
  if (countLabel_) {
    countLabel_->setText(
        QStringLiteral("%1 animator%2")
            .arg(currentCount_)
            .arg(currentCount_ == 1 ? QStringLiteral("") : QStringLiteral("s")));
  }
  if (removeButton_) {
    removeButton_->setEnabled(currentCount_ > minCount_);
  }
  if (addButton_) {
    addButton_->setEnabled(currentCount_ < maxCount_);
  }
}

ArtifactDashPatternPropertyEditor::ArtifactDashPatternPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyDashPatternEditor"));

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  presetCombo_ = new PropertyComboBox(this);
  presetCombo_->addItem(QStringLiteral("Solid"), QString());
  presetCombo_->addItem(QStringLiteral("Dotted"), QStringLiteral("2,4"));
  presetCombo_->addItem(QStringLiteral("Dashed"), QStringLiteral("6,3"));
  presetCombo_->addItem(QStringLiteral("Dash-Dot"), QStringLiteral("6,3,2,3"));
  presetCombo_->addItem(QStringLiteral("Dash-Dot-Dot"), QStringLiteral("6,3,2,3,2,3"));
  presetCombo_->addItem(QStringLiteral("Custom"), QString());
  presetCombo_->setMinimumHeight(22);
  applyPropertyFieldPalette(presetCombo_, true);

  customEdit_ = new QLineEdit(this);
  customEdit_->setPlaceholderText(QStringLiteral("e.g. 4,2"));
  customEdit_->setMinimumHeight(22);
  customEdit_->setEnabled(false);
  applyPropertyFieldPalette(customEdit_, true);

  layout->addWidget(presetCombo_, 1);
  layout->addWidget(customEdit_, 1);

  QObject::connect(presetCombo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
    const QString pattern = presetCombo_->itemData(idx).toString();
    customEdit_->setEnabled(pattern.isEmpty() && idx == presetCombo_->count() - 1);
    if (!pattern.isEmpty()) {
      customEdit_->setText(pattern);
      commitValue(pattern);
    }
  });

  QObject::connect(customEdit_, &QLineEdit::editingFinished, this, [this]() {
    commitValue(customEdit_->text());
  });

  setValueFromVariant(property.getValue());
}

QVariant ArtifactDashPatternPropertyEditor::value() const {
  if (!customEdit_) return {};
  return customEdit_->text();
}

void ArtifactDashPatternPropertyEditor::setValueFromVariant(const QVariant &value) {
  const QString pattern = value.toString();
  customEdit_->setText(pattern);
  for (int i = 0; i < presetCombo_->count(); ++i) {
    if (presetCombo_->itemData(i).toString() == pattern) {
      presetCombo_->setCurrentIndex(i);
      customEdit_->setEnabled(false);
      return;
    }
  }
  if (!pattern.isEmpty()) {
    presetCombo_->setCurrentIndex(presetCombo_->count() - 1);
    customEdit_->setEnabled(true);
  } else {
    presetCombo_->setCurrentIndex(0);
    customEdit_->setEnabled(false);
  }
}

void ArtifactDashPatternPropertyEditor::applyPreset(const QString& pattern) {
  customEdit_->setText(pattern);
  commitValue(pattern);
}

ArtifactRotationPropertyEditor::ArtifactRotationPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyRotationEditor"));

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  auto *knob = new PropertyRotationKnobWidget(this);
  knob_ = knob;
  knob->setFixedSize(36, 36);
  applyPropertyFieldPalette(knob);

  spinBox_ = new ArtifactRelativeDoubleSpinBox(this);
  const auto meta = property.metadata();
  spinBox_->setRange(meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1000000.0,
                     meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1000000.0);
  spinBox_->setValue(property.getValue().toDouble());
  if (meta.step.isValid()) {
    spinBox_->setSingleStep(meta.step.toDouble());
  } else {
    spinBox_->setSingleStep(1.0);
  }
  if (!meta.unit.isEmpty()) {
    spinBox_->setSuffix(QStringLiteral(" ") + meta.unit);
  }
  spinBox_->setMinimumHeight(22);
  spinBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  spinBox_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spinBox_->setFrame(false);
  {
    QFont font = spinBox_->font();
    font.setPointSize(11);
    font.setWeight(QFont::DemiBold);
    spinBox_->setFont(font);
    applyPropertyFieldPalette(spinBox_);
    applyThemeTextPalette(spinBox_);
  }

  layout->addWidget(spinBox_, 0);
  layout->addWidget(knob_, 0, Qt::AlignHCenter);

  knob->setValue(spinBox_->value());
  knob->setPreviewHandler([this](const double value) {
    if (!spinBox_) {
      return;
    }
    const QSignalBlocker blocker(spinBox_);
    spinBox_->setValue(value);
    previewValue(value);
  });
  knob->setCommitHandler([this](const double value) {
    if (!spinBox_) {
      return;
    }
    const QSignalBlocker blocker(spinBox_);
    spinBox_->setValue(value);
    commitValue(value);
  });

  QObject::connect(spinBox_, &QDoubleSpinBox::valueChanged, this,
                   [this, knob](const double nextValue) {
                     const QSignalBlocker blocker(knob);
                     knob->setValue(nextValue);
                     if (spinBox_->hasFocus()) {
                       previewValue(nextValue);
                     }
                   });
  QObject::connect(spinBox_, &QDoubleSpinBox::editingFinished, this,
                   [this]() { commitValue(spinBox_->value()); });
}

QVariant ArtifactRotationPropertyEditor::value() const {
  return spinBox_ ? QVariant(spinBox_->value()) : QVariant();
}

void ArtifactRotationPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!spinBox_) {
    return;
  }
  const double nextValue = value.toDouble();
  {
    const QSignalBlocker spinBlocker(spinBox_);
    spinBox_->setValue(nextValue);
  }
  if (auto *knob = static_cast<PropertyRotationKnobWidget *>(knob_)) {
    const QSignalBlocker knobBlocker(knob);
    knob->setValue(nextValue);
  }
}

bool ArtifactRotationPropertyEditor::supportsScrub() const { return true; }

void ArtifactRotationPropertyEditor::scrubByPixels(
    const int deltaPixels, const Qt::KeyboardModifiers modifiers) {
  if (!spinBox_) {
    return;
  }
  double sensitivity = 0.5;
  if (modifiers.testFlag(Qt::ShiftModifier)) {
    sensitivity *= 0.2;
  }
  if (modifiers.testFlag(Qt::ControlModifier)) {
    sensitivity *= 4.0;
  }
  spinBox_->setValue(spinBox_->value() +
                     static_cast<double>(deltaPixels) * sensitivity);
}

QWidget *ArtifactRotationPropertyEditor::scrubTargetWidget() const {
  if (!spinBox_) {
    return ArtifactAbstractPropertyEditor::scrubTargetWidget();
  }
  auto *spinBox = static_cast<ArtifactRelativeDoubleSpinBox *>(spinBox_);
  if (auto *lineEdit = spinBox->scrubLineEdit()) {
    return lineEdit;
  }
  return ArtifactAbstractPropertyEditor::scrubTargetWidget();
}

ArtifactBoolPropertyEditor::ArtifactBoolPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyBoolEditor"));
  toggleSwitch_ = new ArtifactToggleSwitch(this);
  applyPropertyFieldPalette(toggleSwitch_);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(toggleSwitch_, 0, Qt::AlignLeft | Qt::AlignVCenter);
  layout->addStretch();

  toggleSwitch_->setChecked(property.getValue().toBool());
  QObject::connect(toggleSwitch_, &QAbstractButton::toggled, this,
                   [this](const bool checked) { commitValue(checked); });
}

QVariant ArtifactBoolPropertyEditor::value() const {
  return toggleSwitch_ ? QVariant(toggleSwitch_->isChecked()) : QVariant();
}

void ArtifactBoolPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!toggleSwitch_) {
    return;
  }
  const QSignalBlocker blocker(toggleSwitch_);
  toggleSwitch_->setChecked(value.toBool());
}

ArtifactStringPropertyEditor::ArtifactStringPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyStringEditor"));
  lineEdit_ = new QLineEdit(property.getValue().toString(), this);
  lineEdit_->setMinimumHeight(26);
  lineEdit_->setFrame(false);
  applyPropertyFieldPalette(lineEdit_);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(lineEdit_);

  QObject::connect(lineEdit_, &QLineEdit::editingFinished, this,
                   [this]() { commitValue(lineEdit_->text()); });
}

QVariant ArtifactStringPropertyEditor::value() const {
  return lineEdit_ ? QVariant(lineEdit_->text()) : QVariant();
}

void ArtifactStringPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!lineEdit_) {
    return;
  }
  const QSignalBlocker blocker(lineEdit_);
  lineEdit_->setText(value.toString());
}

ArtifactMultilineStringPropertyEditor::ArtifactMultilineStringPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyMultilineStringEditor"));
  textEdit_ = new QTextEdit(this);
  textEdit_->setAcceptRichText(false);
  textEdit_->setMinimumHeight(72);
  textEdit_->setTabChangesFocus(true);
  textEdit_->setLineWrapMode(QTextEdit::WidgetWidth);
  textEdit_->setFrameStyle(QFrame::NoFrame);
  applyPropertyFieldPalette(textEdit_, true);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(textEdit_);

  setValueFromVariant(property.getValue());

  QObject::connect(textEdit_, &QTextEdit::textChanged, this,
                   [this]() { previewValue(textEdit_->toPlainText()); });
  textEdit_->installEventFilter(this);
}

QVariant ArtifactMultilineStringPropertyEditor::value() const {
  return textEdit_ ? QVariant(textEdit_->toPlainText()) : QVariant();
}

void ArtifactMultilineStringPropertyEditor::setValueFromVariant(
    const QVariant &value) {
  if (!textEdit_) {
    return;
  }
  const QSignalBlocker blocker(textEdit_);
  textEdit_->setPlainText(value.toString());
}

bool ArtifactMultilineStringPropertyEditor::eventFilter(QObject *watched,
                                                        QEvent *event) {
  if (watched == textEdit_ && event->type() == QEvent::FocusOut) {
    commitCurrentValue();
  }
  return ArtifactAbstractPropertyEditor::eventFilter(watched, event);
}

ArtifactFontFamilyPropertyEditor::ArtifactFontFamilyPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyFontEditor"));
  fontPicker_ = new FontPickerWidget(this);
  applyPropertyFieldPalette(fontPicker_);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(fontPicker_);

  setValueFromVariant(property.getValue());
  if (false) {
    QObject::connect(fontPicker_, &FontPickerWidget::fontChanged, this,
                     [this](const QString &family) {
                       commitValue(
                           ArtifactCore::FontManager::resolvedFamily(family));
                     });
  }

  fontChangeSubscription_ =
      ArtifactCore::globalEventBus().subscribe<FontChangedEvent>(
          [this](const FontChangedEvent &ev) {
            commitValue(ArtifactCore::FontManager::resolvedFamily(ev.fontName));
          });
}

QVariant ArtifactFontFamilyPropertyEditor::value() const {
  return fontPicker_ ? QVariant(fontPicker_->currentFont()) : QVariant();
}

void ArtifactFontFamilyPropertyEditor::setValueFromVariant(
    const QVariant &value) {
  if (!fontPicker_) {
    return;
  }
  const QString family =
      ArtifactCore::FontManager::resolvedFamily(value.toString());
  fontPicker_->setCurrentFont(family);
}

ArtifactPathPropertyEditor::ArtifactPathPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyPathEditor"));
  lineEdit_ = new QLineEdit(property.getValue().toString(), this);
  browseButton_ = new QPushButton(QStringLiteral("..."), this);
  lineEdit_->setMinimumHeight(26);
  lineEdit_->setFrame(false);
  browseButton_->setObjectName(QStringLiteral("propertyPathBrowseButton"));
  browseButton_->setFixedSize(28, 26);
  applyPropertyFieldPalette(lineEdit_);
  applyPropertyButtonPalette(browseButton_);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  layout->addWidget(lineEdit_, 1);
  layout->addWidget(browseButton_, 0);

  const QString propertyName = property.getName();
  QObject::connect(lineEdit_, &QLineEdit::editingFinished, this,
                   [this]() { commitValue(lineEdit_->text()); });
  QObject::connect(browseButton_, &QPushButton::clicked, this,
                   [this, propertyName]() {
                     const QString initialPath = lineEdit_->text().trimmed();
                     const QString selectedPath = QFileDialog::getOpenFileName(
                         this, QStringLiteral("Select Source"), initialPath,
                         fileDialogFilterForProperty(propertyName));
                     if (selectedPath.isEmpty()) {
                       return;
                     }
                     lineEdit_->setText(selectedPath);
                     commitValue(selectedPath);
                   });
}

QVariant ArtifactPathPropertyEditor::value() const {
  return lineEdit_ ? QVariant(lineEdit_->text()) : QVariant();
}

void ArtifactPathPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!lineEdit_) {
    return;
  }
  const QSignalBlocker blocker(lineEdit_);
  lineEdit_->setText(value.toString());
}

ArtifactEnumPropertyEditor::ArtifactEnumPropertyEditor(
    const ArtifactCore::AbstractProperty &property, OptionList options,
    QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent), options_(std::move(options)) {
  setObjectName(QStringLiteral("propertyEnumEditor"));
  comboBox_ = new PropertyComboBox(this);
  comboBox_->setMinimumHeight(26);
  comboBox_->setFrame(false);
  applyPropertyFieldPalette(comboBox_);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(comboBox_);

  for (const auto &[value, label] : options_) {
    comboBox_->addItem(label, value);
  }
  setValueFromVariant(property.getValue());

  QObject::connect(comboBox_, &QComboBox::currentIndexChanged, this,
                   [this](int index) {
                     if (index < 0) {
                       return;
                     }
                     commitValue(comboBox_->currentData());
                   });
}

QVariant ArtifactEnumPropertyEditor::value() const {
  return comboBox_ ? comboBox_->currentData() : QVariant();
}

void ArtifactEnumPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!comboBox_) {
    return;
  }
  const int desired = value.toInt();
  for (int i = 0; i < comboBox_->count(); ++i) {
    if (comboBox_->itemData(i).toInt() == desired) {
      const QSignalBlocker blocker(comboBox_);
      comboBox_->setCurrentIndex(i);
      return;
    }
  }
}

ArtifactColorPropertyEditor::ArtifactColorPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyColorEditor"));
  button_ = new QPushButton(QStringLiteral(" "), this);
  valueLabel_ = new QLabel(this);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  layout->addWidget(button_, 0);
  layout->addWidget(valueLabel_, 1);

  button_->setObjectName(QStringLiteral("propertyColorSwatchButton"));
  valueLabel_->setObjectName(QStringLiteral("propertyColorValueLabel"));
  button_->setFixedSize(36, 24);
  applyPropertyButtonPalette(button_, true);
  applyPropertyLabelPalette(valueLabel_);
  currentColor_ = propertyColor(property);
  applyColor(currentColor_);
  QObject::connect(button_, &QPushButton::clicked, this, [this]() {
    ArtifactWidgets::FloatColorPicker picker(button_);
    picker.setWindowTitle(QStringLiteral("Select Color"));
    picker.setInitialColor(ArtifactCore::FloatColor(
        currentColor_.redF(), currentColor_.greenF(), currentColor_.blueF(),
        currentColor_.alphaF()));
    if (picker.exec() != QDialog::Accepted) {
      return;
    }
    const ArtifactCore::FloatColor picked = picker.getColor();
    const QColor nextColor =
        QColor::fromRgbF(picked.r(), picked.g(), picked.b(), picked.a());
    if (!nextColor.isValid()) {
      return;
    }
    applyColor(nextColor);
    commitValue(nextColor);
  });
}

QVariant ArtifactColorPropertyEditor::value() const {
  return QVariant(currentColor_);
}

void ArtifactColorPropertyEditor::setValueFromVariant(const QVariant &value) {
  QColor nextColor;
  if (value.canConvert<QColor>()) {
    nextColor = value.value<QColor>();
  } else {
    const QString text = value.toString().trimmed();
    if (!text.isEmpty()) {
      nextColor = QColor(text);
    }
  }
  if (nextColor.isValid()) {
    applyColor(nextColor);
  }
}

void ArtifactColorPropertyEditor::applyColor(const QColor &color) {
  currentColor_ = color;
  if (button_) {
    QPalette pal = button_->palette();
    pal.setColor(QPalette::Button, color);
    pal.setColor(QPalette::ButtonText,
                 QColor::fromRgbF(color.redF() > 0.5 ? 0.08 : 0.94,
                                  color.greenF() > 0.5 ? 0.08 : 0.94,
                                  color.blueF() > 0.5 ? 0.08 : 0.94));
    button_->setAutoFillBackground(true);
    button_->setPalette(pal);
  }
  if (valueLabel_) {
    valueLabel_->setText(color.name(QColor::HexArgb).toUpper());
  }
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
  // Custom paintEvent draws a rounded-rect background; disable auto-fill so
  // setAutoFillBackground(true) inside applyPropertyFieldPalette doesn't paint
  // square corners on top of the parent container's background.
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

  editor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  applyPropertyFieldPalette(editor_);

  // Load Icons
  QIcon prevIcon = loadPropertyIcon(
      QStringLiteral("Studio/property_key_previous.svg"),
      QStringLiteral("Studio/property_key_previous.svg"));
  QIcon nextIcon = loadPropertyIcon(
      QStringLiteral("Studio/property_key_next.svg"),
      QStringLiteral("Studio/property_key_next.svg"));
  QIcon resIcon =
      loadPropertyIcon(QStringLiteral("MaterialVS/neutral/undo.svg"));
  QIcon exprIcon = loadPropertyIcon(QStringLiteral("MaterialVS/blue/code.svg"));

  // Keep secondary actions compact so the editable value remains the row's
  // dominant surface.
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
  keyframeButton_->setFixedSize(kPropertyKeyButtonSize, kPropertyKeyButtonSize);
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
  resetButton_->setIcon(resIcon);
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
  expressionButton_->setIcon(exprIcon);
  expressionButton_->setIconSize(QSize(14, 14));
  expressionButton_->setFlat(true);
  expressionButton_->setVisible(false);
  expressionButton_->setFocusPolicy(Qt::NoFocus);
  applyPropertyButtonPalette(expressionButton_);

  // Favorite (star) button — uses Unicode ★/☆ for reliable cross-platform display
  favoriteButton_ = new QPushButton(this);
  favoriteButton_->setObjectName(QStringLiteral("propertyFavButton"));
  favoriteButton_->setToolTip(
      QStringLiteral("Favorite: %1").arg(propertyName));
  favoriteButton_->setFixedSize(kPropertyKeyButtonSize, kPropertyKeyButtonSize);
  favoriteButton_->setCheckable(true);
  favoriteButton_->setText(QStringLiteral("\u2606")); // ☆
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
  layout->addWidget(auxContainer);

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
                   [this](bool checked) {
                     favoriteButton_->setText(checked
                        ? QStringLiteral("\u2605")  // ★ filled
                        : QStringLiteral("\u2606")); // ☆ outline
                     if (favoriteHandler_) {
                       favoriteHandler_(checked);
                     }
                   });

  QObject::connect(keyframeButton_, &QPushButton::toggled, this,
                   [this](bool checked) {
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

  QColor fill = propertySurfaceColor(false);
  if (hovered) {
    fill = blendColor(fill, accent, 0.04);
  }
  if (focused) {
    fill = blendColor(fill, selection, 0.24);
  }
  QLinearGradient baseGrad(frame.topLeft(), frame.bottomRight());
  baseGrad.setColorAt(0.0, blendColor(fill, QColor(QStringLiteral("#F2F4F1")),
                                      hovered ? 0.10 : 0.055));
  baseGrad.setColorAt(0.44, fill);
  baseGrad.setColorAt(1.0, blendColor(fill, QColor(QStringLiteral("#050708")),
                                      focused ? 0.12 : 0.07));
  painter.fillPath(path, baseGrad);

  QLinearGradient veilGrad(frame.topLeft(), frame.topRight());
  QColor leftVeil = blendColor(accent, QColor(QStringLiteral("#F4F6F2")),
                               focused ? 0.52 : 0.38);
  leftVeil.setAlpha(hovered || focused ? 30 : 18);
  QColor rightVeil = leftVeil;
  rightVeil.setAlpha(0);
  veilGrad.setColorAt(0.0, leftVeil);
  veilGrad.setColorAt(0.62, rightVeil);
  painter.fillPath(path, veilGrad);

  QColor line = border.lighter(118);
  if (hovered) {
    line = blendColor(line, accent, 0.34);
  }
  if (focused) {
    line = blendColor(line, selection, 0.48);
  }
  painter.setPen(QPen(line, 1.0));
  painter.drawPath(path);

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
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyTextAnimatorColorEditor"));

  textEdit_ = new QTextEdit(this);
  textEdit_->setAcceptRichText(false);
  textEdit_->setMinimumHeight(72);
  textEdit_->setTabChangesFocus(true);
  textEdit_->setLineWrapMode(QTextEdit::WidgetWidth);
  textEdit_->setFrameStyle(QFrame::NoFrame);
  applyPropertyFieldPalette(textEdit_, true);

  colorButton_ = new QPushButton(QStringLiteral(" "), this);
  colorButton_->setObjectName(QStringLiteral("propertyColorSwatchButton"));
  colorButton_->setFixedSize(36, 24);
  colorButton_->setToolTip(QStringLiteral("Apply color to selected text range"));
  colorButton_->hide();
  applyPropertyButtonPalette(colorButton_, true);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(textEdit_);
  layout->addWidget(colorButton_, 0, Qt::AlignTop);

  setValueFromVariant(property.getValue());

  QObject::connect(textEdit_, &QTextEdit::textChanged, this,
                   [this]() { previewValue(textEdit_->toPlainText()); });
  QObject::connect(textEdit_, &QTextEdit::selectionChanged, this,
                   &ArtifactTextAnimatorColorEditor::onSelectionChanged);
  QObject::connect(colorButton_, &QPushButton::clicked, this,
                   &ArtifactTextAnimatorColorEditor::onColorPicked);
  textEdit_->installEventFilter(this);
}

QVariant ArtifactTextAnimatorColorEditor::value() const {
  return textEdit_ ? QVariant(textEdit_->toPlainText()) : QVariant();
}

void ArtifactTextAnimatorColorEditor::setValueFromVariant(
    const QVariant &value) {
  if (!textEdit_) return;
  const QSignalBlocker blocker(textEdit_);
  textEdit_->setPlainText(value.toString());
}

bool ArtifactTextAnimatorColorEditor::eventFilter(QObject *watched,
                                                    QEvent *event) {
  if (watched == textEdit_ && event->type() == QEvent::FocusOut) {
    commitCurrentValue();
  }
  return ArtifactAbstractPropertyEditor::eventFilter(watched, event);
}

void ArtifactTextAnimatorColorEditor::onSelectionChanged() {
  if (!textEdit_) return;
  const QTextCursor cursor = textEdit_->textCursor();
  const bool hasSelection = cursor.hasSelection();
  colorButton_->setVisible(hasSelection);
}

void ArtifactTextAnimatorColorEditor::onColorPicked() {
  if (!textEdit_ || !layer_) return;

  const QTextCursor cursor = textEdit_->textCursor();
  if (!cursor.hasSelection()) return;

  const int selStart = cursor.selectionStart();
  const int selEnd = cursor.selectionEnd();
  if (selEnd <= selStart) return;

  ArtifactWidgets::FloatColorPicker picker(colorButton_);
  picker.setWindowTitle(QStringLiteral("Select Text Range Color"));
  picker.setInitialColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
  if (picker.exec() != QDialog::Accepted) return;

  const ArtifactCore::FloatColor picked = picker.getColor();
  const QColor qColor =
      QColor::fromRgbF(picked.r(), picked.g(), picked.b(), picked.a());
  if (!qColor.isValid()) return;

  const int animIdx = layer_->applyColorToSelectorRange(
      selStart, selEnd,
      ArtifactCore::FloatRGBA(picked.r(), picked.g(), picked.b(), picked.a()));
  if (animIdx >= 0) {
    colorButton_->hide();
    Q_EMIT colorApplied(selStart, selEnd, qColor);
  }
}

ArtifactAbstractPropertyEditor *
createPropertyEditorWidget(const ArtifactCore::AbstractProperty &property,
                           QWidget *parent) {
  if (isMultilineTextProperty(property)) {
    return new ArtifactTextAnimatorColorEditor(property, parent);
  }
  if (isFontFamilyProperty(property)) {
    return new ArtifactFontFamilyPropertyEditor(property, parent);
  }
  if (isPathProperty(property)) {
    return new ArtifactPathPropertyEditor(property, parent);
  }
  if (const auto enumOptions = enumOptionsForProperty(property)) {
    return new ArtifactEnumPropertyEditor(property, *enumOptions, parent);
  }
  if (property.getType() == ArtifactCore::PropertyType::Float &&
      property.getName() == QStringLiteral("transform.rotation")) {
    return new ArtifactRotationPropertyEditor(property, parent);
  }
  if (property.getType() == ArtifactCore::PropertyType::Integer &&
      property.getName() == QStringLiteral("text.animatorCount")) {
    return new ArtifactAnimatorCountPropertyEditor(property, parent);
  }
  if (property.getName() == QStringLiteral("shape.dashPattern")) {
    return new ArtifactDashPatternPropertyEditor(property, parent);
  }

  switch (property.getType()) {
  case ArtifactCore::PropertyType::Float:
    return new ArtifactFloatPropertyEditor(
        property, parent, shouldShowNumericSlider(property));
  case ArtifactCore::PropertyType::Integer:
    return new ArtifactIntPropertyEditor(
        property, parent, shouldShowNumericSlider(property));
  case ArtifactCore::PropertyType::Boolean:
    return new ArtifactBoolPropertyEditor(property, parent);
  case ArtifactCore::PropertyType::Color:
    return new ArtifactColorPropertyEditor(property, parent);
  case ArtifactCore::PropertyType::String:
    return new ArtifactStringPropertyEditor(property, parent);
  case ArtifactCore::PropertyType::ObjectReference:
    return new ArtifactObjectReferencePropertyEditor(property, parent);
  default:
    return nullptr;
  }
}

// ObjectReferencePropertyEditor 実装
ArtifactObjectReferencePropertyEditor::ArtifactObjectReferencePropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  Q_UNUSED(property);

  referenceWidget_ = new QWidget(this);
  auto *layout = new QHBoxLayout(referenceWidget_);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  auto *caption = new QLabel(QStringLiteral("Object Reference"), referenceWidget_);
  valueLabel_ = new QLabel(referenceWidget_);
  pickButton_ = new PropertyCallbackButton(QStringLiteral("Pick"), referenceWidget_);
  clearButton_ = new PropertyCallbackButton(QStringLiteral("Clear"), referenceWidget_);

  layout->addWidget(caption, 0);
  layout->addWidget(valueLabel_, 1);
  layout->addWidget(pickButton_, 0);
  layout->addWidget(clearButton_, 0);

  auto *outer = new QHBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addWidget(referenceWidget_);

  static_cast<PropertyCallbackButton *>(pickButton_)->setCallback(
      [this]() { onReferencePicked(); });
  static_cast<PropertyCallbackButton *>(clearButton_)->setCallback(
      [this]() { onReferenceChanged(-1); });

  clearButton_->setEnabled(false);
  updateReferenceDisplay();
}

QVariant ArtifactObjectReferencePropertyEditor::value() const {
  return QVariant::fromValue<qint64>(currentId_);
}

void ArtifactObjectReferencePropertyEditor::setValueFromVariant(
    const QVariant &value) {
  if (value.canConvert<qint64>()) {
    currentId_ = value.toLongLong();
  } else {
    currentId_ = -1;
  }
  updateReferenceDisplay();
}

void ArtifactObjectReferencePropertyEditor::onReferencePicked() {
  ArtifactObjectPickerDialog dialog(this);
  dialog.setCurrentSelectionId(currentId_ < 0
                                   ? ArtifactCore::Id::Nil()
                                   : ArtifactCore::Id(QString::number(currentId_)));
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const QString selectedIdText = dialog.selectedId().toString();
  bool ok = false;
  const qint64 selectedId = selectedIdText.toLongLong(&ok);
  if (!ok) {
    onReferenceChanged(static_cast<qint64>(qHash(selectedIdText)));
    return;
  }

  onReferenceChanged(selectedId);
}

void ArtifactObjectReferencePropertyEditor::onReferenceChanged(qint64 newId) {
  currentId_ = newId;
  updateReferenceDisplay();
  commitValue(value());
}

void ArtifactObjectReferencePropertyEditor::updateReferenceDisplay() {
  if (!valueLabel_) {
    return;
  }

  if (currentId_ < 0) {
    valueLabel_->setText(QStringLiteral("None"));
    if (clearButton_) {
      clearButton_->setEnabled(false);
    }
    return;
  }

  QString displayText = QString::number(currentId_);
  auto *service = ArtifactProjectService::instance();
  if (service) {
    auto composition = service->currentComposition().lock();
    if (composition) {
      const auto layer = composition->layerById(
          ArtifactCore::LayerID(ArtifactCore::Id(QString::number(currentId_))));
      if (layer) {
        displayText = QStringLiteral("%1 (%2)")
                          .arg(layer->layerName(), QString::number(currentId_));
      }
    }
  }

  valueLabel_->setText(displayText);
  if (clearButton_) {
    clearButton_->setEnabled(true);
  }
}

} // namespace Artifact



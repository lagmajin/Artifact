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
#include <QSvgRenderer>
#include <QAbstractButton>
#include <QComboBox>
#include <QHash>
#include <QLabel>
#include <QSlider>
#include <QStyleOptionSlider>

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
import Property.Abstract;

namespace Artifact {
namespace detail {

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
    const QColor activeFill = palette().color(QPalette::Highlight);
    const QColor trackFill =
        (isSliderDown() || hasFocus())
            ? activeFill
            : trackBase.lighter(112);
    const QColor handleFill = palette().color(QPalette::Button);
    const QColor handleBorder = palette().color(QPalette::Mid);
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

} // namespace detail
} // namespace Artifact

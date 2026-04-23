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
#include <QStyleOptionSlider>
#include <QTextEdit>
#include <QtSVG/QSvgRenderer>

module Artifact.Widgets.PropertyEditor;

import std;
import Utils.Path;
import Color.Float;
import Font.FreeFont;
import Artifact.Widgets.FontPicker;
import Artifact.Event.Types;
import Artifact.Widgets.ExpressionCopilotWidget;
import Artifact.Service.Playback;
import Artifact.Service.Project;
import Time.Rational;
import FloatColorPickerDialog;
import Artifact.Widgets.Dialog.FloatColorPickerHooks;
import Widgets.Utils.CSS;

namespace Artifact {

namespace {
constexpr int kPropertyRowMinHeight = 32;
constexpr int kPropertyRowLabelMinHeight = 24;
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

protected:
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

    painter.fillRect(rect(), surface);

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    const QRect grooveRect =
        style()->subControlRect(QStyle::CC_Slider, &opt,
                                QStyle::SC_SliderGroove, this);
    const QRect handleRect =
        style()->subControlRect(QStyle::CC_Slider, &opt,
                                QStyle::SC_SliderHandle, this);

    const int trackThickness = 4;
    QRect trackRect = grooveRect;
    trackRect.setHeight(trackThickness);
    trackRect.moveCenter(QPoint(trackRect.center().x(), grooveRect.center().y()));
    trackRect = trackRect.adjusted(0, 0, -1, -1);

    painter.setPen(Qt::NoPen);
    painter.setBrush(trackBase);
    painter.drawRoundedRect(trackRect, 2.0, 2.0);

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
    painter.drawRoundedRect(fillRect.intersected(trackRect), 2.0, 2.0);
    painter.setPen(QPen(trackBorder, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(trackRect, 2.0, 2.0);

    painter.setPen(QPen(handleBorder, 1.0));
    painter.setBrush(handleFill);
    painter.drawRoundedRect(handleRect.adjusted(0, 0, -1, -1), 3.0, 3.0);
  }
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
  return blendColor(background, surface, elevated ? 0.72 : 0.58);
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
  if (name == QStringLiteral("orientation")) {
    return ArtifactEnumPropertyEditor::OptionList{
        {0, QStringLiteral("Horizontal")}, {1, QStringLiteral("Vertical")}};
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

QIcon cachedKeyframeIcon(const QSize &size = QSize(14, 14),
                         const QColor &fillColor = QColor(QStringLiteral("#FFD84D")),
                         const QColor &outlineColor = QColor(QStringLiteral("#FFF1A8"))) {
  static QHash<QString, QIcon> iconCache;
  const QString cacheKey = QStringLiteral("%1x%2:%3:%4")
                               .arg(size.width())
                               .arg(size.height())
                               .arg(fillColor.rgba(), 8, 16, QLatin1Char('0'))
                               .arg(outlineColor.rgba(), 8, 16, QLatin1Char('0'));
  auto it = iconCache.constFind(cacheKey);
  if (it != iconCache.constEnd()) {
    return it.value();
  }

  const int width = qMax(1, size.width());
  const int height = qMax(1, size.height());
  QPixmap pixmap(width, height);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(outlineColor, 1.2));
  painter.setBrush(fillColor);
  painter.translate(width * 0.5, height * 0.5);
  painter.rotate(45.0);
  const QRectF square(-width * 0.24, -height * 0.24, width * 0.48, height * 0.48);
  painter.drawRect(square);
  painter.end();

  QIcon icon(pixmap);
  iconCache.insert(cacheKey, icon);
  return icon;
}

// --- Relative Input Support ---

class ArtifactRelativeDoubleSpinBox : public QDoubleSpinBox {
public:
  using QDoubleSpinBox::QDoubleSpinBox;

  QValidator::State validate(QString &input, int &pos) const override {
    if (input.isEmpty())
      return QValidator::Intermediate;
    const QChar first = input.at(0);
    if (first == '+' || first == '-' || first == '*' || first == '/') {
      // Allow relative operators at the start
      QString rest = input.mid(1);
      if (rest.isEmpty())
        return QValidator::Intermediate;
      return QValidator::Acceptable;
    }
    return QDoubleSpinBox::validate(input, pos);
  }

  double valueFromText(const QString &text) const override {
    if (text.isEmpty())
      return value();
    const QChar first = text.at(0);
    if (first == '+' || first == '-' || first == '*' || first == '/') {
      bool ok = false;
      double delta = text.mid(1).toDouble(&ok);
      if (!ok)
        return value();
      if (first == '+')
        return value() + delta;
      if (first == '-')
        return value() - delta;
      if (first == '*')
        return value() * delta;
      if (first == '/')
        return (std::abs(delta) > 1e-9) ? value() / delta : value();
    }
    return QDoubleSpinBox::valueFromText(text);
  }
};

class ArtifactRelativeSpinBox : public QSpinBox {
public:
  using QSpinBox::QSpinBox;

  QValidator::State validate(QString &input, int &pos) const override {
    if (input.isEmpty())
      return QValidator::Intermediate;
    const QChar first = input.at(0);
    if (first == '+' || first == '-' || first == '*' || first == '/') {
      QString rest = input.mid(1);
      if (rest.isEmpty())
        return QValidator::Intermediate;
      return QValidator::Acceptable;
    }
    return QSpinBox::validate(input, pos);
  }

  int valueFromText(const QString &text) const override {
    if (text.isEmpty())
      return value();
    const QChar first = text.at(0);
    if (first == '+' || first == '-' || first == '*' || first == '/') {
      bool ok = false;
      double delta = text.mid(1).toDouble(&ok);
      if (!ok)
        return value();
      if (first == '+')
        return value() + static_cast<int>(delta);
      if (first == '-')
        return value() - static_cast<int>(delta);
      if (first == '*')
        return static_cast<int>(value() * delta);
      if (first == '/')
        return (std::abs(delta) > 1e-9) ? static_cast<int>(value() / delta)
                                        : value();
    }
    return QSpinBox::valueFromText(text);
  }
};

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

    QColor trackColor = isChecked() ? blendColor(selection, accent, 0.44)
                                    : propertySurfaceColor(false);
    QColor borderColor =
        isChecked() ? blendColor(border, accent, 0.52) : border;
    QColor knobColor = text;
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
      QPen focusPen(selection.lighter(130), 1.0);
      focusPen.setStyle(Qt::DashLine);
      painter.setPen(focusPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRoundedRect(trackRect.adjusted(1, 1, -1, -1), radius, radius);
    }
  }
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
                       if (spinBox_) {
                         spinBox_->setValue(defaultValue.toDouble());
                       }
                       if (slider_) {
                         const QSignalBlocker blocker(slider_);
                         slider_->setValue(floatToSliderPosition(
                             defaultValue.toDouble(), softMin_, softMax_));
                       }
                       commitCurrentValue();
                     });
  }

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  if (slider_) {
    if (g_numericEditorLayoutMode ==
        ArtifactNumericEditorLayoutMode::SliderThenValue) {
      layout->addWidget(slider_, 3);
      layout->addWidget(spinBox_, 1);
    } else {
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
  spinBox_->setMinimumHeight(22);
  spinBox_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spinBox_->setFrame(false);

  if (slider_) {
    slider_->setRange(0, 10000); // 精度を向上
    slider_->setMinimumHeight(16);
    slider_->setTracking(true); // ドラッグ中の追従を有効化
    slider_->setValue(floatToSliderPosition(property.getValue().toDouble(),
                                            softMin_, softMax_));
  }

  QObject::connect(spinBox_, &QDoubleSpinBox::valueChanged, this,
                   [this](const double nextValue) {
                     if (slider_) {
                       const QSignalBlocker blocker(slider_);
                       slider_->setValue(
                           floatToSliderPosition(nextValue, softMin_, softMax_));
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
        slider_, &QSlider::valueChanged, this, [this](const int sliderValue) {
          const double nextValue =
              this->sliderPositionToFloat(sliderValue, softMin_, softMax_);
          const QSignalBlocker blocker(spinBox_);
          spinBox_->setValue(nextValue);
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
  if (slider_ && watched == slider_ && event->type() == QEvent::MouseButtonPress) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      double ratio = static_cast<double>(mouseEvent->pos().x()) /
                     static_cast<double>(std::max(1, slider_->width()));
      int newValue = static_cast<int>(std::clamp(ratio, 0.0, 1.0) * 10000.0);
      slider_->setValue(newValue);
      previewCurrentValue();
      commitCurrentValue();
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
  }
}

bool ArtifactFloatPropertyEditor::supportsScrub() const { return true; }

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
  if (slider_) {
    if (g_numericEditorLayoutMode ==
        ArtifactNumericEditorLayoutMode::SliderThenValue) {
      layout->addWidget(slider_, 3);
      layout->addWidget(spinBox_, 1);
    } else {
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
  spinBox_->setMinimumHeight(22);
  spinBox_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spinBox_->setFrame(false);

  if (slider_) {
    slider_->setRange(0, 10000); // 精度を向上
    slider_->setMinimumHeight(16);
    slider_->setTracking(true);
    slider_->setValue(
        intToSliderPosition(property.getValue().toInt(), softMin_, softMax_));
  }

  QObject::connect(
      spinBox_, &QSpinBox::valueChanged, this, [this](const int nextValue) {
        if (slider_) {
          const QSignalBlocker blocker(slider_);
          slider_->setValue(intToSliderPosition(nextValue, softMin_, softMax_));
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
                     [this](const int sliderValue) {
                       const int nextValue =
                           sliderPositionToInt(sliderValue, softMin_, softMax_);
                       const QSignalBlocker blocker(spinBox_);
                       spinBox_->setValue(nextValue);
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
  }
}

bool ArtifactIntPropertyEditor::supportsScrub() const { return true; }

bool ArtifactIntPropertyEditor::eventFilter(QObject *watched, QEvent *event) {
  if (slider_ && watched == slider_ && event->type() == QEvent::MouseButtonPress) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      const double ratio = static_cast<double>(mouseEvent->pos().x()) /
                           static_cast<double>(std::max(1, slider_->width()));
      const int newValue =
          static_cast<int>(std::clamp(ratio, 0.0, 1.0) * 10000.0);
      slider_->setValue(newValue);
      previewCurrentValue();
      commitCurrentValue();
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
  comboBox_ = new QComboBox(this);
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
      scrubHandle_(new QLabel(QStringLiteral("::"), this)), editor_(editor),
      keyframeButton_(new QPushButton(this)),
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
  label_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
  label_->setAutoFillBackground(false);
  scrubHandle_->setToolTip(
      QStringLiteral("Drag to scrub. Shift=fine, Ctrl=coarse, Esc=cancel."));
  scrubHandle_->setObjectName(QStringLiteral("propertyScrubHandle"));
  scrubHandle_->setAlignment(Qt::AlignCenter);
  scrubHandle_->setFixedWidth(16);
  scrubHandle_->setAutoFillBackground(false);
  applyPropertyLabelPalette(label_);
  applyPropertyLabelPalette(scrubHandle_);

  supplementaryLabel_ = new QLabel(this);
  supplementaryLabel_->setObjectName(
      QStringLiteral("propertySupplementaryLabel"));
  supplementaryLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  supplementaryLabel_->setAutoFillBackground(false);
  supplementaryLabel_->setVisible(false);
  supplementaryLabel_->setSizePolicy(QSizePolicy::Minimum,
                                     QSizePolicy::Fixed);
  applyPropertyLabelPalette(supplementaryLabel_, 135);

  editor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  applyPropertyFieldPalette(editor_);

  // Load Icons
  QIcon prevIcon =
      loadPropertyIcon(QStringLiteral("MaterialVS/neutral/arrow_left.svg"));
  QIcon nextIcon =
      loadPropertyIcon(QStringLiteral("MaterialVS/neutral/arrow_right.svg"));
  QIcon resIcon =
      loadPropertyIcon(QStringLiteral("MaterialVS/neutral/undo.svg"));
  QIcon exprIcon = loadPropertyIcon(QStringLiteral("MaterialVS/blue/code.svg"));

  // Aux button setup — all buttons share a fixed-width container so that
  // showing/hiding buttons on hover does NOT change the row's total width.
  prevKeyBtn_->setFixedSize(kPropertyNavButtonWidth, kPropertyNavButtonHeight);
  nextKeyBtn_->setFixedSize(kPropertyNavButtonWidth, kPropertyNavButtonHeight);
  prevKeyBtn_->setIcon(prevIcon);
  nextKeyBtn_->setIcon(nextIcon);
  prevKeyBtn_->setIconSize(QSize(10, 10));
  nextKeyBtn_->setIconSize(QSize(10, 10));
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

  // Fixed-width container that always reserves the same horizontal space.
  auto *auxContainer = new QWidget(this);
  auxContainer->setFixedWidth(kAuxButtonAreaWidth);
  auxContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  auxContainer->setAutoFillBackground(false);
  auto *auxLayout = new QHBoxLayout(auxContainer);
  auxLayout->setContentsMargins(0, 0, 0, 0);
  auxLayout->setSpacing(kPropertyActionSpacing);
  auxLayout->addWidget(prevKeyBtn_);
  auxLayout->addWidget(keyframeButton_);
  auxLayout->addWidget(nextKeyBtn_);
  auxLayout->addWidget(resetButton_);
  auxLayout->addWidget(expressionButton_);

  scrubHandle_->installEventFilter(this);
  editor_->installEventFilter(this);
  label_->setCursor(Qt::ArrowCursor);
  scrubHandle_->setVisible(editor_->supportsScrub());
  scrubHandle_->setCursor(editor_->supportsScrub() ? Qt::SizeHorCursor
                                                   : Qt::ArrowCursor);
  editor_->setCursor(editor_->supportsScrub() ? Qt::SizeHorCursor
                                              : Qt::ArrowCursor);

  if (g_propertyRowLayoutMode ==
      ArtifactPropertyRowLayoutMode::EditorThenLabel) {
    layout->addWidget(editor_, 1);
    layout->addWidget(label_, 0);
    layout->addWidget(scrubHandle_);
  } else {
    layout->addWidget(label_, 0);
    layout->addWidget(scrubHandle_);
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

void ArtifactPropertyEditorRowWidget::setNavigationHandler(
    NavigationHandler handler) {
  navigationHandler_ = std::move(handler);
}

void ArtifactPropertyEditorRowWidget::setEditorToolTip(const QString &tooltip) {
  label_->setToolTip(tooltip);
  scrubHandle_->setToolTip(
      tooltip +
      QStringLiteral("\nDrag to scrub. Shift=fine, Ctrl=coarse, Esc=cancel."));
  editor_->setToolTip(tooltip);
  if (supplementaryLabel_) {
    supplementaryLabel_->setToolTip(tooltip);
  }
  keyframeButton_->setToolTip(tooltip);
  prevKeyBtn_->setToolTip(tooltip);
  nextKeyBtn_->setToolTip(tooltip);
  resetButton_->setToolTip(tooltip);
  expressionButton_->setToolTip(tooltip);
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

void ArtifactPropertyEditorRowWidget::updateKeyframeButtonIcon() {
  if (!keyframeButton_) {
    return;
  }
  const bool checked = keyframeButton_->isChecked();
  const QColor fillColor = checked ? QColor(QStringLiteral("#FFD84D"))
                                   : QColor(QStringLiteral("#6E7681"));
  const QColor outlineColor = checked ? QColor(QStringLiteral("#FFF1A8"))
                                      : QColor(QStringLiteral("#B6C0CD"));
  keyframeButton_->setIcon(
      cachedKeyframeIcon(QSize(14, 14), fillColor, outlineColor));
}

void ArtifactPropertyEditorRowWidget::setKeyframeChecked(const bool checked) {
  const QSignalBlocker blocker(keyframeButton_);
  keyframeButton_->setChecked(checked);
  updateKeyframeButtonIcon();
  update();
}

void ArtifactPropertyEditorRowWidget::setKeyframeEnabled(const bool enabled) {
  keyframeButton_->setEnabled(enabled);
  update();
}

void ArtifactPropertyEditorRowWidget::setNavigationEnabled(const bool enabled) {
  prevKeyBtn_->setProperty("baseVisible", enabled);
  nextKeyBtn_->setProperty("baseVisible", enabled);
  prevKeyBtn_->setEnabled(enabled);
  nextKeyBtn_->setEnabled(enabled);
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

  resetButton_->setVisible(resetVisible);
  expressionButton_->setVisible(exprVisible);
  prevKeyBtn_->setVisible(keyVisible && navVisible);
  nextKeyBtn_->setVisible(keyVisible && navVisible);

  resetButton_->setEnabled(resetVisible && hover);
  expressionButton_->setEnabled(exprVisible && hover);
  prevKeyBtn_->setEnabled(keyVisible && navVisible && hover);
  nextKeyBtn_->setEnabled(keyVisible && navVisible && hover);
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
  const bool keyframed = keyframeButton_ && keyframeButton_->isChecked();

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
  painter.fillPath(path, fill);

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
    if (watched == scrubHandle_) {
      scrubbing_ = true;
      scrubStarted_ = false;
      scrubStartX_ = mouseEvent->globalPosition().toPoint().x();
      scrubStartValue_ = editor_->value();
      scrubHandle_->grabMouse();
      grabKeyboard();
      setFocus(Qt::MouseFocusReason);
      scrubHandle_->setCursor(Qt::SizeHorCursor);
      return true;
    }
    if (watched == editor_) {
      editorScrubbing_ = true;
      editorScrubStarted_ = false;
      editorScrubStartX_ = mouseEvent->globalPosition().toPoint().x();
      editorScrubStartValue_ = editor_->value();
      setFocus(Qt::MouseFocusReason);
      editor_->setCursor(Qt::SizeHorCursor);
    }
    break;
  }
  case QEvent::MouseMove: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (watched == scrubHandle_) {
      if (!scrubbing_) {
        break;
      }
      const int currentX = mouseEvent->globalPosition().toPoint().x();
      const int deltaPixels = currentX - scrubStartX_;
      if (!scrubStarted_ && std::abs(deltaPixels) < scrubThreshold_) {
        return true;
      }
      scrubStarted_ = true;
      editor_->setValueFromVariant(scrubStartValue_);
      editor_->scrubByPixels(deltaPixels, mouseEvent->modifiers());
      editor_->previewCurrentValue();
      return true;
    }
    if (watched == editor_) {
      if (!editorScrubbing_) {
        break;
      }
      const int currentX = mouseEvent->globalPosition().toPoint().x();
      const int deltaPixels = currentX - editorScrubStartX_;
      if (!editorScrubStarted_ && std::abs(deltaPixels) < editorScrubThreshold_) {
        return true;
      }
      editorScrubStarted_ = true;
      editor_->setValueFromVariant(editorScrubStartValue_);
      editor_->scrubByPixels(deltaPixels, mouseEvent->modifiers());
      editor_->previewCurrentValue();
      return true;
    }
  }
  case QEvent::MouseButtonRelease: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (watched == scrubHandle_ && scrubbing_ && mouseEvent->button() == Qt::LeftButton) {
      finishScrub(scrubStarted_);
      return true;
    }
    if (watched == editor_ && editorScrubbing_ && mouseEvent->button() == Qt::LeftButton) {
      const bool commitChanges = editorScrubStarted_;
      editorScrubbing_ = false;
      editorScrubStarted_ = false;
      editor_->setCursor(editor_->supportsScrub() ? Qt::SizeHorCursor
                                                  : Qt::ArrowCursor);
      if (commitChanges) {
        editor_->commitCurrentValue();
      } else {
        editor_->setValueFromVariant(editorScrubStartValue_);
        editor_->previewValueFromVariant(editorScrubStartValue_);
      }
      return true;
    }
    break;
  }
  default:
    break;
  }

  return QWidget::eventFilter(watched, event);
}

void ArtifactPropertyEditorRowWidget::keyPressEvent(QKeyEvent *event) {
  if ((scrubbing_ || editorScrubbing_) && event->key() == Qt::Key_Escape) {
    finishScrub(false);
    editorScrubbing_ = false;
    editorScrubStarted_ = false;
    if (editor_) {
      editor_->setCursor(editor_->supportsScrub() ? Qt::SizeHorCursor
                                                  : Qt::ArrowCursor);
    }
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

void ArtifactPropertyEditorRowWidget::finishScrub(const bool commitChanges) {
  if (!scrubbing_) {
    return;
  }
  scrubbing_ = false;
  scrubHandle_->releaseMouse();
  releaseKeyboard();
  scrubHandle_->setCursor(editor_ && editor_->supportsScrub()
                              ? Qt::SizeHorCursor
                              : Qt::ArrowCursor);

  if (!editor_) {
    scrubStarted_ = false;
    return;
  }

  if (commitChanges) {
    editor_->commitCurrentValue();
  } else {
    editor_->setValueFromVariant(scrubStartValue_);
    editor_->previewValueFromVariant(scrubStartValue_);
  }
  scrubStarted_ = false;
}

ArtifactAbstractPropertyEditor *
createPropertyEditorWidget(const ArtifactCore::AbstractProperty &property,
                           QWidget *parent) {
  if (isMultilineTextProperty(property)) {
    return new ArtifactMultilineStringPropertyEditor(property, parent);
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

  switch (property.getType()) {
  case ArtifactCore::PropertyType::Float:
    return new ArtifactFloatPropertyEditor(
        property, parent,
        !property.getName().startsWith(QStringLiteral("transform.position")));
  case ArtifactCore::PropertyType::Integer:
    return new ArtifactIntPropertyEditor(
        property, parent,
        !property.getName().startsWith(QStringLiteral("transform.position")));
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

  // TODO: ObjectReferenceWidget を使用する実装
  // 現在は簡易的なラベル表示のみ
  auto *label = new QLabel(QStringLiteral("(Object Reference)"), this);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(label);
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
}

void ArtifactObjectReferencePropertyEditor::onReferencePicked() {
  // TODO: ObjectPickerDialog を表示
}

void ArtifactObjectReferencePropertyEditor::onReferenceChanged(qint64 newId) {
  currentId_ = newId;
  commitValue(value());
}

} // namespace Artifact

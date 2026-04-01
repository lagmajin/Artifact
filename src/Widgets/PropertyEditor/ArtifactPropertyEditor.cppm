module;

#include <QAbstractButton>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QCursor>
#include <QDoubleSpinBox>
#include <QEnterEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QPainter>
#include <QIcon>
#include <QtSVG/QSvgRenderer>
#include <QSlider>
#include <QSpinBox>
#include <QTextEdit>

module Artifact.Widgets.PropertyEditor;

import std;
import Utils.Path;
import Color.Float;
import Font.FreeFont;
import Artifact.Widgets.FontPicker;
import Artifact.Widgets.ExpressionCopilotWidget;
import Artifact.Service.Playback;
import Artifact.Service.Project;
import Time.Rational;
import FloatColorPickerDialog;

namespace Artifact {

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

void ArtifactAbstractPropertyEditor::scrubByPixels(int deltaPixels,
                                                   Qt::KeyboardModifiers modifiers) {
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
  return static_cast<int>(std::lround(
      static_cast<double>(minValue) +
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

// --- Icon Loading Helpers ---

QIcon loadSvgAsIcon(const QString& path, int size = 16)
{
    if (path.isEmpty()) return QIcon();
    if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
        QSvgRenderer renderer(path);
        if (renderer.isValid()) {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            renderer.render(&painter);
            painter.end();
            if (!pixmap.isNull()) return QIcon(pixmap);
        }
        return QIcon();
    }
    return QIcon(path);
}

QIcon loadPropertyIcon(const QString& resourceRelativePath, const QString& fallbackFileName = {})
{
    using namespace ArtifactCore;
    static QHash<QString, QIcon> iconCache;
    const QString cacheKey = resourceRelativePath + QStringLiteral("|") + fallbackFileName;
    auto it = iconCache.constFind(cacheKey);
    if (it != iconCache.constEnd()) return it.value();
    QIcon icon = loadSvgAsIcon(resolveIconResourcePath(resourceRelativePath));
    if (!icon.isNull()) { iconCache.insert(cacheKey, icon); return icon; }
    if (!fallbackFileName.isEmpty()) {
        icon = loadSvgAsIcon(resolveIconPath(fallbackFileName));
    }
    iconCache.insert(cacheKey, icon);
    return icon;
}

// --- Relative Input Support ---

class ArtifactRelativeDoubleSpinBox : public QDoubleSpinBox {
public:
    using QDoubleSpinBox::QDoubleSpinBox;

    QValidator::State validate(QString& input, int& pos) const override {
        if (input.isEmpty()) return QValidator::Intermediate;
        const QChar first = input.at(0);
        if (first == '+' || first == '-' || first == '*' || first == '/') {
            // Allow relative operators at the start
            QString rest = input.mid(1);
            if (rest.isEmpty()) return QValidator::Intermediate;
            return QValidator::Acceptable;
        }
        return QDoubleSpinBox::validate(input, pos);
    }

    double valueFromText(const QString& text) const override {
        if (text.isEmpty()) return value();
        const QChar first = text.at(0);
        if (first == '+' || first == '-' || first == '*' || first == '/') {
            bool ok = false;
            double delta = text.mid(1).toDouble(&ok);
            if (!ok) return value();
            if (first == '+') return value() + delta;
            if (first == '-') return value() - delta;
            if (first == '*') return value() * delta;
            if (first == '/') return (std::abs(delta) > 1e-9) ? value() / delta : value();
        }
        return QDoubleSpinBox::valueFromText(text);
    }
};

class ArtifactRelativeSpinBox : public QSpinBox {
public:
    using QSpinBox::QSpinBox;

    QValidator::State validate(QString& input, int& pos) const override {
        if (input.isEmpty()) return QValidator::Intermediate;
        const QChar first = input.at(0);
        if (first == '+' || first == '-' || first == '*' || first == '/') {
            QString rest = input.mid(1);
            if (rest.isEmpty()) return QValidator::Intermediate;
            return QValidator::Acceptable;
        }
        return QSpinBox::validate(input, pos);
    }

    int valueFromText(const QString& text) const override {
        if (text.isEmpty()) return value();
        const QChar first = text.at(0);
        if (first == '+' || first == '-' || first == '*' || first == '/') {
            bool ok = false;
            double delta = text.mid(1).toDouble(&ok);
            if (!ok) return value();
            if (first == '+') return value() + static_cast<int>(delta);
            if (first == '-') return value() - static_cast<int>(delta);
            if (first == '*') return static_cast<int>(value() * delta);
            if (first == '/') return (std::abs(delta) > 1e-9) ? static_cast<int>(value() / delta) : value();
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

        QColor trackColor = isChecked() ? QColor(QStringLiteral("#4CAF50"))
                                        : QColor(QStringLiteral("#4B4B4B"));
        QColor borderColor = isChecked() ? QColor(QStringLiteral("#69D26B"))
                                         : QColor(QStringLiteral("#5B5B5B"));
        QColor knobColor = QColor(QStringLiteral("#F5F5F5"));
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
            QPen focusPen(palette().highlight().color().lighter(130), 1.0);
            focusPen.setStyle(Qt::DashLine);
            painter.setPen(focusPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(trackRect.adjusted(1, 1, -1, -1), radius,
                                    radius);
        }
    }
};

} // namespace

ArtifactFloatPropertyEditor::ArtifactFloatPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyFloatEditor"));
  spinBox_ = new ArtifactRelativeDoubleSpinBox(this);
  slider_ = new QSlider(Qt::Horizontal, this);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  if (g_numericEditorLayoutMode ==
      ArtifactNumericEditorLayoutMode::SliderThenValue) {
    layout->addWidget(slider_, 3);
    layout->addWidget(spinBox_, 1);
  } else {
    layout->addWidget(spinBox_, 1);
    layout->addWidget(slider_, 3);
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

  const QString numericStyle = R"(
    QDoubleSpinBox, QSpinBox {
        background: transparent;
        border: none;
        color: #E8E8E8;
        font-family: 'Segoe UI', sans-serif;
        font-weight: 600;
        font-size: 11px;
        padding-right: 2px;
    }
    QDoubleSpinBox:hover, QSpinBox:hover {
        background: rgba(245, 147, 60, 0.08);
        border-radius: 2px;
    }
    QDoubleSpinBox:focus, QSpinBox:focus {
        background: #232323;
        border: 1px solid #F5933C;
        color: #FFF;
    }
  )";

  spinBox_->setRange(meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1e6,
                     meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1e6);
  spinBox_->setValue(property.getValue().toDouble());
  spinBox_->setStyleSheet(numericStyle);
  if (meta.step.isValid()) {
    spinBox_->setSingleStep(meta.step.toDouble());
  }
  if (!meta.unit.isEmpty()) {
    spinBox_->setSuffix(QStringLiteral(" ") + meta.unit);
  }
  spinBox_->setMinimumHeight(22);
  spinBox_->setButtonSymbols(QAbstractSpinBox::NoButtons);

  slider_->setRange(0, 10000); // 精度を向上
  slider_->setMinimumHeight(16);
  slider_->setTracking(true); // ドラッグ中の追従を有効化
  slider_->setValue(floatToSliderPosition(property.getValue().toDouble(),
                                          softMin_, softMax_));
  slider_->setStyleSheet(R"(
      QSlider::groove:horizontal {
          background: #333;
          height: 2px;
          border-radius: 1px;
      }
      QSlider::handle:horizontal {
          background: #F5933C;
          width: 8px;
          height: 8px;
          margin: -3px 0;
          border-radius: 4px;
      }
  )");

  QObject::connect(spinBox_, &QDoubleSpinBox::valueChanged, this,
                   [this](const double nextValue) {
                     const QSignalBlocker blocker(slider_);
                     slider_->setValue(
                         floatToSliderPosition(nextValue, softMin_, softMax_));
                     if (spinBox_->hasFocus() && !sliderInteracting_) {
                       previewValue(nextValue);
                     }
                   });
  QObject::connect(spinBox_, &QDoubleSpinBox::editingFinished, this,
                   [this]() { commitValue(spinBox_->value()); });

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

  // クリックでジャンプする挙動を追加
  slider_->installEventFilter(this);
}

bool ArtifactFloatPropertyEditor::eventFilter(QObject *watched, QEvent *event) {
  if (watched == slider_ && event->type() == QEvent::MouseButtonPress) {
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
  if (!spinBox_ || !slider_) {
    return;
  }
  const double nextValue = value.toDouble();
  {
    const QSignalBlocker spinBlocker(spinBox_);
    spinBox_->setValue(nextValue);
  }
  const QSignalBlocker sliderBlocker(slider_);
  slider_->setValue(this->floatToSliderPosition(nextValue, softMin_, softMax_));
}

bool ArtifactFloatPropertyEditor::supportsScrub() const { return true; }

void ArtifactFloatPropertyEditor::scrubByPixels(const int deltaPixels,
                                                const Qt::KeyboardModifiers modifiers) {
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
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyIntEditor"));
  spinBox_ = new ArtifactRelativeSpinBox(this);
  slider_ = new QSlider(Qt::Horizontal, this);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);
  if (g_numericEditorLayoutMode ==
      ArtifactNumericEditorLayoutMode::SliderThenValue) {
    layout->addWidget(slider_, 3);
    layout->addWidget(spinBox_, 1);
  } else {
    layout->addWidget(spinBox_, 1);
    layout->addWidget(slider_, 3);
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

  const QString numericStyle = R"(
    QSpinBox {
        background: transparent;
        border: none;
        color: #E8E8E8;
        font-family: 'Segoe UI', sans-serif;
        font-weight: 600;
        font-size: 11px;
        padding-right: 2px;
    }
    QSpinBox:hover {
        background: rgba(245, 147, 60, 0.08);
        border-radius: 2px;
    }
    QSpinBox:focus {
        background: #232323;
        border: 1px solid #F5933C;
        color: #FFF;
    }
  )";

  spinBox_->setRange(meta.hardMin.isValid() ? meta.hardMin.toInt() : -1000000,
                     meta.hardMax.isValid() ? meta.hardMax.toInt() : 1000000);
  spinBox_->setValue(property.getValue().toInt());
  spinBox_->setStyleSheet(numericStyle);
  if (meta.step.isValid()) {
    spinBox_->setSingleStep(meta.step.toInt());
  }
  if (!meta.unit.isEmpty()) {
    spinBox_->setSuffix(QStringLiteral(" ") + meta.unit);
  }
  spinBox_->setMinimumHeight(22);
  spinBox_->setButtonSymbols(QAbstractSpinBox::NoButtons);

  slider_->setRange(0, 10000); // 精度を向上
  slider_->setMinimumHeight(16);
  slider_->setTracking(true);
  slider_->setValue(
      intToSliderPosition(property.getValue().toInt(), softMin_, softMax_));
  slider_->setStyleSheet(R"(
      QSlider::groove:horizontal {
          background: #333;
          height: 2px;
          border-radius: 1px;
      }
      QSlider::handle:horizontal {
          background: #F5933C;
          width: 8px;
          height: 8px;
          margin: -3px 0;
          border-radius: 4px;
      }
  )");

  QObject::connect(
      spinBox_, &QSpinBox::valueChanged, this, [this](const int nextValue) {
        const QSignalBlocker blocker(slider_);
        slider_->setValue(intToSliderPosition(nextValue, softMin_, softMax_));
        if (spinBox_->hasFocus() && !sliderInteracting_) {
          previewValue(nextValue);
        }
      });
  QObject::connect(spinBox_, &QSpinBox::editingFinished, this,
                   [this]() { commitValue(spinBox_->value()); });
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
  slider_->installEventFilter(this);
}

QVariant ArtifactIntPropertyEditor::value() const {
  return spinBox_ ? QVariant(spinBox_->value()) : QVariant();
}

void ArtifactIntPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!spinBox_ || !slider_) {
    return;
  }
  const int nextValue = value.toInt();
  {
    const QSignalBlocker spinBlocker(spinBox_);
    spinBox_->setValue(nextValue);
  }
  const QSignalBlocker sliderBlocker(slider_);
  slider_->setValue(intToSliderPosition(nextValue, softMin_, softMax_));
}

bool ArtifactIntPropertyEditor::supportsScrub() const { return true; }

bool ArtifactIntPropertyEditor::eventFilter(QObject *watched, QEvent *event) {
  if (watched == slider_ && event->type() == QEvent::MouseButtonPress) {
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

void ArtifactIntPropertyEditor::scrubByPixels(const int deltaPixels,
                                              const Qt::KeyboardModifiers modifiers) {
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
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(fontPicker_);

  setValueFromVariant(property.getValue());
  QObject::connect(
      fontPicker_, &FontPickerWidget::fontChanged, this,
      [this](const QString &family) {
        commitValue(ArtifactCore::FontManager::resolvedFamily(family));
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
  browseButton_->setObjectName(QStringLiteral("propertyPathBrowseButton"));
  browseButton_->setFixedSize(28, 26);

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
  currentColor_ = propertyColor(property);
  applyColor(currentColor_);
  QObject::connect(button_, &QPushButton::clicked, this, [this]() {
    ArtifactWidgets::FloatColorPicker picker(button_);
    picker.setWindowTitle(QStringLiteral("Select Color"));
    picker.setInitialColor(ArtifactCore::FloatColor(currentColor_.redF(),
                                            currentColor_.greenF(),
                                            currentColor_.blueF(),
                                            currentColor_.alphaF()));
    if (picker.exec() != QDialog::Accepted) {
      return;
    }
    const ArtifactCore::FloatColor picked = picker.getColor();
    const QColor nextColor = QColor::fromRgbF(picked.r(), picked.g(),
                                              picked.b(), picked.a());
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
  }
  if (nextColor.isValid()) {
    applyColor(nextColor);
  }
}

void ArtifactColorPropertyEditor::applyColor(const QColor &color) {
  currentColor_ = color;
  if (button_) {
    button_->setStyleSheet(QStringLiteral("background-color: %1; border: 1px "
                                          "solid #454545; border-radius: 4px;")
                               .arg(color.name()));
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
      prevKeyBtn_(new QPushButton(this)),
      nextKeyBtn_(new QPushButton(this)) {
  setObjectName(QStringLiteral("propertyRow"));
  setFocusPolicy(Qt::StrongFocus);
  setStyleSheet(R"(
      QWidget#propertyRow {
          background: transparent;
          border-bottom: 1px solid rgba(255, 255, 255, 0.05);
      }
      QWidget#propertyRow:hover {
          background: rgba(255, 255, 255, 0.02);
      }
      QLabel#propertyRowLabel {
          color: #AAA;
          font-size: 10px;
          font-weight: 500;
      }
      QLabel#propertyScrubHandle {
          color: #444;
          font-size: 10px;
      }
  )");

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 4, 8, 4);
  layout->setSpacing(6);
  scrubHandle_->setToolTip(
      QStringLiteral("Drag to scrub. Shift=fine, Ctrl=coarse, Esc=cancel."));

  editor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  // Load Icons
  QIcon keyIcon = loadPropertyIcon(QStringLiteral("MaterialVS/yellow/keyframe.svg"));
  QIcon prevIcon = loadPropertyIcon(QStringLiteral("MaterialVS/neutral/arrow_left.svg"));
  QIcon nextIcon = loadPropertyIcon(QStringLiteral("MaterialVS/neutral/arrow_right.svg"));
  QIcon resIcon = loadPropertyIcon(QStringLiteral("MaterialVS/neutral/undo.svg"));
  QIcon exprIcon = loadPropertyIcon(QStringLiteral("MaterialVS/blue/code.svg"));

  // Keyframe Controls
  auto *keyframeControlLayout = new QHBoxLayout();
  keyframeControlLayout->setSpacing(0);

  const QString navStyle = R"(
        QPushButton {
            background: transparent;
            border: none;
            padding: 0;
            border-radius: 2px;
        }
        QPushButton:hover {
            background: rgba(255, 255, 255, 0.1);
        }
        QPushButton:pressed {
            background: rgba(255, 255, 255, 0.05);
        }
        QPushButton:disabled {
            opacity: 0.3;
        }
    )";

  prevKeyBtn_->setFixedSize(14, 22);
  nextKeyBtn_->setFixedSize(14, 22);
  prevKeyBtn_->setIcon(prevIcon);
  nextKeyBtn_->setIcon(nextIcon);
  prevKeyBtn_->setIconSize(QSize(10, 10));
  nextKeyBtn_->setIconSize(QSize(10, 10));
  prevKeyBtn_->setStyleSheet(navStyle);
  nextKeyBtn_->setStyleSheet(navStyle);
  prevKeyBtn_->setVisible(false);
  nextKeyBtn_->setVisible(false);

  keyframeButton_->setObjectName(QStringLiteral("propertyKeyButton"));
  keyframeButton_->setToolTip(
      QStringLiteral("Toggle Keyframe: %1").arg(propertyName));
  keyframeButton_->setFixedSize(22, 22);
  keyframeButton_->setCheckable(true);
  keyframeButton_->setIcon(keyIcon);
  keyframeButton_->setIconSize(QSize(14, 14));
  keyframeButton_->setStyleSheet(R"(
        QPushButton#propertyKeyButton {
            background: transparent;
            border: none;
            border-radius: 2px;
        }
        QPushButton#propertyKeyButton:hover {
            background: rgba(255, 255, 255, 0.1);
        }
        QPushButton#propertyKeyButton:checked {
            background: rgba(212, 125, 50, 0.2);
            border: 1px solid rgba(212, 125, 50, 0.5);
        }
        QPushButton#propertyKeyButton:disabled {
            opacity: 0.3;
        }
    )");

  keyframeControlLayout->addWidget(prevKeyBtn_);
  keyframeControlLayout->addWidget(keyframeButton_);
  keyframeControlLayout->addWidget(nextKeyBtn_);

  resetButton_->setObjectName(QStringLiteral("propertyResetButton"));
  resetButton_->setToolTip(QStringLiteral("Reset: %1").arg(propertyName));
  resetButton_->setFixedSize(24, 24);
  resetButton_->setIcon(resIcon);
  resetButton_->setIconSize(QSize(14, 14));
  resetButton_->setStyleSheet(R"(
        QPushButton#propertyResetButton {
            background: transparent;
            border: none;
        }
        QPushButton#propertyResetButton:hover {
            background: #4A4A4A;
            border-radius: 3px;
        }
    )");
  resetButton_->setVisible(false);

  expressionButton_->setObjectName(QStringLiteral("propertyExprButton"));
  expressionButton_->setToolTip(
      QStringLiteral("Expression: %1").arg(propertyName));
  expressionButton_->setFixedSize(26, 24);
  expressionButton_->setIcon(exprIcon);
  expressionButton_->setIconSize(QSize(14, 14));
  expressionButton_->setStyleSheet(R"(
        QPushButton#propertyExprButton {
            background: transparent;
            border: none;
        }
        QPushButton#propertyExprButton:hover {
            background: #4A4A4A;
            border-radius: 3px;
        }
    )");
  expressionButton_->setVisible(false);

  scrubHandle_->installEventFilter(this);
  label_->setCursor(Qt::ArrowCursor);
  scrubHandle_->setVisible(editor_->supportsScrub());
  scrubHandle_->setCursor(editor_->supportsScrub() ? Qt::SizeHorCursor
                                                   : Qt::ArrowCursor);

  if (g_propertyRowLayoutMode ==
      ArtifactPropertyRowLayoutMode::EditorThenLabel) {
    layout->addWidget(editor_, 1);
    layout->addWidget(label_);
    layout->addWidget(scrubHandle_);
  } else {
    layout->addWidget(label_);
    layout->addWidget(scrubHandle_);
    layout->addWidget(editor_, 1);
  }
  layout->addLayout(keyframeControlLayout);
  layout->addWidget(resetButton_);
  layout->addWidget(expressionButton_);

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

void setGlobalNumericEditorLayoutMode(const ArtifactNumericEditorLayoutMode mode) {
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
  scrubHandle_->setToolTip(tooltip + QStringLiteral("\nDrag to scrub. Shift=fine, Ctrl=coarse, Esc=cancel."));
  editor_->setToolTip(tooltip);
  keyframeButton_->setToolTip(tooltip);
  prevKeyBtn_->setToolTip(tooltip);
  nextKeyBtn_->setToolTip(tooltip);
  resetButton_->setToolTip(tooltip);
  expressionButton_->setToolTip(tooltip);
}

void ArtifactPropertyEditorRowWidget::setShowExpressionButton(
    const bool visible) {
  expressionButton_->setProperty("baseVisible", visible);
  updateAuxControlVisibility();
}

void ArtifactPropertyEditorRowWidget::setShowResetButton(const bool visible) {
  resetButton_->setProperty("baseVisible", visible);
  updateAuxControlVisibility();
}

void ArtifactPropertyEditorRowWidget::setShowKeyframeButton(
    const bool visible) {
  keyframeButton_->setVisible(visible);
  updateAuxControlVisibility();
}

void ArtifactPropertyEditorRowWidget::setKeyframeChecked(const bool checked) {
  const QSignalBlocker blocker(keyframeButton_);
  keyframeButton_->setChecked(checked);
}

void ArtifactPropertyEditorRowWidget::setKeyframeEnabled(const bool enabled) {
  keyframeButton_->setEnabled(enabled);
}

void ArtifactPropertyEditorRowWidget::setNavigationEnabled(const bool enabled) {
  prevKeyBtn_->setEnabled(enabled);
  nextKeyBtn_->setEnabled(enabled);
  updateAuxControlVisibility();
}

void ArtifactPropertyEditorRowWidget::updateAuxControlVisibility() {
  const bool hover = underMouse();
  const bool keyVisible = keyframeButton_->isVisible();
  const bool resetVisible = resetButton_->property("baseVisible").toBool();
  const bool exprVisible = expressionButton_->property("baseVisible").toBool();
  const bool navVisible = prevKeyBtn_->isEnabled() && nextKeyBtn_->isEnabled();

  resetButton_->setVisible(resetVisible && hover);
  expressionButton_->setVisible(exprVisible && hover);
  prevKeyBtn_->setVisible(keyVisible && navVisible && hover);
  nextKeyBtn_->setVisible(keyVisible && navVisible && hover);
}

void ArtifactPropertyEditorRowWidget::enterEvent(QEnterEvent *event) {
  QWidget::enterEvent(event);
  updateAuxControlVisibility();
}

void ArtifactPropertyEditorRowWidget::leaveEvent(QEvent *event) {
  QWidget::leaveEvent(event);
  updateAuxControlVisibility();
}

bool ArtifactPropertyEditorRowWidget::eventFilter(QObject *watched,
                                                  QEvent *event) {
  if (watched != scrubHandle_ || !editor_ || !editor_->supportsScrub()) {
    return QWidget::eventFilter(watched, event);
  }

  switch (event->type()) {
  case QEvent::MouseButtonPress: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() != Qt::LeftButton) {
      break;
    }
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
  case QEvent::MouseMove: {
    if (!scrubbing_) {
      break;
    }
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
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
  case QEvent::MouseButtonRelease: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (scrubbing_ && mouseEvent->button() == Qt::LeftButton) {
      finishScrub(scrubStarted_);
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
  if (scrubbing_ && event->key() == Qt::Key_Escape) {
    finishScrub(false);
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
    return new ArtifactFloatPropertyEditor(property, parent);
  case ArtifactCore::PropertyType::Integer:
    return new ArtifactIntPropertyEditor(property, parent);
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

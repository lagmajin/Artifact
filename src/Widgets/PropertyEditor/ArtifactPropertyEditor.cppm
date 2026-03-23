module;

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>

module Artifact.Widgets.PropertyEditor;

import std;
import Font.FreeFont;

namespace Artifact {

ArtifactAbstractPropertyEditor::ArtifactAbstractPropertyEditor(QWidget *parent)
    : QWidget(parent) {}

ArtifactAbstractPropertyEditor::~ArtifactAbstractPropertyEditor() = default;

void ArtifactAbstractPropertyEditor::setCommitHandler(CommitHandler handler) {
  commitHandler_ = std::move(handler);
}

void ArtifactAbstractPropertyEditor::commitValue(const QVariant &value) const {
  if (commitHandler_) {
    commitHandler_(value);
  }
}

bool ArtifactAbstractPropertyEditor::supportsScrub() const { return false; }

void ArtifactAbstractPropertyEditor::scrubByPixels(int deltaPixels,
                                                   bool fineAdjust) {
  Q_UNUSED(deltaPixels);
  Q_UNUSED(fineAdjust);
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

  spinBox_->setRange(meta.hardMin.isValid() ? meta.hardMin.toDouble() : -1e6,
                     meta.hardMax.isValid() ? meta.hardMax.toDouble() : 1e6);
  spinBox_->setValue(property.getValue().toDouble());
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

  QObject::connect(spinBox_, &QDoubleSpinBox::valueChanged, this,
                   [this](const double nextValue) {
                     const QSignalBlocker blocker(slider_);
                     slider_->setValue(
                         floatToSliderPosition(nextValue, softMin_, softMax_));
                   });
  QObject::connect(spinBox_, &QDoubleSpinBox::editingFinished, this,
                   [this]() { commitValue(spinBox_->value()); });

  // valueChanged ではなく sliderMoved と actionTriggered を組み合わせるか、
  // あるいは単純に高精度化した valueChanged を使用する
  QObject::connect(
      slider_, &QSlider::valueChanged, this, [this](const int sliderValue) {
        const double nextValue =
            this->sliderPositionToFloat(sliderValue, softMin_, softMax_);
        const QSignalBlocker blocker(spinBox_);
        spinBox_->setValue(nextValue);
        commitValue(nextValue);
      });

  // クリックでジャンプする挙動を追加
  slider_->installEventFilter(this);
}

bool ArtifactFloatPropertyEditor::eventFilter(QObject *watched, QEvent *event) {
  if (watched == slider_ && event->type() == QEvent::MouseButtonPress) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      // クリックした位置にジャンプ
      double ratio = static_cast<double>(mouseEvent->pos().x()) /
                     static_cast<double>(slider_->width());
      int newValue = static_cast<int>(std::clamp(ratio, 0.0, 1.0) * 10000.0);
      slider_->setValue(newValue);
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
                                                const bool fineAdjust) {
  if (!spinBox_) {
    return;
  }

  // DCCツール風の感度計算:
  // softMax - softMin の範囲の 1/500 を 1ピクセルあたりの基本移動量とする
  double range = std::abs(softMax_ - softMin_);
  if (range < 1e-5)
    range = 100.0; // フォールバック

  double sensitivity = range / 500.0;
  if (fineAdjust) {
    sensitivity *= 0.1; // Ctrlキー等での微調整
  }

  const double nextValue =
      spinBox_->value() + static_cast<double>(deltaPixels) * sensitivity;
  spinBox_->setValue(nextValue);
  commitValue(spinBox_->value());
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

  spinBox_->setRange(meta.hardMin.isValid() ? meta.hardMin.toInt() : -1000000,
                     meta.hardMax.isValid() ? meta.hardMax.toInt() : 1000000);
  spinBox_->setValue(property.getValue().toInt());
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
  slider_->setValue(
      std::clamp(property.getValue().toInt(), softMin_, softMax_));

  QObject::connect(
      spinBox_, &QSpinBox::valueChanged, this, [this](const int nextValue) {
        const QSignalBlocker blocker(slider_);
        slider_->setValue(std::clamp(nextValue, softMin_, softMax_));
      });
  QObject::connect(spinBox_, &QSpinBox::editingFinished, this,
                   [this]() { commitValue(spinBox_->value()); });
  QObject::connect(slider_, &QSlider::valueChanged, this,
                   [this](const int sliderValue) {
                     const QSignalBlocker blocker(spinBox_);
                     spinBox_->setValue(sliderValue);
                     commitValue(sliderValue);
                   });
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
  slider_->setValue(std::clamp(nextValue, softMin_, softMax_));
}

bool ArtifactIntPropertyEditor::supportsScrub() const { return true; }

void ArtifactIntPropertyEditor::scrubByPixels(const int deltaPixels,
                                              const bool fineAdjust) {
  if (!spinBox_) {
    return;
  }
  const int baseStep = std::max(1, spinBox_->singleStep());
  const double scaledStep =
      static_cast<double>(baseStep) * (fineAdjust ? 0.25 : 1.0);
  const int nextValue = static_cast<int>(
      std::llround(static_cast<double>(spinBox_->value()) +
                   static_cast<double>(deltaPixels) * scaledStep));
  spinBox_->setValue(nextValue);
  commitValue(spinBox_->value());
}

ArtifactBoolPropertyEditor::ArtifactBoolPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyBoolEditor"));
  checkBox_ = new QCheckBox(this);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(checkBox_);
  layout->addStretch();

  checkBox_->setChecked(property.getValue().toBool());
  QObject::connect(checkBox_, &QCheckBox::toggled, this,
                   [this](const bool checked) { commitValue(checked); });
}

QVariant ArtifactBoolPropertyEditor::value() const {
  return checkBox_ ? QVariant(checkBox_->isChecked()) : QVariant();
}

void ArtifactBoolPropertyEditor::setValueFromVariant(const QVariant &value) {
  if (!checkBox_) {
    return;
  }
  const QSignalBlocker blocker(checkBox_);
  checkBox_->setChecked(value.toBool());
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

ArtifactFontFamilyPropertyEditor::ArtifactFontFamilyPropertyEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyFontEditor"));
  comboBox_ = new QFontComboBox(this);
  comboBox_->setMinimumHeight(26);
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(comboBox_);

  setValueFromVariant(property.getValue());
  QObject::connect(
      comboBox_, &QFontComboBox::currentFontChanged, this,
      [this](const QFont &font) {
        commitValue(ArtifactCore::FontManager::resolvedFamily(font.family()));
      });
}

QVariant ArtifactFontFamilyPropertyEditor::value() const {
  return comboBox_ ? QVariant(comboBox_->currentFont().family()) : QVariant();
}

void ArtifactFontFamilyPropertyEditor::setValueFromVariant(
    const QVariant &value) {
  if (!comboBox_) {
    return;
  }
  const QString family =
      ArtifactCore::FontManager::resolvedFamily(value.toString());
  const QSignalBlocker blocker(comboBox_);
  comboBox_->setCurrentFont(QFont(family));
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
    QColorDialog dialog(button_);
    dialog.setStyleSheet("");
    dialog.setCurrentColor(currentColor_);
    dialog.setWindowTitle(QStringLiteral("Select Color"));
    if (dialog.exec() != QDialog::Accepted) {
      return;
    }
    const QColor nextColor = dialog.currentColor();
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
    : QWidget(parent), label_(new QLabel(labelText, this)), editor_(editor),
      keyframeButton_(new QPushButton(QStringLiteral("K"), this)),
      resetButton_(new QPushButton(QStringLiteral("R"), this)),
      expressionButton_(new QPushButton(QStringLiteral("fx"), this)),
      prevKeyBtn_(new QPushButton(QStringLiteral("<"), this)),
      nextKeyBtn_(new QPushButton(QStringLiteral(">"), this)) {
  setObjectName(QStringLiteral("propertyRow"));
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(4, 2, 4, 2);
  layout->setSpacing(4);

  label_->setObjectName(QStringLiteral("propertyRowLabel"));
  label_->setMinimumWidth(120);
  label_->setMaximumWidth(160);
  label_->setMinimumHeight(24);

  editor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  // Keyframe Controls
  auto *keyframeControlLayout = new QHBoxLayout();
  keyframeControlLayout->setSpacing(0);

  const QString navStyle = R"(
        QPushButton {
            background: #404040;
            border: 1px solid #454545;
            color: #E0E0E0;
            font-size: 10px;
            padding: 0;
            border-radius: 3px;
        }
        QPushButton:hover {
            color: #FFFFFF;
            border-color: #606060;
            background: #4A4A4A;
        }
        QPushButton:disabled {
            color: #606060;
            border-color: #353535;
            background: #353535;
        }
    )";

  prevKeyBtn_->setFixedSize(12, 24);
  nextKeyBtn_->setFixedSize(12, 24);
  prevKeyBtn_->setStyleSheet(navStyle);
  nextKeyBtn_->setStyleSheet(navStyle);

  keyframeButton_->setObjectName(QStringLiteral("propertyKeyButton"));
  keyframeButton_->setToolTip(
      QStringLiteral("Toggle Keyframe: %1").arg(propertyName));
  keyframeButton_->setFixedSize(22, 24);
  keyframeButton_->setCheckable(true);
  keyframeButton_->setStyleSheet(R"(
        QPushButton#propertyKeyButton {
            background: #404040;
            border: 1px solid #454545;
            color: #E0E0E0;
            font-size: 11px;
            font-weight: 700;
            border-radius: 3px;
        }
        QPushButton#propertyKeyButton:hover {
            color: #FFFFFF;
            border-color: #606060;
            background: #4A4A4A;
        }
        QPushButton#propertyKeyButton:checked {
            color: #FFFFFF;
            background: #F5933C;
            border-color: #F5933C;
        }
        QPushButton#propertyKeyButton:disabled {
            color: #606060;
            border-color: #353535;
            background: #353535;
        }
    )");

  keyframeControlLayout->addWidget(prevKeyBtn_);
  keyframeControlLayout->addWidget(keyframeButton_);
  keyframeControlLayout->addWidget(nextKeyBtn_);

  resetButton_->setObjectName(QStringLiteral("propertyResetButton"));
  resetButton_->setToolTip(QStringLiteral("Reset: %1").arg(propertyName));
  resetButton_->setFixedSize(24, 24);
  resetButton_->setStyleSheet(R"(
        QPushButton#propertyResetButton {
            background: transparent;
            border: none;
            color: #888888;
            font-size: 16px;
        }
        QPushButton#propertyResetButton:hover {
            color: #BBBBBB;
        }
    )");

  expressionButton_->setObjectName(QStringLiteral("propertyExprButton"));
  expressionButton_->setToolTip(
      QStringLiteral("Expression: %1").arg(propertyName));
  expressionButton_->setFixedSize(26, 24);
  expressionButton_->setStyleSheet(R"(
        QPushButton#propertyExprButton {
            background: transparent;
            border: none;
            color: #888888;
            font-family: 'Consolas', monospace;
            font-size: 13px;
        }
        QPushButton#propertyExprButton:hover {
            color: #BBBBBB;
        }
    )");

  label_->installEventFilter(this);
  label_->setCursor(editor_->supportsScrub() ? Qt::SizeHorCursor
                                             : Qt::ArrowCursor);

  if (g_propertyRowLayoutMode ==
      ArtifactPropertyRowLayoutMode::EditorThenLabel) {
    layout->addWidget(editor_, 1);
    layout->addWidget(label_);
  } else {
    layout->addWidget(label_);
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
  editor_->setToolTip(tooltip);
  keyframeButton_->setToolTip(tooltip);
  prevKeyBtn_->setToolTip(tooltip);
  nextKeyBtn_->setToolTip(tooltip);
  resetButton_->setToolTip(tooltip);
  expressionButton_->setToolTip(tooltip);
}

void ArtifactPropertyEditorRowWidget::setShowExpressionButton(
    const bool visible) {
  expressionButton_->setVisible(visible);
}

void ArtifactPropertyEditorRowWidget::setShowResetButton(const bool visible) {
  resetButton_->setVisible(visible);
}

void ArtifactPropertyEditorRowWidget::setShowKeyframeButton(
    const bool visible) {
  keyframeButton_->setVisible(visible);
  prevKeyBtn_->setVisible(visible);
  nextKeyBtn_->setVisible(visible);
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
}

bool ArtifactPropertyEditorRowWidget::eventFilter(QObject *watched,
                                                  QEvent *event) {
  if (watched != label_ || !editor_ || !editor_->supportsScrub()) {
    return QWidget::eventFilter(watched, event);
  }

  switch (event->type()) {
  case QEvent::MouseButtonPress: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() != Qt::LeftButton) {
      break;
    }
    scrubbing_ = true;
    scrubStartX_ = mouseEvent->globalPosition().toPoint().x();
    scrubStartValue_ = editor_->value();
    label_->grabMouse();
    label_->setCursor(Qt::SizeHorCursor);
    return true;
  }
  case QEvent::MouseMove: {
    if (!scrubbing_) {
      break;
    }
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    const int currentX = mouseEvent->globalPosition().toPoint().x();
    const int deltaPixels = currentX - scrubStartX_;
    editor_->setValueFromVariant(scrubStartValue_);
    editor_->scrubByPixels(deltaPixels,
                           mouseEvent->modifiers().testFlag(Qt::ShiftModifier));
    return true;
  }
  case QEvent::MouseButtonRelease: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (scrubbing_ && mouseEvent->button() == Qt::LeftButton) {
      scrubbing_ = false;
      label_->releaseMouse();
      label_->setCursor(Qt::SizeHorCursor);
      return true;
    }
    break;
  }
  default:
    break;
  }

  return QWidget::eventFilter(watched, event);
}

ArtifactAbstractPropertyEditor *
createPropertyEditorWidget(const ArtifactCore::AbstractProperty &property,
                           QWidget *parent) {
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

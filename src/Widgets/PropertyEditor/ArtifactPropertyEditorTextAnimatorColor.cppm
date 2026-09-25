module;
#include <QColor>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineF>
#include <QLinearGradient>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLayoutItem>
#include <QSignalBlocker>
#include <QStringList>
#include <QVariant>
#include <QTextEdit>
#include <QWidget>
#include <QVBoxLayout>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

module Artifact.Widgets.PropertyEditor;

import Property.Abstract;
import Color.Float;
import Artifact.Widgets.RelativeSpinBox;
import FloatColorPickerDialog;
import Artifact.Widgets.Dialog.FloatColorPickerHooks;
import Utils.Path;

namespace Artifact {
using namespace detail;

namespace {

class GradientActionButton final : public QPushButton {
public:
  using Callback = std::function<void()>;

  explicit GradientActionButton(const QString& text, QWidget* parent = nullptr)
      : QPushButton(text, parent) {}

  void setCallback(Callback callback) { callback_ = std::move(callback); }

protected:
  void nextCheckState() override {
    if (callback_) {
      callback_();
    }
  }

private:
  Callback callback_;
};

struct GradientStopValue {
  double offset = 0.5;
  ArtifactCore::FloatColor color = ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f);
};

QJsonArray gradientStopsFromJson(const QString& text) {
  if (text.size() > 16384 || text.trimmed().isEmpty()) {
    return {};
  }
  const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8());
  if (!document.isArray()) {
    return {};
  }
  QJsonArray result;
  for (const QJsonValue& value : document.array()) {
    if (!value.isObject() || result.size() >= 32) {
      continue;
    }
    const QJsonObject object = value.toObject();
    const double offset = object.value(QStringLiteral("o")).toDouble(-1.0);
    if (!std::isfinite(offset) || offset < 0.0 || offset > 1.0) {
      continue;
    }
    const double r = object.value(QStringLiteral("r")).toDouble(0.0);
    const double g = object.value(QStringLiteral("g")).toDouble(0.0);
    const double b = object.value(QStringLiteral("b")).toDouble(0.0);
    const double a = object.value(QStringLiteral("a")).toDouble(1.0);
    QJsonObject normalized;
    normalized.insert(QStringLiteral("o"), offset);
    normalized.insert(QStringLiteral("r"), std::clamp(r, 0.0, 1.0));
    normalized.insert(QStringLiteral("g"), std::clamp(g, 0.0, 1.0));
    normalized.insert(QStringLiteral("b"), std::clamp(b, 0.0, 1.0));
    normalized.insert(QStringLiteral("a"), std::clamp(a, 0.0, 1.0));
    result.push_back(normalized);
  }
  return result;
}

QString gradientStopsToJson(const QJsonArray& stops) {
  return QString::fromUtf8(QJsonDocument(stops).toJson(QJsonDocument::Compact));
}

QJsonObject gradientStopObject(const GradientStopValue& stop) {
  QJsonObject object;
  object.insert(QStringLiteral("o"), std::clamp(stop.offset, 0.0, 1.0));
  object.insert(QStringLiteral("r"), std::clamp<double>(stop.color.r(), 0.0, 1.0));
  object.insert(QStringLiteral("g"), std::clamp<double>(stop.color.g(), 0.0, 1.0));
  object.insert(QStringLiteral("b"), std::clamp<double>(stop.color.b(), 0.0, 1.0));
  object.insert(QStringLiteral("a"), std::clamp<double>(stop.color.a(), 0.0, 1.0));
  return object;
}

GradientStopValue gradientStopFromObject(const QJsonObject& object) {
  GradientStopValue stop;
  stop.offset = std::clamp(object.value(QStringLiteral("o")).toDouble(0.5), 0.0, 1.0);
  stop.color = ArtifactCore::FloatColor(
      static_cast<float>(std::clamp(object.value(QStringLiteral("r")).toDouble(0.0), 0.0, 1.0)),
      static_cast<float>(std::clamp(object.value(QStringLiteral("g")).toDouble(0.0), 0.0, 1.0)),
      static_cast<float>(std::clamp(object.value(QStringLiteral("b")).toDouble(0.0), 0.0, 1.0)),
      static_cast<float>(std::clamp(object.value(QStringLiteral("a")).toDouble(1.0), 0.0, 1.0)));
  return stop;
}

bool editGradientStop(QWidget* parent, QJsonObject& stopObject) {
  GradientStopValue stop = gradientStopFromObject(stopObject);
  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("Edit Gradient Stop"));
  auto* layout = new QVBoxLayout(&dialog);
  auto* offsetLabel = new QLabel(QStringLiteral("Position"), &dialog);
  auto* offset = new QDoubleSpinBox(&dialog);
  offset->setRange(0.0, 1.0);
  offset->setDecimals(3);
  offset->setSingleStep(0.01);
  offset->setValue(stop.offset);
  layout->addWidget(offsetLabel);
  layout->addWidget(offset);

  auto* colorButton = new GradientActionButton(QStringLiteral("Choose Color…"), &dialog);
  colorButton->setAccessibleName(QStringLiteral("Gradient stop color"));
  colorButton->setAccessibleDescription(QStringLiteral("Choose the color for this gradient stop"));
  colorButton->setText(QStringLiteral("Color: %1, %2, %3, %4")
      .arg(stop.color.r(), 0, 'f', 2)
      .arg(stop.color.g(), 0, 'f', 2)
      .arg(stop.color.b(), 0, 'f', 2)
      .arg(stop.color.a(), 0, 'f', 2));
  applyPropertyButtonPalette(colorButton, true);
  layout->addWidget(colorButton);
  ArtifactCore::FloatColor selectedColor = stop.color;
  colorButton->setCallback([&dialog, colorButton, &selectedColor]() {
    ArtifactWidgets::FloatColorPicker picker(&dialog);
    picker.setInitialColor(selectedColor);
    configureFloatColorPicker(&picker, ColorSelectionPurpose::Edit);
    if (picker.exec() == QDialog::Accepted) {
      selectedColor = picker.getColor();
      colorButton->setText(QStringLiteral("Color: %1, %2, %3")
          .arg(selectedColor.r(), 0, 'f', 2)
          .arg(selectedColor.g(), 0, 'f', 2)
          .arg(selectedColor.b(), 0, 'f', 2));
    }
  });

  auto* actions = new QHBoxLayout();
  auto* accept = new GradientActionButton(QStringLiteral("OK"), &dialog);
  auto* cancel = new GradientActionButton(QStringLiteral("Cancel"), &dialog);
  accept->setDefault(true);
  applyPropertyButtonPalette(accept, true);
  applyPropertyButtonPalette(cancel);
  accept->setCallback([&dialog]() { dialog.accept(); });
  cancel->setCallback([&dialog]() { dialog.reject(); });
  actions->addStretch(1);
  actions->addWidget(cancel);
  actions->addWidget(accept);
  layout->addLayout(actions);

  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }
  stop.offset = offset->value();
  stop.color = selectedColor;
  stopObject = gradientStopObject(stop);
  return true;
}

bool editGradientStops(QWidget* parent, QJsonArray& stops) {
  QDialog dialog(parent);
  dialog.setWindowTitle(QStringLiteral("Gradient Stops"));
  dialog.setMinimumWidth(320);
  auto* layout = new QVBoxLayout(&dialog);
  auto* rows = new QVBoxLayout();
  rows->setSpacing(4);
  layout->addLayout(rows);
  auto* add = new GradientActionButton(QStringLiteral("Add Stop"), &dialog);
  auto* actions = new QHBoxLayout();
  auto* done = new GradientActionButton(QStringLiteral("Done"), &dialog);
  applyPropertyButtonPalette(add, true);
  applyPropertyButtonPalette(done, true);
  actions->addWidget(add);
  actions->addStretch(1);
  actions->addWidget(done);
  layout->addLayout(actions);

  auto rebuildRows = [&]() {
    while (QLayoutItem* item = rows->takeAt(0)) {
      if (QWidget* widget = item->widget()) {
        widget->hide();
        widget->deleteLater();
      }
      delete item;
    }
    for (qsizetype i = 0; i < stops.size(); ++i) {
      const QJsonObject stop = stops.at(i).toObject();
      auto* row = new QWidget(&dialog);
      auto* rowLayout = new QHBoxLayout(row);
      rowLayout->setContentsMargins(0, 0, 0, 0);
      auto* label = new QLabel(QStringLiteral("%1  ·  %2")
          .arg(i + 1).arg(stop.value(QStringLiteral("o")).toDouble(), 0, 'f', 3), row);
      auto* edit = new GradientActionButton(QStringLiteral("Edit"), row);
      auto* remove = new GradientActionButton(QStringLiteral("Remove"), row);
      applyPropertyButtonPalette(edit);
      applyPropertyButtonPalette(remove);
      edit->setCallback([&, i]() {
        QJsonObject changed = stops.at(i).toObject();
        if (editGradientStop(&dialog, changed)) {
          stops.replace(i, changed);
          rebuildRows();
        }
      });
      remove->setCallback([&, i]() {
        stops.removeAt(i);
        rebuildRows();
      });
      rowLayout->addWidget(label, 1);
      rowLayout->addWidget(edit);
      rowLayout->addWidget(remove);
      rows->addWidget(row);
    }
    add->setEnabled(stops.size() < 32);
  };
  add->setCallback([&]() {
    if (stops.size() >= 32) {
      return;
    }
    GradientStopValue stop;
    if (stops.isEmpty()) {
      stop.offset = 0.5;
    } else {
      const QJsonObject previous = stops.last().toObject();
      const double lastOffset = previous.value(QStringLiteral("o")).toDouble(0.0);
      stop.offset = std::clamp(lastOffset + (1.0 - lastOffset) * 0.5, 0.0, 1.0);
      stop.color = gradientStopFromObject(previous).color;
    }
    stops.push_back(gradientStopObject(stop));
    rebuildRows();
  });
  done->setCallback([&dialog]() { dialog.accept(); });
  rebuildRows();
  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }
  return true;
}

class ArtifactGradientStopsPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
  explicit ArtifactGradientStopsPropertyEditor(
      const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr)
      : ArtifactAbstractPropertyEditor(parent), value_(gradientStopsToJson(gradientStopsFromJson(property.getValue().toString()))) {
    setObjectName(QStringLiteral("propertyGradientStopsEditor"));
    setAccessibleName(QStringLiteral("Gradient stops editor"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    summary_ = new QLabel(this);
    auto* button = new GradientActionButton(QStringLiteral("Edit Stops…"), this);
    button->setAccessibleName(QStringLiteral("Edit gradient stops"));
    applyPropertyFieldPalette(summary_);
    applyPropertyButtonPalette(button, true);
    layout->addWidget(summary_, 1);
    layout->addWidget(button);
    button->setCallback([this]() {
      QJsonArray stops = gradientStopsFromJson(value_);
      if (!editGradientStops(this, stops)) {
        return;
      }
      value_ = gradientStopsToJson(stops);
      updateSummary();
      commitValue(value_);
    });
    updateSummary();
  }

  QVariant value() const override { return value_; }
  void setValueFromVariant(const QVariant& value) override {
    value_ = gradientStopsToJson(gradientStopsFromJson(value.toString()));
    updateSummary();
  }

private:
  void updateSummary() {
    const int count = gradientStopsFromJson(value_).size();
    summary_->setText(count == 0 ? QStringLiteral("2-color default")
                                 : QStringLiteral("%1 stops").arg(count));
  }

  QString value_;
  QLabel* summary_ = nullptr;
};

} // namespace

ArtifactTextAnimatorColorEditor::ArtifactTextAnimatorColorEditor(
    const ArtifactCore::AbstractProperty &property, QWidget *parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyTextAnimatorColorEditor"));
  setAccessibleName(QStringLiteral("Text animator color editor"));
  setAccessibleDescription(QStringLiteral("Edit text animator content and apply color to a selected text range"));

  textEdit_ = new QTextEdit(this);
  textEdit_->setAccessibleName(QStringLiteral("Text animator content"));
  textEdit_->setAccessibleDescription(QStringLiteral("Enter text and select a range to apply an animator color"));
  textEdit_->setAcceptRichText(false);
  textEdit_->setMinimumHeight(72);
  textEdit_->setTabChangesFocus(true);
  textEdit_->setLineWrapMode(QTextEdit::WidgetWidth);
  textEdit_->setFrameStyle(QFrame::NoFrame);
  applyPropertyFieldPalette(textEdit_, true);

  colorButton_ = new QPushButton(QStringLiteral(" "), this);
  colorButton_->setAccessibleName(QStringLiteral("Apply color to selected text range"));
  colorButton_->setAccessibleDescription(QStringLiteral("Open the color picker for the selected text range"));
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

namespace {
QRectF professionalControlRect(const QWidget* widget) {
  return QRectF(widget->rect()).adjusted(8.0, 8.0, -8.0, -18.0);
}

QColor controlGridColor() {
  return detail::themeColor(QStringLiteral("borderMuted"), QColor(78, 82, 90));
}

QColor controlAccentColor() {
  return detail::themeColor(QStringLiteral("accent"), QColor(66, 145, 245));
}
}  // namespace

ArtifactCurvesPropertyEditor::ArtifactCurvesPropertyEditor(
    const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyCurvesEditor"));
  setAccessibleName(QStringLiteral("Tone curve"));
  setAccessibleDescription(
      QStringLiteral("Edit the master tone curve; double-click to add or remove a point"));
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setMinimumHeight(132);
  setValueFromVariant(property.getValue());
}

QVariant ArtifactCurvesPropertyEditor::value() const {
  return serializedPoints();
}

void ArtifactCurvesPropertyEditor::setValueFromVariant(const QVariant& value) {
  std::vector<Point> parsed;
  const auto entries = value.toString().split(QLatin1Char(';'), Qt::SkipEmptyParts);
  for (const QString& entry : entries) {
    const auto coordinates = entry.split(QLatin1Char(':'));
    if (coordinates.size() != 2) continue;
    bool xOk = false;
    bool yOk = false;
    const double x = QLocale::c().toDouble(coordinates[0], &xOk);
    const double y = QLocale::c().toDouble(coordinates[1], &yOk);
    if (xOk && yOk && std::isfinite(x) && std::isfinite(y)) {
      parsed.push_back({std::clamp(x, 0.0, 1.0), std::clamp(y, 0.0, 1.0)});
    }
  }
  points_ = std::move(parsed);
  normalizePoints();
  update();
}

QSize ArtifactCurvesPropertyEditor::sizeHint() const { return QSize(220, 142); }
QSize ArtifactCurvesPropertyEditor::minimumSizeHint() const { return QSize(150, 118); }

QString ArtifactCurvesPropertyEditor::serializedPoints() const {
  QStringList entries;
  entries.reserve(static_cast<qsizetype>(points_.size()));
  for (const Point& point : points_) {
    entries.push_back(QStringLiteral("%1:%2")
                          .arg(point.x, 0, 'g', 9)
                          .arg(point.y, 0, 'g', 9));
  }
  return entries.join(QLatin1Char(';'));
}

void ArtifactCurvesPropertyEditor::normalizePoints() {
  if (points_.size() < 2) points_ = {{0.0, 0.0}, {1.0, 1.0}};
  std::sort(points_.begin(), points_.end(),
            [](const Point& a, const Point& b) { return a.x < b.x; });
  points_.front().x = 0.0;
  points_.back().x = 1.0;
  if (points_.size() > 16) points_.resize(16);
}

QPointF ArtifactCurvesPropertyEditor::widgetPosition(const Point& point) const {
  const QRectF graph = professionalControlRect(this);
  return {graph.left() + point.x * graph.width(),
          graph.bottom() - point.y * graph.height()};
}

ArtifactCurvesPropertyEditor::Point
ArtifactCurvesPropertyEditor::curvePosition(const QPointF& position) const {
  const QRectF graph = professionalControlRect(this);
  return {std::clamp((position.x() - graph.left()) / graph.width(), 0.0, 1.0),
          std::clamp((graph.bottom() - position.y()) / graph.height(), 0.0, 1.0)};
}

int ArtifactCurvesPropertyEditor::pointAt(const QPointF& position) const {
  int nearest = -1;
  double distance = 9.0;
  for (int i = 0; i < static_cast<int>(points_.size()); ++i) {
    const double candidate = QLineF(position, widgetPosition(points_[i])).length();
    if (candidate < distance) {
      distance = candidate;
      nearest = i;
    }
  }
  return nearest;
}

void ArtifactCurvesPropertyEditor::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF graph = professionalControlRect(this);
  painter.fillRect(graph, detail::propertySurfaceColor(true));

  painter.setPen(QPen(controlGridColor(), 1.0));
  for (int i = 0; i <= 4; ++i) {
    const qreal t = static_cast<qreal>(i) / 4.0;
    painter.drawLine(QPointF(graph.left() + graph.width() * t, graph.top()),
                     QPointF(graph.left() + graph.width() * t, graph.bottom()));
    painter.drawLine(QPointF(graph.left(), graph.top() + graph.height() * t),
                     QPointF(graph.right(), graph.top() + graph.height() * t));
  }
  painter.setPen(QPen(controlGridColor().lighter(125), 1.0, Qt::DashLine));
  painter.drawLine(graph.bottomLeft(), graph.topRight());

  QPainterPath curve;
  curve.moveTo(widgetPosition(points_.front()));
  for (std::size_t i = 1; i < points_.size(); ++i) {
    curve.lineTo(widgetPosition(points_[i]));
  }
  painter.setPen(QPen(controlAccentColor(), 2.0));
  painter.drawPath(curve);
  for (int i = 0; i < static_cast<int>(points_.size()); ++i) {
    painter.setBrush(i == activePoint_ ? controlAccentColor().lighter(130)
                                       : controlAccentColor());
    painter.setPen(QPen(detail::propertySurfaceColor(false), 1.0));
    painter.drawEllipse(widgetPosition(points_[i]), 4.5, 4.5);
  }
  painter.setPen(detail::themeColor(QStringLiteral("textMuted"), QColor(170, 174, 184)));
  painter.drawText(QRectF(8.0, height() - 16.0, width() - 16.0, 14.0),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Shadows                         Highlights"));
}

void ArtifactCurvesPropertyEditor::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  activePoint_ = pointAt(event->position());
  dragging_ = activePoint_ >= 0;
  update();
}

void ArtifactCurvesPropertyEditor::mouseMoveEvent(QMouseEvent* event) {
  if (!dragging_ || activePoint_ < 0) return;
  Point next = curvePosition(event->position());
  if (activePoint_ == 0 || activePoint_ + 1 == static_cast<int>(points_.size())) {
    next.x = points_[activePoint_].x;
  } else {
    next.x = std::clamp(next.x, points_[activePoint_ - 1].x + 0.005,
                        points_[activePoint_ + 1].x - 0.005);
  }
  points_[activePoint_] = next;
  previewValue(serializedPoints());
  update();
}

void ArtifactCurvesPropertyEditor::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || !dragging_) return;
  dragging_ = false;
  commitValue(serializedPoints());
}

void ArtifactCurvesPropertyEditor::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  const int existing = pointAt(event->position());
  if (existing > 0 && existing + 1 < static_cast<int>(points_.size())) {
    points_.erase(points_.begin() + existing);
    activePoint_ = -1;
    dragging_ = false;
    commitValue(serializedPoints());
    update();
    return;
  }
  if (points_.size() >= 16) return;
  const Point next = curvePosition(event->position());
  points_.push_back(next);
  normalizePoints();
  activePoint_ = pointAt(widgetPosition(next));
  commitValue(serializedPoints());
  update();
}

ArtifactLevelsPropertyEditor::ArtifactLevelsPropertyEditor(
    const ArtifactCore::AbstractProperty& property, QWidget* parent)
    : ArtifactAbstractPropertyEditor(parent) {
  setObjectName(QStringLiteral("propertyLevelsEditor"));
  setAccessibleName(QStringLiteral("Master levels"));
  setAccessibleDescription(
      QStringLiteral("Edit input black, gamma, input white, and output range"));
  setMinimumHeight(92);
  setValueFromVariant(property.getValue());
}

QVariant ArtifactLevelsPropertyEditor::value() const { return serializedLevels(); }

void ArtifactLevelsPropertyEditor::setValueFromVariant(const QVariant& value) {
  const auto entries = value.toString().split(QLatin1Char(','));
  if (entries.size() == 5) {
    std::array<double, 5> parsed{};
    bool valid = true;
    for (int i = 0; i < 5; ++i) {
      bool ok = false;
      parsed[static_cast<std::size_t>(i)] = QLocale::c().toDouble(entries[i], &ok);
      valid = valid && ok && std::isfinite(parsed[static_cast<std::size_t>(i)]);
    }
    if (valid) values_ = parsed;
  }
  values_[0] = std::clamp(values_[0], 0.0, values_[2]);
  values_[1] = std::clamp(values_[1], 0.01, 10.0);
  values_[2] = std::clamp(values_[2], values_[0], 255.0);
  values_[3] = std::clamp(values_[3], 0.0, values_[4]);
  values_[4] = std::clamp(values_[4], values_[3], 255.0);
  update();
}

QSize ArtifactLevelsPropertyEditor::sizeHint() const { return QSize(220, 96); }
QSize ArtifactLevelsPropertyEditor::minimumSizeHint() const { return QSize(150, 84); }

QString ArtifactLevelsPropertyEditor::serializedLevels() const {
  return QStringLiteral("%1,%2,%3,%4,%5")
      .arg(values_[0], 0, 'g', 9).arg(values_[1], 0, 'g', 9)
      .arg(values_[2], 0, 'g', 9).arg(values_[3], 0, 'g', 9)
      .arg(values_[4], 0, 'g', 9);
}

double ArtifactLevelsPropertyEditor::inputGammaPosition() const {
  const double span = std::max(1.0, values_[2] - values_[0]);
  return std::clamp((values_[0] + span * std::pow(0.5, values_[1])) / 255.0,
                    values_[0] / 255.0, values_[2] / 255.0);
}

int ArtifactLevelsPropertyEditor::handleAt(const QPointF& position) const {
  const QRectF graph = professionalControlRect(this).adjusted(0.0, 10.0, 0.0, -8.0);
  const std::array<QPointF, 5> locations{{
      {graph.left() + graph.width() * values_[0] / 255.0, graph.top()},
      {graph.left() + graph.width() * inputGammaPosition(), graph.top()},
      {graph.left() + graph.width() * values_[2] / 255.0, graph.top()},
      {graph.left() + graph.width() * values_[3] / 255.0, graph.bottom()},
      {graph.left() + graph.width() * values_[4] / 255.0, graph.bottom()}}};
  int nearest = None;
  double distance = 11.0;
  for (int i = 0; i < 5; ++i) {
    const double candidate = QLineF(position, locations[i]).length();
    if (candidate < distance) { distance = candidate; nearest = i; }
  }
  return nearest;
}

void ArtifactLevelsPropertyEditor::updateHandle(const QPointF& position,
                                                 const bool preview) {
  if (activeHandle_ == None) return;
  const QRectF graph = professionalControlRect(this).adjusted(0.0, 10.0, 0.0, -8.0);
  const double normalized = std::clamp((position.x() - graph.left()) / graph.width(),
                                       0.0, 1.0);
  const double level = normalized * 255.0;
  switch (activeHandle_) {
  case InputBlack: values_[0] = std::min(level, values_[2] - 1.0); break;
  case InputWhite: values_[2] = std::max(level, values_[0] + 1.0); break;
  case OutputBlack: values_[3] = std::min(level, values_[4]); break;
  case OutputWhite: values_[4] = std::max(level, values_[3]); break;
  case InputGamma: {
    const double low = values_[0] / 255.0;
    const double high = values_[2] / 255.0;
    const double relative = std::clamp((normalized - low) / std::max(0.0001, high - low),
                                       0.0001, 0.9999);
    values_[1] = std::clamp(std::log(0.5) / std::log(relative), 0.01, 10.0);
    break;
  }
  case None: break;
  }
  if (preview) previewValue(serializedLevels());
  update();
}

void ArtifactLevelsPropertyEditor::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF graph = professionalControlRect(this).adjusted(0.0, 10.0, 0.0, -8.0);
  QLinearGradient gradient(graph.topLeft(), graph.topRight());
  gradient.setColorAt(0.0, Qt::black);
  gradient.setColorAt(1.0, Qt::white);
  painter.fillRect(graph, gradient);
  painter.setPen(QPen(controlGridColor(), 1.0));
  painter.drawRect(graph);

  auto drawHandle = [&](const double normalized, const bool upper, const bool active) {
    const double x = graph.left() + graph.width() * normalized;
    QPolygonF triangle;
    if (upper) {
      triangle << QPointF(x, graph.top()) << QPointF(x - 5.0, graph.top() - 8.0)
               << QPointF(x + 5.0, graph.top() - 8.0);
    } else {
      triangle << QPointF(x, graph.bottom()) << QPointF(x - 5.0, graph.bottom() + 8.0)
               << QPointF(x + 5.0, graph.bottom() + 8.0);
    }
    painter.setBrush(active ? controlAccentColor().lighter(130) : controlAccentColor());
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(triangle);
  };
  drawHandle(values_[0] / 255.0, true, activeHandle_ == InputBlack);
  drawHandle(inputGammaPosition(), true, activeHandle_ == InputGamma);
  drawHandle(values_[2] / 255.0, true, activeHandle_ == InputWhite);
  drawHandle(values_[3] / 255.0, false, activeHandle_ == OutputBlack);
  drawHandle(values_[4] / 255.0, false, activeHandle_ == OutputWhite);
  painter.setPen(detail::themeColor(QStringLiteral("textMuted"), QColor(170, 174, 184)));
  painter.drawText(QRectF(8.0, height() - 16.0, width() - 16.0, 14.0),
                   Qt::AlignCenter,
                   QStringLiteral("Input %1  Gamma %2  Output %3–%4")
                       .arg(values_[0], 0, 'f', 0).arg(values_[1], 0, 'f', 2)
                       .arg(values_[3], 0, 'f', 0).arg(values_[4], 0, 'f', 0));
}

void ArtifactLevelsPropertyEditor::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  activeHandle_ = static_cast<Handle>(handleAt(event->position()));
  updateHandle(event->position(), true);
}

void ArtifactLevelsPropertyEditor::mouseMoveEvent(QMouseEvent* event) {
  if ((event->buttons() & Qt::LeftButton) && activeHandle_ != None) {
    updateHandle(event->position(), true);
  }
}

void ArtifactLevelsPropertyEditor::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton || activeHandle_ == None) return;
  updateHandle(event->position(), false);
  activeHandle_ = None;
  commitValue(serializedLevels());
  update();
}

ArtifactAbstractPropertyEditor *
createPropertyEditorWidget(const ArtifactCore::AbstractProperty &property,
                           QWidget *parent) {
  const QString propertyName = property.getName();
  if (propertyName == QStringLiteral("shape.fillGradientStops") ||
      (propertyName.startsWith(QStringLiteral("shape.content.")) &&
       propertyName.endsWith(QStringLiteral(".fillGradientStops")))) {
    return new ArtifactGradientStopsPropertyEditor(property, parent);
  }
  if (property.getName() == QStringLiteral("curve.master")) {
    return new ArtifactCurvesPropertyEditor(property, parent);
  }
  if (property.getName() == QStringLiteral("levels.master")) {
    return new ArtifactLevelsPropertyEditor(property, parent);
  }
  if (detail::isMultilineTextProperty(property)) {
    return new ArtifactTextAnimatorColorEditor(property, parent);
  }
  if (detail::isFontFamilyProperty(property)) {
    return new ArtifactFontFamilyPropertyEditor(property, parent);
  }
  if (detail::isPathProperty(property)) {
    return new ArtifactPathPropertyEditor(property, parent);
  }
  if (const auto enumOptions = detail::enumOptionsForProperty(property)) {
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
        property, parent, detail::shouldShowNumericSlider(property));
  case ArtifactCore::PropertyType::Integer:
    return new ArtifactIntPropertyEditor(
        property, parent, detail::shouldShowNumericSlider(property));
  case ArtifactCore::PropertyType::Boolean:
    return new ArtifactBoolPropertyEditor(property, parent);
  case ArtifactCore::PropertyType::Color:
    return new ArtifactColorPropertyEditor(property, parent);
  case ArtifactCore::PropertyType::String:
    return new ArtifactStringPropertyEditor(property, parent);
  case ArtifactCore::PropertyType::ObjectReference:
    return new ArtifactObjectReferencePropertyEditor(property, parent);
  case ArtifactCore::PropertyType::Point2D:
    return new ArtifactPoint2DPropertyEditor(property, parent);
  default:
    return nullptr;
  }
}

} // namespace Artifact

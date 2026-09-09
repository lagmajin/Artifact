module;

#include <algorithm>

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

module Artifact.Widgets.CreateLightLayerDialog;

import FloatColorPickerDialog;
import Artifact.Widgets.Dialog.FloatColorPickerHooks;
import Widgets.Utils.CSS;

namespace Artifact {

namespace {

QColor themeColor(const QString& value) { return QColor(value); }

void setTextColor(QLabel* label, const QColor& color)
{
  QPalette palette = label->palette();
  palette.setColor(QPalette::WindowText, color);
  label->setPalette(palette);
}

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
  auto* label = new QLabel(text, parent);
  QFont font = label->font();
  font.setBold(true);
  label->setFont(font);
  setTextColor(label, QColor(225, 151, 63));
  return label;
}

QFrame* divider(QWidget* parent)
{
  auto* line = new QFrame(parent);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Plain);
  return line;
}

class DialogActionButton final : public QPushButton {
public:
  DialogActionButton(const QString& text, QDialog* dialog, bool accept, QWidget* parent)
      : QPushButton(text, parent), dialog_(dialog), accept_(accept) {}

protected:
  void nextCheckState() override
  {
    if (accept_) dialog_->accept();
    else dialog_->reject();
  }

private:
  QDialog* dialog_ = nullptr;
  bool accept_ = false;
};

class TypeTileButton final : public QPushButton {
public:
  TypeTileButton(const QString& text, LightType type, CreateLightLayerDialog* dialog,
                 QWidget* parent)
      : QPushButton(text, parent), type_(type), dialog_(dialog)
  {
    setCheckable(true);
    setMinimumHeight(66);
    setAutoDefault(false);
  }

protected:
  void nextCheckState() override { dialog_->selectLightType(type_); }

private:
  LightType type_ = LightType::Point;
  CreateLightLayerDialog* dialog_ = nullptr;
};

class ColorButton final : public QPushButton {
public:
  ColorButton(CreateLightLayerDialog* dialog, QWidget* parent)
      : QPushButton(parent), dialog_(dialog)
  {
    setMinimumHeight(38);
    setAccessibleName(QStringLiteral("Light color"));
  }

protected:
  void nextCheckState() override { dialog_->chooseColor(); }

  void paintEvent(QPaintEvent*) override
  {
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Button));
    const auto value = dialog_->color();
    const QColor color = QColor::fromRgbF(value.r(), value.g(), value.b(), value.a());
    const QRect swatch(7, 6, 42, height() - 12);
    painter.fillRect(swatch, color);
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(swatch.adjusted(0, 0, -1, -1));
    painter.setPen(palette().color(QPalette::ButtonText));
    painter.drawText(rect().adjusted(62, 0, -8, 0), Qt::AlignLeft | Qt::AlignVCenter,
                     color.name(QColor::HexRgb).toUpper());
    if (hasFocus()) {
      painter.setPen(palette().color(QPalette::Highlight));
      painter.drawRect(rect().adjusted(1, 1, -2, -2));
    }
  }

private:
  CreateLightLayerDialog* dialog_ = nullptr;
};

class PresentationRefreshFilter final : public QObject {
public:
  PresentationRefreshFilter(CreateLightLayerDialog* dialog, QObject* parent)
      : QObject(parent), dialog_(dialog) {}

protected:
  bool eventFilter(QObject* watched, QEvent* event) override
  {
    Q_UNUSED(watched);
    const auto type = event->type();
    if (type == QEvent::KeyRelease || type == QEvent::MouseButtonRelease ||
        type == QEvent::Wheel || type == QEvent::FocusOut) {
      dialog_->refreshPresentation();
    }
    return false;
  }

private:
  CreateLightLayerDialog* dialog_ = nullptr;
};

QWidget* makeFormPage(QWidget* parent, QGridLayout*& grid)
{
  auto* page = new QWidget(parent);
  grid = new QGridLayout(page);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(14);
  grid->setVerticalSpacing(12);
  grid->setColumnStretch(1, 1);
  return page;
}

QLabel* fieldLabel(const QString& text, QWidget* parent)
{
  auto* label = new QLabel(text, parent);
  label->setMinimumWidth(90);
  return label;
}

} // namespace

class CreateLightLayerDialog::Impl {
public:
  QLineEdit* name = nullptr;
  TypeTileButton* typeButtons[5] {};
  ColorButton* colorButton = nullptr;
  QDoubleSpinBox* intensity = nullptr;
  QStackedWidget* typePages = nullptr;
  QLabel* typeTitle = nullptr;
  QLabel* typeDescription = nullptr;
  QLabel* summary = nullptr;
  QDoubleSpinBox* range = nullptr;
  QDoubleSpinBox* spotRange = nullptr;
  QDoubleSpinBox* coneAngle = nullptr;
  QDoubleSpinBox* coneFeather = nullptr;
  QComboBox* areaShape = nullptr;
  QDoubleSpinBox* areaWidth = nullptr;
  QDoubleSpinBox* areaHeight = nullptr;
  QCheckBox* pointShadows = nullptr;
  QCheckBox* spotShadows = nullptr;
  QCheckBox* parallelShadows = nullptr;
  QCheckBox* areaShadows = nullptr;
  ArtifactCore::FloatColor selectedColor {1.0f, 1.0f, 1.0f, 1.0f};
  LightType selectedType = LightType::Point;
};

CreateLightLayerDialog::CreateLightLayerDialog(QWidget* parent)
    : QDialog(parent), impl_(new Impl())
{
  setWindowTitle(QStringLiteral("Create Light Layer"));
  setAccessibleName(QStringLiteral("Create Light Layer Dialog"));
  setMinimumSize(820, 500);

  const auto& theme = ArtifactCore::currentDCCTheme();
  QPalette dialogPalette = palette();
  dialogPalette.setColor(QPalette::Window, themeColor(theme.backgroundColor));
  dialogPalette.setColor(QPalette::Base, themeColor(theme.secondaryBackgroundColor));
  dialogPalette.setColor(QPalette::Button, themeColor(theme.secondaryBackgroundColor));
  dialogPalette.setColor(QPalette::Text, themeColor(theme.textColor));
  dialogPalette.setColor(QPalette::WindowText, themeColor(theme.textColor));
  dialogPalette.setColor(QPalette::ButtonText, themeColor(theme.textColor));
  dialogPalette.setColor(QPalette::Highlight, themeColor(theme.accentColor));
  dialogPalette.setColor(QPalette::HighlightedText, Qt::white);
  setPalette(dialogPalette);
  setAutoFillBackground(true);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(24, 20, 24, 18);
  root->setSpacing(16);

  auto* title = new QLabel(QStringLiteral("Create Light Layer"), this);
  QFont titleFont = title->font();
  titleFont.setBold(true);
  titleFont.setPointSize(14);
  title->setFont(titleFont);
  root->addWidget(title);

  auto* columns = new QHBoxLayout();
  columns->setSpacing(14);

  auto* lightPanel = new QFrame(this);
  lightPanel->setFrameShape(QFrame::StyledPanel);
  auto* lightLayout = new QGridLayout(lightPanel);
  lightLayout->setContentsMargins(20, 18, 20, 18);
  lightLayout->setHorizontalSpacing(14);
  lightLayout->setVerticalSpacing(13);
  lightLayout->setColumnStretch(1, 1);
  lightLayout->addWidget(sectionLabel(QStringLiteral("LIGHT"), lightPanel), 0, 0, 1, 2);

  impl_->name = new QLineEdit(QStringLiteral("Light 1"), lightPanel);
  impl_->name->setAccessibleName(QStringLiteral("Light name"));
  lightLayout->addWidget(fieldLabel(QStringLiteral("Name"), lightPanel), 1, 0);
  lightLayout->addWidget(impl_->name, 1, 1);

  auto* typeRow = new QWidget(lightPanel);
  auto* typeLayout = new QHBoxLayout(typeRow);
  typeLayout->setContentsMargins(0, 0, 0, 0);
  typeLayout->setSpacing(6);
  const QString typeNames[5] = {QStringLiteral("Point"), QStringLiteral("Spot"),
                                QStringLiteral("Parallel"), QStringLiteral("Ambient"),
                                QStringLiteral("Area")};
  for (int index = 0; index < 5; ++index) {
    impl_->typeButtons[index] = new TypeTileButton(
        typeNames[index], static_cast<LightType>(index), this, typeRow);
    impl_->typeButtons[index]->setAccessibleDescription(
        QStringLiteral("Create a %1 light").arg(typeNames[index]));
    typeLayout->addWidget(impl_->typeButtons[index], 1);
  }
  lightLayout->addWidget(fieldLabel(QStringLiteral("Type"), lightPanel), 2, 0);
  lightLayout->addWidget(typeRow, 2, 1);

  impl_->colorButton = new ColorButton(this, lightPanel);
  lightLayout->addWidget(fieldLabel(QStringLiteral("Color"), lightPanel), 3, 0);
  lightLayout->addWidget(impl_->colorButton, 3, 1);

  impl_->intensity = new QDoubleSpinBox(lightPanel);
  impl_->intensity->setRange(0.0, 10000.0);
  impl_->intensity->setDecimals(1);
  impl_->intensity->setValue(100.0);
  impl_->intensity->setSuffix(QStringLiteral(" %"));
  impl_->intensity->setAccessibleName(QStringLiteral("Light intensity"));
  lightLayout->addWidget(fieldLabel(QStringLiteral("Intensity"), lightPanel), 4, 0);
  lightLayout->addWidget(impl_->intensity, 4, 1);
  lightLayout->setRowStretch(5, 1);

  auto* settingsPanel = new QFrame(this);
  settingsPanel->setFrameShape(QFrame::StyledPanel);
  auto* settingsLayout = new QVBoxLayout(settingsPanel);
  settingsLayout->setContentsMargins(20, 18, 20, 18);
  settingsLayout->setSpacing(12);
  settingsLayout->addWidget(sectionLabel(QStringLiteral("TYPE SETTINGS"), settingsPanel));

  impl_->typeTitle = new QLabel(settingsPanel);
  QFont typeFont = impl_->typeTitle->font();
  typeFont.setBold(true);
  typeFont.setPointSize(12);
  impl_->typeTitle->setFont(typeFont);
  settingsLayout->addWidget(impl_->typeTitle);
  impl_->typeDescription = new QLabel(settingsPanel);
  impl_->typeDescription->setWordWrap(true);
  setTextColor(impl_->typeDescription, themeColor(theme.textColor).darker(135));
  settingsLayout->addWidget(impl_->typeDescription);
  settingsLayout->addWidget(divider(settingsPanel));

  impl_->typePages = new QStackedWidget(settingsPanel);
  settingsLayout->addWidget(impl_->typePages, 1);

  QGridLayout* pageGrid = nullptr;
  auto* pointPage = makeFormPage(impl_->typePages, pageGrid);
  impl_->range = new QDoubleSpinBox(pointPage);
  impl_->range->setRange(1.0, 100000.0);
  impl_->range->setValue(500.0);
  impl_->range->setSuffix(QStringLiteral(" px"));
  impl_->pointShadows = new QCheckBox(QStringLiteral("Enabled"), pointPage);
  impl_->pointShadows->setChecked(true);
  pageGrid->addWidget(fieldLabel(QStringLiteral("Range"), pointPage), 0, 0);
  pageGrid->addWidget(impl_->range, 0, 1);
  pageGrid->addWidget(fieldLabel(QStringLiteral("Cast Shadows"), pointPage), 1, 0);
  pageGrid->addWidget(impl_->pointShadows, 1, 1);
  pageGrid->setRowStretch(2, 1);
  impl_->typePages->addWidget(pointPage);

  auto* spotPage = makeFormPage(impl_->typePages, pageGrid);
  impl_->spotRange = new QDoubleSpinBox(spotPage);
  impl_->spotRange->setRange(1.0, 100000.0);
  impl_->spotRange->setValue(500.0);
  impl_->spotRange->setSuffix(QStringLiteral(" px"));
  impl_->coneAngle = new QDoubleSpinBox(spotPage);
  impl_->coneAngle->setRange(0.1, 179.0);
  impl_->coneAngle->setValue(45.0);
  impl_->coneAngle->setSuffix(QStringLiteral(" deg"));
  impl_->coneFeather = new QDoubleSpinBox(spotPage);
  impl_->coneFeather->setRange(0.0, 179.0);
  impl_->coneFeather->setValue(10.0);
  impl_->coneFeather->setSuffix(QStringLiteral(" deg"));
  impl_->spotShadows = new QCheckBox(QStringLiteral("Enabled"), spotPage);
  impl_->spotShadows->setChecked(true);
  pageGrid->addWidget(fieldLabel(QStringLiteral("Range"), spotPage), 0, 0);
  pageGrid->addWidget(impl_->spotRange, 0, 1);
  pageGrid->addWidget(fieldLabel(QStringLiteral("Cone Angle"), spotPage), 1, 0);
  pageGrid->addWidget(impl_->coneAngle, 1, 1);
  pageGrid->addWidget(fieldLabel(QStringLiteral("Cone Feather"), spotPage), 2, 0);
  pageGrid->addWidget(impl_->coneFeather, 2, 1);
  pageGrid->addWidget(fieldLabel(QStringLiteral("Cast Shadows"), spotPage), 3, 0);
  pageGrid->addWidget(impl_->spotShadows, 3, 1);
  impl_->typePages->addWidget(spotPage);

  auto* parallelPage = makeFormPage(impl_->typePages, pageGrid);
  impl_->parallelShadows = new QCheckBox(QStringLiteral("Enabled"), parallelPage);
  impl_->parallelShadows->setChecked(true);
  pageGrid->addWidget(fieldLabel(QStringLiteral("Cast Shadows"), parallelPage), 0, 0);
  pageGrid->addWidget(impl_->parallelShadows, 0, 1);
  pageGrid->setRowStretch(1, 1);
  impl_->typePages->addWidget(parallelPage);

  auto* ambientPage = makeFormPage(impl_->typePages, pageGrid);
  auto* ambientHint = new QLabel(
      QStringLiteral("Ambient light affects the scene uniformly and has no range or shadow controls."),
      ambientPage);
  ambientHint->setWordWrap(true);
  setTextColor(ambientHint, themeColor(theme.textColor).darker(135));
  pageGrid->addWidget(ambientHint, 0, 0, 1, 2);
  pageGrid->setRowStretch(1, 1);
  impl_->typePages->addWidget(ambientPage);

  auto* areaPage = makeFormPage(impl_->typePages, pageGrid);
  impl_->areaShape = new QComboBox(areaPage);
  impl_->areaShape->addItem(QStringLiteral("Rectangle"), static_cast<int>(AreaLightShape::Rectangle));
  impl_->areaShape->addItem(QStringLiteral("Disk"), static_cast<int>(AreaLightShape::Disk));
  impl_->areaWidth = new QDoubleSpinBox(areaPage);
  impl_->areaWidth->setRange(1.0, 100000.0);
  impl_->areaWidth->setValue(100.0);
  impl_->areaWidth->setSuffix(QStringLiteral(" px"));
  impl_->areaHeight = new QDoubleSpinBox(areaPage);
  impl_->areaHeight->setRange(1.0, 100000.0);
  impl_->areaHeight->setValue(100.0);
  impl_->areaHeight->setSuffix(QStringLiteral(" px"));
  impl_->areaShadows = new QCheckBox(QStringLiteral("Enabled"), areaPage);
  impl_->areaShadows->setChecked(true);
  pageGrid->addWidget(fieldLabel(QStringLiteral("Shape"), areaPage), 0, 0);
  pageGrid->addWidget(impl_->areaShape, 0, 1);
  pageGrid->addWidget(fieldLabel(QStringLiteral("Width"), areaPage), 1, 0);
  pageGrid->addWidget(impl_->areaWidth, 1, 1);
  pageGrid->addWidget(fieldLabel(QStringLiteral("Height"), areaPage), 2, 0);
  pageGrid->addWidget(impl_->areaHeight, 2, 1);
  pageGrid->addWidget(fieldLabel(QStringLiteral("Cast Shadows"), areaPage), 3, 0);
  pageGrid->addWidget(impl_->areaShadows, 3, 1);
  impl_->typePages->addWidget(areaPage);

  settingsLayout->addWidget(divider(settingsPanel));
  impl_->summary = new QLabel(settingsPanel);
  impl_->summary->setWordWrap(true);
  impl_->summary->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
  impl_->summary->setMargin(12);
  settingsLayout->addWidget(impl_->summary);

  columns->addWidget(lightPanel, 1);
  columns->addWidget(settingsPanel, 1);
  root->addLayout(columns, 1);

  auto* footer = new QHBoxLayout();
  footer->addStretch();
  auto* cancel = new DialogActionButton(QStringLiteral("Cancel"), this, false, this);
  auto* create = new DialogActionButton(QStringLiteral("Create"), this, true, this);
  cancel->setMinimumSize(108, 36);
  create->setMinimumSize(108, 36);
  create->setDefault(true);
  QPalette primaryPalette = create->palette();
  primaryPalette.setColor(QPalette::Button, QColor(43, 111, 232));
  primaryPalette.setColor(QPalette::ButtonText, Qt::white);
  create->setPalette(primaryPalette);
  create->setAutoFillBackground(true);
  footer->addWidget(cancel);
  footer->addWidget(create);
  root->addLayout(footer);

  auto* refreshFilter = new PresentationRefreshFilter(this, this);
  impl_->intensity->installEventFilter(refreshFilter);
  impl_->range->installEventFilter(refreshFilter);
  impl_->spotRange->installEventFilter(refreshFilter);
  impl_->coneAngle->installEventFilter(refreshFilter);
  impl_->coneFeather->installEventFilter(refreshFilter);
  impl_->areaShape->installEventFilter(refreshFilter);
  impl_->areaWidth->installEventFilter(refreshFilter);
  impl_->areaHeight->installEventFilter(refreshFilter);
  impl_->pointShadows->installEventFilter(refreshFilter);
  impl_->spotShadows->installEventFilter(refreshFilter);
  impl_->parallelShadows->installEventFilter(refreshFilter);
  impl_->areaShadows->installEventFilter(refreshFilter);

  selectLightType(LightType::Point);
}

CreateLightLayerDialog::~CreateLightLayerDialog()
{
  delete impl_;
  impl_ = nullptr;
}

QString CreateLightLayerDialog::lightName() const { return impl_->name->text().trimmed(); }
LightType CreateLightLayerDialog::lightType() const { return impl_->selectedType; }
ArtifactCore::FloatColor CreateLightLayerDialog::color() const { return impl_->selectedColor; }
float CreateLightLayerDialog::intensity() const { return static_cast<float>(impl_->intensity->value()); }
float CreateLightLayerDialog::range() const
{
  return static_cast<float>(impl_->selectedType == LightType::Spot
                                ? impl_->spotRange->value() : impl_->range->value());
}
float CreateLightLayerDialog::coneAngle() const { return static_cast<float>(impl_->coneAngle->value()); }
float CreateLightLayerDialog::coneFeather() const { return static_cast<float>(impl_->coneFeather->value()); }
float CreateLightLayerDialog::areaWidth() const { return static_cast<float>(impl_->areaWidth->value()); }
float CreateLightLayerDialog::areaHeight() const { return static_cast<float>(impl_->areaHeight->value()); }
AreaLightShape CreateLightLayerDialog::areaShape() const
{
  return static_cast<AreaLightShape>(impl_->areaShape->currentData().toInt());
}
bool CreateLightLayerDialog::castsShadows() const
{
  switch (impl_->selectedType) {
    case LightType::Point: return impl_->pointShadows->isChecked();
    case LightType::Spot: return impl_->spotShadows->isChecked();
    case LightType::Parallel: return impl_->parallelShadows->isChecked();
    case LightType::Area: return impl_->areaShadows->isChecked();
    case LightType::Ambient: return false;
  }
  return false;
}

void CreateLightLayerDialog::selectLightType(LightType type)
{
  const int index = std::clamp(static_cast<int>(type), 0, 4);
  impl_->selectedType = static_cast<LightType>(index);
  impl_->typePages->setCurrentIndex(index);
  for (int buttonIndex = 0; buttonIndex < 5; ++buttonIndex) {
    impl_->typeButtons[buttonIndex]->setChecked(buttonIndex == index);
  }
  refreshPresentation();
}

void CreateLightLayerDialog::chooseColor()
{
  ArtifactWidgets::FloatColorPicker picker(this);
  picker.setWindowTitle(QStringLiteral("Choose Light Color"));
  picker.setInitialColor(impl_->selectedColor);
  configureFloatColorPicker(&picker, ColorSelectionPurpose::Creation);
  if (picker.exec() != QDialog::Accepted) return;
  impl_->selectedColor = picker.getColor();
  refreshPresentation();
}

void CreateLightLayerDialog::refreshPresentation()
{
  static const QString titles[5] = {
      QStringLiteral("Point Light"), QStringLiteral("Spot Light"),
      QStringLiteral("Parallel Light"), QStringLiteral("Ambient Light"),
      QStringLiteral("Area Light")};
  static const QString descriptions[5] = {
      QStringLiteral("Emits light in every direction from a single position."),
      QStringLiteral("Projects a focused cone of light from a single position."),
      QStringLiteral("Emits parallel rays across the scene from one direction."),
      QStringLiteral("Adds uniform light to every affected object in the scene."),
      QStringLiteral("Emits soft light from a rectangular or disk-shaped surface.")};
  const int index = std::clamp(static_cast<int>(impl_->selectedType), 0, 4);
  impl_->typeTitle->setText(titles[index]);
  impl_->typeDescription->setText(descriptions[index]);

  const QColor display = QColor::fromRgbF(impl_->selectedColor.r(), impl_->selectedColor.g(),
                                          impl_->selectedColor.b(), impl_->selectedColor.a());
  impl_->summary->setText(
      QStringLiteral("Creates: %1 · %2 · %3% intensity\nMore controls in Inspector.")
          .arg(titles[index], display.name(QColor::HexRgb).toUpper())
          .arg(impl_->intensity->value(), 0, 'f', 1));
  impl_->colorButton->update();
}

void CreateLightLayerDialog::applyTo(ArtifactLightLayer& layer) const
{
  layer.setLightType(lightType());
  layer.setColor(color());
  layer.setIntensity(intensity());
  layer.setCastsShadows(castsShadows());
  if (lightType() == LightType::Point || lightType() == LightType::Spot) {
    layer.setRange(range());
  }
  if (lightType() == LightType::Spot) {
    layer.setConeAngle(coneAngle());
    layer.setConeFeather(std::min(coneFeather(), coneAngle()));
  }
  if (lightType() == LightType::Area) {
    layer.setAreaShape(areaShape());
    layer.setAreaSize(areaWidth(), areaHeight());
  }
}

} // namespace Artifact

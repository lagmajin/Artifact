module;
#include <QTime>
#include <QWidget>
#include <QDialog>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <wobjectimpl.h>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QColorDialog>
#include <QPushButton>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>

module Dialog.Composition;

import Widgets.Utils.CSS;
import Widgets.EditableLabel;
import DragSpinBox;
import Artifact.Project.Manager;
import Utils.String.UniString;

namespace Artifact {

 using namespace ArtifactCore;
 using namespace ArtifactWidgets;
	
 class CompositionSettingPage::Impl
 {
 public:
  Impl();
  QComboBox* resolutionCombobox_ = nullptr;
  QComboBox* fpsCombo_ = nullptr;
  QComboBox* pixelAspectCombo_ = nullptr;
  DragSpinBox* widthSpinBox = nullptr;
  DragSpinBox* heightSpinBox = nullptr;
  DoubleDragSpinBox* durationSpinBox = nullptr;
  QLineEdit* startTimecodeEdit = nullptr;
  QPushButton* bgColorButton = nullptr;
  QColor bgColor = QColor(0, 0, 0, 255);
 };

 CompositionSettingPage::Impl::Impl() {}

 CompositionSettingPage::CompositionSettingPage(QWidget* parent) : QWidget(parent), impl_(new Impl)
 {
  auto mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(15, 15, 15, 15);
  mainLayout->setSpacing(10);

  auto formLayout = new QFormLayout();
  formLayout->setLabelAlignment(Qt::AlignRight);
  formLayout->setVerticalSpacing(12);
  formLayout->setHorizontalSpacing(20);

  QString pageStyle = R"(
      QLabel { color: #AAA; font-size: 11px; }
      QComboBox, QLineEdit, QSpinBox, QDoubleSpinBox {
          background-color: #2D2D30; border: 1px solid #454545; border-radius: 4px; color: white; padding: 4px 8px;
      }
      QComboBox:hover, QLineEdit:focus { border: 1px solid #007ACC; }
  )";

  impl_->widthSpinBox = new DragSpinBox();
  impl_->widthSpinBox->setRange(1, 16384);
  impl_->heightSpinBox = new DragSpinBox();
  impl_->heightSpinBox->setRange(1, 16384);
  
  impl_->resolutionCombobox_ = new QComboBox();
  impl_->resolutionCombobox_->addItem("HD 1080p (1920x1080)", QVariant::fromValue(QSize(1920, 1080)));
  impl_->resolutionCombobox_->addItem("HD 720p (1280x720)", QVariant::fromValue(QSize(1280, 720)));
  impl_->resolutionCombobox_->addItem("4K UHD (3840x2160)", QVariant::fromValue(QSize(3840, 2160)));
  impl_->resolutionCombobox_->addItem("Custom...", QVariant::fromValue(QSize(-1, -1)));

  const QSize initialResolution = impl_->resolutionCombobox_->currentData().toSize();
  if (initialResolution.width() > 0 && initialResolution.height() > 0) {
    impl_->widthSpinBox->setValue(initialResolution.width());
    impl_->heightSpinBox->setValue(initialResolution.height());
  }

  auto sizeWidget = new QWidget();
  auto sizeHBox = new QHBoxLayout(sizeWidget);
  sizeHBox->setContentsMargins(0, 0, 0, 0);
  sizeHBox->addWidget(impl_->widthSpinBox);
  sizeHBox->addWidget(new QLabel("x"));
  sizeHBox->addWidget(impl_->heightSpinBox);
  sizeHBox->addWidget(new QLabel("px"));

  impl_->pixelAspectCombo_ = new QComboBox();
  impl_->pixelAspectCombo_->addItem("Square Pixels (1.0)", QVariant::fromValue(1.0));
  impl_->pixelAspectCombo_->addItem("D1/DV NTSC (0.91)", QVariant::fromValue(0.9091));
  impl_->pixelAspectCombo_->addItem("D1/DV PAL (1.09)", QVariant::fromValue(1.0940));

  impl_->fpsCombo_ = new QComboBox();
  impl_->fpsCombo_->addItem("23.976 fps", QVariant::fromValue(23.976));
  impl_->fpsCombo_->addItem("24 fps", QVariant::fromValue(24.0));
  impl_->fpsCombo_->addItem("25 fps", QVariant::fromValue(25.0));
  impl_->fpsCombo_->addItem("29.97 fps", QVariant::fromValue(29.97));
  impl_->fpsCombo_->addItem("30 fps", QVariant::fromValue(30.0));
  impl_->fpsCombo_->addItem("60 fps", QVariant::fromValue(60.0));
  impl_->fpsCombo_->setCurrentIndex(4); 

  impl_->startTimecodeEdit = new QLineEdit("00:00:00:00");
  
  impl_->durationSpinBox = new DoubleDragSpinBox();
  impl_->durationSpinBox->setRange(0.1, 3600.0);
  impl_->durationSpinBox->setValue(10.0);

  impl_->bgColorButton = new QPushButton("Pick Color");
  impl_->bgColorButton->setFixedSize(100, 24);
  impl_->bgColorButton->setStyleSheet("background-color: #000; border: 1px solid #555; border-radius: 4px; color: white;");
  
  formLayout->addRow("Preset:", impl_->resolutionCombobox_);
  formLayout->addRow("Resolution:", sizeWidget);
  formLayout->addRow("Pixel Aspect:", impl_->pixelAspectCombo_);
  formLayout->addRow("Frame Rate:", impl_->fpsCombo_);
  formLayout->addRow(new QLabel(" ")); 
  formLayout->addRow("Start Time:", impl_->startTimecodeEdit);
  formLayout->addRow("Duration (s):", impl_->durationSpinBox);
  formLayout->addRow(new QLabel(" "));
  formLayout->addRow("Background Color:", impl_->bgColorButton);
  
  mainLayout->addLayout(formLayout);
  mainLayout->addStretch();

  QObject::connect(impl_->resolutionCombobox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    QSize sz = impl_->resolutionCombobox_->currentData().toSize();
    if (sz.width() > 0 && sz.height() > 0) {
      impl_->widthSpinBox->setValue(sz.width());
      impl_->heightSpinBox->setValue(sz.height());
    }
  });
  
  auto forceCustom = [this]() {
      if (impl_->resolutionCombobox_->currentData().toSize().width() != -1) {
          impl_->resolutionCombobox_->blockSignals(true);
          impl_->resolutionCombobox_->setCurrentIndex(impl_->resolutionCombobox_->count() - 1);
          impl_->resolutionCombobox_->blockSignals(false);
      }
  };
  QObject::connect(impl_->widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, forceCustom);
  QObject::connect(impl_->heightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, forceCustom);

  QObject::connect(impl_->bgColorButton, &QPushButton::clicked, this, [this]() {
      QColor c = QColorDialog::getColor(impl_->bgColor, this, "Background Color");
      if (c.isValid()) {
          impl_->bgColor = c;
          impl_->bgColorButton->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 4px; color: %2;")
              .arg(c.name())
              .arg(c.lightness() > 128 ? "black" : "white"));
      }
  });

  setStyleSheet(pageStyle);
 }

 CompositionSettingPage::~CompositionSettingPage() {}

 ArtifactCompositionInitParams CompositionSettingPage::getInitParams(const QString& name) const
 {
     ArtifactCompositionInitParams params;
     UniString u;
     u.setQString(name);
     params.setCompositionName(u);
     params.setResolution(impl_->widthSpinBox->value(), impl_->heightSpinBox->value());
     params.setFrameRate(impl_->fpsCombo_->currentData().toDouble());
     double aspect = impl_->pixelAspectCombo_->currentData().toDouble();
     params.setPixelAspectRatio(AspectRatio(static_cast<int>(aspect * 10000), 10000));
     params.setDurationSeconds(impl_->durationSpinBox->value());
     QColor c = impl_->bgColor;
     params.setBackgroundColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
     return params;
 }

 CompositionExtendSettingPage::CompositionExtendSettingPage(QWidget* parent) : QWidget(parent) {}
 CompositionExtendSettingPage::~CompositionExtendSettingPage() {}

 class CreateCompositionDialog::Impl {
 public:
  Impl(CreateCompositionDialog* pDialog = nullptr);
  CompositionSettingPage* compositionSettingPage_ = nullptr;
  QTabWidget* pTabWidget = nullptr;
  QPoint m_dragPosition;
  bool m_isDragging = false;
  bool okCalled_ = false;
  QPropertyAnimation* m_showAnimation = nullptr;
  EditableLabel* compositionNameEdit_ = nullptr;

  void ok(QDialog* dialog);
  void cancel(QDialog* dialog);
 };

 CreateCompositionDialog::Impl::Impl(CreateCompositionDialog* pDialog) {}

 void CreateCompositionDialog::Impl::ok(QDialog* dialog)
 {
  if (okCalled_) return;
  okCalled_ = true;
  if (compositionNameEdit_) compositionNameEdit_->finishEdit();
  QString name = compositionNameEdit_ ? compositionNameEdit_->text() : "Comp1";
  ArtifactProjectManager::getInstance().suppressDefaultCreate(true);
  if (compositionSettingPage_) {
    ArtifactCompositionInitParams params = compositionSettingPage_->getInitParams(name);
    ArtifactProjectManager::getInstance().createComposition(params);
  }
  ArtifactProjectManager::getInstance().suppressDefaultCreate(false);
  dialog->accept();
 }

 void CreateCompositionDialog::Impl::cancel(QDialog* dialog) { dialog->reject(); }

 W_OBJECT_IMPL(CreateCompositionDialog)

 CreateCompositionDialog::CreateCompositionDialog(QWidget* parent) : QDialog(parent), impl_(new Impl(this))
 {
  setWindowTitle("Composition Settings");
  setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
  setFixedSize(500, 520);
  
  auto mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Header Area
  auto header = new QWidget();
  header->setFixedHeight(50);
  header->setStyleSheet("background-color: #2D2D30; border-bottom: 1px solid #444;");
  auto hLayout = new QHBoxLayout(header);
  hLayout->setContentsMargins(15, 0, 15, 0);
  auto title = new QLabel("🎬 Composition Settings");
  title->setStyleSheet("color: white; font-weight: bold; font-size: 14px;");
  hLayout->addWidget(title);
  hLayout->addStretch();
  mainLayout->addWidget(header);

  // Content Area
  auto content = new QVBoxLayout();
  content->setContentsMargins(20, 20, 20, 10);
  content->setSpacing(15);

  auto nameRow = new QHBoxLayout();
  auto nameLbl = new QLabel("Name:");
  nameLbl->setStyleSheet("color: #AAA; font-weight: bold;");
  nameLbl->setFixedWidth(60);
  impl_->compositionNameEdit_ = new EditableLabel();
  impl_->compositionNameEdit_->setText("Comp1");
  impl_->compositionNameEdit_->setStyleSheet("background: #252526; padding: 4px; border-radius: 4px; font-weight: bold;");
  nameRow->addWidget(nameLbl);
  nameRow->addWidget(impl_->compositionNameEdit_);
  content->addLayout(nameRow);

  impl_->pTabWidget = new QTabWidget();
  impl_->compositionSettingPage_ = new CompositionSettingPage();
  impl_->pTabWidget->addTab(impl_->compositionSettingPage_, "Basic");
  impl_->pTabWidget->setStyleSheet(R"(
      QTabWidget::pane { border: 1px solid #3F3F46; top: -1px; background: #1E1E20; }
      QTabBar::tab { background: #2D2D30; color: #999; padding: 6px 15px; border: 1px solid #3F3F46; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; }
      QTabBar::tab:selected { background: #1E1E20; color: white; border-bottom: 2px solid #007ACC; }
  )");
  content->addWidget(impl_->pTabWidget);
  mainLayout->addLayout(content);

  // Footer / Buttons
  auto footer = new QWidget();
  footer->setStyleSheet("background-color: #252526; border-top: 1px solid #333;");
  auto fLayout = new QHBoxLayout(footer);
  fLayout->setContentsMargins(15, 10, 15, 10);
  auto okBtn = new QPushButton("OK");
  okBtn->setFixedSize(80, 28);
  okBtn->setStyleSheet("background: #007ACC; color: white; border-radius: 4px; font-weight: bold;");
  auto cancelBtn = new QPushButton("Cancel");
  cancelBtn->setFixedSize(80, 28);
  cancelBtn->setStyleSheet("background: #3E3E42; color: #DDD; border-radius: 4px;");
  fLayout->addStretch();
  fLayout->addWidget(okBtn);
  fLayout->addWidget(cancelBtn);
  mainLayout->addWidget(footer);

  QObject::connect(okBtn, &QPushButton::clicked, this, [this]() { impl_->ok(this); });
  QObject::connect(cancelBtn, &QPushButton::clicked, this, [this]() { impl_->cancel(this); });

  setStyleSheet("QDialog { background-color: #1E1E20; border: 1px solid #444; }");
 }

 CreateCompositionDialog::~CreateCompositionDialog() { delete impl_; }

 void CreateCompositionDialog::setCompositionName(const QString& name) { if (impl_->compositionNameEdit_) impl_->compositionNameEdit_->setText(name); }
 QString CreateCompositionDialog::compositionName() const { return impl_->compositionNameEdit_ ? impl_->compositionNameEdit_->text() : ""; }

 void CreateCompositionDialog::keyPressEvent(QKeyEvent* event)
 {
  if (event->key() == Qt::Key_Escape) close();
  QDialog::keyPressEvent(event);
 }

 void CreateCompositionDialog::mousePressEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::LeftButton) {
   impl_->m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
   impl_->m_isDragging = true;
   event->accept();
  } else QDialog::mousePressEvent(event);
 }

 void CreateCompositionDialog::mouseReleaseEvent(QMouseEvent* event)
 {
  if (impl_->m_isDragging && event->button() == Qt::LeftButton) {
   impl_->m_isDragging = false;
   event->accept();
  } else QDialog::mouseReleaseEvent(event);
 }

 void CreateCompositionDialog::mouseMoveEvent(QMouseEvent* event)
 {
  if (impl_->m_isDragging && event->buttons() & Qt::LeftButton) {
   move(event->globalPosition().toPoint() - impl_->m_dragPosition);
   event->accept();
  } else QDialog::mouseMoveEvent(event);
 }

 void CreateCompositionDialog::showAnimated() { show(); } // Animation logic simplified for robustness

 void CreateCompositionDialog::showEvent(QShowEvent* event)
 {
  QDialog::showEvent(event);
  QPoint endPos;
  if (parentWidget()) {
   QRect pr = parentWidget()->geometry();
   endPos = pr.center() - rect().center();
  } else {
   endPos = QGuiApplication::primaryScreen()->availableGeometry().center() - rect().center();
  }
  move(endPos);
 }

 void CreateCompositionDialog::closeEvent(QCloseEvent* event) { QDialog::closeEvent(event); }
 void CreateCompositionDialog::setDefaultFocus() {}
}

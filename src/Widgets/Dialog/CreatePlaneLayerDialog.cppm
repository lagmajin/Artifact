module;
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QDialog>
#include <QLineEdit>
#include <QFormLayout>
#include <QComboBox>
#include <QPushButton>
#include <QColorDialog>
#include <QPropertyAnimation>
#include <wobjectimpl.h>
module Artifact.Widgets.CreatePlaneLayerDialog;

import Widgets.Dialog.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Widgets.Utils.CSS;
import Widgets.EditableLabel;
import DragSpinBox;
import Color.Float;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Composition.Settings;
import Artifact.Layer.Solid2D;

namespace Artifact {
	
 using namespace ArtifactWidgets;

 class PlaneLayerSettingPage::Impl {
 public:
  Impl();
  ~Impl() = default;
  DragSpinBox* widthSpinBox = nullptr;
  DragSpinBox* heightSpinBox = nullptr;
  QComboBox* resolutionCombobox_ = nullptr;
  QPushButton* bgColorButton = nullptr;
  QPushButton* matchCompButton = nullptr;
  QColor bgColor = QColor(255, 255, 255, 255);
 };

 PlaneLayerSettingPage::Impl::Impl()
 {
 }

 PlaneLayerSettingPage::PlaneLayerSettingPage(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  impl_->widthSpinBox = new DragSpinBox();
  impl_->widthSpinBox->setRange(1, 16384);
  impl_->widthSpinBox->setValue(1920);
  
  impl_->heightSpinBox = new DragSpinBox();
  impl_->heightSpinBox->setRange(1, 16384);
  impl_->heightSpinBox->setValue(1080);
  
  impl_->resolutionCombobox_ = new QComboBox();
  impl_->resolutionCombobox_->addItem("1920x1080 (FHD)", QVariant::fromValue(QSize(1920, 1080)));
  impl_->resolutionCombobox_->addItem("1280x720 (HD)", QVariant::fromValue(QSize(1280, 720)));
  impl_->resolutionCombobox_->addItem("3840x2160 (4K)", QVariant::fromValue(QSize(3840, 2160)));
  impl_->resolutionCombobox_->addItem("カスタム", QVariant::fromValue(QSize(-1, -1)));

  impl_->bgColorButton = new QPushButton();
  impl_->bgColorButton->setFixedSize(60, 24);
  impl_->bgColorButton->setStyleSheet("background-color: rgb(255, 255, 255); border: 1px solid #555;");

  impl_->matchCompButton = new QPushButton("コンポジションサイズに合わせる");

  auto vboxLayout = new QFormLayout();
  vboxLayout->addRow("プリセット:", impl_->resolutionCombobox_);
  vboxLayout->addRow("幅 (px):", impl_->widthSpinBox);
  vboxLayout->addRow("高さ (px):", impl_->heightSpinBox);
  vboxLayout->addRow("", impl_->matchCompButton);
  vboxLayout->addRow("カラー:", impl_->bgColorButton);
  setLayout(vboxLayout);

  QObject::connect(impl_->resolutionCombobox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
    QSize sz = impl_->resolutionCombobox_->currentData().toSize();
    if (sz.width() > 0 && sz.height() > 0) {
      impl_->widthSpinBox->setValue(sz.width());
      impl_->heightSpinBox->setValue(sz.height());
    }
  });

  // valueChanged(int) passes an int argument; accept it and ignore inside the handler
  auto forceCustom = [this](int) {
      impl_->resolutionCombobox_->blockSignals(true);
      impl_->resolutionCombobox_->setCurrentIndex(impl_->resolutionCombobox_->count() - 1);
      impl_->resolutionCombobox_->blockSignals(false);
  };
  QObject::connect(impl_->widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, forceCustom);
  QObject::connect(impl_->heightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, forceCustom);

  QObject::connect(impl_->bgColorButton, &QPushButton::clicked, this, [this]() {
      QColor c = QColorDialog::getColor(impl_->bgColor, this, "平面のカラーを選択");
      if (c.isValid()) {
          impl_->bgColor = c;
          QString style = QString("background-color: %1; border: 1px solid #555;").arg(c.name());
          impl_->bgColorButton->setStyleSheet(style);
      }
  });

  QObject::connect(impl_->matchCompButton, &QPushButton::clicked, this, &PlaneLayerSettingPage::resizeCompositionSize);
 }

 PlaneLayerSettingPage::~PlaneLayerSettingPage()
 {
  delete impl_;
 }

 void PlaneLayerSettingPage::setDefaultFocus()
 {
 }

 void PlaneLayerSettingPage::spouitMode()
 {
 }

 void PlaneLayerSettingPage::resizeCompositionSize()
 {
     auto service = ArtifactProjectService::instance();
     if (service) {
         auto compWeak = service->currentComposition();
         if (auto comp = compWeak.lock()) {
             auto size = comp->settings().compositionSize();
             if (size.width() > 0 && size.height() > 0) {
                 impl_->widthSpinBox->setValue(size.width());
                 impl_->heightSpinBox->setValue(size.height());
                 return;
             }
         }
     }
     impl_->widthSpinBox->setValue(1920);
     impl_->heightSpinBox->setValue(1080);
 }
 
  void PlaneLayerSettingPage::setInitialParams(int p_width, int p_height, const FloatColor& color)
  {
      impl_->widthSpinBox->setValue(p_width);
      impl_->heightSpinBox->setValue(p_height);
      QColor c;
      c.setRgbF(color.r(), color.g(), color.b(), color.a());
      impl_->bgColor = c;
      QString style = QString("background-color: %1; border: 1px solid #555;").arg(c.name());
      impl_->bgColorButton->setStyleSheet(style);
  }

 ArtifactSolidLayerInitParams PlaneLayerSettingPage::getInitParams(const QString& name) const
 {
     ArtifactSolidLayerInitParams params(name);
     params.setWidth(impl_->widthSpinBox->value());
     params.setHeight(impl_->heightSpinBox->value());
     QColor c = impl_->bgColor;
     params.setColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
     return params;
 }
 // ReSharper disable CppUnusedFunction
 W_OBJECT_IMPL(CreateSolidLayerSettingDialog)
  // ReSharper restore CppUnusedFunction
	
  class CreateSolidLayerSettingDialog::Impl
 {
 public:
  EditableLabel* nameEditableLabel = nullptr;
  PlaneLayerSettingPage* settingPage = nullptr;
  QDialogButtonBox* dialogButtonBox = nullptr;
  QPropertyAnimation* m_showAnimation = nullptr;
  QPropertyAnimation* m_hideAnimation = nullptr;
  Impl();
  ~Impl()=default;
 };

 CreateSolidLayerSettingDialog::Impl::Impl()
 {
 }

 CreateSolidLayerSettingDialog::CreateSolidLayerSettingDialog(QWidget* parent /*= nullptr*/) :QDialog(parent),impl_(new Impl())
 {
  setWindowTitle(u8"平面設定");
  setFixedSize(600, 400);
  setWindowFlags(windowFlags() | Qt::Dialog | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_NoChildEventsForParent);
  QVBoxLayout* layout = new QVBoxLayout();

  auto editableLabel = impl_->nameEditableLabel = new EditableLabel();
  editableLabel->setText("平面 1");
  
  auto settingPage = impl_->settingPage = new PlaneLayerSettingPage(this);
  
  auto dialogButtonBox = impl_->dialogButtonBox = new QDialogButtonBox();
  dialogButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  
  layout->addWidget(editableLabel);
  layout->addWidget(settingPage);
  layout->addWidget(dialogButtonBox, 0, Qt::AlignRight);
  setLayout(layout);
  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);

  setStyleSheet(style);
  
  // Connect button box signals to close dialog
  QObject::connect(dialogButtonBox, &QDialogButtonBox::accepted, this, [this]() {
      if (impl_->nameEditableLabel) impl_->nameEditableLabel->finishEdit();
      QString name = impl_->nameEditableLabel ? impl_->nameEditableLabel->text() : "Solid";
      ArtifactSolidLayerInitParams params = impl_->settingPage->getInitParams(name);
      Q_EMIT submit(params);
      accept();
  });
  QObject::connect(dialogButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
 }

 CreateSolidLayerSettingDialog::~CreateSolidLayerSettingDialog()
 {
  delete impl_;
 }

 void CreateSolidLayerSettingDialog::keyPressEvent(QKeyEvent* event)
 {
  QDialog::keyPressEvent(event);
 }

 void CreateSolidLayerSettingDialog::mousePressEvent(QMouseEvent* event)
 {
  QDialog::mousePressEvent(event);
 }

 void CreateSolidLayerSettingDialog::mouseReleaseEvent(QMouseEvent* event)
 {
  QDialog::mouseReleaseEvent(event);
 }

 void CreateSolidLayerSettingDialog::mouseMoveEvent(QMouseEvent* event)
 {
  QDialog::mouseMoveEvent(event);
 }

 W_OBJECT_IMPL(EditPlaneLayerSettingDialog)

 class EditPlaneLayerSettingDialog::Impl
 {
 public:
  EditableLabel* nameEditableLabel = nullptr;
  PlaneLayerSettingPage* settingPage = nullptr;
  QDialogButtonBox* dialogButtonBox = nullptr;
  ArtifactSolid2DLayer* targetLayer = nullptr;
 };

 EditPlaneLayerSettingDialog::EditPlaneLayerSettingDialog(QWidget* parent) :QDialog(parent), impl_(new Impl())
 {
  setWindowTitle(u8"平面設定の編集");
  setFixedSize(600, 400);
  setWindowFlags(windowFlags() | Qt::Dialog | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_NoChildEventsForParent);
  QVBoxLayout* layout = new QVBoxLayout();

  auto editableLabel = impl_->nameEditableLabel = new EditableLabel();
  editableLabel->setText("Solid"); // will be updated when a layer is set
  
  auto settingPage = impl_->settingPage = new PlaneLayerSettingPage(this);
  
  auto dialogButtonBox = impl_->dialogButtonBox = new QDialogButtonBox();
  dialogButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  
  layout->addWidget(editableLabel);
  layout->addWidget(settingPage);
  layout->addWidget(dialogButtonBox, 0, Qt::AlignRight);
  setLayout(layout);
  auto style = getDCCStyleSheetPreset(DccStylePreset::ModoStyle);
  setStyleSheet(style);
  
  QObject::connect(dialogButtonBox, &QDialogButtonBox::accepted, this, [this]() {
      if (impl_->nameEditableLabel) impl_->nameEditableLabel->finishEdit();
      QString name = impl_->nameEditableLabel ? impl_->nameEditableLabel->text() : "Solid";
      ArtifactSolidLayerInitParams params = impl_->settingPage->getInitParams(name);
      
      if (impl_->targetLayer) {
          impl_->targetLayer->setLayerName(name);
          impl_->targetLayer->setSize(params.width(), params.height());
          impl_->targetLayer->setColor(params.color());
      }
      accept();
  });
  QObject::connect(dialogButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
 }

 EditPlaneLayerSettingDialog::~EditPlaneLayerSettingDialog()
 {
  delete impl_;
 }

 void EditPlaneLayerSettingDialog::keyPressEvent(QKeyEvent* event) { QDialog::keyPressEvent(event); }
 void EditPlaneLayerSettingDialog::mousePressEvent(QMouseEvent* event) { QDialog::mousePressEvent(event); }
 void EditPlaneLayerSettingDialog::mouseReleaseEvent(QMouseEvent* event) { QDialog::mouseReleaseEvent(event); }
 void EditPlaneLayerSettingDialog::mouseMoveEvent(QMouseEvent* event) { QDialog::mouseMoveEvent(event); }

 void EditPlaneLayerSettingDialog::showAnimated()
 {
 }

 void EditPlaneLayerSettingDialog::setupEdit(std::shared_ptr<ArtifactSolid2DLayer> layer)
 {
     if (!layer) return;
     impl_->targetLayer = layer.get();
     if (impl_->nameEditableLabel) {
         impl_->nameEditableLabel->setText(layer->layerName());
     }
     if (impl_->settingPage) {
         auto size = layer->sourceSize();
         impl_->settingPage->setInitialParams(size.width, size.height, layer->color());
     }
 }

};

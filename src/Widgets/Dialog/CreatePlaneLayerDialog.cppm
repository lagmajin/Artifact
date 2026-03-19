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
#include <QGuiApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <wobjectimpl.h>
module Artifact.Widgets.CreatePlaneLayerDialog;

import std;
import Widgets.Dialog.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Widgets.Utils.CSS;
import Widgets.EditableLabel;
import DragSpinBox;
import Utils.String.UniString;
import Color.Float;
//import Color.Utils;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Composition.Settings;
import Artifact.Layers.SolidImage;

namespace Artifact {
	
 using namespace ArtifactCore;
 using namespace ArtifactWidgets;

 W_OBJECT_IMPL(PlaneLayerSettingPage)

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
          
          // Suggest name
          FloatColor fc(c.redF(), c.greenF(), c.blueF(), c.alphaF());
          //UniString naturalName = ColorUtils::getNaturalColorName(fc);
         // Q_EMIT colorChanged(naturalName.toQString());
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

 W_OBJECT_IMPL(CreateSolidLayerSettingDialog)
	
  class CreateSolidLayerSettingDialog::Impl
 {
 public:
  EditableLabel* nameEditableLabel = nullptr;
  PlaneLayerSettingPage* settingPage = nullptr;
  QDialogButtonBox* dialogButtonBox = nullptr;
  QPropertyAnimation* m_showAnimation = nullptr;
  QPropertyAnimation* m_hideAnimation = nullptr;
  QPoint m_dragPosition;
  bool m_isDragging = false;
  Impl();
  ~Impl()=default;
 };

 CreateSolidLayerSettingDialog::Impl::Impl()
 {
 }

 CreateSolidLayerSettingDialog::CreateSolidLayerSettingDialog(QWidget* parent /*= nullptr*/) :QDialog(parent),impl_(new Impl())
 {
  setWindowTitle("Plane Layer Settings");
  setFixedSize(520, 460);
  setWindowFlags(windowFlags() | Qt::Dialog | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_NoChildEventsForParent);
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  auto* header = new QWidget(this);
  header->setFixedHeight(50);
  header->setStyleSheet("background-color: #2D2D30; border-bottom: 1px solid #444;");
  auto* headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(15, 0, 15, 0);
  auto* title = new QLabel(QStringLiteral("Plane Layer Settings"), header);
  title->setStyleSheet("color: white; font-weight: bold; font-size: 13px;");
  headerLayout->addWidget(title);
  headerLayout->addStretch();
  mainLayout->addWidget(header);

  auto* content = new QWidget(this);
  auto* contentLayout = new QVBoxLayout(content);
  contentLayout->setContentsMargins(20, 20, 20, 10);
  contentLayout->setSpacing(14);

  auto editableLabel = impl_->nameEditableLabel = new EditableLabel();
  editableLabel->setText("平面 1");
  editableLabel->setStyleSheet("background: #252526; padding: 6px; border-radius: 4px; font-weight: bold;");

  auto* nameRow = new QHBoxLayout();
  auto* nameLabel = new QLabel("Name:", content);
  nameLabel->setFixedWidth(60);
  nameLabel->setStyleSheet("color: #AAA; font-weight: bold;");
  nameRow->addWidget(nameLabel);
  nameRow->addWidget(editableLabel, 1);

  auto settingPage = impl_->settingPage = new PlaneLayerSettingPage(this);
  settingPage->resizeCompositionSize();
  auto* settingFrame = new QWidget(content);
  auto* settingFrameLayout = new QVBoxLayout(settingFrame);
  settingFrameLayout->setContentsMargins(10, 10, 10, 10);
  settingFrameLayout->setSpacing(0);
  settingFrameLayout->addWidget(settingPage);
  settingFrame->setStyleSheet("background-color: #232325; border: 1px solid #3F3F46; border-radius: 4px;");

  contentLayout->addLayout(nameRow);
  contentLayout->addWidget(settingFrame, 1);
  mainLayout->addWidget(content, 1);

  auto* footer = new QWidget(this);
  footer->setStyleSheet("background-color: #252526; border-top: 1px solid #333;");
  auto* footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(15, 10, 15, 10);
  auto* dialogButtonBox = impl_->dialogButtonBox = new QDialogButtonBox();
  dialogButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  if (auto* okBtn = dialogButtonBox->button(QDialogButtonBox::Ok)) {
      okBtn->setFixedSize(80, 28);
      okBtn->setText("OK");
      okBtn->setStyleSheet("background: #007ACC; color: white; border-radius: 4px; font-weight: bold;");
  }
  if (auto* cancelBtn = dialogButtonBox->button(QDialogButtonBox::Cancel)) {
      cancelBtn->setFixedSize(80, 28);
      cancelBtn->setText("Cancel");
      cancelBtn->setStyleSheet("background: #3E3E42; color: #DDD; border-radius: 4px; border: 1px solid #555;");
  }
  footerLayout->addStretch();
  footerLayout->addWidget(dialogButtonBox);
  mainLayout->addWidget(footer);

  setStyleSheet("QDialog { background-color: #1E1E20; border: 1px solid #444; }");
  
  QObject::connect(dialogButtonBox, &QDialogButtonBox::accepted, this, [this]() {
      if (impl_->nameEditableLabel) impl_->nameEditableLabel->finishEdit();
      QString name = impl_->nameEditableLabel ? impl_->nameEditableLabel->text() : "Solid";
      ArtifactSolidLayerInitParams params = impl_->settingPage->getInitParams(name);
      Q_EMIT submit(params);
      accept();
  });
  QObject::connect(dialogButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  QObject::connect(settingPage, &PlaneLayerSettingPage::colorChanged, this, [this](const QString& name) {
      if (impl_->nameEditableLabel) {
          impl_->nameEditableLabel->setText(name);
      }
  });
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
  if (event->button() == Qt::LeftButton) {
   impl_->m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
   impl_->m_isDragging = true;
   event->accept();
   return;
  }
  QDialog::mousePressEvent(event);
}

 void CreateSolidLayerSettingDialog::mouseReleaseEvent(QMouseEvent* event)
 {
  if (impl_->m_isDragging && event->button() == Qt::LeftButton) {
   impl_->m_isDragging = false;
   event->accept();
   return;
  }
  QDialog::mouseReleaseEvent(event);
 }

 void CreateSolidLayerSettingDialog::mouseMoveEvent(QMouseEvent* event)
 {
  if (impl_->m_isDragging && (event->buttons() & Qt::LeftButton)) {
   move(event->globalPosition().toPoint() - impl_->m_dragPosition);
   event->accept();
   return;
  }
  QDialog::mouseMoveEvent(event);
 }

 void CreateSolidLayerSettingDialog::showEvent(QShowEvent* event)
 {
  QDialog::showEvent(event);
  QPoint endPos;
  if (parentWidget()) {
   endPos = parentWidget()->mapToGlobal(parentWidget()->rect().center())
            - QPoint(width() / 2, height() / 2);
  } else {
   endPos = QGuiApplication::primaryScreen()->availableGeometry().center()
            - QPoint(width() / 2, height() / 2);
  }
  endPos += QPoint(0, 32);
  move(endPos);
 }

 W_OBJECT_IMPL(EditPlaneLayerSettingDialog)

 class EditPlaneLayerSettingDialog::Impl
 {
 public:
  EditableLabel* nameEditableLabel = nullptr;
  PlaneLayerSettingPage* settingPage = nullptr;
  QDialogButtonBox* dialogButtonBox = nullptr;
  ArtifactSolidImageLayer* targetLayer = nullptr;
 };

 EditPlaneLayerSettingDialog::EditPlaneLayerSettingDialog(QWidget* parent) :QDialog(parent), impl_(new Impl())
 {
  setWindowTitle(u8"平面設定の編集");
  setFixedSize(600, 400);
  setWindowFlags(windowFlags() | Qt::Dialog | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_NoChildEventsForParent);
  QVBoxLayout* layout = new QVBoxLayout();

  auto editableLabel = impl_->nameEditableLabel = new EditableLabel();
  editableLabel->setText("Solid");
  
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

  QObject::connect(settingPage, &PlaneLayerSettingPage::colorChanged, this, [this](const QString& name) {
      if (impl_->nameEditableLabel) {
          impl_->nameEditableLabel->setText(name);
      }
  });
 }

 EditPlaneLayerSettingDialog::~EditPlaneLayerSettingDialog()
 {
  delete impl_;
 }

 void EditPlaneLayerSettingDialog::keyPressEvent(QKeyEvent* event) { QDialog::keyPressEvent(event); }
void EditPlaneLayerSettingDialog::mousePressEvent(QMouseEvent* event) { QDialog::mousePressEvent(event); }
void EditPlaneLayerSettingDialog::mouseReleaseEvent(QMouseEvent* event) { QDialog::mouseReleaseEvent(event); }
void EditPlaneLayerSettingDialog::mouseMoveEvent(QMouseEvent* event) { QDialog::mouseMoveEvent(event); }
void EditPlaneLayerSettingDialog::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  QPoint endPos;
  if (parentWidget()) {
   const QRect pr = parentWidget()->geometry();
   endPos = pr.center() - rect().center();
  } else {
   endPos = QGuiApplication::primaryScreen()->availableGeometry().center() - rect().center();
  }
  move(endPos);
}

 void EditPlaneLayerSettingDialog::setupEdit(std::shared_ptr<ArtifactSolidImageLayer> layer)
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

module;
#include <utility>
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
#include <QPushButton>
#include <QToolButton>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QSignalBlocker>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QDebug>
#include <QSet>
#include <algorithm>
#include <cmath>

module Dialog.Composition;

import Widgets.Utils.CSS;
import Widgets.EditableLabel;
import DragSpinBox;
import Application.AppSettings;
import Color.Float;
import Artifact.Project.Manager;
import Artifact.Project.Items;
import Artifact.Service.Project;
import Utils.String.UniString;
import FloatColorPickerDialog;

namespace Artifact {

 using namespace ArtifactCore;
 using namespace ArtifactWidgets;

namespace {

int indexForFrameRate(QComboBox *combo, double fps)
{
  if (!combo) {
    return -1;
  }
  for (int i = 0; i < combo->count(); ++i) {
    if (std::abs(combo->itemData(i).toDouble() - fps) < 0.001) {
      return i;
    }
  }
  return -1;
}

int indexForSize(QComboBox *combo, const QSize &size)
{
  if (!combo || !size.isValid()) {
    return -1;
  }
  for (int i = 0; i < combo->count(); ++i) {
    const QSize itemSize = combo->itemData(i).toSize();
    if (itemSize == size) {
      return i;
    }
  }
  return -1;
}

void collectCompositionNames(ProjectItem* item, QSet<QString>& names)
{
 if (!item) {
  return;
 }
 if (item->type() == eProjectItemType::Composition) {
  const QString name = item->name.toQString().trimmed();
  if (!name.isEmpty()) {
   names.insert(name);
  }
 }
 for (auto* child : item->children) {
  collectCompositionNames(child, names);
 }
}

QSet<QString> occupiedCompositionNames()
{
 QSet<QString> names;
 if (auto* service = ArtifactProjectService::instance()) {
  for (auto* root : service->projectItems()) {
   collectCompositionNames(root, names);
  }
 }
 return names;
}

QString makeUniqueSequentialName(QString baseName, const QSet<QString>& occupied)
{
 baseName = baseName.trimmed();
 if (baseName.isEmpty()) {
  baseName = QStringLiteral("Comp1");
 }
 if (!occupied.contains(baseName)) {
  return baseName;
 }

 QString prefix = baseName;
 int startNumber = 2;
 int end = baseName.size();
 while (end > 0 && baseName.at(end - 1).isDigit()) {
  --end;
 }
 if (end < baseName.size()) {
  int start = end;
  while (start > 0 && baseName.at(start - 1).isSpace()) {
   --start;
  }
  bool ok = false;
  const int current = baseName.mid(end).toInt(&ok);
  if (ok) {
   prefix = baseName.left(start);
   startNumber = current + 1;
  }
 }
 if (prefix == baseName && !prefix.endsWith(QLatin1Char(' '))) {
  prefix += QLatin1Char(' ');
 }
 for (int index = startNumber; index < 10000; ++index) {
  const QString candidate = prefix + QString::number(index);
  if (!occupied.contains(candidate)) {
   return candidate;
  }
 }
 return baseName;
}

QString uniqueCompositionName(const QString& baseName)
{
 return makeUniqueSequentialName(baseName, occupiedCompositionNames());
}

void updateColorButtonPreview(QPushButton* button, const QColor& color)
{
 if (!button) {
  return;
 }
 QPixmap pix(button->size().isEmpty() ? QSize(40, 24) : button->size());
 pix.fill(Qt::transparent);
 {
  QPainter painter(&pix);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(QColor(85, 85, 85), 1));
  painter.setBrush(color);
  painter.drawRoundedRect(pix.rect().adjusted(1, 1, -2, -2), 3, 3);
 }
 button->setIcon(QIcon(pix));
 button->setIconSize(pix.size());
 button->setToolTip(QStringLiteral("Background Color: %1").arg(color.name(QColor::HexArgb)));
 button->setText(QString());
}

} // namespace
	
 class CompositionSettingPage::Impl
 {
 public:
  Impl();
  QComboBox* resolutionCombobox_ = nullptr;
  QComboBox* fpsCombo_ = nullptr;
  QComboBox* pixelAspectCombo_ = nullptr;
  DragSpinBox* widthSpinBox = nullptr;
  DragSpinBox* heightSpinBox = nullptr;
  QToolButton* aspectLockButton_ = nullptr;
  DoubleDragSpinBox* durationSpinBox = nullptr;
  QLineEdit* startTimecodeEdit = nullptr;
  QPushButton* bgColorButton = nullptr;
  QColor bgColor = QColor(0, 0, 0, 255);
  bool aspectRatioLocked_ = false;
  double lockedAspectRatio_ = 16.0 / 9.0;
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
      QComboBox:hover, QLineEdit:focus { border: 1px solid #D47D32; }
  )";

  impl_->widthSpinBox = new DragSpinBox();
  impl_->widthSpinBox->setRange(1, 16384);
  impl_->heightSpinBox = new DragSpinBox();
  impl_->heightSpinBox->setRange(1, 16384);
  
  impl_->resolutionCombobox_ = new QComboBox();
  impl_->resolutionCombobox_->addItem("HD 1080p (1920x1080)", QVariant::fromValue(QSize(1920, 1080)));
  impl_->resolutionCombobox_->addItem("HD 720p (1280x720)", QVariant::fromValue(QSize(1280, 720)));
  impl_->resolutionCombobox_->addItem("4K UHD (3840x2160)", QVariant::fromValue(QSize(3840, 2160)));
  impl_->resolutionCombobox_->addItem("4K DCI (4096x2160)", QVariant::fromValue(QSize(4096, 2160)));
  impl_->resolutionCombobox_->addItem("2K DCI (2048x1080)", QVariant::fromValue(QSize(2048, 1080)));
  impl_->resolutionCombobox_->insertSeparator(impl_->resolutionCombobox_->count());
  impl_->resolutionCombobox_->addItem("Instagram/TikTok (1080x1920)", QVariant::fromValue(QSize(1080, 1920)));
  impl_->resolutionCombobox_->addItem("Instagram Square (1080x1080)", QVariant::fromValue(QSize(1080, 1080)));
  impl_->resolutionCombobox_->insertSeparator(impl_->resolutionCombobox_->count());
  impl_->resolutionCombobox_->addItem("SD PAL (720x576)", QVariant::fromValue(QSize(720, 576)));
  impl_->resolutionCombobox_->addItem("SD NTSC (720x480)", QVariant::fromValue(QSize(720, 480)));
  impl_->resolutionCombobox_->insertSeparator(impl_->resolutionCombobox_->count());
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
  impl_->aspectLockButton_ = new QToolButton();
  impl_->aspectLockButton_->setCheckable(true);
  impl_->aspectLockButton_->setAutoRaise(true);
  impl_->aspectLockButton_->setToolTip(QStringLiteral("Lock aspect ratio"));
  impl_->aspectLockButton_->setText(QStringLiteral("🔓"));
  sizeHBox->addWidget(impl_->aspectLockButton_);
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
  updateColorButtonPreview(impl_->bgColorButton, impl_->bgColor);
  
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
      const QSignalBlocker widthBlock(impl_->widthSpinBox);
      const QSignalBlocker heightBlock(impl_->heightSpinBox);
      impl_->widthSpinBox->setValue(sz.width());
      impl_->heightSpinBox->setValue(sz.height());
      if (sz.height() > 0) {
        impl_->lockedAspectRatio_ = static_cast<double>(sz.width()) / static_cast<double>(sz.height());
      }
    }
  });

  auto forceCustom = [this]() {
      if (impl_->resolutionCombobox_->currentData().toSize().width() != -1) {
          impl_->resolutionCombobox_->blockSignals(true);
          impl_->resolutionCombobox_->setCurrentIndex(impl_->resolutionCombobox_->count() - 1);
          impl_->resolutionCombobox_->blockSignals(false);
      }
  };

  auto updateHeightFromWidth = [this](int width) {
      if (!impl_->aspectRatioLocked_ || !impl_->heightSpinBox) {
          return;
      }
      const double ratio = impl_->lockedAspectRatio_ > 0.0 ? impl_->lockedAspectRatio_ : 1.0;
      const int newHeight = std::max(1, static_cast<int>(std::round(static_cast<double>(width) / ratio)));
      const QSignalBlocker block(impl_->heightSpinBox);
      impl_->heightSpinBox->setValue(newHeight);
  };

  auto updateWidthFromHeight = [this](int height) {
      if (!impl_->aspectRatioLocked_ || !impl_->widthSpinBox) {
          return;
      }
      const double ratio = impl_->lockedAspectRatio_ > 0.0 ? impl_->lockedAspectRatio_ : 1.0;
      const int newWidth = std::max(1, static_cast<int>(std::round(static_cast<double>(height) * ratio)));
      const QSignalBlocker block(impl_->widthSpinBox);
      impl_->widthSpinBox->setValue(newWidth);
  };

  QObject::connect(impl_->widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, forceCustom, updateHeightFromWidth](int value) {
      forceCustom();
      updateHeightFromWidth(value);
  });
  QObject::connect(impl_->heightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, forceCustom, updateWidthFromHeight](int value) {
      forceCustom();
      updateWidthFromHeight(value);
  });

  QObject::connect(impl_->aspectLockButton_, &QToolButton::toggled, this, [this](bool locked) {
      impl_->aspectRatioLocked_ = locked;
      if (locked) {
          const int width = std::max(1, impl_->widthSpinBox ? impl_->widthSpinBox->value() : 1);
          const int height = std::max(1, impl_->heightSpinBox ? impl_->heightSpinBox->value() : 1);
          impl_->lockedAspectRatio_ = static_cast<double>(width) / static_cast<double>(height);
      }
      if (impl_->aspectLockButton_) {
          impl_->aspectLockButton_->setText(locked ? QStringLiteral("🔒") : QStringLiteral("🔓"));
      }
  });

  QObject::connect(impl_->bgColorButton, &QPushButton::clicked, this, [this]() {
      ArtifactWidgets::FloatColorPicker picker(this);
      picker.setWindowTitle(QStringLiteral("Background Color"));
      picker.setInitialColor(FloatColor(impl_->bgColor.redF(),
                                        impl_->bgColor.greenF(),
                                        impl_->bgColor.blueF(),
                                        impl_->bgColor.alphaF()));
      if (picker.exec() != QDialog::Accepted) {
          return;
      }

      const FloatColor picked = picker.getColor();
      QColor c = QColor::fromRgbF(picked.r(), picked.g(), picked.b(), picked.a());
      if (c.isValid()) {
          impl_->bgColor = c;
          updateColorButtonPreview(impl_->bgColorButton, c);
      }
  });

  if (auto *settings = ArtifactCore::ArtifactAppSettings::instance()) {
    const int defaultWidth = settings->projectDefaultCompositionWidth();
    const int defaultHeight = settings->projectDefaultCompositionHeight();
    const QSize defaultSize(defaultWidth, defaultHeight);
    const int presetIndex = indexForSize(impl_->resolutionCombobox_, defaultSize);
    if (presetIndex >= 0) {
      impl_->resolutionCombobox_->setCurrentIndex(presetIndex);
    }
    if (defaultWidth > 0) {
      impl_->widthSpinBox->setValue(defaultWidth);
    }
    if (defaultHeight > 0) {
      impl_->heightSpinBox->setValue(defaultHeight);
    }
    const int fpsIndex =
        indexForFrameRate(impl_->fpsCombo_, settings->projectDefaultCompositionFrameRate());
    if (fpsIndex >= 0) {
      impl_->fpsCombo_->setCurrentIndex(fpsIndex);
    }
    const QColor defaultBg(
        settings->projectDefaultCompositionBackgroundColor());
    if (defaultBg.isValid()) {
      impl_->bgColor = defaultBg;
      updateColorButtonPreview(impl_->bgColorButton, defaultBg);
    }
  }

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
  ArtifactCompositionInitParams acceptedParams_;
  bool accepted_ = false;

  void ok(QDialog* dialog);
  void cancel(QDialog* dialog);
 };

 CreateCompositionDialog::Impl::Impl(CreateCompositionDialog* pDialog) {}

 void CreateCompositionDialog::Impl::ok(QDialog* dialog)
 {
  if (okCalled_) return;
  okCalled_ = true;
 if (compositionNameEdit_) compositionNameEdit_->finishEdit();
  QString name = compositionNameEdit_ ? compositionNameEdit_->text().trimmed() : QString();
  if (name.isEmpty()) {
    name = uniqueCompositionName(QStringLiteral("Comp1"));
  }
  if (compositionSettingPage_) {
    acceptedParams_ = compositionSettingPage_->getInitParams(name);
    accepted_ = true;
  }
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
  auto hLayout = new QHBoxLayout(header);
  hLayout->setContentsMargins(15, 0, 15, 0);
  auto title = new QLabel("Composition Settings");
  hLayout->addWidget(title);
  hLayout->addStretch();
  mainLayout->addWidget(header);

  // Content Area
  auto content = new QVBoxLayout();
  content->setContentsMargins(20, 20, 20, 10);
  content->setSpacing(15);

  auto nameRow = new QHBoxLayout();
  auto nameLbl = new QLabel("Name:");
  nameLbl->setFixedWidth(60);
  impl_->compositionNameEdit_ = new EditableLabel();
  impl_->compositionNameEdit_->setText(uniqueCompositionName(QStringLiteral("Comp1")));
  nameRow->addWidget(nameLbl);
  nameRow->addWidget(impl_->compositionNameEdit_);
  content->addLayout(nameRow);

  impl_->pTabWidget = new QTabWidget();
  impl_->compositionSettingPage_ = new CompositionSettingPage();
  impl_->pTabWidget->addTab(impl_->compositionSettingPage_, "Basic");
  content->addWidget(impl_->pTabWidget);
  mainLayout->addLayout(content);

  // Footer / Buttons
  auto footer = new QWidget();
  auto fLayout = new QHBoxLayout(footer);
  fLayout->setContentsMargins(15, 10, 15, 10);
  auto okBtn = new QPushButton("OK");
  okBtn->setFixedSize(80, 28);
  auto cancelBtn = new QPushButton("Cancel");
  cancelBtn->setFixedSize(80, 28);
  fLayout->addStretch();
  fLayout->addWidget(okBtn);
  fLayout->addWidget(cancelBtn);
  mainLayout->addWidget(footer);

  QObject::connect(okBtn, &QPushButton::clicked, this, [this]() { impl_->ok(this); });
  QObject::connect(cancelBtn, &QPushButton::clicked, this, [this]() { impl_->cancel(this); });

 }

 CreateCompositionDialog::~CreateCompositionDialog() { delete impl_; }

 void CreateCompositionDialog::setCompositionName(const QString& name) { if (impl_->compositionNameEdit_) impl_->compositionNameEdit_->setText(name); }
 QString CreateCompositionDialog::compositionName() const { return impl_->compositionNameEdit_ ? impl_->compositionNameEdit_->text() : ""; }
 ArtifactCompositionInitParams CreateCompositionDialog::acceptedInitParams() const
 {
  return impl_ ? impl_->acceptedParams_ : ArtifactCompositionInitParams();
 }

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
   endPos = parentWidget()->mapToGlobal(parentWidget()->rect().center())
            - QPoint(width() / 2, height() / 2);
  } else {
   endPos = QGuiApplication::primaryScreen()->availableGeometry().center()
            - QPoint(width() / 2, height() / 2);
  }
  move(endPos);
 }

 void CreateCompositionDialog::closeEvent(QCloseEvent* event) { QDialog::closeEvent(event); }
 void CreateCompositionDialog::setDefaultFocus() {}
}

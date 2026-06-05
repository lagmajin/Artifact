module;
#include <utility>
#include <algorithm>
#include <QColor>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPalette>
#include <QPointF>
#include <QRectF>
#include <QSpinBox>
#include <QSize>
#include <QVBoxLayout>
#include <QTimer>
#include <numeric>
#include <wobjectimpl.h>

module Menu.Composition;
import std;

import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Composition.Abstract;
import Utils.Path;
import Artifact.Composition.InitParams;
import Artifact.Layer.InitParams;
import Artifact.Widgets.SoftwareRenderInspectors;
import Dialog.Composition;
import FloatColorPickerDialog;
import Artifact.Widgets.AppDialogs;
import Widgets.Utils.CSS;

namespace Artifact {
using namespace ArtifactCore;

namespace {

QString aspectRatioLabel(const QSize& size)
{
 if (size.width() > 0 && size.height() > 0) {
  const int divisor = std::gcd(size.width(), size.height());
  if (divisor > 0) {
   return QStringLiteral("%1:%2")
    .arg(size.width() / divisor)
    .arg(size.height() / divisor);
  }
 }
 return QStringLiteral("custom");
}

ResponsiveLayoutVariant makeResponsiveVariant(const QString& id,
                                              const QString& name,
                                              const QSize& size,
                                              const QString& guidePreset)
{
 ResponsiveLayoutVariant variant;
 variant.variantId = id;
 variant.displayName = name;
 variant.baseSize = size.isValid() ? size : QSize(1920, 1080);
 variant.aspectRatio = variant.baseSize.height() > 0
  ? static_cast<qreal>(variant.baseSize.width()) /
    static_cast<qreal>(variant.baseSize.height())
  : 0.0;
 variant.safeArea = QRectF(0.0, 0.0, 1.0, 1.0);
 variant.contentAnchor = QPointF(0.5, 0.5);
 variant.layoutRules.insert(QStringLiteral("scaleMode"), QStringLiteral("fit"));
 variant.layoutRules.insert(QStringLiteral("cropMode"), QStringLiteral("none"));
 variant.layoutRules.insert(QStringLiteral("guidePreset"), guidePreset);
 variant.enabled = true;
 return variant;
}

QString responsiveVariantLabel(const ResponsiveLayoutVariant& variant)
{
 const QString name = variant.displayName.isEmpty() ? variant.variantId
                                                    : variant.displayName;
 const QString sizeLabel = variant.baseSize.isValid()
  ? QStringLiteral("%1x%2").arg(variant.baseSize.width()).arg(variant.baseSize.height())
  : QStringLiteral("custom");
 const QString ratioLabel = aspectRatioLabel(variant.baseSize);
 return QStringLiteral("%1  (%2, %3)").arg(name, sizeLabel, ratioLabel);
}

ResponsiveLayoutSet normalizedResponsiveLayoutForDialog(const ArtifactCompositionPtr& current)
{
 ResponsiveLayoutSet layout = current ? current->responsiveLayout() : ResponsiveLayoutSet{};
 const QSize currentSize = current ? current->settings().compositionSize() : QSize(1920, 1080);
 if (layout.variants.isEmpty()) {
  layout.variants.append(makeResponsiveVariant(QStringLiteral("default"),
                                               QStringLiteral("Default"),
                                               currentSize.isValid() ? currentSize : QSize(1920, 1080),
                                               QStringLiteral("default")));
 }

 const auto hasVariant = [&layout](const QString& id) {
  for (const auto& variant : layout.variants) {
   if (variant.variantId == id) {
    return true;
   }
  }
  return false;
 };

 if (!hasVariant(QStringLiteral("layout_16_9"))) {
  layout.variants.append(makeResponsiveVariant(QStringLiteral("layout_16_9"),
                                               QStringLiteral("16:9"),
                                               QSize(1920, 1080),
                                               QStringLiteral("safeFrame")));
 }
 if (!hasVariant(QStringLiteral("layout_9_16"))) {
  layout.variants.append(makeResponsiveVariant(QStringLiteral("layout_9_16"),
                                               QStringLiteral("9:16"),
                                               QSize(1080, 1920),
                                               QStringLiteral("safeFrame")));
 }
 if (!hasVariant(QStringLiteral("layout_1_1"))) {
  layout.variants.append(makeResponsiveVariant(QStringLiteral("layout_1_1"),
                                               QStringLiteral("1:1"),
                                               QSize(1080, 1080),
                                               QStringLiteral("centerSquare")));
 }

 if (layout.activeVariantId.isEmpty() || !layout.hasVariant(layout.activeVariantId)) {
  layout.activeVariantId = layout.variants.front().variantId;
 }
 return layout;
}

} // namespace

W_OBJECT_IMPL(ArtifactCompositionMenu)

class ArtifactCompositionMenu::Impl {
public:
 Impl(ArtifactCompositionMenu* menu, QWidget* mainWindow);
 ~Impl() = default;

 ArtifactCompositionMenu* menu_ = nullptr;
 QWidget* mainWindow_ = nullptr;
 QAction* createAction = nullptr;
 QMenu* presetMenu = nullptr;
 QAction* presetHdAction = nullptr;
 QAction* preset4kAction = nullptr;
 QAction* presetVerticalAction = nullptr;
 QAction* duplicateAction = nullptr;
 QAction* renameAction = nullptr;
 QAction* deleteAction = nullptr;
 QAction* settingsAction = nullptr;
 QAction* colorAction = nullptr;

 void showCreate();
 void createFromPreset(const ArtifactCompositionInitParams& params);
 void duplicateCurrent();
 void renameCurrent();
 void removeCurrent();
 void showSettings();
 void showColor();
};

ArtifactCompositionMenu::Impl::Impl(ArtifactCompositionMenu* menu, QWidget* mainWindow)
 : menu_(menu), mainWindow_(mainWindow)
{
 createAction = new QAction("新規コンポジション(&N)...");
 createAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
 createAction->setIcon(QIcon(resolveIconPath("Studio/compositionmenu_new.svg")));

 presetMenu = new QMenu("プリセットから作成(&P)", menu);
 presetMenu->setIcon(QIcon(resolveIconPath("Studio/compositionmenu_presets.svg")));
 presetHdAction = presetMenu->addAction("HD 1080p 30fps");
 presetHdAction->setIcon(QIcon(resolveIconPath("Studio/compositionmenu_preset_hd.svg")));
 preset4kAction = presetMenu->addAction("4K UHD 30fps");
 preset4kAction->setIcon(QIcon(resolveIconPath("Studio/compositionmenu_preset_4k.svg")));
 presetVerticalAction = presetMenu->addAction("Vertical 1080x1920 30fps");
 presetVerticalAction->setIcon(QIcon(resolveIconPath("Studio/compositionmenu_preset_vertical.svg")));

 duplicateAction = new QAction("コンポジションを複製(&D)");
 duplicateAction->setIcon(QIcon(resolveIconPath("Studio/compositionmenu_duplicate.svg")));
 renameAction = new QAction("名前を変更(&R)...");
 renameAction->setIcon(QIcon(resolveIconPath("Studio/compositionmenu_rename.svg")));
 deleteAction = new QAction("コンポジションを削除(&X)...");
 deleteAction->setIcon(QIcon(resolveIconPath("Studio/compositionmenu_delete.svg")));

 settingsAction = new QAction("設定 (&S)...", menu);
 settingsAction->setIcon(QIcon(resolveIconPath("Studio/compositionmenu_settings.svg")));

 colorAction = new QAction("背景色(&B)...");
 colorAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
 colorAction->setIcon(QIcon(resolveIconPath("Studio/compositionmenu_background.svg")));

 menu->addAction(createAction);
 menu->addMenu(presetMenu);
 menu->addSeparator();
 menu->addAction(duplicateAction);
 menu->addAction(renameAction);
 menu->addAction(deleteAction);
 menu->addSeparator();
 menu->addAction(settingsAction);
 menu->addSeparator();
 menu->addAction(colorAction);

 QObject::connect(createAction, &QAction::triggered, menu, [this]() { showCreate(); });
 QObject::connect(presetHdAction, &QAction::triggered, menu, [this]() { createFromPreset(ArtifactCompositionInitParams::hdPreset()); });
 QObject::connect(preset4kAction, &QAction::triggered, menu, [this]() { createFromPreset(ArtifactCompositionInitParams::fourKPreset()); });
 QObject::connect(presetVerticalAction, &QAction::triggered, menu, [this]() { createFromPreset(ArtifactCompositionInitParams::verticalPreset()); });
 QObject::connect(duplicateAction, &QAction::triggered, menu, [this]() { duplicateCurrent(); });
 QObject::connect(renameAction, &QAction::triggered, menu, [this]() { renameCurrent(); });
 QObject::connect(deleteAction, &QAction::triggered, menu, [this]() { removeCurrent(); });
 QObject::connect(settingsAction, &QAction::triggered, menu, [this]() { showSettings(); });
 QObject::connect(colorAction, &QAction::triggered, menu, [this]() { showColor(); });
}

void ArtifactCompositionMenu::Impl::showCreate()
{
 auto dialog = new CreateCompositionDialog(mainWindow_);
 if (dialog->exec()) {
  const ArtifactCompositionInitParams params = dialog->acceptedInitParams();
  QTimer::singleShot(0, mainWindow_ ? mainWindow_ : menu_, [params]() {
   if (auto* service = ArtifactProjectService::instance()) {
    service->createComposition(params);
   }
  });
 }
 dialog->deleteLater();
}

void ArtifactCompositionMenu::Impl::createFromPreset(const ArtifactCompositionInitParams& params)
{
 auto* service = ArtifactProjectService::instance();
 if (!service) {
  QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
   "Composition",
   "プロジェクトサービスが利用できません。");
  return;
 }
 service->createComposition(params);
}

void ArtifactCompositionMenu::Impl::duplicateCurrent()
{
 auto* service = ArtifactProjectService::instance();
 auto current = service->currentComposition().lock();
 if (!current) {
  return;
 }

 if (!service->duplicateComposition(current->id())) {
  QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
   "Composition",
   "コンポジションの複製に失敗しました。");
 }
}

void ArtifactCompositionMenu::Impl::renameCurrent()
{
 auto* service = ArtifactProjectService::instance();
 auto current = service->currentComposition().lock();
 if (!current) {
  return;
 }

 bool ok = false;
 const QString newName = QInputDialog::getText(
  mainWindow_ ? mainWindow_ : menu_,
  "コンポジション名の変更",
  "新しい名前:",
  QLineEdit::Normal,
  current->settings().compositionName().toQString().trimmed(),
  &ok);
 if (!ok) {
  return;
 }
 const QString trimmed = newName.trimmed();
 if (trimmed.isEmpty()) {
  return;
 }
 if (!service->renameComposition(current->id(), UniString(trimmed))) {
  QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
   "Composition",
   "コンポジション名の変更に失敗しました。");
 }
}

void ArtifactCompositionMenu::Impl::removeCurrent()
{
 auto* service = ArtifactProjectService::instance();
 auto current = service->currentComposition().lock();
 if (!current) {
  return;
 }

 const QString message = service->compositionRemovalConfirmationMessage(current->id());

 if (!ArtifactMessageBox::confirmDelete(mainWindow_ ? mainWindow_ : menu_, "コンポジション削除", message)) {
  return;
 }

 if (!service->removeCompositionWithRenderQueueCleanup(current->id())) {
  QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
   "Composition",
  "コンポジションの削除に失敗しました。");
 }
}

void ArtifactCompositionMenu::Impl::showSettings()
{
 auto* service = ArtifactProjectService::instance();
 if (!service) {
  QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
                       QStringLiteral("Composition"),
                       QStringLiteral("プロジェクトサービスが利用できません。"));
  return;
 }

 auto current = service->currentComposition().lock();
 if (!current) {
  QMessageBox::information(mainWindow_ ? mainWindow_ : menu_,
                           QStringLiteral("Composition"),
                           QStringLiteral("設定するコンポジションがありません。"));
  return;
 }

 QDialog dialog(mainWindow_ ? mainWindow_ : menu_);
 dialog.setWindowTitle(QStringLiteral("Composition Settings"));
 dialog.setModal(true);
 dialog.resize(420, 320);

 auto* layout = new QVBoxLayout(&dialog);
 layout->setContentsMargins(12, 12, 12, 12);
 layout->setSpacing(10);

 auto* nameLabel = new QLabel(QStringLiteral("Name"), &dialog);
 auto* nameEdit = new QLineEdit(current->settings().compositionName().toQString(), &dialog);
 layout->addWidget(nameLabel);
 layout->addWidget(nameEdit);

 const QSize initialSize = current->effectiveCompositionSize();
 auto* sizeLabel = new QLabel(QStringLiteral("Size"), &dialog);
 layout->addWidget(sizeLabel);
 auto* sizeLayout = new QHBoxLayout();
 auto* widthSpin = new QSpinBox(&dialog);
 widthSpin->setRange(1, 32768);
 widthSpin->setValue(std::max(1, initialSize.width()));
 auto* heightSpin = new QSpinBox(&dialog);
 heightSpin->setRange(1, 32768);
 heightSpin->setValue(std::max(1, initialSize.height()));
 sizeLayout->addWidget(new QLabel(QStringLiteral("Width"), &dialog));
 sizeLayout->addWidget(widthSpin);
 sizeLayout->addWidget(new QLabel(QStringLiteral("Height"), &dialog));
 sizeLayout->addWidget(heightSpin);
 layout->addLayout(sizeLayout);

 auto* responsiveLabel = new QLabel(QStringLiteral("Responsive Layout"), &dialog);
 layout->addWidget(responsiveLabel);
 auto* responsiveCombo = new QComboBox(&dialog);
 const ResponsiveLayoutSet previewLayout = normalizedResponsiveLayoutForDialog(current);
 for (const auto& variant : previewLayout.variants) {
  responsiveCombo->addItem(responsiveVariantLabel(variant), variant.variantId);
  if (variant.variantId == previewLayout.activeVariantId) {
   responsiveCombo->setCurrentIndex(responsiveCombo->count() - 1);
  }
 }
 layout->addWidget(responsiveCombo);
 auto* responsiveHint = new QLabel(
  QStringLiteral("Select the active layout variant for this composition."), &dialog);
 {
  QPalette pal = responsiveHint->palette();
  pal.setColor(QPalette::WindowText,
               QColor(ArtifactCore::currentDCCTheme().textColor).darker(140));
  responsiveHint->setPalette(pal);
 }
 layout->addWidget(responsiveHint);

 auto* fpsLabel = new QLabel(QStringLiteral("Frame Rate"), &dialog);
 layout->addWidget(fpsLabel);
 auto* fpsSpin = new QDoubleSpinBox(&dialog);
 fpsSpin->setRange(1.0, 240.0);
 fpsSpin->setDecimals(3);
 fpsSpin->setSingleStep(0.5);
 fpsSpin->setValue(std::max(1.0, static_cast<double>(current->frameRate().framerate())));
 layout->addWidget(fpsSpin);

 const FrameRange currentRange = current->frameRange().normalized();
 auto* rangeLabel = new QLabel(QStringLiteral("Frame Range"), &dialog);
 layout->addWidget(rangeLabel);
 auto* rangeLayout = new QHBoxLayout();
 auto* startSpin = new QSpinBox(&dialog);
 startSpin->setRange(-1000000, 1000000);
 startSpin->setValue(static_cast<int>(currentRange.start()));
 auto* endSpin = new QSpinBox(&dialog);
 endSpin->setRange(-1000000, 1000000);
 endSpin->setValue(static_cast<int>(currentRange.end()));
 rangeLayout->addWidget(new QLabel(QStringLiteral("Start"), &dialog));
 rangeLayout->addWidget(startSpin);
 rangeLayout->addWidget(new QLabel(QStringLiteral("End"), &dialog));
 rangeLayout->addWidget(endSpin);
 layout->addLayout(rangeLayout);

 const QColor originalBackgroundColor = QColor::fromRgbF(current->backgroundColor().r(),
                                                         current->backgroundColor().g(),
                                                         current->backgroundColor().b(),
                                                         current->backgroundColor().a());
 auto backgroundColor = originalBackgroundColor;
 auto* bgRow = new QHBoxLayout();
 auto* bgLabel = new QLabel(QStringLiteral("Background"), &dialog);
 auto* bgButton = new QPushButton(QStringLiteral("Change..."), &dialog);
 auto* bgPreview = new QLabel(&dialog);
 bgPreview->setMinimumWidth(96);
 bgPreview->setAutoFillBackground(true);
 auto updateBgPreview = [&]() {
  QPalette pal = bgPreview->palette();
  pal.setColor(QPalette::Window, backgroundColor);
  pal.setColor(QPalette::WindowText, backgroundColor);
  bgPreview->setPalette(pal);
  bgPreview->setText(backgroundColor.name(QColor::HexArgb));
 };
 updateBgPreview();
 QObject::connect(bgButton, &QPushButton::clicked, &dialog, [&]() {
  ArtifactWidgets::FloatColorPicker picker(&dialog);
  picker.setWindowTitle(QStringLiteral("Background Color"));
  picker.setColor(FloatColor(backgroundColor.redF(),
                             backgroundColor.greenF(),
                             backgroundColor.blueF(),
                             backgroundColor.alphaF()));
  picker.setInitialColor(FloatColor(backgroundColor.redF(),
                                    backgroundColor.greenF(),
                                    backgroundColor.blueF(),
                                    backgroundColor.alphaF()));
  QObject::connect(&picker, &ArtifactWidgets::FloatColorPicker::colorChanged,
                   &dialog, [&](const FloatColor& picked) {
    backgroundColor = QColor::fromRgbF(picked.r(), picked.g(), picked.b(), picked.a());
    current->setBackGroundColor(FloatColor(
        backgroundColor.redF(),
        backgroundColor.greenF(),
        backgroundColor.blueF(),
        backgroundColor.alphaF()));
    updateBgPreview();
  });
  if (picker.exec() == QDialog::Accepted) {
   const FloatColor picked = picker.getColor();
   backgroundColor = QColor::fromRgbF(picked.r(), picked.g(), picked.b(), picked.a());
   updateBgPreview();
  } else {
   backgroundColor = originalBackgroundColor;
   current->setBackGroundColor(FloatColor(backgroundColor.redF(),
                                          backgroundColor.greenF(),
                                          backgroundColor.blueF(),
                                          backgroundColor.alphaF()));
   updateBgPreview();
  }
 });
 bgRow->addWidget(bgLabel);
 bgRow->addWidget(bgButton);
 bgRow->addWidget(bgPreview);
 bgRow->addStretch();
 layout->addLayout(bgRow);

 auto* infoLabel = new QLabel(QStringLiteral("ID: %1").arg(current->id().toString()), &dialog);
 {
  QPalette pal = infoLabel->palette();
  pal.setColor(QPalette::WindowText,
               QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
  infoLabel->setPalette(pal);
 }
 layout->addWidget(infoLabel);

 auto* buttons =
     new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
 layout->addWidget(buttons);

 QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
  const QString trimmedName = nameEdit->text().trimmed();
  if (trimmedName.isEmpty()) {
   QMessageBox::warning(&dialog,
                        QStringLiteral("Composition Settings"),
                        QStringLiteral("Name must not be empty."));
   return;
  }

  const int startFrame = startSpin->value();
  const int endFrame = endSpin->value();
  if (startFrame > endFrame) {
   QMessageBox::warning(&dialog,
                        QStringLiteral("Composition Settings"),
                        QStringLiteral("Start frame must be less than or equal to end frame."));
   return;
  }

  current->setCompositionName(UniString::fromQString(trimmedName));
  ResponsiveLayoutSet responsiveLayout = normalizedResponsiveLayoutForDialog(current);
  const QString selectedVariantId = responsiveCombo->currentData().toString();
  const QSize requestedSize(std::max(1, widthSpin->value()),
                            std::max(1, heightSpin->value()));
  QSize chosenSize = requestedSize;
  for (auto& variant : responsiveLayout.variants) {
   if (variant.variantId == selectedVariantId) {
    if (requestedSize == initialSize && variant.baseSize.isValid()) {
     chosenSize = variant.baseSize;
    }
    variant.baseSize = chosenSize;
    variant.aspectRatio = requestedSize.height() > 0
     ? static_cast<qreal>(chosenSize.width()) /
       static_cast<qreal>(chosenSize.height())
     : 0.0;
    break;
   }
  }
  if (!selectedVariantId.isEmpty() && responsiveLayout.hasVariant(selectedVariantId)) {
   responsiveLayout.activeVariantId = selectedVariantId;
  }
  current->setResponsiveLayout(responsiveLayout);
  current->setFrameRate(FrameRate(static_cast<float>(fpsSpin->value())));
  current->setFrameRange(FrameRange(FramePosition(startFrame), FramePosition(endFrame)));
 current->setBackGroundColor(FloatColor(backgroundColor.redF(),
                                         backgroundColor.greenF(),
                                         backgroundColor.blueF(),
                                         backgroundColor.alphaF()));

  if (!service->renameComposition(current->id(), UniString::fromQString(trimmedName))) {
   QMessageBox::warning(&dialog,
                        QStringLiteral("Composition Settings"),
                        QStringLiteral("Failed to update composition name."));
   return;
  }

  if (auto project = service->getCurrentProjectSharedPtr()) {
   project->projectChanged();
  }
  if (auto* playback = ArtifactPlaybackService::instance()) {
   playback->setFrameRange(current->frameRange());
   playback->setFrameRate(current->frameRate());
  }

 dialog.accept();
 });
 QObject::connect(&dialog, &QDialog::rejected, &dialog, [&]() {
  current->setBackGroundColor(FloatColor(originalBackgroundColor.redF(),
                                         originalBackgroundColor.greenF(),
                                         originalBackgroundColor.blueF(),
                                         originalBackgroundColor.alphaF()));
 });
 QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

 dialog.exec();
}

void ArtifactCompositionMenu::Impl::showColor()
{
 auto service = ArtifactProjectService::instance();
 if (auto comp = service->currentComposition().lock()) {
  ArtifactWidgets::FloatColorPicker picker(mainWindow_ ? mainWindow_ : menu_);
  picker.setWindowTitle(QStringLiteral("Background Color"));
  picker.setInitialColor(FloatColor(comp->backgroundColor().r(),
                                    comp->backgroundColor().g(),
                                    comp->backgroundColor().b(),
                                    comp->backgroundColor().a()));
  if (picker.exec() == QDialog::Accepted) {
   const FloatColor picked = picker.getColor();
   comp->setBackGroundColor(
       FloatColor(picked.r(), picked.g(), picked.b(), picked.a()));
  }
 }
}

ArtifactCompositionMenu::ArtifactCompositionMenu(QWidget* mainWindow, QWidget* parent)
 : QMenu(parent), impl_(new Impl(this, mainWindow))
{
 setTitle("コンポジション(&C)");
 setIcon(QIcon(resolveIconPath("Studio/composition.svg")));
 connect(this, &QMenu::aboutToShow, this, &ArtifactCompositionMenu::rebuildMenu);
}

ArtifactCompositionMenu::ArtifactCompositionMenu(QWidget* parent)
 : QMenu(parent), impl_(new Impl(this, nullptr))
{
 setTitle("コンポジション(&C)");
 setIcon(QIcon(resolveIconPath("Studio/composition.svg")));
 connect(this, &QMenu::aboutToShow, this, &ArtifactCompositionMenu::rebuildMenu);
}

ArtifactCompositionMenu::~ArtifactCompositionMenu()
{
 delete impl_;
}

void ArtifactCompositionMenu::rebuildMenu()
{
 if (!impl_) return;
 auto service = ArtifactProjectService::instance();
 const bool hasComp = service && !service->currentComposition().expired();
 impl_->createAction->setEnabled(service != nullptr);
 if (impl_->presetMenu) {
  impl_->presetMenu->setEnabled(service != nullptr);
 }
 impl_->duplicateAction->setEnabled(hasComp);
 impl_->renameAction->setEnabled(hasComp);
 impl_->deleteAction->setEnabled(hasComp);
 impl_->settingsAction->setEnabled(hasComp);
 impl_->colorAction->setEnabled(hasComp);
}

void ArtifactCompositionMenu::handleCreateCompositionRequested()
{
 if (impl_) impl_->showCreate();
}

}

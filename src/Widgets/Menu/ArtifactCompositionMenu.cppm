module;
#include <utility>
#include <algorithm>
#include <QColor>
#include <QChar>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QComboBox>
#include <QCoreApplication>
#include <QEventLoop>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaType>
#include <QPushButton>
#include <QProgressDialog>
#include <QPalette>
#include <QPair>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QSpinBox>
#include <QSize>
#include <QStringList>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QTimer>
#include <QVariant>
#include <numeric>
#include <wobjectimpl.h>

module Menu.Composition;
import std;

import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Application.ProjectBundleIpc;
import Artifact.Composition.Abstract;
import Artifact.Layer.Composition;
import Artifact.Project.Items;
import Artifact.Project.Manager;
import Utils.Path;
import Artifact.Composition.InitParams;
import Artifact.Layer.InitParams;
import Artifact.Widgets.SoftwareRenderInspectors;
import Artifact.Widgets.ArtifactPropertyWidget;
import Property.Abstract;
import Property.ExposedPropertyRegistry;
import Dialog.Composition;
import FloatColorPickerDialog;
import Artifact.Widgets.AppDialogs;
import Widgets.Utils.CSS;
import Geometry.ResolutionRemap;
import Artifact.Widgets.ResolutionRemapDialog;
import Artifact.Widgets.Dialog.CompositionShell;
import Undo.UndoManager;
import UI.ShortcutBindings;

namespace Artifact {
using namespace ArtifactCore;

namespace {

class CompositionSettingsResultButton final : public QPushButton {
public:
 CompositionSettingsResultButton(
     const QString& text, QDialog* dialog, int resultCode)
  : QPushButton(text, dialog), dialog_(dialog), resultCode_(resultCode) {}

protected:
 void nextCheckState() override
 {
  if (dialog_) {
   dialog_->done(resultCode_);
  }
 }

private:
 QDialog* dialog_ = nullptr;
 int resultCode_ = QDialog::Rejected;
};

constexpr int kBakeComponentSimulationResult = 1001;
constexpr int kClearComponentSimulationBakeResult = 1002;

ArtifactPropertyWidget* activePropertyWidget(QWidget* root)
{
 if (!root) {
  return nullptr;
 }
 const auto widgets = root->findChildren<ArtifactPropertyWidget*>();
 for (auto* widget : widgets) {
  if (widget && widget->isVisible() && widget->hasActiveExpressionTarget()) {
   return widget;
  }
 }
 return nullptr;
}

QVariant stateOverrideValueFromText(const QString& text,
                                    const QVariant& typeTemplate,
                                    bool* converted)
{
 if (converted) {
  *converted = false;
 }
 if (!typeTemplate.isValid()) {
  if (converted) {
   *converted = true;
  }
  return text;
 }
 if (typeTemplate.metaType().id() == QMetaType::Bool) {
  const QString normalized = text.trimmed().toLower();
  if (normalized == QStringLiteral("true") || normalized == QStringLiteral("1") ||
      normalized == QStringLiteral("on") || normalized == QStringLiteral("yes")) {
   if (converted) {
    *converted = true;
   }
   return true;
  }
  if (normalized == QStringLiteral("false") || normalized == QStringLiteral("0") ||
      normalized == QStringLiteral("off") || normalized == QStringLiteral("no")) {
   if (converted) {
    *converted = true;
   }
   return false;
  }
  return {};
 }
 QVariant value(text);
 const bool ok = value.convert(typeTemplate.metaType());
 if (converted) {
  *converted = ok;
 }
 return ok ? value : QVariant{};
}

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

QString uniqueCompositionStateId(const QVector<CompositionStateVariant>& states,
                                 const QString& requestedName)
{
 const QString base = requestedName.trimmed().isEmpty()
  ? QStringLiteral("state")
  : requestedName.trimmed().toLower().replace(' ', '_');
 QString candidate = base;
 int suffix = 2;
 const auto containsId = [&states](const QString& id) {
  return std::any_of(states.cbegin(), states.cend(), [&id](const auto& state) {
   return state.stateId == id;
  });
 };
 while (containsId(candidate)) {
  candidate = QStringLiteral("%1_%2").arg(base).arg(suffix++);
 }
 return candidate;
}

class ChangeCompositionStatesCommand final : public UndoCommand {
public:
 ChangeCompositionStatesCommand(ArtifactCompositionWeakPtr composition,
                                QVector<CompositionStateVariant> beforeStates,
                                QString beforeActiveId,
                                QVector<CompositionStateVariant> afterStates,
                                QString afterActiveId,
                                QString beforeComparisonAId,
                                QString beforeComparisonBId,
                                QString afterComparisonAId,
                                QString afterComparisonBId,
                                QString label)
  : composition_(std::move(composition)),
    beforeStates_(std::move(beforeStates)),
    beforeActiveId_(std::move(beforeActiveId)),
    afterStates_(std::move(afterStates)),
    afterActiveId_(std::move(afterActiveId)),
    beforeComparisonAId_(std::move(beforeComparisonAId)),
    beforeComparisonBId_(std::move(beforeComparisonBId)),
    afterComparisonAId_(std::move(afterComparisonAId)),
    afterComparisonBId_(std::move(afterComparisonBId)),
    label_(std::move(label))
 {
 }

 void undo() override {
  apply(beforeStates_, beforeActiveId_, beforeComparisonAId_,
        beforeComparisonBId_);
 }
 void redo() override {
  apply(afterStates_, afterActiveId_, afterComparisonAId_,
        afterComparisonBId_);
 }
 QString label() const override { return label_; }

private:
 void apply(const QVector<CompositionStateVariant>& states,
            const QString& activeId, const QString& comparisonAId,
            const QString& comparisonBId)
 {
  if (const auto composition = composition_.lock()) {
   composition->setActiveStateVariantId(QString());
   composition->setStateVariants(states);
   composition->setStateComparisonPair(comparisonAId, comparisonBId);
   composition->setActiveStateVariantId(activeId);
   if (auto* manager = UndoManager::instance()) {
    manager->notifyAnythingChanged();
   }
  }
 }

 ArtifactCompositionWeakPtr composition_;
 QVector<CompositionStateVariant> beforeStates_;
 QString beforeActiveId_;
 QVector<CompositionStateVariant> afterStates_;
 QString afterActiveId_;
 QString beforeComparisonAId_;
 QString beforeComparisonBId_;
 QString afterComparisonAId_;
 QString afterComparisonBId_;
 QString label_;
};

struct MasterPropertyRegistryChange {
 CompositionID parentCompositionId;
 LayerID precompLayerId;
 ExposedPropertyRegistry before;
 ExposedPropertyRegistry after;
 QHash<QString, QVariant> beforeOverrides;
 QHash<QString, QVariant> afterOverrides;
};

class ChangeMasterPropertiesCommand final : public UndoCommand {
public:
 ChangeMasterPropertiesCommand(QVector<MasterPropertyRegistryChange> changes,
                               QString label)
  : changes_(std::move(changes)), label_(std::move(label)) {}

 void undo() override { apply(false); }
 void redo() override { apply(true); }
 QString label() const override { return label_; }

private:
 void apply(const bool useAfter) {
  auto* service = ArtifactProjectService::instance();
  if (!service) {
   return;
  }
  for (const auto& change : changes_) {
   const auto parent = service->findComposition(change.parentCompositionId).ptr.lock();
   const auto layer = parent
    ? std::dynamic_pointer_cast<ArtifactCompositionLayer>(
       parent->layerById(change.precompLayerId))
    : std::shared_ptr<ArtifactCompositionLayer>{};
   if (layer) {
    layer->setExposedProperties(useAfter ? change.after : change.before);
    const auto& overrides = useAfter
     ? change.afterOverrides
     : change.beforeOverrides;
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
     layer->setExposedPropertyOverride(it.key(), it.value());
    }
   }
  }
 }

 QVector<MasterPropertyRegistryChange> changes_;
 QString label_;
};

QString masterPropertyId(const LayerID& layerId, const QString& propertyPath)
{
 QString normalizedPath = propertyPath.trimmed().toLower();
 for (auto& character : normalizedPath) {
  if (!character.isLetterOrNumber()) {
   character = QChar('_');
  }
 }
 while (normalizedPath.contains(QStringLiteral("__"))) {
  normalizedPath.replace(QStringLiteral("__"), QStringLiteral("_"));
 }
 return QStringLiteral("layer_%1_%2")
  .arg(layerId.toString(), normalizedPath)
  .left(180);
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
 QAction* sendAction = nullptr;

 void showCreate();
 void createFromPreset(const ArtifactCompositionInitParams& params);
 void duplicateCurrent();
 void renameCurrent();
 void removeCurrent();
 void showSettings();
 void showColor();
 void sendCurrentComposition();
};

ArtifactCompositionMenu::Impl::Impl(ArtifactCompositionMenu* menu, QWidget* mainWindow)
 : menu_(menu), mainWindow_(mainWindow)
{
 createAction = new QAction("新規コンポジション(&N)...");
 createAction->setShortcut(ShortcutBindings::instance().shortcut(ShortcutId::CompositionCreate));
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
 colorAction->setShortcut(ShortcutBindings::instance().shortcut(ShortcutId::CompositionColor));
 colorAction->setIcon(QIcon(resolveIconPath("Studio/compositionmenu_background.svg")));

 sendAction = new QAction("メインプロジェクトへ送信(&T)...", menu);
 sendAction->setIcon(QIcon(resolveIconPath("Studio/compositionmenu_send_project.svg")));

 menu->addAction(createAction);
 menu->addMenu(presetMenu);
 menu->addSeparator();
 menu->addAction(duplicateAction);
 menu->addAction(renameAction);
 menu->addAction(deleteAction);
 menu->addSeparator();
 menu->addAction(settingsAction);
 menu->addAction(sendAction);
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
 QObject::connect(sendAction, &QAction::triggered, menu, [this]() { sendCurrentComposition(); });
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

 ArtifactCompositionDialogShell dialog(mainWindow_ ? mainWindow_ : menu_);

 const auto& dialogTheme = ArtifactCore::currentDCCTheme();
 dialog.setAutoFillBackground(true);
 QPalette dialogPalette = dialog.palette();
 dialogPalette.setColor(QPalette::Window, QColor(dialogTheme.backgroundColor));
 dialogPalette.setColor(QPalette::Base, QColor(dialogTheme.secondaryBackgroundColor));
 dialogPalette.setColor(QPalette::WindowText, QColor(dialogTheme.textColor));
 dialogPalette.setColor(QPalette::Text, QColor(dialogTheme.textColor));
 dialog.setPalette(dialogPalette);
 auto* contentLayout = dialog.contentLayout();

 auto* nameLabel = new QLabel(QStringLiteral("Name"), &dialog);
 auto* nameEdit = new QLineEdit(current->settings().compositionName().toQString(), &dialog);
 contentLayout->addWidget(nameLabel);
 contentLayout->addWidget(nameEdit);

 const QSize initialSize = current->effectiveCompositionSize();
 auto* sizeLabel = new QLabel(QStringLiteral("Size"), &dialog);
 contentLayout->addWidget(sizeLabel);
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
 contentLayout->addLayout(sizeLayout);

 auto* responsiveLabel = new QLabel(QStringLiteral("Responsive Layout"), &dialog);
 contentLayout->addWidget(responsiveLabel);
 auto* responsiveCombo = new QComboBox(&dialog);
 const ResponsiveLayoutSet previewLayout = normalizedResponsiveLayoutForDialog(current);
 for (const auto& variant : previewLayout.variants) {
  responsiveCombo->addItem(responsiveVariantLabel(variant), variant.variantId);
  if (variant.variantId == previewLayout.activeVariantId) {
   responsiveCombo->setCurrentIndex(responsiveCombo->count() - 1);
  }
 }
 contentLayout->addWidget(responsiveCombo);
 auto* responsiveHint = new QLabel(
  QStringLiteral("Select the active layout variant for this composition."), &dialog);
 {
  QPalette pal = responsiveHint->palette();
  pal.setColor(QPalette::WindowText,
               QColor(ArtifactCore::currentDCCTheme().textColor).darker(140));
  responsiveHint->setPalette(pal);
 }
 contentLayout->addWidget(responsiveHint);

 auto* stateLabel = new QLabel(QStringLiteral("Composition State"), &dialog);
 contentLayout->addWidget(stateLabel);
 auto* stateCombo = new QComboBox(&dialog);
 stateCombo->addItem(QStringLiteral("None (baseline)"), QString());
 const auto initialStates = current->stateVariants();
 const QString initialActiveStateId = current->activeStateVariantId();
 for (const auto& state : initialStates) {
  const QString stateName = state.displayName.trimmed().isEmpty()
   ? state.stateId
   : state.displayName;
  stateCombo->addItem(
   state.stateId == initialActiveStateId
    ? QStringLiteral("%1  [active]  ·  %2 overrides")
       .arg(stateName).arg(state.overrides.size())
    : QStringLiteral("%1  ·  %2 overrides")
       .arg(stateName).arg(state.overrides.size()),
   state.stateId);
 }
 const int activeStateIndex = stateCombo->findData(initialActiveStateId);
 stateCombo->setCurrentIndex(std::max(0, activeStateIndex));
 contentLayout->addWidget(stateCombo);

 auto* stateOperationCombo = new QComboBox(&dialog);
 stateOperationCombo->addItem(QStringLiteral("No state change"), QStringLiteral("none"));
 stateOperationCombo->addItem(QStringLiteral("Create new state"), QStringLiteral("create"));
 stateOperationCombo->addItem(QStringLiteral("Duplicate selected state"), QStringLiteral("duplicate"));
 stateOperationCombo->addItem(QStringLiteral("Rename selected state"), QStringLiteral("rename"));
 stateOperationCombo->addItem(QStringLiteral("Delete selected state"), QStringLiteral("delete"));
 stateOperationCombo->addItem(QStringLiteral("Activate selected state"), QStringLiteral("activate"));
 stateOperationCombo->addItem(QStringLiteral("Deactivate state / restore baseline"), QStringLiteral("deactivate"));
 stateOperationCombo->addItem(QStringLiteral("Capture focused property override"), QStringLiteral("capture"));
 stateOperationCombo->addItem(QStringLiteral("Remove focused property override"), QStringLiteral("remove_override"));
 stateOperationCombo->addItem(QStringLiteral("Compare selected state A/B properties"), QStringLiteral("compare"));
 contentLayout->addWidget(stateOperationCombo);
 auto* stateNameEdit = new QLineEdit(&dialog);
 stateNameEdit->setPlaceholderText(QStringLiteral("State name for create, duplicate, or rename"));
 contentLayout->addWidget(stateNameEdit);
 auto* comparisonStateCombo = new QComboBox(&dialog);
 comparisonStateCombo->addItem(QStringLiteral("Compare with baseline"), QString());
 for (const auto& state : initialStates) {
  const QString stateName = state.displayName.trimmed().isEmpty()
   ? state.stateId
   : state.displayName;
  comparisonStateCombo->addItem(
   QStringLiteral("Compare with %1").arg(stateName), state.stateId);
 }
 const int comparisonStateIndex =
  comparisonStateCombo->findData(current->stateComparisonBId());
 comparisonStateCombo->setCurrentIndex(std::max(0, comparisonStateIndex));
 contentLayout->addWidget(comparisonStateCombo);
 QWidget* propertyRoot = mainWindow_ ? mainWindow_ : menu_->window();
 auto* focusedPropertyWidget = activePropertyWidget(propertyRoot);
 const auto focusedPropertyLayer = focusedPropertyWidget
  ? focusedPropertyWidget->activePropertyLayer()
  : ArtifactAbstractLayerPtr{};
 const QString focusedPropertyPath = focusedPropertyWidget
  ? focusedPropertyWidget->activePropertyPath().trimmed()
  : QString{};
 const auto focusedProperty = focusedPropertyLayer && !focusedPropertyPath.isEmpty()
  ? focusedPropertyLayer->getProperty(focusedPropertyPath)
  : std::shared_ptr<ArtifactCore::AbstractProperty>{};
 const QVariant focusedPropertyBaseline = focusedProperty
  ? focusedProperty->getValue()
  : QVariant{};
 auto* focusedPropertyLabel = new QLabel(
  focusedProperty
   ? QStringLiteral("Focused property: %1").arg(focusedPropertyPath)
   : QStringLiteral("Focused property: none"),
  &dialog);
 contentLayout->addWidget(focusedPropertyLabel);
 auto* stateOverrideValueEdit = new QLineEdit(
  focusedPropertyBaseline.toString(), &dialog);
 stateOverrideValueEdit->setPlaceholderText(
  QStringLiteral("Override value for the focused property"));
 contentLayout->addWidget(stateOverrideValueEdit);

 auto* masterPropertyLabel = new QLabel(QStringLiteral("Master Properties"), &dialog);
 contentLayout->addWidget(masterPropertyLabel);
 auto* masterPropertyOperationCombo = new QComboBox(&dialog);
 masterPropertyOperationCombo->addItem(
  QStringLiteral("No Master Property change"), QStringLiteral("none"));
 masterPropertyOperationCombo->addItem(
  QStringLiteral("Expose focused property to parent precomps"),
  QStringLiteral("expose"));
 masterPropertyOperationCombo->addItem(
  QStringLiteral("Remove focused property from parent precomps"),
  QStringLiteral("remove"));
 contentLayout->addWidget(masterPropertyOperationCombo);
 auto* masterPropertyNameEdit = new QLineEdit(&dialog);
 masterPropertyNameEdit->setPlaceholderText(
  QStringLiteral("Display name for the exposed property"));
 masterPropertyNameEdit->setText(
  focusedPropertyPath.section(QChar('/'), -1).section(QChar('.'), -1));
 contentLayout->addWidget(masterPropertyNameEdit);

 auto* fpsLabel = new QLabel(QStringLiteral("Frame Rate"), &dialog);
 contentLayout->addWidget(fpsLabel);
 auto* fpsSpin = new QDoubleSpinBox(&dialog);
 fpsSpin->setRange(1.0, 240.0);
 fpsSpin->setDecimals(3);
 fpsSpin->setSingleStep(0.5);
 fpsSpin->setValue(std::max(1.0, static_cast<double>(current->frameRate().framerate())));
 contentLayout->addWidget(fpsSpin);

 const FrameRange currentRange = current->frameRange().normalized();
 auto* rangeLabel = new QLabel(QStringLiteral("Frame Range"), &dialog);
 contentLayout->addWidget(rangeLabel);
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
 contentLayout->addLayout(rangeLayout);

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
 contentLayout->addLayout(bgRow);

 auto* infoLabel = new QLabel(QStringLiteral("ID: %1").arg(current->id().toString()), &dialog);
 {
  QPalette pal = infoLabel->palette();
  pal.setColor(QPalette::WindowText,
               QColor(ArtifactCore::currentDCCTheme().textColor).darker(135));
  infoLabel->setPalette(pal);
 }
 contentLayout->addWidget(infoLabel);

 auto* simulationCacheLabel = new QLabel(
  current->hasAuthoritativeLayerComponentSimulation()
   ? QStringLiteral("Component Simulation Cache: ready (bounded to 120 frames / 64 MiB)")
   : QStringLiteral("Component Simulation Cache: not evaluated"),
  &dialog);
 contentLayout->addWidget(simulationCacheLabel);
 auto* simulationCacheRow = new QHBoxLayout();
 auto* bakeSimulationButton = new CompositionSettingsResultButton(
 QStringLiteral("Bake Simulation Cache"), &dialog,
  kBakeComponentSimulationResult);
 bakeSimulationButton->setAutoDefault(false);
 bakeSimulationButton->setEnabled(current->usesLayerComponentSimulation());
 bakeSimulationButton->setToolTip(QStringLiteral(
  "Persist the current bounded component simulation session beside the project file."));
 auto* clearSimulationBakeButton = new CompositionSettingsResultButton(
 QStringLiteral("Clear Bake"), &dialog,
  kClearComponentSimulationBakeResult);
 clearSimulationBakeButton->setAutoDefault(false);
 clearSimulationBakeButton->setEnabled(
  current->hasAuthoritativeLayerComponentSimulation());
 clearSimulationBakeButton->setToolTip(QStringLiteral(
  "Discard the current composition simulation cache and update its sidecar."));
 simulationCacheRow->addWidget(bakeSimulationButton);
 simulationCacheRow->addWidget(clearSimulationBakeButton);
 simulationCacheRow->addStretch();
 contentLayout->addLayout(simulationCacheRow);

 auto* buttons =
     new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
 dialog.footerLayout()->addWidget(buttons);

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

  const QString requestedStateOperation =
   stateOperationCombo->currentData().toString();
  const QString requestedStateId = stateCombo->currentData().toString();
  if ((requestedStateOperation == QStringLiteral("capture") ||
       requestedStateOperation == QStringLiteral("remove_override")) &&
      (requestedStateId.isEmpty() || requestedStateId != initialActiveStateId ||
       !focusedProperty)) {
   QMessageBox::information(
    &dialog, QStringLiteral("Composition State"),
    QStringLiteral("Activate a state and focus an editable property before capturing or removing an override."));
   return;
  }
  QVariant requestedOverrideValue;
  if (requestedStateOperation == QStringLiteral("capture")) {
   bool converted = false;
   requestedOverrideValue = stateOverrideValueFromText(
    stateOverrideValueEdit->text(), focusedPropertyBaseline, &converted);
   if (!converted) {
    QMessageBox::warning(
     &dialog, QStringLiteral("Composition State"),
     QStringLiteral("Override value could not be converted to the property type."));
    return;
   }
  }
  QVector<MasterPropertyRegistryChange> masterPropertyChanges;
  QString masterPropertyCommandLabel;
  const QString requestedMasterPropertyOperation =
   masterPropertyOperationCombo->currentData().toString();
  if (requestedMasterPropertyOperation != QStringLiteral("none")) {
   if (!focusedProperty || !focusedPropertyLayer ||
       !current->containsLayerById(focusedPropertyLayer->id())) {
    QMessageBox::information(
     &dialog, QStringLiteral("Master Properties"),
     QStringLiteral("Focus an editable property inside this composition first."));
    return;
   }

   QVector<ProjectItem*> pendingItems = service->projectItems();
   QSet<ProjectItem*> visitedItems;
   QVector<CompositionID> compositionIds;
   while (!pendingItems.isEmpty()) {
    ProjectItem* item = pendingItems.takeLast();
    if (!item || visitedItems.contains(item)) {
     continue;
    }
    visitedItems.insert(item);
    pendingItems += item->children;
    if (auto* compositionItem = dynamic_cast<CompositionItem*>(item)) {
     compositionIds.append(compositionItem->compositionId);
    }
   }

   const QString bindingId = masterPropertyId(
    focusedPropertyLayer->id(), focusedPropertyPath);
   for (const auto& compositionId : compositionIds) {
    const auto parent = service->findComposition(compositionId).ptr.lock();
    if (!parent) {
     continue;
    }
    for (const auto& candidate : parent->allLayerRef()) {
     const auto precompLayer =
      std::dynamic_pointer_cast<ArtifactCompositionLayer>(candidate);
     if (!precompLayer || precompLayer->sourceCompositionId() != current->id()) {
      continue;
     }
     MasterPropertyRegistryChange change;
     change.parentCompositionId = parent->id();
     change.precompLayerId = precompLayer->id();
     change.before = precompLayer->exposedProperties();
     change.after = change.before;
     for (const auto& binding : change.before.bindings()) {
      if (precompLayer->hasExposedPropertyOverride(binding.id)) {
       change.beforeOverrides.insert(
        binding.id, precompLayer->exposedPropertyOverride(binding.id));
      }
     }

     if (requestedMasterPropertyOperation == QStringLiteral("expose")) {
      ExposedPropertyBinding binding;
      binding.id = bindingId;
      binding.label = masterPropertyNameEdit->text().trimmed().isEmpty()
       ? focusedPropertyPath
       : masterPropertyNameEdit->text().trimmed();
      binding.targetLayerId = focusedPropertyLayer->id().toString();
      binding.internalPath = focusedPropertyPath;
      binding.defaultValue = focusedPropertyBaseline;
      change.after.replace(std::move(binding));
      masterPropertyCommandLabel = QStringLiteral("Expose Master Property");
     } else {
      for (const auto& binding : change.before.bindings()) {
       if (binding.targetLayerId == focusedPropertyLayer->id().toString() &&
           binding.internalPath == focusedPropertyPath) {
        change.after.remove(binding.id);
       }
      }
      masterPropertyCommandLabel = QStringLiteral("Remove Master Property");
     }

     if (change.before.toJson() != change.after.toJson()) {
      for (const auto& binding : change.after.bindings()) {
       if (precompLayer->hasExposedPropertyOverride(binding.id)) {
        change.afterOverrides.insert(
         binding.id, precompLayer->exposedPropertyOverride(binding.id));
       }
      }
      masterPropertyChanges.append(std::move(change));
     }
    }
   }

   if (masterPropertyChanges.isEmpty()) {
    QMessageBox::information(
     &dialog, QStringLiteral("Master Properties"),
     requestedMasterPropertyOperation == QStringLiteral("expose")
      ? QStringLiteral("No parent precomp instance references this composition, or the property is already exposed.")
      : QStringLiteral("The focused property is not exposed on any parent precomp instance."));
    return;
   }
  }
  if (requestedStateOperation == QStringLiteral("compare")) {
   const QString stateAId = stateCombo->currentData().toString();
   const QString stateBId = comparisonStateCombo->currentData().toString();
   current->setStateComparisonPair(stateAId, stateBId);
   const auto findState = [&initialStates](const QString& stateId)
       -> const CompositionStateVariant* {
    const auto found = std::find_if(
     initialStates.cbegin(), initialStates.cend(), [&stateId](const auto& state) {
      return state.stateId == stateId;
     });
    return found == initialStates.cend() ? nullptr : &(*found);
   };
   const auto* stateA = findState(stateAId);
   const auto* stateB = findState(stateBId);
   QVector<QPair<LayerID, QString>> propertyKeys;
   const auto collectKeys = [&propertyKeys](const CompositionStateVariant* state) {
    if (!state) {
     return;
    }
    for (const auto& item : state->overrides) {
     if (!item.enabled) {
      continue;
     }
     const QPair<LayerID, QString> key(item.layerId, item.propertyPath);
     if (!propertyKeys.contains(key)) {
      propertyKeys.append(key);
     }
    }
   };
   collectKeys(stateA);
   collectKeys(stateB);
   const auto findOverride = [](const CompositionStateVariant* state,
                                const LayerID& layerId,
                                const QString& propertyPath)
       -> const CompositionStatePropertyOverride* {
    if (!state) {
     return nullptr;
    }
    const auto found = std::find_if(
     state->overrides.cbegin(), state->overrides.cend(),
     [&layerId, &propertyPath](const auto& item) {
      return item.enabled && item.layerId == layerId &&
             item.propertyPath == propertyPath;
     });
    return found == state->overrides.cend() ? nullptr : &(*found);
   };
   QStringList changedProperties;
   for (const auto& key : propertyKeys) {
    const auto* overrideA = findOverride(stateA, key.first, key.second);
    const auto* overrideB = findOverride(stateB, key.first, key.second);
    const QVariant valueA = overrideA
     ? overrideA->value
     : (overrideB ? overrideB->baselineValue : QVariant{});
    const QVariant valueB = overrideB
     ? overrideB->value
     : (overrideA ? overrideA->baselineValue : QVariant{});
    if (valueA == valueB) {
     continue;
    }
    const auto layer = current->layerById(key.first);
    const QString layerName = layer ? layer->layerName() : key.first.toString();
    changedProperties.append(
     QStringLiteral("%1 · %2\n    A: %3\n    B: %4")
      .arg(layerName, key.second, valueA.toString(), valueB.toString()));
   }
   QMessageBox::information(
    &dialog, QStringLiteral("Composition State Comparison"),
    changedProperties.isEmpty()
     ? QStringLiteral("No property differences between the selected states.")
     : changedProperties.join(QStringLiteral("\n\n")));
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

	  // --- Resolution remap wizard (consistent with ArtifactProjectManagerWidget) ---
	  {
	      const QSize oldSize = current->effectiveCompositionSize();
	      if (oldSize != chosenSize) {
	          bool hasMasks = false;
	          int maskVerts = 0;
	          bool hasAnchors = false;
	          int layerCount = 0;
	          for (const auto& layer : current->allLayerRef()) {
	              if (!layer) continue;
	              ++layerCount;
	              if (layer->hasMasks()) {
	                  hasMasks = true;
	                  for (int mi = 0; mi < layer->maskCount(); ++mi) {
	                      const auto lm = layer->mask(mi);
	                      for (int pi = 0; pi < lm.maskPathCount(); ++pi) {
	                          maskVerts += lm.maskPath(pi).vertexCount();
	                      }
	                  }
	              }
	          }
	          hasAnchors = layerCount > 0;
	          auto impact = ArtifactCore::ResolutionRemap::calculateImpact(
	              oldSize, chosenSize, hasMasks, maskVerts > 0, hasAnchors);
	          impact.maskVertexCount = maskVerts;

	          ArtifactResolutionRemapDialog remapDialog(oldSize, chosenSize, impact);
	          if (remapDialog.exec() == QDialog::Accepted && remapDialog.remapRequested()) {
	              if (auto* mgr = UndoManager::instance()) {
	                  mgr->push(std::make_unique<ChangeCompositionResolutionCommand>(
	                      current, oldSize, chosenSize, remapDialog.selectedPolicy()));
	              } else {
	                  current->applyResolutionRemap(chosenSize, remapDialog.selectedPolicy());
	              }
	          } else {
	              current->setCompositionSize(chosenSize);
	          }
	      }
	  }
	  // --- end remap wizard ---

	  current->setFrameRate(FrameRate(static_cast<float>(fpsSpin->value())));
  current->setFrameRange(FrameRange(FramePosition(startFrame), FramePosition(endFrame)));
 current->setBackGroundColor(FloatColor(backgroundColor.redF(),
                                         backgroundColor.greenF(),
                                         backgroundColor.blueF(),
                                         backgroundColor.alphaF()));

  const QString stateOperation = stateOperationCombo->currentData().toString();
  if (stateOperation != QStringLiteral("none")) {
   QVector<CompositionStateVariant> afterStates = initialStates;
   QString afterActiveStateId = initialActiveStateId;
   QString afterComparisonAId = current->stateComparisonAId();
   QString afterComparisonBId = current->stateComparisonBId();
   const QString selectedStateId = stateCombo->currentData().toString();
   const QString requestedStateName = stateNameEdit->text().trimmed();
   QString stateCommandLabel;
   auto selectedState = std::find_if(
    afterStates.begin(), afterStates.end(), [&selectedStateId](const auto& state) {
     return state.stateId == selectedStateId;
    });

   if (stateOperation == QStringLiteral("create")) {
    CompositionStateVariant state;
    state.displayName = requestedStateName.isEmpty()
     ? QStringLiteral("State %1").arg(afterStates.size() + 1)
     : requestedStateName;
    state.stateId = uniqueCompositionStateId(afterStates, state.displayName);
    afterStates.append(state);
    afterActiveStateId = state.stateId;
    stateCommandLabel = QStringLiteral("Create Composition State");
   } else if (stateOperation == QStringLiteral("duplicate") &&
              selectedState != afterStates.end()) {
    CompositionStateVariant duplicate = *selectedState;
    duplicate.displayName = requestedStateName.isEmpty()
     ? QStringLiteral("%1 Copy").arg(duplicate.displayName)
     : requestedStateName;
    duplicate.stateId = uniqueCompositionStateId(afterStates, duplicate.displayName);
    afterStates.append(duplicate);
    afterActiveStateId = duplicate.stateId;
    stateCommandLabel = QStringLiteral("Duplicate Composition State");
   } else if (stateOperation == QStringLiteral("rename") &&
              selectedState != afterStates.end() && !requestedStateName.isEmpty()) {
    selectedState->displayName = requestedStateName;
    stateCommandLabel = QStringLiteral("Rename Composition State");
   } else if (stateOperation == QStringLiteral("delete") &&
              selectedState != afterStates.end()) {
    afterStates.erase(selectedState);
    if (afterActiveStateId == selectedStateId) {
     afterActiveStateId.clear();
    }
    if (afterComparisonAId == selectedStateId) {
     afterComparisonAId.clear();
    }
    if (afterComparisonBId == selectedStateId) {
     afterComparisonBId.clear();
    }
    stateCommandLabel = QStringLiteral("Delete Composition State");
   } else if (stateOperation == QStringLiteral("activate") &&
              selectedState != afterStates.end()) {
    afterActiveStateId = selectedStateId;
    stateCommandLabel = QStringLiteral("Activate Composition State");
   } else if (stateOperation == QStringLiteral("deactivate")) {
    afterActiveStateId.clear();
    stateCommandLabel = QStringLiteral("Deactivate Composition State");
   } else if ((stateOperation == QStringLiteral("capture") ||
               stateOperation == QStringLiteral("remove_override")) &&
              selectedState != afterStates.end() &&
              selectedStateId == initialActiveStateId && focusedProperty) {
    auto overrideItem = std::find_if(
     selectedState->overrides.begin(), selectedState->overrides.end(),
     [&focusedPropertyLayer, &focusedPropertyPath](const auto& item) {
      return item.layerId == focusedPropertyLayer->id() &&
             item.propertyPath == focusedPropertyPath;
     });
    if (stateOperation == QStringLiteral("remove_override")) {
     if (overrideItem != selectedState->overrides.end()) {
      selectedState->overrides.erase(overrideItem);
      stateCommandLabel = QStringLiteral("Remove Composition State Override");
     }
    } else {
     if (overrideItem == selectedState->overrides.end()) {
      CompositionStatePropertyOverride item;
      item.layerId = focusedPropertyLayer->id();
      item.propertyPath = focusedPropertyPath;
      item.baselineValue = focusedPropertyBaseline;
      item.value = requestedOverrideValue;
      selectedState->overrides.append(item);
     } else {
      overrideItem->value = requestedOverrideValue;
      overrideItem->enabled = true;
     }
     stateCommandLabel = QStringLiteral("Capture Composition State Override");
    }
   }

   if (!stateCommandLabel.isEmpty()) {
    if (auto* manager = UndoManager::instance()) {
     manager->push(std::make_unique<ChangeCompositionStatesCommand>(
     current, initialStates, initialActiveStateId, afterStates,
      afterActiveStateId, current->stateComparisonAId(),
      current->stateComparisonBId(), afterComparisonAId,
      afterComparisonBId, stateCommandLabel));
    } else {
     current->setActiveStateVariantId(QString());
     current->setStateVariants(afterStates);
     current->setStateComparisonPair(afterComparisonAId, afterComparisonBId);
     current->setActiveStateVariantId(afterActiveStateId);
    }
   }
  }

  if (!masterPropertyChanges.isEmpty()) {
   if (auto* manager = UndoManager::instance()) {
    manager->push(std::make_unique<ChangeMasterPropertiesCommand>(
     masterPropertyChanges, masterPropertyCommandLabel));
   } else {
    for (const auto& change : masterPropertyChanges) {
     const auto parent = service->findComposition(change.parentCompositionId).ptr.lock();
     const auto precompLayer = parent
      ? std::dynamic_pointer_cast<ArtifactCompositionLayer>(
         parent->layerById(change.precompLayerId))
      : std::shared_ptr<ArtifactCompositionLayer>{};
     if (precompLayer) {
      precompLayer->setExposedProperties(change.after);
      for (auto it = change.afterOverrides.cbegin();
           it != change.afterOverrides.cend(); ++it) {
       precompLayer->setExposedPropertyOverride(it.key(), it.value());
      }
     }
    }
   }
  }

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

 const int dialogResult = dialog.exec();
 if (dialogResult == kBakeComponentSimulationResult ||
     dialogResult == kClearComponentSimulationBakeResult) {
  current->setBackGroundColor(FloatColor(originalBackgroundColor.redF(),
                                         originalBackgroundColor.greenF(),
                                         originalBackgroundColor.blueF(),
                                         originalBackgroundColor.alphaF()));
 }
 if (dialogResult == kBakeComponentSimulationResult) {
  const FrameRange bakeRange = current->frameRange();
  const int64_t totalFrames = std::max<int64_t>(
   1, bakeRange.end() - bakeRange.start());
  const int progressMaximum = static_cast<int>(std::min<int64_t>(
   totalFrames, std::numeric_limits<int>::max()));
  QProgressDialog progress(
   QStringLiteral("Baking component simulation checkpoints..."),
   QStringLiteral("Cancel"), 0, progressMaximum,
   mainWindow_ ? mainWindow_ : menu_);
  progress.setWindowTitle(QStringLiteral("Component Simulation Bake"));
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(0);
  const bool completed = current->bakeLayerComponentSimulation(
   bakeRange, [&progress, progressMaximum](int64_t completedFrames,
                                           int64_t total) {
    int value = progressMaximum;
    if (total > 0) {
     const long double scaled =
      (static_cast<long double>(completedFrames) * progressMaximum) /
      static_cast<long double>(total);
     value = static_cast<int>(std::clamp<long double>(
      scaled, 0.0L, static_cast<long double>(progressMaximum)));
    }
    progress.setValue(value);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    return !progress.wasCanceled();
   });
  if (!completed) {
   QMessageBox::information(mainWindow_ ? mainWindow_ : menu_,
                            QStringLiteral("Component Simulation Bake"),
                            QStringLiteral("The bake was canceled. The previous simulation cache was restored."));
   return;
  }
  progress.setValue(progressMaximum);
  const bool saved = ArtifactProjectManager::getInstance()
                         .persistComponentSimulationBakes();
  if (saved) {
   QMessageBox::information(mainWindow_ ? mainWindow_ : menu_,
                            QStringLiteral("Component Simulation Bake"),
                            QStringLiteral("The current bounded simulation cache was saved."));
  } else {
   QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
                        QStringLiteral("Component Simulation Bake"),
                        QStringLiteral("Save the project before creating a persistent bake."));
  }
 } else if (dialogResult == kClearComponentSimulationBakeResult) {
  const bool cleared = ArtifactProjectManager::getInstance()
                           .discardComponentSimulationBake(current->id());
  if (cleared) {
   QMessageBox::information(mainWindow_ ? mainWindow_ : menu_,
                            QStringLiteral("Component Simulation Bake"),
                            QStringLiteral("The simulation bake was cleared."));
  } else {
   QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
                        QStringLiteral("Component Simulation Bake"),
                        QStringLiteral("The simulation bake could not be cleared."));
  }
 }
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

void ArtifactCompositionMenu::Impl::sendCurrentComposition()
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
                          QStringLiteral("送信するコンポジションがありません。"));
  return;
 }

 QJsonObject bundle;
 bundle[QStringLiteral("bundleKind")] = QStringLiteral("composition");
 bundle[QStringLiteral("bundleTitle")] = current->settings().compositionName().toQString();
 bundle[QStringLiteral("sourceProjectName")] = service->projectName().toQString();
 bundle[QStringLiteral("sourceCompositionId")] = current->id().toString();
 bundle[QStringLiteral("sourceCompositionName")] = current->settings().compositionName().toQString();
 bundle[QStringLiteral("composition")] = current->toJson().object();

 QString error;
 if (!sendProjectBundleToMainProject(bundle, &error)) {
  QMessageBox::warning(mainWindow_ ? mainWindow_ : menu_,
                       QStringLiteral("Send Bundle"),
                       error.isEmpty()
                           ? QStringLiteral("Failed to send the composition to the main project.")
                           : error);
 }
}

ArtifactCompositionMenu::ArtifactCompositionMenu(QWidget* mainWindow, QWidget* parent)
 : QMenu(parent), impl_(new Impl(this, mainWindow))
{
 setTitle("コンポジション(&C)");
 setIcon(QIcon(resolveIconPath("Studio/menubar_composition.svg")));
 connect(this, &QMenu::aboutToShow, this, &ArtifactCompositionMenu::rebuildMenu);
}

ArtifactCompositionMenu::ArtifactCompositionMenu(QWidget* parent)
 : QMenu(parent), impl_(new Impl(this, nullptr))
{
 setTitle("コンポジション(&C)");
 setIcon(QIcon(resolveIconPath("Studio/menubar_composition.svg")));
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
 impl_->sendAction->setEnabled(hasComp);
 impl_->colorAction->setEnabled(hasComp);
}

void ArtifactCompositionMenu::handleCreateCompositionRequested()
{
 if (impl_) impl_->showCreate();
}

}

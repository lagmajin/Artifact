module;
#include <utility>
#include <QAction>
#include <QDebug>
#include <QDialog>
#include <QFont>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QMetaObject>
#include <QMenu>
#include <QPalette>
#include <QKeySequence>
#include <QThread>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSize>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <wobjectimpl.h>

module Artifact.Menu.Effect;
import std;
import Translation.Manager;

import Event.Bus;
import Artifact.Event.Types;
import Artifact.Service.Effect;
import Artifact.Service.Project;
import Artifact.Effect.Abstract;
import Artifact.Effect.Ofx.Host;
import Artifact.Widgets.FxStudio.Dialog;
import Artifact.Layers.Selection.Manager;
import Artifact.Layer.Abstract;
import Utils.Id;
import Utils.Path;
import UI.ShortcutBindings;

namespace Artifact
{
namespace {
QIcon menuIcon(const QString& path)
{
  return QIcon(resolveIconPath(path));
}

struct EffectMenuCategory {
  QString title;
  QString iconPath;
};

EffectMenuCategory categoryForEffect(const EffectInfo& info)
{
  const QString id = info.id.toString();
  const QString name = info.displayName.toLower();

  if (id.startsWith(QStringLiteral("ofx."))) {
    return {QStringLiteral("OFX"), QStringLiteral("Studio/effect_ops_ofx.svg")};
  }
  if (id.contains(QStringLiteral("colorcorrection")) ||
      name.contains(QStringLiteral("color")) ||
      name.contains(QStringLiteral("brightness")) ||
      name.contains(QStringLiteral("contrast")) ||
      name.contains(QStringLiteral("exposure")) ||
      name.contains(QStringLiteral("curves")) ||
      name.contains(QStringLiteral("levels")) ||
      name.contains(QStringLiteral("tint")) ||
      name.contains(QStringLiteral("filter")) ||
      name.contains(QStringLiteral("gamma"))) {
    return {QStringLiteral("カラー"), QStringLiteral("Studio/effect_ops_color.svg")};
  }
  if (id.contains(QStringLiteral("blur")) ||
      id.contains(QStringLiteral("glow")) ||
      id.contains(QStringLiteral("shadow")) ||
      name.contains(QStringLiteral("blur")) ||
      name.contains(QStringLiteral("glow")) ||
      name.contains(QStringLiteral("shadow")) ||
      name.contains(QStringLiteral("bloom"))) {
    return {QStringLiteral("ブラー / ライト"), QStringLiteral("Studio/effect_ops_blur_light.svg")};
  }
  if (id.contains(QStringLiteral("distortion")) ||
      id.contains(QStringLiteral("displace")) ||
      id.contains(QStringLiteral("twist")) ||
      id.contains(QStringLiteral("bend")) ||
      id.contains(QStringLiteral("wave")) ||
      id.contains(QStringLiteral("spherize")) ||
      id.contains(QStringLiteral("optics")) ||
      id.contains(QStringLiteral("liquify")) ||
      name.contains(QStringLiteral("distort")) ||
      name.contains(QStringLiteral("wave")) ||
      name.contains(QStringLiteral("twist")) ||
      name.contains(QStringLiteral("bend")) ||
      name.contains(QStringLiteral("liquify"))) {
    return {QStringLiteral("ディストート"), QStringLiteral("Studio/effect_ops_distort.svg")};
  }
  if (id.contains(QStringLiteral("key")) ||
      name.contains(QStringLiteral("key"))) {
    return {QStringLiteral("キーイング / マット"), QStringLiteral("Studio/effect_ops_key.svg")};
  }
  if (id.contains(QStringLiteral("noise")) ||
      name.contains(QStringLiteral("noise")) ||
      name.contains(QStringLiteral("grain"))) {
    return {QStringLiteral("ノイズ"), QStringLiteral("Studio/effect_ops_noise.svg")};
  }
  if (id.contains(QStringLiteral("wipe")) ||
      name.contains(QStringLiteral("wipe"))) {
    return {QStringLiteral("トランジション"), QStringLiteral("Studio/effect_ops_transition.svg")};
  }
  return {QStringLiteral("スタイライズ / 生成"), QStringLiteral("Studio/effect_ops_generate.svg")};
}

QString iconForEffect(const EffectInfo& info)
{
  const QString id = info.id.toString();
  const QString name = info.displayName.toLower();
  if (id.contains(QStringLiteral("blur"))) return QStringLiteral("Studio/effect_ops_blur_light.svg");
  if (id.contains(QStringLiteral("glow")) || name.contains(QStringLiteral("bloom"))) return QStringLiteral("Studio/effect_ops_blur_light.svg");
  if (id.contains(QStringLiteral("shadow"))) return QStringLiteral("Studio/effect_ops_shadow.svg");
  if (id.contains(QStringLiteral("colorcorrection")) || name.contains(QStringLiteral("color"))) return QStringLiteral("Studio/effect_ops_color.svg");
  if (name.contains(QStringLiteral("brightness")) || name.contains(QStringLiteral("exposure"))) return QStringLiteral("Studio/effect_ops_blur_light.svg");
  if (name.contains(QStringLiteral("curves")) || name.contains(QStringLiteral("levels"))) return QStringLiteral("Studio/effect_ops_curve.svg");
  if (id.contains(QStringLiteral("key"))) return QStringLiteral("Studio/effect_ops_key.svg");
  if (id.contains(QStringLiteral("noise"))) return QStringLiteral("Studio/effect_ops_noise.svg");
  if (id.contains(QStringLiteral("wave")) || id.contains(QStringLiteral("twist")) || id.contains(QStringLiteral("displace"))) return QStringLiteral("Studio/effect_ops_distort.svg");
  if (id.startsWith(QStringLiteral("ofx."))) return QStringLiteral("Studio/effect_ops_ofx.svg");
  return QStringLiteral("Studio/effect_ops_generate.svg");
}

void forceMenuIconsVisible(QMenu* menu)
{
  if (!menu) {
    return;
  }
  if (auto* menuAction = menu->menuAction()) {
    menuAction->setIconVisibleInMenu(true);
  }
  for (QAction* action : menu->actions()) {
    if (!action) {
      continue;
    }
    action->setIconVisibleInMenu(true);
    if (QMenu* childMenu = action->menu()) {
      forceMenuIconsVisible(childMenu);
    }
  }
}
}

W_OBJECT_IMPL(ArtifactEffectMenu)

class ArtifactEffectMenu::Impl
{
 public:
  explicit Impl(ArtifactEffectMenu* menu);
  ~Impl();

  ArtifactEffectMenu* menu_ = nullptr;
  ArtifactCore::LayerID selectedLayerId_;
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  QAction* inspectorAction_ = nullptr;
  QAction* fxStudioAction_ = nullptr;
  QAction* removeAllAction_ = nullptr;
  QAction* ofxManagerAction_ = nullptr;
  std::vector<QMenu*> effectMenus_;
  std::vector<QAction*> effectActions_;

  ArtifactCore::LayerID currentTargetLayerId() const;
  void addEffectAction(QMenu* categoryMenu, const EffectInfo& info);
  void buildEffectCatalog();
  void handleAddEffect(const EffectInfo& info);
  void handleRemoveAllEffects();
  void requestRefreshEnabledState();
  void refreshEnabledState();
};

ArtifactEffectMenu::Impl::Impl(ArtifactEffectMenu* menu) : menu_(menu)
{
  inspectorAction_ = new QAction(QStringLiteral("エフェクトコントロール"), menu);
  inspectorAction_->setShortcut(
      ShortcutBindings::instance().shortcut(ShortcutId::EffectShowInspector));
  inspectorAction_->setIcon(menuIcon(QStringLiteral("Studio/effect_ops_control.svg")));
  fxStudioAction_ = new QAction(QStringLiteral("FX Studio…"), menu);

  removeAllAction_ = new QAction(QStringLiteral("選択レイヤーのエフェクトをすべて削除"), menu);
  removeAllAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_X));
  removeAllAction_->setIcon(menuIcon(QStringLiteral("Studio/effect_ops_remove_all.svg")));
  ofxManagerAction_ = new QAction(QStringLiteral("OFX Plugin Manager"), menu);
  ofxManagerAction_->setIcon(menuIcon(QStringLiteral("Studio/effect_ops_ofx.svg")));

  menu->addAction(inspectorAction_);
  menu->addAction(fxStudioAction_);
  menu->addSeparator();
  menu->addAction(removeAllAction_);
  menu->addAction(ofxManagerAction_);
  menu->addSeparator();
  buildEffectCatalog();
  forceMenuIconsVisible(menu);

  QObject::connect(inspectorAction_, &QAction::triggered, menu, [this]() {
      ArtifactCore::globalEventBus().publish(ShowEffectInspectorRequested{});
  });
  QObject::connect(fxStudioAction_, &QAction::triggered, menu, [this]() { ArtifactFxStudioDialog dialog(menu_); dialog.exec(); });

  QObject::connect(removeAllAction_, &QAction::triggered, menu, [this]() {
      handleRemoveAllEffects();
  });

  QObject::connect(ofxManagerAction_, &QAction::triggered, menu, [this]() {
      auto& host = Artifact::Ofx::ArtifactOfxHost::instance();
      host.initialize();
      QDialog dialog(menu_);
      dialog.setWindowTitle(QStringLiteral("OFX Plugin Manager"));
      dialog.setModal(true);
      dialog.resize(700, 490);
      dialog.setMinimumSize(620, 420);
      dialog.setAccessibleName(QStringLiteral("OFX Plugin Manager"));

      auto* layout = new QVBoxLayout(&dialog);
      layout->setContentsMargins(18, 16, 18, 16);
      layout->setSpacing(12);

      auto* header = new QHBoxLayout();
      header->setSpacing(10);
      auto* title = new QLabel(QStringLiteral("OFX Plug-ins"), &dialog);
      QFont titleFont = title->font();
      titleFont.setPointSizeF(titleFont.pointSizeF() + 2.0);
      titleFont.setWeight(QFont::DemiBold);
      title->setFont(titleFont);
      auto* loadedCount = new QLabel(&dialog);
      loadedCount->setForegroundRole(QPalette::PlaceholderText);
      loadedCount->setAccessibleName(QStringLiteral("Loaded OFX plug-in count"));
      header->addWidget(title);
      header->addWidget(loadedCount);
      header->addStretch(1);
      layout->addLayout(header);

      auto* contentFrame = new QFrame(&dialog);
      contentFrame->setFrameShape(QFrame::StyledPanel);
      contentFrame->setFrameShadow(QFrame::Plain);
      contentFrame->setAccessibleName(QStringLiteral("OFX plug-in list"));
      auto* contentLayout = new QVBoxLayout(contentFrame);
      contentLayout->setContentsMargins(0, 0, 0, 0);
      contentLayout->setSpacing(0);

      auto* pages = new QStackedWidget(contentFrame);
      auto* list = new QListWidget(pages);
      list->setFrameShape(QFrame::NoFrame);
      list->setSpacing(4);
      list->setUniformItemSizes(true);
      list->setAccessibleName(QStringLiteral("Loaded OFX plug-ins"));
      pages->addWidget(list);

      auto* emptyPage = new QWidget(pages);
      auto* emptyLayout = new QVBoxLayout(emptyPage);
      emptyLayout->setContentsMargins(32, 32, 32, 32);
      emptyLayout->setSpacing(8);
      emptyLayout->addStretch(1);

      auto* emptyIcon = new QLabel(emptyPage);
      emptyIcon->setAlignment(Qt::AlignCenter);
      emptyIcon->setPixmap(menuIcon(QStringLiteral("Studio/effect_ops_ofx.svg"))
                               .pixmap(52, 52));
      emptyIcon->setAccessibleName(QStringLiteral("OFX plug-in"));
      emptyLayout->addWidget(emptyIcon, 0, Qt::AlignHCenter);

      auto* emptyTitle = new QLabel(QStringLiteral("No OFX plug-ins found"), emptyPage);
      QFont emptyTitleFont = emptyTitle->font();
      emptyTitleFont.setPointSizeF(emptyTitleFont.pointSizeF() + 1.0);
      emptyTitleFont.setWeight(QFont::DemiBold);
      emptyTitle->setFont(emptyTitleFont);
      emptyTitle->setAlignment(Qt::AlignCenter);
      emptyLayout->addWidget(emptyTitle);

      auto* emptyHint = new QLabel(
          QStringLiteral("Install an OFX plug-in, then rescan to detect it."),
          emptyPage);
      emptyHint->setAlignment(Qt::AlignCenter);
      emptyHint->setForegroundRole(QPalette::PlaceholderText);
      emptyHint->setWordWrap(true);
      emptyLayout->addWidget(emptyHint);
      emptyLayout->addStretch(1);
      pages->addWidget(emptyPage);

      contentLayout->addWidget(pages);
      layout->addWidget(contentFrame, 1);

      const auto populate = [&]() {
          list->clear();
          for (const auto& plugin : host.getLoadedPlugins()) {
              const QString id = plugin.identifier.toQString().trimmed();
              if (id.isEmpty()) continue;
              const QString version = plugin.version.toQString().trimmed();
              const QString path = plugin.pluginPath.toQString().trimmed();
              auto* item = new QListWidgetItem(
                  version.isEmpty()
                      ? QStringLiteral("%1\n%2").arg(id, path)
                      : QStringLiteral("%1  (%2)\n%3").arg(id, version, path),
                  list);
              item->setSizeHint(QSize(item->sizeHint().width(), 52));
          }
          loadedCount->setText(QStringLiteral("%1 loaded").arg(list->count()));
          pages->setCurrentWidget(list->count() == 0 ? emptyPage : list);
      };
      populate();

      auto* footerRule = new QFrame(&dialog);
      footerRule->setFrameShape(QFrame::HLine);
      footerRule->setFrameShadow(QFrame::Plain);
      layout->addWidget(footerRule);

      auto* buttons = new QHBoxLayout();
      buttons->setSpacing(10);
      auto* rescan = new QPushButton(QStringLiteral("Rescan"), &dialog);
      auto* close = new QPushButton(QStringLiteral("Close"), &dialog);
      rescan->setMinimumSize(116, 36);
      rescan->setDefault(true);
      rescan->setAccessibleName(QStringLiteral("Rescan for OFX plug-ins"));
      close->setMinimumSize(110, 36);
      close->setAccessibleName(QStringLiteral("Close OFX Plugin Manager"));
      buttons->addWidget(rescan);
      buttons->addStretch(1);
      buttons->addWidget(close);
      layout->addLayout(buttons);
      QObject::connect(rescan, &QPushButton::clicked, &dialog, [&]() {
          host.rescan();
          populate();
      });
      QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
      dialog.exec();
  });

  auto& eventBus = ArtifactCore::globalEventBus();
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<LayerSelectionChangedEvent>(
          [this](const LayerSelectionChangedEvent& event) {
              const ArtifactCore::LayerID layerId(event.layerId);
              if (!event.compositionId.isEmpty()) {
                  auto* service = ArtifactProjectService::instance();
                  if (service) {
                      if (const auto comp = service->currentComposition().lock()) {
                          if (comp->id().toString() != event.compositionId) {
                              return;
                          }
                      }
                  }
              }
              selectedLayerId_ = layerId;
              requestRefreshEnabledState();
          }));
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<LayerChangedEvent>(
          [this](const LayerChangedEvent& event) {
              if (!event.compositionId.isEmpty()) {
                  auto* service = ArtifactProjectService::instance();
                  if (service) {
                      if (const auto comp = service->currentComposition().lock()) {
                          if (comp->id().toString() != event.compositionId) {
                              return;
                          }
                      }
                  }
              }
              if (event.changeType == LayerChangedEvent::ChangeType::Removed &&
                  selectedLayerId_ == ArtifactCore::LayerID(event.layerId)) {
                  selectedLayerId_ = {};
              }
              requestRefreshEnabledState();
          }));
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent&) {
              requestRefreshEnabledState();
          }));
  QObject::connect(menu, &QMenu::aboutToShow, menu, [this]() {
      refreshEnabledState();
  });
}

void ArtifactEffectMenu::Impl::requestRefreshEnabledState()
{
  if (!menu_) {
    return;
  }
  if (QThread::currentThread() == menu_->thread()) {
    refreshEnabledState();
    return;
  }
  QMetaObject::invokeMethod(menu_, [this]() {
    refreshEnabledState();
  }, Qt::QueuedConnection);
}

ArtifactEffectMenu::Impl::~Impl() = default;

ArtifactCore::LayerID ArtifactEffectMenu::Impl::currentTargetLayerId() const
{
  if (!selectedLayerId_.isNil()) {
    return selectedLayerId_;
  }
  if (auto* selection = ArtifactLayerSelectionManager::instance()) {
    if (auto layer = selection->currentLayer()) {
      return layer->id();
    }
  }
  return ArtifactCore::LayerID::Nil();
}

void ArtifactEffectMenu::Impl::addEffectAction(QMenu* categoryMenu, const EffectInfo& info)
{
  QAction* action = categoryMenu->addAction(menuIcon(iconForEffect(info)), info.displayName);
  action->setData(info.id.toString());
  effectActions_.push_back(action);
  QObject::connect(action, &QAction::triggered, menu_, [this, info]() {
      handleAddEffect(info);
  });
}

void ArtifactEffectMenu::Impl::buildEffectCatalog()
{
  auto* effectService = ArtifactEffectService::instance();
  if (!effectService) {
    return;
  }

  QHash<QString, QMenu*> categoryMenus;
  const auto effects = effectService->availableEffects();
  for (const auto& info : effects) {
    const auto category = categoryForEffect(info);
    QMenu* categoryMenu = categoryMenus.value(category.title, nullptr);
    if (!categoryMenu) {
      categoryMenu = new QMenu(category.title, menu_);
      categoryMenu->setIcon(menuIcon(category.iconPath));
      categoryMenus.insert(category.title, categoryMenu);
      effectMenus_.push_back(categoryMenu);
      menu_->addMenu(categoryMenu);
    }
    addEffectAction(categoryMenu, info);
  }
}

void ArtifactEffectMenu::Impl::handleAddEffect(const EffectInfo& info)
{
  auto* effectService = ArtifactEffectService::instance();
  const auto layerId = currentTargetLayerId();
  if (!effectService || layerId.isNil()) {
    return;
  }
  const auto result = effectService->addEffectToLayer(layerId, info.id);
  if (!result.success) {
    qWarning() << "[EffectMenu] failed to add effect" << info.id.toString() << result.message;
  }
  refreshEnabledState();
}

void ArtifactEffectMenu::Impl::handleRemoveAllEffects()
{
  auto* effectService = ArtifactEffectService::instance();
  auto* projectService = ArtifactProjectService::instance();
  const auto layerId = currentTargetLayerId();
  if (!effectService || !projectService || layerId.isNil()) {
    return;
  }
  auto comp = projectService->currentComposition().lock();
  auto layer = comp ? comp->layerById(layerId) : ArtifactAbstractLayerPtr{};
  if (!layer) {
    return;
  }

  std::vector<QString> effectIds;
  for (const auto& effect : layer->getEffects()) {
    if (effect) {
      effectIds.push_back(effect->effectID().toQString());
    }
  }
  for (const auto& effectId : effectIds) {
    effectService->removeEffectFromLayer(layerId, effectId);
  }
  refreshEnabledState();
}

void ArtifactEffectMenu::Impl::refreshEnabledState()
{
  auto* service = ArtifactProjectService::instance();
  const bool hasLayer = service && service->hasProject() &&
      static_cast<bool>(service->currentComposition().lock()) &&
      !currentTargetLayerId().isNil();

  inspectorAction_->setEnabled(hasLayer);
  fxStudioAction_->setEnabled(hasLayer);
  removeAllAction_->setEnabled(hasLayer);
  for (QMenu* effectMenu : effectMenus_) {
    if (effectMenu) {
      effectMenu->setEnabled(hasLayer);
    }
  }
  for (QAction* action : effectActions_) {
    if (action) {
      action->setEnabled(hasLayer);
    }
  }
}

ArtifactEffectMenu::ArtifactEffectMenu(QWidget* parent /*= nullptr*/)
    : QMenu(parent), impl_(new Impl(this))
{
  setTitle(TranslationManager::instance().tr(QStringLiteral("menu.effect.label"), QStringLiteral("エフェクト(&F)")));
  setTearOffEnabled(false);
  impl_->refreshEnabledState();
}

ArtifactEffectMenu::~ArtifactEffectMenu()
{
  delete impl_;
}

};

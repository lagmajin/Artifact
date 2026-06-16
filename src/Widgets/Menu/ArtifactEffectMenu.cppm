module;
#include <utility>
#include <QAction>
#include <QDebug>
#include <QHash>
#include <QIcon>
#include <QMenu>
#include <QKeySequence>
#include <QWidget>
#include <wobjectimpl.h>

module Artifact.Menu.Effect;
import std;

import Event.Bus;
import Artifact.Event.Types;
import Artifact.Service.Effect;
import Artifact.Service.Project;
import Artifact.Effect.Abstract;
import Artifact.Layers.Selection.Manager;
import Artifact.Layer.Abstract;
import Utils.Id;
import Utils.Path;

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
      name.contains(QStringLiteral("distort")) ||
      name.contains(QStringLiteral("wave")) ||
      name.contains(QStringLiteral("twist")) ||
      name.contains(QStringLiteral("bend"))) {
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
  QAction* removeAllAction_ = nullptr;
  std::vector<QMenu*> effectMenus_;
  std::vector<QAction*> effectActions_;

  ArtifactCore::LayerID currentTargetLayerId() const;
  void addEffectAction(QMenu* categoryMenu, const EffectInfo& info);
  void buildEffectCatalog();
  void handleAddEffect(const EffectInfo& info);
  void handleRemoveAllEffects();
  void refreshEnabledState();
};

ArtifactEffectMenu::Impl::Impl(ArtifactEffectMenu* menu) : menu_(menu)
{
  inspectorAction_ = new QAction(QStringLiteral("エフェクトコントロール"), menu);
  inspectorAction_->setShortcut(QKeySequence(Qt::Key_F3));
  inspectorAction_->setIcon(menuIcon(QStringLiteral("Studio/effect_ops_control.svg")));

  removeAllAction_ = new QAction(QStringLiteral("選択レイヤーのエフェクトをすべて削除"), menu);
  removeAllAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_X));
  removeAllAction_->setIcon(menuIcon(QStringLiteral("Studio/effect_ops_remove_all.svg")));

  menu->addAction(inspectorAction_);
  menu->addSeparator();
  menu->addAction(removeAllAction_);
  menu->addSeparator();
  buildEffectCatalog();
  forceMenuIconsVisible(menu);

  QObject::connect(removeAllAction_, &QAction::triggered, menu, [this]() {
      handleRemoveAllEffects();
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
              refreshEnabledState();
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
              refreshEnabledState();
          }));
  eventBusSubscriptions_.push_back(
      eventBus.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent&) {
              refreshEnabledState();
          }));
  QObject::connect(menu, &QMenu::aboutToShow, menu, [this]() {
      refreshEnabledState();
  });
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
  setTitle(QStringLiteral("エフェクト(&T)"));
  setIcon(menuIcon(QStringLiteral("Studio/menubar_effect.svg")));
  setTearOffEnabled(false);
  impl_->refreshEnabledState();
}

ArtifactEffectMenu::~ArtifactEffectMenu()
{
  delete impl_;
}

};

module;
#include <utility>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QHash>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QUuid>
#include <QAbstractItemView>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <wobjectimpl.h>

module Artifact.Widgets.ReactiveEventEditorWindow;

import std;
import Artifact.Widgets.ReactiveEventEditorWindow;
import Artifact.Event.Types;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Project.Items;
import Event.Bus;
import Reactive.Events;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

namespace {
constexpr int kRoleRuleIndex = Qt::UserRole + 1;
constexpr int kRoleTypeText = Qt::UserRole + 2;
constexpr int kRoleDetailText = Qt::UserRole + 3;
constexpr int kRoleBadgeColor = Qt::UserRole + 4;

QString typeLabelForProjectItem(eProjectItemType type)
{
 switch (type) {
 case eProjectItemType::Folder:
  return QStringLiteral("Folder");
 case eProjectItemType::Composition:
  return QStringLiteral("Comp");
 case eProjectItemType::Footage:
  return QStringLiteral("Footage");
 case eProjectItemType::Solid:
  return QStringLiteral("Solid");
 default:
  return QStringLiteral("Item");
 }
}

QString typeLabelForLayer(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer) {
  return QStringLiteral("Layer");
 }
 if (layer->is3D()) {
  return QStringLiteral("3D Model Layer");
 }
 return layer->className().toQString();
}

QColor typeColorForLabel(const QString& label)
{
 if (label.contains("Comp", Qt::CaseInsensitive)) {
  return QColor("#5FA8D3");
 }
 if (label.contains("Folder", Qt::CaseInsensitive)) {
  return QColor("#A88C5F");
 }
 if (label.contains("Footage", Qt::CaseInsensitive) || label.contains("Image", Qt::CaseInsensitive) ||
     label.contains("Video", Qt::CaseInsensitive)) {
  return QColor("#D39B5F");
 }
 if (label.contains("Solid", Qt::CaseInsensitive)) {
  return QColor("#7BC96F");
 }
 if (label.contains("Text", Qt::CaseInsensitive)) {
  return QColor("#D17ED8");
 }
 if (label.contains("Mask", Qt::CaseInsensitive)) {
  return QColor("#69C6B8");
 }
 return QColor("#87909C");
}

QString summarizeTrigger(const ReactiveRule& rule)
{
 const auto& t = rule.trigger;
 switch (t.type) {
 case TriggerEventType::OnValueExceed:
  return QStringLiteral("When: %1 > %2").arg(t.propertyPath).arg(t.valueThreshold);
 case TriggerEventType::OnValueDrop:
  return QStringLiteral("When: %1 < %2").arg(t.propertyPath).arg(t.valueThreshold);
 case TriggerEventType::OnValueCross:
  return QStringLiteral("When: %1 crosses %2").arg(t.propertyPath).arg(t.valueThreshold);
 case TriggerEventType::OnFrame:
  return QStringLiteral("When: Frame %1").arg(t.frameNumber);
 case TriggerEventType::OnContact:
  return QStringLiteral("When: %1 touches %2").arg(t.sourceLayerId, t.targetLayerId);
 case TriggerEventType::OnSeparation:
  return QStringLiteral("When: %1 separates from %2").arg(t.sourceLayerId, t.targetLayerId);
 case TriggerEventType::OnProximity:
  return QStringLiteral("When: %1 near %2").arg(t.sourceLayerId, t.targetLayerId);
 case TriggerEventType::OnStart:
  return QStringLiteral("When: %1 starts").arg(t.sourceLayerId);
 case TriggerEventType::OnEnd:
  return QStringLiteral("When: %1 ends").arg(t.sourceLayerId);
 case TriggerEventType::OnEnterRange:
  return QStringLiteral("When: %1 enters range").arg(t.sourceLayerId);
 case TriggerEventType::OnExitRange:
  return QStringLiteral("When: %1 exits range").arg(t.sourceLayerId);
 case TriggerEventType::OnLoop:
  return QStringLiteral("When: %1 loops").arg(t.sourceLayerId);
 default:
  return QStringLiteral("When: None");
 }
}

QString summarizeReaction(const ReactiveRule& rule)
{
 if (rule.reactions.empty()) {
  return QStringLiteral("Do: None");
 }
 const auto& reaction = rule.reactions.front();
 switch (reaction.type) {
 case ReactionType::SetProperty:
  return QStringLiteral("Do: Set %1 = %2").arg(reaction.propertyPath, reaction.value.toString());
 case ReactionType::AnimateProperty:
  return QStringLiteral("Do: Animate %1 -> %2").arg(reaction.propertyPath, reaction.value.toString());
 case ReactionType::SpawnLayer:
  return QStringLiteral("Do: Spawn %1").arg(reaction.spawnLayerType);
 case ReactionType::DestroyLayer:
  return QStringLiteral("Do: Destroy %1").arg(reaction.targetLayerId);
 case ReactionType::GoToFrame:
  return QStringLiteral("Do: Go to frame %1").arg(reaction.targetFrame);
 case ReactionType::PlayAnimation:
  return QStringLiteral("Do: Play animation");
 case ReactionType::PauseAnimation:
  return QStringLiteral("Do: Pause animation");
 case ReactionType::ApplyForce:
  return QStringLiteral("Do: Apply force");
 case ReactionType::ApplyImpulse:
  return QStringLiteral("Do: Apply impulse");
 case ReactionType::Attract:
  return QStringLiteral("Do: Attract");
 case ReactionType::Repel:
  return QStringLiteral("Do: Repel");
 case ReactionType::PlaySound:
  return QStringLiteral("Do: Play sound");
 default:
  return QStringLiteral("Do: %1").arg(reactionTypeName(reaction.type));
 }
}

class ReactiveTargetDelegate final : public QStyledItemDelegate {
public:
 explicit ReactiveTargetDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

 QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
 {
  QSize base = QStyledItemDelegate::sizeHint(option, index);
  base.setHeight(qMax(base.height(), 42));
  return base;
 }

 void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
 {
  painter->save();

  const QString title = index.data(Qt::DisplayRole).toString();
  const QString detail = index.data(kRoleDetailText).toString();
  const QString badge = index.data(kRoleTypeText).toString();
  const QColor badgeColor = index.data(kRoleBadgeColor).value<QColor>();

  QRect rect = option.rect.adjusted(4, 4, -4, -4);
  QColor bg = QColor("#23262B");
  if (option.state & QStyle::State_Selected) {
   bg = QColor("#37465B");
  } else if (option.state & QStyle::State_MouseOver) {
   bg = QColor("#2B2F35");
  }
  painter->fillRect(option.rect, bg);

  const int indicatorWidth = 3;
  painter->fillRect(QRect(rect.left(), rect.top(), indicatorWidth, rect.height()), badgeColor);

  QRect content = rect.adjusted(indicatorWidth + 8, 4, -8, -4);
  QRect badgeRect = content;
  badgeRect.setLeft(content.right() - 84);
  badgeRect.setWidth(84);
  QRect textRect = content;
  textRect.setRight(badgeRect.left() - 8);

  QFont titleFont = option.font;
  titleFont.setBold(true);
  QFont detailFont = option.font;
  detailFont.setPointSize(qMax(8, detailFont.pointSize() - 1));

  painter->setPen(QColor("#F1F5F9"));
  painter->setFont(titleFont);
  painter->drawText(textRect.adjusted(0, 0, 0, -12), Qt::AlignLeft | Qt::AlignVCenter, title);

  painter->setPen(QColor("#AEB8C4"));
  painter->setFont(detailFont);
  painter->drawText(textRect.adjusted(0, 12, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, detail);

  if (!badge.isEmpty()) {
   painter->setPen(Qt::NoPen);
   painter->setBrush(badgeColor);
   QRect badgeChip = badgeRect.adjusted(0, 8, 0, -8);
   painter->drawRoundedRect(badgeChip, 8, 8);
   painter->setPen(QColor("#111111"));
   painter->drawText(badgeChip, Qt::AlignCenter, badge);
  }

  painter->restore();
 }
};
} // namespace

W_OBJECT_IMPL(ArtifactReactiveEventEditorWindow)

class ArtifactReactiveEventEditorWindow::Impl {
public:
 QTreeWidget* targetTree = nullptr;
 QTreeWidget* ruleTree = nullptr;
 QWidget* inspectorContainer = nullptr;
 QPlainTextEdit* eventLog = nullptr;
 QLabel* statusLabel = nullptr;

 QLineEdit* nameEdit = nullptr;
 QComboBox* triggerTypeEdit = nullptr;
 QLineEdit* sourceLayerEdit = nullptr;
 QLineEdit* targetLayerEdit = nullptr;
 QLineEdit* propertyPathEdit = nullptr;
 QDoubleSpinBox* valueThresholdEdit = nullptr;
 QSpinBox* frameNumberEdit = nullptr;
 QDoubleSpinBox* delayEdit = nullptr;
 QDoubleSpinBox* cooldownEdit = nullptr;
 QCheckBox* enabledCheck = nullptr;
 QCheckBox* onceCheck = nullptr;
 QCheckBox* repeatingCheck = nullptr;
 QLabel* ruleIdLabel = nullptr;
 QLabel* lastFiredLabel = nullptr;
 QLabel* reactionLabel = nullptr;
 QPushButton* addRuleButton = nullptr;
 QPushButton* duplicateRuleButton = nullptr;
 QPushButton* removeRuleButton = nullptr;
 QPushButton* refreshButton = nullptr;
 ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
 std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

 std::vector<ReactiveRule> rules;
 int selectedRuleIndex = -1;
 bool updatingInspector = false;

 void rebuildTargetTree();
 void rebuildRuleTree();
 void rebuildInspector();
 void loadSampleRules();
 void refreshAll();
 void selectRule(int index);
 void applyInspectorToRule();
 void appendLog(const QString& text);
 ReactiveRule defaultRuleForCurrentSelection() const;
 ReactiveRule* currentRule();
 const ReactiveRule* currentRule() const;
};

ReactiveRule ArtifactReactiveEventEditorWindow::Impl::defaultRuleForCurrentSelection() const
{
 ReactiveRule rule;
 rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
 rule.name = QStringLiteral("New Rule");
 rule.enabled = true;
 rule.trigger.type = TriggerEventType::OnFrame;
 rule.trigger.frameNumber = 0;
 rule.reactions.push_back(Reaction{});
 return rule;
}

ReactiveRule* ArtifactReactiveEventEditorWindow::Impl::currentRule()
{
 if (selectedRuleIndex < 0 || selectedRuleIndex >= static_cast<int>(rules.size())) {
  return nullptr;
 }
 return &rules[static_cast<size_t>(selectedRuleIndex)];
}

const ReactiveRule* ArtifactReactiveEventEditorWindow::Impl::currentRule() const
{
 if (selectedRuleIndex < 0 || selectedRuleIndex >= static_cast<int>(rules.size())) {
  return nullptr;
 }
 return &rules[static_cast<size_t>(selectedRuleIndex)];
}

void ArtifactReactiveEventEditorWindow::Impl::appendLog(const QString& text)
{
 if (!eventLog) {
  return;
 }
 const QString stamp = QDateTime::currentDateTime().toString("hh:mm:ss");
 eventLog->appendPlainText(QStringLiteral("[%1] %2").arg(stamp, text));
}

void ArtifactReactiveEventEditorWindow::Impl::loadSampleRules()
{
 rules.clear();

 ReactiveRule fadeIn;
 fadeIn.id = QStringLiteral("rule-fadein");
 fadeIn.name = QStringLiteral("FadeInTrigger");
 fadeIn.enabled = true;
 fadeIn.trigger.type = TriggerEventType::OnValueExceed;
 fadeIn.trigger.sourceLayerId = QStringLiteral("Layer 1");
 fadeIn.trigger.propertyPath = QStringLiteral("opacity");
 fadeIn.trigger.valueThreshold = 0.5f;
 fadeIn.cooldown = 0.0f;
 Reaction fadeReaction;
 fadeReaction.type = ReactionType::SetProperty;
 fadeReaction.targetLayerId = QStringLiteral("Layer 2");
 fadeReaction.propertyPath = QStringLiteral("effects.glow.enabled");
 fadeReaction.value = true;
 fadeIn.reactions.push_back(fadeReaction);
 rules.push_back(fadeIn);

 ReactiveRule breakRule;
 breakRule.id = QStringLiteral("rule-outoframe");
 breakRule.name = QStringLiteral("OutOfFrameBreak");
 breakRule.enabled = true;
 breakRule.trigger.type = TriggerEventType::OnExitRange;
 breakRule.trigger.sourceLayerId = QStringLiteral("Layer 1");
 Reaction breakReaction;
 breakReaction.type = ReactionType::DestroyLayer;
 breakReaction.targetLayerId = QStringLiteral("Layer 3");
 breakRule.reactions.push_back(breakReaction);
 rules.push_back(breakRule);
}

void ArtifactReactiveEventEditorWindow::Impl::rebuildTargetTree()
{
 if (!targetTree) {
  return;
 }

 targetTree->clear();

 auto* svc = ArtifactProjectService::instance();
 if (!svc || !svc->hasProject()) {
  auto* item = new QTreeWidgetItem(targetTree);
  item->setText(0, QStringLiteral("No project"));
  item->setData(0, kRoleTypeText, QStringLiteral("Empty"));
  item->setData(0, kRoleDetailText, QStringLiteral("Create or open a project"));
  item->setData(0, kRoleBadgeColor, QColor("#646C7A"));
  return;
 }

 const QVector<ProjectItem*> roots = svc->projectItems();
 auto addProjectNode = [&](auto&& self, ProjectItem* src, QTreeWidgetItem* parent) -> void {
  if (!src) {
   return;
  }
  auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(targetTree);
  const QString title = src->name.toQString();
  const QString kind = typeLabelForProjectItem(src->type());
  const QString detail = QStringLiteral("ID: %1").arg(src->id.toString());
  item->setText(0, title.isEmpty() ? kind : title);
  item->setData(0, kRoleTypeText, kind);
  item->setData(0, kRoleDetailText, detail);
  item->setData(0, kRoleBadgeColor, typeColorForLabel(kind));
  for (ProjectItem* child : src->children) {
   self(self, child, item);
  }
 };

 for (ProjectItem* root : roots) {
  addProjectNode(addProjectNode, root, nullptr);
 }

 ArtifactCompositionWeakPtr weakComp = svc->currentComposition();
 auto comp = weakComp.lock();
 if (comp) {
  auto* currentCompRoot = new QTreeWidgetItem(targetTree);
  const QString compName = comp->settings().compositionName().toQString();
  currentCompRoot->setText(0, compName.isEmpty() ? QStringLiteral("Current Composition") : compName);
  currentCompRoot->setData(0, kRoleTypeText, QStringLiteral("Comp"));
  currentCompRoot->setData(0, kRoleDetailText, QStringLiteral("Current composition layers"));
  currentCompRoot->setData(0, kRoleBadgeColor, QColor("#5FA8D3"));
  const auto layers = comp->allLayer();
  QHash<QString, QTreeWidgetItem*> layerNodes;
  for (const auto& layer : layers) {
   if (!layer) {
    continue;
   }
   const QString id = layer->id().toString();
   const QString parentId = layer->parentLayerId().toString();
   QTreeWidgetItem* parentItem = currentCompRoot;
   if (!parentId.isEmpty() && layerNodes.contains(parentId)) {
    parentItem = layerNodes.value(parentId);
   }
   auto* item = new QTreeWidgetItem(parentItem);
   const QString layerName = layer->layerName();
   const QString kind = typeLabelForLayer(layer);
   item->setText(0, layerName.isEmpty() ? kind : layerName);
   item->setData(0, kRoleTypeText, kind);
   item->setData(0, kRoleDetailText,
                 QStringLiteral("Vis:%1  Op:%2  ID:%3")
                     .arg(layer->isVisible() ? QStringLiteral("Y") : QStringLiteral("N"))
                     .arg(layer->opacity(), 0, 'f', 2)
                     .arg(id));
   item->setData(0, kRoleBadgeColor, typeColorForLabel(kind));
   layerNodes.insert(id, item);
  }
  currentCompRoot->setExpanded(true);
 }

 targetTree->expandToDepth(1);
}

void ArtifactReactiveEventEditorWindow::Impl::rebuildRuleTree()
{
 if (!ruleTree) {
  return;
 }

 QSignalBlocker blocker(ruleTree);
 ruleTree->clear();
 for (size_t i = 0; i < rules.size(); ++i) {
  const ReactiveRule& rule = rules[i];
  auto* item = new QTreeWidgetItem(ruleTree);
  item->setText(0, rule.name.isEmpty() ? QStringLiteral("Unnamed Rule") : rule.name);
  item->setData(0, kRoleRuleIndex, static_cast<int>(i));
  item->setData(0, kRoleTypeText, rule.enabled ? QStringLiteral("ON") : QStringLiteral("OFF"));
  item->setData(0, kRoleDetailText, summarizeTrigger(rule) + QStringLiteral("\n") + summarizeReaction(rule));
  item->setData(0, kRoleBadgeColor, rule.enabled ? QColor("#7BC96F") : QColor("#E16B5B"));
  item->setCheckState(0, rule.enabled ? Qt::Checked : Qt::Unchecked);
 }
 if (selectedRuleIndex >= 0 && selectedRuleIndex < static_cast<int>(rules.size())) {
  if (auto* item = ruleTree->topLevelItem(selectedRuleIndex)) {
   ruleTree->setCurrentItem(item);
  }
 }
}

void ArtifactReactiveEventEditorWindow::Impl::rebuildInspector()
{
 updatingInspector = true;

 const ReactiveRule* rule = currentRule();
 const bool hasRule = rule != nullptr;
 if (enabledCheck) enabledCheck->setChecked(hasRule ? rule->enabled : false);
 if (nameEdit) nameEdit->setText(hasRule ? rule->name : QString());
 if (triggerTypeEdit) {
  const int idx = hasRule ? triggerTypeEdit->findData(static_cast<int>(rule->trigger.type)) : 0;
  triggerTypeEdit->setCurrentIndex(qMax(0, idx));
 }
 if (sourceLayerEdit) sourceLayerEdit->setText(hasRule ? rule->trigger.sourceLayerId : QString());
 if (targetLayerEdit) targetLayerEdit->setText(hasRule ? rule->trigger.targetLayerId : QString());
 if (propertyPathEdit) propertyPathEdit->setText(hasRule ? rule->trigger.propertyPath : QString());
 if (valueThresholdEdit) valueThresholdEdit->setValue(hasRule ? rule->trigger.valueThreshold : 0.0);
 if (frameNumberEdit) frameNumberEdit->setValue(hasRule ? static_cast<int>(rule->trigger.frameNumber) : 0);
 if (delayEdit) delayEdit->setValue(hasRule ? rule->delay : 0.0);
 if (cooldownEdit) cooldownEdit->setValue(hasRule ? rule->cooldown : 0.0);
 if (onceCheck) onceCheck->setChecked(hasRule ? rule->once : false);
 if (repeatingCheck) repeatingCheck->setChecked(hasRule ? rule->repeating : false);
 if (ruleIdLabel) ruleIdLabel->setText(hasRule ? rule->id : QStringLiteral("-"));
 if (lastFiredLabel) {
  lastFiredLabel->setText(hasRule ? QString::number(rule->lastFiredFrame) : QStringLiteral("-"));
 }
 if (reactionLabel) {
  reactionLabel->setText(hasRule ? summarizeReaction(*rule) : QStringLiteral("Do: None"));
 }

 updatingInspector = false;
}

void ArtifactReactiveEventEditorWindow::Impl::applyInspectorToRule()
{
 if (updatingInspector) {
  return;
 }

 ReactiveRule* rule = currentRule();
 if (!rule) {
  return;
 }

 rule->enabled = enabledCheck ? enabledCheck->isChecked() : rule->enabled;
 rule->name = nameEdit ? nameEdit->text() : rule->name;
 if (triggerTypeEdit) {
  rule->trigger.type = static_cast<TriggerEventType>(triggerTypeEdit->currentData().toInt());
 }
 if (sourceLayerEdit) rule->trigger.sourceLayerId = sourceLayerEdit->text();
 if (targetLayerEdit) rule->trigger.targetLayerId = targetLayerEdit->text();
 if (propertyPathEdit) rule->trigger.propertyPath = propertyPathEdit->text();
 if (valueThresholdEdit) rule->trigger.valueThreshold = static_cast<float>(valueThresholdEdit->value());
 if (frameNumberEdit) rule->trigger.frameNumber = frameNumberEdit->value();
 if (delayEdit) rule->delay = static_cast<float>(delayEdit->value());
 if (cooldownEdit) rule->cooldown = static_cast<float>(cooldownEdit->value());
 if (onceCheck) rule->once = onceCheck->isChecked();
 if (repeatingCheck) rule->repeating = repeatingCheck->isChecked();

 rebuildRuleTree();
 rebuildInspector();
 appendLog(QStringLiteral("Rule updated: %1").arg(rule->name));
}

void ArtifactReactiveEventEditorWindow::Impl::refreshAll()
{
 rebuildTargetTree();
 if (rules.empty()) {
  loadSampleRules();
 }
 rebuildRuleTree();
 rebuildInspector();
 if (statusLabel) {
  statusLabel->setText(QStringLiteral("Rules: %1  Selected: %2")
                           .arg(rules.size())
                           .arg(selectedRuleIndex >= 0 ? QString::number(selectedRuleIndex + 1)
                                                       : QStringLiteral("-")));
 }
 appendLog(QStringLiteral("Reactive editor refreshed"));
}

void ArtifactReactiveEventEditorWindow::Impl::selectRule(int index)
{
 if (index < 0 || index >= static_cast<int>(rules.size())) {
  selectedRuleIndex = -1;
  rebuildInspector();
  return;
 }
 selectedRuleIndex = index;
 rebuildInspector();
 if (const auto* rule = currentRule()) {
  appendLog(QStringLiteral("Selected rule: %1").arg(rule->name));
 }
}

ArtifactReactiveEventEditorWindow::ArtifactReactiveEventEditorWindow(QWidget* parent)
 : QMainWindow(parent), impl_(new Impl())
{
 setObjectName(QStringLiteral("ArtifactReactiveEventEditorWindow"));
 setWindowTitle(QStringLiteral("Reactive Event Editor"));
 resize(1440, 900);

 auto* host = new QWidget(this);
 host->setAutoFillBackground(true);
 QPalette hostPalette = host->palette();
 hostPalette.setColor(QPalette::Window, QColor(24, 26, 31));
 hostPalette.setColor(QPalette::WindowText, QColor(229, 231, 235));
 host->setPalette(hostPalette);
 auto* rootLayout = new QVBoxLayout(host);
 rootLayout->setContentsMargins(8, 8, 8, 8);
 rootLayout->setSpacing(8);

 auto* upperSplitter = new QSplitter(Qt::Horizontal, host);
 upperSplitter->setChildrenCollapsible(false);

 impl_->targetTree = new QTreeWidget(upperSplitter);
 impl_->targetTree->setHeaderHidden(true);
 impl_->targetTree->setRootIsDecorated(true);
 impl_->targetTree->setIndentation(18);
 impl_->targetTree->setAlternatingRowColors(false);
 impl_->targetTree->setUniformRowHeights(false);
 impl_->targetTree->setSelectionMode(QAbstractItemView::SingleSelection);
 impl_->targetTree->setMouseTracking(true);
 impl_->targetTree->setItemDelegate(new ReactiveTargetDelegate(impl_->targetTree));
 QPalette targetPalette = impl_->targetTree->palette();
 targetPalette.setColor(QPalette::Base, QColor(29, 32, 38));
 targetPalette.setColor(QPalette::Window, QColor(29, 32, 38));
 impl_->targetTree->setPalette(targetPalette);

 auto* centerPanel = new QWidget(upperSplitter);
 auto* centerLayout = new QVBoxLayout(centerPanel);
 centerLayout->setContentsMargins(0, 0, 0, 0);
 centerLayout->setSpacing(6);

 auto* buttonRow = new QHBoxLayout();
 impl_->addRuleButton = new QPushButton(QStringLiteral("Add"), centerPanel);
 impl_->duplicateRuleButton = new QPushButton(QStringLiteral("Duplicate"), centerPanel);
 impl_->removeRuleButton = new QPushButton(QStringLiteral("Remove"), centerPanel);
 impl_->refreshButton = new QPushButton(QStringLiteral("Refresh"), centerPanel);
 buttonRow->addWidget(impl_->addRuleButton);
 buttonRow->addWidget(impl_->duplicateRuleButton);
 buttonRow->addWidget(impl_->removeRuleButton);
 buttonRow->addStretch(1);
 buttonRow->addWidget(impl_->refreshButton);
 centerLayout->addLayout(buttonRow);

 impl_->ruleTree = new QTreeWidget(centerPanel);
 impl_->ruleTree->setColumnCount(1);
 impl_->ruleTree->setHeaderHidden(true);
 impl_->ruleTree->setSelectionMode(QAbstractItemView::SingleSelection);
 impl_->ruleTree->setMouseTracking(true);
 QPalette rulePalette = impl_->ruleTree->palette();
 rulePalette.setColor(QPalette::Base, QColor(32, 36, 43));
 rulePalette.setColor(QPalette::Window, QColor(32, 36, 43));
 impl_->ruleTree->setPalette(rulePalette);
 centerLayout->addWidget(impl_->ruleTree, 1);

 auto* inspectorScroll = new QScrollArea(upperSplitter);
 inspectorScroll->setWidgetResizable(true);
 inspectorScroll->setFrameShape(QFrame::NoFrame);
 impl_->inspectorContainer = new QWidget(inspectorScroll);
 inspectorScroll->setWidget(impl_->inspectorContainer);
 inspectorScroll->setAutoFillBackground(true);
 QPalette inspectorScrollPalette = inspectorScroll->palette();
 inspectorScrollPalette.setColor(QPalette::Window, QColor(28, 32, 38));
 inspectorScroll->setPalette(inspectorScrollPalette);
 auto* inspectorLayout = new QVBoxLayout(impl_->inspectorContainer);
 inspectorLayout->setContentsMargins(12, 12, 12, 12);
 inspectorLayout->setSpacing(8);

 auto* inspectorTitle = new QLabel(QStringLiteral("Inspector"), impl_->inspectorContainer);
 QFont inspectorTitleFont = inspectorTitle->font();
 inspectorTitleFont.setBold(true);
 inspectorTitleFont.setPointSize(14);
 inspectorTitle->setFont(inspectorTitleFont);
 QPalette inspectorTitlePalette = inspectorTitle->palette();
 inspectorTitlePalette.setColor(QPalette::WindowText, QColor(243, 244, 246));
 inspectorTitle->setPalette(inspectorTitlePalette);
 inspectorLayout->addWidget(inspectorTitle);

 impl_->enabledCheck = new QCheckBox(QStringLiteral("Enabled"), impl_->inspectorContainer);
 impl_->nameEdit = new QLineEdit(impl_->inspectorContainer);
 impl_->triggerTypeEdit = new QComboBox(impl_->inspectorContainer);
 for (int i = static_cast<int>(TriggerEventType::None); i <= static_cast<int>(TriggerEventType::OnFrame); ++i) {
  impl_->triggerTypeEdit->addItem(QString::fromLatin1(triggerEventTypeName(static_cast<TriggerEventType>(i))), i);
 }
 impl_->sourceLayerEdit = new QLineEdit(impl_->inspectorContainer);
 impl_->targetLayerEdit = new QLineEdit(impl_->inspectorContainer);
 impl_->propertyPathEdit = new QLineEdit(impl_->inspectorContainer);
 impl_->valueThresholdEdit = new QDoubleSpinBox(impl_->inspectorContainer);
 impl_->valueThresholdEdit->setDecimals(3);
 impl_->valueThresholdEdit->setRange(-100000.0, 100000.0);
 impl_->frameNumberEdit = new QSpinBox(impl_->inspectorContainer);
 impl_->frameNumberEdit->setRange(-1000000, 1000000);
 impl_->delayEdit = new QDoubleSpinBox(impl_->inspectorContainer);
 impl_->delayEdit->setRange(0.0, 100000.0);
 impl_->delayEdit->setDecimals(3);
 impl_->cooldownEdit = new QDoubleSpinBox(impl_->inspectorContainer);
 impl_->cooldownEdit->setRange(0.0, 100000.0);
 impl_->cooldownEdit->setDecimals(3);
 impl_->onceCheck = new QCheckBox(QStringLiteral("Once"), impl_->inspectorContainer);
 impl_->repeatingCheck = new QCheckBox(QStringLiteral("Repeating"), impl_->inspectorContainer);
 impl_->ruleIdLabel = new QLabel(QStringLiteral("-"), impl_->inspectorContainer);
 impl_->lastFiredLabel = new QLabel(QStringLiteral("-"), impl_->inspectorContainer);
 impl_->reactionLabel = new QLabel(QStringLiteral("Do: None"), impl_->inspectorContainer);
 impl_->reactionLabel->setWordWrap(true);

 auto* form = new QFormLayout();
 form->setLabelAlignment(Qt::AlignLeft);
 form->setFormAlignment(Qt::AlignTop);
 form->setHorizontalSpacing(10);
 form->setVerticalSpacing(8);
 form->addRow(QStringLiteral("Rule ID"), impl_->ruleIdLabel);
 form->addRow(QStringLiteral("Name"), impl_->nameEdit);
 form->addRow(QStringLiteral("Enabled"), impl_->enabledCheck);
 form->addRow(QStringLiteral("Trigger"), impl_->triggerTypeEdit);
 form->addRow(QStringLiteral("Source"), impl_->sourceLayerEdit);
 form->addRow(QStringLiteral("Target"), impl_->targetLayerEdit);
 form->addRow(QStringLiteral("Property"), impl_->propertyPathEdit);
 form->addRow(QStringLiteral("Threshold"), impl_->valueThresholdEdit);
 form->addRow(QStringLiteral("Frame"), impl_->frameNumberEdit);
 form->addRow(QStringLiteral("Delay"), impl_->delayEdit);
 form->addRow(QStringLiteral("Cooldown"), impl_->cooldownEdit);
 form->addRow(QStringLiteral("Last Fired"), impl_->lastFiredLabel);
 form->addRow(QStringLiteral("Reaction"), impl_->reactionLabel);
 form->addRow(QStringLiteral("Once"), impl_->onceCheck);
 form->addRow(QStringLiteral("Repeating"), impl_->repeatingCheck);
 inspectorLayout->addLayout(form);
 inspectorLayout->addStretch(1);

 upperSplitter->addWidget(impl_->targetTree);
 upperSplitter->addWidget(centerPanel);
 upperSplitter->addWidget(inspectorScroll);
 upperSplitter->setStretchFactor(0, 2);
 upperSplitter->setStretchFactor(1, 3);
 upperSplitter->setStretchFactor(2, 2);

 impl_->eventLog = new QPlainTextEdit(host);
 impl_->eventLog->setReadOnly(true);
 impl_->eventLog->setMaximumBlockCount(1000);
 impl_->eventLog->setMinimumHeight(140);
 QPalette eventLogPalette = impl_->eventLog->palette();
 eventLogPalette.setColor(QPalette::Base, QColor(17, 19, 25));
 eventLogPalette.setColor(QPalette::Text, QColor(214, 222, 232));
 impl_->eventLog->setPalette(eventLogPalette);

 impl_->statusLabel = new QLabel(QStringLiteral("Ready"), host);
 QPalette statusPalette = impl_->statusLabel->palette();
 statusPalette.setColor(QPalette::WindowText, QColor(166, 179, 195));
 impl_->statusLabel->setPalette(statusPalette);

 rootLayout->addWidget(upperSplitter, 1);
 rootLayout->addWidget(impl_->eventLog, 0);
 rootLayout->addWidget(impl_->statusLabel, 0);
 setCentralWidget(host);

connect(impl_->ruleTree, &QTreeWidget::currentItemChanged, this,
         [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
          if (!impl_ || !current) {
           return;
          }
          const int index = current->data(0, kRoleRuleIndex).toInt();
          impl_->selectRule(index);
         });

 connect(impl_->ruleTree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
  if (!impl_ || !item || column != 0) {
   return;
  }
  const int index = item->data(0, kRoleRuleIndex).toInt();
  if (index < 0 || index >= static_cast<int>(impl_->rules.size())) {
   return;
  }
  impl_->rules[static_cast<size_t>(index)].enabled = item->checkState(0) == Qt::Checked;
  impl_->appendLog(QStringLiteral("Rule toggled: %1").arg(impl_->rules[static_cast<size_t>(index)].name));
  impl_->rebuildRuleTree();
  impl_->rebuildInspector();
 });

 connect(impl_->addRuleButton, &QPushButton::clicked, this, [this]() {
  if (!impl_) return;
  impl_->rules.push_back(impl_->defaultRuleForCurrentSelection());
  impl_->selectedRuleIndex = static_cast<int>(impl_->rules.size()) - 1;
  impl_->rebuildRuleTree();
  impl_->rebuildInspector();
  impl_->appendLog(QStringLiteral("Rule added"));
 });

 connect(impl_->duplicateRuleButton, &QPushButton::clicked, this, [this]() {
  if (!impl_) return;
  const ReactiveRule* rule = impl_->currentRule();
  if (!rule) return;
  ReactiveRule copy = *rule;
  copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  copy.name = copy.name + QStringLiteral(" Copy");
  impl_->rules.push_back(copy);
  impl_->selectedRuleIndex = static_cast<int>(impl_->rules.size()) - 1;
  impl_->rebuildRuleTree();
  impl_->rebuildInspector();
  impl_->appendLog(QStringLiteral("Rule duplicated"));
 });

 connect(impl_->removeRuleButton, &QPushButton::clicked, this, [this]() {
  if (!impl_) return;
  if (impl_->selectedRuleIndex < 0 || impl_->selectedRuleIndex >= static_cast<int>(impl_->rules.size())) {
   return;
  }
  impl_->rules.erase(impl_->rules.begin() + impl_->selectedRuleIndex);
  if (impl_->rules.empty()) {
   impl_->selectedRuleIndex = -1;
  } else {
   impl_->selectedRuleIndex = qMin(impl_->selectedRuleIndex, static_cast<int>(impl_->rules.size()) - 1);
  }
  impl_->rebuildRuleTree();
  impl_->rebuildInspector();
  impl_->appendLog(QStringLiteral("Rule removed"));
 });

 connect(impl_->refreshButton, &QPushButton::clicked, this, [this]() {
  if (!impl_) return;
  impl_->refreshAll();
 });

 auto hookApply = [this]() {
  if (impl_) {
   impl_->applyInspectorToRule();
  }
 };
 connect(impl_->enabledCheck, &QCheckBox::toggled, this, [hookApply](bool) { hookApply(); });
 connect(impl_->onceCheck, &QCheckBox::toggled, this, [hookApply](bool) { hookApply(); });
 connect(impl_->repeatingCheck, &QCheckBox::toggled, this, [hookApply](bool) { hookApply(); });
 connect(impl_->nameEdit, &QLineEdit::editingFinished, this, hookApply);
 connect(impl_->sourceLayerEdit, &QLineEdit::editingFinished, this, hookApply);
 connect(impl_->targetLayerEdit, &QLineEdit::editingFinished, this, hookApply);
 connect(impl_->propertyPathEdit, &QLineEdit::editingFinished, this, hookApply);
 connect(impl_->triggerTypeEdit, qOverload<int>(&QComboBox::currentIndexChanged), this,
         [hookApply](int) { hookApply(); });
 connect(impl_->valueThresholdEdit, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
         [hookApply](double) { hookApply(); });
 connect(impl_->frameNumberEdit, qOverload<int>(&QSpinBox::valueChanged), this, [hookApply](int) { hookApply(); });
 connect(impl_->delayEdit, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
         [hookApply](double) { hookApply(); });
 connect(impl_->cooldownEdit, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
         [hookApply](double) { hookApply(); });

  auto* svc = ArtifactProjectService::instance();
  if (svc) {
  connect(svc, &ArtifactProjectService::projectChanged, this, [this]() {
   if (impl_) {
    impl_->eventBus_.post<ProjectChangedEvent>(ProjectChangedEvent{QString(), QString()});
    impl_->eventBus_.drain();
   }
  });
  connect(svc, &ArtifactProjectService::projectCreated, this, [this]() {
   if (impl_) {
    impl_->eventBus_.post<CompositionCreatedEvent>(CompositionCreatedEvent{QString(), QString()});
    impl_->eventBus_.drain();
   }
  });
  connect(svc, &ArtifactProjectService::compositionCreated, this, [this](const CompositionID& compositionId) {
   if (impl_) {
    impl_->eventBus_.post<CurrentCompositionChangedEvent>(CurrentCompositionChangedEvent{compositionId.toString()});
    impl_->eventBus_.drain();
   }
  });
  connect(svc, &ArtifactProjectService::currentCompositionChanged, this, [this](const CompositionID& compositionId) {
   if (impl_) {
    impl_->eventBus_.post<CurrentCompositionChangedEvent>(CurrentCompositionChangedEvent{compositionId.toString()});
    impl_->eventBus_.drain();
   }
  });

  impl_->eventBusSubscriptions_.push_back(
   impl_->eventBus_.subscribe<ProjectChangedEvent>([this](const ProjectChangedEvent&) {
    if (impl_) {
     impl_->rebuildTargetTree();
     impl_->appendLog(QStringLiteral("Target tree refreshed"));
    }
   }));
  impl_->eventBusSubscriptions_.push_back(
   impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>([this](const CurrentCompositionChangedEvent&) {
    if (impl_) {
     impl_->rebuildTargetTree();
    }
   }));
  impl_->eventBusSubscriptions_.push_back(
   impl_->eventBus_.subscribe<CompositionCreatedEvent>([this](const CompositionCreatedEvent&) {
    if (impl_) {
     impl_->refreshAll();
    }
   }));
  impl_->eventBusSubscriptions_.push_back(
   impl_->eventBus_.subscribe<LayerSelectionChangedEvent>([this](const LayerSelectionChangedEvent&) {
    if (impl_) {
     impl_->appendLog(QStringLiteral("Layer selection changed"));
    }
   }));
  }

 impl_->loadSampleRules();
 impl_->refreshAll();
 if (!impl_->rules.empty()) {
  impl_->selectedRuleIndex = 0;
  impl_->rebuildRuleTree();
  impl_->rebuildInspector();
 }
}

ArtifactReactiveEventEditorWindow::~ArtifactReactiveEventEditorWindow()
{
 delete impl_;
}

void ArtifactReactiveEventEditorWindow::present()
{
 show();
 if (isMinimized()) {
  showNormal();
 }
 raise();
 activateWindow();
}

}

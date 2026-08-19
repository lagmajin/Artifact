module;
#include <utility>
#include <wobjectimpl.h>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QVBoxLayout>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QMenu>
#include <QAction>
#include <QCursor>
#include <QLineEdit>
#include <QGraphicsOpacityEffect>
#include <QPalette>
#include <QColor>
#include <QInputDialog>
#include <QStringList>
#include <functional>
#include <QSettings>
#include <qevent.h>

module Artifact.Widgets.CompositionGraphWidget;

import Artifact.Service.Project;
import Artifact.Service.Effect;
import Artifact.Widgets.AppDialogs;
import Artifact.Engine.DAG.LayerGraphBuilder;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Event.Types;
import Event.Bus;
import Utils;
import Utils.String.UniString;
import Settings.Accessibility;

namespace Artifact {
    using namespace ArtifactCore;

    W_OBJECT_IMPL(ArtifactCompositionGraphWidget)

    class LayerNodeItem : public QGraphicsRectItem {
    public:
        LayerID layerId;
        QString searchText;
        std::function<void(const LayerID &, const QPointF &)> positionChanged;
        LayerNodeItem(const LayerID& id) : layerId(id) {
            setFlag(ItemIsMovable);
            setFlag(ItemIsSelectable);
            setFlag(ItemSendsGeometryChanges);
        }
        
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override {
            ArtifactProjectService::instance()->selectLayer(layerId);
            QGraphicsRectItem::mouseDoubleClickEvent(event);
        }

        QVariant itemChange(QGraphicsItem::GraphicsItemChange change,
                            const QVariant &value) override {
            const QVariant result = QGraphicsRectItem::itemChange(change, value);
            if (change == QGraphicsItem::ItemPositionHasChanged && positionChanged) {
                positionChanged(layerId, pos());
            }
            return result;
        }
    };

    class EffectNodeItem : public QGraphicsRectItem {
    public:
        LayerID layerId;
        QString effectId;

        EffectNodeItem(const LayerID &layer, const QString &effect,
                       QGraphicsItem *parent = nullptr)
            : QGraphicsRectItem(parent), layerId(layer), effectId(effect) {}

        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override {
            if (auto *service = ArtifactProjectService::instance()) {
                service->selectLayer(layerId);
            }
            QGraphicsRectItem::mouseDoubleClickEvent(event);
        }

        void openContextMenu(const QPoint &screenPos) {
            auto *service = ArtifactProjectService::instance();
            if (!service) {
                return;
            }
            auto composition = service->currentComposition().lock();
            auto layer = composition ? composition->layerById(layerId) : nullptr;
            auto effect = layer ? layer->getEffect(UniString::fromQString(effectId))
                                : nullptr;
            if (!effect) {
                return;
            }
            const auto effects = layer->getEffects();
            const auto effectIt = std::find_if(
                effects.begin(), effects.end(),
                [this](const auto &candidate) {
                    return candidate && candidate->effectID().toQString() == effectId;
                });
            const int effectIndex = effectIt == effects.end()
                ? -1
                : static_cast<int>(std::distance(effects.begin(), effectIt));
            QMenu menu;
            QAction *selectLayerAction = menu.addAction(QStringLiteral("Select Layer"));
            menu.addSeparator();
            QAction *toggleAction = menu.addAction(
                effect->isEnabled() ? QStringLiteral("Disable Effect")
                                    : QStringLiteral("Enable Effect"));
            QAction *moveUpAction = menu.addAction(QStringLiteral("Move Effect Up"));
            moveUpAction->setEnabled(effectIndex > 0);
            QAction *moveDownAction = menu.addAction(QStringLiteral("Move Effect Down"));
            moveDownAction->setEnabled(effectIndex >= 0 &&
                                       effectIndex + 1 < static_cast<int>(effects.size()));
            QAction *duplicateAction = menu.addAction(QStringLiteral("Duplicate Effect"));
            QAction *removeAction = menu.addAction(QStringLiteral("Remove Effect"));
            QAction *chosen = menu.exec(screenPos);
            if (chosen == selectLayerAction) {
                service->selectLayer(layerId);
            } else if (chosen == toggleAction) {
                service->setEffectEnabledInLayerInCurrentComposition(
                    layerId, effectId, !effect->isEnabled());
            } else if (chosen == moveUpAction) {
                ArtifactEffectService::instance()->moveEffect(layerId, effectId, -1);
            } else if (chosen == moveDownAction) {
                ArtifactEffectService::instance()->moveEffect(layerId, effectId, 1);
            } else if (chosen == duplicateAction) {
                ArtifactEffectService::instance()->duplicateEffect(layerId, effectId);
            } else if (chosen == removeAction) {
                service->removeEffectFromLayerInCurrentComposition(layerId, effectId);
            }
        }

        void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override {
            openContextMenu(event->screenPos());
            event->accept();
        }
    };

    class GraphView : public QGraphicsView {
    public:
        GraphView(QGraphicsScene* scene, QWidget* parent = nullptr) : QGraphicsView(scene, parent) {
            setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            setResizeAnchor(QGraphicsView::AnchorUnderMouse);
            setFocusPolicy(Qt::StrongFocus);
        }

    protected:
        void wheelEvent(QWheelEvent* event) override {
            const double scaleFactor = 1.15;
            if (event->angleDelta().y() > 0) {
                scale(scaleFactor, scaleFactor);
            } else {
                scale(1.0 / scaleFactor, 1.0 / scaleFactor);
            }
        }

        void keyPressEvent(QKeyEvent *event) override {
            const bool moveUp = event->key() == Qt::Key_Up;
            const bool moveDown = event->key() == Qt::Key_Down;
            if ((moveUp || moveDown) &&
                event->modifiers().testFlag(Qt::ControlModifier)) {
                std::vector<std::pair<LayerID, QString>> effectTargets;
                for (QGraphicsItem *item : scene()->selectedItems()) {
                    if (auto *effectNode = dynamic_cast<EffectNodeItem *>(item)) {
                        effectTargets.emplace_back(effectNode->layerId,
                                                   effectNode->effectId);
                    }
                }
                if (!effectTargets.empty()) {
                    if (auto *service = ArtifactEffectService::instance()) {
                        const int direction = moveUp ? -1 : 1;
                        for (const auto &[layerId, effectId] : effectTargets) {
                            service->moveEffect(layerId, effectId, direction);
                        }
                    }
                    event->accept();
                    return;
                }
            }
            if (event->key() == Qt::Key_D &&
                event->modifiers().testFlag(Qt::ControlModifier)) {
                std::vector<std::pair<LayerID, QString>> effectTargets;
                for (QGraphicsItem *item : scene()->selectedItems()) {
                    if (auto *effectNode = dynamic_cast<EffectNodeItem *>(item)) {
                        effectTargets.emplace_back(effectNode->layerId,
                                                   effectNode->effectId);
                    }
                }
                if (!effectTargets.empty()) {
                    if (auto *service = ArtifactEffectService::instance()) {
                        for (const auto &[layerId, effectId] : effectTargets) {
                            service->duplicateEffect(layerId, effectId);
                        }
                    }
                    event->accept();
                    return;
                }
            }
            if (event->key() == Qt::Key_Delete ||
                event->key() == Qt::Key_Backspace) {
                std::vector<std::pair<LayerID, QString>> effectTargets;
                for (QGraphicsItem *item : scene()->selectedItems()) {
                    if (auto *effectNode = dynamic_cast<EffectNodeItem *>(item)) {
                        effectTargets.emplace_back(effectNode->layerId,
                                                   effectNode->effectId);
                    }
                }
                if (!effectTargets.empty()) {
                    if (auto *service = ArtifactEffectService::instance()) {
                        for (const auto &[layerId, effectId] : effectTargets) {
                            service->removeEffectFromLayer(layerId, effectId);
                        }
                    }
                    event->accept();
                    return;
                }
            }
            QGraphicsView::keyPressEvent(event);
        }
    };

    class ArtifactCompositionGraphWidget::Impl {
    public:
        QGraphicsView* view;
        QGraphicsScene* scene;
        QLineEdit* searchBar;
        QMap<LayerID, LayerNodeItem*> nodeMap;
        ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
        std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

        void setupUi(QWidget* parent) {
            auto layout = new QVBoxLayout(parent);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);

            // Search Bar Header
            searchBar = new QLineEdit();
            searchBar->setPlaceholderText("Search layers...");
            searchBar->setMinimumHeight(
                Artifact::Accessibility::scaledSize(24));
            searchBar->setAccessibleName(QStringLiteral("Graph layer search"));
            searchBar->setAccessibleDescription(
                QStringLiteral("Filter the composition graph by layer name."));
            QPalette searchPalette = searchBar->palette();
            searchPalette.setColor(QPalette::Base, QColor(45, 45, 48));
            searchPalette.setColor(QPalette::Text, QColor(204, 204, 204));
            searchPalette.setColor(QPalette::PlaceholderText, QColor(136, 136, 136));
            searchBar->setPalette(searchPalette);
            layout->addWidget(searchBar);

            scene = new QGraphicsScene(parent);
            view = new GraphView(scene, parent);
            view->setAccessibleName(QStringLiteral("Composition graph view"));
            view->setAccessibleDescription(
                QStringLiteral("View composition and layer relationships."));
            view->setRenderHint(QPainter::Antialiasing);
            view->setDragMode(QGraphicsView::ScrollHandDrag);
            view->setBackgroundBrush(QColor(30, 30, 32));

            layout->addWidget(view);
            
            view->setContextMenuPolicy(Qt::CustomContextMenu);
            QObject::connect(view, &QGraphicsView::customContextMenuRequested, [this, parent](const QPoint& pos) {
                showContextMenu(pos, parent);
            });

            refresh();
        }

        void showContextMenu(const QPoint& pos, QWidget* parent) {
            QPoint globalPos = view->mapToGlobal(pos);
            QPointF scenePos = view->mapToScene(pos);
            
            QGraphicsItem* item = scene->itemAt(scenePos, view->transform());
            while (item && !dynamic_cast<EffectNodeItem *>(item) &&
                   !dynamic_cast<LayerNodeItem *>(item)) {
                item = item->parentItem();
            }
            if (auto *effectNode = dynamic_cast<EffectNodeItem *>(item)) {
                effectNode->openContextMenu(globalPos);
                return;
            }
            LayerNodeItem* node = dynamic_cast<LayerNodeItem*>(item);

            QMenu menu(parent);

            if (node) {
                auto selectAction = menu.addAction("Select Layer");
                QObject::connect(selectAction, &QAction::triggered, [node]() {
                    ArtifactProjectService::instance()->selectLayer(node->layerId);
                });
                
                menu.addSeparator();
                auto visibilityAction = menu.addAction("Visible");
                visibilityAction->setCheckable(true);
                if (auto *service = ArtifactProjectService::instance()) {
                    visibilityAction->setChecked(
                        service->isLayerVisibleInCurrentComposition(node->layerId));
                }
                QObject::connect(visibilityAction, &QAction::toggled,
                                 [node](bool visible) {
                    if (auto *service = ArtifactProjectService::instance()) {
                        service->setLayerVisibleInCurrentComposition(node->layerId,
                                                                      visible);
                    }
                });
                auto lockAction = menu.addAction("Locked");
                lockAction->setCheckable(true);
                if (auto *service = ArtifactProjectService::instance()) {
                    lockAction->setChecked(
                        service->isLayerLockedInCurrentComposition(node->layerId));
                }
                QObject::connect(lockAction, &QAction::toggled,
                                 [node](bool locked) {
                    if (auto *service = ArtifactProjectService::instance()) {
                        service->setLayerLockedInCurrentComposition(node->layerId,
                                                                     locked);
                    }
                                 });
                auto soloAction = menu.addAction("Solo");
                soloAction->setCheckable(true);
                if (auto *service = ArtifactProjectService::instance()) {
                    soloAction->setChecked(
                        service->isLayerSoloInCurrentComposition(node->layerId));
                }
                QObject::connect(soloAction, &QAction::toggled,
                                 [node](bool solo) {
                    if (auto *service = ArtifactProjectService::instance()) {
                        service->setLayerSoloInCurrentComposition(node->layerId,
                                                                  solo);
                    }
                });
                auto shyAction = menu.addAction("Shy");
                shyAction->setCheckable(true);
                if (auto *service = ArtifactProjectService::instance()) {
                    shyAction->setChecked(
                        service->isLayerShyInCurrentComposition(node->layerId));
                }
                QObject::connect(shyAction, &QAction::toggled,
                                 [node](bool shy) {
                    if (auto *service = ArtifactProjectService::instance()) {
                        service->setLayerShyInCurrentComposition(node->layerId,
                                                                 shy);
                    }
                                 });
                if (auto *service = ArtifactProjectService::instance();
                    service && service->layerHasParentInCurrentComposition(
                                  node->layerId)) {
                    auto clearParentAction = menu.addAction("Clear Parent");
                    QObject::connect(clearParentAction, &QAction::triggered,
                                     [node]() {
                        if (auto *service = ArtifactProjectService::instance()) {
                            service->clearLayerParentInCurrentComposition(
                                node->layerId);
                        }
                    });
                }
                if (auto *service = ArtifactProjectService::instance()) {
                    const auto composition = service->currentComposition().lock();
                    if (composition) {
                        const auto layers = composition->allLayer();
                        const int layerIndex = std::find_if(
                            layers.cbegin(), layers.cend(), [node](const auto &layer) {
                                return layer && layer->id() == node->layerId;
                            }) - layers.cbegin();
                        if (layerIndex >= 0 && layerIndex < layers.size()) {
                            auto moveUpAction = menu.addAction("Move Up");
                            moveUpAction->setEnabled(layerIndex > 0);
                            QObject::connect(moveUpAction, &QAction::triggered,
                                             [node, layerIndex]() {
                                if (auto *service = ArtifactProjectService::instance()) {
                                    service->moveLayerInCurrentComposition(
                                        node->layerId, layerIndex - 1);
                                }
                            });
                            auto moveDownAction = menu.addAction("Move Down");
                            moveDownAction->setEnabled(layerIndex + 1 < layers.size());
                            QObject::connect(moveDownAction, &QAction::triggered,
                                             [node, layerIndex]() {
                                if (auto *service = ArtifactProjectService::instance()) {
                                    service->moveLayerInCurrentComposition(
                                        node->layerId, layerIndex + 1);
                                }
                            });
                            auto moveTopAction = menu.addAction("Move to Top");
                            moveTopAction->setEnabled(layerIndex > 0);
                            QObject::connect(moveTopAction, &QAction::triggered,
                                             [node]() {
                                if (auto *service = ArtifactProjectService::instance()) {
                                    service->moveLayerInCurrentComposition(node->layerId, 0);
                                }
                            });
                            auto moveBottomAction = menu.addAction("Move to Bottom");
                            moveBottomAction->setEnabled(layerIndex + 1 < layers.size());
                            QObject::connect(moveBottomAction, &QAction::triggered,
                                             [node, layers]() {
                                if (auto *service = ArtifactProjectService::instance()) {
                                    service->moveLayerInCurrentComposition(
                                        node->layerId, layers.size() - 1);
                                }
                            });
                        }
                        QStringList parentNames;
                        QVector<LayerID> parentIds;
                        for (const auto &candidate : composition->allLayer()) {
                            if (!candidate || candidate->id() == node->layerId) {
                                continue;
                            }
                            const QString baseName = candidate->layerName();
                            QString displayName = baseName;
                            int duplicateIndex = 2;
                            while (parentNames.contains(displayName)) {
                                displayName = QStringLiteral("%1 (%2)")
                                                   .arg(baseName)
                                                   .arg(duplicateIndex++);
                            }
                            parentNames.push_back(displayName);
                            parentIds.push_back(candidate->id());
                        }
                        if (!parentNames.isEmpty()) {
                            auto setParentAction = menu.addAction("Set Parent...");
                            QObject::connect(setParentAction, &QAction::triggered,
                                             [node, parent, parentNames, parentIds]() {
                                bool accepted = false;
                                const QString name = QInputDialog::getItem(
                                    parent, QStringLiteral("Set Parent"),
                                    QStringLiteral("Parent layer:"), parentNames,
                                    0, false, &accepted);
                                if (!accepted) {
                                    return;
                                }
                                const int index = parentNames.indexOf(name);
                                if (index >= 0) {
                                    if (auto *service = ArtifactProjectService::instance()) {
                                        service->setLayerParentInCurrentComposition(
                                            node->layerId, parentIds.at(index));
                                    }
                                }
                            });
                        }
                    }
                }
                menu.addSeparator();
                auto duplicateAction = menu.addAction("Duplicate Layer");
                QObject::connect(duplicateAction, &QAction::triggered, [node]() {
                    if (auto *service = ArtifactProjectService::instance()) {
                        service->duplicateLayerInCurrentComposition(node->layerId);
                    }
                });
                auto renameAction = menu.addAction("Rename Layer");
                QObject::connect(renameAction, &QAction::triggered, [node, parent]() {
                    auto *service = ArtifactProjectService::instance();
                    if (!service || !node) {
                        return;
                    }
                    const auto composition = service->currentComposition().lock();
                    if (!composition) {
                        return;
                    }
                    const auto layer = composition->layerById(node->layerId);
                    if (!layer) {
                        return;
                    }
                    bool accepted = false;
                    const QString name = QInputDialog::getText(
                        parent, QStringLiteral("Rename Layer"),
                        QStringLiteral("Layer name:"), QLineEdit::Normal,
                        layer->layerName(), &accepted);
                    if (accepted && !name.trimmed().isEmpty()) {
                        service->renameLayerInCurrentComposition(node->layerId,
                                                                 name.trimmed());
                    }
                });
                menu.addSeparator();
                auto deleteAction = menu.addAction("Delete Layer");
                QObject::connect(deleteAction, &QAction::triggered, [node, parent]() {
                    auto *service = ArtifactProjectService::instance();
                    if (!service || !node) {
                        return;
                    }
                    const auto composition = service->currentComposition().lock();
                    if (composition && ArtifactMessageBox::confirmDelete(
                            parent, QStringLiteral("Delete Layer"),
                            QStringLiteral("Delete layer '%1'?").arg(
                                node->layerId.toString()))) {
                        service->removeLayerFromComposition(composition->id(),
                                                            node->layerId);
                    }
                });
            } else {
                QMenu* newMenu = menu.addMenu("New Layer");
                
                auto addNull = newMenu->addAction("Null Object");
                QObject::connect(addNull, &QAction::triggered, []() {
                    ArtifactNullLayerInitParams p("Null");
                    ArtifactProjectService::instance()->addLayerToCurrentComposition(p);
                });

                auto addSolid = newMenu->addAction("Solid...");
                QObject::connect(addSolid, &QAction::triggered, []() {
                    ArtifactSolidLayerInitParams p("Solid");
                    ArtifactProjectService::instance()->addLayerToCurrentComposition(p);
                });

                menu.addSeparator();
                auto fitAction = menu.addAction("Fit to Screen");
                QObject::connect(fitAction, &QAction::triggered, [this]() {
                    view->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
                });

                auto refreshAction = menu.addAction("Refresh Graph");
                QObject::connect(refreshAction, &QAction::triggered, [this]() {
                    refresh();
                });
            }

            int menuX = globalPos.x();
            int menuY = globalPos.y();
            Accessibility::adjustContextMenuPosition(
                menuX, menuY, menu.sizeHint().width());
            menu.exec(QPoint(menuX, menuY));
        }

        void refresh() {
            scene->clear();
            nodeMap.clear();
            auto service = ArtifactProjectService::instance();
            auto compPtr = service->currentComposition();
            if (compPtr.expired()) return;
            auto comp = compPtr.lock();

            auto layers = comp->allLayer();

            // Step 1: Create nodes with colorful palette
            int i = 0;
            for (auto layer : layers) {
                if (!layer) continue;
                
                // Vibrant Palette
                QColor nodeColor;
                if (layer->isAdjustmentLayer()) {
                    nodeColor = QColor(160, 80, 220); // Purple
                } else if (layer->isNullLayer()) {
                    nodeColor = QColor(220, 180, 50); // Yellow/Gold
                } else {
                    static const QVector<QColor> palette = {
                        QColor(30, 150, 220),  // Bright Blue
                        QColor(40, 180, 100),  // Green
                        QColor(220, 70, 70),   // Red
                        QColor(220, 120, 40),  // Orange
                        QColor(220, 60, 160)   // Pink
                    };
                    nodeColor = palette[i % palette.size()];
                }
                
                const auto effects = layer->getEffects();
                const auto effectGraph = LayerGraphBuilder::build(
                    UniString(layer->layerName()), effects);
                QStringList effectNames;
                int disabledEffectCount = 0;
                for (const auto &effect : effects) {
                    if (effect) {
                        if (!effect->isEnabled()) {
                            ++disabledEffectCount;
                        }
                        effectNames.push_back(
                            effect->displayName().toQString() + QStringLiteral(" ") +
                            effect->effectID().toQString());
                    }
                }
                QString nodeTitle = effects.empty()
                    ? layer->layerName()
                    : QStringLiteral("%1  [%2 effects]")
                          .arg(layer->layerName())
                          .arg(static_cast<int>(effects.size()));
                if (disabledEffectCount > 0) {
                    nodeTitle += QStringLiteral("  (%1 disabled)")
                                     .arg(disabledEffectCount);
                }
                auto node = addNode(layer->id(), nodeTitle,
                                    graphPosition(layer->id(), QPointF(0, i * 65)), nodeColor,
                                    layer->layerName() + QStringLiteral(" ") +
                                        effectNames.join(QStringLiteral(" ")));
                node->setToolTip(QStringLiteral("%1\nEffects: %2")
                                     .arg(layer->layerName())
                                     .arg(effectNames.isEmpty()
                                              ? QStringLiteral("none")
                                              : effectNames.join(QStringLiteral(", "))) +
                                     QStringLiteral("\nEffect DAG: %1")
                                         .arg(effects.empty() ? QStringLiteral("n/a")
                                                               : QStringLiteral("available")) +
                                     QStringLiteral("\nNodes: %1  Connections: %2")
                                         .arg(static_cast<int>(effectGraph.nodes().size()))
                                         .arg(static_cast<int>(effectGraph.connections().size())));
                drawEffectNodes(node, effects, effectGraph);
                nodeMap[layer->id()] = node;
                i++;
            }

            // Step 2: Draw connections (AE Parenting Flow)
            for (auto layer : layers) {
                if (!layer || !layer->hasParent()) continue;
                
                auto pId = layer->parentLayerId();
                if (nodeMap.contains(pId) && nodeMap.contains(layer->id())) {
                    drawParentLink(nodeMap[pId], nodeMap[layer->id()]);
                }
            }
        }

        void filterNodes(const QString& text) {
            for (auto it = nodeMap.begin(); it != nodeMap.end(); ++it) {
                LayerNodeItem* node = it.value();
                const bool match = text.isEmpty() ||
                    node->searchText.contains(text, Qt::CaseInsensitive);
                
                if (match) {
                    node->setOpacity(1.0);
                    node->setZValue(1);
                } else {
                    node->setOpacity(0.2);
                    node->setZValue(0);
                }
            }
        }

        QPointF graphPosition(const LayerID &layerId, const QPointF &fallback) const {
            QSettings settings;
            const QVariantList values = settings.value(
                QStringLiteral("ArtifactStudio/CompositionGraph/%1")
                    .arg(layerId.toString())).toList();
            if (values.size() != 2) {
                return fallback;
            }
            return QPointF(values.at(0).toDouble(), values.at(1).toDouble());
        }

        LayerNodeItem* addNode(const LayerID& id, const QString& title, QPointF pos,
                               QColor color, const QString& searchText = {}) {
            auto rect = new LayerNodeItem(id);
            rect->searchText = searchText.isEmpty() ? title : searchText;
            rect->positionChanged = [](const LayerID &layerId,
                                       const QPointF &position) {
                QSettings settings;
                settings.setValue(
                    QStringLiteral("ArtifactStudio/CompositionGraph/%1")
                        .arg(layerId.toString()),
                    QVariantList{position.x(), position.y()});
            };
            rect->setRect(0, 0, 160, 48);
            
            QLinearGradient grad(0, 0, 0, 48);
            grad.setColorAt(0, color.lighter(120));
            grad.setColorAt(1, color.darker(110));
            
            rect->setBrush(grad);
            rect->setPen(QPen(color.lighter(150), 1.5));
            rect->setPos(pos);

            auto text = new QGraphicsTextItem(title, rect);
            text->setDefaultTextColor(Qt::white);
            QFont font = text->font();
            font.setBold(true);
            text->setFont(font);
            text->setPos(6, 6);

            scene->addItem(rect);
            return rect;
        }

        void drawParentLink(QGraphicsRectItem* parentNode, QGraphicsRectItem* childNode) {
            QPainterPath path;
            QPointF start = parentNode->pos() + QPointF(160, 24);
            QPointF end = childNode->pos() + QPointF(0, 24);
            
            path.moveTo(start);
            qreal midX = (start.x() + end.x()) / 2;
            path.cubicTo(midX, start.y(), midX, end.y(), end.x(), end.y());
            
            auto pathItem = new QGraphicsPathItem(path);
            pathItem->setPen(QPen(QColor(255, 255, 255, 120), 1.5, Qt::DashLine));
            pathItem->setZValue(-1);
            scene->addItem(pathItem);
        }

        void drawEffectNodes(LayerNodeItem *layerNode, const auto &effects,
                             const auto &effectGraph) {
            if (!layerNode || effects.empty()) {
                return;
            }
            constexpr qreal effectX = 215.0;
            constexpr qreal effectWidth = 150.0;
            constexpr qreal effectHeight = 28.0;
            constexpr qreal effectGap = 7.0;
            QMap<QString, EffectNodeItem *> effectItems;
            for (size_t index = 0; index < effects.size(); ++index) {
                const auto &effect = effects[index];
                if (!effect) {
                    continue;
                }
                const qreal y = static_cast<qreal>(index) *
                                (effectHeight + effectGap);
                auto *effectNode = new EffectNodeItem(
                    layerNode->layerId, effect->effectID().toQString(), layerNode);
                effectNode->setRect(0.0, 0.0, effectWidth, effectHeight);
                effectNode->setFlag(QGraphicsItem::ItemIsSelectable, true);
                effectNode->setPos(effectX, y);
                static const QColor stageColors[] = {
                    QColor(92, 86, 126),   // Pre-process
                    QColor(102, 78, 148),  // Generator
                    QColor(72, 112, 156),  // Geometry transform
                    QColor(64, 132, 112),  // Material render
                    QColor(55, 98, 128),   // Rasterizer
                    QColor(148, 96, 64)    // Layer transform
                };
                const int stageIndex = std::clamp(
                    static_cast<int>(effect->pipelineStage()), 0, 5);
                const QColor stageColor = stageColors[stageIndex];
                const bool enabled = effect->isEnabled();
                effectNode->setBrush(enabled ? stageColor : stageColor.darker(185));
                effectNode->setOpacity(enabled ? 1.0 : 0.55);
                effectNode->setPen(QPen(
                    enabled ? stageColor.lighter(135) : QColor(120, 126, 138),
                    1.0));
                effectNode->setToolTip(
                    QStringLiteral("%1\nID: %2\nProperties: %3\nState: %4")
                        .arg(effect->displayName().toQString(),
                             effect->effectID().toQString())
                        .arg(static_cast<int>(effect->getProperties().size()))
                        .arg(enabled ? QStringLiteral("Enabled")
                                     : QStringLiteral("Disabled")));
                auto *label = new QGraphicsTextItem(
                    effect->displayName().toQString(), effectNode);
                label->setDefaultTextColor(QColor(220, 226, 236));
                label->setPos(5.0, 4.0);
                effectItems.insert(effect->effectID().toQString(), effectNode);

                QPainterPath path;
                const QPointF effectScenePos = effectNode->scenePos();
                path.moveTo(layerNode->scenePos() + QPointF(160.0, 24.0));
                path.cubicTo(layerNode->scenePos() + QPointF(185.0, 24.0),
                             effectScenePos + QPointF(-25.0, effectHeight * 0.5),
                             effectScenePos + QPointF(0.0, effectHeight * 0.5));
                auto *link = new QGraphicsPathItem(path);
                link->setPen(QPen(QColor(120, 135, 160, 150), 1.0));
                link->setZValue(-1.0);
                scene->addItem(link);
            }

            // The builder's stage ordering is the authoritative execution
            // order. Show those edges separately from the layer-to-effect
            // attachment lines so the graph communicates actual evaluation
            // flow as well as ownership.
            for (const auto &connection : effectGraph.connections()) {
                const auto source = effectGraph.findNode(connection.sourceNodeId);
                const auto target = effectGraph.findNode(connection.targetNodeId);
                if (!source || !target || !source->effect() || !target->effect()) {
                    continue;
                }
                const auto sourceItem = effectItems.value(
                    source->effect()->effectID().toQString(), nullptr);
                const auto targetItem = effectItems.value(
                    target->effect()->effectID().toQString(), nullptr);
                if (!sourceItem || !targetItem) {
                    continue;
                }
                const QPointF start = sourceItem->scenePos() +
                                      QPointF(effectWidth, effectHeight * 0.5);
                const QPointF end = targetItem->scenePos() +
                                    QPointF(0.0, effectHeight * 0.5);
                QPainterPath path;
                path.moveTo(start);
                const qreal midX = (start.x() + end.x()) * 0.5;
                path.cubicTo(midX, start.y(), midX, end.y(), end.x(), end.y());
                auto *link = new QGraphicsPathItem(path);
                link->setPen(QPen(QColor(245, 205, 110, 210), 1.8));
                link->setZValue(-0.5);
                link->setToolTip(QStringLiteral("Effect evaluation connection"));
                scene->addItem(link);
            }
        }
    };

    ArtifactCompositionGraphWidget::ArtifactCompositionGraphWidget(QWidget* parent)
        : QWidget(parent), impl_(new Impl()) {
        setAccessibleName(QStringLiteral("Composition Graph"));
        setAccessibleDescription(
            QStringLiteral("Explore composition and layer relationships."));
        impl_->setupUi(this);

        impl_->eventBusSubscriptions_.push_back(
            impl_->eventBus_.subscribe<ProjectChangedEvent>([this](const ProjectChangedEvent&) {
                impl_->refresh();
            }));
        impl_->eventBusSubscriptions_.push_back(
            impl_->eventBus_.subscribe<CompositionCreatedEvent>([this](const CompositionCreatedEvent&) {
                impl_->refresh();
            }));
        impl_->eventBusSubscriptions_.push_back(
            impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>([this](const CurrentCompositionChangedEvent&) {
                impl_->refresh();
            }));
        impl_->eventBusSubscriptions_.push_back(
            impl_->eventBus_.subscribe<LayerChangedEvent>([this](const LayerChangedEvent&) {
                impl_->refresh();
            }));

        connect(impl_->searchBar, &QLineEdit::textChanged, this, [this](const QString& text) {
            impl_->filterNodes(text);
        });
    }

    ArtifactCompositionGraphWidget::~ArtifactCompositionGraphWidget() {
        delete impl_;
    }

    QSize ArtifactCompositionGraphWidget::sizeHint() const {
        return { 400, 300 };
    }
}

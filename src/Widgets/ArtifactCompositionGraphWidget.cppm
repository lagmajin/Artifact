module;
#include <utility>
#include <wobjectimpl.h>
#include <QGraphicsScene>
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
#include <qevent.h>

module Artifact.Widgets.CompositionGraphWidget;

import std;
import Artifact.Service.Project;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Event.Types;
import Event.Bus;
import Utils;
import Utils.String.UniString;

namespace Artifact {
    using namespace ArtifactCore;

    W_OBJECT_IMPL(ArtifactCompositionGraphWidget)

    class LayerNodeItem : public QGraphicsRectItem {
    public:
        LayerID layerId;
        LayerNodeItem(const LayerID& id) : layerId(id) {
            setFlag(ItemIsMovable);
            setFlag(ItemIsSelectable);
        }
        
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override {
            ArtifactProjectService::instance()->selectLayer(layerId);
            QGraphicsRectItem::mouseDoubleClickEvent(event);
        }
    };

    class GraphView : public QGraphicsView {
    public:
        GraphView(QGraphicsScene* scene, QWidget* parent = nullptr) : QGraphicsView(scene, parent) {
            setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            setResizeAnchor(QGraphicsView::AnchorUnderMouse);
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
            QPalette searchPalette = searchBar->palette();
            searchPalette.setColor(QPalette::Base, QColor(45, 45, 48));
            searchPalette.setColor(QPalette::Text, QColor(204, 204, 204));
            searchPalette.setColor(QPalette::PlaceholderText, QColor(136, 136, 136));
            searchBar->setPalette(searchPalette);
            layout->addWidget(searchBar);

            scene = new QGraphicsScene(parent);
            view = new GraphView(scene, parent);
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
            LayerNodeItem* node = dynamic_cast<LayerNodeItem*>(item);

            QMenu menu(parent);

            if (node) {
                auto selectAction = menu.addAction("Select Layer");
                QObject::connect(selectAction, &QAction::triggered, [node]() {
                    ArtifactProjectService::instance()->selectLayer(node->layerId);
                });
                
                menu.addSeparator();
                auto deleteAction = menu.addAction("Delete Layer");
                QObject::connect(deleteAction, &QAction::triggered, [node]() {
                    qDebug() << "Request delete for layer:" << node->layerId.toString();
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

            menu.exec(globalPos);
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
                
                auto node = addNode(layer->id(), layer->layerName(), QPointF(0, i * 65), nodeColor);
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
                QString title = "";
                for (auto child : node->childItems()) {
                    if (auto textItem = dynamic_cast<QGraphicsTextItem*>(child)) {
                        title = textItem->toPlainText();
                        break;
                    }
                }

                bool match = text.isEmpty() || title.contains(text, Qt::CaseInsensitive);
                
                if (match) {
                    node->setOpacity(1.0);
                    node->setZValue(1);
                } else {
                    node->setOpacity(0.2);
                    node->setZValue(0);
                }
            }
        }

        LayerNodeItem* addNode(const LayerID& id, const QString& title, QPointF pos, QColor color) {
            auto rect = new LayerNodeItem(id);
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
    };

    ArtifactCompositionGraphWidget::ArtifactCompositionGraphWidget(QWidget* parent)
        : QWidget(parent), impl_(new Impl()) {
        impl_->setupUi(this);
        
        auto service = ArtifactProjectService::instance();
        connect(service, &ArtifactProjectService::projectChanged, this, [this]() {
            impl_->eventBus_.post<ProjectChangedEvent>(ProjectChangedEvent{QString(), QString()});
            impl_->eventBus_.drain();
        });
        connect(service, &ArtifactProjectService::compositionCreated, this, [this](const CompositionID&) {
            impl_->eventBus_.post<CompositionCreatedEvent>(CompositionCreatedEvent{QString(), QString()});
            impl_->eventBus_.drain();
        });
        connect(service, &ArtifactProjectService::currentCompositionChanged, this, [this](const CompositionID&) {
            impl_->eventBus_.post<CurrentCompositionChangedEvent>(CurrentCompositionChangedEvent{QString()});
            impl_->eventBus_.drain();
        });
        connect(service, &ArtifactProjectService::layerCreated, this, [this](const CompositionID&, const LayerID& layerId) {
            impl_->eventBus_.post<LayerChangedEvent>(LayerChangedEvent{QString(), layerId.toString(), LayerChangedEvent::ChangeType::Created});
            impl_->eventBus_.drain();
        });
        connect(service, &ArtifactProjectService::layerRemoved, this, [this](const CompositionID&, const LayerID& layerId) {
            impl_->eventBus_.post<LayerChangedEvent>(LayerChangedEvent{QString(), layerId.toString(), LayerChangedEvent::ChangeType::Removed});
            impl_->eventBus_.drain();
        });

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

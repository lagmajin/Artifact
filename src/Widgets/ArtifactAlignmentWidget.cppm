module;
#include <utility>
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QGridLayout>
#include <QVector>
#include <vector>
#include <wobjectimpl.h>

module Artifact.Widgets.Alignment;

import Geometry.LayerAlignment;
import Artifact.Layers.Selection.Manager;
import Artifact.Layer.Abstract;
import Artifact.Application.Manager;
import Time.Rational;
import Undo.UndoManager;

namespace Artifact {

W_OBJECT_IMPL(AlignmentWidget)

class AlignmentWidget::Impl {
public:
    QGridLayout* layout = nullptr;
    // ボタンの保持など
};

AlignmentWidget::AlignmentWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
    setupUi();
}

AlignmentWidget::~AlignmentWidget() {
    delete impl_;
}

void AlignmentWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    auto* grid = new QGridLayout();
    impl_->layout = grid;

    auto createBtn = [&](const QString& text, const QString& tooltip, auto callback) {
        auto* btn = new QPushButton(text);
        btn->setToolTip(tooltip);
        btn->setFixedSize(28, 28);
        connect(btn, &QPushButton::clicked, this, callback);
        return btn;
    };

    // Align Section
    mainLayout->addWidget(new QLabel("Align"));
    auto* alignRow = new QHBoxLayout();
    alignRow->addWidget(createBtn("L", "Align Left", [this]{ onAlignClicked((int)ArtifactCore::AlignType::Left); }));
    alignRow->addWidget(createBtn("CH", "Align Center Horizontal", [this]{ onAlignClicked((int)ArtifactCore::AlignType::CenterHorizontal); }));
    alignRow->addWidget(createBtn("R", "Align Right", [this]{ onAlignClicked((int)ArtifactCore::AlignType::Right); }));
    alignRow->addSpacing(10);
    alignRow->addWidget(createBtn("T", "Align Top", [this]{ onAlignClicked((int)ArtifactCore::AlignType::Top); }));
    alignRow->addWidget(createBtn("CV", "Align Center Vertical", [this]{ onAlignClicked((int)ArtifactCore::AlignType::CenterVertical); }));
    alignRow->addWidget(createBtn("B", "Align Bottom", [this]{ onAlignClicked((int)ArtifactCore::AlignType::Bottom); }));
    alignRow->addStretch();
    mainLayout->addLayout(alignRow);

    // Distribute Section
    mainLayout->addWidget(new QLabel("Distribute"));
    auto* distRow = new QHBoxLayout();
    distRow->addWidget(createBtn("DL", "Distribute Left", [this]{ onDistributeClicked((int)ArtifactCore::DistributeType::Left); }));
    distRow->addWidget(createBtn("DCH", "Distribute Center Horizontal", [this]{ onDistributeClicked((int)ArtifactCore::DistributeType::CenterHorizontal); }));
    distRow->addWidget(createBtn("DR", "Distribute Right", [this]{ onDistributeClicked((int)ArtifactCore::DistributeType::Right); }));
    distRow->addSpacing(10);
    distRow->addWidget(createBtn("DT", "Distribute Top", [this]{ onDistributeClicked((int)ArtifactCore::DistributeType::Top); }));
    distRow->addWidget(createBtn("DCV", "Distribute Center Vertical", [this]{ onDistributeClicked((int)ArtifactCore::DistributeType::CenterVertical); }));
    distRow->addWidget(createBtn("DB", "Distribute Bottom", [this]{ onDistributeClicked((int)ArtifactCore::DistributeType::Bottom); }));
    distRow->addStretch();
    mainLayout->addLayout(distRow);

    mainLayout->addStretch();
}

void AlignmentWidget::onAlignClicked(int type) {
    auto* app = ArtifactApplicationManager::instance();
    auto* selection = app ? app->layerSelectionManager() : nullptr;
    if (!selection) return;
    const auto selectedSet = selection->selectedLayers();
    QVector<ArtifactAbstractLayerPtr> selectedLayers;
    selectedLayers.reserve(selectedSet.size());
    for (const auto& layer : selectedSet) {
        selectedLayers.push_back(layer);
    }
    if (selectedLayers.isEmpty()) return;

    auto composition = selection->activeComposition();
    if (!composition) return;

    // 1. データを AlignmentObject に変換
    std::vector<ArtifactCore::AlignmentObject> objects;
    std::vector<AlignLayerSnapshot> snapshots;
    snapshots.reserve(static_cast<size_t>(selectedLayers.size()));
    for (int i = 0; i < selectedLayers.size(); ++i) {
        auto& layer = selectedLayers[static_cast<size_t>(i)];
        ArtifactCore::AlignmentObject obj;
        obj.id = i;
        obj.bounds = layer->transformedBoundingBox();
        obj.currentPosition = QPointF(layer->transform3D().positionX(), layer->transform3D().positionY());
        objects.push_back(obj);
        const float positionX = static_cast<float>(obj.currentPosition.x());
        const float positionY = static_cast<float>(obj.currentPosition.y());
        snapshots.push_back({layer->id().toString(), positionX, positionY,
                             positionX, positionY,
                             layer->transform3D().scaleX(),
                             layer->transform3D().scaleY(),
                             layer->transform3D().scaleX(),
                             layer->transform3D().scaleY()});
    }

    // 2. ロジック適用 (Selection基準)
    QRectF dummy;
    ArtifactCore::LayerAlignment::align(objects, (ArtifactCore::AlignType)type, ArtifactCore::AlignmentTarget::Selection, dummy);

    // 3. 結果をレイヤーに書き戻す
    ArtifactCore::RationalTime time(0, 30000); // 簡易化のため0フレーム
    for (size_t i = 0; i < objects.size(); ++i) {
        const int sourceIndex = objects[i].id;
        if (sourceIndex < 0 || sourceIndex >= selectedLayers.size()) {
            continue;
        }
        selectedLayers[static_cast<size_t>(sourceIndex)]->transform3D().setPosition(
            time, objects[i].currentPosition.x(), objects[i].currentPosition.y());
        selectedLayers[static_cast<size_t>(sourceIndex)]->changed();
        snapshots[static_cast<size_t>(sourceIndex)].afterX =
            static_cast<float>(objects[i].currentPosition.x());
        snapshots[static_cast<size_t>(sourceIndex)].afterY =
            static_cast<float>(objects[i].currentPosition.y());
    }

    if (auto* undo = UndoManager::instance();
        undo && !undo->push(std::make_unique<AlignLayersUndoCommand>(
                      snapshots, QStringLiteral("Align Layers")))) {
        for (size_t i = 0; i < snapshots.size() && i < static_cast<size_t>(selectedLayers.size()); ++i) {
            selectedLayers[i]->transform3D().setPosition(
                time, snapshots[i].beforeX, snapshots[i].beforeY);
            selectedLayers[i]->changed();
        }
    }
}

void AlignmentWidget::onDistributeClicked(int type) {
    auto* app = ArtifactApplicationManager::instance();
    auto* selection = app ? app->layerSelectionManager() : nullptr;
    if (!selection) return;
    const auto selectedSet = selection->selectedLayers();
    QVector<ArtifactAbstractLayerPtr> selectedLayers;
    selectedLayers.reserve(selectedSet.size());
    for (const auto& layer : selectedSet) {
        selectedLayers.push_back(layer);
    }
    if (selectedLayers.size() < 3) return;

    auto composition = selection->activeComposition();
    if (!composition) return;

    std::vector<ArtifactCore::AlignmentObject> objects;
    std::vector<AlignLayerSnapshot> snapshots;
    snapshots.reserve(static_cast<size_t>(selectedLayers.size()));
    for (int i = 0; i < selectedLayers.size(); ++i) {
        auto& layer = selectedLayers[static_cast<size_t>(i)];
        ArtifactCore::AlignmentObject obj;
        obj.id = i;
        obj.bounds = layer->transformedBoundingBox();
        obj.currentPosition = QPointF(layer->transform3D().positionX(), layer->transform3D().positionY());
        objects.push_back(obj);
        const float positionX = static_cast<float>(obj.currentPosition.x());
        const float positionY = static_cast<float>(obj.currentPosition.y());
        snapshots.push_back({layer->id().toString(), positionX, positionY,
                             positionX, positionY,
                             layer->transform3D().scaleX(),
                             layer->transform3D().scaleY(),
                             layer->transform3D().scaleX(),
                             layer->transform3D().scaleY()});
    }

    ArtifactCore::LayerAlignment::distribute(objects, (ArtifactCore::DistributeType)type);

    ArtifactCore::RationalTime time(0, 30000);
    for (size_t i = 0; i < objects.size(); ++i) {
        const int sourceIndex = objects[i].id;
        if (sourceIndex < 0 || sourceIndex >= selectedLayers.size()) {
            continue;
        }
        selectedLayers[static_cast<size_t>(sourceIndex)]->transform3D().setPosition(
            time, objects[i].currentPosition.x(), objects[i].currentPosition.y());
        selectedLayers[static_cast<size_t>(sourceIndex)]->changed();
        snapshots[static_cast<size_t>(sourceIndex)].afterX =
            static_cast<float>(objects[i].currentPosition.x());
        snapshots[static_cast<size_t>(sourceIndex)].afterY =
            static_cast<float>(objects[i].currentPosition.y());
    }

    if (auto* undo = UndoManager::instance();
        undo && !undo->push(std::make_unique<AlignLayersUndoCommand>(
                      snapshots, QStringLiteral("Distribute Layers")))) {
        for (size_t i = 0; i < snapshots.size() && i < static_cast<size_t>(selectedLayers.size()); ++i) {
            selectedLayers[i]->transform3D().setPosition(
                time, snapshots[i].beforeX, snapshots[i].beforeY);
            selectedLayers[i]->changed();
        }
    }
}

} // namespace Artifact

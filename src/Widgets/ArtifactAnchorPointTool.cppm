module;
#include <wobjectimpl.h>
#include <QWidget>
#include <QPushButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QButtonGroup>
#include <QColor>
#include <QFont>
#include <QPalette>
module Artifact.Widgets.AnchorPointTool;

import Artifact.Widgets.AnchorPointTool;
import Widgets.Utils.CSS;
import Artifact.Layers.Selection.Manager;
import Artifact.Layer.Abstract;
import Artifact.Application.Manager;
import Time.Rational;

namespace Artifact {

W_OBJECT_IMPL(ArtifactAnchorPointTool)

class ArtifactAnchorPointTool::Impl {
public:
    // 3x3グリッドボタン（アンカーポイント位置）
    QPushButton* anchorButtons[3][3] = {};
    QButtonGroup* buttonGroup = nullptr;
    
    QLabel* modeLabel = nullptr;
    QPushButton* applyToSelectedButton = nullptr;
    
    // 現在のモード
    bool maintainVisualPosition = true; // 見た目位置を維持
};

ArtifactAnchorPointTool::ArtifactAnchorPointTool(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // Theme setup
    setAutoFillBackground(true);
    QPalette widgetPalette = palette();
    widgetPalette.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
    widgetPalette.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
    setPalette(widgetPalette);

    // Title
    auto* titleLabel = new QLabel("Anchor Point");
    titleLabel->setFont(QFont(titleLabel->font().family(), -1, QFont::Bold));
    root->addWidget(titleLabel);

    // 3x3 グリッド
    auto* grid = new QGridLayout();
    grid->setSpacing(4);

    impl_->buttonGroup = new QButtonGroup(this);
    impl_->buttonGroup->setExclusive(true);

    // 3x3 ボタン配置
    const QString labels[3][3] = {
        {"↖", "↑", "↗"},
        {"←", "●", "→"},
        {"↙", "↓", "↘"}
    };

    const QString tooltips[3][3] = {
        {"Top-Left", "Top-Center", "Top-Right"},
        {"Middle-Left", "Center", "Middle-Right"},
        {"Bottom-Left", "Bottom-Center", "Bottom-Right"}
    };

    const AnchorPoint positions[3][3] = {
        {AnchorPoint::TopLeft,     AnchorPoint::TopCenter,     AnchorPoint::TopRight},
        {AnchorPoint::MiddleLeft,  AnchorPoint::Center,        AnchorPoint::MiddleRight},
        {AnchorPoint::BottomLeft,  AnchorPoint::BottomCenter,  AnchorPoint::BottomRight}
    };

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            auto* btn = new QPushButton(labels[row][col], this);
            btn->setToolTip(tooltips[row][col]);
            btn->setFixedSize(40, 40);
            btn->setFont(QFont("Segoe UI Symbol", 16));
            btn->setCheckable(true);

            grid->addWidget(btn, row, col);
            impl_->anchorButtons[row][col] = btn;
            impl_->buttonGroup->addButton(btn, static_cast<int>(positions[row][col]));
        }
    }

    root->addLayout(grid);

    // 選択済みボタン
    impl_->anchorButtons[1][1]->setChecked(true); // デフォルト中央

    // モード切替
    impl_->modeLabel = new QLabel("Mode: Maintain Visual Position", this);
    impl_->modeLabel->setStyleSheet("color: #888; font-size: 10px;");
    root->addWidget(impl_->modeLabel);

    // 適用ボタン
    impl_->applyToSelectedButton = new QPushButton("Apply to Selected", this);
    root->addWidget(impl_->applyToSelectedButton);

    // シグナル接続
    connect(impl_->buttonGroup, &QButtonGroup::idClicked,
            this, &ArtifactAnchorPointTool::onAnchorPointClicked);
    connect(impl_->applyToSelectedButton, &QPushButton::clicked,
            this, &ArtifactAnchorPointTool::onApplyToSelected);
}

ArtifactAnchorPointTool::~ArtifactAnchorPointTool()
{
    delete impl_;
}

void ArtifactAnchorPointTool::onAnchorPointClicked(int id)
{
    // 選択中のレイヤーに即時適用（シングルレイヤー前提）
    applyToCurrentSelection(static_cast<AnchorPoint>(id));
}

void ArtifactAnchorPointTool::onApplyToSelected()
{
    // 複数レイヤーに一括適用
    auto checkedId = impl_->buttonGroup->checkedId();
    if (checkedId < 0) return;

    applyToCurrentSelection(static_cast<AnchorPoint>(checkedId));
}

void ArtifactAnchorPointTool::applyToCurrentSelection(AnchorPoint anchorPoint)
{
    auto* selectionManager = ArtifactApplicationManager::instance()->layerSelectionManager();
    if (!selectionManager) return;

    const auto selectedSet = selectionManager->selectedLayers();
    if (selectedSet.isEmpty()) return;

    for (const auto& layer : selectedSet) {
        if (!layer) continue;
        moveAnchorPoint(layer, anchorPoint);
    }
}

void ArtifactAnchorPointTool::moveAnchorPoint(ArtifactAbstractLayerPtr layer, AnchorPoint anchorPoint)
{
    if (!layer) return;

    auto& transform = layer->transform3D();
    const ArtifactCore::RationalTime time(layer->currentFrame(), 24);
    auto bounds = layer->localBounds();

    if (!bounds.isValid() || bounds.isEmpty()) return;

    // 現在のアンカーポイント
    float currentAnchorX = transform.anchorX();
    float currentAnchorY = transform.anchorY();

    // 新しいアンカーポイントを計算
    float newAnchorX = 0.0f;
    float newAnchorY = 0.0f;

    switch (anchorPoint) {
        case AnchorPoint::TopLeft:
            newAnchorX = bounds.left();
            newAnchorY = bounds.top();
            break;
        case AnchorPoint::TopCenter:
            newAnchorX = bounds.center().x();
            newAnchorY = bounds.top();
            break;
        case AnchorPoint::TopRight:
            newAnchorX = bounds.right();
            newAnchorY = bounds.top();
            break;
        case AnchorPoint::MiddleLeft:
            newAnchorX = bounds.left();
            newAnchorY = bounds.center().y();
            break;
        case AnchorPoint::Center:
            newAnchorX = bounds.center().x();
            newAnchorY = bounds.center().y();
            break;
        case AnchorPoint::MiddleRight:
            newAnchorX = bounds.right();
            newAnchorY = bounds.center().y();
            break;
        case AnchorPoint::BottomLeft:
            newAnchorX = bounds.left();
            newAnchorY = bounds.bottom();
            break;
        case AnchorPoint::BottomCenter:
            newAnchorX = bounds.center().x();
            newAnchorY = bounds.bottom();
            break;
        case AnchorPoint::BottomRight:
            newAnchorX = bounds.right();
            newAnchorY = bounds.bottom();
            break;
    }

    if (impl_->maintainVisualPosition) {
        // 見た目位置を維持: アンカー変更分をPositionで補正
        float deltaX = newAnchorX - currentAnchorX;
        float deltaY = newAnchorY - currentAnchorY;

        transform.setAnchor(time, newAnchorX, newAnchorY, transform.anchorZ());
        transform.setPosition(time, transform.positionX() + deltaX, transform.positionY() + deltaY);
    } else {
        // アンカーポイントのみ変更（位置が動く）
        transform.setAnchor(time, newAnchorX, newAnchorY, transform.anchorZ());
    }

    layer->changed();
}

} // namespace Artifact

module;
#include <utility>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QInputDialog>
#include <QKeySequence>
#include <QDialog>
#include <QLineEdit>
#include <QMenu>
#include <QDockWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMetaObject>
#include <QStatusBar>
#include <QUrl>
#include <QtSVG/QSvgRenderer>
#include <QTimer>
#include <QSet>
#include <QVariant>
#include <wobjectimpl.h>

module Artifact.Menu.Layer;
import std;

import Event.Bus;
import Artifact.Event.Types;
import Artifact.Service.Project;
import Artifact.Service.ActiveContext;
import Artifact.Layers.Selection.Manager;
import Utils.Path;
import Utils.Id;
import Utils.String.UniString;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Layer.InitParams;
import Artifact.Layer.Factory;
import Artifact.Layer.Composition;
import Artifact.Layer.Shape;
import Artifact.Layer.Video;
import Artifact.Layer.Camera;
import Artifact.Layer.Particle;
import Layer.Blend;
import Color.Float;
import Artifact.Project.Manager;
import Artifact.Project.PresetManager;
import Artifact.Mask.LayerMask;
import Artifact.Widgets.ProjectManagerWidget;
import Artifact.Composition.Abstract;
import Artifact.Widgets.PrecomposeDialog;
import Artifact.Widgets.CreatePlaneLayerDialog;
import Artifact.Widgets.CreateCameraLayerDialog;
import Artifact.Widgets.AppDialogs;
import Artifact.Tool.CameraTracker;
import Tracking.MotionTracker;

namespace Artifact {
using namespace ArtifactCore;

namespace {

enum class LayerCreationDurationMode {
    Default,
    ToCompEnd,
    WorkArea,
    MatchSelected,
    SourceDuration,
    CustomFrames,
    Infinite
};

LayerCreationPlacementMode& layerCreationPlacementMode()
{
    static LayerCreationPlacementMode mode = LayerCreationPlacementMode::CompositionStart;
    return mode;
}

bool placeAtCurrentFrameRequested()
{
    const bool preferPlayhead =
        layerCreationPlacementMode() == LayerCreationPlacementMode::Playhead;
    const bool altPressed = (QApplication::keyboardModifiers() & Qt::AltModifier) != 0;
    return preferPlayhead ^ altPressed;
}

bool placeAtCurrentFrameRequested(const LayerCreationPlacementMode mode)
{
    return mode == LayerCreationPlacementMode::Playhead;
}

QDockWidget* findDockByTitle(QMainWindow* window, const QString& title)
{
    if (!window) {
        return nullptr;
    }
    const auto docks = window->findChildren<QDockWidget*>();
    for (QDockWidget* dock : docks) {
        if (dock && dock->windowTitle() == title) {
            return dock;
        }
    }
    return nullptr;
}

void setDockVisible(QMainWindow* window, const QString& title, bool visible)
{
    auto* dock = findDockByTitle(window, title);
    if (!dock) {
        return;
    }
    dock->setVisible(visible);
    if (visible) {
        dock->raise();
    }
}

void activateDock(QMainWindow* window, const QString& title)
{
    auto* dock = findDockByTitle(window, title);
    if (!dock) {
        return;
    }
    dock->setVisible(true);
    dock->raise();
    dock->activateWindow();
}

QSet<QString> currentLayerNames()
{
    QSet<QString> names;
    if (auto* service = ArtifactProjectService::instance()) {
        if (auto comp = service->currentComposition().lock()) {
            for (const auto& layer : comp->allLayer()) {
                if (!layer) {
                    continue;
                }
                const QString name = layer->layerName().trimmed();
                if (!name.isEmpty()) {
                    names.insert(name);
                }
            }
        }
    }
    return names;
}

QString makeUniqueSequentialName(QString baseName, const QSet<QString>& occupied)
{
    baseName = baseName.trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("Layer 1");
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

QString uniqueLayerName(const QString& baseName)
{
    return makeUniqueSequentialName(baseName, currentLayerNames());
}

ArtifactAbstractLayerPtr addDebugSolidBlendLayer(
    const CompositionID& compositionId, const QString& name, const QSize& size,
    const FloatColor& color, const LAYER_BLEND_TYPE blendMode, const float opacity)
{
    auto& manager = ArtifactProjectManager::getInstance();
    ArtifactSolidLayerInitParams params(name);
    params.setWidth(std::max(1, size.width()));
    params.setHeight(std::max(1, size.height()));
    params.setColor(color);
    auto result = manager.addLayerToComposition(
        compositionId, static_cast<ArtifactLayerInitParams&>(params));
    if (!result.success || !result.layer) {
        return {};
    }
    result.layer->setBlendMode(blendMode);
    result.layer->setOpacity(std::clamp(opacity, 0.0f, 1.0f));
    return result.layer;
}

} // namespace

W_OBJECT_IMPL(ArtifactLayerMenu)

class ArtifactLayerMenu::Impl {
public:
    Impl(ArtifactLayerMenu* menu);
    ~Impl() = default;

    ArtifactLayerMenu* menu_ = nullptr;
    QWidget* mainWindow_ = nullptr;
    ArtifactCore::LayerID selectedLayerId_;
    std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

    QMenu* createMenu = nullptr;
    QMenu* createShapeMenu = nullptr;
    QMenu* createPlacementMenu = nullptr;
    QMenu* switchMenu = nullptr;
    QMenu* selectMenu = nullptr;
    QMenu* proxyMenu = nullptr;
    QMenu* debugMenu = nullptr;
    QActionGroup* proxyQualityGroup = nullptr;

    QAction* createSolidAction = nullptr;
    QAction* createNullAction = nullptr;
    QAction* createConstructionAction = nullptr;
    QAction* createAdjustAction = nullptr;
    QAction* createTextAction = nullptr;
    QAction* createParticleAction = nullptr;
    QAction* createCameraAction = nullptr;
    QAction* createLightAction = nullptr;
    QAction* createAudioAction = nullptr;
    QAction* createSvgAction = nullptr;
    QAction* createModel3DAction = nullptr;
    QAction* createPlane3DAction = nullptr;
    QAction* placementAtCompStartAction = nullptr;
    QAction* placementAtPlayheadAction = nullptr;
    QAction* cycleLayerForwardAction = nullptr;
    QAction* cycleLayerReverseAction = nullptr;
    QAction* cycleShapeForwardAction = nullptr;
    QAction* cycleShapeReverseAction = nullptr;
    QAction* createShapeRectAction = nullptr;
    QAction* createShapeSquareAction = nullptr;
    QAction* createShapePolygonAction = nullptr;
    QAction* createShapeTriangleAction = nullptr;
    QAction* createShapeEllipseAction = nullptr;
    QAction* createShapeStarAction = nullptr;
    QAction* trackCameraAction = nullptr;
    QAction* createMotionTrackerAction = nullptr;

    QAction* duplicateLayerAction = nullptr;
    QAction* renameLayerAction = nullptr;
    QAction* deleteLayerAction = nullptr;

    QAction* toggleVisibleAction = nullptr;
    QAction* toggleLockAction = nullptr;
    QAction* toggleSoloAction = nullptr;
    QAction* toggleShyAction = nullptr;
    QAction* soloOnlyAction = nullptr;
    QAction* proxyNoneAction = nullptr;
    QAction* proxyQuarterAction = nullptr;
    QAction* proxyHalfAction = nullptr;
    QAction* proxyFullAction = nullptr;
    QAction* generateProxyAction = nullptr;
    QAction* revealProxyAction = nullptr;
    QAction* clearProxyAction = nullptr;
    QAction* generateSelectedProxyAction = nullptr;
    QAction* clearSelectedProxyAction = nullptr;
    QAction* saveMaskPresetAction = nullptr;
    QAction* loadMaskPresetAction = nullptr;

    QAction* selectParentAction = nullptr;
    QAction* clearParentAction = nullptr;
    QAction* openInspectorAction = nullptr;
    QAction* openPropertiesAction = nullptr;

    QAction* addDebugBlendLayersAction = nullptr;
    QAction* addDebugBillboardLayerAction = nullptr;
    QAction* addDebugParticleLayerAction = nullptr;

    QAction* precomposeAction = nullptr;
    QAction* unprecomposeAction = nullptr;
    QAction* groupSelectionAction = nullptr;
    QAction* ungroupAction = nullptr;
    QAction* splitAction = nullptr;

    void handleCreateSolid();
    void handleCreateNull();
    void handleCreateConstruction();
    void handleCreateAdjust();
    void handleCreateText();
    void handleCreateParticle();
    void handleCreateCamera();
    void handleCreateLight();
    void handleCreateAudio();
    void handleCreateSvg();
    void handleCreateModel3D();
    void handleCreatePlane3D();
    void handleCycleLayerCreation(bool reverse);
    void handleCycleShapeCreation(bool reverse);
    void handleCreateShape(ShapeType type, const QString& nameBase);
    void handleCreateMotionTracker();

    void handleDuplicateLayer();
    void handleRenameLayer();
    void handleDeleteLayer();

    void handleToggleVisible();
    void handleToggleLock();
    void handleToggleSolo();
    void handleToggleShy();
    void handleSoloOnlySelected();
    void handleSetProxyQuality(ProxyQuality quality);
    void handleGenerateProxy();
    void handleRevealProxy();
    void handleClearProxy();
    void handleGenerateSelectedProxies();
    void handleClearSelectedProxies();
    void handleSaveMaskPreset();
    void handleLoadMaskPreset();

    void handleSelectParent();
    void handleClearParent();
    void handleOpenInspector();
    void handleOpenProperties();

    void handlePrecompose();
    void handleUnprecompose();
    void handleGroupSelection();
    void handleUngroup();
    void handleSplitLayer();
    void handleTrackCamera();

    bool hasCurrentComposition() const;
    bool ensureCurrentComposition();
    bool hasSelectedLayer() const;
    QStringList selectedVideoSourcePathsInCurrentComposition() const;
    void refreshEnabledState();
};

ArtifactLayerMenu::Impl::Impl(ArtifactLayerMenu* menu) : menu_(menu)
{
    createMenu = new QMenu("新規(&N)", menu);
    createMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_add.svg")));
    createSolidAction = new QAction("平面(&Y)...", createMenu);
    createSolidAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    createSolidAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_palette.svg")));

    createNullAction = new QAction("ヌルオブジェクト(&N)", createMenu);
    createNullAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_Y));
    createNullAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_aspect_ratio.svg")));

    createConstructionAction = new QAction("Construction Layer", createMenu);
    createConstructionAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_grid_on.svg")));
    createConstructionAction->setToolTip(QStringLiteral("Create a construction layer (editor-only by default)"));

    createAdjustAction = new QAction("調整レイヤー(&A)", createMenu);
    createAdjustAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Y));
    createAdjustAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_blur_on.svg")));

    createTextAction = new QAction("テキスト(&T)", createMenu);
    createTextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_T));
    createTextAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_title.svg")));

    createParticleAction = new QAction("パーティクル(&P)", createMenu);
    createParticleAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_particle.svg")));

    createCameraAction = new QAction("カメラ(&C)", createMenu);
    createCameraAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_videocam.svg")));

    createLightAction = new QAction("ライト(&L)", createMenu);
    createLightAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_wb_sunny.svg")));

    createAudioAction = new QAction("オーディオ(&U)...", createMenu);
    createAudioAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_audiotrack.svg")));

    createSvgAction = new QAction("SVG シェイプレイヤー(&V)...", createMenu);
    createSvgAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_svg_layer.svg")));
    createModel3DAction = new QAction("3Dモデルレイヤー(&3)...", createMenu);
    createModel3DAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_model3d.svg")));
    createModel3DAction->setToolTip(QStringLiteral("Import a 3D model as a layer"));
    createPlane3DAction = new QAction("3D平面レイヤー(&P)", createMenu);
    createPlane3DAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_shape_square.svg")));
    createPlane3DAction->setToolTip(QStringLiteral("Create a fixed plane as a 3D layer"));
    createPlacementMenu = new QMenu("作成位置(&O)", createMenu);
    createPlacementMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_settings.svg")));
    auto* placementGroup = new QActionGroup(createPlacementMenu);
    placementGroup->setExclusive(true);
    placementAtCompStartAction = new QAction("コンポジション開始", createPlacementMenu);
    placementAtCompStartAction->setCheckable(true);
    placementAtCompStartAction->setChecked(true);
    placementAtCompStartAction->setToolTip(QStringLiteral("新規レイヤーをコンポジション開始に配置します"));
    placementAtPlayheadAction = new QAction("再生ヘッド", createPlacementMenu);
    placementAtPlayheadAction->setCheckable(true);
    placementAtPlayheadAction->setToolTip(QStringLiteral("新規レイヤーを再生ヘッドに配置します"));
    placementGroup->addAction(placementAtCompStartAction);
    placementGroup->addAction(placementAtPlayheadAction);
    createPlacementMenu->addAction(placementAtCompStartAction);
    createPlacementMenu->addAction(placementAtPlayheadAction);
    cycleLayerForwardAction = new QAction("レイヤーを次々作成", createMenu);
    cycleLayerForwardAction->setToolTip(QStringLiteral("Cycle common layer creation presets"));
    cycleLayerForwardAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N));
    cycleLayerReverseAction = new QAction("レイヤーを逆順で次々作成", createMenu);
    cycleLayerReverseAction->setToolTip(QStringLiteral("Cycle common layer creation presets in reverse"));
    cycleLayerReverseAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_N));

    createShapeMenu = new QMenu("シェイプ(&S)", createMenu);
    createShapeMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_shape_rect.svg")));
    cycleShapeForwardAction = new QAction("シェイプを次々作成", createShapeMenu);
    cycleShapeForwardAction->setToolTip(QStringLiteral("Cycle shape presets"));
    cycleShapeForwardAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    cycleShapeReverseAction = new QAction("シェイプを逆順で次々作成", createShapeMenu);
    cycleShapeReverseAction->setToolTip(QStringLiteral("Cycle shape presets in reverse"));
    cycleShapeReverseAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_S));
    createShapeRectAction = new QAction("四角形", createShapeMenu);
    createShapeRectAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_shape_rect.svg")));
    createShapeSquareAction = new QAction("正方形", createShapeMenu);
    createShapeSquareAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_shape_square.svg")));
    createShapePolygonAction = new QAction("多角形", createShapeMenu);
    createShapePolygonAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_shape_polygon.svg")));
    createShapeTriangleAction = new QAction("三角形", createShapeMenu);
    createShapeTriangleAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_shape_triangle.svg")));
    createShapeEllipseAction = new QAction("楕円", createShapeMenu);
    createShapeEllipseAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_shape_ellipse.svg")));
    createShapeStarAction = new QAction("星形", createShapeMenu);
    createShapeStarAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_shape_star.svg")));
    createShapeMenu->addAction(createShapeRectAction);
    createShapeMenu->addAction(createShapeSquareAction);
    createShapeMenu->addAction(createShapePolygonAction);
    createShapeMenu->addAction(createShapeTriangleAction);
    createShapeMenu->addAction(createShapeEllipseAction);
    createShapeMenu->addAction(createShapeStarAction);
    createShapeMenu->addSeparator();
    createShapeMenu->addAction(cycleShapeForwardAction);
    createShapeMenu->addAction(cycleShapeReverseAction);

    trackCameraAction = new QAction("3Dカメラトラッキング(&T)", menu);
    trackCameraAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_videocam.svg")));
    createMotionTrackerAction = new QAction("モーショントラッカーを作成(&M)", menu);

    createMenu->addAction(createSolidAction);
    createMenu->addAction(createNullAction);
    createMenu->addAction(createConstructionAction);
    createMenu->addAction(createAdjustAction);
    createMenu->addAction(createTextAction);
    createMenu->addAction(createParticleAction);
    createMenu->addAction(createCameraAction);
    createMenu->addAction(createLightAction);
    createMenu->addAction(createAudioAction);
    createMenu->addAction(createSvgAction);
    createMenu->addAction(createModel3DAction);
    createMenu->addAction(createPlane3DAction);
    createMenu->addMenu(createPlacementMenu);
    createMenu->addAction(cycleLayerForwardAction);
    createMenu->addAction(cycleLayerReverseAction);
    createMenu->addMenu(createShapeMenu);

    duplicateLayerAction = new QAction("レイヤーを複製(&D)", menu);
    duplicateLayerAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    duplicateLayerAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_content_copy.svg")));
    renameLayerAction = new QAction("レイヤー名を変更(&R)...", menu);
    renameLayerAction->setShortcut(QKeySequence(Qt::Key_F2));
    renameLayerAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_rename.svg")));
    deleteLayerAction = new QAction("削除(&X)", menu);
    deleteLayerAction->setShortcut(QKeySequence(Qt::Key_Delete));
    deleteLayerAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_delete.svg")));

    switchMenu = new QMenu("スイッチ(&S)", menu);
    switchMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_settings.svg")));
    toggleVisibleAction = new QAction("表示/非表示を切替", switchMenu);
    toggleVisibleAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_visibility.svg")));
    toggleLockAction = new QAction("ロックを切替", switchMenu);
    toggleLockAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_lock.svg")));
    toggleSoloAction = new QAction("ソロを切替", switchMenu);
    toggleSoloAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_headset.svg")));
    toggleShyAction = new QAction("シャイを切替", switchMenu);
    toggleShyAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_shy.svg")));
    soloOnlyAction = new QAction("Smart Solo", switchMenu);
    soloOnlyAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_solo_only.svg")));
    soloOnlyAction->setToolTip(QStringLiteral("選択レイヤーと必要な Parent / Matte をまとめてソロ表示します"));
    switchMenu->addAction(toggleVisibleAction);
    switchMenu->addAction(toggleLockAction);
    switchMenu->addAction(toggleSoloAction);
    switchMenu->addAction(toggleShyAction);
    switchMenu->addSeparator();
    switchMenu->addAction(soloOnlyAction);

    selectMenu = new QMenu("選択(&E)", menu);
    selectMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_select_all.svg")));
    selectParentAction = new QAction("親を選択", selectMenu);
    selectParentAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_parent_select.svg")));
    clearParentAction = new QAction("親を解除", selectMenu);
    clearParentAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_parent_clear.svg")));
    selectMenu->addAction(selectParentAction);
    selectMenu->addAction(clearParentAction);

    proxyMenu = new QMenu("Proxy 画質(&Q)", menu);
    proxyMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_resolution_half.svg")));
    debugMenu = new QMenu("デバッグレイヤー(&D)", menu);
    debugMenu->setIcon(QIcon(resolveIconPath("Studio/testmenu_layer_composite.svg")));
    proxyQualityGroup = new QActionGroup(menu);
    proxyQualityGroup->setExclusive(true);
    proxyNoneAction = proxyMenu->addAction("無効");
    proxyNoneAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_resolution_full.svg")));
    proxyQuarterAction = proxyMenu->addAction("1/4 画質");
    proxyQuarterAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_resolution_quarter.svg")));
    proxyHalfAction = proxyMenu->addAction("1/2 画質");
    proxyHalfAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_resolution_half.svg")));
    proxyFullAction = proxyMenu->addAction("フル画質");
    proxyFullAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_resolution_full.svg")));
    generateProxyAction = proxyMenu->addAction("プロキシを生成");
    generateProxyAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_replay.svg")));
    generateSelectedProxyAction = proxyMenu->addAction("選択レイヤーのプロキシを生成");
    generateSelectedProxyAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_replay.svg")));
    proxyMenu->addSeparator();
    revealProxyAction = proxyMenu->addAction("プロキシを表示");
    revealProxyAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_folder_open.svg")));
    clearProxyAction = proxyMenu->addAction("プロキシを削除");
    clearProxyAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_delete.svg")));
    clearSelectedProxyAction = proxyMenu->addAction("選択レイヤーのプロキシを削除");
    clearSelectedProxyAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_delete.svg")));
    proxyMenu->addSeparator();
    saveMaskPresetAction = proxyMenu->addAction("マスクをプリセットとして保存...");
    saveMaskPresetAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_save.svg")));
    loadMaskPresetAction = proxyMenu->addAction("マスクプリセットを適用...");
    loadMaskPresetAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_folder_open.svg")));
    for (auto *action : {proxyNoneAction, proxyQuarterAction, proxyHalfAction, proxyFullAction}) {
        action->setCheckable(true);
        proxyQualityGroup->addAction(action);
    }

    openInspectorAction = new QAction("Inspector を開く", menu);
    openInspectorAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_inspector.svg")));
    openPropertiesAction = new QAction("Properties を開く", menu);
    openPropertiesAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_settings.svg")));

    addDebugBlendLayersAction = new QAction("Debug Blend Test Layers...", debugMenu);
    addDebugBlendLayersAction->setIcon(QIcon(resolveIconPath("Studio/testmenu_layer_composite.svg")));
    addDebugBlendLayersAction->setToolTip(
        QStringLiteral("Debug 用の合成テストレイヤーをまとめて追加します"));
    debugMenu->addAction(addDebugBlendLayersAction);

    addDebugBillboardLayerAction = new QAction("Debug Billboard Layer...", debugMenu);
    addDebugBillboardLayerAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_particle.svg")));
    addDebugBillboardLayerAction->setToolTip(
        QStringLiteral("ビルボード描画の検証用に、見やすい粒子レイヤーを追加します"));
    debugMenu->addAction(addDebugBillboardLayerAction);

    addDebugParticleLayerAction = new QAction("Debug Particle Layer...", debugMenu);
    addDebugParticleLayerAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_particle.svg")));
    addDebugParticleLayerAction->setToolTip(
        QStringLiteral("独立したデバッグ用パーティクルレイヤーを追加します"));
    debugMenu->addAction(addDebugParticleLayerAction);

    precomposeAction = new QAction("プリコンポーズ(&P)...", menu);
    precomposeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    precomposeAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_view_comfy.svg")));
    unprecomposeAction = new QAction("プリコンポーズを解除", menu);
    unprecomposeAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_ungroup.svg")));
    groupSelectionAction = new QAction("グループ化(&G)...", menu);
    groupSelectionAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_group.svg")));
    ungroupAction = new QAction("グループ解除(&U)", menu);
    ungroupAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_ungroup.svg")));
    splitAction = new QAction("レイヤー分割(&L)", menu);
    splitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    splitAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_content_cut.svg")));

    menu->addMenu(createMenu);
    menu->addAction(trackCameraAction);
    menu->addAction(createMotionTrackerAction);
    menu->addSeparator();
    menu->addAction(duplicateLayerAction);
    menu->addAction(renameLayerAction);
    menu->addAction(deleteLayerAction);
    menu->addSeparator();
    menu->addMenu(switchMenu);
    menu->addMenu(selectMenu);
    menu->addMenu(proxyMenu);
    menu->addMenu(debugMenu);
    menu->addSeparator();
    menu->addAction(openInspectorAction);
    menu->addAction(openPropertiesAction);
    menu->addSeparator();
    menu->addAction(precomposeAction);
    menu->addAction(unprecomposeAction);
    menu->addAction(groupSelectionAction);
    menu->addAction(ungroupAction);
    menu->addAction(splitAction);

    auto dispatchAction = [this](QAction* action) {
        if (!action) {
            return;
        }
        if (action == createSolidAction) { handleCreateSolid(); return; }
        if (action == createNullAction) { handleCreateNull(); return; }
        if (action == createConstructionAction) { handleCreateConstruction(); return; }
        if (action == createAdjustAction) { handleCreateAdjust(); return; }
        if (action == createTextAction) { handleCreateText(); return; }
        if (action == createParticleAction) { handleCreateParticle(); return; }
        if (action == createCameraAction) { handleCreateCamera(); return; }
        if (action == createLightAction) { handleCreateLight(); return; }
        if (action == createAudioAction) { handleCreateAudio(); return; }
        if (action == createSvgAction) { handleCreateSvg(); return; }
        if (action == createModel3DAction) { handleCreateModel3D(); return; }
        if (action == createPlane3DAction) { handleCreatePlane3D(); return; }
        if (action == placementAtCompStartAction) {
            layerCreationPlacementMode() = LayerCreationPlacementMode::CompositionStart;
            placementAtCompStartAction->setChecked(true);
            placementAtPlayheadAction->setChecked(false);
            return;
        }
        if (action == placementAtPlayheadAction) {
            layerCreationPlacementMode() = LayerCreationPlacementMode::Playhead;
            placementAtCompStartAction->setChecked(false);
            placementAtPlayheadAction->setChecked(true);
            return;
        }
        if (action == cycleLayerForwardAction) { handleCycleLayerCreation(false); return; }
        if (action == cycleLayerReverseAction) { handleCycleLayerCreation(true); return; }
        if (action == createShapeRectAction) { handleCreateShape(ShapeType::Rect, QStringLiteral("Shape 1")); return; }
        if (action == createShapeSquareAction) { handleCreateShape(ShapeType::Square, QStringLiteral("Square 1")); return; }
        if (action == createShapePolygonAction) { handleCreateShape(ShapeType::Polygon, QStringLiteral("Polygon 1")); return; }
        if (action == createShapeTriangleAction) { handleCreateShape(ShapeType::Triangle, QStringLiteral("Triangle 1")); return; }
        if (action == createShapeEllipseAction) { handleCreateShape(ShapeType::Ellipse, QStringLiteral("Ellipse 1")); return; }
        if (action == createShapeStarAction) { handleCreateShape(ShapeType::Star, QStringLiteral("Star 1")); return; }
        if (action == cycleShapeForwardAction) { handleCycleShapeCreation(false); return; }
        if (action == cycleShapeReverseAction) { handleCycleShapeCreation(true); return; }
        if (action == duplicateLayerAction) { handleDuplicateLayer(); return; }
        if (action == renameLayerAction) { handleRenameLayer(); return; }
        if (action == deleteLayerAction) { handleDeleteLayer(); return; }
        if (action == toggleVisibleAction) { handleToggleVisible(); return; }
        if (action == toggleLockAction) { handleToggleLock(); return; }
        if (action == toggleSoloAction) { handleToggleSolo(); return; }
        if (action == toggleShyAction) { handleToggleShy(); return; }
        if (action == soloOnlyAction) { handleSoloOnlySelected(); return; }
        if (action == selectParentAction) { handleSelectParent(); return; }
        if (action == clearParentAction) { handleClearParent(); return; }
        if (action == proxyNoneAction) { handleSetProxyQuality(ProxyQuality::None); return; }
        if (action == proxyQuarterAction) { handleSetProxyQuality(ProxyQuality::Quarter); return; }
        if (action == proxyHalfAction) { handleSetProxyQuality(ProxyQuality::Half); return; }
        if (action == proxyFullAction) { handleSetProxyQuality(ProxyQuality::Full); return; }
        if (action == generateProxyAction) { handleGenerateProxy(); return; }
        if (action == generateSelectedProxyAction) { handleGenerateSelectedProxies(); return; }
        if (action == revealProxyAction) { handleRevealProxy(); return; }
        if (action == clearProxyAction) { handleClearProxy(); return; }
        if (action == clearSelectedProxyAction) { handleClearSelectedProxies(); return; }
        if (action == saveMaskPresetAction) { handleSaveMaskPreset(); return; }
        if (action == loadMaskPresetAction) { handleLoadMaskPreset(); return; }
        if (action == openInspectorAction) { handleOpenInspector(); return; }
        if (action == openPropertiesAction) { handleOpenProperties(); return; }
        if (action == addDebugBlendLayersAction) {
            auto* projectService = ArtifactProjectService::instance();
            if (!projectService) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "ProjectService が利用できません。");
                return;
            }
            auto comp = projectService->currentComposition().lock();
            if (!comp) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "先にコンポジションを開いてください。");
                return;
            }

            const QSize compSize = comp->effectiveCompositionSize().isValid()
                                       ? comp->effectiveCompositionSize()
                                       : QSize(1920, 1080);
            const CompositionID compositionId = comp->id();

            ArtifactAbstractLayerPtr lastCreatedLayer;
            lastCreatedLayer = addDebugSolidBlendLayer(
                compositionId, QStringLiteral("Debug Base Plate"), compSize,
                FloatColor(0.32f, 0.32f, 0.36f, 1.0f), LAYER_BLEND_TYPE::BLEND_NORMAL,
                1.0f);
            if (!lastCreatedLayer) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "デバッグ用ベースレイヤーの追加に失敗しました。");
                return;
            }

            lastCreatedLayer = addDebugSolidBlendLayer(
                compositionId, QStringLiteral("Debug Multiply Plate"), compSize,
                FloatColor(0.78f, 0.42f, 0.18f, 1.0f), LAYER_BLEND_TYPE::BLEND_MULTIPLY,
                0.58f);
            if (!lastCreatedLayer) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "Multiply テストレイヤーの追加に失敗しました。");
                return;
            }

            lastCreatedLayer = addDebugSolidBlendLayer(
                compositionId, QStringLiteral("Debug Screen Plate"), compSize,
                FloatColor(0.18f, 0.72f, 0.98f, 1.0f), LAYER_BLEND_TYPE::BLEND_SCREEN,
                0.52f);
            if (!lastCreatedLayer) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "Screen テストレイヤーの追加に失敗しました。");
                return;
            }

            projectService->selectLayer(lastCreatedLayer->id());
            QMessageBox::information(
                menu_->window(), "Debug Layers",
                QStringLiteral("Debug blend test layers を追加しました。\n\n"
                               "- Debug Base Plate\n"
                               "- Debug Multiply Plate\n"
                               "- Debug Screen Plate\n\n"
                               "タイムライン上で並び替えたり、不透明度を変えて合成検証できます。"));
            return;
        }
        if (action == addDebugBillboardLayerAction) {
            auto* service = ArtifactProjectService::instance();
            if (!service) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "ProjectService が利用できません。");
                return;
            }
            const auto comp = service->currentComposition().lock();
            if (!comp) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "先にコンポジションを開いてください。");
                return;
            }

            ArtifactLayerInitParams params(uniqueLayerName(QStringLiteral("Debug Billboard Particle")),
                                           LayerType::Particle);
            service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());

            const auto created = ArtifactLayerSelectionManager::instance()
                                     ? ArtifactLayerSelectionManager::instance()->currentLayer()
                                     : ArtifactAbstractLayerPtr{};
            const auto particleLayer = std::dynamic_pointer_cast<ArtifactParticleLayer>(created);
            if (!particleLayer) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "Particle レイヤーの生成に失敗しました。");
                return;
            }

            particleLayer->setLayerName(QStringLiteral("Debug Billboard Particle"));
            particleLayer->loadPreset(QStringLiteral("sparkles"));
            particleLayer->setOpacity(1.0f);
            particleLayer->changed();
            service->selectLayer(particleLayer->id());

            QMessageBox::information(
                menu_->window(), "Debug Layers",
                QStringLiteral("Debug Billboard Particle を追加しました。\n\n"
                "sparkles プリセットを使うので、ビルボード描画の見え方を"
                "確認しやすいはずです。"));
            return;
        }
        if (action == addDebugParticleLayerAction) {
            auto* service = ArtifactProjectService::instance();
            if (!service) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "ProjectService が利用できません。");
                return;
            }
            if (!ensureCurrentComposition()) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "先にコンポジションを作成してください。");
                return;
            }

            auto comp = service->currentComposition().lock();
            if (!comp) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "現在のコンポジションを取得できません。");
                return;
            }

            auto layer = createParticleDebugLayer();
            if (!layer) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "デバッグ用パーティクルレイヤーの生成に失敗しました。");
                return;
            }

            layer->setLayerName(uniqueLayerName(QStringLiteral("Debug Particle Layer")));
            layer->loadPreset(QStringLiteral("sparkles"));
            layer->setOpacity(1.0f);
            if (placeAtCurrentFrameRequested() && !layer->isTimingLocked()) {
                const qint64 activeFrame = comp->framePosition().framePosition();
                const qint64 duration =
                    std::max<qint64>(1, layer->outPoint().framePosition() -
                                            layer->inPoint().framePosition());
                layer->setInPoint(FramePosition(activeFrame));
                layer->setOutPoint(FramePosition(activeFrame + duration));
                layer->setStartTime(FramePosition(activeFrame));
            }

            auto appendResult = comp->appendLayerTop(layer);
            if (!appendResult.success) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     QStringLiteral("レイヤーの追加に失敗しました: %1")
                                         .arg(appendResult.message));
                return;
            }

            service->selectLayer(layer->id());
            QMessageBox::information(
                menu_->window(), "Debug Layers",
                QStringLiteral("Debug Particle Layer を追加しました。\n\n"
                               "通常の ParticleLayer とは独立したデバッグ用レイヤーです。"));
            return;
        }
        if (action == precomposeAction) { handlePrecompose(); return; }
        if (action == unprecomposeAction) { handleUnprecompose(); return; }
        if (action == groupSelectionAction) { handleGroupSelection(); return; }
        if (action == ungroupAction) { handleUngroup(); return; }
        if (action == splitAction) { handleSplitLayer(); return; }
        if (action == trackCameraAction) { handleTrackCamera(); return; }
        if (action == createMotionTrackerAction) { handleCreateMotionTracker(); return; }
    };

    QObject::connect(menu, &QMenu::triggered, menu, dispatchAction);

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
        eventBus.subscribe<CurrentCompositionChangedEvent>(
            [this](const CurrentCompositionChangedEvent&) {
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

bool ArtifactLayerMenu::Impl::hasCurrentComposition() const
{
    auto* service = ArtifactProjectService::instance();
    return service && static_cast<bool>(service->currentComposition().lock());
}

bool ArtifactLayerMenu::Impl::ensureCurrentComposition()
{
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return false;
    }
    if (service->currentComposition().lock()) {
        return true;
    }
    if (!service->hasProject()) {
        return false;
    }

    service->createComposition(UniString(QStringLiteral("Composition")));
    return static_cast<bool>(service->currentComposition().lock());
}

bool ArtifactLayerMenu::Impl::hasSelectedLayer() const
{
    return hasCurrentComposition() && !selectedLayerId_.isNil();
}

void ArtifactLayerMenu::Impl::refreshEnabledState()
{
    auto* service = ArtifactProjectService::instance();
    const bool hasProject = service && service->hasProject();
    const bool hasComp = hasCurrentComposition();
    const bool hasLayer = hasSelectedLayer();
    bool hasMask = false;
    if (hasLayer && service) {
        if (auto comp = service->currentComposition().lock()) {
            if (auto layer = comp->layerById(selectedLayerId_)) {
                hasMask = layer->hasMasks();
            }
        }
    }
    const bool hasParent = hasLayer && service && service->layerHasParentInCurrentComposition(selectedLayerId_);

    // Creation actions can auto-create first composition when a project exists.
    createSolidAction->setEnabled(hasProject);
    createNullAction->setEnabled(hasProject);
    createConstructionAction->setEnabled(hasProject);
    createAdjustAction->setEnabled(hasProject);
    createTextAction->setEnabled(hasProject);
    createParticleAction->setEnabled(hasProject);
    createCameraAction->setEnabled(hasProject);
    createLightAction->setEnabled(hasProject);
    createAudioAction->setEnabled(hasProject);
    createSvgAction->setEnabled(hasProject);
    createModel3DAction->setEnabled(hasProject);
    createPlane3DAction->setEnabled(hasProject);
    addDebugParticleLayerAction->setEnabled(hasProject);
    createShapeRectAction->setEnabled(hasProject);
    createShapeSquareAction->setEnabled(hasProject);
    createShapePolygonAction->setEnabled(hasProject);
    createShapeTriangleAction->setEnabled(hasProject);
    createShapeEllipseAction->setEnabled(hasProject);
    createShapeStarAction->setEnabled(hasProject);

    duplicateLayerAction->setEnabled(hasLayer);
    renameLayerAction->setEnabled(hasLayer);
    deleteLayerAction->setEnabled(hasLayer);
    if (hasLayer) {
        const bool visible = service && service->isLayerVisibleInCurrentComposition(selectedLayerId_);
        const bool locked = service && service->isLayerLockedInCurrentComposition(selectedLayerId_);
        const bool solo = service && service->isLayerSoloInCurrentComposition(selectedLayerId_);
        const bool shy = service && service->isLayerShyInCurrentComposition(selectedLayerId_);
        toggleVisibleAction->setText(visible ? QStringLiteral("非表示にする") : QStringLiteral("表示する"));
        toggleLockAction->setText(locked ? QStringLiteral("ロックを解除") : QStringLiteral("ロックする"));
        toggleSoloAction->setText(solo ? QStringLiteral("ソロを解除") : QStringLiteral("ソロにする"));
        toggleShyAction->setText(shy ? QStringLiteral("シャイを解除") : QStringLiteral("シャイにする"));
    } else {
        toggleVisibleAction->setText(QStringLiteral("表示/非表示を切替"));
        toggleLockAction->setText(QStringLiteral("ロックを切替"));
        toggleSoloAction->setText(QStringLiteral("ソロを切替"));
        toggleShyAction->setText(QStringLiteral("シャイを切替"));
    }
    toggleVisibleAction->setEnabled(hasLayer);
    toggleLockAction->setEnabled(hasLayer);
    toggleSoloAction->setEnabled(hasLayer);
    toggleShyAction->setEnabled(hasLayer);
    soloOnlyAction->setEnabled(hasLayer && hasComp);
    if (hasLayer && hasParent) {
        const auto parentId = service->layerParentIdInCurrentComposition(selectedLayerId_);
        const QString parentName = service ? service->layerNameInCurrentComposition(parentId) : QString();
        const QString displayName = parentName.trimmed().isEmpty()
                                        ? QStringLiteral("Parent")
                                        : parentName.trimmed();
        selectParentAction->setText(QStringLiteral("親を選択 (%1)").arg(displayName));
        clearParentAction->setText(QStringLiteral("親を解除 (%1)").arg(displayName));
    } else {
        selectParentAction->setText(QStringLiteral("親を選択"));
        clearParentAction->setText(QStringLiteral("親を解除"));
    }
    selectParentAction->setEnabled(hasParent);
    clearParentAction->setEnabled(hasParent);
    openInspectorAction->setEnabled(hasLayer);
    openPropertiesAction->setEnabled(hasLayer);
    precomposeAction->setEnabled(hasLayer);
    bool isPrecomposeLayer = false;
    if (hasLayer) {
        if (auto *svc = ArtifactProjectService::instance()) {
            auto comp = svc->currentComposition().lock();
            auto compLayer = comp ? std::dynamic_pointer_cast<ArtifactCompositionLayer>(
                                        comp->layerById(selectedLayerId_))
                                  : nullptr;
            isPrecomposeLayer = compLayer && !compLayer->sourceCompositionId().isNil();
        }
    }
    unprecomposeAction->setEnabled(isPrecomposeLayer);
    groupSelectionAction->setEnabled(hasLayer && hasComp);
    
    // Ungroup: 選択中のレイヤーがグループの場合のみ有効
    bool isGroupSelected = false;
    auto* app = ArtifactApplicationManager::instance();
    if (app && app->layerSelectionManager()) {
        auto current = app->layerSelectionManager()->currentLayer();
        isGroupSelected = current && current->isGroupLayer();
    }
    ungroupAction->setEnabled(isGroupSelected && hasComp);
    
    splitAction->setEnabled(hasLayer);

    // 3Dカメラトラッキング: 動画レイヤーが選択されている場合のみ有効
    bool isVideoSelected = false;
    bool hasProxy = false;
    int selectedVideoCount = 0;
    int selectedVideoProxyCount = 0;
    ProxyQuality proxyQuality = ProxyQuality::None;
    if (hasLayer && hasComp) {
        if (auto comp = service->currentComposition().lock()) {
            if (auto layer = comp->layerById(selectedLayerId_)) {
                isVideoSelected = layer->hasVideo();
                if (auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
                    proxyQuality = videoLayer->proxyQuality();
                    hasProxy = videoLayer->hasProxy();
                }
            }
        }
    }

    auto* selectionManager = app ? app->layerSelectionManager() : nullptr;
    if (hasComp && selectionManager) {
        const auto selected = selectionManager->selectedLayers();
        auto isSelected = [&selected](const LayerID& id) {
            for (const auto& layer : selected) {
                if (layer && layer->id() == id) {
                    return true;
                }
            }
            return false;
        };
        if (auto comp = service->currentComposition().lock()) {
            QSet<QString> seen;
            for (const auto& layer : comp->allLayer()) {
                if (!layer || !isSelected(layer->id())) {
                    continue;
                }
                auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer);
                if (!videoLayer) {
                    continue;
                }
                const QString sourcePath = videoLayer->sourcePath().trimmed();
                if (sourcePath.isEmpty() || seen.contains(sourcePath)) {
                    continue;
                }
                seen.insert(sourcePath);
                ++selectedVideoCount;
                if (videoLayer->hasProxy()) {
                    ++selectedVideoProxyCount;
                }
            }
        }
    }
    trackCameraAction->setEnabled(isVideoSelected);
    createMotionTrackerAction->setEnabled(isVideoSelected);
    proxyMenu->setEnabled(isVideoSelected);
    generateProxyAction->setEnabled(isVideoSelected);
    revealProxyAction->setEnabled(isVideoSelected && hasProxy);
    clearProxyAction->setEnabled(isVideoSelected && hasProxy);
    generateSelectedProxyAction->setEnabled(selectedVideoCount > 1);
    clearSelectedProxyAction->setEnabled(selectedVideoCount > 1 && selectedVideoProxyCount > 0);
    saveMaskPresetAction->setEnabled(hasMask);
    loadMaskPresetAction->setEnabled(hasLayer);
    if (isVideoSelected) {
        proxyNoneAction->setChecked(proxyQuality == ProxyQuality::None);
        proxyQuarterAction->setChecked(proxyQuality == ProxyQuality::Quarter);
        proxyHalfAction->setChecked(proxyQuality == ProxyQuality::Half);
        proxyFullAction->setChecked(proxyQuality == ProxyQuality::Full);
    } else {
        proxyNoneAction->setChecked(false);
        proxyQuarterAction->setChecked(false);
        proxyHalfAction->setChecked(false);
        proxyFullAction->setChecked(false);
    }
}

void ArtifactLayerMenu::Impl::handleCreateSolid()
{
    auto service = ArtifactProjectService::instance();
    if (!ensureCurrentComposition()) {
        QWidget* parentWindow = mainWindow_ ? mainWindow_ : (menu_ ? menu_->window() : nullptr);
        QMessageBox::warning(parentWindow, "Layer", "コンポジションが選択されていません。");
        return;
    }
    auto* const menu = menu_;
    QWidget* parentWindow = mainWindow_ ? mainWindow_ : (menu_ ? menu_->window() : nullptr);
    CreateSolidLayerSettingDialog dialog(layerCreationPlacementMode(), parentWindow);
    dialog.setModal(true);
    if (dialog.exec() != QDialog::Accepted || !service) {
        return;
    }
    const ArtifactSolidLayerInitParams params = dialog.submittedParams();
    const bool placeAtCurrentFrame =
        placeAtCurrentFrameRequested(dialog.submittedPlacementMode());
    QTimer::singleShot(0, menu, [service, params, menu, placeAtCurrentFrame]() {
        Q_UNUSED(menu);
        service->addLayerToCurrentComposition(params, true, placeAtCurrentFrame);
    });
}

void ArtifactLayerMenu::Impl::handleCreateNull()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactNullLayerInitParams params(uniqueLayerName(u8"Null 1"));
    auto* service = ArtifactProjectService::instance();
    if (auto comp = service->currentComposition().lock()) {
        auto size = comp->settings().compositionSize();
        params.setWidth(size.width());
        params.setHeight(size.height());
    }
    service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
    if (menu_) {
        Q_EMIT menu_->nullLayerCreated();
    }
}

void ArtifactLayerMenu::Impl::handleCreateConstruction()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }

    ArtifactLayerInitParams params(uniqueLayerName(u8"Construction Layer 1"), LayerType::Construction);
    if (auto* service = ArtifactProjectService::instance()) {
        service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
    }
}

void ArtifactLayerMenu::Impl::handleCreateAdjust()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactLayerInitParams params(uniqueLayerName(u8"Adjustment Layer 1"), LayerType::Adjustment);
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
}

void ArtifactLayerMenu::Impl::handleCreateText()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactTextLayerInitParams params(uniqueLayerName(u8"Text 1"));
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
}

void ArtifactLayerMenu::Impl::handleCreateParticle()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactLayerInitParams params(uniqueLayerName(u8"Particle 1"), LayerType::Particle);
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
}

void ArtifactLayerMenu::Impl::handleCreateCamera()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }

    auto* const service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }

    QWidget* parentWindow = mainWindow_ ? mainWindow_ : (menu_ ? menu_->window() : nullptr);
    CreateCameraLayerDialog dialog(parentWindow);
    dialog.setModal(true);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    ArtifactCameraLayerInitParams params;
    params.setName(UniString(dialog.cameraName().trimmed().isEmpty()
                                 ? uniqueLayerName(u8"Camera 1")
                                 : dialog.cameraName()));

    service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());

    auto* app = ArtifactApplicationManager::instance();
    auto* selectionManager = app ? app->layerSelectionManager() : nullptr;
    const auto camera = selectionManager
        ? std::dynamic_pointer_cast<ArtifactCameraLayer>(selectionManager->currentLayer())
        : nullptr;
    if (!camera) {
        return;
    }

    camera->setZoom(dialog.zoom());
    camera->setFocusDistance(dialog.focusDistance());
    camera->setAperture(dialog.apertureF());
    camera->setDepthOfField(dialog.depthOfFieldEnabled());
    camera->setMotionBlur(dialog.motionBlur());
    camera->setBlurAmount(dialog.blurAmount());
    camera->setUseManualFov(true);
    camera->setFov(dialog.fov());
    camera->setLocked(dialog.cameraLocked());
}

void ArtifactLayerMenu::Impl::handleCreateLight()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer",
                             "コンポジションが選択されていません。");
        return;
    }

    auto* const service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }

    ArtifactLayerInitParams params(uniqueLayerName(QStringLiteral("Light 1")),
                                   LayerType::Light);
    service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
}

void ArtifactLayerMenu::Impl::handleCreateAudio()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        menu_ ? menu_->window() : nullptr,
        QStringLiteral("オーディオを選択"),
        QString(),
        QStringLiteral("WAV Audio (*.wav);;All Files (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    ArtifactAudioInitParams params(uniqueLayerName(QFileInfo(path).baseName()));
    params.setAudioPath(path);
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
}

void ArtifactLayerMenu::Impl::handleCreateSvg()
{
    auto* service = ArtifactProjectService::instance();
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    if (!service) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        menu_ ? menu_->window() : nullptr,
        QStringLiteral("SVGを選択"),
        QString(),
        QStringLiteral("SVG (*.svg);;All Files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }
    if (!filePath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr,
                             QStringLiteral("Layer"),
                             QStringLiteral("SVG ファイルを選択してください。"));
        return;
    }
    QSvgRenderer validator(filePath);
    if (!validator.isValid()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr,
                             QStringLiteral("Layer"),
                             QStringLiteral("SVG を読み込めませんでした。"));
        return;
    }

    const QString layerName = uniqueLayerName(QFileInfo(filePath).completeBaseName());
    service->importAssetsFromPathsAsync(QStringList{filePath}, [service, layerName](QStringList importedPaths) {
        if (!service || importedPaths.isEmpty()) {
            return;
        }
        ArtifactSvgInitParams params(layerName);
        params.setSvgPath(importedPaths.first());
    service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
    });
}

void ArtifactLayerMenu::Impl::handleCreateModel3D()
{
    auto* service = ArtifactProjectService::instance();
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    if (!service) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        menu_ ? menu_->window() : nullptr,
        QStringLiteral("3Dモデルを選択"),
        QString(),
        QStringLiteral("3D Models (*.obj *.fbx *.gltf *.glb *.stl *.dae *.abc *.usd *.usdz *.pmd *.pmx);;All Files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }

    const QString layerName = uniqueLayerName(QFileInfo(filePath).completeBaseName());
    service->importAssetsFromPathsAsync(QStringList{filePath}, [service, layerName](QStringList importedPaths) {
        if (!service || importedPaths.isEmpty()) {
            return;
        }
        ArtifactModel3DLayerInitParams params(layerName);
        params.setModelPath(importedPaths.first());
    service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
    });
}

void ArtifactLayerMenu::Impl::handleCreatePlane3D()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    ArtifactFixedGeometry3DLayerInitParams params(uniqueLayerName(QStringLiteral("3D Plane 1")),
                                                  FixedGeometry3D::Plane);
    service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
}

void ArtifactLayerMenu::Impl::handleCycleLayerCreation(bool reverse)
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }

    enum class Preset { Solid, Null, Construction, Adjust, Text, Particle, Camera, Light };
    static int lastIndex = -1;
    static std::chrono::steady_clock::time_point lastAt{};
    const auto now = std::chrono::steady_clock::now();
    const bool armed = lastAt.time_since_epoch().count() > 0 &&
                       (now - lastAt) <= std::chrono::seconds(4);
    constexpr int count = 8;
    if (!armed) {
        lastIndex = reverse ? count - 1 : 0;
    } else {
        lastIndex = reverse ? (lastIndex - 1 + count) % count : (lastIndex + 1) % count;
    }
    lastAt = now;

    switch (static_cast<Preset>(lastIndex)) {
    case Preset::Solid: {
        ArtifactSolidLayerInitParams params(uniqueLayerName(QStringLiteral("Solid 1")));
        params.setColor(FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
        service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
        break;
    }
    case Preset::Null: {
        ArtifactNullLayerInitParams params(uniqueLayerName(QStringLiteral("Null 1")));
        service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
        break;
    }
    case Preset::Construction: {
        ArtifactLayerInitParams params(uniqueLayerName(QStringLiteral("Construction 1")),
                                      LayerType::Construction);
        service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
        break;
    }
    case Preset::Adjust: {
        ArtifactLayerInitParams params(uniqueLayerName(QStringLiteral("Adjustment Layer 1")),
                                      LayerType::Adjustment);
        service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
        break;
    }
    case Preset::Text: {
        ArtifactTextLayerInitParams params(uniqueLayerName(QStringLiteral("Text 1")));
        service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
        break;
    }
    case Preset::Particle: {
        ArtifactLayerInitParams params(uniqueLayerName(QStringLiteral("Particle 1")),
                                      LayerType::Particle);
        service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
        break;
    }
    case Preset::Camera: {
        ArtifactCameraLayerInitParams params;
        service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
        break;
    }
    case Preset::Light: {
        ArtifactLayerInitParams params(uniqueLayerName(QStringLiteral("Light 1")),
                                      LayerType::Light);
        service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
        break;
    }
    }
}

void ArtifactLayerMenu::Impl::handleCycleShapeCreation(bool reverse)
{
    static int lastIndex = -1;
    static std::chrono::steady_clock::time_point lastAt{};
    const auto now = std::chrono::steady_clock::now();
    const bool armed = lastAt.time_since_epoch().count() > 0 &&
                       (now - lastAt) <= std::chrono::seconds(4);
    constexpr int count = 6;
    if (!armed) {
        lastIndex = reverse ? count - 1 : 0;
    } else {
        lastIndex = reverse ? (lastIndex - 1 + count) % count : (lastIndex + 1) % count;
    }
    lastAt = now;

    switch (lastIndex) {
    case 0: handleCreateShape(ShapeType::Rect, QStringLiteral("Shape 1")); return;
    case 1: handleCreateShape(ShapeType::Ellipse, QStringLiteral("Ellipse 1")); return;
    case 2: handleCreateShape(ShapeType::Triangle, QStringLiteral("Triangle 1")); return;
    case 3: handleCreateShape(ShapeType::Square, QStringLiteral("Square 1")); return;
    case 4: handleCreateShape(ShapeType::Polygon, QStringLiteral("Polygon 1")); return;
    case 5: handleCreateShape(ShapeType::Star, QStringLiteral("Star 1")); return;
    }
}

void ArtifactLayerMenu::Impl::handleCreateShape(ShapeType type, const QString& nameBase)
{
    auto* service = ArtifactProjectService::instance();
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    if (!service) {
        return;
    }

    ArtifactLayerInitParams params(uniqueLayerName(nameBase), LayerType::Shape);
        service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());

    if (auto* app = ArtifactApplicationManager::instance()) {
        if (auto* selectionManager = app->layerSelectionManager()) {
            if (auto current = selectionManager->currentLayer()) {
                if (auto shapeLayer = std::dynamic_pointer_cast<ArtifactShapeLayer>(current)) {
                    shapeLayer->setShapeType(type);
                }
            }
        }
    }
}

void ArtifactLayerMenu::Impl::handleDuplicateLayer()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    if (!service->duplicateLayerInCurrentComposition(selectedLayerId_)) {
        QMessageBox::warning(menu_->window(), "Layer", "レイヤー複製に失敗しました。");
    }
}

void ArtifactLayerMenu::Impl::handleRenameLayer()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    const QString layerName = service->layerNameInCurrentComposition(selectedLayerId_);

    bool ok = false;
    const QString newName = QInputDialog::getText(
        menu_->window(),
        "レイヤー名の変更",
        "新しい名前:",
        QLineEdit::Normal,
        layerName,
        &ok);
    if (!ok) return;
    if (!service->renameLayerInCurrentComposition(selectedLayerId_, newName)) {
        QMessageBox::warning(menu_->window(), "Layer", "レイヤー名の変更に失敗しました。");
    }
}

void ArtifactLayerMenu::Impl::handleDeleteLayer()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    auto comp = service->currentComposition().lock();
    if (!comp) return;
    const QString message = service->layerRemovalConfirmationMessage(comp->id(), selectedLayerId_);
    if (!ArtifactMessageBox::confirmDelete(menu_->window(), QStringLiteral("レイヤー削除"), message)) {
        return;
    }
    if (!service->removeLayerFromComposition(comp->id(), selectedLayerId_)) {
        QMessageBox::warning(menu_->window(), QStringLiteral("削除失敗"), QStringLiteral("レイヤー削除に失敗しました。"));
    }
}

void ArtifactLayerMenu::Impl::handleToggleVisible()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    const bool current = service->isLayerVisibleInCurrentComposition(selectedLayerId_);
    service->setLayerVisibleInCurrentComposition(selectedLayerId_, !current);
}

void ArtifactLayerMenu::Impl::handleToggleLock()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    const bool current = service->isLayerLockedInCurrentComposition(selectedLayerId_);
    service->setLayerLockedInCurrentComposition(selectedLayerId_, !current);
}

void ArtifactLayerMenu::Impl::handleToggleSolo()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    const bool current = service->isLayerSoloInCurrentComposition(selectedLayerId_);
    service->setLayerSoloInCurrentComposition(selectedLayerId_, !current);
}

void ArtifactLayerMenu::Impl::handleToggleShy()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    const bool current = service->isLayerShyInCurrentComposition(selectedLayerId_);
    service->setLayerShyInCurrentComposition(selectedLayerId_, !current);
}

void ArtifactLayerMenu::Impl::handleSoloOnlySelected()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    service->smartSoloOnlyLayerInCurrentComposition(selectedLayerId_);
}

void ArtifactLayerMenu::Impl::handleSetProxyQuality(ProxyQuality quality)
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) {
        return;
    }
    auto comp = service->currentComposition().lock();
    if (!comp) {
        return;
    }
    auto layer = comp->layerById(selectedLayerId_);
    auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
    if (!videoLayer || videoLayer->proxyQuality() == quality) {
        return;
    }
    videoLayer->setLayerPropertyValue(QStringLiteral("video.proxyQuality"),
                                      QVariant::fromValue(static_cast<int>(quality)));
    videoLayer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{comp->id().toString(), videoLayer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
}

void ArtifactLayerMenu::Impl::handleGenerateProxy()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) {
        return;
    }
    auto comp = service->currentComposition().lock();
    if (!comp) {
        return;
    }
    auto layer = comp->layerById(selectedLayerId_);
    auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
    if (!videoLayer) {
        return;
    }
    const QString sourcePath = videoLayer->sourcePath().trimmed();
    if (sourcePath.isEmpty()) {
        QMessageBox::information(menu_->window(), QStringLiteral("Proxy"), QStringLiteral("Source file が見つかりません。"));
        return;
    }

    auto* window = qobject_cast<QMainWindow*>(mainWindow_);
    if (!window) {
        return;
    }
    setDockVisible(window, QStringLiteral("Project"), true);
    activateDock(window, QStringLiteral("Project"));

    auto* projectDock = window->findChild<ArtifactProjectManagerWidget*>(QStringLiteral("artifactProjectManagerWidget"));
    if (!projectDock) {
        const auto docks = window->findChildren<ArtifactProjectManagerWidget*>();
        if (!docks.isEmpty()) {
            projectDock = docks.first();
        }
    }
    if (!projectDock) {
        QMessageBox::information(menu_->window(), QStringLiteral("Proxy"), QStringLiteral("Project dock が見つかりません。"));
        return;
    }

    const auto found = projectDock->selectItemsByFilePaths(QStringList{sourcePath});
    if (!found) {
        QMessageBox::information(menu_->window(), QStringLiteral("Proxy"), QStringLiteral("Source file を Project で選択できませんでした。"));
        return;
    }
    projectDock->generateProxyForSelection();
}

void ArtifactLayerMenu::Impl::handleRevealProxy()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) {
        return;
    }
    auto comp = service->currentComposition().lock();
    if (!comp) {
        return;
    }
    auto layer = comp->layerById(selectedLayerId_);
    auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
    if (!videoLayer) {
        return;
    }
    const QString proxyPath = videoLayer->proxyPath();
    if (proxyPath.isEmpty() || !QFileInfo::exists(proxyPath)) {
        QMessageBox::information(menu_->window(), QStringLiteral("Proxy"), QStringLiteral("表示できるプロキシがありません。"));
        return;
    }
    const QString folder = QFileInfo(proxyPath).absolutePath();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folder))) {
        QMessageBox::warning(menu_->window(), QStringLiteral("Proxy"), QStringLiteral("プロキシフォルダを開けませんでした。"));
    }
}

void ArtifactLayerMenu::Impl::handleClearProxy()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) {
        return;
    }
    auto comp = service->currentComposition().lock();
    if (!comp) {
        return;
    }
    auto layer = comp->layerById(selectedLayerId_);
    auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
    if (!videoLayer) {
        return;
    }
    const QString proxyPath = videoLayer->proxyPath();
    if (proxyPath.isEmpty()) {
        return;
    }
    auto* window = qobject_cast<QMainWindow*>(mainWindow_);
    auto* manager = window ? window->findChild<ArtifactProjectManagerWidget*>(QStringLiteral("artifactProjectManagerWidget")) : nullptr;
    if (manager) {
        if (!manager->clearProxyForFilePath(videoLayer->sourcePath())) {
            QMessageBox::warning(menu_->window(), QStringLiteral("Proxy"), QStringLiteral("プロキシファイルを削除できませんでした。"));
        }
        return;
    }
    if (QFileInfo::exists(proxyPath) && !QFile::remove(proxyPath)) {
        QMessageBox::warning(menu_->window(), QStringLiteral("Proxy"), QStringLiteral("プロキシファイルを削除できませんでした。"));
        return;
    }
    videoLayer->clearProxy();
    videoLayer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{comp->id().toString(), videoLayer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});
}

QStringList ArtifactLayerMenu::Impl::selectedVideoSourcePathsInCurrentComposition() const
{
    QStringList sourcePaths;
    auto* app = ArtifactApplicationManager::instance();
    auto* selectionManager = app ? app->layerSelectionManager() : nullptr;
    auto* service = ArtifactProjectService::instance();
    if (!selectionManager || !service) {
        return sourcePaths;
    }

    auto comp = service->currentComposition().lock();
    if (!comp) {
        return sourcePaths;
    }

    const auto selected = selectionManager->selectedLayers();
    auto isSelected = [&selected](const LayerID& id) {
        for (const auto& layer : selected) {
            if (layer && layer->id() == id) {
                return true;
            }
        }
        return false;
    };

    QSet<QString> seen;
    for (const auto& layer : comp->allLayer()) {
        if (!layer || !isSelected(layer->id())) {
            continue;
        }
        auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer);
        if (!videoLayer) {
            continue;
        }
        const QString sourcePath = videoLayer->sourcePath().trimmed();
        if (sourcePath.isEmpty() || seen.contains(sourcePath)) {
            continue;
        }
        seen.insert(sourcePath);
        sourcePaths.push_back(sourcePath);
    }

    return sourcePaths;
}

void ArtifactLayerMenu::Impl::handleGenerateSelectedProxies()
{
    const QStringList sourcePaths = selectedVideoSourcePathsInCurrentComposition();
    if (sourcePaths.size() <= 1) {
        QMessageBox::information(menu_->window(), QStringLiteral("Proxy"), QStringLiteral("複数の動画レイヤーを選択してください。"));
        return;
    }

    auto* window = qobject_cast<QMainWindow*>(mainWindow_);
    if (!window) {
        return;
    }
    setDockVisible(window, QStringLiteral("Project"), true);
    activateDock(window, QStringLiteral("Project"));

    auto* projectDock = window->findChild<ArtifactProjectManagerWidget*>(QStringLiteral("artifactProjectManagerWidget"));
    if (!projectDock) {
        const auto docks = window->findChildren<ArtifactProjectManagerWidget*>();
        if (!docks.isEmpty()) {
            projectDock = docks.first();
        }
    }
    if (!projectDock) {
        QMessageBox::information(menu_->window(), QStringLiteral("Proxy"), QStringLiteral("Project dock が見つかりません。"));
        return;
    }

    for (const QString& sourcePath : sourcePaths) {
        projectDock->generateProxyForFilePath(sourcePath);
    }
}

void ArtifactLayerMenu::Impl::handleClearSelectedProxies()
{
    const QStringList sourcePaths = selectedVideoSourcePathsInCurrentComposition();
    if (sourcePaths.size() <= 1) {
        QMessageBox::information(menu_->window(), QStringLiteral("Proxy"), QStringLiteral("複数の動画レイヤーを選択してください。"));
        return;
    }

    auto* window = qobject_cast<QMainWindow*>(mainWindow_);
    if (!window) {
        return;
    }
    auto* projectDock = window->findChild<ArtifactProjectManagerWidget*>(QStringLiteral("artifactProjectManagerWidget"));
    if (!projectDock) {
        const auto docks = window->findChildren<ArtifactProjectManagerWidget*>();
        if (!docks.isEmpty()) {
            projectDock = docks.first();
        }
    }
    if (!projectDock) {
        QMessageBox::information(menu_->window(), QStringLiteral("Proxy"), QStringLiteral("Project dock が見つかりません。"));
        return;
    }

    bool anyCleared = false;
    for (const QString& sourcePath : sourcePaths) {
        anyCleared = projectDock->clearProxyForFilePath(sourcePath) || anyCleared;
    }
    if (!anyCleared) {
        QMessageBox::information(menu_->window(), QStringLiteral("Proxy"), QStringLiteral("削除できるプロキシがありませんでした。"));
    }
}

void ArtifactLayerMenu::Impl::handleSaveMaskPreset()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) {
        return;
    }

    auto comp = service->currentComposition().lock();
    if (!comp) {
        return;
    }
    auto layer = comp->layerById(selectedLayerId_);
    if (!layer || !layer->hasMasks()) {
        QMessageBox::information(menu_->window(), QStringLiteral("Mask Preset"),
                                 QStringLiteral("保存するマスクがありません。"));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        menu_ ? menu_->window() : nullptr,
        QStringLiteral("マスクプリセットを保存"),
        QString(),
        QStringLiteral("Mask Preset (*.mask.json *.json);;All Files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }
    QString resolvedPath = filePath;
    if (!resolvedPath.endsWith(QStringLiteral(".mask.json"), Qt::CaseInsensitive) &&
        !resolvedPath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        resolvedPath += QStringLiteral(".mask.json");
    }

    LayerMask mask;
    for (int i = 0; i < layer->maskCount(); ++i) {
        const LayerMask sourceMask = layer->mask(i);
        for (int p = 0; p < sourceMask.maskPathCount(); ++p) {
            mask.addMaskPath(sourceMask.maskPath(p));
        }
    }

    if (!ArtifactPresetManager::saveMaskPreset(mask, resolvedPath)) {
        QMessageBox::warning(menu_->window(), QStringLiteral("Mask Preset"),
                             QStringLiteral("マスクプリセットを保存できませんでした。"));
    }
}

void ArtifactLayerMenu::Impl::handleLoadMaskPreset()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) {
        return;
    }

    auto comp = service->currentComposition().lock();
    if (!comp) {
        return;
    }
    auto layer = comp->layerById(selectedLayerId_);
    if (!layer) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        menu_ ? menu_->window() : nullptr,
        QStringLiteral("マスクプリセットを適用"),
        QString(),
        QStringLiteral("Mask Preset (*.mask.json *.json);;All Files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }

    LayerMask mask;
    if (!ArtifactPresetManager::loadMaskPreset(mask, filePath)) {
        QMessageBox::warning(menu_->window(), QStringLiteral("Mask Preset"),
                             QStringLiteral("マスクプリセットを読み込めませんでした。"));
        return;
    }

    layer->clearMasks();
    layer->addMask(mask);
    layer->changed();
}

void ArtifactLayerMenu::Impl::handleSelectParent()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    const auto parentId = service->layerParentIdInCurrentComposition(selectedLayerId_);
    if (parentId.isNil()) return;
    service->selectLayer(parentId);
}

void ArtifactLayerMenu::Impl::handleClearParent()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    service->clearLayerParentInCurrentComposition(selectedLayerId_);
}

void ArtifactLayerMenu::Impl::handleOpenInspector()
{
    auto* window = qobject_cast<QMainWindow*>(mainWindow_);
    if (!window) {
        return;
    }
    setDockVisible(window, QStringLiteral("Inspector"), true);
    activateDock(window, QStringLiteral("Inspector"));
}

void ArtifactLayerMenu::Impl::handleOpenProperties()
{
    auto* window = qobject_cast<QMainWindow*>(mainWindow_);
    if (!window) {
        return;
    }
    setDockVisible(window, QStringLiteral("Properties"), true);
    activateDock(window, QStringLiteral("Properties"));
}

void ArtifactLayerMenu::Impl::handlePrecompose()
{
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    auto comp = service->currentComposition().lock();
    if (!comp) {
        QMessageBox::warning(menu_->window(), "プリコンポーズ", "コンポジションが選択されていません。");
        return;
    }

    auto* app = ArtifactApplicationManager::instance();
    auto* selectionManager = app ? app->layerSelectionManager() : nullptr;
    if (!selectionManager) {
        QMessageBox::warning(menu_->window(), "プリコンポーズ", "選択レイヤーを取得できませんでした。");
        return;
    }

    const auto selected = selectionManager->selectedLayers();
    QVector<LayerID> selectedIds;
    QStringList selectedNames;
    selectedIds.reserve(selected.size());
    selectedNames.reserve(selected.size());
    auto isSelected = [&selected](const LayerID& id) {
        for (const auto& layer : selected) {
            if (layer && layer->id() == id) {
                return true;
            }
        }
        return false;
    };
    for (const auto& layer : comp->allLayer()) {
        if (!layer || !isSelected(layer->id())) {
            continue;
        }
        selectedIds.push_back(layer->id());
        selectedNames.push_back(layer->layerName());
    }

    if (selectedIds.isEmpty()) {
        QMessageBox::warning(menu_->window(), "プリコンポーズ", "選択レイヤーがありません。");
        return;
    }

    PrecomposeDialog dialog(menu_->window());
    dialog.setSelectedLayerNames(selectedNames);
    dialog.setTotalLayerCount(comp->layerCount());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const PrecomposeMode mode = dialog.moveSelectedOnly()
                                    ? PrecomposeMode::MoveSelected
                                    : PrecomposeMode::MoveAllAttributes;
    if (!service->precomposeLayersWithUndo(
            selectedIds, UniString(dialog.newCompositionName()),
            dialog.openNewComposition(), dialog.matchWorkspaceDuration(), mode)) {
        QMessageBox::warning(menu_->window(), "プリコンポーズ", "プリコンポーズに失敗しました。");
    }
}

void ArtifactLayerMenu::Impl::handleUnprecompose()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) {
        return;
    }

    auto comp = service->currentComposition().lock();
    auto compLayer = comp ? std::dynamic_pointer_cast<ArtifactCompositionLayer>(
                                comp->layerById(selectedLayerId_))
                          : nullptr;
    if (!compLayer || compLayer->sourceCompositionId().isNil()) {
        return;
    }

    const auto reply = QMessageBox::question(
        menu_->window(),
        QStringLiteral("プリコンポーズを解除"),
        QStringLiteral("選択したプリコンポーズを解除しますか？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    if (!service->unprecomposeLayerWithUndo(selectedLayerId_, true)) {
        QMessageBox::warning(menu_->window(), QStringLiteral("プリコンポーズを解除"),
                             QStringLiteral("プリコンポーズの解除に失敗しました。"));
    }
}

void ArtifactLayerMenu::Impl::handleGroupSelection()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || !hasCurrentComposition()) {
        return;
    }

    bool ok = false;
    const QString groupName = QInputDialog::getText(
        menu_->window(),
        "グループ化",
        "グループ名:",
        QLineEdit::Normal,
        QStringLiteral("Group 1"),
        &ok);
    if (!ok) {
        return;
    }

    if (!service->groupSelectedLayersInCurrentComposition(UniString(groupName))) {
        QMessageBox::warning(menu_->window(), "グループ化", "選択レイヤーをグループ化できませんでした。");
    }
}

void ArtifactLayerMenu::Impl::handleUngroup()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || !hasCurrentComposition()) {
        return;
    }

    if (!service->ungroupSelectedGroupInCurrentComposition()) {
        QMessageBox::warning(menu_->window(), "グループ解除", "グループを解除できませんでした。グループを選択してください。");
    }
}

void ArtifactLayerMenu::Impl::handleSplitLayer()
{
    if (selectedLayerId_.isNil()) {
        QMessageBox::warning(menu_->window(), QStringLiteral("Layer"),
                             QStringLiteral("分割するレイヤーが選択されていません。"));
        return;
    }
    if (auto* ctx = ArtifactApplicationManager::instance()
                        ? ArtifactApplicationManager::instance()->activeContextService()
                        : nullptr) {
        ctx->splitLayerAtCurrentTime();
        refreshEnabledState();
        return;
    }
    QMessageBox::warning(menu_->window(), QStringLiteral("Layer"),
                         QStringLiteral("アクティブコンテキストが利用できません。"));
}

void ArtifactLayerMenu::Impl::handleTrackCamera()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;

    auto comp = service->currentComposition().lock();
    if (!comp) return;

    auto layer = comp->layerById(selectedLayerId_);
    if (!layer || !layer->hasVideo()) {
        QMessageBox::warning(menu_->window(), "3D Tracker", "動画レイヤーを選択してください。");
        return;
    }

    // 実行
    auto* window = qobject_cast<QMainWindow*>(menu_ ? menu_->window() : nullptr);
    bool success = ArtifactCameraTrackerTool::run(
        comp.get(), layer,
        [window](const ArtifactCameraTrackerTool::ProgressUpdate& update) {
            qDebug() << "Tracking progress:" << update.currentFrame << "/" << update.totalFrames
                     << update.message;
            if (!window) {
                return;
            }
            const QString status = QStringLiteral("Tracking %1/%2: %3")
                                       .arg(update.currentFrame)
                                       .arg(update.totalFrames)
                                       .arg(update.message);
            QMetaObject::invokeMethod(
                window,
                [window, status]() {
                    if (window) {
                        window->statusBar()->showMessage(status, 1500);
                    }
                },
                Qt::QueuedConnection);
        });

    if (success) {
        QMessageBox::information(menu_->window(), "3D Tracker", "トラッキングが完了しました。カメラと特徴点レイヤーが作成されました。");
    } else {
        QMessageBox::warning(menu_->window(), "3D Tracker", "トラッキングに失敗しました。十分な特徴点が見つからなかった可能性があります。");
    }
}

void ArtifactLayerMenu::Impl::handleCreateMotionTracker()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) {
        return;
    }

    auto comp = service->currentComposition().lock();
    if (!comp) {
        return;
    }

    auto layer = comp->layerById(selectedLayerId_);
    auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
    if (!videoLayer || !videoLayer->hasVideo()) {
        QMessageBox::warning(menu_->window(), "Motion Tracker", "動画レイヤーを選択してください。");
        return;
    }

    const int existingTrackerId = videoLayer->motionTrackerId();
    if (existingTrackerId > 0 && ArtifactCore::TrackerManager::instance().tracker(existingTrackerId)) {
        QMessageBox::information(menu_->window(), "Motion Tracker",
                                 QStringLiteral("このレイヤーには既存のトラッカー #%1 が紐づいています。")
                                     .arg(existingTrackerId));
        return;
    }

    const QString baseName = videoLayer->layerName().trimmed().isEmpty()
                                 ? QStringLiteral("Motion Tracker")
                                 : QStringLiteral("%1 Tracker").arg(videoLayer->layerName().trimmed());
    auto* tracker = ArtifactCore::TrackerManager::instance().createTracker(baseName);
    if (!tracker) {
        QMessageBox::warning(menu_->window(), "Motion Tracker", "トラッカーを作成できませんでした。");
        return;
    }

    videoLayer->setMotionTrackerId(tracker->id());
    videoLayer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
        LayerChangedEvent{comp->id().toString(), videoLayer->id().toString(),
                          LayerChangedEvent::ChangeType::Modified});

    QMessageBox::information(menu_->window(), "Motion Tracker",
                             QStringLiteral("トラッカー #%1 を作成してレイヤーに紐づけました。")
                                 .arg(tracker->id()));
}

ArtifactLayerMenu::ArtifactLayerMenu(QWidget* mainWindow, QWidget* parent)
    : QMenu(parent), impl_(new Impl(this))
{
    impl_->mainWindow_ = mainWindow ? mainWindow->window() : nullptr;
    setTitle("レイヤー(&L)");
}

ArtifactLayerMenu::~ArtifactLayerMenu()
{
    delete impl_;
}

QMenu* ArtifactLayerMenu::newLayerMenu() const
{
    return impl_->createMenu;
}

} // namespace Artifact


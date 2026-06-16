module;
#include <utility>
#include <QAction>
#include <QActionGroup>
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
#include <QProgressDialog>
#include <wobjectimpl.h>
#include <atomic>
#include <memory>

module Artifact.Menu.Layer;
import std;

import Event.Bus;
import Artifact.Event.Types;
import Artifact.Service.Project;
import Artifact.Service.ActiveContext;
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
import Artifact.Widgets.ProjectManagerWidget;
import Artifact.Composition.Abstract;
import Artifact.Widgets.PrecomposeDialog;
import Artifact.Widgets.CreatePlaneLayerDialog;
import Artifact.Widgets.CreateCameraLayerDialog;
import Artifact.Widgets.AppDialogs;
import Artifact.Tool.CameraTracker;
import Tracking.MotionTracker;
import Artifact.Tool.PointTracker;

namespace Artifact {
using namespace ArtifactCore;

namespace {

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
    QMenu* switchMenu = nullptr;
    QMenu* selectMenu = nullptr;
    QMenu* proxyMenu = nullptr;
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
    QAction* createShapeRectAction = nullptr;
    QAction* createShapeSquareAction = nullptr;
    QAction* createShapePolygonAction = nullptr;
    QAction* createShapeTriangleAction = nullptr;
    QAction* createShapeEllipseAction = nullptr;
    QAction* createShapeStarAction = nullptr;
    QAction* trackCameraAction = nullptr;
    QAction* createMotionTrackerAction = nullptr;
    QAction* analyzeForwardAction = nullptr;
    QAction* analyzeBackwardAction = nullptr;
    QAction* analyzeAllAction = nullptr;
    QAction* applyTrackingToNullAction = nullptr;

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

    QAction* selectParentAction = nullptr;
    QAction* clearParentAction = nullptr;
    QAction* openInspectorAction = nullptr;
    QAction* openPropertiesAction = nullptr;

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
    void handleCreateShape(ShapeType type, const QString& nameBase);
    void handleCreateMotionTracker();
    void handleAnalyzeForward();
    void handleAnalyzeBackward();
    void handleAnalyzeAll();
    void handleApplyTrackingToNull();

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

    createShapeMenu = new QMenu("シェイプ(&S)", createMenu);
    createShapeMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_shape_rect.svg")));
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

    trackCameraAction = new QAction("3Dカメラトラッキング(&T)", menu);
    trackCameraAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_videocam.svg")));
    createMotionTrackerAction = new QAction("モーショントラッカーを作成(&M)", menu);
    analyzeForwardAction = new QAction("前方にトラッキング分析(&F)", menu);
    analyzeBackwardAction = new QAction("後方にトラッキング分析(&B)", menu);
    analyzeAllAction = new QAction("全フレームトラッキング分析(&A)", menu);
    applyTrackingToNullAction = new QAction("トラッキング結果をヌルレイヤーに適用(&P)", menu);

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
    for (auto *action : {proxyNoneAction, proxyQuarterAction, proxyHalfAction, proxyFullAction}) {
        action->setCheckable(true);
        proxyQualityGroup->addAction(action);
    }

    openInspectorAction = new QAction("Inspector を開く", menu);
    openInspectorAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_inspector.svg")));
    openPropertiesAction = new QAction("Properties を開く", menu);
    openPropertiesAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_settings.svg")));

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
    menu->addAction(analyzeForwardAction);
    menu->addAction(analyzeBackwardAction);
    menu->addAction(analyzeAllAction);
    menu->addAction(applyTrackingToNullAction);
    menu->addSeparator();
    menu->addAction(duplicateLayerAction);
    menu->addAction(renameLayerAction);
    menu->addAction(deleteLayerAction);
    menu->addSeparator();
    menu->addMenu(switchMenu);
    menu->addMenu(selectMenu);
    menu->addMenu(proxyMenu);
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
        if (action == createShapeRectAction) { handleCreateShape(ShapeType::Rect, QStringLiteral("Shape 1")); return; }
        if (action == createShapeSquareAction) { handleCreateShape(ShapeType::Square, QStringLiteral("Square 1")); return; }
        if (action == createShapePolygonAction) { handleCreateShape(ShapeType::Polygon, QStringLiteral("Polygon 1")); return; }
        if (action == createShapeTriangleAction) { handleCreateShape(ShapeType::Triangle, QStringLiteral("Triangle 1")); return; }
        if (action == createShapeEllipseAction) { handleCreateShape(ShapeType::Ellipse, QStringLiteral("Ellipse 1")); return; }
        if (action == createShapeStarAction) { handleCreateShape(ShapeType::Star, QStringLiteral("Star 1")); return; }
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
        if (action == openInspectorAction) { handleOpenInspector(); return; }
        if (action == openPropertiesAction) { handleOpenProperties(); return; }
        if (action == precomposeAction) { handlePrecompose(); return; }
        if (action == unprecomposeAction) { handleUnprecompose(); return; }
        if (action == groupSelectionAction) { handleGroupSelection(); return; }
        if (action == ungroupAction) { handleUngroup(); return; }
        if (action == splitAction) { handleSplitLayer(); return; }
        if (action == trackCameraAction) { handleTrackCamera(); return; }
        if (action == createMotionTrackerAction) { handleCreateMotionTracker(); return; }
        if (action == analyzeForwardAction) { handleAnalyzeForward(); return; }
        if (action == analyzeBackwardAction) { handleAnalyzeBackward(); return; }
        if (action == analyzeAllAction) { handleAnalyzeAll(); return; }
        if (action == applyTrackingToNullAction) { handleApplyTrackingToNull(); return; }
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

    // トラッキング分析アクション: 選択レイヤーが tracker を持つ場合のみ有効
    {
        bool hasTracker = false;
        if (isVideoSelected) {
            auto* selectionManager = app ? app->layerSelectionManager() : nullptr;
            if (selectionManager) {
                const auto selected = selectionManager->selectedLayers();
                for (const auto& layer : selected) {
                    auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer);
                    if (videoLayer && videoLayer->motionTrackerId() > 0) {
                        hasTracker = true;
                        break;
                    }
                }
            }
        }
        analyzeForwardAction->setVisible(hasTracker);
        analyzeBackwardAction->setVisible(hasTracker);
        analyzeAllAction->setVisible(hasTracker);
        applyTrackingToNullAction->setVisible(hasTracker);
    }
    proxyMenu->setEnabled(isVideoSelected);
    generateProxyAction->setEnabled(isVideoSelected);
    revealProxyAction->setEnabled(isVideoSelected && hasProxy);
    clearProxyAction->setEnabled(isVideoSelected && hasProxy);
    generateSelectedProxyAction->setEnabled(selectedVideoCount > 1);
    clearSelectedProxyAction->setEnabled(selectedVideoCount > 1 && selectedVideoProxyCount > 0);
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
    CreateSolidLayerSettingDialog dialog(parentWindow);
    QObject::connect(&dialog, &CreateSolidLayerSettingDialog::submit, menu, [service, menu](const ArtifactSolidLayerInitParams& params) {
        if (!service) {
            return;
        }
        QTimer::singleShot(0, menu, [service, params, menu]() {
            Q_UNUSED(menu);
            service->addLayerToCurrentComposition(params);
        });
    });
    dialog.setModal(true);
    dialog.exec();
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
    service->addLayerToCurrentComposition(params);
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
        service->addLayerToCurrentComposition(params);
    }
}

void ArtifactLayerMenu::Impl::handleCreateAdjust()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactLayerInitParams params(uniqueLayerName(u8"Adjustment Layer 1"), LayerType::Adjustment);
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params);
}

void ArtifactLayerMenu::Impl::handleCreateText()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactTextLayerInitParams params(uniqueLayerName(u8"Text 1"));
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params);
}

void ArtifactLayerMenu::Impl::handleCreateParticle()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    ArtifactLayerInitParams params(uniqueLayerName(u8"Particle 1"), LayerType::Particle);
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params);
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

    service->addLayerToCurrentComposition(params);

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
    service->addLayerToCurrentComposition(params);
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
    ArtifactProjectService::instance()->addLayerToCurrentComposition(params);
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
        service->addLayerToCurrentComposition(params);
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
        service->addLayerToCurrentComposition(params);
    });
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
    service->addLayerToCurrentComposition(params);

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

void ArtifactLayerMenu::Impl::handleAnalyzeForward()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    auto comp = service->currentComposition().lock();
    if (!comp) return;

    auto layer = comp->layerById(selectedLayerId_);
    auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
    if (!videoLayer) return;

    const int trackerId = videoLayer->motionTrackerId();
    auto* tracker = ArtifactCore::TrackerManager::instance().tracker(trackerId);
    if (!tracker) {
        QMessageBox::warning(menu_->window(), "トラッキング分析", "トラッカーが見つかりません。");
        return;
    }

    // 現在フレームから末尾まで前方トラッキング
    const int64_t startFrame = videoLayer->currentFrame();
    const double fps = videoLayer->streamInfo().frameRate;
    if (fps <= 0.0) return;

    // 動画の総フレーム数を推定（streamInfo または decodeFrameToQImage で確認）
    const int64_t totalFrames = videoLayer->streamInfo().totalFrames;
    if (totalFrames <= 0 || startFrame >= totalFrames) {
        QMessageBox::information(menu_->window(), "トラッキング分析", "これ以上前方にフレームがありません。");
        return;
    }

    // 進捗ダイアログ
    const int rangeSize = static_cast<int>(totalFrames - startFrame);
    QProgressDialog progress("前方トラッキング分析中...", "キャンセル", 0, rangeSize, menu_->window());
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
    QObject::connect(&progress, &QProgressDialog::canceled, [cancelFlag]() { *cancelFlag = true; });

    // Preserve any user-tuned tracker settings and only switch the tracking
    // method required for this analysis pass.
    auto settings = tracker->settings();
    settings.method = ArtifactCore::TrackingMethod::NormalizedCrossCorrelation;
    tracker->setSettings(settings);

    // フレームを順次デコードして frameBuffer に蓄積
    for (int64_t f = startFrame; f < totalFrames; ++f) {
        if (*cancelFlag) break;

        const double time = static_cast<double>(f) / fps;
        QImage frameImg = videoLayer->decodeFrameToQImage(f);
        if (!frameImg.isNull()) {
            tracker->setFrame(time, frameImg);
        }
        progress.setValue(static_cast<int>(f - startFrame));
        if (progress.wasCanceled()) break;
    }

    // 前方トラッキング実行
    const double startTime = static_cast<double>(startFrame) / fps;
    const double endTime = static_cast<double>(totalFrames - 1) / fps;
    tracker->trackRange(startTime, endTime, [&progress, cancelFlag](double p) -> bool {
        progress.setValue(static_cast<int>(p * progress.maximum()));
        return !*cancelFlag;
    });

    progress.close();
    QMessageBox::information(menu_->window(), "トラッキング分析",
                             QStringLiteral("前方トラッキング完了 (フレーム %1 - %2)")
                                 .arg(startFrame).arg(totalFrames - 1));
}

void ArtifactLayerMenu::Impl::handleAnalyzeBackward()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    auto comp = service->currentComposition().lock();
    if (!comp) return;

    auto layer = comp->layerById(selectedLayerId_);
    auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
    if (!videoLayer) return;

    const int trackerId = videoLayer->motionTrackerId();
    auto* tracker = ArtifactCore::TrackerManager::instance().tracker(trackerId);
    if (!tracker) {
        QMessageBox::warning(menu_->window(), "トラッキング分析", "トラッカーが見つかりません。");
        return;
    }

    const int64_t currentFrameNum = videoLayer->currentFrame();
    const double fps = videoLayer->streamInfo().frameRate;
    if (fps <= 0.0) return;

    if (currentFrameNum <= 0) {
        QMessageBox::information(menu_->window(), "トラッキング分析", "これ以上後方にフレームがありません。");
        return;
    }

    const int rangeSize = static_cast<int>(currentFrameNum + 1);
    QProgressDialog progress("後方トラッキング分析中...", "キャンセル", 0, rangeSize, menu_->window());
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
    QObject::connect(&progress, &QProgressDialog::canceled, [cancelFlag]() { *cancelFlag = true; });

    // Preserve any user-tuned tracker settings and only switch the tracking
    // method required for this analysis pass.
    auto settings = tracker->settings();
    settings.method = ArtifactCore::TrackingMethod::NormalizedCrossCorrelation;
    tracker->setSettings(settings);

    // フレーム 0 から現在フレームまでを蓄積
    for (int64_t f = 0; f <= currentFrameNum; ++f) {
        if (*cancelFlag) break;
        const double time = static_cast<double>(f) / fps;
        QImage frameImg = videoLayer->decodeFrameToQImage(f);
        if (!frameImg.isNull()) {
            tracker->setFrame(time, frameImg);
        }
        progress.setValue(static_cast<int>(f));
        if (progress.wasCanceled()) break;
    }

    // 後方トラッキング実行
    const double fromTime = static_cast<double>(currentFrameNum) / fps;
    const double toTime = 0.0;
    tracker->trackBackward(fromTime, toTime);

    progress.close();
    QMessageBox::information(menu_->window(), "トラッキング分析",
                             QStringLiteral("後方トラッキング完了 (フレーム 0 - %1)")
                                 .arg(currentFrameNum));
}

void ArtifactLayerMenu::Impl::handleAnalyzeAll()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    auto comp = service->currentComposition().lock();
    if (!comp) return;

    auto layer = comp->layerById(selectedLayerId_);
    auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
    if (!videoLayer) return;

    const int trackerId = videoLayer->motionTrackerId();
    auto* tracker = ArtifactCore::TrackerManager::instance().tracker(trackerId);
    if (!tracker) {
        QMessageBox::warning(menu_->window(), "トラッキング分析", "トラッカーが見つかりません。");
        return;
    }

    const double fps = videoLayer->streamInfo().frameRate;
    if (fps <= 0.0) return;

    const int64_t totalFrames = videoLayer->streamInfo().totalFrames;
    if (totalFrames <= 0) {
        QMessageBox::warning(menu_->window(), "トラッキング分析", "動画フレームがありません。");
        return;
    }

    const int rangeSize = static_cast<int>(totalFrames);
    QProgressDialog progress("全フレームトラッキング分析中...", "キャンセル", 0, rangeSize, menu_->window());
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    auto cancelFlag = std::make_shared<std::atomic<bool>>(false);
    QObject::connect(&progress, &QProgressDialog::canceled, [cancelFlag]() { *cancelFlag = true; });

    // Preserve any user-tuned tracker settings and only switch the tracking
    // method required for this analysis pass.
    auto settings = tracker->settings();
    settings.method = ArtifactCore::TrackingMethod::NormalizedCrossCorrelation;
    tracker->setSettings(settings);

    // 全フレームを frameBuffer に蓄積
    for (int64_t f = 0; f < totalFrames; ++f) {
        if (*cancelFlag) break;
        const double time = static_cast<double>(f) / fps;
        QImage frameImg = videoLayer->decodeFrameToQImage(f);
        if (!frameImg.isNull()) {
            tracker->setFrame(time, frameImg);
        }
        progress.setValue(static_cast<int>(f));
        if (progress.wasCanceled()) break;
    }

    // トラッキング実行
    tracker->trackAll([&progress, cancelFlag](double p) -> bool {
        progress.setValue(static_cast<int>(p * progress.maximum()));
        return !*cancelFlag;
    });

    progress.close();
    QMessageBox::information(menu_->window(), "トラッキング分析",
                             QStringLiteral("全フレームトラッキング完了 (%1 フレーム)")
                                 .arg(totalFrames));
}

void ArtifactLayerMenu::Impl::handleApplyTrackingToNull()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    auto comp = service->currentComposition().lock();
    if (!comp) return;

    auto layer = comp->layerById(selectedLayerId_);
    auto videoLayer = layer ? std::dynamic_pointer_cast<ArtifactVideoLayer>(layer) : nullptr;
    if (!videoLayer) return;

    const int trackerId = videoLayer->motionTrackerId();
    auto* tracker = ArtifactCore::TrackerManager::instance().tracker(trackerId);
    if (!tracker || !tracker->hasResult()) {
        QMessageBox::warning(menu_->window(), "適用", "トラッキング結果がありません。先に分析を実行してください。");
        return;
    }

    ArtifactPointTrackerTool::ApplyOptions options;
    options.pointId = 0;
    options.createNullLayer = true;
    options.writeAnchor = true;

    const bool ok = ArtifactPointTrackerTool::applyTrackingResult(comp.get(), *tracker, options);
    if (ok) {
        QMessageBox::information(menu_->window(), "適用",
                                 "トラッキング結果をヌルレイヤーに適用しました。");
    } else {
        QMessageBox::warning(menu_->window(), "適用", "トラッキング結果の適用に失敗しました。");
    }
}

ArtifactLayerMenu::ArtifactLayerMenu(QWidget* mainWindow, QWidget* parent)
    : QMenu(parent), impl_(new Impl(this))
{
    impl_->mainWindow_ = mainWindow ? mainWindow->window() : nullptr;
    setTitle("レイヤー(&L)");
    setIcon(QIcon(resolveIconPath("Studio/menubar_layer.svg")));
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


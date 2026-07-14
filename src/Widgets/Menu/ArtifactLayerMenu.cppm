module;
#include <algorithm>
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
#include <QWidget>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMetaObject>
#include <QPair>
#include <QStatusBar>
#include <QThread>
#include <QUrl>
#include <QtSVG/QSvgRenderer>
#include <QTimer>
#include <QSet>
#include <QPointF>
#include <QRectF>
#include <QUuid>
#include <QVariant>
#include <cmath>
#include <numbers>
#include <wobjectimpl.h>

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
import Artifact.Layer.ParametricComposition;
import Composition.ParametricComposition;
import Artifact.Layer.Shape;
import Artifact.Layer.Video;
import Artifact.Layer.Audio;
import Artifact.Layer.Camera;
import Artifact.Layer.Particle;
import Artifact.Layer.Paint;
import Artifact.Layer.FormParticle;
import Artifact.Layer.Procedural3D;
import Artifact.Layer.ParametricComposition;
import Artifact.Layer.Switch;
import Layer.Blend;
import Color.Float;
import Artifact.Project.Manager;
import Artifact.Project.PresetManager;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Widgets.ProjectManagerWidget;
import Artifact.Widgets.ArtifactPropertyWidget;
import Artifact.Composition.Abstract;
import Artifact.Widgets.PrecomposeDialog;
import Artifact.Widgets.CreatePlaneLayerDialog;
import Artifact.Widgets.QuickLayerCreationDialog;
import Artifact.Widgets.CreateCameraLayerDialog;
import Artifact.Widgets.AppDialogs;
import Artifact.Tool.CameraTracker;
import Tracking.MotionTracker;
import Geometry.LayerAlignment;
import Undo.UndoManager;
import Artifact.Service.Playback;
import ArtifactCore.Control.External;
import UI.ShortcutBindings;
import Time.Rational;

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

bool startHiddenRequested()
{
    if (auto* service = ArtifactProjectService::instance()) {
        return service->defaultNewLayerHidden();
    }
    return false;
}

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

QDockWidget* findDockByTitle(QWidget* window, const QString& title)
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

void setDockVisible(QWidget* window, const QString& title, bool visible)
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

void activateDock(QWidget* window, const QString& title)
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

ArtifactAbstractLayerPtr addDebugBindlessPlaneLayer(
    const CompositionID& compositionId, const QString& name, const QSize& size,
    const FloatColor& startColor, const FloatColor& endColor,
    const float opacity)
{
    auto& manager = ArtifactProjectManager::getInstance();
    ArtifactSolidLayerInitParams params(name);
    params.setWidth(std::max(1, size.width()));
    params.setHeight(std::max(1, size.height()));
    params.setFillType(ArtifactSolidFillType::LinearGradient);
    params.setGradientStartColor(startColor);
    params.setGradientEndColor(endColor);
    params.setGradientAngleDegrees(35.0f);
    auto result = manager.addLayerToComposition(
        compositionId, static_cast<ArtifactLayerInitParams&>(params));
    if (!result.success || !result.layer) {
        return {};
    }
    result.layer->setLayerName(name);
    result.layer->setOpacity(std::clamp(opacity, 0.0f, 1.0f));
    return result.layer;
}

struct RadialTransformSnapshot {
    ArtifactAbstractLayerWeak layer;
    float beforeX = 0.0f;
    float beforeY = 0.0f;
    float beforeScaleX = 1.0f;
    float beforeScaleY = 1.0f;
    float afterX = 0.0f;
    float afterY = 0.0f;
    float afterScaleX = 1.0f;
    float afterScaleY = 1.0f;
};

class RadialTransformLayersCommand final : public UndoCommand {
public:
    RadialTransformLayersCommand(
        const RationalTime& time, std::vector<RadialTransformSnapshot> snapshots)
        : time_(time), snapshots_(std::move(snapshots))
    {
    }

    void undo() override
    {
        apply(true);
    }

    void redo() override
    {
        apply(false);
    }

    QString label() const override
    {
        return QStringLiteral("Radial Transform Layers");
    }

private:
    void apply(const bool before)
    {
        for (const auto& snapshot : snapshots_) {
            const auto layer = snapshot.layer.lock();
            if (!layer) {
                continue;
            }
            auto& transform = layer->transform3D();
            transform.setPosition(
                time_,
                before ? snapshot.beforeX : snapshot.afterX,
                before ? snapshot.beforeY : snapshot.afterY);
            transform.setScale(
                time_,
                before ? snapshot.beforeScaleX : snapshot.afterScaleX,
                before ? snapshot.beforeScaleY : snapshot.afterScaleY);
            layer->changed();
        }
        if (auto* manager = UndoManager::instance()) {
            manager->notifyAnythingChanged();
        }
    }

    RationalTime time_;
    std::vector<RadialTransformSnapshot> snapshots_;
};

class AddCompositionTransformFieldCommand final : public UndoCommand {
public:
    AddCompositionTransformFieldCommand(
        ArtifactCompositionWeakPtr composition, CompositionTransformField field)
        : composition_(std::move(composition)), field_(std::move(field))
    {
    }

    void undo() override
    {
        if (const auto composition = composition_.lock()) {
            composition->removeTransformField(field_.fieldId);
        }
    }

    void redo() override
    {
        if (const auto composition = composition_.lock()) {
            composition->addTransformField(field_);
            composition->setActiveTransformFieldId(field_.fieldId);
        }
    }

    QString label() const override
    {
        return QStringLiteral("Create Live Radial Field");
    }

private:
    ArtifactCompositionWeakPtr composition_;
    CompositionTransformField field_;
};

class UpdateCompositionTransformFieldCommand final : public UndoCommand {
public:
    UpdateCompositionTransformFieldCommand(
        ArtifactCompositionWeakPtr composition, CompositionTransformField before,
        CompositionTransformField after)
        : composition_(std::move(composition)),
          before_(std::move(before)),
          after_(std::move(after))
    {
    }

    void undo() override
    {
        if (const auto composition = composition_.lock()) {
            composition->addTransformField(before_);
        }
    }

    void redo() override
    {
        if (const auto composition = composition_.lock()) {
            composition->addTransformField(after_);
        }
    }

    QString label() const override
    {
        return QStringLiteral("Edit Live Radial Field");
    }

private:
    ArtifactCompositionWeakPtr composition_;
    CompositionTransformField before_;
    CompositionTransformField after_;
};

class RemoveCompositionTransformFieldCommand final : public UndoCommand {
public:
    RemoveCompositionTransformFieldCommand(
        ArtifactCompositionWeakPtr composition, CompositionTransformField field,
        QString activeFieldIdBefore)
        : composition_(std::move(composition)),
          field_(std::move(field)),
          activeFieldIdBefore_(std::move(activeFieldIdBefore))
    {
    }

    void undo() override
    {
        if (const auto composition = composition_.lock()) {
            composition->addTransformField(field_);
            if (activeFieldIdBefore_ == field_.fieldId) {
                composition->setActiveTransformFieldId(activeFieldIdBefore_);
            }
        }
    }

    void redo() override
    {
        if (const auto composition = composition_.lock()) {
            composition->removeTransformField(field_.fieldId);
            if (activeFieldIdBefore_ == field_.fieldId) {
                composition->setActiveTransformFieldId(QString());
            }
        }
    }

    QString label() const override
    {
        return QStringLiteral("Remove Live Radial Field");
    }

private:
    ArtifactCompositionWeakPtr composition_;
    CompositionTransformField field_;
    QString activeFieldIdBefore_;
};

class ReorderCompositionTransformFieldsCommand final : public UndoCommand {
public:
    ReorderCompositionTransformFieldsCommand(
        ArtifactCompositionWeakPtr composition, QVector<CompositionTransformField> before,
        QVector<CompositionTransformField> after, QString label)
        : composition_(std::move(composition)),
          before_(std::move(before)),
          after_(std::move(after)),
          label_(std::move(label))
    {
    }

    void undo() override
    {
        apply(before_);
    }

    void redo() override
    {
        apply(after_);
    }

    QString label() const override
    {
        return label_;
    }

private:
    void apply(const QVector<CompositionTransformField>& fields)
    {
        if (const auto composition = composition_.lock()) {
            composition->setTransformFields(fields);
            if (auto* mgr = UndoManager::instance()) {
                mgr->notifyAnythingChanged();
            }
        }
    }

    ArtifactCompositionWeakPtr composition_;
    QVector<CompositionTransformField> before_;
    QVector<CompositionTransformField> after_;
    QString label_;
};

class SetActiveCompositionTransformFieldCommand final : public UndoCommand {
public:
    SetActiveCompositionTransformFieldCommand(
        ArtifactCompositionWeakPtr composition, QString beforeFieldId,
        QString afterFieldId)
        : composition_(std::move(composition)),
          beforeFieldId_(std::move(beforeFieldId)),
          afterFieldId_(std::move(afterFieldId))
    {
    }

    void undo() override
    {
        if (const auto composition = composition_.lock()) {
            composition->setActiveTransformFieldId(beforeFieldId_);
        }
    }

    void redo() override
    {
        if (const auto composition = composition_.lock()) {
            composition->setActiveTransformFieldId(afterFieldId_);
        }
    }

    QString label() const override
    {
        return QStringLiteral("Activate Live Field");
    }

private:
    ArtifactCompositionWeakPtr composition_;
    QString beforeFieldId_;
    QString afterFieldId_;
};

class SetParametricDefinitionCommand final : public UndoCommand {
public:
    SetParametricDefinitionCommand(
        ArtifactAbstractLayerWeak layer,
        std::shared_ptr<const ParametricCompositionDefinition> before,
        std::shared_ptr<const ParametricCompositionDefinition> after,
        QString label)
        : layer_(std::move(layer)),
          before_(std::move(before)),
          after_(std::move(after)),
          label_(std::move(label))
    {
    }

    void undo() override
    {
        apply(before_);
    }

    void redo() override
    {
        apply(after_);
    }

    QString label() const override
    {
        return label_;
    }

private:
    void apply(const std::shared_ptr<const ParametricCompositionDefinition>& definition)
    {
        const auto layer = std::dynamic_pointer_cast<ArtifactParametricCompositionLayer>(
            layer_.lock());
        if (!layer) {
            return;
        }
        layer->setDefinition(definition);
        if (auto* manager = UndoManager::instance()) {
            manager->notifyAnythingChanged();
        }
    }

    ArtifactAbstractLayerWeak layer_;
    std::shared_ptr<const ParametricCompositionDefinition> before_;
    std::shared_ptr<const ParametricCompositionDefinition> after_;
    QString label_;
};

QStringList transformFieldBlendModeChoices()
{
    return {
        QStringLiteral("normal"),
        QStringLiteral("additive"),
        QStringLiteral("multiply"),
        QStringLiteral("screen"),
    };
}

QString transformFieldBlendModeLabel(const QString& blendMode)
{
    const QString normalized = blendMode.trimmed().toLower();
    if (normalized == QStringLiteral("additive")) {
        return QStringLiteral("Additive");
    }
    if (normalized == QStringLiteral("multiply")) {
        return QStringLiteral("Multiply");
    }
    if (normalized == QStringLiteral("screen")) {
        return QStringLiteral("Screen");
    }
    return QStringLiteral("Normal");
}

QString transformFieldStrengthLabel(qreal strength)
{
    return QStringLiteral("x%1").arg(QString::number(strength, 'f', 1));
}

void clearLiveFieldListActions(QMenu* menu, QVector<QAction*>& actions)
{
    if (!menu) {
        actions.clear();
        return;
    }
    for (auto* action : actions) {
        if (!action) {
            continue;
        }
        menu->removeAction(action);
        delete action;
    }
    actions.clear();
}

std::optional<CompositionTransformField> chooseTransformField(
    QWidget* parent, const ArtifactCompositionPtr& composition,
    const QString& title)
{
    if (!composition) {
        return std::nullopt;
    }
    const auto fields = composition->transformFields();
    if (fields.isEmpty()) {
        QMessageBox::information(
            parent, title, QStringLiteral("このコンポジションにライブFieldはありません。"));
        return std::nullopt;
    }

    QStringList choices;
    choices.reserve(fields.size());
    const QString activeFieldId = composition->activeTransformFieldId();
    int activeIndex = -1;
    for (const auto& field : fields) {
        const QString displayName = field.displayName.trimmed().isEmpty()
                                        ? QStringLiteral("Radial Transform Field")
                                        : field.displayName.trimmed();
        const QString shapeLabel = field.shape == QStringLiteral("box")
                                       ? QStringLiteral("box")
                                       : field.shape == QStringLiteral("linear")
                                             ? QStringLiteral("linear")
                                             : QStringLiteral("radial");
        const QString state = QStringLiteral("%1 / %2 / %3 / %4")
                                  .arg(field.enabled ? QStringLiteral("有効")
                                                     : QStringLiteral("無効"),
                                       field.fieldId == activeFieldId
                                           ? QStringLiteral("active")
                                           : QStringLiteral("inactive"),
                                       shapeLabel,
                                       QStringLiteral("%1 %2%3")
                                           .arg(transformFieldBlendModeLabel(field.blendMode),
                                                transformFieldStrengthLabel(field.strength),
                                                field.invert ? QStringLiteral(" inv")
                                                             : QString()));
        if (activeIndex < 0 && field.fieldId == activeFieldId) {
            activeIndex = choices.size();
        }
        choices.append(QStringLiteral("%1. %2 — %3")
                           .arg(QString::number(choices.size() + 1), displayName,
                                state));
    }
    bool accepted = false;
    const int initialIndex =
        activeIndex >= 0 ? activeIndex : 0;
    const int maxIndex = fields.isEmpty() ? 0 : fields.size() - 1;
    const QString choice = QInputDialog::getItem(
        parent, title, QStringLiteral("Field"), choices,
        std::clamp(initialIndex, 0, maxIndex), false,
        &accepted);
    if (!accepted) {
        return std::nullopt;
    }
    const int index = choices.indexOf(choice);
    if (index < 0 || index >= fields.size()) {
        return std::nullopt;
    }
    return fields.at(index);
}

std::optional<CompositionTransformField> activeOrChosenTransformField(
    QWidget* parent, const ArtifactCompositionPtr& composition,
    const QString& title)
{
    if (!composition) {
        return std::nullopt;
    }
    const QString activeFieldId = composition->activeTransformFieldId().trimmed();
    if (!activeFieldId.isEmpty()) {
        for (const auto& field : composition->transformFields()) {
            if (field.fieldId == activeFieldId) {
                return field;
            }
        }
    }
    return chooseTransformField(parent, composition, title);
}

std::optional<QPair<QVector<CompositionTransformField>, int>>
reorderedTransformFieldsForMove(const ArtifactCompositionPtr& composition,
                               const QString& fieldId, int direction)
{
    if (!composition || fieldId.isEmpty() || direction == 0) {
        return std::nullopt;
    }

    auto fields = composition->transformFields();
    const int index = std::find_if(
        fields.begin(), fields.end(),
        [&fieldId](const CompositionTransformField& field) {
            return field.fieldId == fieldId;
        }) - fields.begin();
    if (index < 0 || index >= fields.size()) {
        return std::nullopt;
    }

    const int targetIndex = index + direction;
    if (targetIndex < 0 || targetIndex >= fields.size()) {
        return std::nullopt;
    }

    std::swap(fields[index], fields[targetIndex]);
    return QPair<QVector<CompositionTransformField>, int>{std::move(fields), targetIndex};
}

std::shared_ptr<ArtifactParametricCompositionLayer> currentSelectedParametricLayer()
{
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return {};
    }
    auto composition = service->currentComposition().lock();
    if (!composition) {
        return {};
    }
    auto* selection = ArtifactLayerSelectionManager::instance();
    if (!selection) {
        return {};
    }
    return std::dynamic_pointer_cast<ArtifactParametricCompositionLayer>(
        selection->currentLayer());
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
    QAction* createPaintAction = nullptr;
    QAction* createFormParticleAction = nullptr;
    QAction* createTerrainAction = nullptr;
    QAction* createPathTubeAction = nullptr;
    QAction* createCameraAction = nullptr;
    QAction* createLightAction = nullptr;
    QAction* createAudioAction = nullptr;
    QAction* createSvgAction = nullptr;
    QAction* createModel3DAction = nullptr;
    QAction* createPlane3DAction = nullptr;
    QAction* createBox3DAction = nullptr;
    QAction* createSphere3DAction = nullptr;
    QAction* createCylinder3DAction = nullptr;
    QAction* createCone3DAction = nullptr;
    QAction* placementAtCompStartAction = nullptr;
    QAction* placementAtPlayheadAction = nullptr;
    QAction* startHiddenAction = nullptr;
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
    QMenu* cacheMenu = nullptr;
    QAction* cacheDefaultAction = nullptr;
    QAction* cacheEnabledAction = nullptr;
    QAction* cacheDisabledAction = nullptr;
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
    QAction* applyLipSyncAction = nullptr;

    QAction* addDebugBlendLayersAction = nullptr;
    QAction* addDebugBindlessPlanesAction = nullptr;
    QAction* addDebugBillboardLayerAction = nullptr;
    QAction* addDebugParticleLayerAction = nullptr;

    QAction* precomposeAction = nullptr;
    QAction* unprecomposeAction = nullptr;
    QAction* groupSelectionAction = nullptr;
    QAction* ungroupAction = nullptr;
    QAction* splitAction = nullptr;

    QMenu* arrangeMenu = nullptr;
    QAction* bringToFrontAction = nullptr;
    QAction* bringForwardAction = nullptr;
    QAction* sendBackwardAction = nullptr;
    QAction* sendToBackAction = nullptr;
    QMenu* alignMenu = nullptr;
    QAction* alignLeftAction = nullptr;
    QAction* alignHCenterAction = nullptr;
    QAction* alignRightAction = nullptr;
    QAction* alignTopAction = nullptr;
    QAction* alignVCenterAction = nullptr;
    QAction* alignBottomAction = nullptr;
    QMenu* distributeMenu = nullptr;
    QAction* distributeHCenterAction = nullptr;
    QAction* distributeVCenterAction = nullptr;
    QAction* distributeSpacingAction = nullptr;
    QAction* radialTransformAction = nullptr;
    QAction* createLiveRadialFieldAction = nullptr;
    QAction* createLiveBoxFieldAction = nullptr;
    QAction* createLiveLinearFieldAction = nullptr;
    QMenu* liveFieldMenu = nullptr;
    QAction* selectLiveRadialFieldAction = nullptr;
    QAction* activatePreviousLiveRadialFieldAction = nullptr;
    QAction* activateNextLiveRadialFieldAction = nullptr;
    QAction* editLiveRadialFieldAction = nullptr;
    QAction* toggleLiveRadialFieldAction = nullptr;
    QAction* moveActiveLiveRadialFieldUpAction = nullptr;
    QAction* moveActiveLiveRadialFieldDownAction = nullptr;
    QAction* moveLiveRadialFieldUpAction = nullptr;
    QAction* moveLiveRadialFieldDownAction = nullptr;
    QAction* removeLiveRadialFieldAction = nullptr;
    QVector<QAction*> liveFieldListActions_;
    QAction* addParametricParameterAction = nullptr;
    QAction* publishParametricParameterAction = nullptr;
    QAction* editParametricControlAction = nullptr;
    QAction* unpublishParametricControlAction = nullptr;
    QAction* controllerLearnAction = nullptr;

    void handleCreateSolid();
    void handleCreateNull();
    void handleCreateConstruction();
    void handleCreateAdjust();
    void handleCreateText();
    void handleCreateParticle();
    void handleCreatePaint();
    void handleCreateFormParticle();
    void handleCreateProcedural3D(Procedural3DLayerKind kind);
    void handleCreateCamera();
    void handleCreateLight();
    void handleCreateAudio();
    void handleCreateSvg();
    void handleCreateModel3D();
    void handleCreatePlane3D();
    void handleCreateBox3D();
    void handleCreateSphere3D();
    void handleCreateCylinder3D();
    void handleCreateCone3D();
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
    void handleSetLayerCachePolicy(int policy);
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
    void handleApplyLipSyncToSwitchLayer();

    void handlePrecompose();
    void handleUnprecompose();
    void handleGroupSelection();
    void handleUngroup();
    void handleSplitLayer();
    void handleTrackCamera();

    void handleBringToFront();
    void handleBringForward();
    void handleSendBackward();
    void handleSendToBack();
    void handleAlign(ArtifactCore::AlignType type);
    void handleDistribute(ArtifactCore::DistributeType type);
    void handleDistributeSpacing();
    void handleRadialTransform();
    void handleCreateLiveTransformField(const QString& shape);
    void handleSelectLiveRadialField();
    void handleActivateLiveRadialField(int direction);
    void handleActivateLiveFieldById(const QString& fieldId);
    void handleEditLiveRadialField();
    void handleToggleLiveRadialField();
    void handleMoveActiveLiveRadialField(int direction);
    void handleMoveLiveRadialField(int direction);
    void handleRemoveLiveRadialField();
    void handleAddParametricParameter();
    void handlePublishParametricParameter();
    void handleEditParametricControl();
    void handleUnpublishParametricControl();
    void handleControllerLearn();

    bool hasCurrentComposition() const;
    bool ensureCurrentComposition();
    bool hasSelectedLayer() const;
    QStringList selectedVideoSourcePathsInCurrentComposition() const;
    void requestRefreshEnabledState();
    void refreshEnabledState();
};

ArtifactLayerMenu::Impl::Impl(ArtifactLayerMenu* menu) : menu_(menu)
{
    createMenu = new QMenu("新規(&N)", menu);
    createMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_add.svg")));
    createSolidAction = new QAction("平面(&Y)...", createMenu);
    createSolidAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerCreateSolid));
    createSolidAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_palette.svg")));

    createNullAction = new QAction("ヌルオブジェクト(&N)", createMenu);
    createNullAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerCreateNull));
    createNullAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_aspect_ratio.svg")));

    createConstructionAction = new QAction("Construction Layer", createMenu);
    createConstructionAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_grid_on.svg")));
    createConstructionAction->setToolTip(QStringLiteral("Create a construction layer (editor-only by default)"));

    createAdjustAction = new QAction("調整レイヤー(&A)", createMenu);
    createAdjustAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerCreateAdjust));
    createAdjustAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_blur_on.svg")));

    createTextAction = new QAction("テキスト(&T)", createMenu);
    createTextAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerCreateText));
    createTextAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_title.svg")));

    createParticleAction = new QAction("パーティクル(&P)", createMenu);
    createParticleAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_particle.svg")));
    createPaintAction = new QAction("ペイントレイヤー(&R)...", createMenu);
    createPaintAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_paint.svg")));
    createPaintAction->setToolTip(QStringLiteral("Create a frame-by-frame Paint Layer for brush work."));
    createFormParticleAction = new QAction("Form Particle(&F)", createMenu);
    createFormParticleAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_particle.svg")));
    createFormParticleAction->setToolTip(QStringLiteral("Grid / point-cloud particle layer inspired by Trapcode Form"));
    createTerrainAction = new QAction("Terrain (Mir)", createMenu);
    createTerrainAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_model3d.svg")));
    createTerrainAction->setToolTip(QStringLiteral("Animated procedural height-field surface"));
    createPathTubeAction = new QAction("Path Tube (Tao)", createMenu);
    createPathTubeAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_model3d.svg")));
    createPathTubeAction->setToolTip(QStringLiteral("Procedural tube or ribbon along an animated path"));

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
    createBox3DAction = new QAction("3D Boxレイヤー(&B)", createMenu);
    createBox3DAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_model3d.svg")));
    createBox3DAction->setToolTip(QStringLiteral("Create a fixed box as a 3D layer"));
    createSphere3DAction = new QAction("3D Sphereレイヤー(&S)", createMenu);
    createSphere3DAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_model3d.svg")));
    createSphere3DAction->setToolTip(QStringLiteral("Create a fixed sphere as a 3D layer"));
    createCylinder3DAction = new QAction("3D Cylinderレイヤー(&Y)", createMenu);
    createCylinder3DAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_model3d.svg")));
    createCylinder3DAction->setToolTip(QStringLiteral("Create a fixed cylinder as a 3D layer"));
    createCone3DAction = new QAction("3D Coneレイヤー(&N)", createMenu);
    createCone3DAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_model3d.svg")));
    createCone3DAction->setToolTip(QStringLiteral("Create a fixed cone as a 3D layer"));
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
    startHiddenAction = new QAction("非表示のまま追加", createPlacementMenu);
    startHiddenAction->setCheckable(true);
    startHiddenAction->setChecked(startHiddenRequested());
    startHiddenAction->setToolTip(QStringLiteral("新規レイヤーを表示オフのまま追加します"));
    createPlacementMenu->addAction(startHiddenAction);
    cycleLayerForwardAction = new QAction("レイヤーを次々作成", createMenu);
    cycleLayerForwardAction->setToolTip(QStringLiteral("Cycle common layer creation presets"));
    cycleLayerForwardAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerCreateLayerCycleForward));
    cycleLayerReverseAction = new QAction("レイヤーを逆順で次々作成", createMenu);
    cycleLayerReverseAction->setToolTip(QStringLiteral("Cycle common layer creation presets in reverse"));
    cycleLayerReverseAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerCreateLayerCycleReverse));

    createShapeMenu = new QMenu("シェイプ(&S)", createMenu);
    createShapeMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_shape_rect.svg")));
    cycleShapeForwardAction = new QAction("シェイプを次々作成", createShapeMenu);
    cycleShapeForwardAction->setToolTip(QStringLiteral("Cycle shape presets"));
    cycleShapeForwardAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerCreateShapeCycleForward));
    cycleShapeReverseAction = new QAction("シェイプを逆順で次々作成", createShapeMenu);
    cycleShapeReverseAction->setToolTip(QStringLiteral("Cycle shape presets in reverse"));
    cycleShapeReverseAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerCreateShapeCycleReverse));
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
    createMenu->addAction(createPaintAction);
    createMenu->addAction(createFormParticleAction);
    createMenu->addAction(createTerrainAction);
    createMenu->addAction(createPathTubeAction);
    createMenu->addAction(createCameraAction);
    createMenu->addAction(createLightAction);
    createMenu->addAction(createAudioAction);
    createMenu->addAction(createSvgAction);
    createMenu->addAction(createModel3DAction);
    createMenu->addAction(createPlane3DAction);
    createMenu->addAction(createBox3DAction);
    createMenu->addAction(createSphere3DAction);
    createMenu->addAction(createCylinder3DAction);
    createMenu->addAction(createCone3DAction);
    createMenu->addMenu(createPlacementMenu);
    createMenu->addAction(cycleLayerForwardAction);
    createMenu->addAction(cycleLayerReverseAction);
    createMenu->addMenu(createShapeMenu);

    duplicateLayerAction = new QAction("レイヤーを複製(&D)", menu);
    duplicateLayerAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerDuplicate));
    duplicateLayerAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_content_copy.svg")));
    renameLayerAction = new QAction("レイヤー名を変更(&R)...", menu);
    renameLayerAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerRename));
    renameLayerAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_rename.svg")));
    deleteLayerAction = new QAction("削除(&X)", menu);
    deleteLayerAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerDelete));
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
    cacheMenu = new QMenu("Cache Policy", switchMenu);
    cacheMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_settings.svg")));
    cacheDefaultAction = new QAction("Default", cacheMenu);
    cacheEnabledAction = new QAction("Enabled", cacheMenu);
    cacheDisabledAction = new QAction("Disabled", cacheMenu);
    for (auto *action : {cacheDefaultAction, cacheEnabledAction, cacheDisabledAction}) {
        action->setCheckable(true);
        cacheMenu->addAction(action);
    }
    switchMenu->addAction(toggleVisibleAction);
    switchMenu->addAction(toggleLockAction);
    switchMenu->addAction(toggleSoloAction);
    switchMenu->addAction(toggleShyAction);
    switchMenu->addSeparator();
    switchMenu->addMenu(cacheMenu);
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

    arrangeMenu = new QMenu("配置(&A)", menu);
    arrangeMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_arrange.svg")));
    bringToFrontAction = new QAction("最前面へ(&F)", arrangeMenu);
    bringToFrontAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerBringToFront));
    bringToFrontAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_bring_to_front.svg")));
    bringForwardAction = new QAction("1つ前面へ(&W)", arrangeMenu);
    bringForwardAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerBringForward));
    sendBackwardAction = new QAction("1つ背面へ(&B)", arrangeMenu);
    sendBackwardAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerSendBackward));
    sendToBackAction = new QAction("最背面へ(&K)", arrangeMenu);
    sendToBackAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerSendToBack));
    sendToBackAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_send_to_back.svg")));
    arrangeMenu->addAction(bringToFrontAction);
    arrangeMenu->addAction(bringForwardAction);
    arrangeMenu->addAction(sendBackwardAction);
    arrangeMenu->addAction(sendToBackAction);
    arrangeMenu->addSeparator();
    radialTransformAction = new QAction("放射状変形...", arrangeMenu);
    radialTransformAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_arrange.svg")));
    radialTransformAction->setToolTip(
        QStringLiteral("選択中心からの距離に応じて、複数レイヤーの位置とスケールを一度だけ変更します"));
    arrangeMenu->addAction(radialTransformAction);
    createLiveRadialFieldAction = new QAction("ライブ放射状Fieldを作成...", arrangeMenu);
    createLiveRadialFieldAction->setIcon(
        QIcon(resolveIconPath("Studio/layermenu_arrange.svg")));
    createLiveRadialFieldAction->setToolTip(
        QStringLiteral("元のTransformを保ったまま、選択レイヤーへ放射状変形を適用します"));
    arrangeMenu->addAction(createLiveRadialFieldAction);
    createLiveBoxFieldAction = new QAction("ライブBox Fieldを作成...", arrangeMenu);
    createLiveBoxFieldAction->setIcon(
        QIcon(resolveIconPath("Studio/layermenu_arrange.svg")));
    createLiveBoxFieldAction->setToolTip(
        QStringLiteral("元のTransformを保ったまま、選択レイヤーへ矩形範囲の変形を適用します"));
    arrangeMenu->addAction(createLiveBoxFieldAction);
    createLiveLinearFieldAction = new QAction("ライブLinear Fieldを作成...", arrangeMenu);
    createLiveLinearFieldAction->setIcon(
        QIcon(resolveIconPath("Studio/layermenu_arrange.svg")));
    createLiveLinearFieldAction->setToolTip(
        QStringLiteral("元のTransformを保ったまま、選択レイヤーへ方向付きField変形を適用します"));
    arrangeMenu->addAction(createLiveLinearFieldAction);
    liveFieldMenu = new QMenu("Live Fields", arrangeMenu);
    liveFieldMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_settings.svg")));
    arrangeMenu->addMenu(liveFieldMenu);
    selectLiveRadialFieldAction = new QAction("ライブFieldを選択...", arrangeMenu);
    arrangeMenu->addAction(selectLiveRadialFieldAction);
    activatePreviousLiveRadialFieldAction = new QAction("前のFieldをアクティブ", arrangeMenu);
    activateNextLiveRadialFieldAction = new QAction("次のFieldをアクティブ", arrangeMenu);
    arrangeMenu->addAction(activatePreviousLiveRadialFieldAction);
    arrangeMenu->addAction(activateNextLiveRadialFieldAction);
    editLiveRadialFieldAction = new QAction("ライブFieldを編集...", arrangeMenu);
    toggleLiveRadialFieldAction = new QAction("ライブFieldを有効/無効...", arrangeMenu);
    moveActiveLiveRadialFieldUpAction = new QAction("アクティブFieldを上へ", arrangeMenu);
    moveActiveLiveRadialFieldDownAction = new QAction("アクティブFieldを下へ", arrangeMenu);
    moveLiveRadialFieldUpAction = new QAction("ライブFieldを上へ...", arrangeMenu);
    moveLiveRadialFieldDownAction = new QAction("ライブFieldを下へ...", arrangeMenu);
    removeLiveRadialFieldAction = new QAction("ライブFieldを削除...", arrangeMenu);
    liveFieldMenu->addAction(editLiveRadialFieldAction);
    liveFieldMenu->addAction(toggleLiveRadialFieldAction);
    liveFieldMenu->addSeparator();
    liveFieldMenu->addAction(moveActiveLiveRadialFieldUpAction);
    liveFieldMenu->addAction(moveActiveLiveRadialFieldDownAction);
    liveFieldMenu->addSeparator();
    liveFieldMenu->addAction(removeLiveRadialFieldAction);
    liveFieldMenu->addSeparator();
    arrangeMenu->addAction(editLiveRadialFieldAction);
    arrangeMenu->addAction(toggleLiveRadialFieldAction);
    arrangeMenu->addAction(moveActiveLiveRadialFieldUpAction);
    arrangeMenu->addAction(moveActiveLiveRadialFieldDownAction);
    arrangeMenu->addAction(moveLiveRadialFieldUpAction);
    arrangeMenu->addAction(moveLiveRadialFieldDownAction);
    arrangeMenu->addAction(removeLiveRadialFieldAction);
    arrangeMenu->addSeparator();
    addParametricParameterAction = new QAction("Parametric Parameterを追加...", arrangeMenu);
    publishParametricParameterAction = new QAction("ParameterをPublished Controlにする...", arrangeMenu);
    editParametricControlAction = new QAction("Published Controlを編集...", arrangeMenu);
    unpublishParametricControlAction = new QAction("Published Controlを解除...", arrangeMenu);
    controllerLearnAction = new QAction("Controller Learn を現在Propertyへ適用", arrangeMenu);
    arrangeMenu->addAction(addParametricParameterAction);
    arrangeMenu->addAction(publishParametricParameterAction);
    arrangeMenu->addAction(editParametricControlAction);
    arrangeMenu->addAction(unpublishParametricControlAction);
    arrangeMenu->addSeparator();
    arrangeMenu->addAction(controllerLearnAction);

    alignMenu = new QMenu("整列(&L)", menu);
    alignMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_align.svg")));
    alignLeftAction = new QAction("左端を揃える", alignMenu);
    alignLeftAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerAlignLeft));
    alignLeftAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_align_left.svg")));
    alignHCenterAction = new QAction("水平中央を揃える", alignMenu);
    alignHCenterAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerAlignHCenter));
    alignHCenterAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_align_hcenter.svg")));
    alignRightAction = new QAction("右端を揃える", alignMenu);
    alignRightAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerAlignRight));
    alignRightAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_align_right.svg")));
    alignMenu->addSeparator();
    alignTopAction = new QAction("上端を揃える", alignMenu);
    alignTopAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerAlignTop));
    alignTopAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_align_top.svg")));
    alignVCenterAction = new QAction("垂直中央を揃える", alignMenu);
    alignVCenterAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerAlignVCenter));
    alignVCenterAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_align_vcenter.svg")));
    alignBottomAction = new QAction("下端を揃える", alignMenu);
    alignBottomAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerAlignBottom));
    alignBottomAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_align_bottom.svg")));
    alignMenu->addAction(alignLeftAction);
    alignMenu->addAction(alignHCenterAction);
    alignMenu->addAction(alignRightAction);
    alignMenu->addAction(alignTopAction);
    alignMenu->addAction(alignVCenterAction);
    alignMenu->addAction(alignBottomAction);

    distributeMenu = new QMenu("分布(&D)", menu);
    distributeMenu->setIcon(QIcon(resolveIconPath("Studio/layermenu_distribute.svg")));
    distributeHCenterAction = new QAction("水平中央を分布", distributeMenu);
    distributeHCenterAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerDistributeHCenter));
    distributeVCenterAction = new QAction("垂直中央を分布", distributeMenu);
    distributeVCenterAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerDistributeVCenter));
    distributeSpacingAction = new QAction("等間隔に配置", distributeMenu);
    distributeSpacingAction->setShortcut(
        ShortcutBindings::instance().shortcut(ShortcutId::LayerDistributeSpacing));
    distributeMenu->addAction(distributeHCenterAction);
    distributeMenu->addAction(distributeVCenterAction);
    distributeMenu->addAction(distributeSpacingAction);

    openInspectorAction = new QAction("Inspector を開く", menu);
    openInspectorAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_inspector.svg")));
    openPropertiesAction = new QAction("Properties を開く", menu);
    openPropertiesAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_settings.svg")));
    applyLipSyncAction = new QAction("Lip Sync を Switch Layer に適用", menu);

    addDebugBlendLayersAction = new QAction("Debug Blend Test Layers...", debugMenu);
    addDebugBlendLayersAction->setIcon(QIcon(resolveIconPath("Studio/testmenu_layer_composite.svg")));
    addDebugBlendLayersAction->setToolTip(
        QStringLiteral("Debug 用の合成テストレイヤーをまとめて追加します"));
    debugMenu->addAction(addDebugBlendLayersAction);

    addDebugBindlessPlanesAction = new QAction("Debug Bindless Sprite Planes...", debugMenu);
    addDebugBindlessPlanesAction->setIcon(QIcon(resolveIconPath("Studio/testmenu_layer_composite.svg")));
    addDebugBindlessPlanesAction->setToolTip(
        QStringLiteral("Bindless sprite batch の検証用にテクスチャ平面だけを追加します"));
    debugMenu->addAction(addDebugBindlessPlanesAction);

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
    precomposeAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_view_comfy.svg")));
    unprecomposeAction = new QAction("プリコンポーズを解除", menu);
    unprecomposeAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_ungroup.svg")));
    groupSelectionAction = new QAction("グループ化(&G)...", menu);
    groupSelectionAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_group.svg")));
    ungroupAction = new QAction("グループ解除(&U)", menu);
    ungroupAction->setIcon(QIcon(resolveIconPath("Studio/layermenu_ungroup.svg")));
    splitAction = new QAction("レイヤー分割(&L)", menu);
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
    menu->addMenu(arrangeMenu);
    menu->addMenu(alignMenu);
    menu->addMenu(distributeMenu);
    menu->addSeparator();
    menu->addAction(openInspectorAction);
    menu->addAction(openPropertiesAction);
    menu->addAction(applyLipSyncAction);
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
        const QString actionData = action->data().toString();
        if (actionData.startsWith(QStringLiteral("live-field:activate:"))) {
            handleActivateLiveFieldById(actionData.mid(QStringLiteral("live-field:activate:").size()));
            return;
        }
        if (action == createSolidAction) { handleCreateSolid(); return; }
        if (action == createNullAction) { handleCreateNull(); return; }
        if (action == createConstructionAction) { handleCreateConstruction(); return; }
        if (action == createAdjustAction) { handleCreateAdjust(); return; }
        if (action == createTextAction) { handleCreateText(); return; }
        if (action == createParticleAction) { handleCreateParticle(); return; }
        if (action == createPaintAction) { handleCreatePaint(); return; }
        if (action == createFormParticleAction) { handleCreateFormParticle(); return; }
        if (action == createTerrainAction) { handleCreateProcedural3D(Procedural3DLayerKind::Terrain); return; }
        if (action == createPathTubeAction) { handleCreateProcedural3D(Procedural3DLayerKind::PathTube); return; }
        if (action == createCameraAction) { handleCreateCamera(); return; }
        if (action == createLightAction) { handleCreateLight(); return; }
        if (action == createAudioAction) { handleCreateAudio(); return; }
        if (action == createSvgAction) { handleCreateSvg(); return; }
        if (action == createModel3DAction) { handleCreateModel3D(); return; }
        if (action == createPlane3DAction) { handleCreatePlane3D(); return; }
        if (action == createBox3DAction) { handleCreateBox3D(); return; }
        if (action == createSphere3DAction) { handleCreateSphere3D(); return; }
        if (action == createCylinder3DAction) { handleCreateCylinder3D(); return; }
        if (action == createCone3DAction) { handleCreateCone3D(); return; }
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
        if (action == startHiddenAction) {
            if (auto* service = ArtifactProjectService::instance()) {
                service->setDefaultNewLayerHidden(startHiddenAction->isChecked());
            }
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
        if (action == cacheDefaultAction) { handleSetLayerCachePolicy(0); return; }
        if (action == cacheEnabledAction) { handleSetLayerCachePolicy(1); return; }
        if (action == cacheDisabledAction) { handleSetLayerCachePolicy(2); return; }
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
        if (action == applyLipSyncAction) { handleApplyLipSyncToSwitchLayer(); return; }
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
        if (action == addDebugBindlessPlanesAction) {
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
            const QSize planeSize(std::max(64, compSize.width() / 2),
                                  std::max(64, compSize.height() / 2));
            const CompositionID compositionId = comp->id();

            ArtifactAbstractLayerPtr lastCreatedLayer;
            lastCreatedLayer = addDebugBindlessPlaneLayer(
                compositionId, uniqueLayerName(QStringLiteral("Debug Bindless Plane A")),
                planeSize, FloatColor(0.95f, 0.18f, 0.34f, 1.0f),
                FloatColor(0.12f, 0.18f, 0.70f, 1.0f), 0.88f);
            if (!lastCreatedLayer) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "Bindless テスト平面 A の追加に失敗しました。");
                return;
            }

            lastCreatedLayer = addDebugBindlessPlaneLayer(
                compositionId, uniqueLayerName(QStringLiteral("Debug Bindless Plane B")),
                planeSize, FloatColor(0.10f, 0.75f, 0.60f, 1.0f),
                FloatColor(0.92f, 0.74f, 0.18f, 1.0f), 0.72f);
            if (!lastCreatedLayer) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "Bindless テスト平面 B の追加に失敗しました。");
                return;
            }

            lastCreatedLayer = addDebugBindlessPlaneLayer(
                compositionId, uniqueLayerName(QStringLiteral("Debug Bindless Plane C")),
                planeSize, FloatColor(0.72f, 0.28f, 0.92f, 1.0f),
                FloatColor(0.16f, 0.64f, 0.95f, 1.0f), 0.58f);
            if (!lastCreatedLayer) {
                QMessageBox::warning(menu_->window(), "Debug Layers",
                                     "Bindless テスト平面 C の追加に失敗しました。");
                return;
            }

            projectService->selectLayer(lastCreatedLayer->id());
            QMessageBox::information(
                menu_->window(), "Debug Layers",
                QStringLiteral("Debug Bindless Sprite Planes を追加しました。\n\n"
                               "gradient solid planes だけで構成されるため、"
                               "composition 描画では SpriteXform packet の bindless batch を検証できます。"));
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
        if (action == bringToFrontAction) { handleBringToFront(); return; }
        if (action == bringForwardAction) { handleBringForward(); return; }
        if (action == sendBackwardAction) { handleSendBackward(); return; }
        if (action == sendToBackAction) { handleSendToBack(); return; }
        if (action == radialTransformAction) { handleRadialTransform(); return; }
        if (action == createLiveRadialFieldAction) { handleCreateLiveTransformField(QStringLiteral("radial")); return; }
        if (action == createLiveBoxFieldAction) { handleCreateLiveTransformField(QStringLiteral("box")); return; }
        if (action == createLiveLinearFieldAction) { handleCreateLiveTransformField(QStringLiteral("linear")); return; }
        if (action == selectLiveRadialFieldAction) { handleSelectLiveRadialField(); return; }
        if (action == activatePreviousLiveRadialFieldAction) { handleActivateLiveRadialField(-1); return; }
        if (action == activateNextLiveRadialFieldAction) { handleActivateLiveRadialField(1); return; }
        if (action == editLiveRadialFieldAction) { handleEditLiveRadialField(); return; }
        if (action == toggleLiveRadialFieldAction) { handleToggleLiveRadialField(); return; }
        if (action == moveActiveLiveRadialFieldUpAction) { handleMoveActiveLiveRadialField(-1); return; }
        if (action == moveActiveLiveRadialFieldDownAction) { handleMoveActiveLiveRadialField(1); return; }
        if (action == moveLiveRadialFieldUpAction) { handleMoveLiveRadialField(-1); return; }
        if (action == moveLiveRadialFieldDownAction) { handleMoveLiveRadialField(1); return; }
        if (action == removeLiveRadialFieldAction) { handleRemoveLiveRadialField(); return; }
        if (action == addParametricParameterAction) { handleAddParametricParameter(); return; }
        if (action == publishParametricParameterAction) { handlePublishParametricParameter(); return; }
        if (action == editParametricControlAction) { handleEditParametricControl(); return; }
        if (action == unpublishParametricControlAction) { handleUnpublishParametricControl(); return; }
        if (action == controllerLearnAction) { handleControllerLearn(); return; }
        if (action == alignLeftAction) { handleAlign(ArtifactCore::AlignType::Left); return; }
        if (action == alignHCenterAction) { handleAlign(ArtifactCore::AlignType::CenterHorizontal); return; }
        if (action == alignRightAction) { handleAlign(ArtifactCore::AlignType::Right); return; }
        if (action == alignTopAction) { handleAlign(ArtifactCore::AlignType::Top); return; }
        if (action == alignVCenterAction) { handleAlign(ArtifactCore::AlignType::CenterVertical); return; }
        if (action == alignBottomAction) { handleAlign(ArtifactCore::AlignType::Bottom); return; }
        if (action == distributeHCenterAction) { handleDistribute(ArtifactCore::DistributeType::CenterHorizontal); return; }
        if (action == distributeVCenterAction) { handleDistribute(ArtifactCore::DistributeType::CenterVertical); return; }
        if (action == distributeSpacingAction) { handleDistributeSpacing(); return; }
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
        eventBus.subscribe<CurrentCompositionChangedEvent>(
            [this](const CurrentCompositionChangedEvent&) {
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

void ArtifactLayerMenu::Impl::requestRefreshEnabledState()
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
    createPaintAction->setEnabled(hasProject);
    createCameraAction->setEnabled(hasProject);
    createLightAction->setEnabled(hasProject);
    createAudioAction->setEnabled(hasProject);
    createSvgAction->setEnabled(hasProject);
    createModel3DAction->setEnabled(hasProject);
    createPlane3DAction->setEnabled(hasProject);
    createBox3DAction->setEnabled(hasProject);
    createSphere3DAction->setEnabled(hasProject);
    createCylinder3DAction->setEnabled(hasProject);
    createCone3DAction->setEnabled(hasProject);
    startHiddenAction->setEnabled(hasProject);
    if (service) {
        startHiddenAction->setChecked(service->defaultNewLayerHidden());
    }
    addDebugBindlessPlanesAction->setEnabled(hasProject);
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
        int cachePolicy = 0;
        if (service) {
            if (auto comp = service->currentComposition().lock()) {
                if (auto layer = comp->layerById(selectedLayerId_)) {
                    if (auto prop = layer->getProperty(QStringLiteral("layer.cachePolicy"))) {
                        cachePolicy = prop->getValue().toInt();
                    }
                }
            }
        }
        toggleVisibleAction->setText(visible ? QStringLiteral("非表示にする") : QStringLiteral("表示する"));
        toggleLockAction->setText(locked ? QStringLiteral("ロックを解除") : QStringLiteral("ロックする"));
        toggleSoloAction->setText(solo ? QStringLiteral("ソロを解除") : QStringLiteral("ソロにする"));
        toggleShyAction->setText(shy ? QStringLiteral("シャイを解除") : QStringLiteral("シャイにする"));
        cacheDefaultAction->setChecked(cachePolicy == 0);
        cacheEnabledAction->setChecked(cachePolicy == 1);
        cacheDisabledAction->setChecked(cachePolicy == 2);
    } else {
        toggleVisibleAction->setText(QStringLiteral("表示/非表示を切替"));
        toggleLockAction->setText(QStringLiteral("ロックを切替"));
        toggleSoloAction->setText(QStringLiteral("ソロを切替"));
        toggleShyAction->setText(QStringLiteral("シャイを切替"));
        cacheDefaultAction->setChecked(true);
        cacheEnabledAction->setChecked(false);
        cacheDisabledAction->setChecked(false);
    }
    toggleVisibleAction->setEnabled(hasLayer);
    toggleLockAction->setEnabled(hasLayer);
    toggleSoloAction->setEnabled(hasLayer);
    toggleShyAction->setEnabled(hasLayer);
    cacheDefaultAction->setEnabled(hasLayer);
    cacheEnabledAction->setEnabled(hasLayer);
    cacheDisabledAction->setEnabled(hasLayer);
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
    auto* app = ArtifactApplicationManager::instance();
    auto* selectionManager = app ? app->layerSelectionManager() : nullptr;
    bool canApplyLipSync = false;
    if (hasComp && selectionManager) {
        const auto selected = selectionManager->selectedLayers();
        bool hasAudio = false;
        bool hasSwitch = false;
        if (auto comp = service->currentComposition().lock()) {
            for (const auto& layer : comp->allLayer()) {
                if (!layer) {
                    continue;
                }
                bool selectedMatch = false;
                for (const auto& selectedLayer : selected) {
                    if (selectedLayer && selectedLayer->id() == layer->id()) {
                        selectedMatch = true;
                        break;
                    }
                }
                if (!selectedMatch) {
                    continue;
                }
                hasAudio = hasAudio ||
                           static_cast<bool>(std::dynamic_pointer_cast<ArtifactAudioLayer>(layer));
                hasSwitch = hasSwitch ||
                            static_cast<bool>(std::dynamic_pointer_cast<ArtifactSwitchLayer>(layer));
            }
        }
        canApplyLipSync = hasAudio && hasSwitch;
    }
    applyLipSyncAction->setEnabled(canApplyLipSync);
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
    if (app && app->layerSelectionManager()) {
        auto current = app->layerSelectionManager()->currentLayer();
        isGroupSelected = current && current->isGroupLayer();
    }
    ungroupAction->setEnabled(isGroupSelected && hasComp);
    
    splitAction->setEnabled(hasLayer);

    const auto parametricLayer = currentSelectedParametricLayer();
    const bool hasParametricLayer = static_cast<bool>(parametricLayer);
    const auto parametricDefinition =
        hasParametricLayer ? parametricLayer->definition()
                           : std::shared_ptr<const ParametricCompositionDefinition>{};
    addParametricParameterAction->setEnabled(hasParametricLayer);
    publishParametricParameterAction->setEnabled(
        parametricDefinition && !parametricDefinition->parameters().isEmpty());
    editParametricControlAction->setEnabled(
        parametricDefinition && !parametricDefinition->publishedControls().isEmpty());
    unpublishParametricControlAction->setEnabled(
        parametricDefinition && !parametricDefinition->publishedControls().isEmpty());
    controllerLearnAction->setEnabled(
        activePropertyWidget(mainWindow_ ? mainWindow_ : menu_->window()) != nullptr);

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

    auto* currentSelectionManager = app ? app->layerSelectionManager() : nullptr;
    const int selectedLayerCount =
        currentSelectionManager ? currentSelectionManager->selectedLayers().size() : 0;
    radialTransformAction->setEnabled(hasComp && selectedLayerCount >= 2);
    createLiveRadialFieldAction->setEnabled(hasComp && selectedLayerCount >= 2);
    createLiveBoxFieldAction->setEnabled(hasComp && selectedLayerCount >= 2);
    createLiveLinearFieldAction->setEnabled(hasComp && selectedLayerCount >= 2);
    const auto currentComposition =
        service ? service->currentComposition().lock() : ArtifactCompositionPtr{};
    const bool hasLiveFields =
        currentComposition && !currentComposition->transformFields().isEmpty();
    const bool canReorderLiveFields =
        currentComposition && currentComposition->transformFields().size() >= 2;
    const bool hasActiveLiveField =
        currentComposition && !currentComposition->activeTransformFieldId().trimmed().isEmpty();
    if (liveFieldMenu) {
        liveFieldMenu->setEnabled(hasLiveFields);
        if (hasLiveFields && currentComposition) {
            const auto fields = currentComposition->transformFields();
            const QString activeFieldId = currentComposition->activeTransformFieldId();
            const int fieldCount = fields.size();
            QString activeFieldName = QStringLiteral("none");
            for (const auto& field : fields) {
                if (field.fieldId == activeFieldId) {
                    activeFieldName = field.displayName.trimmed().isEmpty()
                                          ? QStringLiteral("Radial Transform Field")
                                          : field.displayName.trimmed();
                    break;
                }
            }
            liveFieldMenu->setTitle(
                QStringLiteral("Live Fields (%1)").arg(fieldCount));
            liveFieldMenu->setToolTip(
                QStringLiteral("Active: %1").arg(activeFieldName));
        } else {
            liveFieldMenu->setTitle(QStringLiteral("Live Fields"));
            liveFieldMenu->setToolTip(QStringLiteral("No live fields in this composition"));
        }
    }
    selectLiveRadialFieldAction->setEnabled(hasLiveFields);
    activatePreviousLiveRadialFieldAction->setEnabled(canReorderLiveFields);
    activateNextLiveRadialFieldAction->setEnabled(canReorderLiveFields);
    editLiveRadialFieldAction->setEnabled(hasLiveFields);
    toggleLiveRadialFieldAction->setEnabled(hasLiveFields);
    moveActiveLiveRadialFieldUpAction->setEnabled(canReorderLiveFields && hasActiveLiveField);
    moveActiveLiveRadialFieldDownAction->setEnabled(canReorderLiveFields && hasActiveLiveField);
    moveLiveRadialFieldUpAction->setEnabled(canReorderLiveFields);
    moveLiveRadialFieldDownAction->setEnabled(canReorderLiveFields);
    removeLiveRadialFieldAction->setEnabled(hasLiveFields);

    clearLiveFieldListActions(liveFieldMenu, liveFieldListActions_);
    if (hasLiveFields && liveFieldMenu && currentComposition) {
        const auto fields = currentComposition->transformFields();
        const QString activeFieldId = currentComposition->activeTransformFieldId();
        for (const auto& field : fields) {
            const QString blendLabel = transformFieldBlendModeLabel(field.blendMode);
            const QString shapeLabel = field.shape == QStringLiteral("box")
                                           ? QStringLiteral("box")
                                           : field.shape == QStringLiteral("linear")
                                                 ? QStringLiteral("linear")
                                                 : QStringLiteral("radial");
            auto* action = new QAction(
                QStringLiteral("%1  [%2 / %3 / %4 / %5 / %6%7]")
                    .arg(field.displayName.trimmed().isEmpty()
                             ? QStringLiteral("Radial Transform Field")
                             : field.displayName.trimmed(),
                         field.fieldId == activeFieldId ? QStringLiteral("active")
                                                        : QStringLiteral("inactive"),
                          field.enabled ? QStringLiteral("enabled")
                                        : QStringLiteral("disabled"),
                          shapeLabel,
                          blendLabel,
                          transformFieldStrengthLabel(field.strength),
                          field.invert ? QStringLiteral(" / inv") : QString()),
                liveFieldMenu);
            action->setCheckable(true);
            action->setChecked(field.fieldId == activeFieldId);
            action->setData(QStringLiteral("live-field:activate:%1").arg(field.fieldId));
            liveFieldMenu->addAction(action);
            liveFieldListActions_.push_back(action);
        }
    }
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
    QuickLayerCreationDialog dialog(parentWindow);
    dialog.setModal(true);
    if (dialog.exec() != QDialog::Accepted || !service) {
        return;
    }
    const QuickLayerCreationOptions options = dialog.submittedOptions();
    const ArtifactSolidLayerInitParams params = options.solidParams;
    const bool placeAtCurrentFrame = placeAtCurrentFrameRequested(layerCreationPlacementMode());
    QTimer::singleShot(0, menu, [service, params, options, menu, placeAtCurrentFrame]() {
        Q_UNUSED(menu);
        service->addLayerToCurrentComposition(params, true, placeAtCurrentFrame);
        auto layer = ArtifactLayerSelectionManager::instance()->currentLayer();
        if (!layer) {
            return;
        }
        if (options.envelope.enabled) {
            layer->setEffectEnvelope(options.envelope);
        }
        if (options.maskShape == QuickLayerMaskShape::None) {
            return;
        }
        const double width = static_cast<double>(params.width());
        const double height = static_cast<double>(params.height());
        const double cx = width * 0.5;
        const double cy = height * 0.5;
        const double rx = width * 0.5;
        const double ry = height * 0.5;
        MaskPath path;
        if (options.maskShape == QuickLayerMaskShape::Rectangle) {
            for (const QPointF point : {QPointF(0.0, 0.0), QPointF(width, 0.0),
                                        QPointF(width, height), QPointF(0.0, height)}) {
                path.addVertex(MaskVertex{point, QPointF(), QPointF()});
            }
        } else {
            constexpr int samples = 32;
            for (int i = 0; i < samples; ++i) {
                const double angle = (2.0 * std::numbers::pi * i) / samples;
                path.addVertex(MaskVertex{QPointF(cx + rx * std::cos(angle),
                                                  cy + ry * std::sin(angle)),
                                          QPointF(), QPointF()});
            }
        }
        path.setClosed(true);
        path.setFeather(options.maskFeather);
        path.setName(UniString(QStringLiteral("Quick Mask")));
        LayerMask mask;
        mask.addMaskPath(path);
        layer->addMask(mask);
        layer->changed();
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

void ArtifactLayerMenu::Impl::handleCreatePaint()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, QStringLiteral("Layer"),
                             QStringLiteral("コンポジションが選択されていません。"));
        return;
    }

    ArtifactLayerInitParams params(uniqueLayerName(QStringLiteral("Paint Layer 1")),
                                   LayerType::Paint);
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
}

void ArtifactLayerMenu::Impl::handleCreateFormParticle()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }

    ArtifactLayerInitParams params(uniqueLayerName(QStringLiteral("Form Particle 1")),
                                   LayerType::FormParticle);
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());

    const auto created = ArtifactLayerSelectionManager::instance()
        ? ArtifactLayerSelectionManager::instance()->currentLayer()
        : ArtifactAbstractLayerPtr{};
    if (const auto formLayer = std::dynamic_pointer_cast<ArtifactFormParticleLayer>(created)) {
        formLayer->loadPreset(QStringLiteral("dotGrid"));
    }
}

void ArtifactLayerMenu::Impl::handleCreateProcedural3D(Procedural3DLayerKind kind)
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }

    const QString baseName = kind == Procedural3DLayerKind::Terrain
        ? QStringLiteral("Terrain 1")
        : QStringLiteral("Path Tube 1");
    ArtifactLayerInitParams params(uniqueLayerName(baseName), LayerType::Procedural3D);
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());

    const auto created = ArtifactLayerSelectionManager::instance()
        ? ArtifactLayerSelectionManager::instance()->currentLayer()
        : ArtifactAbstractLayerPtr{};
    if (const auto layer = std::dynamic_pointer_cast<ArtifactProcedural3DLayer>(created)) {
        layer->loadPreset(kind == Procedural3DLayerKind::Terrain
                              ? QStringLiteral("lowPolyTerrain")
                              : QStringLiteral("neonPathTube"));
    }
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
        QStringLiteral("3D Models (*.obj *.fbx *.gltf *.glb *.stl *.dae *.abc *.usd *.usda *.usdc *.usdz *.pmd *.pmx);;All Files (*.*)"));
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

void ArtifactLayerMenu::Impl::handleCreateBox3D()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    ArtifactFixedGeometry3DLayerInitParams params(uniqueLayerName(QStringLiteral("3D Box 1")),
                                                  FixedGeometry3D::Cube);
    service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
}

void ArtifactLayerMenu::Impl::handleCreateSphere3D()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    ArtifactFixedGeometry3DLayerInitParams params(uniqueLayerName(QStringLiteral("3D Sphere 1")),
                                                  FixedGeometry3D::Sphere);
    service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
}

void ArtifactLayerMenu::Impl::handleCreateCylinder3D()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    ArtifactFixedGeometry3DLayerInitParams params(uniqueLayerName(QStringLiteral("3D Cylinder 1")),
                                                  FixedGeometry3D::Cylinder);
    service->addLayerToCurrentComposition(params, true, placeAtCurrentFrameRequested());
}

void ArtifactLayerMenu::Impl::handleCreateCone3D()
{
    if (!ensureCurrentComposition()) {
        QMessageBox::warning(menu_ ? menu_->window() : nullptr, "Layer", "コンポジションが選択されていません。");
        return;
    }
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }
    ArtifactFixedGeometry3DLayerInitParams params(uniqueLayerName(QStringLiteral("3D Cone 1")),
                                                  FixedGeometry3D::Cone);
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
    auto comp = service->currentComposition().lock();
    if (!comp) return;
    auto layer = comp->layerById(selectedLayerId_);
    if (!layer) return;
    UndoManager::instance()->push(
        std::make_unique<SetLayerVisibilityCommand>(layer, !layer->isVisible()));
}

void ArtifactLayerMenu::Impl::handleToggleLock()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    auto comp = service->currentComposition().lock();
    if (!comp) return;
    auto layer = comp->layerById(selectedLayerId_);
    if (!layer) return;
    UndoManager::instance()->push(
        std::make_unique<SetLayerLockCommand>(layer, !layer->isLocked()));
}

void ArtifactLayerMenu::Impl::handleToggleSolo()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    auto comp = service->currentComposition().lock();
    if (!comp) return;
    auto layer = comp->layerById(selectedLayerId_);
    if (!layer) return;
    UndoManager::instance()->push(
        std::make_unique<SetLayerSoloCommand>(layer, !layer->isSolo()));
}

void ArtifactLayerMenu::Impl::handleToggleShy()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || selectedLayerId_.isNil()) return;
    auto comp = service->currentComposition().lock();
    if (!comp) return;
    auto layer = comp->layerById(selectedLayerId_);
    if (!layer) return;
    UndoManager::instance()->push(
        std::make_unique<SetLayerShyCommand>(layer, !layer->isShy()));
}

void ArtifactLayerMenu::Impl::handleSetLayerCachePolicy(int policy)
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
    layer->setLayerPropertyValue(QStringLiteral("layer.cachePolicy"), policy);
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

    auto* window = qobject_cast<QWidget*>(mainWindow_);
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
    auto* window = qobject_cast<QWidget*>(mainWindow_);
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

    auto* window = qobject_cast<QWidget*>(mainWindow_);
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

    auto* window = qobject_cast<QWidget*>(mainWindow_);
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
    auto* window = qobject_cast<QWidget*>(mainWindow_);
    if (!window) {
        return;
    }
    setDockVisible(window, QStringLiteral("Inspector"), true);
    activateDock(window, QStringLiteral("Inspector"));
}

void ArtifactLayerMenu::Impl::handleOpenProperties()
{
    auto* window = qobject_cast<QWidget*>(mainWindow_);
    if (!window) {
        return;
    }
    setDockVisible(window, QStringLiteral("Properties"), true);
    activateDock(window, QStringLiteral("Properties"));
}

void ArtifactLayerMenu::Impl::handleApplyLipSyncToSwitchLayer()
{
    auto* service = ArtifactProjectService::instance();
    if (!service) {
        return;
    }

    auto comp = service->currentComposition().lock();
    if (!comp) {
        QMessageBox::warning(menu_->window(), QStringLiteral("Lip Sync"),
                             QStringLiteral("コンポジションが選択されていません。"));
        return;
    }

    auto* app = ArtifactApplicationManager::instance();
    auto* selectionManager = app ? app->layerSelectionManager() : nullptr;
    if (!selectionManager) {
        QMessageBox::warning(menu_->window(), QStringLiteral("Lip Sync"),
                             QStringLiteral("選択レイヤーを取得できませんでした。"));
        return;
    }

    ArtifactAbstractLayerPtr audioLayer;
    ArtifactAbstractLayerPtr switchLayer;
    const auto selected = selectionManager->selectedLayers();
    for (const auto& layer : selected) {
        if (!layer) {
            continue;
        }
        if (!audioLayer && std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
            audioLayer = layer;
            continue;
        }
        if (!switchLayer && std::dynamic_pointer_cast<ArtifactSwitchLayer>(layer)) {
            switchLayer = layer;
            continue;
        }
    }

    const auto audio = std::dynamic_pointer_cast<ArtifactAudioLayer>(audioLayer);
    auto switchTargetPtr = std::dynamic_pointer_cast<ArtifactSwitchLayer>(switchLayer);
    if (!audio || !switchTargetPtr) {
        QMessageBox::warning(menu_->window(), QStringLiteral("Lip Sync"),
                             QStringLiteral("音声レイヤーと Switch Layer の両方を選択してください。"));
        return;
    }

    QMessageBox::information(menu_->window(), QStringLiteral("Lip Sync"),
                             QStringLiteral("Lip Sync の適用は現在のビルドでは無効化されています。"));
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

    if (!service->groupSelectedLayersWithUndo(UniString(groupName))) {
        QMessageBox::warning(menu_->window(), "グループ化", "選択レイヤーをグループ化できませんでした。");
    }
}

void ArtifactLayerMenu::Impl::handleUngroup()
{
    auto* service = ArtifactProjectService::instance();
    if (!service || !hasCurrentComposition()) {
        return;
    }

    if (!service->ungroupSelectedGroupWithUndo()) {
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
    auto* svc = ArtifactProjectService::instance();
    if (!svc) {
        QMessageBox::warning(menu_->window(), QStringLiteral("Layer"),
                             QStringLiteral("アクティブコンテキストが利用できません。"));
        return;
    }
    auto comp = svc->currentComposition().lock();
    if (!comp) return;
    svc->splitLayerWithUndo(comp->id(), selectedLayerId_);
    refreshEnabledState();
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
    auto* window = qobject_cast<QWidget*>(menu_ ? menu_->window() : nullptr);
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
                        if (auto *statusBar = window->findChild<QStatusBar *>()) {
                          statusBar->showMessage(status, 1500);
                        }
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

void ArtifactLayerMenu::Impl::handleBringToFront()
{
    auto* sel = ArtifactLayerSelectionManager::instance();
    if (!sel) return;
    auto comp = sel->activeComposition();
    if (!comp) return;
    const auto layers = sel->selectedLayersInOrder();
    if (layers.isEmpty()) return;
    const auto allLayers = comp->allLayerRef();
    // Move from topmost selected first to preserve relative order
    for (int i = layers.size() - 1; i >= 0; --i) {
        auto layer = layers[i];
        if (!layer) continue;
        const int idx = allLayers.indexOf(layer);
        if (idx < 0 || idx >= allLayers.size() - 1) continue;
        UndoManager::instance()->push(std::make_unique<MoveLayerIndexCommand>(comp, layer, idx, allLayers.size() - 1));
    }
}

void ArtifactLayerMenu::Impl::handleBringForward()
{
    auto* sel = ArtifactLayerSelectionManager::instance();
    if (!sel) return;
    auto comp = sel->activeComposition();
    if (!comp) return;
    const auto layers = sel->selectedLayersInOrder();
    if (layers.isEmpty()) return;
    const auto allLayers = comp->allLayerRef();
    // Move from topmost selected first
    for (int i = layers.size() - 1; i >= 0; --i) {
        auto layer = layers[i];
        if (!layer) continue;
        const int idx = allLayers.indexOf(layer);
        if (idx < 0 || idx >= allLayers.size() - 1) continue;
        UndoManager::instance()->push(std::make_unique<MoveLayerIndexCommand>(comp, layer, idx, idx + 1));
    }
}

void ArtifactLayerMenu::Impl::handleSendBackward()
{
    auto* sel = ArtifactLayerSelectionManager::instance();
    if (!sel) return;
    auto comp = sel->activeComposition();
    if (!comp) return;
    const auto layers = sel->selectedLayersInOrder();
    if (layers.isEmpty()) return;
    const auto allLayers = comp->allLayerRef();
    // Move from bottommost selected first
    for (int i = 0; i < layers.size(); ++i) {
        auto layer = layers[i];
        if (!layer) continue;
        const int idx = allLayers.indexOf(layer);
        if (idx <= 0) continue;
        UndoManager::instance()->push(std::make_unique<MoveLayerIndexCommand>(comp, layer, idx, idx - 1));
    }
}

void ArtifactLayerMenu::Impl::handleSendToBack()
{
    auto* sel = ArtifactLayerSelectionManager::instance();
    if (!sel) return;
    auto comp = sel->activeComposition();
    if (!comp) return;
    const auto layers = sel->selectedLayersInOrder();
    if (layers.isEmpty()) return;
    const auto allLayers = comp->allLayerRef();
    // Move from bottommost selected first to preserve relative order
    for (int i = 0; i < layers.size(); ++i) {
        auto layer = layers[i];
        if (!layer) continue;
        const int idx = allLayers.indexOf(layer);
        if (idx <= 0) continue;
        UndoManager::instance()->push(std::make_unique<MoveLayerIndexCommand>(comp, layer, idx, 0));
    }
}

void ArtifactLayerMenu::Impl::handleAlign(ArtifactCore::AlignType type)
{
    auto* sel = ArtifactLayerSelectionManager::instance();
    if (!sel) return;
    auto comp = sel->activeComposition();
    if (!comp) return;
    const auto layers = sel->selectedLayersInOrder();
    if (layers.size() < 2) return;

    // Capture before state
    std::vector<AlignLayerSnapshot> snapshots;
    std::vector<ArtifactCore::AlignmentObject> objects;
    for (int i = 0; i < layers.size(); ++i) {
        auto layer = layers[i];
        if (!layer) continue;
        const float px = layer->transform3D().positionX();
        const float py = layer->transform3D().positionY();
        snapshots.push_back({layer->id().toString(), px, py, px, py});
        ArtifactCore::AlignmentObject obj;
        obj.id = i;
        obj.bounds = layer->transformedBoundingBox();
        obj.currentPosition = QPointF(px, py);
        objects.push_back(obj);
    }
    if (objects.size() < 2) return;

    // Run alignment
    QRectF containerBounds;
    ArtifactCore::LayerAlignment::align(objects, type, ArtifactCore::AlignmentTarget::Selection, containerBounds);

    // Write back and capture after state
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& obj = objects[i];
        auto layer = comp->layerById(ArtifactCore::LayerID(snapshots[i].layerId));
        if (!layer) continue;
        layer->transform3D().setPosition(ArtifactCore::RationalTime(0, 30000),
            static_cast<float>(obj.currentPosition.x()), static_cast<float>(obj.currentPosition.y()));
        layer->changed();
        snapshots[i].afterX = static_cast<float>(obj.currentPosition.x());
        snapshots[i].afterY = static_cast<float>(obj.currentPosition.y());
    }

    UndoManager::instance()->push(std::make_unique<AlignLayersUndoCommand>(snapshots, QStringLiteral("Align Layers")));
}

void ArtifactLayerMenu::Impl::handleDistribute(ArtifactCore::DistributeType type)
{
    auto* sel = ArtifactLayerSelectionManager::instance();
    if (!sel) return;
    auto comp = sel->activeComposition();
    if (!comp) return;
    const auto layers = sel->selectedLayersInOrder();
    if (layers.size() < 3) return;

    std::vector<AlignLayerSnapshot> snapshots;
    std::vector<ArtifactCore::AlignmentObject> objects;
    for (int i = 0; i < layers.size(); ++i) {
        auto layer = layers[i];
        if (!layer) continue;
        const float px = layer->transform3D().positionX();
        const float py = layer->transform3D().positionY();
        snapshots.push_back({layer->id().toString(), px, py, px, py});
        ArtifactCore::AlignmentObject obj;
        obj.id = i;
        obj.bounds = layer->transformedBoundingBox();
        obj.currentPosition = QPointF(px, py);
        objects.push_back(obj);
    }
    if (objects.size() < 3) return;

    ArtifactCore::LayerAlignment::distribute(objects, type);

    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& obj = objects[i];
        auto layer = comp->layerById(ArtifactCore::LayerID(snapshots[i].layerId));
        if (!layer) continue;
        layer->transform3D().setPosition(ArtifactCore::RationalTime(0, 30000),
            static_cast<float>(obj.currentPosition.x()), static_cast<float>(obj.currentPosition.y()));
        layer->changed();
        snapshots[i].afterX = static_cast<float>(obj.currentPosition.x());
        snapshots[i].afterY = static_cast<float>(obj.currentPosition.y());
    }

    UndoManager::instance()->push(std::make_unique<AlignLayersUndoCommand>(snapshots, QStringLiteral("Distribute Layers")));
}

void ArtifactLayerMenu::Impl::handleDistributeSpacing()
{
    auto* sel = ArtifactLayerSelectionManager::instance();
    if (!sel) return;
    auto comp = sel->activeComposition();
    if (!comp) return;
    const auto layers = sel->selectedLayersInOrder();
    if (layers.size() < 3) return;

    std::vector<AlignLayerSnapshot> snapshots;
    std::vector<ArtifactCore::AlignmentObject> objects;
    for (int i = 0; i < layers.size(); ++i) {
        auto layer = layers[i];
        if (!layer) continue;
        const float px = layer->transform3D().positionX();
        const float py = layer->transform3D().positionY();
        snapshots.push_back({layer->id().toString(), px, py, px, py});
        ArtifactCore::AlignmentObject obj;
        obj.id = i;
        obj.bounds = layer->transformedBoundingBox();
        obj.currentPosition = QPointF(px, py);
        objects.push_back(obj);
    }
    if (objects.size() < 3) return;

    QRectF compBounds;
    if (comp->effectiveCompositionSize().isValid()) {
        const auto s = comp->effectiveCompositionSize();
        compBounds = QRectF(0, 0, s.width(), s.height());
    }
    ArtifactCore::LayerAlignment::distributeSpacing(objects, ArtifactCore::DistributeType::CenterHorizontal,
        ArtifactCore::AlignmentTarget::Selection, compBounds);

    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& obj = objects[i];
        auto layer = comp->layerById(ArtifactCore::LayerID(snapshots[i].layerId));
        if (!layer) continue;
        layer->transform3D().setPosition(ArtifactCore::RationalTime(0, 30000),
            static_cast<float>(obj.currentPosition.x()), static_cast<float>(obj.currentPosition.y()));
        layer->changed();
        snapshots[i].afterX = static_cast<float>(obj.currentPosition.x());
        snapshots[i].afterY = static_cast<float>(obj.currentPosition.y());
    }

    UndoManager::instance()->push(std::make_unique<AlignLayersUndoCommand>(snapshots, QStringLiteral("Distribute Spacing")));
}

void ArtifactLayerMenu::Impl::handleRadialTransform()
{
    auto* selection = ArtifactLayerSelectionManager::instance();
    auto* playback = ArtifactPlaybackService::instance();
    if (!selection) {
        return;
    }
    const auto composition = selection->activeComposition();
    if (!composition) {
        return;
    }

    QVector<ArtifactAbstractLayerPtr> layers;
    for (const auto& layer : selection->selectedLayersInOrder()) {
        if (layer && !layer->isTransformLocked()) {
            layers.push_back(layer);
        }
    }
    if (layers.size() < 2) {
        QMessageBox::information(
            menu_->window(), QStringLiteral("放射状変形"),
            QStringLiteral("変形ロックされていないレイヤーを2つ以上選択してください。"));
        return;
    }

    const LayerID commonParentId = layers.front()->parentLayerId();
    for (const auto& layer : layers) {
        if (layer->parentLayerId() != commonParentId) {
            QMessageBox::information(
                menu_->window(), QStringLiteral("放射状変形"),
                QStringLiteral("異なる親を持つレイヤーは座標系が異なるため、同じ親のレイヤーだけを選択してください。"));
            return;
        }
    }

    bool accepted = false;
    const double expansionPercent = QInputDialog::getDouble(
        menu_->window(), QStringLiteral("放射状変形"),
        QStringLiteral("外側レイヤーの移動量 (%)\n"
                       "正数で外側へ、負数で中心へ移動します。"),
        25.0, -90.0, 500.0, 1, &accepted);
    if (!accepted) {
        return;
    }

    const double edgeScalePercent = QInputDialog::getDouble(
        menu_->window(), QStringLiteral("放射状変形"),
        QStringLiteral("最外周のスケール (%)\n"
                       "中心は元のスケールを維持します。"),
        70.0, 1.0, 500.0, 1, &accepted);
    if (!accepted) {
        return;
    }

    QPointF center;
    for (const auto& layer : layers) {
        center += QPointF(
            static_cast<qreal>(layer->transform3D().positionX()),
            static_cast<qreal>(layer->transform3D().positionY()));
    }
    center /= static_cast<qreal>(layers.size());

    double maxDistance = 0.0;
    for (const auto& layer : layers) {
        const QPointF delta(
            static_cast<qreal>(layer->transform3D().positionX()) - center.x(),
            static_cast<qreal>(layer->transform3D().positionY()) - center.y());
        maxDistance = std::max(maxDistance, std::hypot(delta.x(), delta.y()));
    }
    if (maxDistance <= 0.0001) {
        QMessageBox::information(
            menu_->window(), QStringLiteral("放射状変形"),
            QStringLiteral("選択レイヤーが同じ位置にあるため、距離に基づく変形を計算できません。"));
        return;
    }

    const qint64 frame = playback ? playback->currentFrame().framePosition() : 0;
    const qint64 timeScale = std::max<qint64>(
        1, static_cast<qint64>(std::llround(composition->frameRate().framerate())));
    const RationalTime time(frame, timeScale);
    const double expansion = expansionPercent / 100.0;
    const double edgeScale = edgeScalePercent / 100.0;

    std::vector<RadialTransformSnapshot> snapshots;
    snapshots.reserve(static_cast<std::size_t>(layers.size()));
    for (const auto& layer : layers) {
        auto& transform = layer->transform3D();
        const QPointF beforePosition(
            static_cast<qreal>(transform.positionX()),
            static_cast<qreal>(transform.positionY()));
        const QPointF delta = beforePosition - center;
        const double normalizedDistance =
            std::clamp(std::hypot(delta.x(), delta.y()) / maxDistance, 0.0, 1.0);
        const double influence =
            normalizedDistance * normalizedDistance * (3.0 - 2.0 * normalizedDistance);
        const QPointF afterPosition =
            center + delta * (1.0 + expansion * influence);
        const double scaleFactor = std::lerp(1.0, edgeScale, influence);

        RadialTransformSnapshot snapshot;
        snapshot.layer = layer;
        snapshot.beforeX = transform.positionX();
        snapshot.beforeY = transform.positionY();
        snapshot.beforeScaleX = transform.scaleX();
        snapshot.beforeScaleY = transform.scaleY();
        snapshot.afterX = static_cast<float>(afterPosition.x());
        snapshot.afterY = static_cast<float>(afterPosition.y());
        snapshot.afterScaleX =
            static_cast<float>(static_cast<double>(snapshot.beforeScaleX) * scaleFactor);
        snapshot.afterScaleY =
            static_cast<float>(static_cast<double>(snapshot.beforeScaleY) * scaleFactor);
        snapshots.push_back(snapshot);
    }

    if (auto* manager = UndoManager::instance()) {
        manager->push(
            std::make_unique<RadialTransformLayersCommand>(time, std::move(snapshots)));
    }
}

void ArtifactLayerMenu::Impl::handleCreateLiveTransformField(const QString& requestedShape)
{
    const QString normalizedShape = requestedShape.trimmed().toLower();
    const QString shape = normalizedShape == QStringLiteral("box") ||
                                  normalizedShape == QStringLiteral("linear")
                              ? normalizedShape
                              : QStringLiteral("radial");
    const QString dialogTitle = shape == QStringLiteral("box")
                                    ? QStringLiteral("ライブBox Field")
                                    : shape == QStringLiteral("linear")
                                          ? QStringLiteral("ライブLinear Field")
                                          : QStringLiteral("ライブ放射状Field");
    auto* selection = ArtifactLayerSelectionManager::instance();
    if (!selection) {
        return;
    }
    const auto composition = selection->activeComposition();
    if (!composition) {
        return;
    }

    QVector<ArtifactAbstractLayerPtr> layers;
    for (const auto& layer : selection->selectedLayersInOrder()) {
        if (layer && !layer->isTransformLocked()) {
            layers.push_back(layer);
        }
    }
    if (layers.size() < 2) {
        QMessageBox::information(
            menu_->window(), dialogTitle,
            QStringLiteral("変形ロックされていないレイヤーを2つ以上選択してください。"));
        return;
    }

    const LayerID commonParentId = layers.front()->parentLayerId();
    for (const auto& layer : layers) {
        if (layer->parentLayerId() != commonParentId) {
            QMessageBox::information(
                menu_->window(), dialogTitle,
                QStringLiteral("異なる親を持つレイヤーは座標系が異なるため、同じ親のレイヤーだけを選択してください。"));
            return;
        }
    }

    bool accepted = false;
    const double expansionPercent = QInputDialog::getDouble(
        menu_->window(), dialogTitle,
        QStringLiteral("外側レイヤーの移動量 (%)"),
        25.0, -90.0, 500.0, 1, &accepted);
    if (!accepted) {
        return;
    }
    const double edgeScalePercent = QInputDialog::getDouble(
        menu_->window(), dialogTitle,
        QStringLiteral("最外周のスケール (%)"),
        70.0, 1.0, 500.0, 1, &accepted);
    if (!accepted) {
        return;
    }
    const double strength = QInputDialog::getDouble(
        menu_->window(), dialogTitle,
        QStringLiteral("強さ (strength)"),
        1.0, 0.0, 4.0, 2, &accepted);
    if (!accepted) {
        return;
    }

    const auto blendChoices = transformFieldBlendModeChoices();
    const QString blendMode = QInputDialog::getItem(
        menu_->window(), dialogTitle,
        QStringLiteral("Blend mode"), blendChoices, 0, false, &accepted);
    if (!accepted) {
        return;
    }

    const auto invertChoice = QMessageBox::question(
        menu_->window(), dialogTitle,
        QStringLiteral("Field を反転しますか?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    const bool invert = invertChoice == QMessageBox::Yes;

    QPointF center;
    for (const auto& layer : layers) {
        center += QPointF(
            static_cast<qreal>(layer->transform3D().positionX()),
            static_cast<qreal>(layer->transform3D().positionY()));
    }
    center /= static_cast<qreal>(layers.size());

    qreal radius = 0.0;
    qreal secondaryRadius = 0.0;
    CompositionTransformField field;
    field.fieldId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    field.shape = shape;
    field.displayName = shape == QStringLiteral("box")
                            ? QStringLiteral("Box Transform Field")
                            : shape == QStringLiteral("linear")
                                  ? QStringLiteral("Linear Transform Field")
                                  : QStringLiteral("Radial Transform Field");
    field.center = center;
    field.expansion = expansionPercent / 100.0;
    field.edgeScale = edgeScalePercent / 100.0;
    field.strength = strength;
    field.blendMode = blendMode;
    field.invert = invert;
    field.coordinateParentLayerId = commonParentId;
    for (const auto& layer : layers) {
        const QPointF position(
            static_cast<qreal>(layer->transform3D().positionX()),
            static_cast<qreal>(layer->transform3D().positionY()));
        if (shape == QStringLiteral("box") || shape == QStringLiteral("linear")) {
            radius = std::max(radius, std::abs(position.x() - center.x()));
            secondaryRadius =
                std::max(secondaryRadius, std::abs(position.y() - center.y()));
        } else {
            radius = std::max(radius, std::hypot(
                position.x() - center.x(), position.y() - center.y()));
        }
        field.targetLayerIds.append(layer->id());
    }
    if (shape == QStringLiteral("linear") && secondaryRadius > radius) {
        std::swap(radius, secondaryRadius);
        field.rotationDegrees = 90.0;
    }
    if (shape == QStringLiteral("box") || shape == QStringLiteral("linear")) {
        if (radius <= 0.0001 && secondaryRadius > 0.0001) {
            radius = secondaryRadius * 0.5;
        }
        if (secondaryRadius <= 0.0001 && radius > 0.0001) {
            secondaryRadius = radius * 0.5;
        }
    }
    if (radius <= 0.0001 ||
        ((shape == QStringLiteral("box") || shape == QStringLiteral("linear")) &&
         secondaryRadius <= 0.0001)) {
        QMessageBox::information(
            menu_->window(), dialogTitle,
            QStringLiteral("選択レイヤーが同じ位置にあるため、Fieldを作成できません。"));
        return;
    }
    field.radius = radius;
    field.secondaryRadius = shape == QStringLiteral("box") ||
                                    shape == QStringLiteral("linear")
                                ? secondaryRadius
                                : radius;

    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<AddCompositionTransformFieldCommand>(
            ArtifactCompositionWeakPtr(composition), std::move(field)));
    } else {
        composition->addTransformField(field);
        composition->setActiveTransformFieldId(field.fieldId);
    }
}

void ArtifactLayerMenu::Impl::handleSelectLiveRadialField()
{
    const auto composition =
        ArtifactProjectService::instance()
            ? ArtifactProjectService::instance()->currentComposition().lock()
            : ArtifactCompositionPtr{};
    auto selected = chooseTransformField(
        menu_->window(), composition, QStringLiteral("ライブFieldを選択"));
    if (!selected.has_value() || !composition) {
        return;
    }

    const QString beforeFieldId = composition->activeTransformFieldId();
    if (beforeFieldId == selected->fieldId) {
        return;
    }

    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<SetActiveCompositionTransformFieldCommand>(
            ArtifactCompositionWeakPtr(composition), beforeFieldId,
            selected->fieldId));
    } else {
        composition->setActiveTransformFieldId(selected->fieldId);
    }
}

void ArtifactLayerMenu::Impl::handleActivateLiveRadialField(const int direction)
{
    const auto composition =
        ArtifactProjectService::instance()
            ? ArtifactProjectService::instance()->currentComposition().lock()
            : ArtifactCompositionPtr{};
    if (!composition || direction == 0) {
        return;
    }

    const auto fields = composition->transformFields();
    if (fields.size() < 2) {
        return;
    }

    const QString activeFieldId = composition->activeTransformFieldId();
    int activeIndex = -1;
    for (int index = 0; index < fields.size(); ++index) {
        if (fields.at(index).fieldId == activeFieldId) {
            activeIndex = index;
            break;
        }
    }

    if (activeIndex < 0) {
        activeIndex = 0;
    }

    const int count = fields.size();
    const int nextIndex = (activeIndex + direction + count) % count;
    if (nextIndex < 0 || nextIndex >= count) {
        return;
    }

    const QString beforeFieldId = activeFieldId;
    const QString afterFieldId = fields.at(nextIndex).fieldId;
    if (beforeFieldId == afterFieldId) {
        return;
    }

    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<SetActiveCompositionTransformFieldCommand>(
            ArtifactCompositionWeakPtr(composition), beforeFieldId, afterFieldId));
    } else {
        composition->setActiveTransformFieldId(afterFieldId);
    }
}

void ArtifactLayerMenu::Impl::handleActivateLiveFieldById(const QString& fieldId)
{
    const auto composition =
        ArtifactProjectService::instance()
            ? ArtifactProjectService::instance()->currentComposition().lock()
            : ArtifactCompositionPtr{};
    if (!composition || fieldId.trimmed().isEmpty()) {
        return;
    }

    const QString beforeFieldId = composition->activeTransformFieldId();
    const QString afterFieldId = fieldId.trimmed();
    if (beforeFieldId == afterFieldId) {
        return;
    }

    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<SetActiveCompositionTransformFieldCommand>(
            ArtifactCompositionWeakPtr(composition), beforeFieldId,
            afterFieldId));
    } else {
        composition->setActiveTransformFieldId(afterFieldId);
    }
}

void ArtifactLayerMenu::Impl::handleEditLiveRadialField()
{
    const auto composition =
        ArtifactProjectService::instance()
            ? ArtifactProjectService::instance()->currentComposition().lock()
            : ArtifactCompositionPtr{};
    auto selected = activeOrChosenTransformField(
        menu_->window(), composition, QStringLiteral("ライブFieldを編集"));
    if (!selected.has_value()) {
        return;
    }

    CompositionTransformField edited = *selected;
    bool accepted = false;
    edited.radius = QInputDialog::getDouble(
        menu_->window(), QStringLiteral("ライブFieldを編集"),
        edited.shape == QStringLiteral("box")
            ? QStringLiteral("X半径")
            : edited.shape == QStringLiteral("linear")
                  ? QStringLiteral("グラデーション半幅")
                  : QStringLiteral("半径"),
        edited.radius, 0.01, 100000.0, 2, &accepted);
    if (!accepted) {
        return;
    }
    if (edited.shape == QStringLiteral("box") ||
        edited.shape == QStringLiteral("linear")) {
        edited.secondaryRadius = QInputDialog::getDouble(
            menu_->window(), QStringLiteral("ライブFieldを編集"),
            edited.shape == QStringLiteral("box") ? QStringLiteral("Y半径")
                                                    : QStringLiteral("ガイド長"),
            edited.secondaryRadius, 0.01, 100000.0,
            2, &accepted);
        if (!accepted) {
            return;
        }
    }
    if (edited.shape == QStringLiteral("linear")) {
        edited.rotationDegrees = QInputDialog::getDouble(
            menu_->window(), QStringLiteral("ライブFieldを編集"),
            QStringLiteral("方向 (degrees)"), edited.rotationDegrees,
            -360.0, 360.0, 1, &accepted);
        if (!accepted) {
            return;
        }
    }
    edited.expansion = QInputDialog::getDouble(
        menu_->window(), QStringLiteral("ライブFieldを編集"),
        QStringLiteral("外側レイヤーの移動量 (%)"),
        edited.expansion * 100.0, -90.0, 500.0, 1, &accepted) / 100.0;
    if (!accepted) {
        return;
    }
    edited.edgeScale = QInputDialog::getDouble(
        menu_->window(), QStringLiteral("ライブFieldを編集"),
        QStringLiteral("最外周のスケール (%)"),
        edited.edgeScale * 100.0, 1.0, 500.0, 1, &accepted) / 100.0;
    if (!accepted) {
        return;
    }
    edited.timeOffsetSeconds = QInputDialog::getDouble(
        menu_->window(), QStringLiteral("ライブFieldを編集"),
        QStringLiteral("Clone time offset (seconds)"),
        edited.timeOffsetSeconds, -60.0, 60.0, 3, &accepted);
    if (!accepted) {
        return;
    }

    edited.strength = QInputDialog::getDouble(
        menu_->window(), QStringLiteral("ライブFieldを編集"),
        QStringLiteral("強さ (strength)"),
        edited.strength, 0.0, 4.0, 2, &accepted);
    if (!accepted) {
        return;
    }

    const auto blendChoices = transformFieldBlendModeChoices();
    const qsizetype currentBlendIndexValue = std::max<qsizetype>(
        0, blendChoices.indexOf(
               transformFieldBlendModeLabel(edited.blendMode).toLower()));
    const int currentBlendIndex = static_cast<int>(currentBlendIndexValue);
    edited.blendMode = QInputDialog::getItem(
        menu_->window(), QStringLiteral("ライブFieldを編集"),
        QStringLiteral("Blend mode"), blendChoices, currentBlendIndex, false,
        &accepted);
    if (!accepted) {
        return;
    }

    const auto invertChoice = QMessageBox::question(
        menu_->window(), QStringLiteral("ライブFieldを編集"),
        QStringLiteral("Field を反転しますか?"),
        QMessageBox::Yes | QMessageBox::No,
        edited.invert ? QMessageBox::Yes : QMessageBox::No);
    edited.invert = invertChoice == QMessageBox::Yes;

    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<UpdateCompositionTransformFieldCommand>(
            ArtifactCompositionWeakPtr(composition), *selected, std::move(edited)));
    } else {
        composition->addTransformField(std::move(edited));
    }
}

void ArtifactLayerMenu::Impl::handleToggleLiveRadialField()
{
    const auto composition =
        ArtifactProjectService::instance()
            ? ArtifactProjectService::instance()->currentComposition().lock()
            : ArtifactCompositionPtr{};
    auto selected = activeOrChosenTransformField(
        menu_->window(), composition, QStringLiteral("ライブFieldを有効/無効"));
    if (!selected.has_value()) {
        return;
    }
    CompositionTransformField edited = *selected;
    edited.enabled = !edited.enabled;
    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<UpdateCompositionTransformFieldCommand>(
            ArtifactCompositionWeakPtr(composition), *selected, std::move(edited)));
    } else {
        composition->addTransformField(std::move(edited));
    }
}

void ArtifactLayerMenu::Impl::handleMoveActiveLiveRadialField(const int direction)
{
    const auto composition =
        ArtifactProjectService::instance()
            ? ArtifactProjectService::instance()->currentComposition().lock()
            : ArtifactCompositionPtr{};
    if (!composition || direction == 0) {
        return;
    }

    const QString activeFieldId = composition->activeTransformFieldId();
    if (activeFieldId.isEmpty()) {
        QMessageBox::information(
            menu_->window(), QStringLiteral("ライブFieldの順序を変更"),
            QStringLiteral("アクティブな Field がありません。先に Field を選択してください。"));
        return;
    }

    const auto reordered =
        reorderedTransformFieldsForMove(composition, activeFieldId, direction);
    if (!reordered.has_value()) {
        QMessageBox::information(
            menu_->window(), QStringLiteral("ライブFieldの順序を変更"),
            direction < 0
                ? QStringLiteral("これ以上上へ移動できません。")
                : QStringLiteral("これ以上下へ移動できません。"));
        return;
    }

    const auto before = composition->transformFields();
    const auto& after = reordered->first;
    if (before.size() == after.size() &&
        std::equal(before.cbegin(), before.cend(), after.cbegin(),
                   [](const CompositionTransformField& lhs,
                      const CompositionTransformField& rhs) {
                       return lhs.fieldId == rhs.fieldId;
                   })) {
        return;
    }

    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<ReorderCompositionTransformFieldsCommand>(
            ArtifactCompositionWeakPtr(composition), before, after,
            direction < 0 ? QStringLiteral("Move Active Live Field Up")
                          : QStringLiteral("Move Active Live Field Down")));
    } else {
        composition->setTransformFields(after);
    }
}

void ArtifactLayerMenu::Impl::handleMoveLiveRadialField(const int direction)
{
    const auto composition =
        ArtifactProjectService::instance()
            ? ArtifactProjectService::instance()->currentComposition().lock()
            : ArtifactCompositionPtr{};
    auto selected = chooseTransformField(
        menu_->window(), composition, QStringLiteral("ライブFieldの順序を変更"));
    if (!selected.has_value() || !composition) {
        return;
    }

    const auto reordered =
        reorderedTransformFieldsForMove(composition, selected->fieldId, direction);
    if (!reordered.has_value()) {
        QMessageBox::information(
            menu_->window(), QStringLiteral("ライブFieldの順序を変更"),
            direction < 0
                ? QStringLiteral("これ以上上へ移動できません。")
                : QStringLiteral("これ以上下へ移動できません。"));
        return;
    }

    const auto before = composition->transformFields();
    const auto& after = reordered->first;
    const bool sameOrder =
        before.size() == after.size() &&
        std::equal(before.cbegin(), before.cend(), after.cbegin(),
                   [](const CompositionTransformField& lhs,
                      const CompositionTransformField& rhs) {
                       return lhs.fieldId == rhs.fieldId;
                   });
    if (sameOrder) {
        return;
    }

    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<ReorderCompositionTransformFieldsCommand>(
            ArtifactCompositionWeakPtr(composition), before, after,
            direction < 0 ? QStringLiteral("Move Live Field Up")
                          : QStringLiteral("Move Live Field Down")));
    } else {
        composition->setTransformFields(after);
    }
}

void ArtifactLayerMenu::Impl::handleRemoveLiveRadialField()
{
    const auto composition =
        ArtifactProjectService::instance()
            ? ArtifactProjectService::instance()->currentComposition().lock()
            : ArtifactCompositionPtr{};
    auto selected = activeOrChosenTransformField(
        menu_->window(), composition, QStringLiteral("ライブFieldを削除"));
    if (!selected.has_value()) {
        return;
    }
    const QString activeFieldId = composition ? composition->activeTransformFieldId() : QString();
    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<RemoveCompositionTransformFieldCommand>(
            ArtifactCompositionWeakPtr(composition), std::move(*selected),
            activeFieldId));
    } else {
        composition->removeTransformField(selected->fieldId);
    }
}

void ArtifactLayerMenu::Impl::handleAddParametricParameter()
{
    const auto layer = currentSelectedParametricLayer();
    if (!layer) {
        return;
    }

    bool accepted = false;
    const QString key = QInputDialog::getText(
        menu_->window(), QStringLiteral("Parametric Parameter"),
        QStringLiteral("Parameter Key"),
        QLineEdit::Normal, QStringLiteral("control.value"), &accepted).trimmed();
    if (!accepted || key.isEmpty()) {
        return;
    }

    const double defaultValue = QInputDialog::getDouble(
        menu_->window(), QStringLiteral("Parametric Parameter"),
        QStringLiteral("Default Value"), 0.0, -1000000.0, 1000000.0, 3, &accepted);
    if (!accepted) {
        return;
    }

    const auto before = layer->definition();
    auto after = std::make_shared<ParametricCompositionDefinition>(
        before ? *before
               : makeDefaultParametricCompositionDefinition(
                     QStringLiteral("parametric.layer"),
                     QStringLiteral("Parametric Composition")));
    if (after->hasParameter(key)) {
        QMessageBox::warning(
            menu_->window(), QStringLiteral("Parametric Parameter"),
            QStringLiteral("Parameter を追加できませんでした。既に同名の parameter がある可能性があります。"));
        return;
    }

    ParametricCompositionParameter parameter;
    parameter.key = key;
    parameter.displayName = key;
    parameter.defaultValue = QVariant(defaultValue);
    if (!after->addParameter(parameter)) {
        QMessageBox::warning(
            menu_->window(), QStringLiteral("Parametric Parameter"),
            QStringLiteral("Parameter を追加できませんでした。"));
        return;
    }

    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<SetParametricDefinitionCommand>(
            ArtifactAbstractLayerWeak(layer), before, after,
            QStringLiteral("Add Parametric Parameter")));
    } else {
        layer->setDefinition(after);
    }
}

void ArtifactLayerMenu::Impl::handlePublishParametricParameter()
{
    const auto layer = currentSelectedParametricLayer();
    if (!layer || !layer->definition()) {
        return;
    }

    QStringList parameterChoices;
    for (const auto& parameter : layer->definition()->parameters()) {
        parameterChoices.append(parameter.key);
    }
    if (parameterChoices.isEmpty()) {
        QMessageBox::information(
            menu_->window(), QStringLiteral("Published Control"),
            QStringLiteral("公開できる parameter がありません。先に parameter を追加してください。"));
        return;
    }

    bool accepted = false;
    const QString parameterKey = QInputDialog::getItem(
        menu_->window(), QStringLiteral("Published Control"),
        QStringLiteral("Parameter"), parameterChoices, 0, false, &accepted);
    if (!accepted || parameterKey.isEmpty()) {
        return;
    }

    const QString displayName = QInputDialog::getText(
        menu_->window(), QStringLiteral("Published Control"),
        QStringLiteral("Display Name"),
        QLineEdit::Normal, parameterKey, &accepted).trimmed();
    if (!accepted) {
        return;
    }

    const auto before = layer->definition();
    if (!before) {
        return;
    }
    auto after = std::make_shared<ParametricCompositionDefinition>(*before);
    ParametricCompositionPublishedControl control;
    control.sourceParameterKey = parameterKey;
    control.controlId = parameterKey;
    control.displayName = displayName.isEmpty() ? parameterKey : displayName;
    if (const auto* parameter = after->parameter(parameterKey)) {
        control.defaultValue = parameter->defaultValue;
        control.displayName =
            displayName.isEmpty() ? parameter->displayName : displayName;
        control.valueType = QString::fromLatin1(parameter->defaultValue.typeName());
    }
    if (after->hasPublishedControl(control.controlId) ||
        !after->addPublishedControl(control)) {
        QMessageBox::warning(
            menu_->window(), QStringLiteral("Published Control"),
            QStringLiteral("Published Control を作成できませんでした。既に公開済みの可能性があります。"));
        return;
    }

    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<SetParametricDefinitionCommand>(
            ArtifactAbstractLayerWeak(layer), before, after,
            QStringLiteral("Publish Parametric Control")));
    } else {
        layer->setDefinition(after);
    }
}

void ArtifactLayerMenu::Impl::handleEditParametricControl()
{
    const auto layer = currentSelectedParametricLayer();
    if (!layer || !layer->definition()) {
        return;
    }

    const auto& controls = layer->definition()->publishedControls();
    if (controls.isEmpty()) {
        QMessageBox::information(
            menu_->window(), QStringLiteral("Published Control"),
            QStringLiteral("編集できる Published Control がありません。"));
        return;
    }

    QStringList controlChoices;
    controlChoices.reserve(controls.size());
    for (const auto& control : controls) {
        const QString title = control.displayName.trimmed().isEmpty()
                                  ? control.controlId
                                  : control.displayName;
        controlChoices.append(QStringLiteral("%1 (%2)")
                                  .arg(title, control.controlId));
    }

    bool accepted = false;
    const QString selectedChoice = QInputDialog::getItem(
        menu_->window(), QStringLiteral("Published Controlを編集"),
        QStringLiteral("Control"), controlChoices, 0, false, &accepted);
    if (!accepted || selectedChoice.isEmpty()) {
        return;
    }

    const int selectedIndex = controlChoices.indexOf(selectedChoice);
    if (selectedIndex < 0 || selectedIndex >= controls.size()) {
        return;
    }

    const auto before = layer->definition();
    if (!before) {
        return;
    }
    auto after = std::make_shared<ParametricCompositionDefinition>(*before);
    auto selectedControl = controls.at(selectedIndex);

    const QString newDisplayName = QInputDialog::getText(
        menu_->window(), QStringLiteral("Published Controlを編集"),
        QStringLiteral("Display Name"),
        QLineEdit::Normal, selectedControl.displayName, &accepted).trimmed();
    if (!accepted) {
        return;
    }

    const QString newControlId = QInputDialog::getText(
        menu_->window(), QStringLiteral("Published Controlを編集"),
        QStringLiteral("Control ID"),
        QLineEdit::Normal, selectedControl.controlId, &accepted).trimmed();
    if (!accepted || newControlId.isEmpty()) {
        return;
    }

    if (newControlId != selectedControl.controlId &&
        after->hasPublishedControl(newControlId)) {
        QMessageBox::warning(
            menu_->window(), QStringLiteral("Published Control"),
            QStringLiteral("同じ Control ID が既に存在します。"));
        return;
    }

    if (!after->removePublishedControl(selectedControl.controlId)) {
        QMessageBox::warning(
            menu_->window(), QStringLiteral("Published Control"),
            QStringLiteral("Published Control を更新できませんでした。"));
        return;
    }

    selectedControl.displayName =
        newDisplayName.isEmpty() ? selectedControl.displayName : newDisplayName;
    selectedControl.controlId = newControlId;
    if (!after->addPublishedControl(selectedControl)) {
        QMessageBox::warning(
            menu_->window(), QStringLiteral("Published Control"),
            QStringLiteral("Published Control を更新できませんでした。"));
        return;
    }

    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<SetParametricDefinitionCommand>(
            ArtifactAbstractLayerWeak(layer), before, after,
            QStringLiteral("Edit Published Control")));
    } else {
        layer->setDefinition(after);
    }
}

void ArtifactLayerMenu::Impl::handleUnpublishParametricControl()
{
    const auto layer = currentSelectedParametricLayer();
    if (!layer || !layer->definition()) {
        return;
    }

    const auto& controls = layer->definition()->publishedControls();
    if (controls.isEmpty()) {
        QMessageBox::information(
            menu_->window(), QStringLiteral("Published Control"),
            QStringLiteral("解除できる Published Control がありません。"));
        return;
    }

    QStringList controlChoices;
    for (const auto& control : controls) {
        controlChoices.append(control.controlId);
    }

    bool accepted = false;
    const QString controlId = QInputDialog::getItem(
        menu_->window(), QStringLiteral("Published Control を解除"),
        QStringLiteral("Control"), controlChoices, 0, false, &accepted);
    if (!accepted || controlId.isEmpty()) {
        return;
    }

    const auto before = layer->definition();
    if (!before) {
        return;
    }
    auto after = std::make_shared<ParametricCompositionDefinition>(*before);
    if (!after->removePublishedControl(controlId)) {
        QMessageBox::warning(
            menu_->window(), QStringLiteral("Published Control"),
            QStringLiteral("Published Control を解除できませんでした。"));
        return;
    }

    if (auto* manager = UndoManager::instance()) {
        manager->push(std::make_unique<SetParametricDefinitionCommand>(
            ArtifactAbstractLayerWeak(layer), before, after,
            QStringLiteral("Unpublish Parametric Control")));
    } else {
        layer->setDefinition(after);
    }
}

void ArtifactLayerMenu::Impl::handleControllerLearn()
{
    QWidget* root = mainWindow_ ? mainWindow_ : (menu_ ? menu_->window() : nullptr);
    auto* propertyWidget = activePropertyWidget(root);
    if (!propertyWidget) {
        QMessageBox::information(
            root,
            QStringLiteral("Controller Learn"),
            QStringLiteral("フォーカス中の animatable property が見つかりません。"));
        return;
    }

    const auto layer = propertyWidget->activePropertyLayer();
    const QString propertyPath = propertyWidget->activePropertyPath().trimmed();
    if (!layer || propertyPath.isEmpty()) {
        QMessageBox::information(
            root,
            QStringLiteral("Controller Learn"),
            QStringLiteral("Learn 先の property を特定できませんでした。"));
        return;
    }

    auto& controlManager = ArtifactCore::ExternalControlManager::instance();
    QString address = controlManager.lastObservedAddress().trimmed();
    if (address.isEmpty()) {
        bool ok = false;
        address = QInputDialog::getText(
            root,
            QStringLiteral("Controller Learn"),
            QStringLiteral("最後に観測した controller input がありません。address を入力してください。"),
            QLineEdit::Normal,
            QStringLiteral("midi:1:1"),
            &ok).trimmed();
        if (!ok || address.isEmpty()) {
            return;
        }
    }

    controlManager.setMapping(address, layer->id(), propertyPath);
    QMessageBox::information(
        root,
        QStringLiteral("Controller Learn"),
        QStringLiteral("%1 → %2 に割り当てました。")
            .arg(address, propertyPath));
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


module;
#include <utility>
#include <QString>
#include <QStringList>
#include <QVariant>

export module Artifact.Event.Types;

import Playback.State;
import Math.Interpolate;

export namespace Artifact {

class Artifact3DModelViewer;

struct ProjectChangedEvent {
    QString projectId;
    QString projectName;
};

struct ProjectDirtyChangedEvent {
    bool dirty = false;
};

struct ProjectCreatedEvent {
    QString projectId;
    QString projectName;
};

struct CompositionCreatedEvent {
    QString compositionId;
    QString compositionName;
};

struct CreateCompositionRequestedEvent {};

struct OpenRecentProjectRequestedEvent {
    QString path;
};

struct OpenProjectRequestedEvent {};

struct ImportAssetsRequestedEvent {};

struct CompositionRemovedEvent {
    QString compositionId;
};

struct CompositionChangedEvent {
    QString compositionId;
    // Monotonically increasing content revision. Consumers that cache a
    // composition frame can use this directly in their cache key.
    uint64_t revision = 0;
};

struct CompositionNoteChangedEvent {
    QString compositionId;
    QString note;
};

struct CurrentCompositionChangedEvent {
    QString compositionId;
};

struct CompositionThumbnailUpdatedEvent {
    QString compositionId;
};

struct SelectionChangedEvent {
    QStringList selectedItemIds;
    QString currentItemId;
    int selectedCount = 0;
    // Project Manager selection consumers use stable values instead of
    // forwarding QModelIndex/model pointers across widget boundaries.
    QString currentCompositionId;
    QString currentFootagePath;
    QStringList selectedFootagePaths;
};

struct FrameChangedEvent {
    QString compositionId;
    qint64 frame = 0;
};

struct PlaybackStateChangedEvent {
    ArtifactCore::PlaybackState state = ArtifactCore::PlaybackState::Stopped;
};

struct PlaybackSpeedChangedEvent {
    float speed = 1.0f;
};

struct PreviewQualityPresetChangedEvent {
    int preset = 1;
};

struct ModelViewerDisplayModeChangedEvent {
    Artifact3DModelViewer* source = nullptr;
    int mode = 0;
};

struct PlaybackLoopingChangedEvent {
    bool loop = false;
};

struct PlaybackFrameRangeChangedEvent {
    qint64 startFrame = 0;
    qint64 endFrame = 0;
};

struct PlaybackInOutPointsChangedEvent {
    bool hasInPoint = false;
    bool hasOutPoint = false;
};

struct PlaybackCompositionChangedEvent {
    QString compositionId;
};

struct PlaybackShortcutExecutedEvent {
    QString actionId;
};

struct TimelineZoomLevelChangedEvent {
    double zoomPercent = 100.0;
};

struct TimelineTrackRowHeightChangedEvent {
    int rowHeight = 0;
};

struct TimelineKeyframeSelectionChangedEvent {
    int selectedCount = 0;
};

struct TimelineLayerSelectionRequestedEvent {
    QString clipId;
    QString layerId;
};

struct TimelineClipMoveRequestedEvent {
    QString clipId;
    double startFrame = 0.0;
};

struct TimelineClipResizeRequestedEvent {
    QString clipId;
    double startFrame = 0.0;
    double durationFrame = 0.0;
};

struct TimelineClipSlideRequestedEvent {
    QString clipId;
    double startFrame = 0.0;
};

struct TimelineKeyframeMoveRequestedEvent {
    QString layerId;
    QString propertyPath;
    qint64 fromFrame = 0;
    qint64 toFrame = 0;
};

struct CurveEditorInteractionStartedEvent {
};

struct CurveEditorInteractionFinishedEvent {
};

struct CurveEditorCurrentFrameChangedEvent {
    qint64 frame = 0;
};

struct CurveEditorKeySelectedEvent {
    int trackIndex = -1;
    int keyIndex = -1;
};

struct CurveEditorKeyMovedEvent {
    int trackIndex = -1;
    int keyIndex = -1;
    qint64 newFrame = 0;
    float newValue = 0.0f;
};

struct CurveEditorKeyDeletedEvent {
    int trackIndex = -1;
    int keyIndex = -1;
};

struct TimelinePropertyFocusChangedEvent {
    QString compositionId;
    QString layerId;
    QString propertyPath;
};

struct TimelineSeekRequestedEvent {
    double frame = 0.0;
};

struct TimelineScrubStartedEvent {};

struct TimelineScrubFinishedEvent {};

struct TimelineDebugMessageEvent {
    QString message;
};

struct ToolOptionChangedEvent {
    QString toolName;
    QString optionName;
    QVariant value;
};

struct WorkspaceModeChangedEvent {
    int mode = 0;
};

enum class CompositionViewCommandKind {
    Reset,
    ZoomIn,
    ZoomOut,
    ZoomFit,
    Zoom100,
    SetGridVisible,
    SetGuidesVisible,
};

struct CompositionViewCommandRequestedEvent {
    CompositionViewCommandKind kind = CompositionViewCommandKind::Reset;
    bool visible = false;
};

struct AssetBrowserSelectionChangedEvent {
    QStringList selectedFiles;
};

struct AssetBrowserItemDoubleClickedEvent {
    QString itemPath;
};

enum class ProjectItemActivationKind {
    Unknown,
    Composition,
    Footage,
};

struct ProjectItemActivatedEvent {
    ProjectItemActivationKind kind = ProjectItemActivationKind::Unknown;
    QString itemId;
    QString compositionId;
    QString filePath;
};

enum class AIClientChangeKind {
    MessageReceived,
    PartialMessageReceived,
    ErrorOccurred,
    InitializationFinished,
    MessageCancelled
};

struct AIClientChangedEvent {
    AIClientChangeKind kind = AIClientChangeKind::MessageReceived;
    QString text;
    bool success = false;
    QString modelPath;
};

struct PlaybackRamPreviewStateChangedEvent {
    bool enabled = false;
    qint64 startFrame = 0;
    qint64 endFrame = 0;
};

struct PlaybackRamPreviewStatsChangedEvent {
    float hitRate = 0.0f;
    int cachedFrameCount = 0;
};

struct AudioLevelChangedEvent {
    float leftRms = 0.0f;
    float rightRms = 0.0f;
    float leftPeak = 0.0f;
    float rightPeak = 0.0f;
};

struct WorkAreaChangedEvent {
    QString compositionId;
    qint64 startFrame = 0;
    qint64 endFrame = 0;
};

struct TimelineWorkAreaChangeRequestedEvent {
    double start = 0.0;
    double end = 1.0;
};

struct TimelineNavigatorRangeChangedEvent {
    double start = 0.0;
    double end = 1.0;
};

struct TimelineShyChangedEvent {
    bool shy = false;
};

struct TimelineMotionBlurChangedEvent {
    bool enabled = false;
};

struct TimelineFrameBlendingChangedEvent {
    bool enabled = false;
};

struct TimelineGraphEditorToggledEvent {
    bool enabled = false;
};

enum class TimelineKeyframeNavigationKind {
    Next,
    Previous,
    First,
    Last,
};

struct TimelineKeyframeNavigationRequestedEvent {
    TimelineKeyframeNavigationKind kind = TimelineKeyframeNavigationKind::Next;
};

enum class TimelineKeyframeEditCommandKind {
    Add,
    Remove,
    SelectAll,
    Copy,
    Paste,
    ReverseSelected,
    ReverseCurrentLayer,
    ReverseSelectedLayers,
    ReverseComposition,
};

struct TimelineKeyframeEditCommandRequestedEvent {
    TimelineKeyframeEditCommandKind kind = TimelineKeyframeEditCommandKind::Add;
};

enum class TimelineGraphCommandKind {
    ShowEditor,
    ShowValue,
    ShowSpeed,
};

struct TimelineGraphCommandRequestedEvent {
    TimelineGraphCommandKind kind = TimelineGraphCommandKind::ShowEditor;
};

enum class TimelineTimeRemapCommandKind {
    Enable,
    Freeze,
    Reverse,
};

struct TimelineTimeRemapCommandRequestedEvent {
    TimelineTimeRemapCommandKind kind = TimelineTimeRemapCommandKind::Enable;
};

struct TimelineInterpolationCommandRequestedEvent {
    ArtifactCore::InterpolationType type = ArtifactCore::InterpolationType::Linear;
};

struct TimelineVerticalScrollEvent {
    double offset = 0.0;
    QString sourceWidget; // Optional, to prevent echo/feedback loops
};

struct TimelineVisibleRowsChangedEvent {
    // Fired when rows are rebuilt (e.g. unfold, search, layer count changed)
};

struct FontChangedEvent {
    QString fontName;
};

struct ColorSwatchSelectedEvent {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct ColorSwatchChangedEvent {
};

enum class ColorScienceManagerChangeKind {
    SettingsChanged,
    LutChanged,
    CompositionSettingsChanged
};

struct ColorScienceManagerChangedEvent {
    ColorScienceManagerChangeKind kind =
        ColorScienceManagerChangeKind::SettingsChanged;
    QString compositionId;
};

enum class OCIOManagerChangeKind {
    ConfigChanged,
    WorkingSpaceChanged,
    DisplayViewChanged
};

struct OCIOManagerChangedEvent {
    OCIOManagerChangeKind kind = OCIOManagerChangeKind::ConfigChanged;
    QString colorSpace;
    QString display;
    QString view;
};

enum class LayerSelectionChangeReason {
    Unknown,
    UserCleared,
    LayerDeleted,
    CompositionChanged,
    ProjectChanged,
    ProjectClosed,
    InvalidSelection,
    TransientSync,
    ProgrammaticReselect,
    SelectionBridgeSync
};

inline QString layerSelectionChangeReasonToString(LayerSelectionChangeReason reason) {
    switch (reason) {
    case LayerSelectionChangeReason::UserCleared:
        return QStringLiteral("UserCleared");
    case LayerSelectionChangeReason::LayerDeleted:
        return QStringLiteral("LayerDeleted");
    case LayerSelectionChangeReason::CompositionChanged:
        return QStringLiteral("CompositionChanged");
    case LayerSelectionChangeReason::ProjectChanged:
        return QStringLiteral("ProjectChanged");
    case LayerSelectionChangeReason::ProjectClosed:
        return QStringLiteral("ProjectClosed");
    case LayerSelectionChangeReason::InvalidSelection:
        return QStringLiteral("InvalidSelection");
    case LayerSelectionChangeReason::TransientSync:
        return QStringLiteral("TransientSync");
    case LayerSelectionChangeReason::ProgrammaticReselect:
        return QStringLiteral("ProgrammaticReselect");
    case LayerSelectionChangeReason::SelectionBridgeSync:
        return QStringLiteral("SelectionBridgeSync");
    case LayerSelectionChangeReason::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

struct LayerSelectionChangedEvent {
    QString compositionId;
    QString layerId;
    LayerSelectionChangeReason reason = LayerSelectionChangeReason::Unknown;
};

// Internal boundary event emitted by ArtifactLayerSelectionManager. The
// project service translates it into the semantic LayerSelectionChangedEvent
// consumed by widgets and other cross-component clients.
struct LayerSelectionManagerSelectionChangedEvent {
};

struct LayerChangedEvent {
    QString compositionId;
    QString layerId;
    enum class ChangeType { Created, Removed, Modified } changeType;
};

struct LayerNoteChangedEvent {
    QString compositionId;
    QString layerId;
    QString note;
};

enum class EffectServiceChangeKind {
    Added,
    Removed,
    Changed,
    OrderChanged
};

struct EffectServiceChangedEvent {
    EffectServiceChangeKind kind = EffectServiceChangeKind::Changed;
    QString layerId;
    QString effectId;
};

struct RenderQueueChangedEvent {
    int queueCount = 0;
    int selectedIndex = -1;
    QString reason;
};

enum class RenderQueueServiceChangeKind {
    JobAdded,
    JobRemoved,
    JobUpdated,
    JobStatusChanged,
    JobProgressChanged,
    AllJobsCompleted,
    AllJobsRemoved,
    QueueReordered,
    PreviewFrameReady
};

struct RenderQueueServiceChangedEvent {
    RenderQueueServiceChangeKind kind =
        RenderQueueServiceChangeKind::JobUpdated;
    int index = -1;
    int value = 0;
    int secondaryIndex = -1;
};

struct RenderQueueLogEvent {
    QString message;
    int sourceIndex = -1;
    bool alsoHistory = true;
};

enum class UndoManagerChangeKind {
    PropertyChanged,
    AnythingChanged,
    HistoryChanged
};

struct UndoManagerChangedEvent {
    UndoManagerChangeKind kind = UndoManagerChangeKind::HistoryChanged;
    QString effectId;
};

enum class PlaybackRangeMode {
    All,        // 全範囲
    WorkArea,   // ワークエリア (In-Out)
    Selection   // 選択範囲
};

struct PlaybackRangeModeChangedEvent {
    PlaybackRangeMode mode;
};

enum class PlaybackSkipMode {
    None,       // 全フレーム (1)
    Skip1,      // 1フレームおき (2)
    Skip3       // 3フレームおき (4)
};

struct PlaybackSkipModeChangedEvent {
    PlaybackSkipMode mode;
};

struct ClipCopiedEvent {
    QString compositionId;
    QString layerId;
    QString layerName;
    qint64 frame = 0;
    QVariant data;
};

struct ClipCutEvent {
    QString compositionId;
    QString layerId;
    QString layerName;
    qint64 frame = 0;
    QVariant data;
};

struct ClipPasteRequestedEvent {
    QVariant data;
};

struct ShowEffectInspectorRequested {
};

} // namespace Artifact

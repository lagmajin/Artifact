module;
#include <utility>
#include <QString>
#include <QStringList>

export module Artifact.Event.Types;

export import Artifact.Tool.Manager;

import Playback.State;

export namespace Artifact {

struct ProjectChangedEvent {
    QString projectId;
    QString projectName;
};

struct ProjectCreatedEvent {
    QString projectId;
    QString projectName;
};

struct CompositionCreatedEvent {
    QString compositionId;
    QString compositionName;
};

struct CompositionRemovedEvent {
    QString compositionId;
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

struct WorkAreaChangedEvent {
    QString compositionId;
    qint64 startFrame = 0;
    qint64 endFrame = 0;
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

struct LayerChangedEvent {
    QString compositionId;
    QString layerId;
    enum class ChangeType { Created, Removed, Modified } changeType;
};

struct RenderQueueChangedEvent {
    int queueCount = 0;
    int selectedIndex = -1;
    QString reason;
};

struct RenderQueueLogEvent {
    QString message;
    int sourceIndex = -1;
    bool alsoHistory = true;
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

struct ToolChangedEvent {
    ToolType toolType;
};

} // namespace Artifact

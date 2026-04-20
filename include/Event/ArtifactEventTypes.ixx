module;
#include <utility>
#include <QString>
#include <QStringList>

export module Artifact.Event.Types;


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

struct TimelineSearchTextChangedEvent {
    QString text;
};

struct TimelineSearchNextRequestedEvent {
};

struct TimelineSearchPrevRequestedEvent {
};

struct TimelineSearchClearedEvent {
};

struct TimelineNavigatorStartChangedEvent {
    float value = 0.0f;
};

struct TimelineNavigatorEndChangedEvent {
    float value = 0.0f;
};

struct TimelineWorkAreaStartChangedEvent {
    float value = 0.0f;
};

struct TimelineWorkAreaEndChangedEvent {
    float value = 0.0f;
};

struct TimelineScrubFrameChangedEvent {
    qint64 frame = 0;
};

struct TimelineScrubFrameDragStartedEvent {
};

struct TimelineScrubFrameDragFinishedEvent {
};

struct TimelineSeekRequestedEvent {
    double frame = 0.0;
};

struct TimelineClipSelectedEvent {
    QString clipId;
    QString layerId;
};

struct TimelineClipDeselectedEvent {
};

struct TimelineClipMovedEvent {
    QString clipId;
    double startFrame = 0.0;
};

struct TimelineClipResizedEvent {
    QString clipId;
    double startFrame = 0.0;
    double durationFrame = 1.0;
};

struct TimelineKeyframeSelectionChangedEvent {
    int selectedCount = 0;
};

struct TimelineKeyframeMoveRequestedEvent {
    QString layerId;
    QString propertyPath;
    qint64 fromFrame = 0;
    qint64 toFrame = 0;
};

struct TimelineDebugMessageEvent {
    QString message;
};

struct TimelineZoomLevelChangedEvent {
    double zoomPercent = 100.0;
};

struct TimelineVisibleRowsChangedEvent {
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

struct LayerSelectionChangedEvent {
    QString compositionId;
    QString layerId;
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

} // namespace Artifact

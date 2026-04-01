module;
#include <QString>
#include <QStringList>

export module Artifact.Event.Types;

export namespace Artifact {

struct ProjectChangedEvent {
    QString projectId;
    QString projectName;
};

struct CompositionCreatedEvent {
    QString compositionId;
    QString compositionName;
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

struct WorkAreaChangedEvent {
    QString compositionId;
    qint64 startFrame = 0;
    qint64 endFrame = 0;
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

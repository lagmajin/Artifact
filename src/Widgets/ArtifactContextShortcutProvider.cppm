module;
#include <QString>
#include <QList>
#include <QKeySequence>

module Artifact.Widgets.ContextShortcutProvider;

import UI.ShortcutBindings;

namespace Artifact {

QList<ArtifactShortcutHelpEntry> ArtifactContextShortcutProvider::getShortcutsForMode(WorkspaceMode mode) {
    QList<ArtifactShortcutHelpEntry> result;
    const auto &bindings = ArtifactCore::ShortcutBindings::instance();

    auto addEntry = [&](ArtifactCore::ShortcutId id, const QString &category, const QString &desc) {
        ArtifactShortcutHelpEntry entry;
        entry.category = category;
        entry.actionName = ArtifactCore::shortcutDisplayName(id);
        entry.shortcut = bindings.shortcut(id);
        entry.description = desc;
        result.append(entry);
    };

    // Add generic shortcuts
    addEntry(ArtifactCore::ShortcutId::Undo, QStringLiteral("Edit"), QStringLiteral("Undo last action"));
    addEntry(ArtifactCore::ShortcutId::Redo, QStringLiteral("Edit"), QStringLiteral("Redo last undone action"));

    // Add tools based on Workspace Mode
    addEntry(ArtifactCore::ShortcutId::SelectionTool, QStringLiteral("Tools"), QStringLiteral("Select tool"));
    addEntry(ArtifactCore::ShortcutId::HandTool, QStringLiteral("Tools"), QStringLiteral("Hand pan tool"));
    addEntry(ArtifactCore::ShortcutId::ZoomTool, QStringLiteral("Tools"), QStringLiteral("Zoom tool"));
    addEntry(ArtifactCore::ShortcutId::RotateTool, QStringLiteral("Tools"), QStringLiteral("Rotate tool"));

    if (mode == WorkspaceMode::Animation || mode == WorkspaceMode::Default) {
        addEntry(ArtifactCore::ShortcutId::TimelineCopySelectedKeyframes, QStringLiteral("Timeline"), QStringLiteral("Copy selected keyframes"));
        addEntry(ArtifactCore::ShortcutId::TimelinePasteKeyframesAtPlayhead, QStringLiteral("Timeline"), QStringLiteral("Paste keyframes at playhead"));
        addEntry(ArtifactCore::ShortcutId::TimelineSelectAllKeyframes, QStringLiteral("Timeline"), QStringLiteral("Select all keyframes"));
        addEntry(ArtifactCore::ShortcutId::TimelineAddKeyframeAtPlayhead, QStringLiteral("Timeline"), QStringLiteral("Add keyframe at playhead"));
        addEntry(ArtifactCore::ShortcutId::TimelineRemoveKeyframeAtPlayhead, QStringLiteral("Timeline"), QStringLiteral("Remove keyframe at playhead"));
        addEntry(ArtifactCore::ShortcutId::TimelineCleanKeyframes, QStringLiteral("Timeline"), QStringLiteral("Clean duplicate keyframes"));
        addEntry(ArtifactCore::ShortcutId::TimelineJumpToFirstKeyframe, QStringLiteral("Timeline Navigation"), QStringLiteral("Jump to first keyframe"));
        addEntry(ArtifactCore::ShortcutId::TimelineJumpToLastKeyframe, QStringLiteral("Timeline Navigation"), QStringLiteral("Jump to last keyframe"));
        addEntry(ArtifactCore::ShortcutId::TimelineJumpToNextKeyframe, QStringLiteral("Timeline Navigation"), QStringLiteral("Jump to next keyframe"));
        addEntry(ArtifactCore::ShortcutId::TimelineJumpToPreviousKeyframe, QStringLiteral("Timeline Navigation"), QStringLiteral("Jump to previous keyframe"));
    }

    if (mode == WorkspaceMode::Import) {
        addEntry(ArtifactCore::ShortcutId::ImportPlacementConfirm, QStringLiteral("Placement"), QStringLiteral("Confirm placement"));
        addEntry(ArtifactCore::ShortcutId::ImportPlacementCancel, QStringLiteral("Placement"), QStringLiteral("Cancel placement"));
    }

    return result;
}

} // namespace Artifact

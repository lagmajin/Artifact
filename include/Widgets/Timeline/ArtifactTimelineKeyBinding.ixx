module;

#include <QKeyEvent>
#include <QKeySequence>
#include <QString>
#include <QVector>

export module Artifact.Timeline.KeyBinding;

import UI.ShortcutBindings;

export namespace Artifact {

enum class ArtifactTimelineAction {
  None,
  CopySelectedKeyframes,
  PasteKeyframesAtPlayhead,
  SelectAllKeyframes,
  AddKeyframeAtPlayhead,
  RemoveKeyframeAtPlayhead,
  JumpToFirstKeyframe,
  JumpToLastKeyframe,
  JumpToNextKeyframe,
  JumpToPreviousKeyframe,
};

inline ArtifactCore::ShortcutId timelineShortcutIdForAction(ArtifactTimelineAction action)
{
  using ArtifactCore::ShortcutId;
  switch (action) {
  case ArtifactTimelineAction::CopySelectedKeyframes:
    return ShortcutId::TimelineCopySelectedKeyframes;
  case ArtifactTimelineAction::PasteKeyframesAtPlayhead:
    return ShortcutId::TimelinePasteKeyframesAtPlayhead;
  case ArtifactTimelineAction::SelectAllKeyframes:
    return ShortcutId::TimelineSelectAllKeyframes;
  case ArtifactTimelineAction::AddKeyframeAtPlayhead:
    return ShortcutId::TimelineAddKeyframeAtPlayhead;
  case ArtifactTimelineAction::RemoveKeyframeAtPlayhead:
    return ShortcutId::TimelineRemoveKeyframeAtPlayhead;
  case ArtifactTimelineAction::JumpToFirstKeyframe:
    return ShortcutId::TimelineJumpToFirstKeyframe;
  case ArtifactTimelineAction::JumpToLastKeyframe:
    return ShortcutId::TimelineJumpToLastKeyframe;
  case ArtifactTimelineAction::JumpToNextKeyframe:
    return ShortcutId::TimelineJumpToNextKeyframe;
  case ArtifactTimelineAction::JumpToPreviousKeyframe:
    return ShortcutId::TimelineJumpToPreviousKeyframe;
  case ArtifactTimelineAction::None:
    return ShortcutId::Undo;
  }
  return ShortcutId::Undo;
}

inline QVector<ArtifactTimelineAction> allTimelineActions()
{
  return {
      ArtifactTimelineAction::CopySelectedKeyframes,
      ArtifactTimelineAction::PasteKeyframesAtPlayhead,
      ArtifactTimelineAction::SelectAllKeyframes,
      ArtifactTimelineAction::AddKeyframeAtPlayhead,
      ArtifactTimelineAction::RemoveKeyframeAtPlayhead,
      ArtifactTimelineAction::JumpToFirstKeyframe,
      ArtifactTimelineAction::JumpToLastKeyframe,
      ArtifactTimelineAction::JumpToNextKeyframe,
      ArtifactTimelineAction::JumpToPreviousKeyframe,
  };
}

inline QString timelineActionLabel(ArtifactTimelineAction action)
{
  switch (action) {
  case ArtifactTimelineAction::CopySelectedKeyframes:
    return QStringLiteral("Copy Selected Keyframes");
  case ArtifactTimelineAction::PasteKeyframesAtPlayhead:
    return QStringLiteral("Paste Keyframes at Playhead");
  case ArtifactTimelineAction::SelectAllKeyframes:
    return QStringLiteral("Select All Keyframes");
  case ArtifactTimelineAction::AddKeyframeAtPlayhead:
    return QStringLiteral("Add Keyframe at Playhead");
  case ArtifactTimelineAction::RemoveKeyframeAtPlayhead:
    return QStringLiteral("Remove Keyframe at Playhead");
  case ArtifactTimelineAction::JumpToFirstKeyframe:
    return QStringLiteral("Jump to First Keyframe");
  case ArtifactTimelineAction::JumpToLastKeyframe:
    return QStringLiteral("Jump to Last Keyframe");
  case ArtifactTimelineAction::JumpToNextKeyframe:
    return QStringLiteral("Jump to Next Keyframe");
  case ArtifactTimelineAction::JumpToPreviousKeyframe:
    return QStringLiteral("Jump to Previous Keyframe");
  case ArtifactTimelineAction::None:
    return QStringLiteral("None");
  }
  return QStringLiteral("None");
}

inline QKeySequence timelineActionDefaultShortcut(ArtifactTimelineAction action)
{
  if (action == ArtifactTimelineAction::None) {
    return {};
  }
  const auto& shortcuts = ArtifactCore::ShortcutBindings::instance();
  return shortcuts.shortcut(timelineShortcutIdForAction(action));
}

inline ArtifactTimelineAction resolveTimelineAction(const QKeyEvent* event)
{
  if (!event) {
    return ArtifactTimelineAction::None;
  }

  const auto& shortcuts = ArtifactCore::ShortcutBindings::instance();
  for (const auto action : allTimelineActions()) {
    if (shortcuts.matches(event, timelineShortcutIdForAction(action))) {
      return action;
    }
  }

  return ArtifactTimelineAction::None;
}

} // namespace Artifact

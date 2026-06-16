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
  CleanKeyframes,
  JumpToFirstKeyframe,
  JumpToLastKeyframe,
  JumpToNextKeyframe,
  JumpToPreviousKeyframe,
  JumpToInPoint,
  JumpToOutPoint,
  SoloSelected,
  SnapInToStart,
  SnapOutToEnd,
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
  case ArtifactTimelineAction::CleanKeyframes:
    return ShortcutId::TimelineCleanKeyframes;
  case ArtifactTimelineAction::JumpToFirstKeyframe:
    return ShortcutId::TimelineJumpToFirstKeyframe;
  case ArtifactTimelineAction::JumpToLastKeyframe:
    return ShortcutId::TimelineJumpToLastKeyframe;
  case ArtifactTimelineAction::JumpToNextKeyframe:
    return ShortcutId::TimelineJumpToNextKeyframe;
  case ArtifactTimelineAction::JumpToPreviousKeyframe:
    return ShortcutId::TimelineJumpToPreviousKeyframe;
  case ArtifactTimelineAction::JumpToInPoint:
    return ShortcutId::TimelineJumpToInPoint;
  case ArtifactTimelineAction::JumpToOutPoint:
    return ShortcutId::TimelineJumpToOutPoint;
  case ArtifactTimelineAction::SoloSelected:
    return ShortcutId::TimelineSoloSelected;
  case ArtifactTimelineAction::SnapInToStart:
    return ShortcutId::TimelineSnapInToStart;
  case ArtifactTimelineAction::SnapOutToEnd:
    return ShortcutId::TimelineSnapOutToEnd;
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
      ArtifactTimelineAction::CleanKeyframes,
      ArtifactTimelineAction::JumpToFirstKeyframe,
      ArtifactTimelineAction::JumpToLastKeyframe,
      ArtifactTimelineAction::JumpToNextKeyframe,
      ArtifactTimelineAction::JumpToPreviousKeyframe,
      ArtifactTimelineAction::JumpToInPoint,
      ArtifactTimelineAction::JumpToOutPoint,
      ArtifactTimelineAction::SoloSelected,
      ArtifactTimelineAction::SnapInToStart,
      ArtifactTimelineAction::SnapOutToEnd,
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
  case ArtifactTimelineAction::CleanKeyframes:
    return QStringLiteral("Clean Keyframes");
  case ArtifactTimelineAction::JumpToFirstKeyframe:
    return QStringLiteral("Jump to First Keyframe");
  case ArtifactTimelineAction::JumpToLastKeyframe:
    return QStringLiteral("Jump to Last Keyframe");
  case ArtifactTimelineAction::JumpToNextKeyframe:
    return QStringLiteral("Jump to Next Keyframe");
  case ArtifactTimelineAction::JumpToPreviousKeyframe:
    return QStringLiteral("Jump to Previous Keyframe");
  case ArtifactTimelineAction::JumpToInPoint:
    return QStringLiteral("Jump to In Point");
  case ArtifactTimelineAction::JumpToOutPoint:
    return QStringLiteral("Jump to Out Point");
  case ArtifactTimelineAction::SoloSelected:
    return QStringLiteral("Toggle Solo Selected Layer");
  case ArtifactTimelineAction::SnapInToStart:
    return QStringLiteral("Snap Selected Layer In to 0");
  case ArtifactTimelineAction::SnapOutToEnd:
    return QStringLiteral("Snap Selected Layer Out to End");
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

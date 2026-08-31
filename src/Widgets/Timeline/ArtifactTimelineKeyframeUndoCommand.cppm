module;

#include <QString>

#include <functional>
#include <utility>

export module Artifact.Timeline.KeyframeUndoCommand;

import Undo.UndoManager;

export namespace Artifact {

class TimelineKeyframeSnapshotCommand final : public UndoCommand {
public:
  using Operation = std::function<bool()>;

  TimelineKeyframeSnapshotCommand(QString label, Operation redoFunc,
                                  Operation undoFunc)
      : label_(std::move(label)), redoFunc_(std::move(redoFunc)),
        undoFunc_(std::move(undoFunc)) {}

  void undo() override { lastOperationSucceeded_ = undoFunc_ && undoFunc_(); }
  void redo() override { lastOperationSucceeded_ = redoFunc_ && redoFunc_(); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return label_; }

private:
  QString label_;
  Operation redoFunc_;
  Operation undoFunc_;
  bool lastOperationSucceeded_ = false;
};

} // namespace Artifact

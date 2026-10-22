module;

#include <QString>
#include <QVector>

#include <algorithm>
#include <utility>

export module Artifact.Menu.LayerTransformFieldUndoCommands;

import Artifact.Composition.Abstract;
import Undo.UndoManager;

namespace Artifact {
using namespace ArtifactCore;

namespace {

bool compositionTransformFieldsEqual(
    const QVector<CompositionTransformField>& actual,
    const QVector<CompositionTransformField>& expected)
{
    if (actual.size() != expected.size()) {
        return false;
    }
    for (int index = 0; index < actual.size(); ++index) {
        if (actual[index].toJson() != expected[index].toJson()) {
            return false;
        }
    }
    return true;
}

bool compositionHasTransformField(
    const ArtifactCompositionPtr& composition, const QString& fieldId)
{
    if (!composition) {
        return false;
    }
    const auto fields = composition->transformFields();
    return std::any_of(
        fields.cbegin(), fields.cend(),
        [&fieldId](const CompositionTransformField& field) {
            return field.fieldId == fieldId;
        });
}

} // namespace

} // namespace Artifact

export namespace Artifact {
using namespace ArtifactCore;

class AddCompositionTransformFieldCommand final : public UndoCommand {
public:
    AddCompositionTransformFieldCommand(
        ArtifactCompositionWeakPtr composition, CompositionTransformField field)
        : composition_(std::move(composition)), field_(std::move(field))
    {
    }

    void undo() override { lastOperationSucceeded_ = removeField(); }

    void redo() override { lastOperationSucceeded_ = addField(); }

    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }

    QString label() const override
    {
        return QStringLiteral("Create Live Radial Field");
    }

private:
    bool addField()
    {
        const auto composition = composition_.lock();
        if (!composition) return false;
        composition->addTransformField(field_);
        composition->setActiveTransformFieldId(field_.fieldId);
        return compositionHasTransformField(composition, field_.fieldId) &&
               composition->activeTransformFieldId() == field_.fieldId;
    }

    bool removeField()
    {
        const auto composition = composition_.lock();
        if (!composition) return false;
        if (!composition->removeTransformField(field_.fieldId)) return false;
        return !compositionHasTransformField(composition, field_.fieldId);
    }

    ArtifactCompositionWeakPtr composition_;
    CompositionTransformField field_;
    bool lastOperationSucceeded_ = true;
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

    void undo() override { lastOperationSucceeded_ = apply(before_); }

    void redo() override { lastOperationSucceeded_ = apply(after_); }

    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }

    QString label() const override
    {
        return QStringLiteral("Edit Live Radial Field");
    }

private:
    bool apply(const CompositionTransformField& field)
    {
        const auto composition = composition_.lock();
        if (!composition) return false;
        composition->addTransformField(field);
        return compositionHasTransformField(composition, field.fieldId);
    }

    ArtifactCompositionWeakPtr composition_;
    CompositionTransformField before_;
    CompositionTransformField after_;
    bool lastOperationSucceeded_ = true;
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

    void undo() override { lastOperationSucceeded_ = restoreField(); }

    void redo() override { lastOperationSucceeded_ = removeField(); }

    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }

    QString label() const override
    {
        return QStringLiteral("Remove Live Radial Field");
    }

private:
    bool restoreField()
    {
        const auto composition = composition_.lock();
        if (!composition) return false;
        composition->addTransformField(field_);
        if (activeFieldIdBefore_ == field_.fieldId) {
            composition->setActiveTransformFieldId(activeFieldIdBefore_);
        }
        return compositionHasTransformField(composition, field_.fieldId) &&
               (activeFieldIdBefore_ != field_.fieldId ||
                composition->activeTransformFieldId() == activeFieldIdBefore_);
    }

    bool removeField()
    {
        const auto composition = composition_.lock();
        if (!composition) return false;
        if (!composition->removeTransformField(field_.fieldId)) return false;
        if (activeFieldIdBefore_ == field_.fieldId) {
            composition->setActiveTransformFieldId(QString());
        }
        return !compositionHasTransformField(composition, field_.fieldId) &&
               (activeFieldIdBefore_ != field_.fieldId ||
                composition->activeTransformFieldId().isEmpty());
    }

    ArtifactCompositionWeakPtr composition_;
    CompositionTransformField field_;
    QString activeFieldIdBefore_;
    bool lastOperationSucceeded_ = true;
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

    void undo() override { lastOperationSucceeded_ = apply(before_); }

    void redo() override { lastOperationSucceeded_ = apply(after_); }

    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }

    QString label() const override { return label_; }

private:
    bool apply(const QVector<CompositionTransformField>& fields)
    {
        const auto composition = composition_.lock();
        if (!composition) return false;
        composition->setTransformFields(fields);
        if (!compositionTransformFieldsEqual(composition->transformFields(), fields)) {
            return false;
        }
        if (auto* mgr = UndoManager::instance()) {
            mgr->notifyAnythingChanged();
        }
        return true;
    }

    ArtifactCompositionWeakPtr composition_;
    QVector<CompositionTransformField> before_;
    QVector<CompositionTransformField> after_;
    QString label_;
    bool lastOperationSucceeded_ = true;
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

    void undo() override { lastOperationSucceeded_ = apply(beforeFieldId_); }

    void redo() override { lastOperationSucceeded_ = apply(afterFieldId_); }

    bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }

    QString label() const override
    {
        return QStringLiteral("Activate Live Field");
    }

private:
    bool apply(const QString& fieldId)
    {
        const auto composition = composition_.lock();
        if (!composition) return false;
        composition->setActiveTransformFieldId(fieldId);
        return composition->activeTransformFieldId() == fieldId;
    }

    ArtifactCompositionWeakPtr composition_;
    QString beforeFieldId_;
    QString afterFieldId_;
    bool lastOperationSucceeded_ = true;
};

} // namespace Artifact

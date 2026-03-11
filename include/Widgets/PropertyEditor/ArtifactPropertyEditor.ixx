module;

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVariant>

export module Artifact.Widgets.PropertyEditor;

import std;

import Property.Abstract;

export namespace Artifact {

class ArtifactAbstractPropertyEditor : public QWidget {
public:
    using CommitHandler = std::function<void(const QVariant&)>;

    explicit ArtifactAbstractPropertyEditor(QWidget* parent = nullptr);
    ~ArtifactAbstractPropertyEditor() override;

    void setCommitHandler(CommitHandler handler);

protected:
    void commitValue(const QVariant& value) const;

private:
    CommitHandler commitHandler_;
};

class ArtifactFloatPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactFloatPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
};

class ArtifactIntPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactIntPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
};

class ArtifactBoolPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactBoolPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
};

class ArtifactStringPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactStringPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
};

class ArtifactColorPropertyEditor final : public ArtifactAbstractPropertyEditor {
public:
    explicit ArtifactColorPropertyEditor(const ArtifactCore::AbstractProperty& property, QWidget* parent = nullptr);
};

class ArtifactPropertyEditorRowWidget final : public QWidget {
public:
    explicit ArtifactPropertyEditorRowWidget(
        const QString& labelText,
        ArtifactAbstractPropertyEditor* editor,
        const QString& propertyName,
        QWidget* parent = nullptr);
    ~ArtifactPropertyEditorRowWidget() override;

    QLabel* label() const;
    ArtifactAbstractPropertyEditor* editor() const;
    void setExpressionHandler(std::function<void()> handler);
    void setEditorToolTip(const QString& tooltip);
    void setShowExpressionButton(bool visible);

private:
    QLabel* label_ = nullptr;
    ArtifactAbstractPropertyEditor* editor_ = nullptr;
    QPushButton* expressionButton_ = nullptr;
    std::function<void()> expressionHandler_;
};

ArtifactAbstractPropertyEditor* createPropertyEditorWidget(
    const ArtifactCore::AbstractProperty& property,
    QWidget* parent = nullptr);

} // namespace Artifact

module;
#include <QWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <wobjectdefs.h>

export module Artifact.Widgets.TemplateLockEditor;

import Composition.TemplateLock;

W_REGISTER_ARGTYPE(ArtifactCore::TemplateLockSchema)

export class TemplateLockEditorWidget : public QWidget {
    W_OBJECT(TemplateLockEditorWidget)
public:
    explicit TemplateLockEditorWidget(QWidget* parent = nullptr);

    void setSchema(const ArtifactCore::TemplateLockSchema& schema);
    ArtifactCore::TemplateLockSchema schema() const;

    void loadFromJson(const QJsonObject& obj);
    QJsonObject toJson() const;

public:
    void schemaChanged(const ArtifactCore::TemplateLockSchema& schema)
    W_SIGNAL(schemaChanged, schema)

private:
    void setupUi();
    void rebuildRegionList();
    void rebuildFieldList();

    ArtifactCore::TemplateLockSchema schema_;

    QLineEdit* templateIdEdit_;
    QLineEdit* templateNameEdit_;
    QCheckBox* enabledCheck_;
    QTreeWidget* regionTree_;
    QTreeWidget* fieldTree_;
    QPushButton* addRegionButton_;
    QPushButton* removeRegionButton_;
    QPushButton* addFieldButton_;
    QPushButton* removeFieldButton_;
};

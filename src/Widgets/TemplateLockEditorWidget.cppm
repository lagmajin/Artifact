module;
#include <QHeaderView>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QGroupBox>
#include <QFormLayout>
#include <QFont>
#include <QLabel>
#include <QMessageBox>
#include <wobjectimpl.h>

module Artifact.Widgets.TemplateLockEditor;

W_OBJECT_IMPL(TemplateLockEditorWidget)

namespace {

QString lockScopeText(ArtifactCore::LockScope scope) {
    switch (scope) {
    case ArtifactCore::LockScope::Layout: return QStringLiteral("Layout");
    case ArtifactCore::LockScope::ExportSettings: return QStringLiteral("Export");
    case ArtifactCore::LockScope::CompositionStructure: return QStringLiteral("Structure");
    case ArtifactCore::LockScope::LayerProperties: return QStringLiteral("Layer");
    case ArtifactCore::LockScope::Effects: return QStringLiteral("Effects");
    case ArtifactCore::LockScope::All: return QStringLiteral("All");
    default: return QStringLiteral("Unknown");
    }
}

QString editabilityText(ArtifactCore::Editability edit) {
    switch (edit) {
    case ArtifactCore::Editability::Locked: return QStringLiteral("Locked");
    case ArtifactCore::Editability::Editable: return QStringLiteral("Editable");
    case ArtifactCore::Editability::EditableWithWarning: return QStringLiteral("Editable*");
    default: return QStringLiteral("Unknown");
    }
}

}

TemplateLockEditorWidget::TemplateLockEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void TemplateLockEditorWidget::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto* titleLabel = new QLabel(QStringLiteral("Template Lock Editor"));
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto* infoGroup = new QGroupBox(QStringLiteral("Template Info"));
    auto* infoLayout = new QFormLayout(infoGroup);
    templateIdEdit_ = new QLineEdit;
    templateIdEdit_->setPlaceholderText(QStringLiteral("template.id"));
    infoLayout->addRow(QStringLiteral("Template ID:"), templateIdEdit_);

    templateNameEdit_ = new QLineEdit;
    templateNameEdit_->setPlaceholderText(QStringLiteral("Template Name"));
    infoLayout->addRow(QStringLiteral("Name:"), templateNameEdit_);

    enabledCheck_ = new QCheckBox(QStringLiteral("Enable template locking"));
    enabledCheck_->setChecked(true);
    infoLayout->addRow(QString(), enabledCheck_);
    layout->addWidget(infoGroup);

    auto* regionGroup = new QGroupBox(QStringLiteral("Protected Regions"));
    auto* regionLayout = new QVBoxLayout(regionGroup);

    regionTree_ = new QTreeWidget;
    regionTree_->setColumnCount(4);
    regionTree_->setHeaderLabels({
        QStringLiteral("Region ID"),
        QStringLiteral("Display Name"),
        QStringLiteral("Scope"),
        QStringLiteral("Editability")
    });
    regionTree_->setRootIsDecorated(false);
    regionTree_->setAlternatingRowColors(true);
    regionTree_->header()->setStretchLastSection(true);
    regionLayout->addWidget(regionTree_, 1);

    auto* regionBtnLayout = new QHBoxLayout;
    addRegionButton_ = new QPushButton(QStringLiteral("Add Region"));
    removeRegionButton_ = new QPushButton(QStringLiteral("Remove"));
    regionBtnLayout->addWidget(addRegionButton_);
    regionBtnLayout->addWidget(removeRegionButton_);
    regionBtnLayout->addStretch();
    regionLayout->addLayout(regionBtnLayout);
    layout->addWidget(regionGroup);

    auto* fieldGroup = new QGroupBox(QStringLiteral("Editable Fields"));
    auto* fieldLayout = new QVBoxLayout(fieldGroup);

    fieldTree_ = new QTreeWidget;
    fieldTree_->setColumnCount(3);
    fieldTree_->setHeaderLabels({
        QStringLiteral("Field ID"),
        QStringLiteral("Display Name"),
        QStringLiteral("Slot ID")
    });
    fieldTree_->setRootIsDecorated(false);
    fieldTree_->setAlternatingRowColors(true);
    fieldTree_->header()->setStretchLastSection(true);
    fieldLayout->addWidget(fieldTree_, 1);

    auto* fieldBtnLayout = new QHBoxLayout;
    addFieldButton_ = new QPushButton(QStringLiteral("Add Field"));
    removeFieldButton_ = new QPushButton(QStringLiteral("Remove"));
    fieldBtnLayout->addWidget(addFieldButton_);
    fieldBtnLayout->addWidget(removeFieldButton_);
    fieldBtnLayout->addStretch();
    fieldLayout->addLayout(fieldBtnLayout);
    layout->addWidget(fieldGroup);

    connect(templateIdEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        schema_.templateId = text;
        Q_EMIT schemaChanged(schema_);
    });
    connect(templateNameEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        schema_.templateName = text;
        Q_EMIT schemaChanged(schema_);
    });
    connect(enabledCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        schema_.isEnabled = checked;
        Q_EMIT schemaChanged(schema_);
    });
    connect(addRegionButton_, &QPushButton::clicked, this, [this]() {
        ArtifactCore::ProtectedRegion region;
        region.id = QStringLiteral("region.%1").arg(schema_.protectedRegions.size() + 1);
        region.displayName = QStringLiteral("New Region");
        region.scope = ArtifactCore::LockScope::Layout;
        region.editability = ArtifactCore::Editability::Locked;
        schema_.protectedRegions.append(region);
        rebuildRegionList();
        Q_EMIT schemaChanged(schema_);
    });
    connect(removeRegionButton_, &QPushButton::clicked, this, [this]() {
        int idx = regionTree_->currentIndex().row();
        if (idx >= 0 && idx < schema_.protectedRegions.size()) {
            schema_.protectedRegions.removeAt(idx);
            rebuildRegionList();
            Q_EMIT schemaChanged(schema_);
        }
    });
    connect(addFieldButton_, &QPushButton::clicked, this, [this]() {
        ArtifactCore::EditableField field;
        field.fieldId = QStringLiteral("field.%1").arg(schema_.editableFields.size() + 1);
        field.displayName = QStringLiteral("New Field");
        field.allowedValueType = QStringLiteral("Text");
        schema_.editableFields.append(field);
        rebuildFieldList();
        Q_EMIT schemaChanged(schema_);
    });
    connect(removeFieldButton_, &QPushButton::clicked, this, [this]() {
        int idx = fieldTree_->currentIndex().row();
        if (idx >= 0 && idx < schema_.editableFields.size()) {
            schema_.editableFields.removeAt(idx);
            rebuildFieldList();
            Q_EMIT schemaChanged(schema_);
        }
    });
}

void TemplateLockEditorWidget::rebuildRegionList()
{
    regionTree_->clear();
    for (const auto& r : schema_.protectedRegions) {
        auto* item = new QTreeWidgetItem(regionTree_);
        item->setText(0, r.id);
        item->setText(1, r.displayName);
        item->setText(2, lockScopeText(r.scope));
        item->setText(3, editabilityText(r.editability));
    }
}

void TemplateLockEditorWidget::rebuildFieldList()
{
    fieldTree_->clear();
    for (const auto& f : schema_.editableFields) {
        auto* item = new QTreeWidgetItem(fieldTree_);
        item->setText(0, f.fieldId);
        item->setText(1, f.displayName);
        item->setText(2, f.slotId);
    }
}

void TemplateLockEditorWidget::setSchema(const ArtifactCore::TemplateLockSchema& schema)
{
    schema_ = schema;
    templateIdEdit_->setText(schema_.templateId);
    templateNameEdit_->setText(schema_.templateName);
    enabledCheck_->setChecked(schema_.isEnabled);
    rebuildRegionList();
    rebuildFieldList();
}

ArtifactCore::TemplateLockSchema TemplateLockEditorWidget::schema() const
{
    return schema_;
}

void TemplateLockEditorWidget::loadFromJson(const QJsonObject& obj)
{
    setSchema(ArtifactCore::TemplateLockSchema::fromJson(obj));
}

QJsonObject TemplateLockEditorWidget::toJson() const
{
    return schema_.toJson();
}

module;
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <wobjectimpl.h>

module Artifact.Widgets.TemplateParameters;

namespace Artifact {

W_OBJECT_IMPL(ArtifactTemplateParametersWidget)

ArtifactTemplateParametersWidget::ArtifactTemplateParametersWidget(QWidget* parent)
    : QWidget(parent), tree_(new QTreeWidget(this)) {
    tree_->setColumnCount(4);
    tree_->setHeaderLabels({QStringLiteral("ID"), QStringLiteral("Label"),
                            QStringLiteral("Type"), QStringLiteral("Default")});
    tree_->setRootIsDecorated(false);
    tree_->setAlternatingRowColors(true);
    tree_->setEditTriggers(QAbstractItemView::DoubleClicked |
                           QAbstractItemView::EditKeyPressed |
                           QAbstractItemView::SelectedClicked);
    tree_->header()->setStretchLastSection(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(tree_);
}

void ArtifactTemplateParametersWidget::setParameters(const QJsonArray& parameters) {
    document_.exposedParameters = parameters;
    tree_->clear();
    for (const auto& value : parameters) {
        if (!value.isObject()) continue;
        const auto object = value.toObject();
        auto* item = new QTreeWidgetItem(tree_);
        item->setText(0, object.value(QStringLiteral("id")).toString(
                          object.value(QStringLiteral("path")).toString()));
        item->setText(1, object.value(QStringLiteral("label")).toString());
        item->setText(2, object.value(QStringLiteral("type")).toString());
        const auto defaultValue = object.value(QStringLiteral("defaultValue"));
        item->setText(3, defaultValue.isUndefined()
                           ? QString{}
                           : QString::fromUtf8(QJsonDocument(
                                 QJsonArray{defaultValue}).toJson(QJsonDocument::Compact))
                                 .mid(1).chopped(1));
        item->setData(0, Qt::UserRole, object);
    }
    tree_->resizeColumnToContents(0);
    tree_->resizeColumnToContents(1);
    tree_->resizeColumnToContents(2);
}

QJsonArray ArtifactTemplateParametersWidget::parameters() const {
    QJsonArray result;
    for (int row = 0; row < tree_->topLevelItemCount(); ++row) {
        const auto* item = tree_->topLevelItem(row);
        QJsonObject object = item->data(0, Qt::UserRole).toJsonObject();
        object.insert(QStringLiteral("id"), item->text(0));
        object.insert(QStringLiteral("label"), item->text(1));
        object.insert(QStringLiteral("type"), item->text(2));
        const QJsonDocument parsed = QJsonDocument::fromJson(
            item->text(3).toUtf8());
        object.insert(QStringLiteral("defaultValue"), parsed.isNull()
                                                           ? QJsonValue(item->text(3))
                                                           : (parsed.isArray()
                                                                  ? parsed.array().at(0)
                                                                  : QJsonValue(item->text(3))));
        result.append(object);
    }
    return result;
}

void ArtifactTemplateParametersWidget::setDocument(
    const ArtifactTemplateDocument& document) {
    document_ = document;
    setParameters(document_.exposedParameters);
}

ArtifactTemplateDocument ArtifactTemplateParametersWidget::document() const {
    ArtifactTemplateDocument result = document_;
    result.exposedParameters = parameters();
    return result;
}

} // namespace Artifact

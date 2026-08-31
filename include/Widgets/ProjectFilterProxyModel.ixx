module;

#include <QModelIndex>
#include <QObject>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <QVariant>

export module Artifact.Widgets.ProjectFilterProxyModel;

import Artifact.Project;
import Artifact.Layer.Search.Query;

export namespace Artifact {

class ProjectFilterProxyModel : public QSortFilterProxyModel {
public:
    explicit ProjectFilterProxyModel(QObject* parent = nullptr);

    void setSearchQuery(const ArtifactLayerSearchQuery& query);
    void setUnusedAssetPaths(const QSet<QString>& unusedPaths);
    const QSet<QString>& unusedAssetPaths() const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setAdvancedFilter(const QString& expression,
                           const QString& typeFilter,
                           bool unusedOnly);
    int visibleRowCount() const;

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex& sourceParent) const override;

private:
    int countAcceptedRows(const QModelIndex& sourceParent) const;
    bool matchesLegacyQuery(int sourceRow,
                            const QModelIndex& sourceParent) const;
    void parseExpression();
    bool typeMatches(eProjectItemType itemType, const ProjectItem* item) const;
    bool matchesAdvanced(const QModelIndex& index,
                         eProjectItemType itemType,
                         ProjectItem* item) const;

    ArtifactLayerSearchQuery query_;
    QSet<QString> unusedAssetPaths_;
    QString rawExpression_;
    QString typeFilter_;
    bool unusedOnly_ = false;
    bool usedOnly_ = false;
    bool missingOnly_ = false;
    bool regexEnabled_ = false;
    QString regexPattern_;
    QStringList plainTerms_;
    QStringList tagTerms_;
};

}

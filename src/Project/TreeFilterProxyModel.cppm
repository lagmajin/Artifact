module;
#include <utility>
#include <QSortFilterProxyModel>
#include <Qt>
module Project.TreeFilterProxyModel;



namespace Artifact
{
















 TreeFilterProxyModel::TreeFilterProxyModel()
 {
  setDynamicSortFilter(true);
  setFilterCaseSensitivity(Qt::CaseInsensitive);
  setSortCaseSensitivity(Qt::CaseInsensitive);
  setRecursiveFilteringEnabled(true);
  setFilterKeyColumn(-1);

 }

 TreeFilterProxyModel::~TreeFilterProxyModel()
 {

 }

}

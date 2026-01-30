module;
#include <QString>
#include <memory>

export module Artifact.Layer.Search.Query;

import std;

export namespace Artifact {

 // レイヤータイプの定義
 enum class LayerSearchType {
  Any, Solid, Adjustment, ShapeLayer, TextLayer, CameraLayer, LightLayer
 };

 // 検索フィルターオプション
 enum class SearchFilterOption {
  CaseSensitive = 1 << 0,
  RegexPattern = 1 << 1,
  WholeWord = 1 << 2,
  Visible = 1 << 3,
  Locked = 1 << 4,
  Selected = 1 << 5
 };

 // ソート順序とフィールド
 enum class SortOrder { Ascending, Descending };
 enum class SortField { Name, Type, CreatedTime, ModifiedTime, Index };

 class ArtifactLayerSearchQuery {
 private:
  struct Data {
   QString searchText;
   int layerTypeFilter = 0;
   int filterOptions = 0;
   bool visibilityFilter = false;
   bool lockFilter = false;
   bool selectionFilter = false;
   int sortField = 0;
   int sortOrder = 0;
  };
  std::shared_ptr<Data> data_;

 public:
  ArtifactLayerSearchQuery() : data_(std::make_shared<Data>()) {}
  ~ArtifactLayerSearchQuery() = default;
  ArtifactLayerSearchQuery(const ArtifactLayerSearchQuery&) = default;
  ArtifactLayerSearchQuery(ArtifactLayerSearchQuery&&) noexcept = default;
  ArtifactLayerSearchQuery& operator=(const ArtifactLayerSearchQuery&) = default;
  ArtifactLayerSearchQuery& operator=(ArtifactLayerSearchQuery&&) noexcept = default;

  void setSearchText(const QString& text) { if (data_) data_->searchText = text; }
  QString getSearchText() const { return data_ ? data_->searchText : QString(); }

  void setLayerType(LayerSearchType type) { if (data_) data_->layerTypeFilter = static_cast<int>(type); }
  LayerSearchType getLayerType() const { return data_ ? static_cast<LayerSearchType>(data_->layerTypeFilter) : LayerSearchType::Any; }

  void setLayerTypes(const std::vector<LayerSearchType>& types) {
   if (!data_) return;
   data_->layerTypeFilter = 0;
   for (auto t : types) data_->layerTypeFilter |= static_cast<int>(t);
  }

  std::vector<LayerSearchType> getLayerTypes() const {
   std::vector<LayerSearchType> result;
   if (!data_) return result;
   for (int i = 0; i < 7; ++i) {
    if (data_->layerTypeFilter & (1 << i)) result.push_back(static_cast<LayerSearchType>(i));
   }
   return result;
  }

  void setFilterOptions(int opts) { if (data_) data_->filterOptions = opts; }
  int getFilterOptions() const { return data_ ? data_->filterOptions : 0; }
  bool hasFilterOption(SearchFilterOption opt) const { return data_ && (data_->filterOptions & static_cast<int>(opt)) != 0; }
  void addFilterOption(SearchFilterOption opt) { if (data_) data_->filterOptions |= static_cast<int>(opt); }
  void removeFilterOption(SearchFilterOption opt) { if (data_) data_->filterOptions &= ~static_cast<int>(opt); }

  void setVisibilityFilter(bool v) { if (data_) data_->visibilityFilter = v; }
  bool getVisibilityFilter() const { return data_ && data_->visibilityFilter; }

  void setLockFilter(bool v) { if (data_) data_->lockFilter = v; }
  bool getLockFilter() const { return data_ && data_->lockFilter; }

  void setSelectionFilter(bool v) { if (data_) data_->selectionFilter = v; }
  bool getSelectionFilter() const { return data_ && data_->selectionFilter; }

  void setSortField(SortField f) { if (data_) data_->sortField = static_cast<int>(f); }
  SortField getSortField() const { return data_ ? static_cast<SortField>(data_->sortField) : SortField::Name; }

  void setSortOrder(SortOrder o) { if (data_) data_->sortOrder = static_cast<int>(o); }
  SortOrder getSortOrder() const { return data_ ? static_cast<SortOrder>(data_->sortOrder) : SortOrder::Ascending; }

  void addSortField(SortField f, SortOrder o) {}
  void clearSortFields() { if (data_) { data_->sortField = 0; data_->sortOrder = 0; } }

  void reset() {
   if (data_) {
    data_->searchText.clear();
    data_->layerTypeFilter = 0;
    data_->filterOptions = 0;
    data_->visibilityFilter = false;
    data_->lockFilter = false;
    data_->selectionFilter = false;
    data_->sortField = 0;
    data_->sortOrder = 0;
   }
  }

  bool isValid() const { return data_ && (!data_->searchText.isEmpty() || data_->layerTypeFilter != 0); }
  bool isSearchTextEmpty() const { return !data_ || data_->searchText.isEmpty(); }
  int getActiveFilterCount() const {
   if (!data_) return 0;
   int cnt = 0;
   if (!data_->searchText.isEmpty()) cnt++;
   if (data_->layerTypeFilter != 0) cnt++;
   if (data_->visibilityFilter) cnt++;
   if (data_->lockFilter) cnt++;
   if (data_->selectionFilter) cnt++;
   return cnt;
  }

  bool matchesName(const QString& name) const {
   if (!data_ || data_->searchText.isEmpty()) return true;
   bool cs = (data_->filterOptions & static_cast<int>(SearchFilterOption::CaseSensitive)) != 0;
   bool whole = (data_->filterOptions & static_cast<int>(SearchFilterOption::WholeWord)) != 0;
   Qt::CaseSensitivity sensitivity = cs ? Qt::CaseSensitive : Qt::CaseInsensitive;
   return whole ? (name.compare(data_->searchText, sensitivity) == 0) : name.contains(data_->searchText, sensitivity);
  }

  bool matchesType(LayerSearchType type) const {
   if (!data_ || data_->layerTypeFilter == 0) return true;
   return (data_->layerTypeFilter & static_cast<int>(type)) != 0;
  }

  bool matches(const QString& name, LayerSearchType type, bool visible, bool locked, bool selected) const {
   if (!data_) return false;
   if (!matchesName(name)) return false;
   if (!matchesType(type)) return false;
   if (data_->visibilityFilter && !visible) return false;
   if (data_->lockFilter && locked) return false;
   if (data_->selectionFilter && !selected) return false;
   return true;
  }

  QString toString() const {
   if (!data_) return QString();
   QString res;
   if (!data_->searchText.isEmpty()) res += QString("Search: \"%1\"").arg(data_->searchText);
   if (data_->layerTypeFilter != 0) { if (!res.isEmpty()) res += ", "; res += "Type filter"; }
   if (data_->visibilityFilter) { if (!res.isEmpty()) res += ", "; res += "Visible"; }
   if (data_->lockFilter) { if (!res.isEmpty()) res += ", "; res += "Unlocked"; }
   if (data_->selectionFilter) { if (!res.isEmpty()) res += ", "; res += "Selected"; }
   return res;
  }

  bool operator==(const ArtifactLayerSearchQuery& other) const {
   if (!data_ || !other.data_) return data_ == other.data_;
   return data_->searchText == other.data_->searchText &&
          data_->layerTypeFilter == other.data_->layerTypeFilter &&
          data_->filterOptions == other.data_->filterOptions;
  }

  bool operator!=(const ArtifactLayerSearchQuery& other) const { return !(*this == other); }
 };

};
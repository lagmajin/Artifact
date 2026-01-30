module;
#include <QString>
#include <QSize>
#include <memory>

export module Artifact.Composition.FindQuery;

import std;

export namespace Artifact {

 // 検索フィルターオプション
 enum class CompositionSearchOption {
  CaseSensitive = 1 << 0,    // 大文字小文字を区別
  RegexPattern  = 1 << 1,    // 正規表現を使用
  WholeWord     = 1 << 2,    // 単語全体で検索
  ExactMatch    = 1 << 3     // 完全一致
 };

 // 解像度範囲
 struct ResolutionRange {
  int minWidth = 0;
  int maxWidth = 30000;
  int minHeight = 0;
  int maxHeight = 30000;
  
  bool matches(int width, int height) const {
   return width >= minWidth && width <= maxWidth &&
          height >= minHeight && height <= maxHeight;
  }
 };

 // フレームレート範囲
 struct FrameRateRange {
  double minFps = 0.0;
  double maxFps = 999.0;
  
  bool matches(double fps) const {
   return fps >= minFps && fps <= maxFps;
  }
 };

 // デュレーション範囲（秒）
 struct DurationRange {
  double minSeconds = 0.0;
  double maxSeconds = 10800.0;  // 3時間
  
  bool matches(double seconds) const {
   return seconds >= minSeconds && seconds <= maxSeconds;
  }
 };

 // ソート順序とフィールド
 enum class CompositionSortOrder { Ascending, Descending };
 enum class CompositionSortField {
  Name,           // 名前
  CreatedTime,    // 作成日時
  ModifiedTime,   // 更新日時
  Width,          // 幅
  Height,         // 高さ
  FrameRate,      // フレームレート
  Duration        // デュレーション
 };

 class ArtifactFindCompositionQuery {
 private:
  struct Data {
   QString searchText;
   int filterOptions = 0;
   ResolutionRange resolutionRange;
   FrameRateRange frameRateRange;
   DurationRange durationRange;
   QSize exactResolution = QSize(-1, -1);
   double exactFrameRate = -1.0;
   int sortField = 0;
   int sortOrder = 0;
   bool enableResolutionFilter = false;
   bool enableFrameRateFilter = false;
   bool enableDurationFilter = false;
  };
  std::shared_ptr<Data> data_;

 public:
  ArtifactFindCompositionQuery() : data_(std::make_shared<Data>()) {}
  ~ArtifactFindCompositionQuery() = default;
  ArtifactFindCompositionQuery(const ArtifactFindCompositionQuery&) = default;
  ArtifactFindCompositionQuery(ArtifactFindCompositionQuery&&) noexcept = default;
  ArtifactFindCompositionQuery& operator=(const ArtifactFindCompositionQuery&) = default;
  ArtifactFindCompositionQuery& operator=(ArtifactFindCompositionQuery&&) noexcept = default;

  // ---- 検索テキスト ----

  void setSearchText(const QString& text) {
   if (data_) data_->searchText = text;
  }

  QString getSearchText() const {
   return data_ ? data_->searchText : QString();
  }

  // ---- フィルターオプション ----

  void setFilterOptions(int options) {
   if (data_) data_->filterOptions = options;
  }

  int getFilterOptions() const {
   return data_ ? data_->filterOptions : 0;
  }

  bool hasFilterOption(CompositionSearchOption option) const {
   return data_ && (data_->filterOptions & static_cast<int>(option)) != 0;
  }

  void addFilterOption(CompositionSearchOption option) {
   if (data_) data_->filterOptions |= static_cast<int>(option);
  }

  void removeFilterOption(CompositionSearchOption option) {
   if (data_) data_->filterOptions &= ~static_cast<int>(option);
  }

  // ---- 解像度フィルター ----

  void setResolutionRange(const ResolutionRange& range) {
   if (data_) {
    data_->resolutionRange = range;
    data_->enableResolutionFilter = true;
   }
  }

  ResolutionRange getResolutionRange() const {
   return data_ ? data_->resolutionRange : ResolutionRange();
  }

  void setExactResolution(const QSize& size) {
   if (data_) {
    data_->exactResolution = size;
    data_->enableResolutionFilter = true;
   }
  }

  QSize getExactResolution() const {
   return data_ ? data_->exactResolution : QSize(-1, -1);
  }

  void clearResolutionFilter() {
   if (data_) {
    data_->enableResolutionFilter = false;
    data_->exactResolution = QSize(-1, -1);
   }
  }

  // ---- フレームレートフィルター ----

  void setFrameRateRange(const FrameRateRange& range) {
   if (data_) {
    data_->frameRateRange = range;
    data_->enableFrameRateFilter = true;
   }
  }

  FrameRateRange getFrameRateRange() const {
   return data_ ? data_->frameRateRange : FrameRateRange();
  }

  void setExactFrameRate(double fps) {
   if (data_) {
    data_->exactFrameRate = fps;
    data_->enableFrameRateFilter = true;
   }
  }

  double getExactFrameRate() const {
   return data_ ? data_->exactFrameRate : -1.0;
  }

  void clearFrameRateFilter() {
   if (data_) {
    data_->enableFrameRateFilter = false;
    data_->exactFrameRate = -1.0;
   }
  }

  // ---- デュレーションフィルター ----

  void setDurationRange(const DurationRange& range) {
   if (data_) {
    data_->durationRange = range;
    data_->enableDurationFilter = true;
   }
  }

  DurationRange getDurationRange() const {
   return data_ ? data_->durationRange : DurationRange();
  }

  void clearDurationFilter() {
   if (data_) data_->enableDurationFilter = false;
  }

  // ---- ソート設定 ----

  void setSortField(CompositionSortField field) {
   if (data_) data_->sortField = static_cast<int>(field);
  }

  CompositionSortField getSortField() const {
   return data_ ? static_cast<CompositionSortField>(data_->sortField) : CompositionSortField::Name;
  }

  void setSortOrder(CompositionSortOrder order) {
   if (data_) data_->sortOrder = static_cast<int>(order);
  }

  CompositionSortOrder getSortOrder() const {
   return data_ ? static_cast<CompositionSortOrder>(data_->sortOrder) : CompositionSortOrder::Ascending;
  }

  // ---- クエリ実行 ----

  void reset() {
   if (data_) {
    data_->searchText.clear();
    data_->filterOptions = 0;
    data_->resolutionRange = ResolutionRange();
    data_->frameRateRange = FrameRateRange();
    data_->durationRange = DurationRange();
    data_->exactResolution = QSize(-1, -1);
    data_->exactFrameRate = -1.0;
    data_->sortField = 0;
    data_->sortOrder = 0;
    data_->enableResolutionFilter = false;
    data_->enableFrameRateFilter = false;
    data_->enableDurationFilter = false;
   }
  }

  bool isValid() const {
   return data_ && (!data_->searchText.isEmpty() ||
                    data_->enableResolutionFilter ||
                    data_->enableFrameRateFilter ||
                    data_->enableDurationFilter);
  }

  bool isSearchTextEmpty() const {
   return !data_ || data_->searchText.isEmpty();
  }

  int getActiveFilterCount() const {
   if (!data_) return 0;
   int count = 0;
   if (!data_->searchText.isEmpty()) count++;
   if (data_->enableResolutionFilter) count++;
   if (data_->enableFrameRateFilter) count++;
   if (data_->enableDurationFilter) count++;
   return count;
  }

  // ---- マッチング判定 ----

  bool matchesName(const QString& name) const {
   if (!data_ || data_->searchText.isEmpty()) return true;

   bool caseSensitive = (data_->filterOptions & static_cast<int>(CompositionSearchOption::CaseSensitive)) != 0;
   bool wholeWord = (data_->filterOptions & static_cast<int>(CompositionSearchOption::WholeWord)) != 0;
   bool exactMatch = (data_->filterOptions & static_cast<int>(CompositionSearchOption::ExactMatch)) != 0;

   Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

   if (exactMatch || wholeWord) {
    return name.compare(data_->searchText, cs) == 0;
   } else {
    return name.contains(data_->searchText, cs);
   }
  }

  bool matchesResolution(int width, int height) const {
   if (!data_ || !data_->enableResolutionFilter) return true;

   // 完全一致チェック
   if (data_->exactResolution.width() > 0 && data_->exactResolution.height() > 0) {
    return width == data_->exactResolution.width() && height == data_->exactResolution.height();
   }

   // 範囲チェック
   return data_->resolutionRange.matches(width, height);
  }

  bool matchesFrameRate(double fps) const {
   if (!data_ || !data_->enableFrameRateFilter) return true;

   // 完全一致チェック
   if (data_->exactFrameRate > 0.0) {
    return std::abs(fps - data_->exactFrameRate) < 0.001;
   }

   // 範囲チェック
   return data_->frameRateRange.matches(fps);
  }

  bool matchesDuration(double seconds) const {
   if (!data_ || !data_->enableDurationFilter) return true;
   return data_->durationRange.matches(seconds);
  }

  bool matches(const QString& name, int width, int height, double fps, double duration) const {
   if (!data_) return false;
   if (!matchesName(name)) return false;
   if (!matchesResolution(width, height)) return false;
   if (!matchesFrameRate(fps)) return false;
   if (!matchesDuration(duration)) return false;
   return true;
  }

  // ---- ユーティリティ ----

  QString toString() const {
   if (!data_) return QString();

   QString result;

   if (!data_->searchText.isEmpty()) {
    result += QString("Search: \"%1\"").arg(data_->searchText);
   }

   if (data_->enableResolutionFilter) {
    if (!result.isEmpty()) result += ", ";
    if (data_->exactResolution.width() > 0) {
     result += QString("Resolution: %1x%2")
               .arg(data_->exactResolution.width())
               .arg(data_->exactResolution.height());
    } else {
     result += QString("Resolution: %1-%2 x %3-%4")
               .arg(data_->resolutionRange.minWidth)
               .arg(data_->resolutionRange.maxWidth)
               .arg(data_->resolutionRange.minHeight)
               .arg(data_->resolutionRange.maxHeight);
    }
   }

   if (data_->enableFrameRateFilter) {
    if (!result.isEmpty()) result += ", ";
    if (data_->exactFrameRate > 0.0) {
     result += QString("FPS: %1").arg(data_->exactFrameRate);
    } else {
     result += QString("FPS: %1-%2")
               .arg(data_->frameRateRange.minFps)
               .arg(data_->frameRateRange.maxFps);
    }
   }

   if (data_->enableDurationFilter) {
    if (!result.isEmpty()) result += ", ";
    result += QString("Duration: %1-%2s")
              .arg(data_->durationRange.minSeconds)
              .arg(data_->durationRange.maxSeconds);
   }

   return result;
  }

  bool operator==(const ArtifactFindCompositionQuery& other) const {
   if (!data_ || !other.data_) return data_ == other.data_;

   return data_->searchText == other.data_->searchText &&
          data_->filterOptions == other.data_->filterOptions &&
          data_->enableResolutionFilter == other.data_->enableResolutionFilter &&
          data_->enableFrameRateFilter == other.data_->enableFrameRateFilter &&
          data_->enableDurationFilter == other.data_->enableDurationFilter;
  }

  bool operator!=(const ArtifactFindCompositionQuery& other) const {
   return !(*this == other);
  }
 };

};
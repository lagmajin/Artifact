module;

#include <QtWidgets/QStatusBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtGui/QContextMenuEvent>
#include <array>
#include <cstdint>

export module ArtifactStatusBar;

export namespace Artifact
{
 class ArtifactStatusBar : public QStatusBar
 {
 public:
  enum class Item
  {
   Zoom = 0,
   Coordinates,
   Frame,
   FPS,
   Memory,
   Project,
   Layer,
   Drops,
   TimelineDebug,
   Console
  };

  explicit ArtifactStatusBar(QWidget* parent = nullptr);
  ~ArtifactStatusBar() override;

  void setZoomPercent(float zoomPercent);
  void setCoordinates(int x, int y);
  void setFPS(double fps);
  void setFrame(int64_t frame);
  void setMemoryMB(quint64 memoryMB);
  void setProjectText(const QString& text);
  void setLayerText(const QString& text);
  void setDropSummaryText(const QString& text);
  void setTimelineDebugText(const QString& text);
  void setConsoleSummary(int errors, int warnings);

  void setItemVisible(Item item, bool visible);
  [[nodiscard]] bool isItemVisible(Item item) const;
  void setAllItemsVisible(bool visible);

  void showReadyMessage();

 protected:
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  static constexpr int kItemCount = 10;
  static int itemIndex(Item item);
  QLabel* itemLabel(Item item) const;
  QString itemTitle(Item item) const;
  void rebuildVisibilityMenu(QMenu& menu);

 private:
  std::array<QLabel*, kItemCount> labels_ {};
 };
}

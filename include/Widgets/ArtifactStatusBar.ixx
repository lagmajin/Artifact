module;
#include <utility>
#include <array>
#include <cstdint>
#include <vector>

#include <QtWidgets/QStatusBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtGui/QContextMenuEvent>
export module ArtifactStatusBar;

import Event.Bus;


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
   Selection,
   Drops,
   TimelineDebug,
   Console,
   Accessibility
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
  void setSelectionCount(int count);
  void setDropSummaryText(const QString& text);
  void setTimelineDebugText(const QString& text);
  void setConsoleSummary(int errors, int warnings);
  void setAccessibilityText(const QString& text);
  void setCompositionInfo(const QString& name, int width, int height, double fps);

  void setItemVisible(Item item, bool visible);
  [[nodiscard]] bool isItemVisible(Item item) const;
  void setAllItemsVisible(bool visible);

  void showReadyMessage();

 protected:
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  static constexpr int kItemCount = 12;
  static int itemIndex(Item item);
  QLabel* itemLabel(Item item) const;
  QString itemTitle(Item item) const;
  void rebuildVisibilityMenu(QMenu& menu);

  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

 private:
  std::array<QLabel*, kItemCount> labels_ {};
 };
}

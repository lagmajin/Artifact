module;
#include <utility>
#include <wobjectdefs.h>
#include <QWidget>
#include <QVector>
#include <QWheelEvent>

export module Artifact.Widgets.LayerPanelWidget;

import Utils.Id;

export namespace Artifact
{
 using namespace ArtifactCore;

 enum class SearchMatchMode
 {
  AllVisible,
  HighlightOnly,
  FilterOnly
 };

 enum class TimelineLayerDisplayMode
 {
  AllLayers,
  SelectedOnly,
  AnimatedOnly,
  ImportantAndKeyframed,
  AudioOnly,
  VideoOnly
 };
	
   class ArtifactLayerPanelHeaderWidget :public QWidget
   {
    W_OBJECT(ArtifactLayerPanelHeaderWidget)
   private:
    class Impl;
    Impl* impl_;
  protected:
   void mousePressEvent(QMouseEvent* event) override;
   void mouseMoveEvent(QMouseEvent* event) override;
   void mouseReleaseEvent(QMouseEvent* event) override;
   void leaveEvent(QEvent* event) override;

   public:
    explicit ArtifactLayerPanelHeaderWidget(QWidget* parent = nullptr);
    ~ArtifactLayerPanelHeaderWidget();

    void setPropertyColumnWidth(int width);
    int propertyColumnWidth() const;

    // Get button dimensions for layout synchronization
    int buttonSize() const;  // Returns fixed button size (e.g., 28)
    int iconSize() const;    // Returns icon size used in buttons (e.g., 16)
    int totalHeaderHeight() const;  // Returns total header height

   public /*signals*/:
   void lockClicked() W_SIGNAL(lockClicked);
   void soloClicked() W_SIGNAL(soloClicked);
   void shyToggled(bool hidden) W_SIGNAL(shyToggled, hidden);
   void propertyColumnWidthChanged(int width) W_SIGNAL(propertyColumnWidthChanged, width);
   };


  class ArtifactLayerPanelWidget :public QWidget
  {
   W_OBJECT(ArtifactLayerPanelWidget)
  private:
   class Impl;
   Impl* impl_;
  protected:
   void mousePressEvent(QMouseEvent* event) override;
   void mouseDoubleClickEvent(QMouseEvent* event) override;
   void mouseMoveEvent(QMouseEvent* event) override;
   void mouseReleaseEvent(QMouseEvent* event) override;
   void keyPressEvent(QKeyEvent* event) override;
   void wheelEvent(QWheelEvent* event) override;
   void leaveEvent(QEvent* event) override;
   void paintEvent(QPaintEvent* event) override;
   void dragEnterEvent(class QDragEnterEvent* event) override;
   void dragMoveEvent(class QDragMoveEvent* event) override;
   void dropEvent(class QDropEvent* event) override;
   void dragLeaveEvent(class QDragLeaveEvent* event) override;
  public:
  explicit ArtifactLayerPanelWidget(QWidget* parent = nullptr);

  ~ArtifactLayerPanelWidget();
 
  // Set the composition to display layers for
  void setComposition(const CompositionID& id);
  void setShyHidden(bool hidden);
  void setDisplayMode(TimelineLayerDisplayMode mode);
  TimelineLayerDisplayMode displayMode() const;
  void setRowHeight(int rowHeight);
  int rowHeight() const;
  void setPropertyColumnWidth(int width);
  int propertyColumnWidth() const;
  void setVerticalOffset(double offset);
  double verticalOffset() const;
  void setFilterText(const QString& text);
  void setSearchMatchMode(SearchMatchMode mode);
  SearchMatchMode searchMatchMode() const;
  void setLayerNameEditable(bool enabled);
  bool isLayerNameEditable() const;
  void updateLayout();
  QVector<LayerID> matchingTimelineRows() const;
  QVector<LayerID> visibleTimelineRows() const;
  int layerRowIndex(const LayerID& id) const;
  void editLayerName(const LayerID& id);
  void scrollToLayer(const LayerID& id);

 private:
  void performUpdateLayout();

 public /*signals*/:
  void visibleRowsChanged() W_SIGNAL(visibleRowsChanged);
  void verticalOffsetChanged(double offset) W_SIGNAL(verticalOffsetChanged, offset);

 };

 class ArtifactLayerTimelinePanelWrapper :public QWidget
 {
  W_OBJECT(ArtifactLayerTimelinePanelWrapper)
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  explicit ArtifactLayerTimelinePanelWrapper(QWidget* parent = nullptr);
  ArtifactLayerTimelinePanelWrapper(const CompositionID& id, QWidget* parent = nullptr);
  ~ArtifactLayerTimelinePanelWrapper();
  void setComposition(const CompositionID& id);
  void setFilterText(const QString& text);
  void setSearchMatchMode(SearchMatchMode mode);
  SearchMatchMode searchMatchMode() const;
  void setDisplayMode(TimelineLayerDisplayMode mode);
  TimelineLayerDisplayMode displayMode() const;
  void setRowHeight(int rowHeight);
  int rowHeight() const;
  void setPropertyColumnWidth(int width);
  int propertyColumnWidth() const;
  void setVerticalOffset(double offset);
  double verticalOffset() const;
  QVector<LayerID> matchingTimelineRows() const;
  QVector<LayerID> visibleTimelineRows() const;
  void setLayerNameEditable(bool enabled);
  bool isLayerNameEditable() const;
  void scrollToLayer(const LayerID& id);

 public /*signals*/:
  void visibleRowsChanged() W_SIGNAL(visibleRowsChanged);
  void verticalOffsetChanged(double offset) W_SIGNAL(verticalOffsetChanged, offset);
	
 };

};

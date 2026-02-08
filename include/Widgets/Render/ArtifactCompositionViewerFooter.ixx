module;
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Widgets.CompositionFooter;

export namespace Artifact {


 class ArtifactCompositionViewerFooter :public QWidget{
 	W_OBJECT(ArtifactCompositionViewerFooter)
 private:
  class Impl;
  Impl* impl_;
 protected:
 	
 public:
  explicit ArtifactCompositionViewerFooter(QWidget* parent = nullptr);
  ~ArtifactCompositionViewerFooter();
  
  // Status display methods (stubs for future implementation)
  void setZoomLevel(float zoomPercent) {}
  void setMouseCoordinates(int x, int y) {}
  void setFPS(double fps) {}
  void setMemoryUsage(uint64_t memoryMB) {}
  void setSelectedLayerInfo(const QString& layerInfo) {}
  void setResolutionInfo(uint32_t width, uint32_t height) {}
  
 public/*signal*/:
  void takeSnapShotRequested() W_SIGNAL(takeSnapShotRequested);
  void zoomInRequested() W_SIGNAL(zoomInRequested);
  void zoomOutRequested() W_SIGNAL(zoomOutRequested);
  void zoomFitRequested() W_SIGNAL(zoomFitRequested);
  void zoom100Requested() W_SIGNAL(zoom100Requested);
 };


}
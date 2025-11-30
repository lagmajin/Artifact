
module;
#include <wobjectdefs.h>
#include <QObject>
#include <QWidget>
export module Artifact.Widgets.RenderQueueJobPanel;

export namespace Artifact
{
 class ColumnWidthManager : public QObject {
  W_OBJECT(ColumnWidthManager)

 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ColumnWidthManager(QObject* parent = nullptr);
  ~ColumnWidthManager();

 };
 
	class RenderQueueManagerJobDetailWidget :public QWidget {
  W_OBJECT(RenderQueueManagerJobDetailWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
  explicit RenderQueueManagerJobDetailWidget(QWidget* parent = nullptr);
  ~RenderQueueManagerJobDetailWidget();
 };

	
 class RenderQueueJobWidget :public QWidget
 {
 	W_OBJECT(RenderQueueJobWidget)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit RenderQueueJobWidget(QWidget* parent = nullptr);
  ~RenderQueueJobWidget();
 };











};
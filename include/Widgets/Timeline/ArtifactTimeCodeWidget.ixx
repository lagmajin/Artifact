module;
#include <utility>
#include <wobjectdefs.h>
#include <QEvent>
#include <QObject>
#include <QWidget>

export module Artifact.Timeline.TimeCodeWidget;

import Event.Bus;

export namespace Artifact
{
 class ArtifactTimeCodeWidget :public QWidget {
  W_OBJECT(ArtifactTimeCodeWidget)
 private:
  class Impl;
  Impl* impl_;
 protected:

 public:
 explicit ArtifactTimeCodeWidget(QWidget* parent = nullptr);
  ~ArtifactTimeCodeWidget();

  void updateTimeCode(int frame);

 protected:
  void paintEvent(QPaintEvent* event) override;
 };
 
 class ArtifactTimelineSearchBarWidget :public QWidget {
  W_OBJECT(ArtifactTimelineSearchBarWidget)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactTimelineSearchBarWidget(QWidget* parent = nullptr);
  ~ArtifactTimelineSearchBarWidget();

  void focusSearch();
  void clearSearch();
  bool hasSearchText() const;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 public:
  void setEventBus(ArtifactCore::EventBus* eventBus);
 };

};

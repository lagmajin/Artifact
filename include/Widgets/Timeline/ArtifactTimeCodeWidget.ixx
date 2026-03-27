module;
#include <QEvent>
#include <QObject>
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Timeline.TimeCodeWidget;

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
 };
 
 class ArtifactTimelineSearchBarWidget :public QWidget {
  W_OBJECT(ArtifactTimelineSearchBarWidget)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactTimelineSearchBarWidget(QWidget* parent = nullptr);
  ~ArtifactTimelineSearchBarWidget();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 public:
  void searchTextChanged(const QString& text) W_SIGNAL(searchTextChanged, text);
  void searchNextRequested() W_SIGNAL(searchNextRequested);
  void searchPrevRequested() W_SIGNAL(searchPrevRequested);
  void searchCleared() W_SIGNAL(searchCleared);
 };

};

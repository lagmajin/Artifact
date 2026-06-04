module;
#include <utility>
#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QRect>
#include <QString>
#include <QWidget>
#include <wobjectimpl.h>

module Menu.Time;
import std;

import Event.Bus;
import Artifact.Event.Types;
import Artifact.Service.Playback;
import Artifact.MainWindow;
import Artifact.Widgets.PlaybackControlWidget;
import Utils.Path;

namespace Artifact {

namespace {
ArtifactMainWindow* activeMainWindow(QWidget* widget)
{
 return qobject_cast<ArtifactMainWindow*>(widget ? widget->window() : nullptr);
}

ArtifactPlaybackService* playbackService()
{
 return ArtifactPlaybackService::instance();
}
}

W_OBJECT_IMPL(ArtifactTimeMenu)

class ArtifactTimeMenu::Impl {
public:
  explicit Impl(ArtifactTimeMenu* menu);
  ~Impl() = default;

  ArtifactTimeMenu* menu_ = nullptr;
  QAction* showControlAction = nullptr;
  QAction* playPauseAction = nullptr;
  QAction* stopAction = nullptr;
  QAction* stepBackwardAction = nullptr;
  QAction* stepForwardAction = nullptr;
  QAction* seekStartAction = nullptr;
  QAction* seekEndAction = nullptr;
  QAction* prevMarkerAction = nullptr;
  QAction* nextMarkerAction = nullptr;
  QAction* setInPointAction = nullptr;
  QAction* setOutPointAction = nullptr;
  QAction* clearInOutAction = nullptr;
  QAction* loopAction = nullptr;
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  void handleProjectOpened();
  void handleCompositionOpened();
  void handleCompositionClosed();
  void handleProjectClosed();
  void showPlaybackControl();
  void refreshState();
};

ArtifactTimeMenu::Impl::Impl(ArtifactTimeMenu* menu)
  : menu_(menu)
{
  menu_->setSeparatorsCollapsible(true);
  menu_->setMinimumWidth(220);

  showControlAction = menu_->addAction("Playback Control...");
  showControlAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_control.svg")));
  showControlAction->setStatusTip(QStringLiteral("Open the playback control panel."));

  playPauseAction = menu_->addAction("再生");
  playPauseAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_play.svg")));
  playPauseAction->setStatusTip(QStringLiteral("Space toggles playback. J/K/L controls shuttle playback."));
  stopAction = menu_->addAction("停止");
  stopAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_stop.svg")));
  stopAction->setStatusTip(QStringLiteral("Stop playback and return shuttle speed to zero."));

  menu_->addSeparator();
  stepBackwardAction = menu_->addAction("1 フレーム戻る");
  stepBackwardAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_step_backward.svg")));
  stepBackwardAction->setStatusTip(QStringLiteral("Left Arrow steps back one frame. J shuttles reverse."));
  stepForwardAction = menu_->addAction("1 フレーム進む");
  stepForwardAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_step_forward.svg")));
  stepForwardAction->setStatusTip(QStringLiteral("Right Arrow steps forward one frame. L shuttles forward."));

  seekStartAction = menu_->addAction("先頭へ移動");
  seekStartAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_seek_start.svg")));
  seekEndAction = menu_->addAction("末尾へ移動");
  seekEndAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_seek_end.svg")));

  menu_->addSeparator();
  prevMarkerAction = menu_->addAction("前のマーカーへ");
  prevMarkerAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_marker_previous.svg")));
  nextMarkerAction = menu_->addAction("次のマーカーへ");
  nextMarkerAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_marker_next.svg")));

  menu_->addSeparator();
  setInPointAction = menu_->addAction("In Point を現在位置に設定");
  setInPointAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_in_point.svg")));
  setOutPointAction = menu_->addAction("Out Point を現在位置に設定");
  setOutPointAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_out_point.svg")));
  clearInOutAction = menu_->addAction("In/Out をクリア");
  clearInOutAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_clear_in_out.svg")));
  loopAction = menu_->addAction("ループ再生");
  loopAction->setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_loop.svg")));
  loopAction->setCheckable(true);
  loopAction->setStatusTip(QStringLiteral("Alt+L toggles loop playback."));

  QObject::connect(showControlAction, &QAction::triggered, menu_, [this]() {
    showPlaybackControl();
  });
  QObject::connect(playPauseAction, &QAction::triggered, menu_, [this]() {
    if (auto* svc = playbackService()) {
      svc->togglePlayPause();
    }
  });
  QObject::connect(stopAction, &QAction::triggered, menu_, [this]() {
    if (auto* svc = playbackService()) {
      svc->stop();
    }
  });
  QObject::connect(stepBackwardAction, &QAction::triggered, menu_, [this]() {
    if (auto* svc = playbackService()) {
      svc->goToPreviousFrame();
    }
  });
  QObject::connect(stepForwardAction, &QAction::triggered, menu_, [this]() {
    if (auto* svc = playbackService()) {
      svc->goToNextFrame();
    }
  });
  QObject::connect(seekStartAction, &QAction::triggered, menu_, [this]() {
    if (auto* svc = playbackService()) {
      svc->goToStartFrame();
    }
  });
  QObject::connect(seekEndAction, &QAction::triggered, menu_, [this]() {
    if (auto* svc = playbackService()) {
      svc->goToEndFrame();
    }
  });
  QObject::connect(prevMarkerAction, &QAction::triggered, menu_, [this]() {
    if (auto* svc = playbackService()) {
      svc->goToPreviousMarker();
    }
  });
  QObject::connect(nextMarkerAction, &QAction::triggered, menu_, [this]() {
    if (auto* svc = playbackService()) {
      svc->goToNextMarker();
    }
  });
  QObject::connect(setInPointAction, &QAction::triggered, menu_, [this]() {
    if (auto* svc = playbackService()) {
      svc->setInPointAtCurrentFrame();
    }
  });
  QObject::connect(setOutPointAction, &QAction::triggered, menu_, [this]() {
    if (auto* svc = playbackService()) {
      svc->setOutPointAtCurrentFrame();
    }
  });
  QObject::connect(clearInOutAction, &QAction::triggered, menu_, [this]() {
    if (auto* svc = playbackService()) {
      svc->clearInOutPoints();
    }
  });
  QObject::connect(loopAction, &QAction::triggered, menu_, [this](bool checked) {
    if (auto* svc = playbackService()) {
      svc->setLooping(checked);
    }
  });

  QObject::connect(menu_, &QMenu::aboutToShow, menu_, [this]() {
    refreshState();
  });

  auto& eventBus = ArtifactCore::globalEventBus();
  eventBusSubscriptions_.push_back(eventBus.subscribe<PlaybackStateChangedEvent>(
      [this](const PlaybackStateChangedEvent&) { refreshState(); }));
  eventBusSubscriptions_.push_back(eventBus.subscribe<PlaybackLoopingChangedEvent>(
      [this](const PlaybackLoopingChangedEvent&) { refreshState(); }));
  eventBusSubscriptions_.push_back(eventBus.subscribe<PlaybackCompositionChangedEvent>(
      [this](const PlaybackCompositionChangedEvent&) { refreshState(); }));
  eventBusSubscriptions_.push_back(eventBus.subscribe<ProjectChangedEvent>(
      [this](const ProjectChangedEvent&) { refreshState(); }));
}

void ArtifactTimeMenu::Impl::showPlaybackControl()
{
  if (auto* mw = activeMainWindow(menu_)) {
    const QString dockTitle = QStringLiteral("Playback Control");
    if (mw->hasDock(dockTitle)) {
      mw->setDockVisible(dockTitle, true);
      mw->activateDock(dockTitle);
      return;
    }
    auto* widget = new ArtifactPlaybackControlWidget(mw);
    mw->addDockedWidgetFloating(
        dockTitle,
        QStringLiteral("PlaybackControl"),
        widget,
        QRect(120, 828, 720, 210));
    return;
  }

  auto* widget = new ArtifactPlaybackControlWidget(nullptr);
  widget->setAttribute(Qt::WA_DeleteOnClose, true);
  widget->setWindowTitle(QStringLiteral("Playback Control"));
  widget->show();
  widget->raise();
  widget->activateWindow();
}

void ArtifactTimeMenu::Impl::refreshState()
{
  auto* svc = playbackService();
  const bool hasPlayback = svc && static_cast<bool>(svc->currentComposition());

  if (showControlAction) {
    showControlAction->setEnabled(true);
  }
  if (playPauseAction) {
    playPauseAction->setEnabled(hasPlayback);
    playPauseAction->setText(svc && svc->isPlaying() ? QStringLiteral("一時停止")
                                                     : QStringLiteral("再生"));
  }
  if (stopAction) {
    stopAction->setEnabled(hasPlayback);
  }
  if (stepBackwardAction) {
    stepBackwardAction->setEnabled(hasPlayback);
  }
  if (stepForwardAction) {
    stepForwardAction->setEnabled(hasPlayback);
  }
  if (seekStartAction) {
    seekStartAction->setEnabled(hasPlayback);
  }
  if (seekEndAction) {
    seekEndAction->setEnabled(hasPlayback);
  }
  if (prevMarkerAction) {
    prevMarkerAction->setEnabled(hasPlayback);
  }
  if (nextMarkerAction) {
    nextMarkerAction->setEnabled(hasPlayback);
  }
  if (setInPointAction) {
    setInPointAction->setEnabled(hasPlayback);
  }
  if (setOutPointAction) {
    setOutPointAction->setEnabled(hasPlayback);
  }
  if (clearInOutAction) {
    clearInOutAction->setEnabled(hasPlayback);
  }
  if (loopAction) {
    loopAction->setEnabled(hasPlayback);
    loopAction->setChecked(svc && svc->isLooping());
  }
}

void ArtifactTimeMenu::Impl::handleProjectOpened()
{
  refreshState();
}

void ArtifactTimeMenu::Impl::handleCompositionOpened()
{
  refreshState();
}

void ArtifactTimeMenu::Impl::handleCompositionClosed()
{
  refreshState();
}

void ArtifactTimeMenu::Impl::handleProjectClosed()
{
  refreshState();
}

ArtifactTimeMenu::ArtifactTimeMenu(QWidget* parent /*= nullptr*/)
  :QMenu(parent),impl_(new Impl(this))
{
  setTitle("時間(&T)");
  setIcon(QIcon(ArtifactCore::resolveIconPath("Studio/timemenu_timer.svg")));
  impl_->refreshState();
}

ArtifactTimeMenu::~ArtifactTimeMenu()
{
  delete impl_;
}

}

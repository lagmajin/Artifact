module;
#include <algorithm>
#include <functional>
#include <map>
#include <utility>

#include <cmath>
#include <QAction>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QLinearGradient>
#include <QMetaObject>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QResizeEvent>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QJsonObject>
#include <wobjectimpl.h>

module Artifact.Widgets.CompositionAudioMixer;

import Artifact.Audio.Mixer;
import Artifact.Widgets.AudioMixer;
import Audio.Mixer;
import Memory.SharedPtr;
import Artifact.Layer.Abstract;
import Artifact.Layer.Audio;
import Artifact.Layer.Video;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Service.Effect;
import Artifact.Widgets.AudioMixerControls;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Artifact.Service.Audio;
import Undo.UndoManager;
import Settings.Accessibility;
import Event.Bus;
import std;

namespace Artifact {

using namespace detail;

static QPoint accessibilityMenuPosition(const QMenu &menu,
                                        const QPoint &origin) {
  int x = origin.x();
  int y = origin.y();
  Accessibility::adjustContextMenuPosition(x, y, menu.sizeHint().width());
  return QPoint(x, y);
}

void queueMixerRefreshFromWidget(ArtifactCompositionAudioMixerWidget* widget);
std::unique_ptr<UndoCommand> createAudioMixerSnapshotUndoCommand(
    const ArtifactCore::SharedPtr<ArtifactCore::AudioMixer>& mixer,
    const QJsonObject& before, const QJsonObject& after);
QWidget* createAudioMixerMasterRow(AudioMixerMasterBus* masterBus,
                                    QWidget* parent);
QWidget* createAudioMixerStripRow(
    AudioMixerChannelStrip* strip, QWidget* parent,
    ArtifactCompositionAudioMixerWidget* owner);


W_OBJECT_IMPL(ArtifactCompositionAudioMixerWidget)

class ArtifactCompositionAudioMixerWidget::Impl {
public:
  AudioMixer *mixer_ = nullptr;
  QWidget *contentWidget_ = nullptr;
  QHBoxLayout *contentLayout_ = nullptr;
  QLabel *summaryLabel_ = nullptr;
  QLabel *emptyLabel_ = nullptr;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  void clearRows() {
    if (!contentLayout_) {
      return;
    }
    while (QLayoutItem *item = contentLayout_->takeAt(0)) {
      if (QWidget *widget = item->widget()) {
        widget->deleteLater();
      }
      delete item;
    }
  }
};

ArtifactCompositionAudioMixerWidget::ArtifactCompositionAudioMixerWidget(
    QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  setAccessibleName(QStringLiteral("Composition Audio Mixer"));
  setAccessibleDescription(
      QStringLiteral("Monitor and adjust audio buses for the active composition."));
  setAttribute(Qt::WA_StyledBackground, true);
  setAutoFillBackground(true);

  impl_->mixer_ = new AudioMixer(this);

  if (auto *playbackService = ArtifactPlaybackService::instance()) {
    if (auto *masterBus = impl_->mixer_->masterBus()) {
      QObject::connect(masterBus, &AudioMixerMasterBus::volumeChanged, this,
                       [](const float volume) {
                         ArtifactAudioService::instance()->setMasterVolume(volume);
                       });
      QObject::connect(masterBus, &AudioMixerMasterBus::muteChanged, this,
                       [](const bool muted) {
                         ArtifactAudioService::instance()->setMasterMuted(muted);
                       });
      ArtifactAudioService::instance()->setMasterVolume(masterBus->volume());
      ArtifactAudioService::instance()->setMasterMuted(masterBus->isMuted());
    }

    impl_->eventBusSubscriptions_.push_back(
        impl_->eventBus_.subscribe<AudioLevelChangedEvent>(
            [this](const AudioLevelChangedEvent& event) {
              impl_->mixer_->updatePlaybackLevels(event.leftRms,
                                                   event.rightRms);
            }));
  }

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  auto *header = new QWidget(this);
  header->setObjectName(QStringLiteral("audioMixerHeader"));
  header->setAutoFillBackground(true);
  {
    QPalette headerPalette = header->palette();
    headerPalette.setColor(QPalette::Window, QColor(31, 34, 37));
    header->setPalette(headerPalette);
  }
  auto *headerLayout = new QVBoxLayout(header);
  headerLayout->setContentsMargins(12, 9, 12, 9);
  headerLayout->setSpacing(1);

  auto *titleLabel = new QLabel(QStringLiteral("Audio Mixer"), header);
  auto *subtitleLabel = new QLabel(
      QStringLiteral("Master bus and current composition audio layers"),
      header);
  {
    QPalette titlePalette = titleLabel->palette();
    titlePalette.setColor(QPalette::WindowText, QColor(240, 243, 246));
    titleLabel->setPalette(titlePalette);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() > 0 ? titleFont.pointSize() + 1
                                                     : 11);
    titleLabel->setFont(titleFont);

    QPalette subtitlePalette = subtitleLabel->palette();
    subtitlePalette.setColor(QPalette::WindowText, QColor(166, 179, 195));
    subtitleLabel->setPalette(subtitlePalette);
  }

  headerLayout->addWidget(titleLabel);
  headerLayout->addWidget(subtitleLabel);
  impl_->summaryLabel_ = new QLabel(header);
  {
    impl_->summaryLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    impl_->summaryLabel_->setSizePolicy(QSizePolicy::Expanding,
                                        QSizePolicy::Preferred);
    QPalette summaryPalette = impl_->summaryLabel_->palette();
    summaryPalette.setColor(QPalette::WindowText, QColor(207, 214, 221));
    impl_->summaryLabel_->setPalette(summaryPalette);
    QFont summaryFont = impl_->summaryLabel_->font();
    summaryFont.setBold(true);
    summaryFont.setPointSize(summaryFont.pointSize() > 0 ? summaryFont.pointSize() - 1
                                                         : 9);
    impl_->summaryLabel_->setFont(summaryFont);
  }
  headerLayout->addWidget(impl_->summaryLabel_, 0, Qt::AlignRight);

  auto *routingButton = new AudioRoutingButton(header);
  routingButton->setText(QStringLiteral("Advanced Routing…"));
  routingButton->setAccessibleName(QStringLiteral("Open advanced audio routing"));
  routingButton->setAccessibleDescription(
      QStringLiteral("Edit bus output routes and sidechain sends for the current composition."));
  routingButton->invoked = [this]() {
    ArtifactCompositionPtr composition;
    if (auto *projectService = ArtifactProjectService::instance()) {
      composition = projectService->currentComposition().lock();
    }
    const auto coreMixer = composition ? composition->getAudioMixer() : nullptr;
    if (!composition || !coreMixer) {
      QMessageBox::information(
          this, QStringLiteral("Audio routing unavailable"),
          QStringLiteral("Add an audio-capable layer before editing audio routing."));
      return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Advanced Audio Routing"));
    dialog.setModal(true);
    dialog.resize(760, 540);
    auto *dialogLayout = new QVBoxLayout(&dialog);
    dialogLayout->setContentsMargins(10, 10, 10, 10);
    bool routingChangeAttempted = false;
    bool routingUndoAccepted = true;
    auto routingUndo = [&routingChangeAttempted, &routingUndoAccepted, coreMixer](
                           const QJsonObject& before, const QJsonObject& after) {
      routingChangeAttempted = true;
      auto* manager = UndoManager::instance();
      if (manager) {
        routingUndoAccepted = manager->push(
            createAudioMixerSnapshotUndoCommand(coreMixer, before, after));
      } else {
        // The editor has already applied the live change. Keep that change
        // when history is unavailable, but verify the resulting mixer state.
        routingUndoAccepted = coreMixer->serialize() == after;
      }
      if (!routingUndoAccepted) {
        coreMixer->deserialize(before);
      }
    };
    auto *routingWidget = new Artifact::AudioMixerWidget(
        coreMixer.get(), &dialog, std::move(routingUndo));
    dialogLayout->addWidget(routingWidget, 1);
    dialog.exec();

    if (!routingChangeAttempted) {
      refreshFromCurrentComposition();
      return;
    }
    if (!routingUndoAccepted) {
      refreshFromCurrentComposition();
      QMessageBox::warning(
          this, QStringLiteral("Audio routing update failed"),
          QStringLiteral("The routing change could not be recorded in Undo history."));
      return;
    }

    // Routing lives in the composition mixer serialization. Mark the owning
    // composition changed after the existing editor closes, then rebuild the
    // compact surface from the same Core graph.
    composition->changed();
    refreshFromCurrentComposition();
  };
  headerLayout->addWidget(routingButton, 0, Qt::AlignRight);

  auto *scrollArea = new QScrollArea(this);
  scrollArea->setObjectName(QStringLiteral("audioMixerScrollArea"));
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->viewport()->setAutoFillBackground(true);
  {
    QPalette scrollPalette = scrollArea->viewport()->palette();
    scrollPalette.setColor(QPalette::Window, QColor(22, 25, 28));
    scrollArea->viewport()->setPalette(scrollPalette);
  }

  impl_->contentWidget_ = new QWidget(scrollArea);
  impl_->contentWidget_->setObjectName(QStringLiteral("audioMixerContentWidget"));
  impl_->contentWidget_->setAutoFillBackground(true);
  {
    QPalette contentPalette = impl_->contentWidget_->palette();
    contentPalette.setColor(QPalette::Window, QColor(22, 25, 28));
    impl_->contentWidget_->setPalette(contentPalette);
  }
  impl_->contentLayout_ = new QHBoxLayout(impl_->contentWidget_);
  impl_->contentLayout_->setContentsMargins(10, 10, 10, 10);
  impl_->contentLayout_->setSpacing(6);
  impl_->contentLayout_->addStretch();
  scrollArea->setWidget(impl_->contentWidget_);

  rootLayout->addWidget(header);
  rootLayout->addWidget(scrollArea, 1);

  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<ProjectChangedEvent>(
          [this](const ProjectChangedEvent &) {
            queueMixerRefreshFromWidget(this);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<CurrentCompositionChangedEvent>(
          [this](const CurrentCompositionChangedEvent &) {
            queueMixerRefreshFromWidget(this);
          }));
  impl_->eventBusSubscriptions_.push_back(
      impl_->eventBus_.subscribe<LayerChangedEvent>(
          [this](const LayerChangedEvent &) {
            queueMixerRefreshFromWidget(this);
          }));

  refreshFromCurrentComposition();
}

ArtifactCompositionAudioMixerWidget::~ArtifactCompositionAudioMixerWidget() {
  delete impl_;
}

void ArtifactCompositionAudioMixerWidget::refreshFromCurrentComposition() {
  ArtifactCompositionPtr composition;
  if (auto *service = ArtifactProjectService::instance()) {
    composition = service->currentComposition().lock();
  }
  ArtifactAudioService::instance()->syncCurrentComposition();
  impl_->mixer_->connectToCoreMixer(
      composition ? composition->getAudioMixer() : nullptr);

  // The advanced routing editor edits the Core master bus directly. Mirror
  // that value into the playback service after the Core binding is refreshed,
  // otherwise the compact UI and the actual output gain/mute can diverge.
  if (const auto coreMixer = composition ? composition->getAudioMixer() : nullptr) {
    if (const auto masterBus = coreMixer->getMasterBus()) {
      const float masterVolume = std::pow(
          10.0f, masterBus->getVolume() / 20.0f);
      ArtifactAudioService::instance()->setMasterVolume(masterVolume);
      ArtifactAudioService::instance()->setMasterMuted(masterBus->isMute());
    }
  }

  impl_->mixer_->syncFromComposition(composition);
  impl_->clearRows();
  const auto strips = impl_->mixer_->allChannelStrips();
  int mutedCount = 0;
  int soloCount = 0;
  int fxCount = 0;
  int missingCount = 0;
  int unloadedCount = 0;
  for (auto *strip : strips) {
    if (!strip) {
      continue;
    }
    if (strip->isMuted()) {
      ++mutedCount;
    }
    if (strip->isSolo()) {
      ++soloCount;
    }
    const auto effectChain = strip->effectChain();
    fxCount += effectChain.size();
    if (composition) {
      const auto layer = composition->layerById(strip->layerId());
      if (const auto audioLayer = ArtifactCore::dynamicPointerCast<ArtifactAudioLayer>(layer)) {
        const QString sourcePath = audioLayer->sourcePath();
        if (!sourcePath.isEmpty() && !QFileInfo::exists(sourcePath)) {
          ++missingCount;
        } else if (!audioLayer->isLoaded()) {
          ++unloadedCount;
        }
      }
    }
  }
  if (impl_->summaryLabel_) {
    if (strips.isEmpty()) {
      impl_->summaryLabel_->setText(QStringLiteral("Unavailable"));
    } else {
      impl_->summaryLabel_->setText(QStringLiteral("%1 layers · %2 FX · %3 solo · %4 mute%5%6")
                                        .arg(strips.size())
                                        .arg(fxCount)
                                        .arg(soloCount)
                                        .arg(mutedCount)
                                        .arg(missingCount > 0
                                                 ? QStringLiteral(" · %1 missing").arg(missingCount)
                                                 : QString())
                                        .arg(unloadedCount > 0
                                                 ? QStringLiteral(" · %1 unloaded").arg(unloadedCount)
                                                 : QString()));
    }
  }

  if (strips.isEmpty()) {
    impl_->emptyLabel_ =
        new QLabel(QStringLiteral("Audio Mixer is unavailable until the composition has audio layers"),
                   impl_->contentWidget_);
    impl_->emptyLabel_->setAlignment(Qt::AlignCenter);
    impl_->emptyLabel_->setWordWrap(true);
    impl_->emptyLabel_->setMinimumWidth(260);
    impl_->emptyLabel_->setMaximumWidth(420);
    impl_->emptyLabel_->setSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Preferred);
    impl_->emptyLabel_->setEnabled(false);
    impl_->emptyLabel_->setAutoFillBackground(true);
    QPalette emptyPalette = impl_->emptyLabel_->palette();
    emptyPalette.setColor(QPalette::WindowText, QColor(145, 155, 165));
    emptyPalette.setColor(QPalette::Window, QColor(22, 27, 32));
    emptyPalette.setColor(QPalette::Base, QColor(22, 27, 32));
    emptyPalette.setColor(QPalette::Disabled, QPalette::WindowText,
                          QColor(108, 117, 126));
    emptyPalette.setColor(QPalette::Disabled, QPalette::Window,
                          QColor(20, 23, 26));
    impl_->emptyLabel_->setPalette(emptyPalette);
    impl_->contentLayout_->addStretch();
    impl_->contentLayout_->addWidget(impl_->emptyLabel_);
    impl_->contentLayout_->addStretch();
  } else {
    if (auto *masterBus = impl_->mixer_->masterBus()) {
      impl_->contentLayout_->addWidget(
          createAudioMixerMasterRow(masterBus, impl_->contentWidget_));
    }
    impl_->contentLayout_->addWidget(
        new AudioStripSeparatorWidget(impl_->contentWidget_));
    for (int i = 0; i < strips.size(); ++i) {
      auto *strip = strips.at(i);
      impl_->contentLayout_->addWidget(
          createAudioMixerStripRow(strip, impl_->contentWidget_, this));
      if (i + 1 < strips.size()) {
        impl_->contentLayout_->addWidget(
            new AudioStripSeparatorWidget(impl_->contentWidget_));
      }
    }
  }
  if (!strips.isEmpty()) {
    impl_->contentLayout_->addStretch();
  }
}

} // namespace Artifact

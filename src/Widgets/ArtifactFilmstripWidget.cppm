module;
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <functional>
#include <utility>
#include <QCheckBox>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QImage>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QPalette>
#include <QPixmap>
#include <QPointer>
#include <QShowEvent>
#include <QSpinBox>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QTimerEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

module Artifact.Widgets.Filmstrip;
import Artifact.Service.Playback;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Event.Bus;
import Frame.Position;
import Frame.Range;
import Image.ImageF32x4_RGBA;
import FloatRGBA;
import Graphics.SurfaceColorContract;

namespace Artifact {
using ArtifactCore::FramePosition;
using ArtifactCore::FrameRange;
namespace {
// Qt's normal activation path covers pointer, keyboard and accessibility.
class FilmstripButton final : public QToolButton {
public:
  std::function<void()> action;
  explicit FilmstripButton(QWidget *parent) : QToolButton(parent) {}
protected:
  void nextCheckState() override { if (action) action(); }
};

class FilmstripSpin final : public QSpinBox {
public:
  std::function<void()> changed;
  explicit FilmstripSpin(QWidget *parent) : QSpinBox(parent) {}
protected:
  void stepBy(int steps) override {
    QSpinBox::stepBy(steps);
    if (changed) changed();
  }
  void focusOutEvent(QFocusEvent *event) override {
    QSpinBox::focusOutEvent(event);
    if (changed) changed();
  }
  void keyPressEvent(QKeyEvent *event) override {
    QSpinBox::keyPressEvent(event);
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        changed) changed();
  }
};

class FilmstripFollow final : public QCheckBox {
public:
  std::function<void()> changed;
  explicit FilmstripFollow(QWidget *parent) : QCheckBox(parent) {}
protected:
  void nextCheckState() override {
    QCheckBox::nextCheckState();
    if (changed) changed();
  }
};

class FilmstripWidget final : public QWidget {
  struct Tile {
    FilmstripButton *button = nullptr;
    QLabel *caption = nullptr;
    QWidget *surface = nullptr;
    ArtifactCore::ImageF32x4_RGBA thumbnail;
    int64_t frame = -1;
    uint64_t revision = 0;
    bool loaded = false;
  };
  std::array<Tile, 7> tiles_;
  ArtifactCore::EventBus bus_ = ArtifactCore::globalEventBus();
  std::array<ArtifactCore::EventBus::Subscription, 4> subscriptions_;
  FilmstripSpin *radius_ = nullptr;
  FilmstripSpin *spacing_ = nullptr;
  FilmstripFollow *follow_ = nullptr;
  FilmstripFollow *onion_ = nullptr;
  FilmstripSpin *opacity_ = nullptr;
  FilmstripSpin *reference_ = nullptr;
  FilmstripSpin *zoom_ = nullptr;
  FilmstripButton *comparison_ = nullptr;
  QLabel *preview_ = nullptr;
  QLabel *previewCaption_ = nullptr;
  ArtifactCore::ImageF32x4_RGBA previewPixels_;
  int comparisonMode_ = 0; // 0: none, 1: side by side, 2: difference
  bool previewDirty_ = true;
  QLabel *status_ = nullptr;
  QString compositionId_;
  int64_t center_ = 0;
  int refreshTimer_ = 0;

  static ArtifactCore::FloatRGBA straightPixel(const Tile &tile, int x, int y) {
    using namespace ArtifactCore;
    const auto value = tile.thumbnail.getPixel(x, y);
    const auto descriptor = tile.thumbnail.colorDescriptor();
    const bool bgra = descriptor.channelOrder == SurfaceChannelOrder::BGRA;
    float r = bgra ? value.b() : value.r();
    float g = value.g();
    float b = bgra ? value.r() : value.b();
    const float alpha = descriptor.alphaMode == SurfaceAlphaMode::Opaque
        ? 1.0f : std::clamp(value.a(), 0.0f, 1.0f);
    if (descriptor.alphaMode == SurfaceAlphaMode::Premultiplied) {
      if (alpha > 0.000001f) { r /= alpha; g /= alpha; b /= alpha; }
      else { r = 0.0f; g = 0.0f; b = 0.0f; }
    }
    return FloatRGBA(r, g, b, alpha);
  }

  void refreshPreview() {
    using namespace ArtifactCore;
    if (!previewDirty_) return;
    previewDirty_ = false;
    const auto &center = tiles_[3];
    const auto &reference = tiles_[3 + reference_->value()];
    if (!center.loaded || (comparisonMode_ && !reference.loaded)) {
      preview_->clear();
      preview_->setText(tr("Preview frames are not cached"));
      previewCaption_->setText(tr("Render the selected frames to populate the preview cache."));
      return;
    }
    auto descriptor = center.thumbnail.colorDescriptor();
    if (comparisonMode_) {
      const auto other = reference.thumbnail.colorDescriptor();
      if (descriptor.transfer != other.transfer ||
          descriptor.primaries != other.primaries) {
        preview_->clear();
        preview_->setText(tr("Frames use different color encodings"));
        previewCaption_->clear();
        return;
      }
    }
    descriptor.channelOrder = SurfaceChannelOrder::RGBA;
    descriptor.alphaMode = SurfaceAlphaMode::Straight;
    previewPixels_.setColorDescriptor(descriptor);
    const float opacity = opacity_->value() / 100.0f;
    const bool sideBySide = comparisonMode_ == 1;
    // Fixed 320x90 workspace, sampled from cached 160x90 thumbnails only.
    for (int y = 0; y < 90; ++y) {
      for (int x = 0; x < 320; ++x) {
        const int sampleX = sideBySide ? x % 160 : x / 2;
        const auto current = straightPixel(center, sampleX, y);
        FloatRGBA output = current;
        if (sideBySide) {
          output = x < 160 ? current : straightPixel(reference, sampleX, y);
        } else if (comparisonMode_ == 2) {
          const auto other = straightPixel(reference, sampleX, y);
          // Difference includes coverage, so alpha-only edits remain visible.
          const float alphaDifference = std::abs(current.a() - other.a());
          output = FloatRGBA(std::max(alphaDifference,
                              std::abs(current.r() * current.a() - other.r() * other.a())),
                             std::max(alphaDifference,
                              std::abs(current.g() * current.a() - other.g() * other.a())),
                             std::max(alphaDifference,
                              std::abs(current.b() * current.a() - other.b() * other.a())), 1.0f);
        } else if (onion_->isChecked()) {
          float r = current.r() * current.a();
          float g = current.g() * current.a();
          float b = current.b() * current.a();
          float alpha = current.a();
          for (const int index : std::array<int, 2>{2, 4}) {
            const auto &ghost = tiles_[index];
            if (!ghost.loaded) continue;
            const auto ghostDescriptor = ghost.thumbnail.colorDescriptor();
            if (ghostDescriptor.transfer != descriptor.transfer ||
                ghostDescriptor.primaries != descriptor.primaries) continue;
            const auto pixel = straightPixel(ghost, sampleX, y);
            const float coverage = pixel.a() * opacity * 0.5f;
            r = pixel.r() * coverage + r * (1.0f - coverage);
            g = pixel.g() * coverage + g * (1.0f - coverage);
            b = pixel.b() * coverage + b * (1.0f - coverage);
            alpha = coverage + alpha * (1.0f - coverage);
          }
          output = alpha > 0.000001f ? FloatRGBA(r / alpha, g / alpha, b / alpha, alpha)
                                     : FloatRGBA();
        }
        previewPixels_.setPixel(x, y, output);
      }
    }
    const auto pixmap = QPixmap::fromImage(previewPixels_.toQImage());
    // Zoom is a display-only Qt boundary, never a full-frame render resize.
    preview_->setPixmap(pixmap.scaled((sideBySide ? 640 : 320) * zoom_->value() / 100,
                                      180 * zoom_->value() / 100,
                                      Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    previewCaption_->setText(comparisonMode_ ? tr("Frame %1 / %2 · %3")
        .arg(center.frame).arg(reference.frame)
        .arg(comparisonMode_ == 1 ? tr("Side by side") : tr("Difference"))
        : tr("Frame %1 · %2").arg(center.frame)
            .arg(onion_->isChecked() ? tr("Onion skin (cached neighbors)") : tr("Preview")));
  }

  void scheduleRefresh() {
    if (isVisible() && !refreshTimer_) refreshTimer_ = startTimer(100);
  }

  void refresh() {
    auto *playback = ArtifactPlaybackService::instance();
    const auto composition = playback ? playback->currentComposition() : nullptr;
    const QString id = composition ? composition->id().toString() : QString();
    if (id != compositionId_) {
      compositionId_ = id;
      center_ = playback ? playback->currentFrame().framePosition() : 0;
      for (auto &tile : tiles_) tile.loaded = false;
      previewDirty_ = true;
    }
    if (playback && follow_->isChecked())
      center_ = playback->currentFrame().framePosition();
    int ready = 0;
    const int radius = radius_->value();
    reference_->setRange(-radius, radius);
    const auto range = playback ? playback->playableRange()
                                : FrameRange(FramePosition(0), FramePosition(0));
    for (int i = 0; i < 7; ++i) {
      auto &tile = tiles_[i];
      const int offset = i - 3;
      const bool visible = std::abs(offset) <= radius;
      tile.surface->setVisible(visible);
      if (!visible) continue;
      const int64_t frame = center_ + int64_t(offset) * spacing_->value();
      if (frame != tile.frame) { tile.loaded = false; previewDirty_ = true; }
      tile.frame = frame;
      const bool valid = composition && tile.frame >= range.start() &&
                         tile.frame < range.end();
      tile.button->setEnabled(valid);
      tile.button->setChecked(offset == 0);
      const auto state = playback ? playback->ramPreviewFrameState(frame)
                                 : ArtifactRamPreviewFrameCacheState{};
      if (!valid || !state.ready || !state.inRam || state.failed ||
          state.compositionRevision != tile.revision) {
        if (tile.loaded) previewDirty_ = true;
        tile.loaded = false;
      }
      if (!tile.loaded) {
        tile.button->setIcon(QIcon());
        tile.button->setText(valid ? tr("Not cached") : tr("Outside range"));
      }
      if (!tile.loaded && valid &&
          playback->sampleRamPreviewThumbnail(tile.frame, tile.thumbnail)) {
        // Explicit, small Qt display boundary; no full-frame QImage/readback.
        tile.button->setIcon(QIcon(QPixmap::fromImage(tile.thumbnail.toQImage())));
        tile.button->setText(QString());
        tile.loaded = true;
        tile.revision = state.compositionRevision;
        previewDirty_ = true;
      }
      if (tile.loaded) ++ready;
      tile.button->setAccessibleName(tr("Go to frame %1").arg(tile.frame));
      tile.button->setToolTip(tr("Frame %1").arg(tile.frame));
      tile.caption->setText(offset == 0 ? tr("Current · %1f").arg(tile.frame)
          : QStringLiteral("%1%2f").arg(offset > 0 ? "+" : "")
                .arg(int64_t(offset) * spacing_->value()));
      QPalette captionPalette = palette();
      if (offset == 0)
        captionPalette.setColor(QPalette::WindowText,
                                palette().color(QPalette::Highlight));
      tile.caption->setPalette(captionPalette);
    }
    status_->setText(composition ? tr("%1 / %2 cached · uncached frames appear after preview rendering")
        .arg(ready).arg(radius * 2 + 1) : tr("Select a composition"));
    refreshPreview();
  }

protected:
  void showEvent(QShowEvent *event) override {
    QWidget::showEvent(event);
    scheduleRefresh();
  }
  void hideEvent(QHideEvent *event) override {
    if (refreshTimer_) { killTimer(refreshTimer_); refreshTimer_ = 0; }
    QWidget::hideEvent(event);
  }
  void timerEvent(QTimerEvent *event) override {
    if (event->timerId() != refreshTimer_) { QWidget::timerEvent(event); return; }
    killTimer(refreshTimer_);
    refreshTimer_ = 0;
    refresh();
  }

public:
  explicit FilmstripWidget(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("artifactFilmstripWidget"));
    setAccessibleName(tr("Filmstrip View"));
    auto *layout = new QVBoxLayout(this);
    auto *toolbar = new QHBoxLayout;
    radius_ = new FilmstripSpin(this);
    radius_->setRange(1, 3); radius_->setValue(3);
    radius_->setAccessibleName(tr("Frames on each side"));
    spacing_ = new FilmstripSpin(this);
    spacing_->setRange(1, 100); spacing_->setValue(1);
    spacing_->setAccessibleName(tr("Frame interval"));
    follow_ = new FilmstripFollow(this);
    follow_->setText(tr("Follow playhead")); follow_->setChecked(true);
    toolbar->addWidget(new QLabel(tr("Frame steps"), this));
    toolbar->addWidget(radius_);
    toolbar->addWidget(new QLabel(tr("Interval"), this));
    toolbar->addWidget(spacing_);
    toolbar->addWidget(follow_);
    toolbar->addStretch();
    auto addAction = [&](const QString &text, std::function<void()> action) {
      auto *button = new FilmstripButton(this);
      button->setText(text); button->action = std::move(action);
      toolbar->addWidget(button);
    };
    addAction(tr("Previous"), [] {
      if (auto *p = ArtifactPlaybackService::instance()) p->goToPreviousFrame();
    });
    addAction(tr("Next"), [] {
      if (auto *p = ArtifactPlaybackService::instance()) p->goToNextFrame();
    });
    addAction(tr("Refresh"), [this] {
      for (auto &tile : tiles_) tile.loaded = false;
      scheduleRefresh();
    });
    layout->addLayout(toolbar);
    auto *inspection = new QHBoxLayout;
    onion_ = new FilmstripFollow(this);
    onion_->setText(tr("Onion skin"));
    opacity_ = new FilmstripSpin(this);
    opacity_->setRange(0, 100); opacity_->setValue(40); opacity_->setSuffix(" %");
    opacity_->setAccessibleName(tr("Onion skin opacity"));
    reference_ = new FilmstripSpin(this);
    reference_->setRange(-3, 3); reference_->setValue(-1); reference_->setSuffix(" step");
    reference_->setAccessibleName(tr("Comparison frame offset"));
    zoom_ = new FilmstripSpin(this);
    zoom_->setRange(50, 200); zoom_->setValue(100); zoom_->setSuffix(" %");
    zoom_->setAccessibleName(tr("Preview zoom"));
    comparison_ = new FilmstripButton(this);
    comparison_->setText(tr("Comparison: none"));
    comparison_->action = [this] {
      comparisonMode_ = (comparisonMode_ + 1) % 3;
      comparison_->setText(comparisonMode_ == 0 ? tr("Comparison: none")
          : comparisonMode_ == 1 ? tr("Comparison: side by side") : tr("Comparison: difference"));
      onion_->setEnabled(comparisonMode_ == 0);
      opacity_->setEnabled(comparisonMode_ == 0);
      previewDirty_ = true;
      scheduleRefresh();
    };
    inspection->addWidget(onion_);
    inspection->addWidget(opacity_);
    inspection->addWidget(comparison_);
    inspection->addWidget(new QLabel(tr("Reference"), this));
    inspection->addWidget(reference_);
    inspection->addStretch();
    inspection->addWidget(new QLabel(tr("Zoom"), this));
    inspection->addWidget(zoom_);
    layout->addLayout(inspection);
    auto *strip = new QHBoxLayout;
    for (auto &tile : tiles_) {
      tile.thumbnail.resize(160, 90);
      tile.surface = new QWidget(this);
      auto *column = new QVBoxLayout(tile.surface);
      column->setContentsMargins(0, 0, 0, 0);
      tile.button = new FilmstripButton(tile.surface);
      tile.button->setCheckable(true);
      tile.button->setIconSize(QSize(160, 90));
      tile.button->setMinimumSize(100, 100);
      tile.button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      tile.caption = new QLabel(tile.surface);
      tile.caption->setAlignment(Qt::AlignCenter);
      auto *selectedTile = &tile;
      tile.button->action = [selectedTile] {
        if (auto *p = ArtifactPlaybackService::instance())
          p->pauseAndGoToFrame(FramePosition(selectedTile->frame));
      };
      column->addWidget(tile.button, 1);
      column->addWidget(tile.caption);
      strip->addWidget(tile.surface, 1);
    }
    layout->addLayout(strip, 1);
    previewPixels_.resize(320, 90);
    preview_ = new QLabel(this);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setAccessibleName(tr("Filmstrip comparison preview"));
    previewCaption_ = new QLabel(this);
    previewCaption_->setAlignment(Qt::AlignCenter);
    layout->addWidget(preview_);
    layout->addWidget(previewCaption_);
    status_ = new QLabel(this);
    layout->addWidget(status_);
    radius_->changed = [this] { previewDirty_ = true; scheduleRefresh(); };
    spacing_->changed = [this] { previewDirty_ = true; scheduleRefresh(); };
    follow_->changed = [this] { scheduleRefresh(); };
    const auto inspectionChanged = [this] { previewDirty_ = true; scheduleRefresh(); };
    onion_->changed = inspectionChanged;
    opacity_->changed = inspectionChanged;
    reference_->changed = inspectionChanged;
    zoom_->changed = inspectionChanged;
    const QPointer<FilmstripWidget> guard(this);
    subscriptions_[0] = bus_.subscribe<FrameChangedEvent>([guard](const auto &) {
      if (guard && guard->follow_->isChecked()) guard->scheduleRefresh();
    });
    subscriptions_[1] = bus_.subscribe<PlaybackCompositionChangedEvent>([guard](const auto &) {
      if (guard) guard->scheduleRefresh();
    });
    subscriptions_[2] = bus_.subscribe<CurrentCompositionChangedEvent>([guard](const auto &) {
      if (guard) guard->scheduleRefresh();
    });
    subscriptions_[3] = bus_.subscribe<PlaybackRamPreviewStatsChangedEvent>([guard](const auto &) {
      if (guard) guard->scheduleRefresh();
    });
  }
};
}

QWidget *createFilmstripWidget(QWidget *parent) {
  return new FilmstripWidget(parent);
}
}

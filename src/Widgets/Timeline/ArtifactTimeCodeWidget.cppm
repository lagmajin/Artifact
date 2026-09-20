module;
#include <utility>
#include <QLabel>
#include <QBoxLayout>
#include <QVBoxLayout>
#include <QString>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QPushButton>
#include <QKeyEvent>
#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPainter>
#include <QMenu>
#include <QInputDialog>
#include <QSettings>
#include <QHash>
#include <wobjectimpl.h>

module Artifact.Timeline.TimeCodeWidget;

import Core.ArtifactMath;
import Time.Rational;
import Widgets.Utils.CSS;

namespace Artifact
{
 W_OBJECT_IMPL(ArtifactTimeCodeWidget)
 class ArtifactTimeCodeWidget::Impl {
 public:
 Impl();
  QLabel* timecodeLabel_ = nullptr;
  QLabel* frameNumberLabel_ = nullptr;
  QLabel* fpsLabel_ = nullptr;
  int fps_ = 30;
  int currentFrame_ = 0;
 };

 ArtifactTimeCodeWidget::Impl::Impl()
 {
  timecodeLabel_ = new QLabel();
  timecodeLabel_->setText("00:00:00:00");
  frameNumberLabel_ = new QLabel();
  frameNumberLabel_->setText("0 f");
  fpsLabel_ = new QLabel();
  fpsLabel_->setText("30 fps");
 }

 ArtifactTimeCodeWidget::ArtifactTimeCodeWidget(QWidget* parent /*= nullptr*/) : QWidget(parent), impl_(new Impl())
 {
  // Keep the timing readout on one shared toolbar baseline, matching the
  // approved normal-timeline design instead of presenting a detached box.
  auto layout = new QHBoxLayout();
  layout->setSpacing(12);
  layout->setContentsMargins(14, 0, 12, 0);

  impl_->timecodeLabel_->setObjectName("timeLabel");
  impl_->frameNumberLabel_->setObjectName("frameLabel");
  impl_->fpsLabel_->setObjectName("fpsLabel");
  impl_->timecodeLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  impl_->frameNumberLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  impl_->fpsLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

 setAttribute(Qt::WA_StyledBackground, false);
 setAutoFillBackground(false);
  setAccessibleName(QStringLiteral("Timeline timecode"));
  setAccessibleDescription(QStringLiteral("Current timeline timecode and frame number"));
  impl_->timecodeLabel_->setAccessibleName(QStringLiteral("Timecode"));
  impl_->frameNumberLabel_->setAccessibleName(QStringLiteral("Frame number"));

  {
   const QColor textColor = QColor(ArtifactCore::currentDCCTheme().textColor);
   const QColor mutedTextColor = textColor.darker(150);

   QPalette timePal = impl_->timecodeLabel_->palette();
   timePal.setColor(QPalette::WindowText, textColor);
   timePal.setColor(QPalette::Text, textColor);
   impl_->timecodeLabel_->setPalette(timePal);
   impl_->timecodeLabel_->setAutoFillBackground(false);

   QPalette framePal = impl_->frameNumberLabel_->palette();
   framePal.setColor(QPalette::WindowText, mutedTextColor);
   framePal.setColor(QPalette::Text, mutedTextColor);
   impl_->frameNumberLabel_->setPalette(framePal);
   impl_->frameNumberLabel_->setAutoFillBackground(false);
   impl_->fpsLabel_->setPalette(framePal);
   impl_->fpsLabel_->setAutoFillBackground(false);
  }

  auto* firstDivider = new QLabel(QStringLiteral("|"));
  auto* secondDivider = new QLabel(QStringLiteral("|"));
  QPalette dividerPalette = firstDivider->palette();
  dividerPalette.setColor(QPalette::WindowText,
                          QColor(ArtifactCore::currentDCCTheme().borderColor));
  firstDivider->setPalette(dividerPalette);
  secondDivider->setPalette(dividerPalette);
  layout->addWidget(impl_->timecodeLabel_);
  layout->addWidget(firstDivider);
  layout->addWidget(impl_->frameNumberLabel_);
  layout->addWidget(secondDivider);
  layout->addWidget(impl_->fpsLabel_);
  layout->addStretch(1);

  setLayout(layout);
  QFont timeFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  timeFont.setBold(true);
  timeFont.setPixelSize(18);
  impl_->timecodeLabel_->setFont(timeFont);

  QFont frameFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  frameFont.setBold(false);
  frameFont.setPixelSize(11);
  impl_->frameNumberLabel_->setFont(frameFont);
  impl_->fpsLabel_->setFont(frameFont);

  const QFontMetrics timeMetrics(timeFont);
  const QFontMetrics frameMetrics(frameFont);
  impl_->timecodeLabel_->setMinimumHeight(timeMetrics.height() + 4);
  impl_->frameNumberLabel_->setMinimumHeight(timeMetrics.height() + 4);
  impl_->fpsLabel_->setMinimumHeight(timeMetrics.height() + 4);
  setFixedHeight(50);

  // Include the layout's left/right margins. Omitting them let the label paint
  // into the neighbouring mode button when the timeline dock became narrow.
  const QMargins margins = layout->contentsMargins();
  const int minimumWidth = margins.left() +
                           timeMetrics.horizontalAdvance(
                               QStringLiteral("00:00:00:00")) +
                           frameMetrics.horizontalAdvance(
                               QStringLiteral("|  0000 f  |  120 fps")) +
                           margins.right();
  setMinimumWidth(minimumWidth);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
 }

 ArtifactTimeCodeWidget::~ArtifactTimeCodeWidget()
 {
  delete impl_;
 }

 void ArtifactTimeCodeWidget::setFps(int fps)
 {
    if (!impl_) {
      return;
    }
    const int sanitized = ArtifactCore::artifactMax(1, fps);
    if (impl_->fps_ == sanitized) {
      return;
    }
    impl_->fps_ = sanitized;
    if (impl_->fpsLabel_) {
      impl_->fpsLabel_->setText(QStringLiteral("%1 fps").arg(sanitized));
    }
    updateTimeCode(impl_->currentFrame_);
 }

 void ArtifactTimeCodeWidget::updateTimeCode(int frame)
 {
    const int fps = ArtifactCore::artifactMax(1, impl_->fps_);
    impl_->currentFrame_ = frame;

    // Use RationalTime to represent the frame/time (value = frame count, scale = fps)
    ArtifactCore::RationalTime rt(static_cast<int64_t>(frame), static_cast<int64_t>(fps));

    // Compute hours/minutes/seconds/frames
    const bool negative = frame < 0;
    const qint64 totalFrames = negative
        ? -static_cast<qint64>(frame)
        : static_cast<qint64>(frame);
    const qint64 ff = totalFrames % fps;
    const qint64 totalSeconds = totalFrames / fps;
    const qint64 s = totalSeconds % 60;
    const qint64 m = (totalSeconds / 60) % 60;
    const qint64 h = totalSeconds / 3600;
    const QString sign = negative ? QStringLiteral("-") : QString();

    QString tc = QString("%1%2:%3:%4:%5")
        .arg(sign)
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(ff, 2, 10, QChar('0'));

    if (impl_->timecodeLabel_) impl_->timecodeLabel_->setText(tc);
    if (impl_->frameNumberLabel_) {
        impl_->frameNumberLabel_->setText(QString("%1%2 f")
            .arg(sign)
            .arg(totalFrames));
    }
    setAccessibleDescription(QStringLiteral("Current timecode %1, frame %2")
                                 .arg(tc)
                                 .arg(frame));

    // Example use of RationalTime API (kept for future extension)
    Q_UNUSED(rt);
 }

 void ArtifactTimeCodeWidget::paintEvent(QPaintEvent* event)
 {
  QWidget::paintEvent(event);
 }

 W_OBJECT_IMPL(ArtifactTimelineSearchBarWidget)

 class ArtifactTimelineSearchBarWidget::Impl {
 public:
  Impl();
  QLineEdit* searchLineEdit_ = nullptr;
  QStringList savedFilters_;
 };

ArtifactTimelineSearchBarWidget::Impl::Impl()
{
 searchLineEdit_ = new QLineEdit();
 searchLineEdit_->setPlaceholderText("検索 (type:text fx:blur tag:bg parent:none visible:false)");
 searchLineEdit_->setAccessibleName(QStringLiteral("Timeline search"));
 searchLineEdit_->setAccessibleDescription(
     QStringLiteral("Search timeline layers and properties; Enter finds next and Shift-Enter finds previous"));
 QSettings settings;
 savedFilters_ = settings.value(QStringLiteral("Timeline/SavedSearchFilters")).toStringList();
 savedFilters_.removeDuplicates();
}

ArtifactTimelineSearchBarWidget::ArtifactTimelineSearchBarWidget(QWidget* parent)
  : QWidget(parent), impl_(new Impl())
 {
  auto layout = new QHBoxLayout(this);
  layout->setSpacing(6);
  layout->setContentsMargins(6, 5, 6, 5);

  impl_->searchLineEdit_->setObjectName("timelineSearchBox");
  
  // Set a clear button equivalent
  impl_->searchLineEdit_->setClearButtonEnabled(true);
  impl_->searchLineEdit_->setMinimumHeight(28);
  setFixedHeight(38);
  impl_->searchLineEdit_->installEventFilter(this);
  {
   const auto& theme = ArtifactCore::currentDCCTheme();
   QPalette searchPal = impl_->searchLineEdit_->palette();
   searchPal.setColor(QPalette::Base, QColor(theme.secondaryBackgroundColor).darker(106));
   searchPal.setColor(QPalette::Text, QColor(theme.textColor));
   searchPal.setColor(QPalette::PlaceholderText, QColor(theme.textColor).darker(155));
   searchPal.setColor(QPalette::Window, QColor(theme.secondaryBackgroundColor).darker(108));
   searchPal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor).darker(108));
   impl_->searchLineEdit_->setPalette(searchPal);
  }

 layout->addWidget(impl_->searchLineEdit_);

  setAccessibleName(QStringLiteral("Timeline search bar"));
  setAccessibleDescription(
      QStringLiteral("Search and filter timeline layers and properties"));

  setAttribute(Qt::WA_StyledBackground, true);
  setAutoFillBackground(false);

 QObject::connect(impl_->searchLineEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
   searchTextChanged(text);
  });
 }

void ArtifactTimelineSearchBarWidget::focusSearch()
{
 if (impl_ && impl_->searchLineEdit_) {
  impl_->searchLineEdit_->setFocus(Qt::ShortcutFocusReason);
  impl_->searchLineEdit_->selectAll();
 }
}

void ArtifactTimelineSearchBarWidget::clearSearch()
{
 if (impl_ && impl_->searchLineEdit_ && !impl_->searchLineEdit_->text().isEmpty()) {
  impl_->searchLineEdit_->blockSignals(true);
  impl_->searchLineEdit_->clear();
  impl_->searchLineEdit_->blockSignals(false);
  searchCleared();
 }
}

bool ArtifactTimelineSearchBarWidget::hasSearchText() const
{
 return impl_ && impl_->searchLineEdit_ && !impl_->searchLineEdit_->text().isEmpty();
}

void ArtifactTimelineSearchBarWidget::contextMenuEvent(QContextMenuEvent* event)
{
 if (!impl_ || !impl_->searchLineEdit_ || !event) return;
 QMenu menu(this);
 QAction* save = menu.addAction(QStringLiteral("Save Current Filter..."));
 if (!impl_->savedFilters_.isEmpty()) menu.addSeparator();
 QHash<QAction*, QString> applyActions;
 for (const auto& filter : impl_->savedFilters_) {
  QAction* action = menu.addAction(QStringLiteral("Apply: %1").arg(filter));
  applyActions.insert(action, filter);
 }
 QAction* chosen = menu.exec(event->globalPos());
 if (chosen == save) {
  const QString filter = impl_->searchLineEdit_->text().trimmed();
  if (filter.isEmpty()) return;
  bool accepted = false;
  const QString name = QInputDialog::getText(
      this, QStringLiteral("Save Timeline Filter"), QStringLiteral("Filter name:"),
      QLineEdit::Normal, filter, &accepted).trimmed();
  if (!accepted || name.isEmpty()) return;
  impl_->savedFilters_.removeAll(name);
  impl_->savedFilters_.append(name);
  QSettings settings;
  settings.setValue(QStringLiteral("Timeline/SavedSearchFilters"), impl_->savedFilters_);
 } else if (applyActions.contains(chosen)) {
  impl_->searchLineEdit_->setText(applyActions.value(chosen));
 }
}

 bool ArtifactTimelineSearchBarWidget::eventFilter(QObject* watched, QEvent* event)
 {
 if (watched == impl_->searchLineEdit_ && event && event->type() == QEvent::KeyPress) {
   auto* keyEvent = static_cast<QKeyEvent*>(event);
   if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
    if (keyEvent->modifiers() & Qt::ShiftModifier) {
     searchPrevRequested();
    } else {
     searchNextRequested();
    }
    return true;
   }
   if (keyEvent->key() == Qt::Key_Escape) {
    if (impl_->searchLineEdit_ && !impl_->searchLineEdit_->text().isEmpty()) {
     clearSearch();
     return true;
  }
  if (watched == impl_->searchLineEdit_ && event &&
      event->type() == QEvent::ContextMenu) {
   contextMenuEvent(static_cast<QContextMenuEvent*>(event));
   return true;
  }
 }
  }
  return QWidget::eventFilter(watched, event);
 }

 ArtifactTimelineSearchBarWidget::~ArtifactTimelineSearchBarWidget()
 {
  delete impl_;
 }

};

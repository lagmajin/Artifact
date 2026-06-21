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
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPainter>
#include <wobjectimpl.h>

module Artifact.Timeline.TimeCodeWidget;

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
 };

 ArtifactTimeCodeWidget::Impl::Impl()
 {
  timecodeLabel_ = new QLabel();
  timecodeLabel_->setText("00:00:00:00");
  frameNumberLabel_ = new QLabel();
  frameNumberLabel_->setText("0 f");
 }

 ArtifactTimeCodeWidget::ArtifactTimeCodeWidget(QWidget* parent /*= nullptr*/) : QWidget(parent), impl_(new Impl())
 {
  // Vertical layout: timecode row on top, frame number row below.
  auto layout = new QVBoxLayout();
 layout->setSpacing(1);
  layout->setContentsMargins(12, 6, 10, 6);

  impl_->timecodeLabel_->setObjectName("timeLabel");
  impl_->frameNumberLabel_->setObjectName("frameLabel");
  impl_->timecodeLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  impl_->frameNumberLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  setAttribute(Qt::WA_StyledBackground, false);
  setAutoFillBackground(false);

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
  }

  layout->addWidget(impl_->timecodeLabel_);
  layout->addWidget(impl_->frameNumberLabel_);

  setLayout(layout);
  QFont timeFont(QStringLiteral("Consolas"));
  timeFont.setStyleHint(QFont::Monospace);
  timeFont.setBold(true);
  timeFont.setPixelSize(20);
  impl_->timecodeLabel_->setFont(timeFont);

  QFont frameFont(QStringLiteral("Consolas"));
  frameFont.setStyleHint(QFont::Monospace);
  frameFont.setBold(false);
  frameFont.setPixelSize(11);
  impl_->frameNumberLabel_->setFont(frameFont);

  const QFontMetrics timeMetrics(timeFont);
  const QFontMetrics frameMetrics(frameFont);
  impl_->timecodeLabel_->setMinimumHeight(timeMetrics.height() + 4);
  impl_->frameNumberLabel_->setMinimumHeight(frameMetrics.height() + 5);
  setMinimumHeight(timeMetrics.height() + frameMetrics.height() + 18);

  const int minimumWidth = 10 + timeMetrics.horizontalAdvance(QStringLiteral("00:00:00:00")) + 8;
  setMinimumWidth(minimumWidth);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
 }

 ArtifactTimeCodeWidget::~ArtifactTimeCodeWidget()
 {
  delete impl_;
 }

 void ArtifactTimeCodeWidget::updateTimeCode(int frame)
 {
    // Default FPS for display ? could be made configurable later
    const int fps = 30;

    // Use RationalTime to represent the frame/time (value = frame count, scale = fps)
    ArtifactCore::RationalTime rt(static_cast<int64_t>(frame), static_cast<int64_t>(fps));

    // Compute hours/minutes/seconds/frames
    int totalFrames = frame;
    int ff = totalFrames % fps;
    int totalSeconds = totalFrames / fps;
    int s = totalSeconds % 60;
    int m = (totalSeconds / 60) % 60;
    int h = totalSeconds / 3600;

    QString tc = QString("%1:%2:%3:%4")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(ff, 2, 10, QChar('0'));

    if (impl_->timecodeLabel_) impl_->timecodeLabel_->setText(tc);
    if (impl_->frameNumberLabel_) impl_->frameNumberLabel_->setText(QString("%1 f").arg(totalFrames));

    // Example use of RationalTime API (kept for future extension)
    Q_UNUSED(rt);
 }

 void ArtifactTimeCodeWidget::paintEvent(QPaintEvent* event)
 {
 QPainter painter(this);
  const auto& theme = ArtifactCore::currentDCCTheme();
  painter.fillRect(rect(), QColor(theme.secondaryBackgroundColor).darker(108));
  painter.setPen(QPen(QColor(theme.borderColor).darker(115), 1));
  painter.drawRect(rect().adjusted(0, 0, -1, -1));
  QColor accent(theme.accentColor);
  accent.setAlpha(18);
  painter.fillRect(QRect(rect().left(), rect().top(), rect().width(), 2), accent);
  QWidget::paintEvent(event);
 }

 W_OBJECT_IMPL(ArtifactTimelineSearchBarWidget)

 class ArtifactTimelineSearchBarWidget::Impl {
 public:
  Impl();
  QLineEdit* searchLineEdit_ = nullptr;
 };

ArtifactTimelineSearchBarWidget::Impl::Impl()
{
 searchLineEdit_ = new QLineEdit();
 searchLineEdit_->setPlaceholderText("検索 (type:text fx:blur tag:bg parent:none visible:false)");
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
   }
  }
  return QWidget::eventFilter(watched, event);
 }

 ArtifactTimelineSearchBarWidget::~ArtifactTimelineSearchBarWidget()
 {
  delete impl_;
 }

};

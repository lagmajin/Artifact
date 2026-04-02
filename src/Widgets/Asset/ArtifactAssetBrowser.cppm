module;
#include <QFileSystemModel>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QActionGroup>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWidget>
#include <QListView>
#include <QListWidget>
#include <QToolButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QDrag>
#include <QQueue>
#include <QButtonGroup>
#include <QPixmap>
#include <QIcon>
#include <QHash>
#include <QFileInfo>
#include <QStyle>
#include <QApplication>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QApplication>
#include <QFileDialog>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QMessageBox>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <QFontMetrics>
#include <QSlider>
#include <QGroupBox>
#include <QGridLayout>
#include <QElapsedTimer>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QScrollArea>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <QHBoxLayout>
#include <QAbstractItemView>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QSignalBlocker>
#include <wobjectimpl.h>
#ifdef emit
#undef emit
#endif
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/task_group.h>
#include <mutex>

module Widgets.AssetBrowser;

import Widgets.AssetBrowser;
import Widgets.Utils.CSS;
import Artifact.Service.Project;
import Artifact.Project.Manager;
import Artifact.Project.Items;
import Artifact.Project.Cleanup;
import AssetMenuModel;
import AssetDirectoryModel;
import Utils.String.UniString;
import File.TypeDetector;
import Audio.Segment;
import Artifact.Audio.Waveform;
import MediaPlaybackController;
import Asset.Sequence;
import Reactive.Events;

namespace Artifact {

namespace {

struct SequenceMatch
{
  QString prefix;
  QString separator;
  QString digits;
  QString extension;
  int frameNumber = -1;
  int padding = 0;

  bool isValid() const
  {
    return !prefix.isEmpty() && padding > 0 && frameNumber >= 0;
  }
};

static bool parseSequenceMatch(const QString& fileName, SequenceMatch& match)
{
  static const QRegularExpression pattern(
      QStringLiteral(R"((.*?)([._-]?)(\d{2,})$)"),
      QRegularExpression::CaseInsensitiveOption);

  const QFileInfo info(fileName);
  const QString baseName = info.completeBaseName();
  const auto captured = pattern.match(baseName);
  if (!captured.hasMatch()) {
    return false;
  }

  match.prefix = captured.captured(1) + captured.captured(2);
  match.separator = captured.captured(2);
  match.digits = captured.captured(3);
  match.extension = info.suffix().toLower();
  match.padding = match.digits.size();
  match.frameNumber = match.digits.toInt();
  return match.isValid();
}

static QString sequenceDisplayName(const SequenceMatch& match)
{
  if (!match.isValid()) {
    return {};
  }

  QString display = match.prefix;
  display += QString(match.padding, QLatin1Char('#'));
  if (!match.extension.isEmpty()) {
    display += QLatin1Char('.');
    display += match.extension;
  }
  return display;
}

static QString sequenceGroupKey(const QFileInfo& info, const SequenceMatch& match)
{
  return QStringLiteral("%1|%2|%3")
      .arg(QDir::cleanPath(info.absolutePath()),
           match.prefix,
           match.extension);
}

static QString sequenceRangeLabel(int startFrame, int endFrame, int padding)
{
  const QString first = QString::number(startFrame).rightJustified(std::max(1, padding), QLatin1Char('0'));
  const QString last = QString::number(endFrame).rightJustified(std::max(1, padding), QLatin1Char('0'));
  return QStringLiteral("%1-%2").arg(first, last);
}

static bool pcm16StereoToAudioSegment(const QByteArray& pcm, int sampleRate, ArtifactCore::AudioSegment& segment)
{
  if (pcm.size() < static_cast<int>(sizeof(qint16) * 2)) {
    return false;
  }

  const int frameCount = pcm.size() / static_cast<int>(sizeof(qint16) * 2);
  if (frameCount <= 0) {
    return false;
  }

  const auto* samples = reinterpret_cast<const qint16*>(pcm.constData());
  segment.sampleRate = sampleRate > 0 ? sampleRate : 44100;
  segment.layout = ArtifactCore::AudioChannelLayout::Stereo;
  segment.channelData.clear();
  segment.channelData.resize(2);
  segment.channelData[0].resize(frameCount);
  segment.channelData[1].resize(frameCount);

  for (int i = 0; i < frameCount; ++i) {
    const int base = i * 2;
    segment.channelData[0][i] = static_cast<float>(samples[base]) / 32768.0f;
    segment.channelData[1][i] = static_cast<float>(samples[base + 1]) / 32768.0f;
  }

  return true;
}

static QIcon buildAudioWaveformIcon(const QString& filePath, const QSize& targetSize)
{
  if (filePath.isEmpty() || targetSize.isEmpty()) {
    return {};
  }

  ArtifactCore::MediaPlaybackController playback;
  if (!playback.openMediaFile(filePath)) {
    return {};
  }

  QByteArray pcm;
  pcm.reserve(32768);
  for (int i = 0; i < 48 && pcm.size() < 65536; ++i) {
    const QByteArray chunk = playback.getNextAudioFrame();
    if (chunk.isEmpty()) {
      break;
    }
    pcm.append(chunk);
  }

  ArtifactCore::AudioSegment segment;
  if (!pcm16StereoToAudioSegment(pcm, 44100, segment)) {
    return {};
  }

  Artifact::AudioWaveformGenerator generator;
  const int waveformWidth = std::max(96, targetSize.width() * 2);
  const Artifact::WaveformData waveform = generator.generate(segment, waveformWidth);
  if (waveform.peaks.isEmpty()) {
    return {};
  }

  QImage image(targetSize, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor(20, 21, 25));

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QRect frameRect = image.rect().adjusted(1, 1, -1, -1);
  painter.setPen(QPen(QColor(57, 60, 68), 1));
  painter.setBrush(QColor(28, 29, 33));
  painter.drawRoundedRect(frameRect, 8, 8);

  const QRect waveRect = frameRect.adjusted(8, 8, -8, -8);
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(14, 15, 18));
  painter.drawRoundedRect(waveRect, 6, 6);

  const QColor waveColor(92, 184, 255);
  const QColor peakColor(155, 219, 255);
  const int centerY = waveRect.center().y();
  const int halfHeight = std::max(1, waveRect.height() / 2 - 2);
  const int count = waveform.peaks.size();
  const double step = count > 0 ? static_cast<double>(waveRect.width()) / static_cast<double>(count) : 0.0;

  painter.setPen(QPen(QColor(74, 78, 88), 1));
  painter.drawLine(QPointF(waveRect.left(), centerY), QPointF(waveRect.right(), centerY));

  painter.setPen(QPen(waveColor, 1.6, Qt::SolidLine, Qt::RoundCap));
  for (int i = 0; i < count; ++i) {
    const float peak = std::clamp(waveform.peaks[i], 0.0f, 1.0f);
    const int bar = std::max(1, static_cast<int>(std::round(peak * static_cast<float>(halfHeight))));
    const int x = waveRect.left() + static_cast<int>(std::round((i + 0.5) * step));
    painter.setPen(QPen(i % 8 == 0 ? peakColor : waveColor, 1.4, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(x, centerY - bar), QPointF(x, centerY + bar));
  }

  painter.end();
  return QIcon(QPixmap::fromImage(image));
}

} // namespace

 using namespace ArtifactCore;

class AssetFileListView final : public QListView
 {
 public:
  explicit AssetFileListView(QWidget* parent = nullptr) : QListView(parent) {}

 protected:
  void startDrag(Qt::DropActions supportedActions) override
  {
   const QModelIndexList indexes = selectedIndexes();
   if (indexes.isEmpty() || !model()) {
    return;
   }

   QElapsedTimer dragTimer;
   dragTimer.start();

   std::unique_ptr<QMimeData> mimeData(model()->mimeData(indexes));
   if (!mimeData) {
    return;
   }

   auto* drag = new QDrag(this);
   drag->setMimeData(mimeData.release());

   QPixmap dragPixmap(160, 28);
   dragPixmap.fill(QColor(32, 32, 32, 220));
   {
    QPainter painter(&dragPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(255, 255, 255, 235));
    painter.drawRoundedRect(dragPixmap.rect().adjusted(0, 0, -1, -1), 6, 6);
    const QString label = indexes.size() == 1
     ? model()->data(indexes.first(), Qt::DisplayRole).toString()
     : QStringLiteral("%1 items").arg(indexes.size());
    painter.drawText(dragPixmap.rect().adjusted(10, 0, -10, 0),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     QFontMetrics(font()).elidedText(label, Qt::ElideRight, 140));
   }
   drag->setPixmap(dragPixmap);
   drag->setHotSpot(QPoint(12, dragPixmap.height() / 2));

   qDebug() << "[AssetBrowser][Drag]" << "mimeMs=" << dragTimer.elapsed()
            << "items=" << indexes.size();
   drag->exec(supportedActions, Qt::CopyAction);
  }
 };

class AssetDirectoryItemDelegate final : public QStyledItemDelegate
{
public:
  explicit AssetDirectoryItemDelegate(QObject* parent = nullptr)
      : QStyledItemDelegate(parent) {}

  static QString nodeBadge(const QModelIndex& index)
  {
    if (!index.isValid()) {
      return {};
    }

    const auto* model = qobject_cast<const AssetDirectoryModel*>(index.model());
    if (!model) {
      return {};
    }

    if (model->isVirtualNode(index)) {
      const QString name = model->nameFromIndex(index);
      return name.isEmpty() ? QStringLiteral("ROOT") : name.toUpper();
    }

    if (!model->isFolderNode(index)) {
      return QStringLiteral("FILE");
    }

    const QString path = model->pathFromIndex(index);
    if (path.isEmpty()) {
      return QStringLiteral("DIR");
    }

    if (path.contains(QStringLiteral("/Packages/"), Qt::CaseInsensitive) ||
        path.endsWith(QStringLiteral("/Packages"), Qt::CaseInsensitive)) {
      return QStringLiteral("PACKAGE");
    }

    if (path.contains(QStringLiteral("/Assets/"), Qt::CaseInsensitive) ||
        path.endsWith(QStringLiteral("/Assets"), Qt::CaseInsensitive)) {
      return QStringLiteral("ASSET");
    }

    return QStringLiteral("DIR");
  }

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override
  {
    if (!painter || !index.isValid()) {
      return;
    }

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRect rowRect = opt.rect.adjusted(4, 3, -4, -3);
    const bool selected = opt.state.testFlag(QStyle::State_Selected);
    const bool hovered = opt.state.testFlag(QStyle::State_MouseOver);
    const bool hasFocus = opt.state.testFlag(QStyle::State_HasFocus);

    QColor fill = selected ? QColor(56, 66, 82)
                           : hovered ? QColor(40, 43, 50)
                                     : QColor(28, 29, 33);
    QColor border = selected ? QColor(96, 126, 170)
                             : hovered ? QColor(70, 76, 88)
                                       : QColor(48, 50, 58);
    if (hasFocus && selected) {
      border = QColor(138, 181, 235);
    }

    painter->setPen(QPen(border, 1));
    painter->setBrush(fill);
    painter->drawRoundedRect(rowRect, 8, 8);

    const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    const QString title = index.data(Qt::DisplayRole).toString();
    const QString subtitle = index.data(Qt::ToolTipRole).toString();
    const QString badge = nodeBadge(index);

    const QRect iconRect(rowRect.left() + 10, rowRect.top() + 8, 20, 20);
    if (!icon.isNull()) {
      icon.paint(painter, iconRect, Qt::AlignCenter,
                 selected ? QIcon::Selected : QIcon::Normal,
                 QIcon::On);
    }

    const int textLeft = iconRect.right() + 10;
    const int rightPad = badge.isEmpty() ? 12 : 78;
    QRect titleRect(textLeft, rowRect.top() + 6,
                    rowRect.width() - (textLeft - rowRect.left()) - rightPad, 18);
    QRect subtitleRect(textLeft, rowRect.top() + 22,
                       rowRect.width() - (textLeft - rowRect.left()) - rightPad, 16);

    painter->setPen(selected ? QColor(250, 251, 255) : QColor(232, 235, 240));
    QFont titleFont = painter->font();
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));

    if (!subtitle.isEmpty()) {
      QFont bodyFont = painter->font();
      bodyFont.setBold(false);
      bodyFont.setPointSizeF(std::max(8.0, bodyFont.pointSizeF() - 2.0));
      painter->setFont(bodyFont);
      painter->setPen(QColor(158, 165, 178));
      painter->drawText(subtitleRect, Qt::AlignVCenter | Qt::AlignLeft,
                        painter->fontMetrics().elidedText(subtitle, Qt::ElideMiddle, subtitleRect.width()));
    }

    if (!badge.isEmpty()) {
      QFont badgeFont = painter->font();
      badgeFont.setBold(true);
      badgeFont.setPointSizeF(std::max(7.0, badgeFont.pointSizeF() - 3.0));
      painter->setFont(badgeFont);

      const int badgeWidth = std::max(48, painter->fontMetrics().horizontalAdvance(badge) + 16);
      const QRect badgeRect(rowRect.right() - badgeWidth - 10, rowRect.top() + 9,
                            badgeWidth, 18);
      painter->setPen(Qt::NoPen);
      painter->setBrush(selected ? QColor(84, 105, 136) : QColor(45, 47, 54));
      painter->drawRoundedRect(badgeRect, 9, 9);
      painter->setPen(selected ? QColor(235, 241, 252) : QColor(181, 189, 200));
      painter->drawText(badgeRect, Qt::AlignCenter, badge);
    }

    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override
  {
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(220, 42);
  }
};

 class AssetInfoPanelWidget final : public QWidget
 {
 public:
  explicit AssetInfoPanelWidget(QWidget* parent = nullptr) : QWidget(parent) {}

  void setAsset(const QString& filePath,
                const QString& title,
                const QPixmap& preview,
                const QStringList& lines,
                const QColor& accent,
                const QStringList& badges)
  {
   filePath_ = filePath;
   title_ = title;
   preview_ = preview;
   lines_ = lines;
   accent_ = accent;
   badges_ = badges;
   update();
  }

  void clearAsset()
  {
   filePath_.clear();
   title_.clear();
   preview_ = QPixmap();
   lines_.clear();
   badges_.clear();
   update();
  }

protected:
  void paintEvent(QPaintEvent*) override
  {
   QPainter p(this);
   p.setRenderHint(QPainter::Antialiasing, true);
   p.fillRect(rect(), QColor(28, 29, 33));

   const QRect content = rect().adjusted(12, 12, -12, -12);
   p.setPen(QPen(QColor(64, 67, 75), 1));
   p.setBrush(QColor(18, 19, 23));
   p.drawRoundedRect(content, 10, 10);

   QRect previewRect = content.adjusted(14, 14, -14, -content.height() / 2);
   previewRect.setWidth(std::min(180, content.width() / 2));
   previewRect.setHeight(std::min(120, content.height() / 3));

   p.setPen(QPen(accent_.isValid() ? accent_ : QColor(96, 96, 96), 2));
   p.setBrush(QColor(12, 13, 16));
   p.drawRoundedRect(previewRect, 8, 8);

   if (!preview_.isNull()) {
    const QRect target = previewRect.adjusted(8, 8, -8, -8);
    const QPixmap fitted = preview_.scaled(target.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QPoint topLeft(target.center().x() - fitted.width() / 2,
                         target.center().y() - fitted.height() / 2);
    p.drawPixmap(topLeft, fitted);
   }

   QRect textRect = content.adjusted(previewRect.width() + 24, 16, -16, -16);
   if (textRect.width() < 120) {
    textRect = content.adjusted(16, previewRect.bottom() + 16, -16, -16);
   }

   p.setPen(QColor(241, 243, 247));
   QFont titleFont = p.font();
   titleFont.setPointSizeF(titleFont.pointSizeF() > 0 ? titleFont.pointSizeF() + 2.0 : 11.0);
   titleFont.setBold(true);
   p.setFont(titleFont);
   p.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
              title_.isEmpty() ? QStringLiteral("No file selected") : title_);

   int y = textRect.top() + 34;
   QFont bodyFont = p.font();
   bodyFont.setPointSizeF(std::max(8.0, bodyFont.pointSizeF() - 2.0));
   bodyFont.setBold(false);
   p.setFont(bodyFont);
   p.setPen(QColor(180, 186, 196));

   const int lineHeight = QFontMetrics(bodyFont).height() + 3;
   for (const auto& line : lines_) {
    if (y + lineHeight > textRect.bottom()) {
     break;
    }
    p.drawText(QRect(textRect.left(), y, textRect.width(), lineHeight),
               Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, line);
    y += lineHeight;
   }

   if (!badges_.isEmpty()) {
    int badgeX = textRect.left();
    int badgeY = content.bottom() - 32;
    for (const auto& badge : badges_) {
     const QSize badgeSize(QFontMetrics(bodyFont).horizontalAdvance(badge) + 18, 22);
     QRect badgeRect(badgeX, badgeY, badgeSize.width(), badgeSize.height());
     p.setPen(Qt::NoPen);
     p.setBrush(QColor(42, 44, 50));
     p.drawRoundedRect(badgeRect, 10, 10);
     p.setPen(accent_.isValid() ? accent_ : QColor(214, 218, 226));
     p.drawText(badgeRect, Qt::AlignCenter, badge);
     badgeX += badgeSize.width() + 8;
    }
   }
  }

 private:
  QString filePath_;
  QString title_;
  QPixmap preview_;
  QStringList lines_;
  QStringList badges_;
  QColor accent_;
 };

 // ─────────────────────────────────────────────────────────
 // P1-1: Breadcrumb Navigation Widget
 // ─────────────────────────────────────────────────────────

 class ArtifactBreadcrumbWidget::Impl {
 public:
  Impl() = default;
  ~Impl() = default;

  QString currentPath_;
  QString rootPath_;
  QHBoxLayout* layout_ = nullptr;
  QList<QToolButton*> buttons_;

  void clearButtons() {
   for (auto* btn : buttons_) {
    btn->deleteLater();
   }
   buttons_.clear();
  }

  void rebuildButtons() {
   clearButtons();

   if (currentPath_.isEmpty()) return;

   QDir dir(currentPath_);
   QStringList parts;
   QString accumulated = rootPath_;

   // Build path parts from root to current
   QString relativePath = dir.absolutePath();
   if (relativePath.startsWith(rootPath_, Qt::CaseInsensitive)) {
    relativePath = relativePath.mid(rootPath_.length());
    if (relativePath.startsWith('/')) relativePath = relativePath.mid(1);
    parts = relativePath.split('/', Qt::SkipEmptyParts);
   }

   // Root button
   auto* rootBtn = new QToolButton();
   rootBtn->setText(QFileInfo(rootPath_).fileName() + " ▸");
   rootBtn->setObjectName("breadcrumbRoot");
   rootBtn->setStyleSheet(
    "QToolButton { background: transparent; color: #8888aa; border: none; padding: 4px 8px; font-size: 12px; }"
    "QToolButton:hover { color: #ffffff; background: rgba(255,255,255,0.1); border-radius: 3px; }"
   );
   rootBtn->setCursor(Qt::PointingHandCursor);
   layout_->addWidget(rootBtn);
   buttons_.append(rootBtn);

   QObject::connect(rootBtn, &QToolButton::clicked, [this, rootBtn]() {
    if (auto* breadcrumb = qobject_cast<ArtifactBreadcrumbWidget*>(rootBtn->parentWidget()->parentWidget())) {
     breadcrumb->pathClicked(rootPath_);
    }
   });

   // Build accumulated path and buttons for each part
   for (int i = 0; i < parts.size(); ++i) {
    accumulated += "/" + parts[i];
    QString partName = parts[i];
    QString clickPath = accumulated;

    auto* btn = new QToolButton();
    bool isLast = (i == parts.size() - 1);
    btn->setText(isLast ? partName : partName + " ▸");
    btn->setObjectName(isLast ? "breadcrumbCurrent" : "breadcrumbPart");
    btn->setStyleSheet(isLast
     ? "QToolButton { background: transparent; color: #ffffff; border: none; padding: 4px 8px; font-size: 12px; font-weight: bold; }"
     : "QToolButton { background: transparent; color: #8888aa; border: none; padding: 4px 8px; font-size: 12px; }"
       "QToolButton:hover { color: #ffffff; background: rgba(255,255,255,0.1); border-radius: 3px; }"
    );
    btn->setCursor(isLast ? Qt::ArrowCursor : Qt::PointingHandCursor);
    btn->setEnabled(!isLast);
    layout_->addWidget(btn);
    buttons_.append(btn);

    if (!isLast) {
     QObject::connect(btn, &QToolButton::clicked, [this, btn, clickPath]() {
      if (auto* breadcrumb = qobject_cast<ArtifactBreadcrumbWidget*>(btn->parentWidget()->parentWidget())) {
       breadcrumb->pathClicked(clickPath);
      }
     });
    }
   }

   layout_->addStretch();
  }
 };

 W_OBJECT_IMPL(ArtifactBreadcrumbWidget)

 ArtifactBreadcrumbWidget::ArtifactBreadcrumbWidget(QWidget* parent)
   : QFrame(parent), impl_(new Impl())
 {
  setFrameStyle(QFrame::NoFrame);
  setFixedHeight(28);
  setStyleSheet("background-color: #1a1a2e; border-bottom: 1px solid #333355;");

  impl_->layout_ = new QHBoxLayout(this);
  impl_->layout_->setContentsMargins(8, 2, 8, 2);
  impl_->layout_->setSpacing(2);
 }

 ArtifactBreadcrumbWidget::~ArtifactBreadcrumbWidget() { delete impl_; }

 void ArtifactBreadcrumbWidget::setPath(const QString& path) {
  impl_->currentPath_ = path;
  impl_->rebuildButtons();
 }

 void ArtifactBreadcrumbWidget::setRootPath(const QString& rootPath) {
  impl_->rootPath_ = rootPath;
  impl_->rebuildButtons();
 }


 class ArtifactAssetBrowserToolBar::Impl
 {
 private:
 public:
  Impl();
  ~Impl();
  QLineEdit* searchWidget = nullptr;
 };

 ArtifactAssetBrowserToolBar::Impl::Impl()
 {
  searchWidget = new QLineEdit();
 }

 ArtifactAssetBrowserToolBar::Impl::~Impl()
 {
 }

 W_OBJECT_IMPL(ArtifactAssetBrowserToolBar)

 ArtifactAssetBrowserToolBar::ArtifactAssetBrowserToolBar(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl())
 {
  auto layout = new QHBoxLayout();
  auto upButton = new QToolButton(this);
  upButton->setObjectName(QStringLiteral("assetBrowserUpButton"));
  upButton->setText(QStringLiteral("Up"));
  upButton->setToolTip(QStringLiteral("Go to parent folder"));
  auto refreshButton = new QToolButton(this);
  refreshButton->setObjectName(QStringLiteral("assetBrowserRefreshButton"));
  refreshButton->setText(QStringLiteral("Refresh"));
  refreshButton->setToolTip(QStringLiteral("Refresh current folder"));
  impl_->searchWidget->setPlaceholderText(QStringLiteral("Search assets..."));
  impl_->searchWidget->setClearButtonEnabled(true);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(upButton);
  layout->addWidget(refreshButton);
  layout->addWidget(impl_->searchWidget);
  setLayout(layout);
 }

 ArtifactAssetBrowserToolBar::~ArtifactAssetBrowserToolBar()
 {
  delete impl_;
 }

 class ArtifactAssetBrowser::Impl
 {
 private:
  QHash<QString, QIcon> thumbnailCache_;  // Cache thumbnails by file path
  QSize thumbnailSize_{64, 64};
  QIcon defaultFileIcon_;
  QIcon defaultImageIcon_;
  QIcon defaultVideoIcon_;
  QIcon defaultAudioIcon_;
  QIcon defaultFontIcon_;
  QSet<QString> importedAssetPaths_;
  QSet<QString> unusedAssetPaths_;
  QFileSystemWatcher* fileWatcher_ = nullptr;
  QTimer* fileWatcherDebounce_ = nullptr;
  bool fileWatcherPending_ = false;
 public:
  Impl();
  ~Impl();
  QToolButton* upButton_ = nullptr;
  QToolButton* refreshButton_ = nullptr;
  QTreeView* directoryView_ = nullptr;
  AssetDirectoryModel* directoryModel_ = nullptr;
  QListView* fileView_ = nullptr;
  AssetMenuModel* assetModel_ = nullptr;
  QLineEdit* searchEdit_ = nullptr;
  QFileSystemModel* fileModel_ = nullptr;
  QButtonGroup* filterButtonGroup_ = nullptr;
  QLabel* currentPathLabel_ = nullptr;
  ArtifactBreadcrumbWidget* breadcrumbWidget_ = nullptr;
  QLabel* browserStatusLabel_ = nullptr;
  AssetInfoPanelWidget* fileInfoPanel_ = nullptr;  // File details display
  QSlider* thumbnailSizeSlider_ = nullptr;  // Thumbnail size adjustment
  QTimer* thumbnailWarmupTimer_ = nullptr;
  QQueue<QString> thumbnailWarmupQueue_;
  QSet<QString> thumbnailWarmupPending_;
  bool listViewMode_ = false;
   QString currentSortMode_ = "name";
   QString currentDirectoryPath_;
   QString currentFileTypeFilter_ = "all";
   QString currentStatusFilter_ = "all";
   QString currentSearchFilter_;

  void handleDirectryChanged();
  void handleDoubleClicked(ArtifactAssetBrowser* owner);
  void defaultHandleMousePressEvent(QMouseEvent* event);
  void applyFilters();
  bool matchesFileTypeFilter(const QString& fileName) const;
  bool matchesSearchFilter(const QString& fileName) const;
  QIcon generateThumbnail(const QString& filePath);
  QIcon placeholderIconFor(const QString& fileName, bool isDir) const;
  void queueThumbnailWarmup(const QString& filePath);
  void processThumbnailWarmupBatch();
  QIcon getFileIcon(const QString& fileName, const QString& filePath);
  void clearThumbnailCache();
  void refreshImportedAssetCache();
   bool isImageFile(const QString& fileName) const;
   bool isVideoFile(const QString& fileName) const;
   bool isAudioFile(const QString& fileName) const;
   bool isFontFile(const QString& fileName) const;
   ArtifactCore::FileType fileType(const QString& fileName) const;
  bool isImportedAssetPath(const QString& filePath) const;
  bool isUnusedAssetPath(const QString& filePath) const;
  bool isMissingAssetPath(const QString& filePath) const;
  QStringList selectedAssetPaths() const;
  int rowForPath(const QString& filePath) const;
   std::vector<Artifact::FootageItem*> footageItemsForPaths(const QStringList& filePaths) const;
  bool removeProjectItemsForPaths(const QStringList& filePaths, QWidget* parent);
  bool relinkProjectItemsForPath(const QString& oldPath, QWidget* parent);
  void updateBrowserStatus();
  void syncProjectAssetRoot();
  void syncDirectorySelection();
  void refreshUnusedAssetCache();
  void sortItems(QList<AssetMenuItem>& items) const;
  
  std::vector<ReactiveRule> rules;
  int selectedRuleIndex = -1;
  bool updatingInspector = false;

  QString expandedSequenceKey_; // M-AB-2 Phase 2: 現在展開中のシーケンスキー
 };

 ArtifactAssetBrowser::Impl::Impl()
 {
  // Initialize default icons using Qt standard icons
  QStyle* style = QApplication::style();
  if (style) {
   defaultFileIcon_ = style->standardIcon(QStyle::SP_FileIcon);
   defaultImageIcon_ = style->standardIcon(QStyle::SP_FileIcon);
   defaultVideoIcon_ = style->standardIcon(QStyle::SP_MediaPlay);
   defaultAudioIcon_ = style->standardIcon(QStyle::SP_MediaVolume);
   defaultFontIcon_ = style->standardIcon(QStyle::SP_FileDialogDetailedView);
  }

  // P0-1: File system watcher for auto-refresh
  fileWatcher_ = new QFileSystemWatcher();
  fileWatcherDebounce_ = new QTimer();
  fileWatcherDebounce_->setSingleShot(true);
  fileWatcherDebounce_->setInterval(500); // 500ms debounce

  QObject::connect(fileWatcher_, &QFileSystemWatcher::directoryChanged,
   fileWatcherDebounce_, [this]() {
    fileWatcherPending_ = true;
    fileWatcherDebounce_->start();
   });

  QObject::connect(fileWatcher_, &QFileSystemWatcher::fileChanged,
   fileWatcherDebounce_, [this]() {
    fileWatcherPending_ = true;
    fileWatcherDebounce_->start();
   });

  QObject::connect(fileWatcherDebounce_, &QTimer::timeout,
   fileWatcherDebounce_, [this]() {
    if (fileWatcherPending_) {
     fileWatcherPending_ = false;
     // Refresh imported cache and re-apply filters
     refreshImportedAssetCache();
     refreshUnusedAssetCache();
     applyFilters();
    }
   });
 }

 ArtifactAssetBrowser::Impl::~Impl()
 {
  if (fileWatcher_) {
   fileWatcher_->deleteLater();
  }
  if (fileWatcherDebounce_) {
   fileWatcherDebounce_->deleteLater();
  }
 }

void ArtifactAssetBrowser::Impl::handleDoubleClicked(ArtifactAssetBrowser* owner)
{
  if (!fileView_ || !assetModel_ || !fileView_->selectionModel()) {
   return;
  }

  const QModelIndex currentIndex = fileView_->selectionModel()->currentIndex();
  if (!currentIndex.isValid()) {
   return;
  }

  const AssetMenuItem item = assetModel_->itemAt(currentIndex.row());
  const QString filePath = item.path.toQString();
  if (filePath.isEmpty()) {
   return;
  }
  if (owner) {
   owner->itemDoubleClicked(filePath);
  }

  if (item.isFolder) {
   currentDirectoryPath_ = filePath;
   clearThumbnailCache();
   applyFilters();
   syncDirectorySelection();
    if (owner) {
     owner->folderChanged(filePath);
    }
     return;
  }

  auto* svc = ArtifactProjectService::instance();
  if (!svc) {
   return;
  }
  const QStringList paths = item.isSequence && !item.sequencePaths.isEmpty()
      ? item.sequencePaths
      : QStringList{filePath};
  svc->importAssetsFromPaths(paths);
}

void ArtifactAssetBrowser::Impl::defaultHandleMousePressEvent(QMouseEvent* event)
{
  if (fileView_) {
   fileView_->setFocus(Qt::MouseFocusReason);
  }
  if (event) {
   event->accept();
  }
}

 bool ArtifactAssetBrowser::Impl::matchesFileTypeFilter(const QString& fileName) const
 {
  if (currentFileTypeFilter_ == "all") return true;

  QString lower = fileName.toLower();

  if (currentFileTypeFilter_ == "images") {
   return lower.endsWith(".png") || lower.endsWith(".jpg") ||
          lower.endsWith(".jpeg") || lower.endsWith(".bmp") ||
          lower.endsWith(".gif") || lower.endsWith(".tga") ||
          lower.endsWith(".tiff") || lower.endsWith(".exr");
  }
  else if (currentFileTypeFilter_ == "videos") {
   return lower.endsWith(".mp4") || lower.endsWith(".mov") ||
          lower.endsWith(".avi") || lower.endsWith(".mkv") ||
          lower.endsWith(".webm") || lower.endsWith(".flv");
  }
  else if (currentFileTypeFilter_ == "audio") {
   return lower.endsWith(".mp3") || lower.endsWith(".wav") ||
          lower.endsWith(".ogg") || lower.endsWith(".flac") ||
          lower.endsWith(".aac") || lower.endsWith(".m4a");
  }
  else if (currentFileTypeFilter_ == "fonts") {
   return lower.endsWith(".ttf") || lower.endsWith(".otf") ||
          lower.endsWith(".ttc") || lower.endsWith(".woff") ||
          lower.endsWith(".woff2");
  }

  return true;
 }

 bool ArtifactAssetBrowser::Impl::matchesSearchFilter(const QString& fileName) const
 {
  if (currentSearchFilter_.isEmpty()) return true;
  return fileName.contains(currentSearchFilter_, Qt::CaseInsensitive);
 }

 FileType ArtifactAssetBrowser::Impl::fileType(const QString& fileName) const
 {
  static FileTypeDetector detector;
  return detector.detectByExtension(fileName);
 }

 bool ArtifactAssetBrowser::Impl::isImageFile(const QString& fileName) const
 {
  return fileType(fileName) == ArtifactCore::FileType::Image;
 }

 bool ArtifactAssetBrowser::Impl::isVideoFile(const QString& fileName) const
 {
  return fileType(fileName) == ArtifactCore::FileType::Video;
 }

 bool ArtifactAssetBrowser::Impl::isAudioFile(const QString& fileName) const
 {
  return fileType(fileName) == ArtifactCore::FileType::Audio;
 }

 bool ArtifactAssetBrowser::Impl::isFontFile(const QString& fileName) const
 {
  QString lower = fileName.toLower();
  return lower.endsWith(".ttf") || lower.endsWith(".otf") ||
         lower.endsWith(".ttc") || lower.endsWith(".woff") ||
         lower.endsWith(".woff2");
 }

bool ArtifactAssetBrowser::Impl::isImportedAssetPath(const QString& filePath) const
{
  if (filePath.isEmpty()) {
   return false;
  }

  const QString canonicalTarget = QFileInfo(filePath).canonicalFilePath().isEmpty()
    ? QFileInfo(filePath).absoluteFilePath()
    : QFileInfo(filePath).canonicalFilePath();
  const QString cleanedTarget = QDir::cleanPath(canonicalTarget);
  return importedAssetPaths_.contains(cleanedTarget)
    || importedAssetPaths_.contains(QDir::cleanPath(filePath));
}

QStringList ArtifactAssetBrowser::Impl::selectedAssetPaths() const
{
  QStringList paths;
  if (!fileView_ || !assetModel_ || !fileView_->selectionModel()) {
   return paths;
  }

  const QModelIndexList selectedIndexes = fileView_->selectionModel()->selectedIndexes();
  QSet<QString> seen;
  paths.reserve(selectedIndexes.size());
  for (const QModelIndex& index : selectedIndexes) {
   const AssetMenuItem item = assetModel_->itemAt(index.row());
   if (item.isFolder) {
    continue;
   }

   const QStringList itemPaths = item.isSequence && !item.sequencePaths.isEmpty()
       ? item.sequencePaths
       : QStringList{item.path.toQString()};
   for (const QString& path : itemPaths) {
    if (!path.isEmpty() && !seen.contains(path)) {
     seen.insert(path);
     paths.append(path);
    }
   }
  }
  return paths;
}

int ArtifactAssetBrowser::Impl::rowForPath(const QString& filePath) const
{
  if (!assetModel_ || filePath.isEmpty()) {
    return -1;
  }

  const QString normalized = QDir::cleanPath(filePath);
  for (int row = 0; row < assetModel_->rowCount(); ++row) {
    const AssetMenuItem item = assetModel_->itemAt(row);
    if (QDir::cleanPath(item.path.toQString()) == normalized) {
      return row;
    }
    if (item.isSequence) {
      for (const QString& sequencePath : item.sequencePaths) {
        if (QDir::cleanPath(sequencePath) == normalized) {
          return row;
        }
      }
    }
  }
  return -1;
}

int selectedVisibleRowCount(const QItemSelectionModel* selectionModel)
{
  if (!selectionModel) {
    return 0;
  }
  return selectionModel->selectedRows().size();
}

void ArtifactAssetBrowser::Impl::updateBrowserStatus()
{
  if (!browserStatusLabel_) {
    return;
  }

  const int selectedRows = selectedVisibleRowCount(fileView_ ? fileView_->selectionModel() : nullptr);
  const QString selectedText = QStringLiteral("Selected: %1").arg(selectedRows);
  const QString typeText = QStringLiteral("Type: %1").arg(currentFileTypeFilter_);
  const QString statusText = QStringLiteral("Status: %1").arg(currentStatusFilter_);
  const QString sortText = QStringLiteral("Sort: %1").arg(currentSortMode_);
  const QString searchText = currentSearchFilter_.isEmpty()
      ? QStringLiteral("Search: -")
      : QStringLiteral("Search: %1").arg(currentSearchFilter_);
  browserStatusLabel_->setText(
      QStringLiteral("%1 | %2 | %3 | %4 | %5").arg(selectedText, typeText, statusText, sortText, searchText));
}

void ArtifactAssetBrowser::Impl::sortItems(QList<AssetMenuItem>& items) const
{
  std::sort(items.begin(), items.end(), [this](const AssetMenuItem& lhs, const AssetMenuItem& rhs) {
    if (lhs.isFolder != rhs.isFolder) {
      return lhs.isFolder;
    }

    const QString lhsName = lhs.name.toQString();
    const QString rhsName = rhs.name.toQString();
    const QString lhsType = lhs.type.toQString();
    const QString rhsType = rhs.type.toQString();

    if (currentSortMode_ == "type") {
      const int typeCompare = QString::localeAwareCompare(lhsType, rhsType);
      if (typeCompare != 0) {
        return typeCompare < 0;
      }
      return QString::localeAwareCompare(lhsName, rhsName) < 0;
    }

    const int nameCompare = QString::localeAwareCompare(lhsName, rhsName);
    if (nameCompare != 0) {
      return nameCompare < 0;
    }
    return QString::localeAwareCompare(lhsType, rhsType) < 0;
  });
}

bool ArtifactAssetBrowser::Impl::isUnusedAssetPath(const QString& filePath) const
{
  const QString canonicalPath = QFileInfo(filePath).canonicalFilePath().isEmpty()
    ? QFileInfo(filePath).absoluteFilePath()
    : QFileInfo(filePath).canonicalFilePath();
  return unusedAssetPaths_.contains(QDir::cleanPath(canonicalPath))
    || unusedAssetPaths_.contains(QDir::cleanPath(filePath));
}

bool ArtifactAssetBrowser::Impl::isMissingAssetPath(const QString& filePath) const
{
  if (filePath.isEmpty()) {
   return false;
  }
  return !QFileInfo::exists(filePath);
}

std::vector<Artifact::FootageItem*> ArtifactAssetBrowser::Impl::footageItemsForPaths(const QStringList& filePaths) const
{
  std::vector<Artifact::FootageItem*> matches;
  if (filePaths.isEmpty()) {
    return matches;
  }

  auto* svc = ArtifactProjectService::instance();
  if (!svc) {
    return matches;
  }

  std::set<Artifact::FootageItem*> seen;
  const auto roots = svc->projectItems();
  const auto normalize = [](const QString& path) {
    QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return (canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : QDir::cleanPath(canonical)).toCaseFolded();
  };

  const QStringList normalizedTargets = [&]() {
    QStringList normalized;
    normalized.reserve(filePaths.size());
    for (const QString& path : filePaths) {
      if (!path.isEmpty()) {
        normalized.append(normalize(path));
      }
    }
    normalized.removeDuplicates();
    return normalized;
  }();

  if (normalizedTargets.isEmpty()) {
    return matches;
  }

  std::function<void(ProjectItem*)> visit = [&](ProjectItem* item) {
    if (!item) {
      return;
    }
    if (item->type() == eProjectItemType::Footage) {
      auto* footage = static_cast<Artifact::FootageItem*>(item);
      const QString candidate = normalize(footage->filePath);
      if (normalizedTargets.contains(candidate) && !seen.contains(footage)) {
        seen.insert(footage);
        matches.push_back(footage);
      }
    }
    for (auto* child : item->children) {
      visit(child);
    }
  };

  for (auto* root : roots) {
    visit(root);
  }

  return matches;
}

bool ArtifactAssetBrowser::Impl::removeProjectItemsForPaths(const QStringList& filePaths, QWidget* parent)
{
  auto* svc = ArtifactProjectService::instance();
  if (!svc) {
    return false;
  }

  const auto targets = footageItemsForPaths(filePaths);
  if (targets.empty()) {
    return false;
  }

  QString message;
  if (targets.size() == 1) {
    message = svc->projectItemRemovalConfirmationMessage(targets.front());
  } else {
    message = QStringLiteral("%1 project item(s) linked to the selected file(s) will be removed from the project.\nContinue?")
                  .arg(static_cast<int>(targets.size()));
  }

  const auto response = QMessageBox::question(
      parent,
      QStringLiteral("Remove from Project"),
      message,
      QMessageBox::Yes | QMessageBox::No,
      QMessageBox::No);
  if (response != QMessageBox::Yes) {
    return false;
  }

  bool removedAny = false;
  for (auto* item : targets) {
    if (!item) {
      continue;
    }
    removedAny |= svc->removeProjectItem(item);
  }

  if (removedAny) {
    svc->projectChanged();
  }
  return removedAny;
}

bool ArtifactAssetBrowser::Impl::relinkProjectItemsForPath(const QString& oldPath, QWidget* parent)
{
  auto* svc = ArtifactProjectService::instance();
  if (!svc || oldPath.trimmed().isEmpty()) {
    return false;
  }

  const auto targets = footageItemsForPaths(QStringList{oldPath});
  if (targets.empty()) {
    return false;
  }

  const QString suggestedDir = QFileInfo(oldPath).absolutePath();
  const QString replacement = QFileDialog::getOpenFileName(
      parent,
      QStringLiteral("Relink Asset"),
      suggestedDir,
      QStringLiteral("All Files (*.*)"));
  if (replacement.isEmpty()) {
    return false;
  }

  const QString canonicalReplacement = QFileInfo(replacement).absoluteFilePath();
  for (auto* footage : targets) {
    if (footage) {
      footage->filePath = canonicalReplacement;
    }
  }
  svc->projectChanged();
  return true;
}

QIcon ArtifactAssetBrowser::Impl::generateThumbnail(const QString& filePath)
{
  // Check cache first
  if (thumbnailCache_.contains(filePath)) {
   return thumbnailCache_[filePath];
  }

  QFileInfo fileInfo(filePath);

  // For folders, use folder icon
  if (fileInfo.isDir()) {
   QStyle* style = QApplication::style();
   if (style) {
    QIcon folderIcon = style->standardIcon(QStyle::SP_DirIcon);
    thumbnailCache_[filePath] = folderIcon;
    return folderIcon;
   }
   return defaultFileIcon_;
  }

  // Generate thumbnail for image files
  if (isImageFile(fileInfo.fileName())) {
   QPixmap pixmap(filePath);
   if (pixmap.isNull()) {
    qWarning() << "[AssetBrowser] Failed to load image thumbnail via QPixmap:" << filePath;
   } else {
    QPixmap scaled = pixmap.scaled(thumbnailSize_, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QIcon icon(scaled);
    thumbnailCache_[filePath] = icon;
    return icon;
   }
  }

  // Extract first frame as thumbnail for video files
  if (isVideoFile(fileInfo.fileName())) {
      cv::VideoCapture cap(filePath.toLocal8Bit().constData());
      if (cap.isOpened()) {
          cv::Mat frame;
          if (cap.read(frame) && !frame.empty()) {
              cv::Mat rgb;
              cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
              QImage qimg(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
              QPixmap pixmap = QPixmap::fromImage(qimg).scaled(thumbnailSize_, Qt::KeepAspectRatio, Qt::SmoothTransformation);
              QIcon icon(pixmap);
              thumbnailCache_[filePath] = icon;
              return icon;
          }
      }
      // Fallback
      thumbnailCache_[filePath] = defaultVideoIcon_;
      return defaultVideoIcon_;
  }

  // For audio files, use a default audio icon
  if (isAudioFile(fileInfo.fileName())) {
   const QIcon waveformIcon = buildAudioWaveformIcon(filePath, thumbnailSize_);
   if (!waveformIcon.isNull()) {
    thumbnailCache_[filePath] = waveformIcon;
    return waveformIcon;
   }
   thumbnailCache_[filePath] = defaultAudioIcon_;
   return defaultAudioIcon_;
  }

  if (isFontFile(fileInfo.fileName())) {
   thumbnailCache_[filePath] = defaultFontIcon_;
   return defaultFontIcon_;
  }

  // Default file icon
  thumbnailCache_[filePath] = defaultFileIcon_;
  return defaultFileIcon_;
 }

 QIcon ArtifactAssetBrowser::Impl::placeholderIconFor(const QString& fileName, const bool isDir) const
 {
  if (isDir) {
   return defaultFileIcon_;
  }
  if (isImageFile(fileName)) {
   return defaultImageIcon_;
  }
  if (isVideoFile(fileName)) {
   return defaultVideoIcon_;
  }
  if (isAudioFile(fileName)) {
   return defaultAudioIcon_;
  }
  if (isFontFile(fileName)) {
   return defaultFontIcon_;
  }
  return defaultFileIcon_;
 }

 void ArtifactAssetBrowser::Impl::queueThumbnailWarmup(const QString& filePath)
 {
  if (filePath.isEmpty() || thumbnailWarmupPending_.contains(filePath)) {
   return;
  }
  thumbnailWarmupPending_.insert(filePath);
  thumbnailWarmupQueue_.enqueue(filePath);
  if (thumbnailWarmupTimer_ && !thumbnailWarmupTimer_->isActive()) {
   thumbnailWarmupTimer_->start();
  }
 }

 void ArtifactAssetBrowser::Impl::processThumbnailWarmupBatch()
 {
  if (!fileView_ || !assetModel_) {
   thumbnailWarmupQueue_.clear();
   thumbnailWarmupPending_.clear();
   return;
  }

  // P0-2: TBB parallel thumbnail generation
  // Collect batch of paths to process
  const int batchSize = std::min(16, static_cast<int>(thumbnailWarmupQueue_.size()));
  QStringList batchPaths;
  for (int i = 0; i < batchSize; ++i) {
   batchPaths.append(thumbnailWarmupQueue_.dequeue());
  }

  // Process in parallel using TBB
  std::mutex cacheMutex;
  tbb::task_group tg;

  for (const QString& path : batchPaths) {
   tg.run([this, &cacheMutex, path]() {
    // Generate thumbnail (thread-safe for QPixmap and OpenCV)
    QIcon icon;
    QFileInfo fileInfo(path);

    if (fileInfo.isDir()) {
     QStyle* style = QApplication::style();
     if (style) icon = style->standardIcon(QStyle::SP_DirIcon);
     else icon = defaultFileIcon_;
    } else if (isImageFile(fileInfo.fileName())) {
     QPixmap pixmap(path);
     if (!pixmap.isNull()) {
      icon = QIcon(pixmap.scaled(thumbnailSize_, Qt::KeepAspectRatio, Qt::SmoothTransformation));
     }
    } else if (isVideoFile(fileInfo.fileName())) {
     cv::VideoCapture cap(path.toLocal8Bit().constData());
     if (cap.isOpened()) {
      cv::Mat frame;
      if (cap.read(frame) && !frame.empty()) {
       cv::Mat rgb;
       cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
       QImage qimg(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
       QPixmap pm = QPixmap::fromImage(qimg).scaled(thumbnailSize_, Qt::KeepAspectRatio, Qt::SmoothTransformation);
       icon = QIcon(pm);
      }
     }
    } else if (isAudioFile(fileInfo.fileName())) {
     icon = buildAudioWaveformIcon(path, thumbnailSize_);
     if (icon.isNull()) icon = defaultAudioIcon_;
    } else if (isFontFile(fileInfo.fileName())) {
     icon = defaultFontIcon_;
    }

    if (icon.isNull()) icon = defaultFileIcon_;

    // Thread-safe cache update
    {
     std::lock_guard<std::mutex> lock(cacheMutex);
     thumbnailCache_[path] = icon;
    }
   });
  }

  tg.wait(); // Wait for all parallel tasks to complete

  // Remove processed paths from pending set
  for (const QString& path : batchPaths) {
   thumbnailWarmupPending_.remove(path);
  }

  // Update the view
  if (!batchPaths.isEmpty()) {
   assetModel_->refreshIcons();
  }

  // Continue warmup if there are more items
   if (thumbnailWarmupQueue_.isEmpty() && thumbnailWarmupTimer_) {
    thumbnailWarmupTimer_->stop();
   }
  }

 QIcon ArtifactAssetBrowser::Impl::getFileIcon(const QString& fileName, const QString& filePath)
 {
  return generateThumbnail(filePath);
 }

void ArtifactAssetBrowser::Impl::clearThumbnailCache()
{
  thumbnailCache_.clear();
  thumbnailWarmupQueue_.clear();
  thumbnailWarmupPending_.clear();
  if (thumbnailWarmupTimer_) {
   thumbnailWarmupTimer_->stop();
  }
}

void ArtifactAssetBrowser::Impl::refreshImportedAssetCache()
{
  importedAssetPaths_.clear();

  auto* svc = ArtifactProjectService::instance();
  if (!svc) {
    return;
  }

  std::function<void(ProjectItem*)> visit = [&](ProjectItem* item) {
    if (!item) {
      return;
    }

    if (item->type() == eProjectItemType::Footage) {
      const auto* footage = static_cast<const Artifact::FootageItem*>(item);
      if (footage) {
        const QString filePath = footage->filePath;
        if (!filePath.isEmpty()) {
          const QFileInfo info(filePath);
          const QString canonical = info.canonicalFilePath().isEmpty()
              ? QDir::cleanPath(info.absoluteFilePath())
              : QDir::cleanPath(info.canonicalFilePath());
          importedAssetPaths_.insert(QDir::cleanPath(filePath));
          importedAssetPaths_.insert(canonical);
        }
      }
    }

    for (auto* child : item->children) {
      visit(child);
    }
  };

  const auto roots = svc->projectItems();
  for (auto* root : roots) {
    visit(root);
  }
}

void ArtifactAssetBrowser::Impl::syncProjectAssetRoot()
{
  if (!directoryModel_) return;

  QString assetsPath = ArtifactProjectManager::getInstance().currentProjectAssetsPath();
  if (assetsPath.isEmpty()) {
   assetsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Assets";
  }

  QDir assetsDir(assetsPath);
  if (!assetsDir.exists()) {
   assetsDir.mkpath(".");
  }

  // P0-1: Update file system watcher
  if (fileWatcher_) {
   QStringList oldDirs = fileWatcher_->directories();
   for (const QString& dir : oldDirs) {
    fileWatcher_->removePath(dir);
   }
   // Watch the asset root and all immediate subdirectories
   fileWatcher_->addPath(assetsPath);
   for (const QFileInfo& entry : assetsDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    fileWatcher_->addPath(entry.absoluteFilePath());
   }
  }

  QString previousRoot = currentDirectoryPath_;
  directoryModel_->setAssetRootPath(assetsPath);

  if (previousRoot.isEmpty() || !QDir(previousRoot).exists() || !previousRoot.startsWith(assetsPath, Qt::CaseInsensitive)) {
   currentDirectoryPath_ = assetsPath;
  } else {
   currentDirectoryPath_ = previousRoot;
  }

  refreshUnusedAssetCache();
  refreshImportedAssetCache();
  clearThumbnailCache();
  applyFilters();
  syncDirectorySelection();
}

void ArtifactAssetBrowser::Impl::refreshUnusedAssetCache()
{
  unusedAssetPaths_.clear();
  auto* svc = ArtifactProjectService::instance();
  if (!svc) {
   return;
  }
  auto project = svc->getCurrentProjectSharedPtr();
  if (!project) {
   return;
  }
  const QStringList unused = ArtifactProjectCleanupTool::findUnusedAssetPaths(project.get());
  for (const QString& path : unused) {
   const QString canonicalPath = QFileInfo(path).canonicalFilePath().isEmpty()
    ? QFileInfo(path).absoluteFilePath()
    : QFileInfo(path).canonicalFilePath();
   unusedAssetPaths_.insert(QDir::cleanPath(path));
   unusedAssetPaths_.insert(QDir::cleanPath(canonicalPath));
  }
}

 void ArtifactAssetBrowser::Impl::syncDirectorySelection()
 {
  if (!directoryView_ || !directoryModel_ || currentDirectoryPath_.isEmpty()) {
   return;
  }

  std::function<QModelIndex(const QModelIndex&)> findByPath = [&](const QModelIndex& parent) -> QModelIndex {
   const int rowCount = directoryModel_->rowCount(parent);
   for (int row = 0; row < rowCount; ++row) {
    const QModelIndex index = directoryModel_->index(row, 0, parent);
    if (!index.isValid()) {
     continue;
    }
    const QString path = directoryModel_->pathFromIndex(index);
    if (QDir::cleanPath(path) == QDir::cleanPath(currentDirectoryPath_)) {
     return index;
    }
    if (directoryModel_->canFetchMore(index)) {
     directoryModel_->fetchMore(index);
    }
    if (const QModelIndex child = findByPath(index); child.isValid()) {
     return child;
    }
   }
   return {};
  };

  const QModelIndex matchedIndex = findByPath({});
  if (!matchedIndex.isValid()) {
   return;
  }

  directoryView_->expand(matchedIndex.parent());
  directoryView_->setCurrentIndex(matchedIndex);
  directoryView_->scrollTo(matchedIndex, QAbstractItemView::PositionAtCenter);
 }

void ArtifactAssetBrowser::Impl::applyFilters()
{
  if (!fileView_ || !assetModel_ || currentDirectoryPath_.isEmpty()) {
    return;
  }

  QDir dir(currentDirectoryPath_);
  if (!dir.exists()) {
    return;
  }

  if (breadcrumbWidget_) {
   breadcrumbWidget_->setPath(currentDirectoryPath_);
  }
  if (breadcrumbWidget_) {
   breadcrumbWidget_->setPath(currentDirectoryPath_);
  }
  if (currentPathLabel_) {
    if (expandedSequenceKey_.isEmpty()) {
      currentPathLabel_->setText(currentDirectoryPath_);
    } else {
      currentPathLabel_->setText(QStringLiteral("%1 > %2")
        .arg(currentDirectoryPath_)
        .arg(expandedSequenceKey_.split('|').last()));
    }
  }
  updateBrowserStatus();

  const bool inSequenceMode = !expandedSequenceKey_.isEmpty();

  QStringList entries = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
  const int entryCount = entries.size();

  QStringList fullPaths;
  fullPaths.reserve(entryCount);
  for (const QString& entry : entries) {
    fullPaths.append(dir.absoluteFilePath(entry));
  }

  std::vector<AssetMenuItem> builtItems(static_cast<size_t>(entryCount));
  std::vector<char> keepFlags(static_cast<size_t>(entryCount), 0);
  std::vector<char> sequenceFlags(static_cast<size_t>(entryCount), 0);
  std::vector<SequenceMatch> sequenceMatches(static_cast<size_t>(entryCount));

  tbb::parallel_for(
      tbb::blocked_range<int>(0, entryCount),
      [&](const tbb::blocked_range<int>& range) {
        for (int i = range.begin(); i != range.end(); ++i) {
          const QString& entry = entries.at(i);
          const QString& fullPath = fullPaths.at(i);
          QFileInfo fileInfo(fullPath);
          const bool isDir = fileInfo.isDir();

          if (isDir && currentFileTypeFilter_ != "all") {
            continue;
          }
          if (!matchesSearchFilter(entry)) {
            continue;
          }
          if (!isDir && !matchesFileTypeFilter(entry)) {
            continue;
          }

          AssetMenuItem item;
          item.name = UniString::fromQString(entry);
          item.path = UniString::fromQString(fullPath);
          item.type = UniString::fromQString(isDir ? QStringLiteral("Folder")
                                                   : fileInfo.suffix().toUpper());
          item.isFolder = isDir;
          item.icon = placeholderIconFor(entry, isDir);

          if (!isDir && isImageFile(entry)) {
            SequenceMatch match;
            if (parseSequenceMatch(entry, match)) {
              sequenceMatches[static_cast<size_t>(i)] = match;
              sequenceFlags[static_cast<size_t>(i)] = 1;
            }
          }

          builtItems[static_cast<size_t>(i)] = std::move(item);
          keepFlags[static_cast<size_t>(i)] = 1;
        }
      });

  struct SequenceBucket
  {
    QString key;
    QString displayName;
    QString representativePath;
    QString extension;
    QStringList paths;
    int startFrame = std::numeric_limits<int>::max();
    int endFrame = std::numeric_limits<int>::min();
    int padding = 0;
    bool emitted = false;
  };

  QHash<QString, SequenceBucket> sequenceBuckets;
  sequenceBuckets.reserve(entryCount);

  for (int i = 0; i < entryCount; ++i) {
    if (!keepFlags[static_cast<size_t>(i)] || !sequenceFlags[static_cast<size_t>(i)]) {
      continue;
    }
    const SequenceMatch match = sequenceMatches[static_cast<size_t>(i)];
    if (!match.isValid()) {
      continue;
    }

    const QString filePath = fullPaths.at(i);
    const QFileInfo info(filePath);
    const QString key = sequenceGroupKey(info, match);
    auto& bucket = sequenceBuckets[key];
    if (bucket.paths.isEmpty()) {
      bucket.key = key;
      bucket.displayName = sequenceDisplayName(match);
      bucket.representativePath = filePath;
      bucket.extension = match.extension;
      bucket.padding = match.padding;
    }
    bucket.paths.append(filePath);
    bucket.startFrame = std::min(bucket.startFrame, match.frameNumber);
    bucket.endFrame = std::max(bucket.endFrame, match.frameNumber);
    if (bucket.representativePath.isEmpty()) {
      bucket.representativePath = filePath;
    }
  }

  for (auto it = sequenceBuckets.begin(); it != sequenceBuckets.end(); ++it) {
    auto& bucket = it.value();
    std::sort(bucket.paths.begin(), bucket.paths.end(), [](const QString& lhs, const QString& rhs) {
      return QString::localeAwareCompare(lhs, rhs) < 0;
    });
    if (!bucket.paths.isEmpty()) {
      bucket.representativePath = bucket.paths.first();
    }
  }

  const auto aggregateStatus = [this](const QStringList& paths) {
    struct Status {
      bool imported = false;
      bool unused = false;
      bool missing = false;
    };

    Status status;
    status.imported = !paths.isEmpty();
    status.unused = !paths.isEmpty();
    for (const QString& path : paths) {
      const bool imported = isImportedAssetPath(path);
      const bool unused = isUnusedAssetPath(path);
      const bool missing = isMissingAssetPath(path);
      status.imported = status.imported && imported;
      status.unused = status.unused && unused;
      status.missing = status.missing || missing;
    }
    return status;
  };

  const auto decorateType = [](const QString& baseType, bool imported, bool unused, bool missing) {
    QString result = baseType;
    if (missing && imported && unused) {
      result = QStringLiteral("Missing • Imported • Unused • %1").arg(result);
    } else if (missing && imported) {
      result = QStringLiteral("Missing • Imported • %1").arg(result);
    } else if (missing) {
      result = QStringLiteral("Missing • %1").arg(result);
    } else if (imported && unused) {
      result = QStringLiteral("Imported • Unused • %1").arg(result);
    } else if (imported) {
      result = QStringLiteral("Imported • %1").arg(result);
    } else if (unused) {
      result = QStringLiteral("Unused • %1").arg(result);
    }
    return result;
  };

  QList<AssetMenuItem> items;
  items.reserve(entryCount);
  for (int i = 0; i < entryCount; ++i) {
    if (!keepFlags[static_cast<size_t>(i)]) {
      continue;
    }

    AssetMenuItem item = builtItems[static_cast<size_t>(i)];
    const QString filePath = item.path.toQString();
    if (item.isFolder) {
      items.append(std::move(item));
      continue;
    }

    if (sequenceFlags[static_cast<size_t>(i)]) {
      const SequenceMatch match = sequenceMatches[static_cast<size_t>(i)];
      if (!match.isValid()) {
        continue;
      }
      const QString key = sequenceGroupKey(QFileInfo(filePath), match);

      if (inSequenceMode) {
        if (key != expandedSequenceKey_) {
          continue; // スキップ: 別のシーケンス
        }
        // 展開モードでは、集約せず通常ファイルとして下の共通処理へ流す。
      } else {
        auto bucketIt = sequenceBuckets.find(key);
        if (bucketIt == sequenceBuckets.end()) {
          continue;
        }
        SequenceBucket& bucket = bucketIt.value();
        if (bucket.emitted) {
          continue;
        }
        bucket.emitted = true;

        if (bucket.paths.size() < 2) {
          // Not enough members to form a real sequence; fall back to the single file.
          const auto singleStatus = aggregateStatus(QStringList{filePath});
          if (currentStatusFilter_ != "all") {
            if (currentStatusFilter_ == "imported" && !singleStatus.imported) continue;
            if (currentStatusFilter_ == "missing" && !singleStatus.missing) continue;
            if (currentStatusFilter_ == "unused" && !singleStatus.unused) continue;
          }
          item.type = UniString::fromQString(decorateType(item.type.toQString(),
                                                          singleStatus.imported,
                                                          singleStatus.unused,
                                                          singleStatus.missing));
          if (isImageFile(entries.at(i)) || isVideoFile(entries.at(i))) {
            queueThumbnailWarmup(filePath);
          }
          items.append(std::move(item));
          continue;
        }

        const auto status = aggregateStatus(bucket.paths);
        if (currentStatusFilter_ != "all") {
          if (currentStatusFilter_ == "imported" && !status.imported) continue;
          if (currentStatusFilter_ == "missing" && !status.missing) continue;
          if (currentStatusFilter_ == "unused" && !status.unused) continue;
        }

        const QString rangeLabel = sequenceRangeLabel(bucket.startFrame, bucket.endFrame, bucket.padding);
        item.name = UniString::fromQString(bucket.displayName);
        item.type = UniString::fromQString(
            QStringLiteral("Sequence • %1 frames • %2 • %3")
                .arg(bucket.paths.size())
                .arg(rangeLabel)
                .arg(bucket.extension.toUpper()));
        item.isSequence = true;
        item.sequenceFrameCount = bucket.paths.size();
        item.sequenceStartFrame = bucket.startFrame;
        item.sequencePadding = bucket.padding;
        item.sequencePaths = bucket.paths;
        item.path = UniString::fromQString(bucket.representativePath);
        item.icon = placeholderIconFor(QFileInfo(bucket.representativePath).fileName(), false);
        item.type = UniString::fromQString(decorateType(item.type.toQString(),
                                                        status.imported,
                                                        status.unused,
                                                        status.missing));
        queueThumbnailWarmup(bucket.representativePath);
        items.append(std::move(item));
        continue;
      }
    } else if (inSequenceMode) {
      // 展開モード中は、ターゲットシーケンスに該当しない通常ファイルなどは表示しない
      continue;
    }

    const auto status = aggregateStatus(QStringList{filePath});
    if (currentStatusFilter_ != "all") {
      if (currentStatusFilter_ == "imported" && !status.imported) continue;
      if (currentStatusFilter_ == "missing" && !status.missing) continue;
      if (currentStatusFilter_ == "unused" && !status.unused) continue;
    }

    item.type = UniString::fromQString(decorateType(item.type.toQString(),
                                                    status.imported,
                                                    status.unused,
                                                    status.missing));
    if (isImageFile(entries.at(i)) || isVideoFile(entries.at(i))) {
      queueThumbnailWarmup(filePath);
    }
    items.append(std::move(item));
  }

  sortItems(items);
  assetModel_->setItems(items);
}

ArtifactAssetBrowser::ArtifactAssetBrowser(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setWindowTitle("AssetBrowser");

  // Enable drag and drop
  setAcceptDrops(true);

  impl_->thumbnailWarmupTimer_ = new QTimer(this);
  impl_->thumbnailWarmupTimer_->setInterval(16);
  impl_->thumbnailWarmupTimer_->setSingleShot(false);
  QObject::connect(impl_->thumbnailWarmupTimer_, &QTimer::timeout, this, [this]() {
   impl_->processThumbnailWarmupBatch();
  });

  auto assetToolBar = new ArtifactAssetBrowserToolBar();
  impl_->searchEdit_ = assetToolBar->findChild<QLineEdit*>();
  impl_->upButton_ = assetToolBar->findChild<QToolButton*>(QStringLiteral("assetBrowserUpButton"));
  impl_->refreshButton_ = assetToolBar->findChild<QToolButton*>(QStringLiteral("assetBrowserRefreshButton"));
  auto viewModeButton = new QToolButton();
  viewModeButton->setObjectName(QStringLiteral("assetBrowserViewModeButton"));
  viewModeButton->setCheckable(true);
  viewModeButton->setChecked(false);
  viewModeButton->setText(QStringLiteral("List"));
  viewModeButton->setToolTip(QStringLiteral("Toggle asset view mode"));

  auto sortModeButton = new QToolButton();
  sortModeButton->setObjectName(QStringLiteral("assetBrowserSortModeButton"));
  sortModeButton->setPopupMode(QToolButton::InstantPopup);
  sortModeButton->setText(QStringLiteral("Sort: Name"));
  sortModeButton->setToolTip(QStringLiteral("Sort assets"));
  auto* sortMenu = new QMenu(sortModeButton);
  auto* sortGroup = new QActionGroup(sortModeButton);
  sortGroup->setExclusive(true);
  auto* sortByNameAction = sortMenu->addAction(QStringLiteral("Name"));
  sortByNameAction->setCheckable(true);
  sortByNameAction->setChecked(true);
  auto* sortByTypeAction = sortMenu->addAction(QStringLiteral("Type"));
  sortByTypeAction->setCheckable(true);
  sortGroup->addAction(sortByNameAction);
  sortGroup->addAction(sortByTypeAction);
  sortModeButton->setMenu(sortMenu);

  // File type filter buttons
  auto filterButtonsLayout = new QHBoxLayout();
  impl_->filterButtonGroup_ = new QButtonGroup(this);

  auto allButton = new QToolButton();
  allButton->setText("All");
  allButton->setCheckable(true);
  allButton->setChecked(true);

  auto imagesButton = new QToolButton();
  imagesButton->setText("Images");
  imagesButton->setCheckable(true);

  auto videosButton = new QToolButton();
  videosButton->setText("Videos");
  videosButton->setCheckable(true);

  auto audioButton = new QToolButton();
  audioButton->setText("Audio");
  audioButton->setCheckable(true);

  auto fontsButton = new QToolButton();
  fontsButton->setText("Fonts");
  fontsButton->setCheckable(true);

  impl_->filterButtonGroup_->addButton(allButton, 0);
  impl_->filterButtonGroup_->addButton(imagesButton, 1);
  impl_->filterButtonGroup_->addButton(videosButton, 2);
  impl_->filterButtonGroup_->addButton(audioButton, 3);
  impl_->filterButtonGroup_->addButton(fontsButton, 4);

   filterButtonsLayout->addWidget(allButton);
   filterButtonsLayout->addWidget(imagesButton);
   filterButtonsLayout->addWidget(videosButton);
   filterButtonsLayout->addWidget(audioButton);
   filterButtonsLayout->addWidget(fontsButton);

   // Status filter: All / Imported / Missing / Unused
   auto* sep = new QFrame();
   sep->setFrameShape(QFrame::VLine);
   sep->setFixedHeight(20);
   filterButtonsLayout->addWidget(sep);

   auto statusAllBtn = new QToolButton();
   statusAllBtn->setText("Status: All");
   statusAllBtn->setCheckable(true);
   statusAllBtn->setChecked(true);

   auto importedBtn = new QToolButton();
   importedBtn->setText("Imported");
   importedBtn->setCheckable(true);

   auto missingBtn = new QToolButton();
   missingBtn->setText("Missing");
   missingBtn->setCheckable(true);

   auto unusedBtn = new QToolButton();
   unusedBtn->setText("Unused");
   unusedBtn->setCheckable(true);

   auto* statusGroup = new QButtonGroup(this);
   statusGroup->setExclusive(true);
   statusGroup->addButton(statusAllBtn, 0);
   statusGroup->addButton(importedBtn, 1);
   statusGroup->addButton(missingBtn, 2);
   statusGroup->addButton(unusedBtn, 3);

   filterButtonsLayout->addWidget(statusAllBtn);
   filterButtonsLayout->addWidget(importedBtn);
   filterButtonsLayout->addWidget(missingBtn);
   filterButtonsLayout->addWidget(unusedBtn);
   filterButtonsLayout->addStretch();

   connect(statusGroup, &QButtonGroup::idClicked, this, [this](int id) {
    switch (id) {
     case 0: impl_->currentStatusFilter_ = "all"; break;
     case 1: impl_->currentStatusFilter_ = "imported"; break;
     case 2: impl_->currentStatusFilter_ = "missing"; break;
     case 3: impl_->currentStatusFilter_ = "unused"; break;
    }
    impl_->applyFilters();
   });

  auto vLayout = new QVBoxLayout();

  auto layout = new QHBoxLayout();

  auto directoryView = impl_->directoryView_ = new QTreeView();
  auto directoryModel = impl_->directoryModel_ = new AssetDirectoryModel(this);

  QString assetsPath = ArtifactProjectManager::getInstance().currentProjectAssetsPath();
  if (assetsPath.isEmpty()) {
   assetsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Assets";
  }
  QString packagesPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Packages";

  directoryModel->setAssetRootPath(assetsPath);
  directoryModel->setPackageRootPath(packagesPath);

  directoryView->setModel(directoryModel);
  directoryView->setItemDelegate(new AssetDirectoryItemDelegate(directoryView));
  directoryView->setHeaderHidden(true);
  directoryView->setSelectionBehavior(QAbstractItemView::SelectRows);
  directoryView->setSelectionMode(QAbstractItemView::SingleSelection);
  directoryView->setIndentation(15);
  directoryView->setExpandsOnDoubleClick(true);
  directoryView->setAnimated(true);
  directoryView->setUniformRowHeights(true);
  directoryView->setAllColumnsShowFocus(true);
  directoryView->setMouseTracking(true);
  directoryView->setAcceptDrops(true);
  directoryView->setDropIndicatorShown(true);
  directoryView->setDragDropMode(QAbstractItemView::DropOnly);

  QString desktopPath = assetsPath;

  auto assetPathLabel = new QLabel("Assets");
  assetPathLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  // P1-1: Breadcrumb navigation widget (replaces old path label)
  auto breadcrumb = impl_->breadcrumbWidget_ = new ArtifactBreadcrumbWidget();
  breadcrumb->setRootPath(assetsPath);
  breadcrumb->setPath(desktopPath);
  connect(breadcrumb, &ArtifactBreadcrumbWidget::pathClicked, this, [this](const QString& path) {
   if (path.isEmpty() || path == impl_->currentDirectoryPath_) return;
   impl_->currentDirectoryPath_ = path;
   impl_->clearThumbnailCache();
   impl_->applyFilters();
   impl_->syncDirectorySelection();
   folderChanged(path);
  });

  auto browserStatusLabel = impl_->browserStatusLabel_ = new QLabel(
      QStringLiteral("Selected: 0 | Type: all | Status: all | Sort: name | Search: -"));
  browserStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  browserStatusLabel->setWordWrap(true);

  auto assetModel = impl_->assetModel_ = new AssetMenuModel(this);
  auto fileView = impl_->fileView_ = new AssetFileListView();
  fileView->setModel(assetModel);
  impl_->currentDirectoryPath_ = desktopPath;  // Set initial directory
  auto applyViewMode = [fileView, viewModeButton, this](bool listMode) {
   impl_->listViewMode_ = listMode;
   fileView->setViewMode(listMode ? QListView::ListMode : QListView::IconMode);
   fileView->setFlow(listMode ? QListView::TopToBottom : QListView::LeftToRight);
   fileView->setWrapping(!listMode);
   fileView->setResizeMode(QListView::Adjust);
   fileView->setSelectionMode(QAbstractItemView::ExtendedSelection);
   fileView->setWordWrap(!listMode);
   fileView->setUniformItemSizes(true);
   if (listMode) {
    fileView->setIconSize(QSize(28, 28));
    fileView->setGridSize(QSize(260, 40));
    fileView->setSpacing(2);
    viewModeButton->setText(QStringLiteral("Icon"));
   } else {
    fileView->setIconSize(QSize(64, 64));
    fileView->setGridSize(QSize(100, 100));
    fileView->setSpacing(5);
    viewModeButton->setText(QStringLiteral("List"));
   }
  };
  applyViewMode(false);
  fileView->setTextElideMode(Qt::ElideMiddle);  // Show "longfile...name.png"
  fileView->setWordWrap(true);
  fileView->setDragEnabled(true);
  fileView->setAcceptDrops(true);
  fileView->setDropIndicatorShown(true);
  fileView->setDragDropMode(QAbstractItemView::DragDrop);
  fileView->setDefaultDropAction(Qt::CopyAction);
  fileView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  fileView->setContextMenuPolicy(Qt::CustomContextMenu);  // Enable custom context menu

  connect(viewModeButton, &QToolButton::toggled, this, [applyViewMode](bool checked) {
   applyViewMode(checked);
  });

  connect(sortGroup, &QActionGroup::triggered, this, [this, sortModeButton](QAction* action) {
   if (!action) {
    return;
   }
   if (action->text() == QStringLiteral("Type")) {
    impl_->currentSortMode_ = "type";
    sortModeButton->setText(QStringLiteral("Sort: Type"));
   } else {
    impl_->currentSortMode_ = "name";
    sortModeButton->setText(QStringLiteral("Sort: Name"));
   }
   impl_->applyFilters();
  });

  // Connect search filter
  if (impl_->searchEdit_) {
   connect(impl_->searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
    impl_->currentSearchFilter_ = text;
    impl_->applyFilters();
   });
  }

  if (impl_->upButton_) {
   connect(impl_->upButton_, &QToolButton::clicked, this, [this]() {
    if (!impl_->expandedSequenceKey_.isEmpty()) {
     impl_->expandedSequenceKey_.clear();
     impl_->applyFilters();
     return;
    }
    if (impl_->currentDirectoryPath_.isEmpty()) return;
    const QString assetsRoot = ArtifactProjectManager::getInstance().currentProjectAssetsPath();
    const QDir currentDir(impl_->currentDirectoryPath_);
    QString nextPath = QFileInfo(currentDir.absolutePath()).dir().absolutePath();
    if (nextPath.isEmpty()) {
     nextPath = assetsRoot;
    }
   if (!assetsRoot.isEmpty() && !nextPath.startsWith(assetsRoot, Qt::CaseInsensitive)) {
     nextPath = assetsRoot;
    }
    if (nextPath.isEmpty() || nextPath == impl_->currentDirectoryPath_) return;
    impl_->currentDirectoryPath_ = nextPath;
    impl_->clearThumbnailCache();
    impl_->applyFilters();
    impl_->syncDirectorySelection();
    folderChanged(nextPath);
   });
  }

  if (impl_->refreshButton_) {
   connect(impl_->refreshButton_, &QToolButton::clicked, this, [this]() {
    impl_->clearThumbnailCache();
    impl_->applyFilters();
   });
  }

  // Connect file type filter buttons
  connect(impl_->filterButtonGroup_, &QButtonGroup::idClicked, this, [this](int id) {
   switch(id) {
    case 0: impl_->currentFileTypeFilter_ = "all"; break;
   case 1: impl_->currentFileTypeFilter_ = "images"; break;
   case 2: impl_->currentFileTypeFilter_ = "videos"; break;
   case 3: impl_->currentFileTypeFilter_ = "audio"; break;
   case 4: impl_->currentFileTypeFilter_ = "fonts"; break;
   }
   impl_->applyFilters();
  });

  // Connect directory change to update file list (LEFT -> RIGHT widget coordination)
  connect(directoryView, &QTreeView::clicked, this, [this, directoryModel](const QModelIndex& index) {
   QString path = directoryModel->pathFromIndex(index);

   if (!path.isEmpty()) {
    impl_->currentDirectoryPath_ = path;
    impl_->clearThumbnailCache();
    impl_->applyFilters();
    impl_->syncDirectorySelection();
    folderChanged(path);
   }
  });

  // Connect file double-click to add to project or navigate into folder
  connect(fileView, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
   if (!index.isValid()) return;
   AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
   QString filePath = item.path.toQString();
   if (filePath.isEmpty()) return;
   itemDoubleClicked(filePath);

   // If it's a sequence, expand it
   if (item.isSequence) {
    SequenceMatch sm;
    if (parseSequenceMatch(QFileInfo(filePath).fileName(), sm)) {
     impl_->expandedSequenceKey_ = sequenceGroupKey(QFileInfo(filePath), sm);
     impl_->applyFilters();
     return;
    }
   }

   // If it's a folder, navigate into it
   if (item.isFolder) {
    impl_->currentDirectoryPath_ = filePath;
    impl_->clearThumbnailCache();
    impl_->applyFilters();
    impl_->syncDirectorySelection();
    this->folderChanged(filePath);
    return;
   }

   // Otherwise, add file to project
   auto* svc = ArtifactProjectService::instance();
   if (!svc) return;
   svc->importAssetsFromPaths(QStringList() << filePath);
  });

  // Connect right-click context menu
  connect(fileView, &QListView::customContextMenuRequested, this, &ArtifactAssetBrowser::showContextMenu);

  // Connect file item selection to update details
  connect(fileView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
   const QStringList selectedFiles = impl_->selectedAssetPaths();
   if (!selectedFiles.isEmpty()) {
    updateFileInfo(selectedFiles.first());
   } else if (impl_->fileInfoPanel_) {
    impl_->fileInfoPanel_->clearAsset();
   }
   impl_->updateBrowserStatus();
   selectionChanged(selectedFiles);
  });

  // Create thumbnail size adjustment
  auto thumbnailControlGroup = new QGroupBox("Thumbnail Size");
  auto thumbnailLayout = new QHBoxLayout();

  auto sizeLabel = new QLabel("64px");
  auto sizeSlider = impl_->thumbnailSizeSlider_ = new QSlider(Qt::Horizontal);
  sizeSlider->setMinimum(32);  // Min 32px
  sizeSlider->setMaximum(256);  // Max 256px
  sizeSlider->setValue(64);  // Default 64px
  sizeSlider->setTickPosition(QSlider::TicksBelow);
  sizeSlider->setTickInterval(32);

  connect(sizeSlider, &QSlider::valueChanged, this, [this, sizeLabel, fileView](int value) {
   sizeLabel->setText(QString("%1px").arg(value));

   // Update icon size and grid size
   fileView->setIconSize(QSize(value, value));
   int gridSize = value + 36;  // Add padding for text and spacing
   fileView->setGridSize(QSize(gridSize, gridSize));
  });

  thumbnailLayout->addWidget(new QLabel("Size:"));
  thumbnailLayout->addWidget(sizeSlider);
  thumbnailLayout->addWidget(sizeLabel);
  thumbnailControlGroup->setLayout(thumbnailLayout);

  // Create file info panel
  auto fileInfoGroup = new QGroupBox("File Details");
  auto fileInfoLayout = new QVBoxLayout();
  impl_->fileInfoPanel_ = new AssetInfoPanelWidget();
  impl_->fileInfoPanel_->setMinimumHeight(180);
  fileInfoLayout->addWidget(impl_->fileInfoPanel_);
  fileInfoGroup->setLayout(fileInfoLayout);
  fileInfoGroup->setMinimumHeight(220);

  auto* projectService = ArtifactProjectService::instance();
  if (projectService) {
   if (projectService->hasProject()) {
    impl_->syncProjectAssetRoot();
   } else {
    impl_->applyFilters();
   }
   connect(projectService, &ArtifactProjectService::projectCreated, this, [this]() {
    impl_->syncProjectAssetRoot();
   });
   connect(projectService, &ArtifactProjectService::projectChanged, this, [this]() {
    impl_->syncProjectAssetRoot();
   });
  } else {
   impl_->applyFilters();
  }

  auto toolbarRow = new QHBoxLayout();
  toolbarRow->setContentsMargins(0, 0, 0, 0);
  toolbarRow->addWidget(assetToolBar, 1);
  toolbarRow->addWidget(sortModeButton, 0);
  toolbarRow->addWidget(viewModeButton, 0);

  auto VBoxLayout = new  QVBoxLayout();
  VBoxLayout->addWidget(assetPathLabel);
  VBoxLayout->addWidget(breadcrumb);
  VBoxLayout->addWidget(browserStatusLabel);
  VBoxLayout->addWidget(thumbnailControlGroup);
  VBoxLayout->addWidget(fileView);
  VBoxLayout->addWidget(fileInfoGroup);

  vLayout->addLayout(toolbarRow);
  vLayout->addLayout(filterButtonsLayout);
  layout->addWidget(directoryView, 1);
  layout->addLayout(VBoxLayout, 3);
  setLayout(layout);
 }

 ArtifactAssetBrowser::~ArtifactAssetBrowser()
 {
  delete impl_;
 }

 QSize ArtifactAssetBrowser::sizeHint() const
 {
  return QSize(250, 600);
 }

 W_OBJECT_IMPL(ArtifactAssetBrowser)

 void ArtifactAssetBrowser::mousePressEvent(QMouseEvent* event)
 {
  impl_->defaultHandleMousePressEvent(event);
 }

 void ArtifactAssetBrowser::keyPressEvent(QKeyEvent* event)
 {
  if (!impl_->fileView_ || !impl_->assetModel_) {
   QWidget::keyPressEvent(event);
   return;
  }

  const auto* sel = impl_->fileView_->selectionModel();

  // Ctrl+A — 全選択
  if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
   impl_->fileView_->selectAll();
   event->accept();
   return;
  }

  // Ctrl+C — パスをコピー
  if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
   QStringList paths = impl_->selectedAssetPaths();
   if (!paths.isEmpty()) {
    QApplication::clipboard()->setText(paths.join("\n"));
   }
   event->accept();
   return;
  }

  // Ctrl+V — クリップボードのファイルをインポート
  if (event->key() == Qt::Key_V && (event->modifiers() & Qt::ControlModifier)) {
   const QMimeData* mime = QApplication::clipboard()->mimeData();
   if (mime && mime->hasUrls()) {
    QStringList paths;
    for (const QUrl& url : mime->urls()) {
     if (url.isLocalFile()) paths.append(url.toLocalFile());
    }
    if (!paths.isEmpty() && ArtifactProjectService::instance()) {
     ArtifactProjectService::instance()->importAssetsFromPaths(paths);
    }
   }
   event->accept();
   return;
  }

  // Delete — 選択ファイルをプロジェクトから削除
  if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
   QStringList paths = impl_->selectedAssetPaths();
   if (!paths.isEmpty()) {
    if (!impl_->removeProjectItemsForPaths(paths, this)) {
     qWarning() << "[AssetBrowser] Delete requested, but no imported project items matched the selected files.";
    } else {
     impl_->refreshUnusedAssetCache();
     impl_->applyFilters();
    }
   }
   event->accept();
   return;
  }

  // Enter — ダブルクリック相当
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
   if (sel && sel->hasSelection()) {
    QModelIndex idx = sel->currentIndex();
    if (idx.isValid()) {
     impl_->handleDoubleClicked(this);
    }
   }
   event->accept();
   return;
  }

  QWidget::keyPressEvent(event);
 }

 void ArtifactAssetBrowser::keyReleaseEvent(QKeyEvent* event)
 {
 }

 void ArtifactAssetBrowser::dragEnterEvent(QDragEnterEvent* event)
 {
  // Accept file drops from external sources
  if (event->mimeData()->hasUrls()) {
   event->acceptProposedAction();
  }
 }

 void ArtifactAssetBrowser::dropEvent(QDropEvent* event)
 {
  const QMimeData* mimeData = event->mimeData();

  if (mimeData->hasUrls()) {
   QStringList filePaths;
   QList<QUrl> urls = mimeData->urls();

   for (const QUrl& url : urls) {
    if (url.isLocalFile()) {
     QString filePath = url.toLocalFile();
     filePaths.append(filePath);
    }
   }

   if (!filePaths.isEmpty()) {
    auto* svc = ArtifactProjectService::instance();
    if (svc) {
     QStringList imported = svc->importAssetsFromPaths(filePaths);
     if (!imported.isEmpty()) {
      filesDropped(imported);
     }
    }
    // Refresh file view
    impl_->applyFilters();
   }

   event->acceptProposedAction();
  }
 }

 void ArtifactAssetBrowser::setSearchFilter(const QString& filter)
 {
  impl_->currentSearchFilter_ = filter;
  if (impl_->searchEdit_) {
   impl_->searchEdit_->setText(filter);
  }
  impl_->applyFilters();
 }

  void ArtifactAssetBrowser::setFileTypeFilter(const QString& type)
  {
   impl_->currentFileTypeFilter_ = type;

   // Update button state
   if (impl_->filterButtonGroup_) {
    if (type == "all") impl_->filterButtonGroup_->button(0)->setChecked(true);
    else if (type == "images") impl_->filterButtonGroup_->button(1)->setChecked(true);
    else if (type == "videos") impl_->filterButtonGroup_->button(2)->setChecked(true);
    else if (type == "audio") impl_->filterButtonGroup_->button(3)->setChecked(true);
    else if (type == "fonts") impl_->filterButtonGroup_->button(4)->setChecked(true);
   }

   impl_->applyFilters();
  }

  void ArtifactAssetBrowser::setStatusFilter(const QString& status)
  {
   impl_->currentStatusFilter_ = status;
   impl_->applyFilters();
  }

  void ArtifactAssetBrowser::navigateToFolder(const QString& folderPath)
  {
   if (folderPath.isEmpty() || !QDir(folderPath).exists()) return;
   impl_->currentDirectoryPath_ = folderPath;
   impl_->clearThumbnailCache();
   impl_->applyFilters();
   impl_->syncDirectorySelection();
    this->folderChanged(folderPath);
  }

  void ArtifactAssetBrowser::selectAssetPaths(const QStringList& filePaths)
  {
   if (!impl_ || !impl_->fileView_ || !impl_->assetModel_ || filePaths.isEmpty()) {
    return;
   }

   const QString firstPath = filePaths.first().trimmed();
   if (firstPath.isEmpty()) {
    return;
   }

   const QFileInfo info(firstPath);
   if (info.exists() && info.isDir()) {
    navigateToFolder(info.absoluteFilePath());
    return;
   }

   const QString targetFolder = info.dir().absolutePath();
   if (!targetFolder.isEmpty() && QDir(targetFolder).exists() &&
       QDir::cleanPath(targetFolder) != QDir::cleanPath(impl_->currentDirectoryPath_)) {
    impl_->currentDirectoryPath_ = targetFolder;
    impl_->clearThumbnailCache();
    impl_->applyFilters();
    impl_->syncDirectorySelection();
   }

   const int row = impl_->rowForPath(firstPath);
   if (row < 0) {
    return;
   }

   const QModelIndex index = impl_->assetModel_->index(row, 0);
   if (!index.isValid()) {
    return;
   }

   if (auto* sel = impl_->fileView_->selectionModel()) {
    QSignalBlocker blocker(sel);
    sel->clearSelection();
    sel->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
   }
   impl_->fileView_->scrollTo(index, QAbstractItemView::PositionAtCenter);
   updateFileInfo(firstPath);
   impl_->updateBrowserStatus();
  }

 void ArtifactAssetBrowser::updateFileInfo(const QString& filePath)
 {
  if (filePath.isEmpty() || !impl_->fileInfoPanel_) return;

  QFileInfo fileInfo(filePath);

  if (!fileInfo.exists()) {
   impl_->fileInfoPanel_->setAsset(filePath,
                                   QStringLiteral("File not found"),
                                   QPixmap(),
                                   {QStringLiteral("Path: %1").arg(filePath)},
                                   QColor(150, 74, 74),
                                   {QStringLiteral("Missing")});
   return;
  }

  QStringList lines;
  QStringList badges;
  QColor accent = QColor(96, 96, 96);
  bool imported = impl_->isImportedAssetPath(filePath);
  bool unused = impl_->isUnusedAssetPath(filePath);
  bool missing = impl_->isMissingAssetPath(filePath);
  const QString fileName = fileInfo.fileName();
  AssetMenuItem modelItem;
  const int modelRow = impl_->rowForPath(filePath);
  if (modelRow >= 0 && impl_->assetModel_) {
   modelItem = impl_->assetModel_->itemAt(modelRow);
  }
  if (modelItem.isSequence && !modelItem.sequencePaths.isEmpty()) {
   imported = true;
   unused = true;
   missing = false;
   for (const QString& sequencePath : modelItem.sequencePaths) {
    imported = imported && impl_->isImportedAssetPath(sequencePath);
    unused = unused && impl_->isUnusedAssetPath(sequencePath);
    missing = missing || impl_->isMissingAssetPath(sequencePath);
   }
  }

  // Check if it's a folder
  if (fileInfo.isDir()) {
   badges << QStringLiteral("Folder");
   accent = QColor(176, 138, 46);
   lines << QStringLiteral("Entries: %1").arg(QDir(filePath).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).size());
   lines << QStringLiteral("Project: %1").arg(imported ? QStringLiteral("Imported") : QStringLiteral("Not Imported"));
   lines << QStringLiteral("Usage: %1").arg(unused ? QStringLiteral("Unused") : QStringLiteral("In Use / N.A."));
   lines << QStringLiteral("Status: %1").arg(missing ? QStringLiteral("Missing") : QStringLiteral("OK"));
   const QPixmap preview = impl_->generateThumbnail(filePath).pixmap(160, 120);
   impl_->fileInfoPanel_->setAsset(filePath, fileName, preview, lines, accent, badges);
   return;
  }

  lines << QStringLiteral("Size: %1 KB").arg(fileInfo.size() / 1024);
  lines << QStringLiteral("Type: %1").arg(fileInfo.suffix().toUpper());
  lines << QStringLiteral("Modified: %1").arg(fileInfo.lastModified().toString("yyyy-MM-dd hh:mm"));
  lines << QStringLiteral("Project: %1").arg(imported ? QStringLiteral("Imported") : QStringLiteral("Not Imported"));
  lines << QStringLiteral("Usage: %1").arg(unused ? QStringLiteral("Unused") : QStringLiteral("In Use"));
  lines << QStringLiteral("Status: %1").arg(missing ? QStringLiteral("Missing") : QStringLiteral("OK"));
  badges << (impl_->isImageFile(fileName) ? QStringLiteral("Image")
                : impl_->isVideoFile(fileName) ? QStringLiteral("Video")
                : impl_->isAudioFile(fileName) ? QStringLiteral("Audio")
                : impl_->isFontFile(fileName) ? QStringLiteral("Font")
                : QStringLiteral("File"));

  // Get image resolution for image files
  const QString lowerName = fileName.toLower();

  if (lowerName.endsWith(".png") || lowerName.endsWith(".jpg") ||
      lowerName.endsWith(".jpeg") || lowerName.endsWith(".bmp") ||
      lowerName.endsWith(".gif") || lowerName.endsWith(".tga") ||
      lowerName.endsWith(".tiff") || lowerName.endsWith(".exr")) {
   QImageReader imageReader(filePath);
   const QSize imageSize = imageReader.size();
   if (imageSize.isValid()) {
    lines << QStringLiteral("Resolution: %1 x %2 px").arg(imageSize.width()).arg(imageSize.height());
    const QByteArray imageFormat = imageReader.format();
    if (!imageFormat.isEmpty()) {
     lines << QStringLiteral("Format: %1").arg(QString::fromLatin1(imageFormat).toUpper());
    }
   }
   accent = QColor(66, 148, 98);
  }
  else if (impl_->isVideoFile(fileName)) {
   cv::VideoCapture cap(filePath.toLocal8Bit().constData());
   if (cap.isOpened()) {
    const double width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    const double height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    const double fps = cap.get(cv::CAP_PROP_FPS);
    const double frameCount = cap.get(cv::CAP_PROP_FRAME_COUNT);
    if (width > 0.0 && height > 0.0) {
     lines << QStringLiteral("Resolution: %1 x %2 px").arg(static_cast<int>(width)).arg(static_cast<int>(height));
    }
    if (fps > 0.0) {
     lines << QStringLiteral("FPS: %1").arg(QString::number(fps, 'f', fps == std::floor(fps) ? 0 : 2));
    }
    if (fps > 0.0 && frameCount > 0.0) {
     lines << QStringLiteral("Duration: %1 s").arg(QString::number(frameCount / fps, 'f', 2));
    }
   }
   accent = QColor(74, 128, 191);
  }
  else if (impl_->isAudioFile(fileName)) {
   lines << QStringLiteral("Kind: Audio");
   accent = QColor(170, 90, 48);
  }
  else if (impl_->isFontFile(fileName)) {
   lines << QStringLiteral("Kind: Font");
   accent = QColor(121, 82, 168);
  } else {
   accent = QColor(96, 96, 96);
  }

  if (modelItem.isSequence) {
   badges.prepend(QStringLiteral("Sequence"));
   lines.prepend(QStringLiteral("Frames: %1").arg(modelItem.sequenceFrameCount));
   if (!modelItem.sequencePaths.isEmpty()) {
    const QString firstFrame = QFileInfo(modelItem.sequencePaths.first()).fileName();
    const QString lastFrame = QFileInfo(modelItem.sequencePaths.last()).fileName();
    lines.prepend(QStringLiteral("Range: %1 .. %2").arg(firstFrame, lastFrame));
    lines.prepend(QStringLiteral("Representative: %1").arg(QFileInfo(modelItem.path.toQString()).fileName()));
   }
   accent = QColor(88, 132, 190);
  }

  if (imported) {
   badges << QStringLiteral("Imported");
  } else {
   badges << QStringLiteral("Not Imported");
  }
  if (unused) {
   badges << QStringLiteral("Unused");
  }
  if (missing) {
   badges << QStringLiteral("Missing");
   accent = QColor(150, 74, 74);
  } else {
   badges << QStringLiteral("OK");
  }

  const QPixmap preview = impl_->generateThumbnail(filePath).pixmap(160, 120);
  impl_->fileInfoPanel_->setAsset(filePath, fileName, preview, lines, accent, badges);
}

 void ArtifactAssetBrowser::showContextMenu(const QPoint& pos)
 {
  QModelIndex index = impl_->fileView_->indexAt(pos);
  if (!index.isValid()) return;  // No item under cursor

  AssetMenuItem item = impl_->assetModel_->itemAt(index.row());
  QString filePath = item.path.toQString();
  if (filePath.isEmpty()) return;

  // Create context menu
  QMenu contextMenu;

  const QStringList selectedAssetPaths = impl_->selectedAssetPaths();
  const QStringList itemPaths = item.isSequence && !item.sequencePaths.isEmpty()
   ? item.sequencePaths
   : QStringList{filePath};
  const QStringList importTargets = selectedAssetPaths.isEmpty() ? itemPaths : selectedAssetPaths;

  // Add to Project action
  const QString addActionLabel = item.isSequence
   ? QStringLiteral("Add Sequence to Project")
   : (importTargets.size() > 1
      ? QStringLiteral("Add %1 Items to Project").arg(importTargets.size())
      : QStringLiteral("Add to Project"));
  QAction* addToProjectAction = contextMenu.addAction(addActionLabel);
  connect(addToProjectAction, &QAction::triggered, this, [this, importTargets, filePath]() {
   if (importTargets.isEmpty() && filePath.isEmpty()) return;
   auto* svc = ArtifactProjectService::instance();
   if (!svc) return;
   const QStringList imported = svc->importAssetsFromPaths(importTargets.isEmpty() ? QStringList{filePath} : importTargets);
   if (!imported.isEmpty()) {
    filesDropped(imported);
    impl_->applyFilters();
   }
  });

  contextMenu.addSeparator();

  if (item.isFolder) {
   QAction* openFolderAction = contextMenu.addAction("Open Folder");
   connect(openFolderAction, &QAction::triggered, this, [this, filePath]() {
    if (filePath.isEmpty()) return;
    impl_->currentDirectoryPath_ = filePath;
    impl_->clearThumbnailCache();
    impl_->applyFilters();
    impl_->syncDirectorySelection();
     this->folderChanged(filePath);
   });
   contextMenu.addSeparator();
  }

  // Open in File Explorer action
  QAction* openInExplorerAction = contextMenu.addAction("Open in File Explorer");
  connect(openInExplorerAction, &QAction::triggered, this, [filePath]() {
   QFileInfo fileInfo(filePath);
   QString folderPath = fileInfo.dir().absolutePath();
   QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
  });

  // Copy file path action
  QAction* copyPathAction = contextMenu.addAction("Copy File Path");
  connect(copyPathAction, &QAction::triggered, this, [filePath]() {
   QApplication::clipboard()->setText(filePath);
  });

  const auto removeTargets = selectedAssetPaths.isEmpty() ? itemPaths : selectedAssetPaths;
  const auto removableItems = impl_->footageItemsForPaths(removeTargets);
  if (!removableItems.empty()) {
   QAction* removeFromProjectAction = contextMenu.addAction("Remove from Project");
   connect(removeFromProjectAction, &QAction::triggered, this, [this, removeTargets]() {
    impl_->removeProjectItemsForPaths(removeTargets, this);
    impl_->refreshUnusedAssetCache();
    impl_->applyFilters();
   });
  }

  if (!item.isFolder && (impl_->isMissingAssetPath(filePath) || item.isSequence) && !impl_->footageItemsForPaths(itemPaths).empty()) {
   QAction* relinkAction = contextMenu.addAction("Relink Missing...");
   connect(relinkAction, &QAction::triggered, this, [this, itemPaths, filePath]() {
    const QString relinkTarget = itemPaths.isEmpty() ? filePath : itemPaths.first();
    if (impl_->relinkProjectItemsForPath(relinkTarget, this)) {
     impl_->refreshUnusedAssetCache();
     impl_->applyFilters();
    }
   });
  }

  contextMenu.addSeparator();

  if (item.isSequence && impl_->expandedSequenceKey_.isEmpty()) {
   QAction* showFramesAction = contextMenu.addAction("Show Sequence Frames");
   connect(showFramesAction, &QAction::triggered, this, [this, filePath]() {
    SequenceMatch sm;
    if (parseSequenceMatch(QFileInfo(filePath).fileName(), sm)) {
     impl_->expandedSequenceKey_ = sequenceGroupKey(QFileInfo(filePath), sm);
     impl_->applyFilters();
    }
   });
   contextMenu.addSeparator();
  } else if (!impl_->expandedSequenceKey_.isEmpty()) {
   QAction* backToFolderAction = contextMenu.addAction("Back to Folder");
   connect(backToFolderAction, &QAction::triggered, this, [this]() {
    impl_->expandedSequenceKey_.clear();
    impl_->applyFilters();
   });
   contextMenu.addSeparator();
  }

  // Show file properties action
  QAction* showPropertiesAction = contextMenu.addAction("Properties");
  connect(showPropertiesAction, &QAction::triggered, this, [this, filePath]() {
   QFileInfo fileInfo(filePath);
   QString info = QString("Name: %1\nSize: %2 bytes\nType: %3\nPath: %4")
     .arg(fileInfo.fileName())
     .arg(fileInfo.size())
     .arg(fileInfo.suffix())
     .arg(filePath);
   QMessageBox::information(this, QStringLiteral("Asset Properties"), info);
  });

  // Show menu at cursor position
  contextMenu.exec(impl_->fileView_->mapToGlobal(pos));
 }

}

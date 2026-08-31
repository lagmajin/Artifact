module;

#include <QApplication>
#include <QDrag>
#include <QColor>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QModelIndex>
#include <QMouseEvent>
#include <QMimeData>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <functional>
#include <utility>

export module Artifact.Widgets.AssetBrowserPresentation;

import AssetMenuModel;
import Widgets.Utils.CSS;

export namespace Artifact {

using namespace ArtifactCore;

namespace detail {
class RecentFolderButton final : public QToolButton {
 public:
  explicit RecentFolderButton(QWidget* parent = nullptr) : QToolButton(parent) {
    setAutoRaise(true);
    setCursor(Qt::PointingHandCursor);
   setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
   setIconSize(QSize(16, 16));
   setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setMinimumHeight(28);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QPalette pal = palette();
    const auto& theme = ArtifactCore::currentDCCTheme();
    pal.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor).darker(108));
    pal.setColor(QPalette::ButtonText, QColor(theme.textColor));
    setAutoFillBackground(true);
    setPalette(pal);
  }

  void setEntry(const QString& text, const QString& path, std::function<void(const QString&)> activate) {
    text_ = text;
    path_ = path;
    activate_ = std::move(activate);
    setText(text_.isEmpty() ? QStringLiteral("(unnamed)") : text_);
    setToolTip(path_.isEmpty() ? text_ : path_);
    setVisible(!path_.isEmpty());
  }

 protected:
  void mouseReleaseEvent(QMouseEvent* event) override {
    QToolButton::mouseReleaseEvent(event);
    if (event && event->button() == Qt::LeftButton && activate_ && !path_.isEmpty()) {
      activate_(path_);
    }
  }

 private:
  QString text_;
 QString path_;
  std::function<void(const QString&)> activate_;
};

class HoverPreviewPopup final : public QFrame {
 public:
  explicit HoverPreviewPopup(QWidget* parent = nullptr) : QFrame(parent) {
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Plain);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    title_ = new QLabel(this);
    title_->setWordWrap(true);
    preview_ = new QLabel(this);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(320, 180);
    preview_->setScaledContents(false);
    details_ = new QLabel(this);
    details_->setWordWrap(true);
    layout->addWidget(title_);
    layout->addWidget(preview_);
    layout->addWidget(details_);
  }

  void showFile(const QString& filePath, const QPoint& globalPos, const QIcon& icon, const QFileInfo& info) {
    const QSize targetSize(480, 270);
    QPixmap pixmap = icon.isNull() ? QPixmap() : icon.pixmap(targetSize);
    if (pixmap.isNull()) {
      pixmap = QPixmap(targetSize);
      pixmap.fill(Qt::transparent);
    }
    preview_->setPixmap(pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    title_->setText(info.fileName().isEmpty() ? filePath : info.fileName());
    const QString location = info.exists() ? info.absoluteFilePath() : QStringLiteral("Missing");
    const QString kind = info.isDir() ? QStringLiteral("Folder") : info.suffix().toUpper();
    const QString size = info.exists() && info.isFile()
                             ? QStringLiteral("%1 bytes").arg(info.size())
                             : QStringLiteral("-");
    const QString modified = info.exists()
                                 ? info.lastModified().toString(Qt::ISODate)
                                 : QStringLiteral("-");
    details_->setText(QStringLiteral("%1\n%2  •  Size: %3  •  Modified: %4")
                          .arg(location, kind, size, modified));
    adjustSize();
    QPoint popupPos = globalPos + QPoint(18, 18);
    if (QScreen* screen = QApplication::screenAt(globalPos)) {
      const QRect available = screen->availableGeometry();
      popupPos.setX(qBound(available.left(), popupPos.x(),
                           available.right() - width() + 1));
      popupPos.setY(qBound(available.top(), popupPos.y(),
                           available.bottom() - height() + 1));
    }
    move(popupPos);
    show();
    raise();
  }

 private:
  QLabel* title_ = nullptr;
  QLabel* preview_ = nullptr;
  QLabel* details_ = nullptr;
};
class AssetFileListView final : public QListView
 {
 public:
  explicit AssetFileListView(QWidget* parent = nullptr) : QListView(parent) {}

  void setEmptyStateMessage(const QString& message)
  {
   emptyStateMessage_ = message;
   viewport()->update();
  }

 protected:
  void startDrag(Qt::DropActions supportedActions) override
  {
   const QModelIndexList indexes = selectedIndexes();
   if (indexes.isEmpty() || !model()) {
    return;
   }

   QElapsedTimer dragTimer;
   dragTimer.start();
   auto* mimeData = model()->mimeData(indexes);
   if (!mimeData || mimeData->urls().isEmpty()) {
    delete mimeData;
    return;
   }

   auto* drag = new QDrag(this);
   drag->setMimeData(mimeData);

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

 protected:
  void paintEvent(QPaintEvent* event) override
  {
   QListView::paintEvent(event);
   if (!model() || model()->rowCount() != 0 || emptyStateMessage_.isEmpty()) {
    return;
   }
   QPainter painter(viewport());
   painter.setRenderHint(QPainter::TextAntialiasing, true);
   painter.setPen(palette().color(QPalette::PlaceholderText));
   painter.drawText(viewport()->rect().adjusted(24, 24, -24, -24),
                    Qt::AlignCenter | Qt::TextWordWrap,
                    emptyStateMessage_);
  }

 private:
  QString emptyStateMessage_;
};
class AssetCardDelegate final : public QStyledItemDelegate
{
public:
 explicit AssetCardDelegate(QListView* view)
     : QStyledItemDelegate(view), view_(view) {}

 QSize sizeHint(const QStyleOptionViewItem& option,
                const QModelIndex& index) const override
 {
  if (view_ && view_->viewMode() == QListView::ListMode) {
   return QStyledItemDelegate::sizeHint(option, index);
  }
  const int thumbnail = std::max(64, option.decorationSize.width());
  return QSize(thumbnail + 24, thumbnail + 54);
 }

 void paint(QPainter* painter, const QStyleOptionViewItem& option,
            const QModelIndex& index) const override
 {
  if (!painter || !index.isValid()) {
   return;
  }
  if (view_ && view_->viewMode() == QListView::ListMode) {
   QStyledItemDelegate::paint(painter, option, index);
   return;
  }

  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);
  const bool selected = opt.state.testFlag(QStyle::State_Selected);
  const bool hovered = opt.state.testFlag(QStyle::State_MouseOver);
  const bool focused = opt.state.testFlag(QStyle::State_HasFocus);
  const QRect cardRect = opt.rect.adjusted(3, 3, -3, -3);
  const int informationHeight = 43;
  const QRect thumbnailRect = cardRect.adjusted(1, 1, -1,
                                                 -informationHeight);
  const QRect informationRect(cardRect.left() + 1,
                              thumbnailRect.bottom() + 1,
                              cardRect.width() - 2,
                              informationHeight - 1);

  const QColor base = opt.palette.color(QPalette::Base);
  const QColor text = opt.palette.color(QPalette::Text);
  const QColor accent = opt.palette.color(QPalette::Highlight);
  const QColor cardFill = hovered ? base.lighter(128) : base.lighter(112);
  QColor border = hovered ? text.darker(190) : base.lighter(150);
  if (selected) {
   border = accent.lighter(focused ? 135 : 118);
  }

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(QPen(border, selected ? 2.0 : 1.0));
  painter->setBrush(cardFill);
  painter->drawRoundedRect(cardRect, 6, 6);

  QPainterPath thumbnailClip;
  thumbnailClip.addRoundedRect(thumbnailRect, 5, 5);
  painter->save();
  painter->setClipPath(thumbnailClip);
  painter->fillRect(thumbnailRect, base.darker(118));
  const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
  if (!icon.isNull()) {
   const QSize targetSize = thumbnailRect.size() - QSize(10, 10);
   const QPixmap pixmap = icon.pixmap(targetSize);
   if (!pixmap.isNull()) {
    const QSize fitted = pixmap.size().scaled(targetSize, Qt::KeepAspectRatio);
    const QRect pixmapRect(QPoint(0, 0), fitted);
    QRect centered = pixmapRect;
    centered.moveCenter(thumbnailRect.center());
    painter->drawPixmap(centered, pixmap);
   }
  }
  painter->restore();

  const QString title = index.data(static_cast<int>(AssetMenuRole::Name)).toString();
  const QString rawType = index.data(static_cast<int>(AssetMenuRole::Type)).toString();
  const bool isFolder = index.data(static_cast<int>(AssetMenuRole::IsFolder)).toBool();
  const bool favorite = rawType.contains(QStringLiteral("Favorite"), Qt::CaseInsensitive);
  const bool imported = rawType.contains(QStringLiteral("Imported"), Qt::CaseInsensitive);
  const bool missing = rawType.contains(QStringLiteral("Missing"), Qt::CaseInsensitive);
  const bool unused = rawType.contains(QStringLiteral("Unused"), Qt::CaseInsensitive);

  QStringList metadataParts;
  for (const QString& part : rawType.split(QStringLiteral(" • "), Qt::SkipEmptyParts)) {
   if (part.compare(QStringLiteral("Favorite"), Qt::CaseInsensitive) == 0 ||
       part.compare(QStringLiteral("Imported"), Qt::CaseInsensitive) == 0 ||
       part.compare(QStringLiteral("Missing"), Qt::CaseInsensitive) == 0 ||
       part.compare(QStringLiteral("Unused"), Qt::CaseInsensitive) == 0 ||
       part.startsWith(QStringLiteral("Source Uses:"), Qt::CaseInsensitive)) {
    continue;
   }
   metadataParts.push_back(part);
  }
  const QString metadata = isFolder
      ? QStringLiteral("Folder")
      : metadataParts.value(0, QStringLiteral("Asset"));

  const QRect titleRect = informationRect.adjusted(7, 3, -24, -19);
  const QRect metadataRect = informationRect.adjusted(7, 21, -7, -3);
  QFont titleFont = opt.font;
  titleFont.setWeight(QFont::DemiBold);
  painter->setFont(titleFont);
  painter->setPen(selected ? opt.palette.color(QPalette::HighlightedText) : text);
  painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                    painter->fontMetrics().elidedText(
                        title, Qt::ElideRight, titleRect.width()));

  QFont metadataFont = opt.font;
  metadataFont.setPointSizeF(std::max(8.0, metadataFont.pointSizeF() - 1.0));
  metadataFont.setWeight(QFont::Normal);
  painter->setFont(metadataFont);
  painter->setPen(text.darker(145));
  painter->drawText(metadataRect, Qt::AlignLeft | Qt::AlignVCenter,
                    painter->fontMetrics().elidedText(
                        metadata, Qt::ElideRight, metadataRect.width()));

  if (favorite) {
   painter->setPen(QColor(245, 195, 66));
   painter->drawText(QRect(informationRect.right() - 23,
                           informationRect.top() + 2, 18, 18),
                     Qt::AlignCenter, QStringLiteral("★"));
  }
  if (imported || missing || unused) {
   const QColor statusColor = missing ? QColor(224, 82, 82)
       : unused ? QColor(224, 168, 68)
                : QColor(78, 190, 112);
   painter->setPen(Qt::NoPen);
   painter->setBrush(statusColor);
   painter->drawEllipse(QPoint(informationRect.right() - 10,
                               informationRect.bottom() - 9), 3, 3);
  }
  painter->restore();
 }

private:
 QListView* view_ = nullptr;
};
} // namespace detail
} // namespace Artifact

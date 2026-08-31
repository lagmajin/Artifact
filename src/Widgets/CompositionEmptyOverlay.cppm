module;

#include <QFileInfo>
#include <QColor>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QMimeData>
#include <QPalette>
#include <QPushButton>
#include <QSizePolicy>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

module Artifact.Widgets.CompositionEmptyOverlay;

namespace Artifact {

EmptyCompositionOverlayWidget::EmptyCompositionOverlayWidget(
    QWidget *parent, std::function<void()> createRequested,
    std::function<void(const QStringList &)> filesDropped)
    : QWidget(parent), createRequested_(std::move(createRequested)),
      filesDropped_(std::move(filesDropped)) {
  setAutoFillBackground(false);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_TranslucentBackground);
  setFocusPolicy(Qt::NoFocus);
  setAcceptDrops(true);

  rootLayout_ = new QVBoxLayout(this);
  rootLayout_->setContentsMargins(24, 24, 24, 24);
  rootLayout_->setSpacing(0);

  card_ = new QFrame(this);
  card_->setObjectName(QStringLiteral("compositionCardFrame"));
  card_->setFrameShape(QFrame::StyledPanel);
  card_->setFrameShadow(QFrame::Plain);
  card_->setAutoFillBackground(true);
  card_->setMinimumWidth(0);
  card_->setMaximumWidth(640);
  card_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

  QPalette cardPalette = card_->palette();
  cardPalette.setColor(QPalette::Window, QColor(18, 20, 25, 230));
  cardPalette.setColor(QPalette::WindowText, QColor(248, 248, 248));
  cardPalette.setColor(QPalette::Base, QColor(18, 20, 25, 230));
  cardPalette.setColor(QPalette::Text, QColor(248, 248, 248));
  card_->setPalette(cardPalette);

  cardLayout_ = new QVBoxLayout(card_);
  cardLayout_->setContentsMargins(32, 28, 32, 28);
  cardLayout_->setSpacing(14);

  titleLabel_ = new QLabel(QStringLiteral("まだコンポジションがありません"), card_);
  QFont titleFont = titleLabel_->font();
  titleFont.setPointSizeF(std::max(14.0, titleFont.pointSizeF() + 2.0));
  titleFont.setBold(true);
  titleFont.setStyleStrategy(QFont::PreferAntialias);
  titleLabel_->setFont(titleFont);
  titleLabel_->setAlignment(Qt::AlignCenter);
  titleLabel_->setMinimumWidth(0);
  titleLabel_->setWordWrap(true);

  bodyLabel_ = new QLabel(
      QStringLiteral("新規コンポジションを作成して、編集を始めましょう。"),
      card_);
  bodyLabel_->setAlignment(Qt::AlignCenter);
  QFont bodyFont = bodyLabel_->font();
  bodyFont.setStyleStrategy(QFont::PreferAntialias);
  bodyLabel_->setFont(bodyFont);
  bodyLabel_->setMinimumWidth(0);
  bodyLabel_->setWordWrap(true);

  helperLabel_ = new QLabel(
      QStringLiteral("ボタンを押すと、コンポジション設定ダイアログを開きます。"),
      card_);
  helperLabel_->setAlignment(Qt::AlignCenter);
  QFont helperFont = helperLabel_->font();
  helperFont.setStyleStrategy(QFont::PreferAntialias);
  helperLabel_->setFont(helperFont);
  helperLabel_->setMinimumWidth(0);
  helperLabel_->setWordWrap(true);

  createButton_ = new QPushButton(QStringLiteral("新規コンポジション"), card_);
  createButton_->setMinimumHeight(46);
  createButton_->setMinimumWidth(0);
  createButton_->setMaximumWidth(240);
  QFont buttonFont = createButton_->font();
  buttonFont.setPointSizeF(std::max(12.0, buttonFont.pointSizeF() + 1.0));
  buttonFont.setBold(true);
  buttonFont.setStyleStrategy(QFont::PreferAntialias);
  createButton_->setFont(buttonFont);
  createButton_->setCursor(Qt::PointingHandCursor);
  createButton_->setDefault(true);

  cardLayout_->addWidget(titleLabel_);
  cardLayout_->addWidget(bodyLabel_);
  cardLayout_->addWidget(helperLabel_);
  cardLayout_->addSpacing(8);
  cardLayout_->addWidget(createButton_, 0, Qt::AlignHCenter);

  updateResponsiveLayout();

  QObject::connect(createButton_, &QPushButton::clicked, this, [this]() {
    if (createRequested_) {
      createRequested_();
    }
  });
}

void EmptyCompositionOverlayWidget::setCompositionAvailable(bool hasComposition) {
  if (!titleLabel_ || !bodyLabel_ || !helperLabel_ || !createButton_) {
    return;
  }
  if (compositionStateInitialized_ && hasComposition_ == hasComposition) {
    return;
  }
  compositionStateInitialized_ = true;
  if (hasComposition) {
    hasComposition_ = true;
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    titleLabel_->setText(QStringLiteral("レイヤーがありません"));
    bodyLabel_->setText(QStringLiteral(
        "平面やテキストなどのレイヤーを追加すると、ここに表示されます。"));
    helperLabel_->setText(QStringLiteral(
        "Layer メニューからレイヤーを追加してください。"));
    createButton_->hide();
    updateResponsiveLayout();
    update();
    return;
  }
  hasComposition_ = false;
  setAttribute(Qt::WA_TransparentForMouseEvents, false);
  titleLabel_->setText(QStringLiteral("まだコンポジションがありません"));
  bodyLabel_->setText(QStringLiteral(
      "新規コンポジションを作成して、編集を始めましょう。"));
  helperLabel_->setText(QStringLiteral(
      "ボタンを押すと、コンポジション設定ダイアログを開きます。"));
  createButton_->show();
  updateResponsiveLayout();
  update();
}

QSize EmptyCompositionOverlayWidget::preferredOverlaySize(
    const QSize &available) const {
  const int preferredWidth = hasComposition_ ? 480 : 600;
  const int preferredHeight = hasComposition_ ? 190 : 250;
  return QSize(std::max(1, std::min(preferredWidth, available.width())),
               std::max(1, std::min(preferredHeight, available.height())));
}

void EmptyCompositionOverlayWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateResponsiveLayout();
}

void EmptyCompositionOverlayWidget::dragEnterEvent(QDragEnterEvent *event) {
  if (hasComposition_ && event->mimeData() && event->mimeData()->hasUrls()) {
    for (const auto &url : event->mimeData()->urls()) {
      const QFileInfo info(url.toLocalFile());
      if (url.isLocalFile() && info.exists() && !info.isDir()) {
        event->acceptProposedAction();
        return;
      }
    }
  }
  event->ignore();
}

void EmptyCompositionOverlayWidget::dragMoveEvent(QDragMoveEvent *event) {
  if (hasComposition_ && event->mimeData() && event->mimeData()->hasUrls()) {
    for (const auto &url : event->mimeData()->urls()) {
      const QFileInfo info(url.toLocalFile());
      if (url.isLocalFile() && info.exists() && !info.isDir()) {
        event->acceptProposedAction();
        return;
      }
    }
  }
  event->ignore();
}

void EmptyCompositionOverlayWidget::dropEvent(QDropEvent *event) {
  QStringList paths;
  if (hasComposition_ && event->mimeData()) {
    for (const auto &url : event->mimeData()->urls()) {
      if (!url.isLocalFile()) {
        continue;
      }
      const QFileInfo info(url.toLocalFile());
      if (info.exists() && !info.isDir()) {
        paths.push_back(info.absoluteFilePath());
      }
    }
  }
  paths.removeDuplicates();
  if (paths.isEmpty() || !filesDropped_) {
    event->ignore();
    return;
  }
  filesDropped_(paths);
  event->acceptProposedAction();
}

void EmptyCompositionOverlayWidget::updateResponsiveLayout() {
  if (!rootLayout_ || !card_ || !cardLayout_ || !createButton_) {
    return;
  }
  const bool compactWidth = width() < 420;
  const bool compactHeight = height() < 300;
  const bool veryCompactHeight = height() < 210;
  const int outerMargin = compactWidth || compactHeight ? 10 : 24;
  const int innerHorizontalMargin = compactWidth ? 14 : 32;
  const int innerVerticalMargin = compactHeight ? 12 : 28;
  rootLayout_->setContentsMargins(0, 0, 0, 0);
  cardLayout_->setContentsMargins(innerHorizontalMargin, innerVerticalMargin,
                                  innerHorizontalMargin, innerVerticalMargin);
  cardLayout_->setSpacing(compactHeight ? 7 : 14);
  bodyLabel_->setVisible(!veryCompactHeight);
  helperLabel_->setVisible(!compactHeight);
  createButton_->setMinimumHeight(compactHeight ? 34 : 46);
  const int preferredCardWidth = hasComposition_ ? 420 : 640;
  const int cardWidth = std::max(
      0, std::min(preferredCardWidth, width() - outerMargin * 2));
  card_->setFixedWidth(cardWidth);
  const int buttonWidth = std::max(
      0, std::min(240, cardWidth - innerHorizontalMargin * 2));
  createButton_->setMaximumWidth(buttonWidth);
  cardLayout_->invalidate();
  cardLayout_->activate();
  card_->adjustSize();
  const int availableHeight = std::max(0, height() - outerMargin * 2);
  const int cardHeight = std::min(card_->sizeHint().height(), availableHeight);
  card_->setGeometry((width() - cardWidth) / 2, (height() - cardHeight) / 2,
                     cardWidth, cardHeight);
}

}

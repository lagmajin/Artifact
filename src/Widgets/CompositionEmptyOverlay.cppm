module;

#include <QFileInfo>
#include <QColor>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMimeData>
#include <QPalette>
#include <QPushButton>
#include <QSize>
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
  card_->setFrameShape(QFrame::NoFrame);
  card_->setFrameShadow(QFrame::Plain);
  card_->setAutoFillBackground(false);
  card_->setMinimumWidth(0);
  card_->setMaximumWidth(540);
  card_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

  QPalette cardPalette = card_->palette();
  cardPalette.setColor(QPalette::WindowText, QColor(225, 232, 240));
  cardPalette.setColor(QPalette::Text, QColor(225, 232, 240));
  card_->setPalette(cardPalette);

  cardLayout_ = new QVBoxLayout(card_);
  cardLayout_->setContentsMargins(28, 16, 28, 16);
  cardLayout_->setSpacing(10);

  layerEmptyRow_ = new QWidget(card_);
  layerEmptyRow_->setAutoFillBackground(false);
  auto *layerEmptyLayout = new QHBoxLayout(layerEmptyRow_);
  layerEmptyLayout->setContentsMargins(0, 0, 0, 0);
  layerEmptyLayout->setSpacing(12);

  layerIconLabel_ = new QLabel(layerEmptyRow_);
  layerIconLabel_->setFixedSize(24, 24);
  layerIconLabel_->setPixmap(
      QIcon(QStringLiteral(":/icons/Studio/composition_empty_layers.svg"))
          .pixmap(QSize(20, 20)));
  layerIconLabel_->setAlignment(Qt::AlignCenter);

  layerEmptyLabel_ = new QLabel(QStringLiteral("レイヤーがありません"),
                                layerEmptyRow_);
  QFont layerEmptyFont = layerEmptyLabel_->font();
  layerEmptyFont.setPointSizeF(std::max(10.5, layerEmptyFont.pointSizeF()));
  layerEmptyFont.setWeight(QFont::Medium);
  layerEmptyFont.setStyleStrategy(QFont::PreferAntialias);
  layerEmptyLabel_->setFont(layerEmptyFont);
  layerEmptyLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  layerEmptyLayout->addWidget(layerIconLabel_);
  layerEmptyLayout->addWidget(layerEmptyLabel_);
  layerEmptyRow_->hide();

  compositionIconLabel_ = new QLabel(card_);
  compositionIconLabel_->setFixedSize(48, 48);
  compositionIconLabel_->setPixmap(
      QIcon(QStringLiteral(":/icons/Studio/composition_empty_composition.svg"))
          .pixmap(QSize(44, 44)));
  compositionIconLabel_->setAlignment(Qt::AlignCenter);

  titleLabel_ = new QLabel(QStringLiteral("コンポジションがありません"), card_);
  QFont titleFont = titleLabel_->font();
  titleFont.setPointSizeF(std::max(14.0, titleFont.pointSizeF() + 1.0));
  titleFont.setWeight(QFont::Medium);
  titleFont.setStyleStrategy(QFont::PreferAntialias);
  titleLabel_->setFont(titleFont);
  titleLabel_->setAlignment(Qt::AlignCenter);
  titleLabel_->setMinimumWidth(0);
  titleLabel_->setWordWrap(true);

  bodyLabel_ = new QLabel(
      QStringLiteral("サイズとフレームレートを設定して始めます"),
      card_);
  bodyLabel_->setAlignment(Qt::AlignCenter);
  QFont bodyFont = bodyLabel_->font();
  bodyFont.setPointSizeF(std::max(10.0, bodyFont.pointSizeF()));
  bodyFont.setStyleStrategy(QFont::PreferAntialias);
  bodyLabel_->setFont(bodyFont);
  {
    QPalette bodyPalette = bodyLabel_->palette();
    bodyPalette.setColor(QPalette::WindowText, QColor(174, 182, 192));
    bodyLabel_->setPalette(bodyPalette);
  }
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
  createButton_->setMinimumHeight(36);
  createButton_->setMaximumHeight(36);
  createButton_->setMinimumWidth(168);
  createButton_->setMaximumWidth(196);
  QFont buttonFont = createButton_->font();
  buttonFont.setPointSizeF(std::max(10.5, buttonFont.pointSizeF()));
  buttonFont.setBold(true);
  buttonFont.setStyleStrategy(QFont::PreferAntialias);
  createButton_->setFont(buttonFont);
  createButton_->setCursor(Qt::PointingHandCursor);
  createButton_->setDefault(true);
  {
    QPalette buttonPalette = createButton_->palette();
    buttonPalette.setColor(QPalette::Button, QColor(32, 93, 190));
    buttonPalette.setColor(QPalette::ButtonText, Qt::white);
    buttonPalette.setColor(QPalette::Highlight, QColor(54, 124, 232));
    buttonPalette.setColor(QPalette::HighlightedText, Qt::white);
    createButton_->setPalette(buttonPalette);
  }

  cardLayout_->addWidget(layerEmptyRow_, 0, Qt::AlignHCenter);
  cardLayout_->addWidget(compositionIconLabel_, 0, Qt::AlignHCenter);
  cardLayout_->addWidget(titleLabel_);
  cardLayout_->addWidget(bodyLabel_);
  helperLabel_->hide();
  cardLayout_->addSpacing(2);
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
    layerEmptyRow_->show();
    compositionIconLabel_->hide();
    titleLabel_->hide();
    bodyLabel_->hide();
    helperLabel_->hide();
    createButton_->hide();
    updateResponsiveLayout();
    update();
    return;
  }
  hasComposition_ = false;
  setAttribute(Qt::WA_TransparentForMouseEvents, false);
  layerEmptyRow_->hide();
  compositionIconLabel_->show();
  titleLabel_->show();
  titleLabel_->setText(QStringLiteral("コンポジションがありません"));
  bodyLabel_->setText(QStringLiteral(
      "サイズとフレームレートを設定して始めます"));
  bodyLabel_->show();
  helperLabel_->hide();
  createButton_->show();
  updateResponsiveLayout();
  update();
}

QSize EmptyCompositionOverlayWidget::preferredOverlaySize(
    const QSize &available) const {
  const int preferredWidth = hasComposition_ ? 340 : 540;
  const int preferredHeight = hasComposition_ ? 84 : 190;
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
  const int innerHorizontalMargin = hasComposition_ ? 16 : (compactWidth ? 14 : 28);
  const int innerVerticalMargin = hasComposition_ ? 12 : (compactHeight ? 10 : 16);
  rootLayout_->setContentsMargins(0, 0, 0, 0);
  cardLayout_->setContentsMargins(innerHorizontalMargin, innerVerticalMargin,
                                  innerHorizontalMargin, innerVerticalMargin);
  cardLayout_->setSpacing(compactHeight ? 7 : 10);
  bodyLabel_->setVisible(!hasComposition_ && !veryCompactHeight);
  helperLabel_->hide();
  compositionIconLabel_->setVisible(!hasComposition_ && !veryCompactHeight);
  createButton_->setMinimumHeight(compactHeight ? 34 : 36);
  createButton_->setMaximumHeight(compactHeight ? 34 : 36);
  const int preferredCardWidth = hasComposition_ ? 320 : 540;
  const int cardWidth = std::max(
      0, std::min(preferredCardWidth, width() - outerMargin * 2));
  card_->setFixedWidth(cardWidth);
  const int buttonWidth = std::max(
      0, std::min(196, cardWidth - innerHorizontalMargin * 2));
  createButton_->setMinimumWidth(std::min(168, buttonWidth));
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

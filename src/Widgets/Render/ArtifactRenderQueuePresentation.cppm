module;

#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDropEvent>
#include <QFocusEvent>
#include <QFont>
#include <QFrame>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPixmap>
#include <QProgressBar>
#include <QProxyStyle>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QString>
#include <QStyleOptionButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <algorithm>
#include <functional>

export module Artifact.Widgets.RenderQueuePresentation;

import Utils.Path;
import Widgets.Utils.CSS;

export namespace Artifact {

namespace detail {

inline QIcon loadIconWithFallback(const QString& fileName)
{
  const QString resourcePath = ArtifactCore::resolveIconResourcePath(fileName);
  QIcon icon(resourcePath);
  if (!icon.isNull()) {
    return icon;
  }
  return QIcon(ArtifactCore::resolveIconPath(fileName));
}

class RenderQueueSearchEdit final : public QLineEdit
{
 public:
  std::function<void(const QString&)> changed;
  using QLineEdit::QLineEdit;

 protected:
  void keyReleaseEvent(QKeyEvent* event) override
  {
    QLineEdit::keyReleaseEvent(event);
    if (event->key() == Qt::Key_Escape && !text().isEmpty()) {
      clear();
    }
    if (changed) changed(text());
  }
};

class RenderQueueActionButton final : public QPushButton
{
 public:
  std::function<void()> action;
  using QPushButton::QPushButton;

 protected:
  void mouseReleaseEvent(QMouseEvent* event) override
  {
    QPushButton::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton && action) action();
  }
};

class RenderQueueListWidget final : public QListWidget
{
 public:
  std::function<void(int, int)> reordered;
  using QListWidget::QListWidget;

 protected:
  void dropEvent(QDropEvent* event) override
  {
    const int sourceRow = currentRow();
    const int sourceId = sourceRow >= 0 && sourceRow < count()
        ? item(sourceRow)->data(Qt::UserRole).toInt()
        : -1;
    QListWidget::dropEvent(event);
    int resolvedTarget = -1;
    if (sourceId >= 0) {
      for (int row = 0; row < count(); ++row) {
        if (item(row)->data(Qt::UserRole).toInt() == sourceId) {
          resolvedTarget = row;
          break;
        }
      }
    }
    if (reordered && sourceRow >= 0 && resolvedTarget >= 0 && sourceRow != resolvedTarget) {
      reordered(sourceRow, resolvedTarget);
    }
  }
};

class RenderQueuePathEdit final : public QLineEdit
{
 public:
  std::function<void(const QString&)> committed;
  using QLineEdit::QLineEdit;

 protected:
  void keyReleaseEvent(QKeyEvent* event) override
  {
    QLineEdit::keyReleaseEvent(event);
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
      if (committed) committed(text());
    }
  }

  void focusOutEvent(QFocusEvent* event) override
  {
    QLineEdit::focusOutEvent(event);
  }
};

class RenderQueueIntSpinBox final : public QSpinBox
{
 public:
  std::function<void(int)> committed;
  using QSpinBox::QSpinBox;

 protected:
  void keyReleaseEvent(QKeyEvent* event) override
  {
    QSpinBox::keyReleaseEvent(event);
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
      if (committed) committed(value());
    }
  }

  void focusOutEvent(QFocusEvent* event) override
  {
    QSpinBox::focusOutEvent(event);
  }
};

class RenderQueueDoubleSpinBox final : public QDoubleSpinBox
{
 public:
  std::function<void(double)> committed;
  using QDoubleSpinBox::QDoubleSpinBox;

 protected:
  void keyReleaseEvent(QKeyEvent* event) override
  {
    QDoubleSpinBox::keyReleaseEvent(event);
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
      if (committed) committed(value());
    }
  }

  void focusOutEvent(QFocusEvent* event) override
  {
    QDoubleSpinBox::focusOutEvent(event);
  }
};

class RenderQueueJobCard final : public QFrame
{
 public:
  std::function<void()> selected;
  QLabel* statusLabel = nullptr;
  QLabel* statusIconLabel = nullptr;
  QLabel* thumbnailLabel = nullptr;
  QLabel* nameLabel = nullptr;
  QLabel* outputLabel = nullptr;
  QLabel* backendLabel = nullptr;
  QProgressBar* progressBar = nullptr;

  explicit RenderQueueJobCard(QWidget* parent = nullptr)
      : QFrame(parent)
  {
    // Queue rows use the list's one-pixel rhythm rather than an outline
    // around every job, matching the compact Render Manager reference.
    setFrameShape(QFrame::NoFrame);
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(10, 4, 12, 4);
    root->setSpacing(14);

    thumbnailLabel = new QLabel(QStringLiteral("PREVIEW"));
    thumbnailLabel->setFixedSize(102, 58);
    thumbnailLabel->setAlignment(Qt::AlignCenter);
    thumbnailLabel->setScaledContents(false);
    thumbnailLabel->setAutoFillBackground(true);
    QPalette thumbnailPalette = thumbnailLabel->palette();
    thumbnailPalette.setColor(QPalette::Window, QColor(18, 24, 29));
    thumbnailPalette.setColor(QPalette::WindowText, QColor(130, 145, 155));
    thumbnailLabel->setPalette(thumbnailPalette);
    root->addWidget(thumbnailLabel);

    auto* nameColumn = new QVBoxLayout();
    nameColumn->setContentsMargins(0, 0, 0, 0);
    nameColumn->setSpacing(1);
    nameLabel = new QLabel();
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(nameFont.pointSize() + 1);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setMinimumWidth(136);
    nameLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    outputLabel = new QLabel();
    backendLabel = new QLabel();
    outputLabel->setWordWrap(false);
    backendLabel->setWordWrap(false);
    outputLabel->setMinimumWidth(172);
    outputLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    nameColumn->addStretch();
    nameColumn->addWidget(nameLabel);
    nameColumn->addStretch();
    root->addLayout(nameColumn);
    root->addWidget(outputLabel);

    auto* statusColumn = new QVBoxLayout();
    statusColumn->setContentsMargins(0, 0, 0, 0);
    statusColumn->setSpacing(3);
    auto* statusHeader = new QHBoxLayout();
    statusHeader->setContentsMargins(0, 0, 0, 0);
    statusIconLabel = new QLabel();
    statusIconLabel->setFixedSize(18, 18);
    statusIconLabel->setAlignment(Qt::AlignCenter);
    statusLabel = new QLabel("WAIT");
    statusLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    statusHeader->addWidget(statusIconLabel);
    statusHeader->addWidget(statusLabel);
    statusHeader->addStretch();
    statusColumn->addLayout(statusHeader);
    statusColumn->addWidget(backendLabel);
    progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setTextVisible(true);
    progressBar->setMinimumWidth(142);
    progressBar->setMaximumHeight(6);
    statusColumn->addWidget(progressBar);
    root->addLayout(statusColumn, 1);

    // The card owns its presentation, but a click is still a list selection.
    // Keep child controls out of the mouse route so it stays consistent across
    // the preview, text, and status portions of the row.
    thumbnailLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    outputLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    backendLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    statusIconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    statusLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    progressBar->setAttribute(Qt::WA_TransparentForMouseEvents);
  }

  void setJob(const QString& status, const QString& name, const QString& output,
              const QString& backend, const QString& errorMessage,
              int progress, const QColor& accent)
  {
    const bool needsAttention = !errorMessage.trimmed().isEmpty();
    const QString visibleStatus = needsAttention
        ? QStringLiteral("Needs attention")
        : status;
    statusLabel->setText(visibleStatus);
    QString statusIcon = QStringLiteral("Studio/render_status_ready.svg");
    if (needsAttention) {
      statusIcon = QStringLiteral("Studio/render_status_attention.svg");
    }
    if (!needsAttention &&
        status.compare(QStringLiteral("Rendering"), Qt::CaseInsensitive) == 0) {
      statusIcon = QStringLiteral("Studio/render_status_rendering.svg");
    } else if (!needsAttention &&
               status.compare(QStringLiteral("Completed"), Qt::CaseInsensitive) == 0) {
      statusIcon = QStringLiteral("Studio/render_status_completed.svg");
    }
    statusIconLabel->setPixmap(
        loadIconWithFallback(statusIcon).pixmap(QSize(16, 16)));
    if (needsAttention) {
      // Keep the queue scanable: failed jobs use the same compact missing
      // preview cue as the reference, while the actual failure remains in the
      // Status column and inspector rather than repeating a long label here.
      thumbnailLabel->setText({});
      thumbnailLabel->setPixmap(
          loadIconWithFallback(QStringLiteral("Studio/asset_missing_small.svg"))
              .pixmap(QSize(28, 28)));
    } else if (thumbnailLabel->pixmap().isNull()) {
      thumbnailLabel->setText(QStringLiteral("PREVIEW"));
    }
    nameLabel->setText(name);
    outputLabel->setText(output);
    QPalette outputPalette = outputLabel->palette();
    outputPalette.setColor(QPalette::WindowText,
        !needsAttention
            ? QColor(155, 165, 175)
            : QColor(224, 174, 78));
    outputLabel->setPalette(outputPalette);
    backendLabel->setText(needsAttention
        ? QStringLiteral("Composition missing")
        : backend);
    backendLabel->setVisible(true);
    progressBar->setValue(std::clamp(progress, 0, 100));
    progressBar->setVisible(
        status.compare(QStringLiteral("Rendering"), Qt::CaseInsensitive) == 0);
    QPalette palette = statusLabel->palette();
    palette.setColor(QPalette::WindowText,
                     needsAttention ? QColor(224, 174, 78) : accent);
    statusLabel->setPalette(palette);
    QPalette barPalette = progressBar->palette();
    barPalette.setColor(QPalette::Highlight, accent);
    progressBar->setPalette(barPalette);
  }

  void setPreview(const QPixmap& pixmap)
  {
    if (!thumbnailLabel || pixmap.isNull()) return;
    thumbnailLabel->setPixmap(pixmap.scaled(
        thumbnailLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    thumbnailLabel->setToolTip(QStringLiteral("Latest rendered frame"));
  }

 protected:
  void mousePressEvent(QMouseEvent* event) override
  {
    if (event && event->button() == Qt::LeftButton && selected) {
      selected();
    }
    QFrame::mousePressEvent(event);
  }
};

class RenderQueuePrimaryButtonStyle final : public QProxyStyle
{
 public:
  using QProxyStyle::QProxyStyle;

  void drawControl(ControlElement element, const QStyleOption* option,
                   QPainter* painter,
                   const QWidget* widget = nullptr) const override
  {
    if (element != CE_PushButton) {
      QProxyStyle::drawControl(element, option, painter, widget);
      return;
    }

    const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option);
    if (!button) {
      QProxyStyle::drawControl(element, option, painter, widget);
      return;
    }

    const bool enabled = button->state.testFlag(State_Enabled);
    const bool hovered = enabled && button->state.testFlag(State_MouseOver);
    const bool pressed = enabled && button->state.testFlag(State_Sunken);
    QColor fill = enabled ? QColor(226, 166, 47) : QColor(69, 64, 55);
    QColor border = enabled ? QColor(244, 187, 68) : QColor(83, 78, 68);
    QColor text = enabled ? QColor(25, 22, 17) : QColor(142, 135, 121);
    if (pressed) {
      fill = fill.darker(112);
    } else if (hovered) {
      fill = fill.lighter(108);
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF surface = QRectF(button->rect).adjusted(0.5, 0.5, -0.5, -0.5);
    painter->setPen(QPen(border, 1.0));
    painter->setBrush(fill);
    painter->drawRoundedRect(surface, 3.0, 3.0);
    painter->restore();

    QStyleOptionButton labelOption(*button);
    labelOption.palette.setColor(QPalette::ButtonText, text);
    QProxyStyle::drawControl(CE_PushButtonLabel, &labelOption, painter, widget);
  }
};

} // namespace detail
} // namespace Artifact

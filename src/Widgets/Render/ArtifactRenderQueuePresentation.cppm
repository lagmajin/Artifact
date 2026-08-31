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
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QString>
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
    setFrameShape(QFrame::StyledPanel);
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(12, 10, 14, 10);
    root->setSpacing(14);

    statusLabel = new QLabel("WAIT");
    statusLabel->setMinimumWidth(86);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusIconLabel = new QLabel();
    statusIconLabel->setFixedSize(18, 18);
    statusIconLabel->setAlignment(Qt::AlignCenter);

    thumbnailLabel = new QLabel(QStringLiteral("PREVIEW"));
    thumbnailLabel->setFixedSize(280, 172);
    thumbnailLabel->setAlignment(Qt::AlignCenter);
    thumbnailLabel->setScaledContents(false);
    thumbnailLabel->setAutoFillBackground(true);
    QPalette thumbnailPalette = thumbnailLabel->palette();
    thumbnailPalette.setColor(QPalette::Window, QColor(18, 24, 29));
    thumbnailPalette.setColor(QPalette::WindowText, QColor(130, 145, 155));
    thumbnailLabel->setPalette(thumbnailPalette);
    root->addWidget(thumbnailLabel);

    auto* body = new QVBoxLayout();
    body->setSpacing(7);
    nameLabel = new QLabel();
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(nameFont.pointSize() + 2);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    outputLabel = new QLabel();
    backendLabel = new QLabel();
    outputLabel->setWordWrap(true);
    backendLabel->setWordWrap(true);
    auto* cardHeader = new QHBoxLayout();
    cardHeader->setContentsMargins(0, 0, 0, 0);
    cardHeader->addWidget(nameLabel, 1);
    cardHeader->addWidget(statusIconLabel);
    cardHeader->addWidget(statusLabel);
    body->addLayout(cardHeader);
    body->addWidget(outputLabel);
    body->addWidget(backendLabel);
    body->addStretch();
    progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setTextVisible(true);
    progressBar->setMinimumWidth(190);
    body->addWidget(progressBar);
    root->addLayout(body, 1);
  }

  void setJob(const QString& status, const QString& name, const QString& output,
              const QString& backend, const QString& errorMessage,
              int progress, const QColor& accent)
  {
    statusLabel->setText(status.toUpper());
    QString statusIcon = QStringLiteral("Studio/animationmenu_schedule.svg");
    if (status.compare(QStringLiteral("Rendering"), Qt::CaseInsensitive) == 0) {
      statusIcon = QStringLiteral("Studio/figma_media_play.svg");
    } else if (status.compare(QStringLiteral("Completed"), Qt::CaseInsensitive) == 0) {
      statusIcon = QStringLiteral("Studio/check_circle.svg");
    } else if (status.compare(QStringLiteral("Failed"), Qt::CaseInsensitive) == 0) {
      statusIcon = QStringLiteral("Studio/asset_missing_small.svg");
    } else if (status.compare(QStringLiteral("Paused"), Qt::CaseInsensitive) == 0) {
      statusIcon = QStringLiteral("Studio/animationmenu_pause.svg");
    }
    statusIconLabel->setPixmap(
        loadIconWithFallback(statusIcon).pixmap(QSize(16, 16)));
    nameLabel->setText(name);
    outputLabel->setText(errorMessage.trimmed().isEmpty()
        ? QStringLiteral("Output  •  %1").arg(output)
        : QStringLiteral("Error  •  %1").arg(errorMessage));
    QPalette outputPalette = outputLabel->palette();
    outputPalette.setColor(QPalette::WindowText,
        errorMessage.trimmed().isEmpty()
            ? QColor(155, 165, 175)
            : QColor(225, 95, 85));
    outputLabel->setPalette(outputPalette);
    backendLabel->setText(errorMessage.trimmed().isEmpty()
        ? backend
        : QStringLiteral("%1  |  action: retry").arg(backend));
    progressBar->setValue(std::clamp(progress, 0, 100));
    QPalette palette = statusLabel->palette();
    palette.setColor(QPalette::WindowText, accent);
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
};

} // namespace detail
} // namespace Artifact

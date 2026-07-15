module;
#include <algorithm>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QFont>
#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QScrollArea>
#include <QString>
#include <QWidget>

module Artifact.Widgets.Dialog.CompositionShell;

namespace Artifact {
namespace {
QString compositionDialogIllustrationPath(const QString& relativePath) {
  const QString appDir = QCoreApplication::applicationDirPath();
  const QString appRelative =
      QDir(appDir).filePath(QStringLiteral("Illustration/%1").arg(relativePath));
  if (QFileInfo::exists(appRelative)) {
    return appRelative;
  }
  const QString workspaceRelative = QDir(QDir::currentPath()).filePath(
      QStringLiteral("Artifact/App/Illustration/%1").arg(relativePath));
  if (QFileInfo::exists(workspaceRelative)) {
    return workspaceRelative;
  }
  return QDir::cleanPath(QDir(appDir).filePath(
      QStringLiteral("../Artifact/App/Illustration/%1").arg(relativePath)));
}

class CompositionSettingsHeader final : public QWidget {
 public:
  explicit CompositionSettingsHeader(QWidget* parent = nullptr)
      : QWidget(parent),
        banner_(compositionDialogIllustrationPath(
            QStringLiteral("Studio/composition_dialog_header.png"))) {
    setFixedHeight(82);
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(0x24, 0x24, 0x24));
    if (!banner_.isNull()) {
      const QPixmap scaled = banner_.scaled(
          size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
      painter.setOpacity(0.62);
      painter.drawPixmap(
          QPoint((width() - scaled.width()) / 2,
                 (height() - scaled.height()) / 2),
          scaled);
      painter.setOpacity(1.0);
    }
    QLinearGradient shade(rect().topLeft(), rect().topRight());
    shade.setColorAt(0.0, QColor(25, 25, 26, 235));
    shade.setColorAt(0.48, QColor(25, 25, 26, 185));
    shade.setColorAt(1.0, QColor(25, 25, 26, 86));
    painter.fillRect(rect(), shade);

    QFont titleFont = font();
    titleFont.setPointSize(std::max(12, titleFont.pointSize() + 2));
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor(238, 242, 248));
    painter.drawText(rect().adjusted(22, 12, -22, -34),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Composition Settings"));

    QFont subFont = font();
    subFont.setPointSize(std::max(8, subFont.pointSize() - 1));
    painter.setFont(subFont);
    painter.setPen(QColor(166, 176, 188, 210));
    painter.drawText(rect().adjusted(22, 44, -22, -12),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("Edit resolution, timing, color, and composition options."));
    painter.setPen(QColor(255, 255, 255, 28));
    painter.drawLine(rect().bottomLeft(), rect().bottomRight());
  }

 private:
  QPixmap banner_;
};
}  // namespace

ArtifactCompositionDialogShell::ArtifactCompositionDialogShell(QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(QStringLiteral("Composition Settings"));
  setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
  setModal(true);
  resize(620, 720);
  setMinimumSize(620, 720);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  root->addWidget(new CompositionSettingsHeader(this));

  auto* scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  auto* content = new QFrame(scrollArea);
  content_ = new QVBoxLayout(content);
  content_->setContentsMargins(20, 18, 20, 12);
  content_->setSpacing(12);
  scrollArea->setWidget(content);
  root->addWidget(scrollArea, 1);

  auto* footer = new QFrame(this);
  footer_ = new QHBoxLayout(footer);
  footer_->setContentsMargins(20, 12, 20, 14);
  footer_->addStretch(1);
  root->addWidget(footer);
}

QVBoxLayout* ArtifactCompositionDialogShell::contentLayout() const { return content_; }
QHBoxLayout* ArtifactCompositionDialogShell::footerLayout() const { return footer_; }
}

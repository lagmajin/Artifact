module;
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QFont>

module Artifact.Widgets.Dialog.CompositionShell;

namespace Artifact {
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

  auto* header = new QFrame(this);
  auto* headerLayout = new QVBoxLayout(header);
  headerLayout->setContentsMargins(20, 18, 20, 14);
  headerLayout->setSpacing(4);
  auto* title = new QLabel(QStringLiteral("Composition Settings"), header);
  auto titleFont = title->font();
  titleFont.setPointSize(16);
  titleFont.setWeight(QFont::DemiBold);
  title->setFont(titleFont);
  headerLayout->addWidget(title);
  auto* hint = new QLabel(
      QStringLiteral("Adjust the active composition without leaving the workspace."),
      header);
  hint->setWordWrap(true);
  headerLayout->addWidget(hint);
  root->addWidget(header);

  auto* content = new QFrame(this);
  content_ = new QVBoxLayout(content);
  content_->setContentsMargins(20, 18, 20, 12);
  content_->setSpacing(12);
  root->addWidget(content, 1);

  auto* footer = new QFrame(this);
  footer_ = new QHBoxLayout(footer);
  footer_->setContentsMargins(20, 12, 20, 14);
  footer_->addStretch(1);
  root->addWidget(footer);
}

QVBoxLayout* ArtifactCompositionDialogShell::contentLayout() const { return content_; }
QHBoxLayout* ArtifactCompositionDialogShell::footerLayout() const { return footer_; }
}

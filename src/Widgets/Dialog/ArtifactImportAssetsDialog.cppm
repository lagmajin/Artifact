module;
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QRegularExpression>

module Artifact.Widgets.ImportAssetsDialog;

import File.TypeDetector;

namespace Artifact {
namespace {

struct ImportGroup {
  QString title;
  QStringList paths;
};

bool isSequenceName(const QString& path)
{
  const QString fileName = QFileInfo(path).fileName();
  static const QRegularExpression sequencePattern(
      QStringLiteral(R"((?:^|[^A-Za-z])(?:\d{3,})(?=\.[^.]+$))"));
  return sequencePattern.match(fileName).hasMatch();
}

} // namespace

ArtifactImportAssetsDialog::ArtifactImportAssetsDialog(const QStringList& files, QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(QStringLiteral("Project/Assets に取り込み"));
  resize(560, 360);

  ArtifactCore::FileTypeDetector detector;
  ImportGroup stillImages{QStringLiteral("静止画（連番以外）")};
  ImportGroup videoFiles{QStringLiteral("動画系")};
  ImportGroup audioFiles{QStringLiteral("音声")};
  ImportGroup sequences{QStringLiteral("連番")};
  ImportGroup otherFiles{QStringLiteral("その他")};

  auto targetGroup = [&](const QString& path) -> ImportGroup* {
    switch (detector.detectByExtension(path)) {
      case ArtifactCore::FileType::Image:
        return isSequenceName(path) ? &sequences : &stillImages;
      case ArtifactCore::FileType::Video:
        return &videoFiles;
      case ArtifactCore::FileType::Audio:
        return &audioFiles;
      default:
        return &otherFiles;
    }
  };
  for (const QString& path : files) {
    if (auto* group = targetGroup(path)) {
      group->paths.append(path);
    }
  }

  auto* layout = new QVBoxLayout(this);
  auto* description = new QLabel(
      QStringLiteral("選択したアセットは現在のプロジェクトの Assets フォルダへコピーしてから取り込みます。"),
      this);
  description->setWordWrap(true);
  layout->addWidget(description);

  auto* tree = new QTreeWidget(this);
  tree->setHeaderLabels({QStringLiteral("Group"), QStringLiteral("Items")});
  tree->setRootIsDecorated(false);
  tree->setSelectionMode(QAbstractItemView::NoSelection);
  tree->setAlternatingRowColors(true);
  layout->addWidget(tree);

  auto addGroup = [&](const ImportGroup& group) {
    if (group.paths.isEmpty()) {
      return;
    }
    auto* item = new QTreeWidgetItem(tree);
    item->setText(0, group.title);
    item->setText(1, QString::number(group.paths.size()));
    item->setCheckState(0, Qt::Checked);
    item->setData(0, Qt::UserRole, group.paths);
  };
  addGroup(stillImages);
  addGroup(videoFiles);
  addGroup(audioFiles);
  addGroup(sequences);
  addGroup(otherFiles);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Project/Assets にコピーして取り込む"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("キャンセル"));
  layout->addWidget(buttons);
  QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QStringList ArtifactImportAssetsDialog::selectedPaths() const
{
  auto* tree = findChild<QTreeWidget*>();
  if (!tree) {
    return {};
  }
  QStringList filtered;
  for (int i = 0; i < tree->topLevelItemCount(); ++i) {
    auto* item = tree->topLevelItem(i);
    if (!item || item->checkState(0) != Qt::Checked) {
      continue;
    }
    filtered.append(item->data(0, Qt::UserRole).toStringList());
  }
  return filtered;
}

} // namespace Artifact

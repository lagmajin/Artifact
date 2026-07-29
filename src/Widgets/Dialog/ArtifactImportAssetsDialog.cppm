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
  setAccessibleName(QStringLiteral("Import Assets Dialog"));
  setAccessibleDescription(QStringLiteral("Select asset groups to copy into the project Assets folder"));
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
  description->setAccessibleName(QStringLiteral("Import destination description"));
  layout->addWidget(description);

  auto* tree = new QTreeWidget(this);
  tree->setHeaderLabels({QStringLiteral("Group"), QStringLiteral("Items")});
  tree->setRootIsDecorated(false);
  tree->setSelectionMode(QAbstractItemView::NoSelection);
  tree->setAlternatingRowColors(true);
  tree->setAccessibleName(QStringLiteral("Asset import groups"));
  tree->setAccessibleDescription(QStringLiteral("Checked groups will be copied into the project Assets folder"));
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
  buttons->setAccessibleName(QStringLiteral("Asset import actions"));
  buttons->button(QDialogButtonBox::Ok)->setAccessibleName(QStringLiteral("Import selected assets"));
  buttons->button(QDialogButtonBox::Ok)->setAccessibleDescription(QStringLiteral("Copy checked asset groups into the project Assets folder"));
  buttons->button(QDialogButtonBox::Cancel)->setAccessibleName(QStringLiteral("Cancel asset import"));
  buttons->button(QDialogButtonBox::Cancel)->setAccessibleDescription(QStringLiteral("Close without importing assets"));
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

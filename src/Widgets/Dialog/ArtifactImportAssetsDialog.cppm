module;
#include <algorithm>
#include <QAbstractItemView>
#include <QColor>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QRegularExpression>
#include <QStringList>

module Artifact.Widgets.ImportAssetsDialog;

import File.TypeDetector;
import Widgets.Utils.CSS;

namespace Artifact {
namespace {

struct ImportGroup {
  QString title;
  QStringList paths;
};

QString formatByteSize(const quint64 bytes)
{
  constexpr quint64 kKilobyte = 1024;
  constexpr quint64 kMegabyte = kKilobyte * 1024;
  constexpr quint64 kGigabyte = kMegabyte * 1024;
  if (bytes >= kGigabyte) {
    return QStringLiteral("%1 GB").arg(
        static_cast<double>(bytes) / static_cast<double>(kGigabyte), 0, 'f', 1);
  }
  if (bytes >= kMegabyte) {
    return QStringLiteral("%1 MB").arg(
        static_cast<double>(bytes) / static_cast<double>(kMegabyte), 0, 'f', 1);
  }
  return QStringLiteral("%1 KB").arg(
      static_cast<double>(bytes) / static_cast<double>(kKilobyte), 0, 'f', 1);
}

QString groupDetails(const ImportGroup& group)
{
  if (group.paths.isEmpty()) return QStringLiteral("-");
  if (group.title == QStringLiteral("連番") && group.paths.size() > 1) {
    return QStringLiteral("%1 – %2")
        .arg(QFileInfo(group.paths.first()).fileName(),
             QFileInfo(group.paths.last()).fileName());
  }
  const QString firstName = QFileInfo(group.paths.first()).fileName();
  return group.paths.size() > 1
      ? QStringLiteral("%1 ほか").arg(firstName)
      : firstName;
}

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
  setWindowTitle(QStringLiteral("Import Assets"));
  setAccessibleName(QStringLiteral("Import Assets Dialog"));
  setAccessibleDescription(QStringLiteral("Select asset groups to copy into the project Assets folder"));
  setMinimumSize(820, 540);

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

  quint64 totalBytes = 0;
  QHash<QString, int> fileNameCounts;
  for (const QString& path : files) {
    const QFileInfo info(path);
    totalBytes += static_cast<quint64>(std::max<qint64>(0, info.size()));
    const QString key = info.fileName().toCaseFolded();
    fileNameCounts.insert(key, fileNameCounts.value(key) + 1);
  }
  int duplicateFileCount = 0;
  for (auto it = fileNameCounts.cbegin(); it != fileNameCounts.cend(); ++it) {
    if (it.value() > 1) duplicateFileCount += it.value();
  }

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 20, 24, 18);
  layout->setSpacing(14);
  auto* title = new QLabel(QStringLiteral("Import Assets"), this);
  QFont titleFont = title->font();
  titleFont.setBold(true);
  titleFont.setPointSize(14);
  title->setFont(titleFont);
  layout->addWidget(title);
  auto* description = new QLabel(
      QStringLiteral("選択したアセットは現在のプロジェクトの Assets フォルダへコピーしてから取り込みます。"),
      this);
  description->setWordWrap(true);
  description->setAccessibleName(QStringLiteral("Import destination description"));
  QPalette descriptionPalette = description->palette();
  descriptionPalette.setColor(QPalette::WindowText,
                              palette().color(QPalette::PlaceholderText));
  description->setPalette(descriptionPalette);
  layout->addWidget(description);

  auto* summary = new QFrame(this);
  summary->setFrameShape(QFrame::StyledPanel);
  auto* summaryLayout = new QHBoxLayout(summary);
  summaryLayout->setContentsMargins(18, 12, 18, 12);
  auto* countLabel = new QLabel(QStringLiteral("%1 files\nSelected for import").arg(files.size()), summary);
  auto* sizeLabel = new QLabel(QStringLiteral("%1\nTotal size").arg(formatByteSize(totalBytes)), summary);
  auto* destinationLabel = new QLabel(QStringLiteral("Project/Assets\nDestination folder"), summary);
  QFont summaryFont = countLabel->font();
  summaryFont.setBold(true);
  summaryFont.setPointSize(11);
  countLabel->setFont(summaryFont);
  sizeLabel->setFont(summaryFont);
  destinationLabel->setFont(summaryFont);
  summaryLayout->addWidget(countLabel);
  summaryLayout->addStretch();
  summaryLayout->addWidget(sizeLabel);
  summaryLayout->addStretch();
  summaryLayout->addWidget(destinationLabel);
  layout->addWidget(summary);

  auto* tree = new QTreeWidget(this);
  tree->setHeaderLabels({QStringLiteral("Group"), QStringLiteral("Items"),
                         QStringLiteral("Details")});
  tree->setRootIsDecorated(false);
  tree->setSelectionMode(QAbstractItemView::NoSelection);
  tree->setAlternatingRowColors(true);
  tree->setMinimumHeight(220);
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
    item->setText(2, groupDetails(group));
    item->setCheckState(0, Qt::Checked);
    item->setData(0, Qt::UserRole, group.paths);
  };
  addGroup(stillImages);
  addGroup(videoFiles);
  addGroup(audioFiles);
  addGroup(sequences);
  addGroup(otherFiles);

  tree->resizeColumnToContents(0);
  tree->resizeColumnToContents(1);

  auto* resultRow = new QFrame(this);
  resultRow->setFrameShape(QFrame::StyledPanel);
  auto* resultLayout = new QHBoxLayout(resultRow);
  resultLayout->setContentsMargins(12, 8, 12, 8);
  auto* warningLabel = new QLabel(resultRow);
  if (duplicateFileCount > 0) {
    warningLabel->setText(QStringLiteral("⚠ %1 件の同名ファイルは確認が必要です")
                              .arg(duplicateFileCount));
    QPalette warningPalette = warningLabel->palette();
    warningPalette.setColor(QPalette::WindowText,
                            QColor(ArtifactCore::currentDCCTheme().accentColor));
    warningLabel->setPalette(warningPalette);
  }
  resultLayout->addWidget(warningLabel);
  resultLayout->addStretch();
  resultLayout->addWidget(new QLabel(
      QStringLiteral("取り込み候補: %1 ファイル / %2")
          .arg(files.size()).arg(formatByteSize(totalBytes)), resultRow));
  layout->addWidget(resultRow);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Project/Assets にコピーして取り込む"));
  buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("キャンセル"));
  buttons->setAccessibleName(QStringLiteral("Asset import actions"));
  buttons->button(QDialogButtonBox::Ok)->setAccessibleName(QStringLiteral("Import selected assets"));
  buttons->button(QDialogButtonBox::Ok)->setAccessibleDescription(QStringLiteral("Copy checked asset groups into the project Assets folder"));
  buttons->button(QDialogButtonBox::Cancel)->setAccessibleName(QStringLiteral("Cancel asset import"));
  buttons->button(QDialogButtonBox::Cancel)->setAccessibleDescription(QStringLiteral("Close without importing assets"));
  buttons->button(QDialogButtonBox::Ok)->setMinimumHeight(36);
  buttons->button(QDialogButtonBox::Cancel)->setMinimumHeight(36);
  QPalette importPalette = buttons->button(QDialogButtonBox::Ok)->palette();
  importPalette.setColor(QPalette::Button, QColor(43, 111, 232));
  importPalette.setColor(QPalette::ButtonText, Qt::white);
  buttons->button(QDialogButtonBox::Ok)->setPalette(importPalette);
  buttons->button(QDialogButtonBox::Ok)->setAutoFillBackground(true);
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

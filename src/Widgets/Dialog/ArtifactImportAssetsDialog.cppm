module;
#include <QAbstractItemView>
#include <QCheckBox>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFont>
#include <QFrame>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMouseEvent>
#include <QPalette>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSize>
#include <QStandardPaths>
#include <QTimer>
#include <QTreeWidget>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>
#include <QRegularExpression>
#include <QStringList>

module Artifact.Widgets.ImportAssetsDialog;

import Core.ArtifactMath;
import File.TypeDetector;
import Artifact.Widgets.AssetThumbnailPipeline;
import Widgets.Utils.CSS;
import Translation.Manager;

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
  if (group.title == TranslationManager::instance().tr(QStringLiteral("import.group.sequence"), QStringLiteral("連番")) && group.paths.size() > 1) {
    return QStringLiteral("%1 – %2")
        .arg(QFileInfo(group.paths.first()).fileName(),
             QFileInfo(group.paths.last()).fileName());
  }
  const QString firstName = QFileInfo(group.paths.first()).fileName();
  return group.paths.size() > 1
      ? TranslationManager::instance().tr(QStringLiteral("import.other_files_suffix"), QStringLiteral("%1 ほか")).arg(firstName)
      : firstName;
}

bool isSequenceName(const QString& path)
{
  const QString fileName = QFileInfo(path).fileName();
  static const QRegularExpression sequencePattern(
      QStringLiteral(R"((?:^|[^A-Za-z])(?:\d{3,})(?=\.[^.]+$))"));
  return sequencePattern.match(fileName).hasMatch();
}

class MediaPickerFileModel final : public QFileSystemModel {
public:
  explicit MediaPickerFileModel(QObject* parent = nullptr) : QFileSystemModel(parent) {}

  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    if (role == Qt::DecorationRole) {
      const QFileInfo info = fileInfo(index);
      if (info.isFile()) {
        auto it = cachedIcons_.constFind(info.absoluteFilePath());
        if (it != cachedIcons_.cend()) return it.value();
        const QIcon cached = AssetThumbnail::loadFromDisk(info);
        if (!cached.isNull()) {
          cachedIcons_.insert(info.absoluteFilePath(), cached);
          return cached;
        }
      }
    }
    return QFileSystemModel::data(index, role);
  }

private:
  mutable QHash<QString, QIcon> cachedIcons_;
};

class MediaPickerProxyModel final : public QSortFilterProxyModel {
public:
  explicit MediaPickerProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

  void setMediaCategory(const QString& category) { category_ = category; invalidateFilter(); }
  void setSearchText(const QString& text) { search_ = text.trimmed(); invalidateFilter(); }

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
    const auto* fs = qobject_cast<const QFileSystemModel*>(sourceModel());
    if (!fs) return true;
    const QFileInfo info = fs->fileInfo(fs->index(sourceRow, 0, sourceParent));
    if (info.isDir()) return true;
    if (!search_.isEmpty() && !info.fileName().contains(search_, Qt::CaseInsensitive)) return false;
    const ArtifactCore::FileType type = detector_.detectByExtension(info.absoluteFilePath());
    if (category_ == QStringLiteral("image")) return type == ArtifactCore::FileType::Image;
    if (category_ == QStringLiteral("video")) return type == ArtifactCore::FileType::Video;
    if (category_ == QStringLiteral("audio")) return type == ArtifactCore::FileType::Audio;
    if (category_ == QStringLiteral("3d")) return type == ArtifactCore::FileType::Model3D;
    return type == ArtifactCore::FileType::Image || type == ArtifactCore::FileType::Video ||
           type == ArtifactCore::FileType::Audio || type == ArtifactCore::FileType::Model3D;
  }

private:
  QString category_ = QStringLiteral("all");
  QString search_;
  mutable ArtifactCore::FileTypeDetector detector_;
};

QString formatPickerSize(const quint64 bytes)
{
  constexpr quint64 kKiB = 1024;
  constexpr quint64 kMiB = kKiB * 1024;
  constexpr quint64 kGiB = kMiB * 1024;
  if (bytes >= kGiB) return QStringLiteral("%1 GB").arg(double(bytes) / double(kGiB), 0, 'f', 1);
  if (bytes >= kMiB) return QStringLiteral("%1 MB").arg(double(bytes) / double(kMiB), 0, 'f', 1);
  return QStringLiteral("%1 KB").arg(double(bytes) / double(kKiB), 0, 'f', 1);
}

QString pickerTypeLabel(const ArtifactCore::FileType type)
{
  switch (type) {
    case ArtifactCore::FileType::Image: return QStringLiteral("Image");
    case ArtifactCore::FileType::Video: return QStringLiteral("Video");
    case ArtifactCore::FileType::Audio: return QStringLiteral("Audio");
    case ArtifactCore::FileType::Model3D: return QStringLiteral("3D Model");
    default: return QStringLiteral("Unsupported");
  }
}

QListView* mediaPickerView(const QDialog* dialog) {
  return dialog->findChild<QListView*>(QStringLiteral("mediaPickerView"));
}
QFileSystemModel* mediaPickerFileModel(const QDialog* dialog) {
  return dialog->findChild<QFileSystemModel*>(QStringLiteral("mediaPickerFileModel"));
}
MediaPickerProxyModel* mediaPickerProxy(const QDialog* dialog) {
  return static_cast<MediaPickerProxyModel*>(
      dialog->findChild<QSortFilterProxyModel*>(QStringLiteral("mediaPickerProxy")));
}

QStringList selectedPickerFiles(const QDialog* dialog)
{
  auto* view = mediaPickerView(dialog);
  auto* model = mediaPickerFileModel(dialog);
  auto* proxy = mediaPickerProxy(dialog);
  if (!view || !model || !proxy || !view->selectionModel()) return {};
  QStringList paths;
  for (const QModelIndex& proxyIndex : view->selectionModel()->selectedIndexes()) {
    const QFileInfo info = model->fileInfo(proxy->mapToSource(proxyIndex));
    if (info.isFile()) paths.append(info.absoluteFilePath());
  }
  paths.removeDuplicates();
  return paths;
}

void refreshMediaPickerDetails(QDialog* dialog)
{
  const QStringList paths = selectedPickerFiles(dialog);
  auto* name = dialog->findChild<QLabel*>(QStringLiteral("mediaPickerName"));
  auto* details = dialog->findChild<QLabel*>(QStringLiteral("mediaPickerDetails"));
  auto* status = dialog->findChild<QLabel*>(QStringLiteral("mediaPickerStatus"));
  auto* proceed = dialog->findChild<QPushButton*>(QStringLiteral("mediaPickerContinue"));
  if (!name || !details || !status || !proceed) return;
  quint64 bytes = 0;
  for (const QString& path : paths) bytes += static_cast<quint64>(ArtifactCore::artifactMax<qint64>(0, QFileInfo(path).size()));
  if (paths.isEmpty()) {
    name->setText(QStringLiteral("No media selected"));
    details->setText(QStringLiteral("Select one or more supported files.\n\nFolders open with double-click."));
    status->setText(QStringLiteral("0 items selected"));
    proceed->setEnabled(false);
    return;
  }
  const QFileInfo first(paths.first());
  ArtifactCore::FileTypeDetector detector;
  name->setText(first.fileName());
  name->setToolTip(first.absoluteFilePath());
  details->setText(QStringLiteral("Type        %1%2\nSize        %3\nModified    %4\n\n%5")
      .arg(pickerTypeLabel(detector.detectByExtension(first.absoluteFilePath())),
           isSequenceName(first.absoluteFilePath()) ? QStringLiteral(" · Sequence candidate") : QString(),
           formatPickerSize(static_cast<quint64>(ArtifactCore::artifactMax<qint64>(0, first.size()))),
           first.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm")),
           paths.size() > 1 ? QStringLiteral("%1 files selected").arg(paths.size())
                            : QStringLiteral("Ready to continue")));
  status->setText(QStringLiteral("%1 items selected · %2").arg(paths.size()).arg(formatPickerSize(bytes)));
  proceed->setEnabled(true);
}

} // namespace

namespace {

QListWidget* projectPickerView(const QDialog* dialog)
{
  return dialog->findChild<QListWidget*>(QStringLiteral("projectPickerView"));
}

void refreshProjectPickerDetails(QDialog* dialog)
{
  auto* view = projectPickerView(dialog);
  auto* name = dialog->findChild<QLabel*>(QStringLiteral("projectPickerName"));
  auto* details = dialog->findChild<QLabel*>(QStringLiteral("projectPickerDetails"));
  auto* status = dialog->findChild<QLabel*>(QStringLiteral("projectPickerStatus"));
  auto* open = dialog->findChild<QPushButton*>(QStringLiteral("projectPickerOpen"));
  if (!view || !name || !details || !status || !open) return;
  auto* item = view->currentItem();
  const QString path = item ? item->data(Qt::UserRole).toString() : QString();
  const QFileInfo info(path);
  const bool valid = info.exists() && info.isFile();
  if (!valid) {
    name->setText(QStringLiteral("No project selected"));
    details->setText(QStringLiteral("Select a recent project, or choose a project file from the system picker."));
    status->setText(QStringLiteral("%1 recent projects").arg(view->count()));
    open->setEnabled(false);
    return;
  }
  name->setText(info.completeBaseName());
  name->setToolTip(info.absoluteFilePath());
  details->setText(QStringLiteral("Path        %1\nModified    %2\nSize        %3\nStatus      Not checked until opening")
      .arg(QDir::toNativeSeparators(info.absoluteFilePath()),
           info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm")),
           formatPickerSize(static_cast<quint64>(ArtifactCore::artifactMax<qint64>(0, info.size())))));
  status->setText(QStringLiteral("Ready to open · %1").arg(info.fileName()));
  open->setEnabled(true);
}

} // namespace

ArtifactProjectOpenPickerDialog::ArtifactProjectOpenPickerDialog(
    const QStringList& recentProjects, QWidget* parent) : QDialog(parent)
{
  setWindowTitle(QStringLiteral("Open Project"));
  setAccessibleName(QStringLiteral("Project Open Picker"));
  setMinimumSize(880, 530);
  resize(1060, 620);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(14, 12, 14, 12);
  root->setSpacing(8);
  auto* heading = new QLabel(QStringLiteral("Open Project"), this);
  QFont headingFont = heading->font();
  headingFont.setBold(true);
  heading->setFont(headingFont);
  root->addWidget(heading);
  auto* search = new QLineEdit(this);
  search->setObjectName(QStringLiteral("projectPickerSearch"));
  search->setPlaceholderText(QStringLiteral("Search recent projects…"));
  search->setClearButtonEnabled(true);
  search->installEventFilter(this);
  root->addWidget(search);

  auto* content = new QHBoxLayout();
  content->setSpacing(8);
  auto* places = new QListWidget(this);
  places->setObjectName(QStringLiteral("projectPickerPlaces"));
  places->setFixedWidth(164);
  auto* recent = new QListWidgetItem(QStringLiteral("Recent Projects"), places);
  recent->setData(Qt::UserRole, QStringLiteral("recent"));
  auto* system = new QListWidgetItem(QStringLiteral("System Files…"), places);
  system->setData(Qt::UserRole, QStringLiteral("system"));
  places->installEventFilter(this);
  content->addWidget(places);

  auto* view = new QListWidget(this);
  view->setObjectName(QStringLiteral("projectPickerView"));
  view->setViewMode(QListView::IconMode);
  view->setResizeMode(QListView::Adjust);
  view->setMovement(QListView::Static);
  view->setIconSize(QSize(112, 72));
  view->setGridSize(QSize(176, 132));
  view->setSpacing(6);
  view->setWordWrap(true);
  view->installEventFilter(this);
  for (const QString& path : recentProjects) {
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) continue;
    auto* item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("document-open")),
                                     QStringLiteral("%1\n%2").arg(info.completeBaseName(),
                                       info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"))), view);
    item->setData(Qt::UserRole, info.absoluteFilePath());
    item->setToolTip(QDir::toNativeSeparators(info.absoluteFilePath()));
  }
  content->addWidget(view, 1);

  auto* inspector = new QFrame(this);
  inspector->setFrameShape(QFrame::StyledPanel);
  inspector->setFixedWidth(292);
  auto* inspectorLayout = new QVBoxLayout(inspector);
  inspectorLayout->setContentsMargins(16, 16, 16, 16);
  inspectorLayout->setSpacing(10);
  auto* inspectorHeading = new QLabel(QStringLiteral("Project details"), inspector);
  QFont inspectorFont = inspectorHeading->font();
  inspectorFont.setBold(true);
  inspectorHeading->setFont(inspectorFont);
  auto* name = new QLabel(QStringLiteral("No project selected"), inspector);
  name->setObjectName(QStringLiteral("projectPickerName"));
  name->setWordWrap(true);
  QFont nameFont = name->font();
  nameFont.setBold(true);
  name->setFont(nameFont);
  auto* details = new QLabel(QStringLiteral("Select a recent project, or choose a project file from the system picker."), inspector);
  details->setObjectName(QStringLiteral("projectPickerDetails"));
  details->setWordWrap(true);
  auto* note = new QLabel(QStringLiteral("Project health and external-source checks remain owned by the project loading service."), inspector);
  note->setWordWrap(true);
  QPalette notePalette = note->palette();
  notePalette.setColor(QPalette::WindowText, palette().color(QPalette::PlaceholderText));
  note->setPalette(notePalette);
  inspectorLayout->addWidget(inspectorHeading);
  inspectorLayout->addWidget(name);
  inspectorLayout->addWidget(details);
  inspectorLayout->addStretch();
  inspectorLayout->addWidget(note);
  content->addWidget(inspector);
  root->addLayout(content, 1);

  auto* footer = new QHBoxLayout();
  auto* systemPicker = new QPushButton(QStringLiteral("Use System Picker…"), this);
  systemPicker->setObjectName(QStringLiteral("projectPickerSystem"));
  systemPicker->installEventFilter(this);
  auto* status = new QLabel(this);
  status->setObjectName(QStringLiteral("projectPickerStatus"));
  auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
  cancel->setObjectName(QStringLiteral("projectPickerCancel"));
  cancel->installEventFilter(this);
  auto* open = new QPushButton(QStringLiteral("Open Project"), this);
  open->setObjectName(QStringLiteral("projectPickerOpen"));
  open->setDefault(true);
  open->installEventFilter(this);
  footer->addWidget(systemPicker);
  footer->addWidget(status);
  footer->addStretch();
  footer->addWidget(cancel);
  footer->addWidget(open);
  root->addLayout(footer);
  refreshProjectPickerDetails(this);
}

QString ArtifactProjectOpenPickerDialog::selectedPath() const
{
  const QString systemPath = property("projectPickerSystemPath").toString();
  if (!systemPath.isEmpty()) return systemPath;
  auto* view = projectPickerView(this);
  return view && view->currentItem() ? view->currentItem()->data(Qt::UserRole).toString() : QString();
}

bool ArtifactProjectOpenPickerDialog::eventFilter(QObject* watched, QEvent* event)
{
  const bool keyboardActivate = event->type() == QEvent::KeyRelease &&
      (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Return ||
       static_cast<QKeyEvent*>(event)->key() == Qt::Key_Enter ||
       static_cast<QKeyEvent*>(event)->key() == Qt::Key_Space);
  const bool mouseActivate = event->type() == QEvent::MouseButtonRelease &&
      static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton;
  const bool activate = keyboardActivate || mouseActivate;
  const QString name = watched->objectName();
  auto* view = projectPickerView(this);
  if (name == QStringLiteral("projectPickerView")) {
    if (event->type() == QEvent::MouseButtonDblClick && view) {
      if (view->itemAt(static_cast<QMouseEvent*>(event)->position().toPoint())) accept();
      return true;
    }
    if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::KeyRelease)
      QTimer::singleShot(0, this, [this]() { refreshProjectPickerDetails(this); });
  } else if (name == QStringLiteral("projectPickerSearch") && event->type() == QEvent::KeyRelease && view) {
    const QString search = static_cast<QLineEdit*>(watched)->text().trimmed();
    for (int index = 0; index < view->count(); ++index)
      view->item(index)->setHidden(!search.isEmpty() &&
          !view->item(index)->text().contains(search, Qt::CaseInsensitive));
    refreshProjectPickerDetails(this);
  } else if ((name == QStringLiteral("projectPickerSystem") || name == QStringLiteral("projectPickerPlaces")) && activate) {
    bool chooseSystem = name == QStringLiteral("projectPickerSystem");
    if (!chooseSystem && event->type() == QEvent::MouseButtonRelease) {
      auto* places = static_cast<QListWidget*>(watched);
      if (auto* item = places->itemAt(static_cast<QMouseEvent*>(event)->position().toPoint()))
        chooseSystem = item->data(Qt::UserRole).toString() == QStringLiteral("system");
    }
    if (chooseSystem) {
      const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Project"), QString(),
          QStringLiteral("Artifact Project (*.artifact *.json);;All Files (*.*)"));
      if (!path.isEmpty()) { setProperty("projectPickerSystemPath", path); accept(); }
      return true;
    }
  } else if (name == QStringLiteral("projectPickerCancel") && activate) {
    reject();
    return true;
  } else if (name == QStringLiteral("projectPickerOpen") && activate) {
    if (!selectedPath().isEmpty()) accept();
    return true;
  }
  return QDialog::eventFilter(watched, event);
}

ArtifactMediaImportPickerDialog::ArtifactMediaImportPickerDialog(QWidget* parent) : QDialog(parent)
{
  setWindowTitle(QStringLiteral("Media Import Picker"));
  setAccessibleName(QStringLiteral("Media Import Picker"));
  setMinimumSize(980, 640);
  resize(1180, 720);
  setProperty("mediaPickerCurrentPath", QString());

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(14, 12, 14, 12);
  root->setSpacing(8);

  auto* navigation = new QHBoxLayout();
  navigation->setSpacing(6);
  auto makeActionButton = [&](const QString& label, const QString& name) {
    auto* button = new QPushButton(label, this);
    button->setObjectName(name);
    button->setFixedWidth(38);
    button->installEventFilter(this);
    return button;
  };
  navigation->addWidget(makeActionButton(QStringLiteral("←"), QStringLiteral("mediaPickerBack")));
  navigation->addWidget(makeActionButton(QStringLiteral("↑"), QStringLiteral("mediaPickerUp")));
  auto* pathEdit = new QLineEdit(this);
  pathEdit->setObjectName(QStringLiteral("mediaPickerPath"));
  pathEdit->setAccessibleName(QStringLiteral("Current media folder"));
  pathEdit->installEventFilter(this);
  auto* search = new QLineEdit(this);
  search->setObjectName(QStringLiteral("mediaPickerSearch"));
  search->setPlaceholderText(QStringLiteral("Search media…"));
  search->setClearButtonEnabled(true);
  search->setFixedWidth(270);
  search->installEventFilter(this);
  navigation->addWidget(pathEdit, 1);
  navigation->addWidget(search);
  root->addLayout(navigation);

  auto* filterRow = new QHBoxLayout();
  filterRow->setSpacing(6);
  const struct { const char* key; const char* label; } filters[] = {
      {"all", "All Media"}, {"image", "Images"}, {"video", "Video"},
      {"audio", "Audio"}, {"3d", "3D"}};
  for (const auto& entry : filters) {
    auto* button = new QPushButton(QString::fromLatin1(entry.label), this);
    button->setObjectName(QStringLiteral("mediaPickerFilter"));
    button->setProperty("mediaCategory", QString::fromLatin1(entry.key));
    button->setCheckable(true);
    button->setChecked(QString::fromLatin1(entry.key) == QStringLiteral("all"));
    button->installEventFilter(this);
    filterRow->addWidget(button);
  }
  filterRow->addStretch();
  root->addLayout(filterRow);

  auto* content = new QHBoxLayout();
  content->setSpacing(8);
  auto* places = new QListWidget(this);
  places->setObjectName(QStringLiteral("mediaPickerPlaces"));
  places->setFixedWidth(176);
  const struct { const char* label; QStandardPaths::StandardLocation location; } locations[] = {
      {"Home", QStandardPaths::HomeLocation}, {"Desktop", QStandardPaths::DesktopLocation},
      {"Documents", QStandardPaths::DocumentsLocation}, {"Pictures", QStandardPaths::PicturesLocation},
      {"Movies", QStandardPaths::MoviesLocation}, {"Music", QStandardPaths::MusicLocation}};
  for (const auto& location : locations) {
    auto* item = new QListWidgetItem(QString::fromLatin1(location.label), places);
    item->setData(Qt::UserRole, QStandardPaths::writableLocation(location.location));
  }
  places->installEventFilter(this);
  content->addWidget(places);

  const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
  const QString startPath = pictures.isEmpty() ? QDir::homePath() : pictures;
  auto* fileModel = new MediaPickerFileModel(this);
  fileModel->setObjectName(QStringLiteral("mediaPickerFileModel"));
  fileModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
  fileModel->setRootPath(startPath);
  auto* proxy = new MediaPickerProxyModel(this);
  proxy->setObjectName(QStringLiteral("mediaPickerProxy"));
  proxy->setSourceModel(fileModel);
  auto* view = new QListView(this);
  view->setObjectName(QStringLiteral("mediaPickerView"));
  view->setModel(proxy);
  view->setRootIndex(proxy->mapFromSource(fileModel->index(startPath)));
  view->setViewMode(QListView::IconMode);
  view->setResizeMode(QListView::Adjust);
  view->setMovement(QListView::Static);
  view->setIconSize(QSize(144, 94));
  view->setGridSize(QSize(176, 142));
  view->setSpacing(4);
  view->setWordWrap(true);
  view->setUniformItemSizes(true);
  view->setSelectionMode(QAbstractItemView::ExtendedSelection);
  view->installEventFilter(this);
  content->addWidget(view, 1);

  auto* inspector = new QFrame(this);
  inspector->setFrameShape(QFrame::StyledPanel);
  inspector->setFixedWidth(270);
  auto* inspectorLayout = new QVBoxLayout(inspector);
  inspectorLayout->setContentsMargins(16, 16, 16, 16);
  inspectorLayout->setSpacing(10);
  auto* heading = new QLabel(QStringLiteral("Import selection"), inspector);
  QFont headingFont = heading->font();
  headingFont.setBold(true);
  heading->setFont(headingFont);
  auto* selectedName = new QLabel(QStringLiteral("No media selected"), inspector);
  selectedName->setObjectName(QStringLiteral("mediaPickerName"));
  selectedName->setWordWrap(true);
  QFont selectedFont = selectedName->font();
  selectedFont.setBold(true);
  selectedName->setFont(selectedFont);
  auto* details = new QLabel(QStringLiteral("Select one or more supported files.\n\nFolders open with double-click."), inspector);
  details->setObjectName(QStringLiteral("mediaPickerDetails"));
  details->setWordWrap(true);
  auto* sequence = new QCheckBox(QStringLiteral("Detect image sequences"), inspector);
  sequence->setObjectName(QStringLiteral("mediaPickerSequence"));
  sequence->setChecked(true);
  sequence->setAccessibleDescription(QStringLiteral("Expand a selected numbered image to matching sibling frames"));
  inspectorLayout->addWidget(heading);
  inspectorLayout->addWidget(selectedName);
  inspectorLayout->addWidget(details);
  inspectorLayout->addStretch();
  inspectorLayout->addWidget(sequence);
  auto* note = new QLabel(QStringLiteral("Color space, alpha and proxy settings remain editable after import."), inspector);
  note->setWordWrap(true);
  QPalette notePalette = note->palette();
  notePalette.setColor(QPalette::WindowText, palette().color(QPalette::PlaceholderText));
  note->setPalette(notePalette);
  inspectorLayout->addWidget(note);
  content->addWidget(inspector);
  root->addLayout(content, 1);
  pathEdit->setText(QDir::toNativeSeparators(startPath));
  setProperty("mediaPickerCurrentPath", startPath);

  auto* footer = new QHBoxLayout();
  auto* system = new QPushButton(QStringLiteral("Use System Picker…"), this);
  system->setObjectName(QStringLiteral("mediaPickerSystem"));
  system->installEventFilter(this);
  auto* status = new QLabel(QStringLiteral("0 items selected"), this);
  status->setObjectName(QStringLiteral("mediaPickerStatus"));
  auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
  cancel->setObjectName(QStringLiteral("mediaPickerCancel"));
  cancel->installEventFilter(this);
  auto* proceed = new QPushButton(QStringLiteral("Continue"), this);
  proceed->setObjectName(QStringLiteral("mediaPickerContinue"));
  proceed->setDefault(true);
  proceed->setEnabled(false);
  proceed->installEventFilter(this);
  footer->addWidget(system);
  footer->addWidget(status);
  footer->addStretch();
  footer->addWidget(cancel);
  footer->addWidget(proceed);
  root->addLayout(footer);
}

QStringList ArtifactMediaImportPickerDialog::selectedPaths() const
{
  const QStringList systemPaths = property("systemPickerPaths").toStringList();
  if (!systemPaths.isEmpty()) return systemPaths;
  QStringList paths = selectedPickerFiles(this);
  auto* sequenceToggle = findChild<QCheckBox*>(QStringLiteral("mediaPickerSequence"));
  ArtifactCore::FileTypeDetector detector;
  if (!sequenceToggle || !sequenceToggle->isChecked() || paths.size() != 1 ||
      detector.detectByExtension(paths.first()) != ArtifactCore::FileType::Image ||
      !isSequenceName(paths.first())) return paths;
  const QFileInfo selected(paths.first());
  const QRegularExpression digitPattern(QStringLiteral(R"((\d{3,})(?=\.[^.]+$))"));
  const auto match = digitPattern.match(selected.fileName());
  if (!match.hasMatch()) return paths;
  QString pattern = QRegularExpression::escape(selected.fileName());
  pattern.replace(QRegularExpression::escape(match.captured(1)), QStringLiteral("\\d{%1}").arg(match.capturedLength(1)));
  const QRegularExpression siblings(QStringLiteral("^%1$").arg(pattern), QRegularExpression::CaseInsensitiveOption);
  QStringList sequencePaths;
  for (const QFileInfo& sibling : selected.dir().entryInfoList(QDir::Files, QDir::Name)) {
    if (siblings.match(sibling.fileName()).hasMatch()) sequencePaths.append(sibling.absoluteFilePath());
  }
  return sequencePaths.isEmpty() ? paths : sequencePaths;
}

bool ArtifactMediaImportPickerDialog::eventFilter(QObject* watched, QEvent* event)
{
  auto* view = mediaPickerView(this);
  auto* fileModel = mediaPickerFileModel(this);
  auto* proxy = mediaPickerProxy(this);
  auto* pathEdit = findChild<QLineEdit*>(QStringLiteral("mediaPickerPath"));
  const bool keyboardActivate = event->type() == QEvent::KeyRelease &&
      (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Return ||
       static_cast<QKeyEvent*>(event)->key() == Qt::Key_Enter ||
       static_cast<QKeyEvent*>(event)->key() == Qt::Key_Space);
  const bool mouseActivate = event->type() == QEvent::MouseButtonRelease &&
      static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton;
  const bool activate = keyboardActivate || mouseActivate;
  auto navigate = [&](const QString& path) {
    if (!view || !fileModel || !proxy || !QFileInfo(path).isDir()) return;
    const QString current = property("mediaPickerCurrentPath").toString();
    view->setRootIndex(proxy->mapFromSource(fileModel->index(path)));
    view->clearSelection();
    if (pathEdit) pathEdit->setText(QDir::toNativeSeparators(path));
    setProperty("mediaPickerPreviousPath", current);
    setProperty("mediaPickerCurrentPath", path);
    refreshMediaPickerDetails(this);
  };

  const QString name = watched->objectName();
  if (name == QStringLiteral("mediaPickerView") && view && fileModel && proxy) {
    if (event->type() == QEvent::MouseButtonDblClick) {
      const QModelIndex index = view->indexAt(static_cast<QMouseEvent*>(event)->position().toPoint());
      const QFileInfo info = fileModel->fileInfo(proxy->mapToSource(index));
      if (info.isDir()) { navigate(info.absoluteFilePath()); return true; }
    }
    if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::KeyRelease)
      QTimer::singleShot(0, this, [this]() { refreshMediaPickerDetails(this); });
  } else if (name == QStringLiteral("mediaPickerPlaces") && mouseActivate) {
    auto* places = static_cast<QListWidget*>(watched);
    if (auto* item = places->itemAt(static_cast<QMouseEvent*>(event)->position().toPoint()))
      navigate(item->data(Qt::UserRole).toString());
  } else if (name == QStringLiteral("mediaPickerFilter") && activate && proxy) {
    proxy->setMediaCategory(watched->property("mediaCategory").toString());
    for (auto* button : findChildren<QPushButton*>(QStringLiteral("mediaPickerFilter")))
      button->setChecked(button == watched);
    refreshMediaPickerDetails(this);
    return true;
  } else if (name == QStringLiteral("mediaPickerSearch") && event->type() == QEvent::KeyRelease && proxy) {
    proxy->setSearchText(static_cast<QLineEdit*>(watched)->text());
    refreshMediaPickerDetails(this);
  } else if (name == QStringLiteral("mediaPickerPath") && keyboardActivate && pathEdit) {
    navigate(QDir::fromNativeSeparators(pathEdit->text().trimmed()));
  } else if (name == QStringLiteral("mediaPickerUp") && activate && pathEdit) {
    navigate(QFileInfo(QDir::fromNativeSeparators(pathEdit->text())).dir().absolutePath());
    return true;
  } else if (name == QStringLiteral("mediaPickerBack") && activate) {
    navigate(property("mediaPickerPreviousPath").toString());
    return true;
  } else if (name == QStringLiteral("mediaPickerSystem") && activate) {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Select media"), pathEdit ? pathEdit->text() : QString(),
        QStringLiteral("Supported media (*.png *.jpg *.jpeg *.bmp *.gif *.tga *.tif *.tiff *.webp *.hdr *.exr *.ico *.dds *.ktx *.psd *.psb *.svg *.mp4 *.mov *.mkv *.avi *.webm *.mp3 *.wav *.flac *.ogg *.aac *.m4a *.obj *.fbx *.gltf *.glb *.pmd *.abc *.usd *.usda *.usdc)"));
    if (!files.isEmpty()) { setProperty("systemPickerPaths", files); accept(); }
    return true;
  } else if (name == QStringLiteral("mediaPickerCancel") && activate) {
    reject(); return true;
  } else if (name == QStringLiteral("mediaPickerContinue") && activate) {
    if (!selectedPaths().isEmpty()) accept();
    return true;
  }
  return QDialog::eventFilter(watched, event);
}

ArtifactImportAssetsDialog::ArtifactImportAssetsDialog(const QStringList& files, QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(QStringLiteral("Import Assets"));
  setAccessibleName(QStringLiteral("Import Assets Dialog"));
  setAccessibleDescription(QStringLiteral("Select asset groups to copy into the project Assets folder"));
  setMinimumSize(820, 540);

  ArtifactCore::FileTypeDetector detector;
  ImportGroup stillImages{TranslationManager::instance().tr(QStringLiteral("import.group.still_images"), QStringLiteral("静止画（連番以外）"))};
  ImportGroup videoFiles{TranslationManager::instance().tr(QStringLiteral("import.group.video"), QStringLiteral("動画系"))};
  ImportGroup audioFiles{TranslationManager::instance().tr(QStringLiteral("import.group.audio"), QStringLiteral("音声"))};
  ImportGroup sequences{TranslationManager::instance().tr(QStringLiteral("import.group.sequence"), QStringLiteral("連番"))};
  ImportGroup otherFiles{TranslationManager::instance().tr(QStringLiteral("import.group.other"), QStringLiteral("その他"))};

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
    totalBytes += static_cast<quint64>(ArtifactCore::artifactMax<qint64>(0, info.size()));
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
      TranslationManager::instance().tr(QStringLiteral("dialog.import_assets.copy_notice"), QStringLiteral("選択したアセットは現在のプロジェクトの Assets フォルダへコピーしてから取り込みます。")),
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
    warningLabel->setText(TranslationManager::instance().tr(QStringLiteral("dialog.import_assets.duplicate_warning"), QStringLiteral("⚠ %1 件の同名ファイルは確認が必要です"))
                              .arg(duplicateFileCount));
    QPalette warningPalette = warningLabel->palette();
    warningPalette.setColor(QPalette::WindowText,
                            QColor(ArtifactCore::currentDCCTheme().accentColor));
    warningLabel->setPalette(warningPalette);
  }
  resultLayout->addWidget(warningLabel);
  resultLayout->addStretch();
  resultLayout->addWidget(new QLabel(
      TranslationManager::instance().tr(QStringLiteral("dialog.import_assets.candidate_summary"), QStringLiteral("取り込み候補: %1 ファイル / %2"))
          .arg(files.size()).arg(formatByteSize(totalBytes)), resultRow));
  layout->addWidget(resultRow);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Ok)->setText(TranslationManager::instance().tr(QStringLiteral("dialog.import_assets.copy_and_import"), QStringLiteral("Project/Assets にコピーして取り込む")));
  buttons->button(QDialogButtonBox::Cancel)->setText(TranslationManager::instance().tr(QStringLiteral("dialog.button.cancel"), QStringLiteral("キャンセル")));
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

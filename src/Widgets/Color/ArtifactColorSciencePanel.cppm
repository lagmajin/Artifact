module;
#include <utility>
#include <array>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QAbstractItemView>
#include <QAction>
#include <QFormLayout>
#include <QGroupBox>
#include <QContextMenuEvent>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListView>
#include <QMouseEvent>
#include <QMenu>
#include <QHeaderView>
#include <QKeyEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QMetaObject>
#include <QThread>
#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QPointer>
#include <QFrame>
#include <QSettings>
#include <QStandardPaths>
#include <QSlider>
#include <QSizePolicy>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <limits>
#include <QVBoxLayout>
#include <wobjectimpl.h>




module Artifact.Widgets.ColorSciencePanel;
import Memory.SharedPtr;
import Color.Float;
import Color.ScienceManager;
import Color.LUT;
import Artifact.Color.Palette;
import Artifact.Color.OCIOManager;
import Event.Bus;
import Artifact.Event.Types;
import Artifact.Widgets.CompositionEditor;
import Artifact.Widgets.CompositionRenderController;
import Render.HDRMonitor;
import Color.LUTWriter;
import HistgramWidget;
import VectorScopeWidget;
import WaveformScopeWidget;
import ParadeScopeWidget;
namespace Artifact {

namespace {

struct LutPickerEntry {
  QString displayName;
  QString source;
  bool builtin = false;
};

QString lutPickerFormatLabel(const ArtifactCore::ColorLUT& lut)
{
  switch (lut.format()) {
    case ArtifactCore::LUTFormat::Cube: return QStringLiteral("3D LUT · .cube");
    case ArtifactCore::LUTFormat::_3dl: return QStringLiteral("3D LUT · .3dl");
    case ArtifactCore::LUTFormat::Csp: return QStringLiteral("CSP LUT");
    case ArtifactCore::LUTFormat::Mga: return QStringLiteral("MGA LUT");
    case ArtifactCore::LUTFormat::Look: return QStringLiteral("LOOK LUT");
    case ArtifactCore::LUTFormat::PNG: return QStringLiteral("HaldCLUT");
    default: return QStringLiteral("Unknown LUT");
  }
}

ArtifactCore::ColorLUT lutPickerLoad(const QString& source)
{
  if (source.startsWith(QStringLiteral("builtin:"))) {
    return ArtifactCore::LUTManager::instance().getLUT(source.mid(8));
  }
  return ArtifactCore::ColorLUT(source);
}

QListWidget* lutPickerGrid(const QDialog* dialog)
{
  return dialog->findChild<QListWidget*>(QStringLiteral("lutPickerGrid"));
}

QLabel* lutPickerDetails(const QDialog* dialog)
{
  return dialog->findChild<QLabel*>(QStringLiteral("lutPickerDetails"));
}

void updateLutPickerDetails(QDialog* dialog)
{
  auto* grid = lutPickerGrid(dialog);
  auto* details = lutPickerDetails(dialog);
  auto* useButton = dialog->findChild<QPushButton*>(QStringLiteral("lutPickerUse"));
  auto* count = dialog->findChild<QLabel*>(QStringLiteral("lutPickerCount"));
  if (!grid || !details || !useButton || !count) return;
  count->setText(QStringLiteral("%1 LUTs · %2 selected")
      .arg(grid->count()).arg(grid->currentItem() ? 1 : 0));
  auto* item = grid->currentItem();
  if (!item) {
    details->setText(QStringLiteral("Select a LUT to inspect its format and compatibility."));
    useButton->setEnabled(false);
    return;
  }
  const QString source = item->data(Qt::UserRole).toString();
  const ArtifactCore::ColorLUT lut = lutPickerLoad(source);
  if (!lut.isValid()) {
    details->setText(QStringLiteral("%1\n\nStatus: Incompatible\n%2")
        .arg(item->text(), lut.errorMessage()));
    useButton->setEnabled(false);
    return;
  }
  const auto size = lut.size();
  const QFileInfo info(source);
  details->setText(QStringLiteral("%1\n\nType        %2\nSize        %3³\nInput       Unspecified\nOutput      Working color space\nDomain      Parser-defined\nStatus      Compatible\n\nSource\n%4")
      .arg(item->text(), lutPickerFormatLabel(lut), QString::number(size.dimX),
           source.startsWith(QStringLiteral("builtin:"))
               ? QStringLiteral("Built-in library") : QDir::toNativeSeparators(info.absoluteFilePath())));
  useButton->setEnabled(true);
}

} // namespace

ArtifactLutColorReferencePickerDialog::ArtifactLutColorReferencePickerDialog(
    ArtifactColorScienceManager* manager, QWidget* parent)
    : QDialog(parent)
{
  setWindowTitle(QStringLiteral("LUT & Color Reference Picker"));
  setAccessibleName(QStringLiteral("LUT and Color Reference Picker"));
  setAccessibleDescription(QStringLiteral("Browse, inspect, and select a color LUT"));
  setMinimumSize(940, 620);
  resize(1120, 700);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(16, 14, 16, 14);
  root->setSpacing(8);

  auto* header = new QHBoxLayout();
  auto* title = new QLabel(QStringLiteral("LUT & Color Reference Picker"), this);
  QFont titleFont = title->font();
  titleFont.setBold(true);
  titleFont.setPointSize(13);
  title->setFont(titleFont);
  auto* search = new QLineEdit(this);
  search->setObjectName(QStringLiteral("lutPickerSearch"));
  search->setPlaceholderText(QStringLiteral("Search LUTs…"));
  search->setClearButtonEnabled(true);
  search->setFixedWidth(290);
  search->installEventFilter(this);
  header->addWidget(title);
  header->addStretch();
  header->addWidget(search);
  root->addLayout(header);

  auto* body = new QHBoxLayout();
  body->setSpacing(8);
  auto* library = new QListWidget(this);
  library->setObjectName(QStringLiteral("lutPickerLibrary"));
  library->setFixedWidth(160);
  for (const QString& section : {QStringLiteral("All LUTs"), QStringLiteral("Built-in"),
                                 QStringLiteral("Files")}) {
    auto* item = new QListWidgetItem(section, library);
    item->setData(Qt::UserRole, section);
  }
  library->setCurrentRow(0);
  library->installEventFilter(this);
  body->addWidget(library);

  auto* grid = new QListWidget(this);
  grid->setObjectName(QStringLiteral("lutPickerGrid"));
  grid->setViewMode(QListView::IconMode);
  grid->setResizeMode(QListView::Adjust);
  grid->setMovement(QListView::Static);
  grid->setIconSize(QSize(92, 64));
  grid->setGridSize(QSize(150, 108));
  grid->setWordWrap(true);
  grid->setSelectionMode(QAbstractItemView::SingleSelection);
  grid->installEventFilter(this);
  body->addWidget(grid, 1);

  auto* inspector = new QFrame(this);
  inspector->setFrameShape(QFrame::StyledPanel);
  inspector->setFixedWidth(276);
  auto* inspectorLayout = new QVBoxLayout(inspector);
  inspectorLayout->setContentsMargins(16, 16, 16, 16);
  auto* inspectorTitle = new QLabel(QStringLiteral("LUT details"), inspector);
  QFont inspectorFont = inspectorTitle->font();
  inspectorFont.setBold(true);
  inspectorTitle->setFont(inspectorFont);
  auto* preview = new QFrame(inspector);
  preview->setFrameShape(QFrame::StyledPanel);
  preview->setMinimumHeight(132);
  auto* previewLayout = new QVBoxLayout(preview);
  auto* previewTitle = new QLabel(QStringLiteral("Preview uses the active Color Science panel"), preview);
  previewTitle->setWordWrap(true);
  previewTitle->setAlignment(Qt::AlignCenter);
  previewLayout->addWidget(previewTitle);
  auto* details = new QLabel(QStringLiteral("Select a LUT to inspect its format and compatibility."), inspector);
  details->setObjectName(QStringLiteral("lutPickerDetails"));
  details->setWordWrap(true);
  auto* workingOnly = new QCheckBox(QStringLiteral("Use as working preview only"), inspector);
  workingOnly->setEnabled(false);
  workingOnly->setToolTip(QStringLiteral("Working-preview routing is not yet exposed by the color manager."));
  inspectorLayout->addWidget(inspectorTitle);
  inspectorLayout->addWidget(preview);
  inspectorLayout->addWidget(details);
  inspectorLayout->addStretch();
  inspectorLayout->addWidget(workingOnly);
  body->addWidget(inspector);
  root->addLayout(body, 1);

  std::vector<LutPickerEntry> entries;
  if (manager) {
    const auto available = manager->getAvailableLUTs();
    entries.reserve(available.size());
    for (const auto& path : available) {
      const QString source = QString::fromStdString(path);
      const bool builtin = source.startsWith(QStringLiteral("builtin:"));
      entries.push_back({builtin ? source.mid(8) : QFileInfo(source).baseName(), source, builtin});
    }
  }
  for (const auto& entry : entries) {
    auto* item = new QListWidgetItem(entry.displayName, grid);
    item->setData(Qt::UserRole, entry.source);
    item->setData(Qt::UserRole + 1, entry.builtin ? QStringLiteral("Built-in") : QStringLiteral("Files"));
    item->setToolTip(entry.source);
  }
  auto* footer = new QHBoxLayout();
  auto* importButton = new QPushButton(QStringLiteral("Import LUT…"), this);
  importButton->setObjectName(QStringLiteral("lutPickerImport"));
  importButton->installEventFilter(this);
  auto* count = new QLabel(QStringLiteral("%1 LUTs · 0 selected").arg(grid->count()), this);
  count->setObjectName(QStringLiteral("lutPickerCount"));
  auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
  cancel->setObjectName(QStringLiteral("lutPickerCancel"));
  cancel->installEventFilter(this);
  auto* use = new QPushButton(QStringLiteral("Use LUT"), this);
  use->setObjectName(QStringLiteral("lutPickerUse"));
  use->setDefault(true);
  use->setEnabled(false);
  use->installEventFilter(this);
  footer->addWidget(importButton);
  footer->addWidget(count);
  footer->addStretch();
  footer->addWidget(cancel);
  footer->addWidget(use);
  root->addLayout(footer);
}

QString ArtifactLutColorReferencePickerDialog::selectedSource() const
{
  if (const QString imported = property("lutPickerImportedSource").toString(); !imported.isEmpty()) return imported;
  auto* grid = lutPickerGrid(this);
  return grid && grid->currentItem() ? grid->currentItem()->data(Qt::UserRole).toString() : QString();
}

bool ArtifactLutColorReferencePickerDialog::eventFilter(QObject* watched, QEvent* event)
{
  const bool keyActivate = event->type() == QEvent::KeyRelease &&
      (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Return ||
       static_cast<QKeyEvent*>(event)->key() == Qt::Key_Enter ||
       static_cast<QKeyEvent*>(event)->key() == Qt::Key_Space);
  const bool mouseActivate = event->type() == QEvent::MouseButtonRelease &&
      static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton;
  const bool activate = keyActivate || mouseActivate;
  auto* grid = lutPickerGrid(this);
  const QString name = watched->objectName();
  if (name == QStringLiteral("lutPickerGrid")) {
    if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::KeyRelease) {
      QTimer::singleShot(0, this, [this]() { updateLutPickerDetails(this); });
    }
    if (event->type() == QEvent::MouseButtonDblClick && grid) {
      const QModelIndex index = grid->indexAt(static_cast<QMouseEvent*>(event)->position().toPoint());
      if (index.isValid()) { grid->setCurrentIndex(index); updateLutPickerDetails(this); accept(); return true; }
    }
  } else if (name == QStringLiteral("lutPickerLibrary") && mouseActivate && grid) {
    auto* list = static_cast<QListWidget*>(watched);
    auto* item = list->itemAt(static_cast<QMouseEvent*>(event)->position().toPoint());
    if (!item) return QDialog::eventFilter(watched, event);
    const QString category = item->data(Qt::UserRole).toString();
    for (int row = 0; row < grid->count(); ++row) {
      auto* lut = grid->item(row);
      const QString type = lut->data(Qt::UserRole + 1).toString();
      lut->setHidden(category == QStringLiteral("Built-in") ? type != category :
                     category == QStringLiteral("Files") ? type != category : false);
    }
    updateLutPickerDetails(this);
  } else if (name == QStringLiteral("lutPickerSearch") && event->type() == QEvent::KeyRelease && grid) {
    const QString query = static_cast<QLineEdit*>(watched)->text().trimmed();
    for (int row = 0; row < grid->count(); ++row) {
      auto* item = grid->item(row);
      item->setHidden(!query.isEmpty() && !item->text().contains(query, Qt::CaseInsensitive));
    }
    updateLutPickerDetails(this);
  } else if (name == QStringLiteral("lutPickerImport") && activate) {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Import LUT"), QString(),
        QStringLiteral("LUT files (*.cube *.3dl *.lut);;All files (*.*)"));
    if (!path.isEmpty()) { setProperty("lutPickerImportedSource", path); accept(); }
    return true;
  } else if (name == QStringLiteral("lutPickerCancel") && activate) {
    reject(); return true;
  } else if (name == QStringLiteral("lutPickerUse") && activate) {
    if (!selectedSource().isEmpty()) accept();
    return true;
  }
  return QDialog::eventFilter(watched, event);
}

namespace {

class ScopeDashboard final : public QWidget {
public:
  enum class ViewMode {
    Grid,
    Dual,
    Colorist,
    Parade,
    Vectorscope,
    Waveform,
    Histogram
  };

  explicit ScopeDashboard(QWidget *parent = nullptr)
      : QWidget(parent), grid_(new QGridLayout(this)) {
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(6);
    grid_->setVerticalSpacing(6);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    setAccessibleName(QStringLiteral("Scopes dashboard"));
    setAccessibleDescription(QStringLiteral(
        "RGB parade, vectorscope, luma waveform, and histogram dashboard"));
  }

  void setScopes(ArtifactWidgets::ParadeScopeWidget *parade,
                 ArtifactWidgets::VectorScopeWidget *vectorscope,
                 ArtifactWidgets::WaveformScopeWidget *waveform,
                 ArtifactWidgets::HistogramWidget *histogram) {
    paradeScope_ = parade;
    vectorscope_ = vectorscope;
    waveform_ = waveform;
    histogram_ = histogram;
    paradeTile_ = makeTile(QStringLiteral("PARADE"), parade);
    vectorscopeTile_ = makeTile(QStringLiteral("VECTORSCOPE"), vectorscope);
    waveformTile_ = makeTile(QStringLiteral("WAVEFORM"), waveform);
    histogramTile_ = makeTile(QStringLiteral("HISTOGRAM"), histogram);
    restorePreferences();
    applyViewMode(viewMode_);
  }

  void setRefreshTimer(QTimer *timer) { refreshTimer_ = timer; }
  bool frozen() const { return frozen_; }
  int refreshIntervalMs() const { return refreshIntervalMs_; }
  ScopeSignalRange qcSignalRange() const {
    return videoLegalQc_ ? ScopeSignalRange::VideoLegal
                         : ScopeSignalRange::Full;
  }
  quint64 revision() const { return preferenceRevision_; }
  bool showsParade() const { return paradeTile_ && paradeTile_->isVisible(); }
  bool showsVectorscope() const {
    return vectorscopeTile_ && vectorscopeTile_->isVisible();
  }
  bool showsWaveform() const {
    return waveformTile_ && waveformTile_->isVisible();
  }
  bool showsHistogram() const {
    return histogramTile_ && histogramTile_->isVisible();
  }

protected:
  void contextMenuEvent(QContextMenuEvent *event) override {
    QMenu menu(this);
    QMenu *layoutMenu = menu.addMenu(QStringLiteral("Layout"));
    QAction *gridAction = layoutMenu->addAction(QStringLiteral("Quad 2 x 2"));
    QAction *dualAction = layoutMenu->addAction(QStringLiteral("Dual: Waveform + Vectorscope"));
    QAction *coloristAction = layoutMenu->addAction(QStringLiteral("Colorist: Parade + Vectorscope + Histogram"));
    layoutMenu->addSeparator();
    QAction *paradeAction = layoutMenu->addAction(QStringLiteral("RGB Parade"));
    QAction *vectorscopeAction = layoutMenu->addAction(QStringLiteral("Vectorscope"));
    QAction *waveformAction = layoutMenu->addAction(QStringLiteral("Waveform"));
    QAction *histogramAction = layoutMenu->addAction(QStringLiteral("Histogram"));

    auto markLayout = [this](QAction *action, ViewMode mode) {
      action->setCheckable(true);
      action->setChecked(viewMode_ == mode);
    };
    markLayout(gridAction, ViewMode::Grid);
    markLayout(dualAction, ViewMode::Dual);
    markLayout(coloristAction, ViewMode::Colorist);
    markLayout(paradeAction, ViewMode::Parade);
    markLayout(vectorscopeAction, ViewMode::Vectorscope);
    markLayout(waveformAction, ViewMode::Waveform);
    markLayout(histogramAction, ViewMode::Histogram);

    QMenu *waveformMenu = menu.addMenu(QStringLiteral("Waveform Mode"));
    QAction *waveformLuma = waveformMenu->addAction(QStringLiteral("Luma"));
    QAction *waveformRgb = waveformMenu->addAction(QStringLiteral("RGB Overlay"));
    QAction *waveformYCbCr = waveformMenu->addAction(QStringLiteral("YCbCr"));
    if (waveform_) {
      waveformLuma->setCheckable(true);
      waveformRgb->setCheckable(true);
      waveformYCbCr->setCheckable(true);
      waveformLuma->setChecked(waveform_->mode() == ArtifactWidgets::WaveformMode::Luma);
      waveformRgb->setChecked(waveform_->mode() == ArtifactWidgets::WaveformMode::RGB);
      waveformYCbCr->setChecked(waveform_->mode() == ArtifactWidgets::WaveformMode::YCbCr);
    }

    QMenu *vectorscopeMenu = menu.addMenu(QStringLiteral("Vectorscope Mode"));
    QAction *vectorStandard = vectorscopeMenu->addAction(QStringLiteral("Standard"));
    QAction *vectorHls = vectorscopeMenu->addAction(QStringLiteral("HLS"));
    QAction *vectorSkin = vectorscopeMenu->addAction(QStringLiteral("Skin Tone Indicator"));
    if (vectorscope_) {
      vectorStandard->setCheckable(true);
      vectorHls->setCheckable(true);
      vectorSkin->setCheckable(true);
      vectorStandard->setChecked(vectorscope_->mode() == ArtifactWidgets::VectorScopeMode::Standard);
      vectorHls->setChecked(vectorscope_->mode() == ArtifactWidgets::VectorScopeMode::HLS);
      vectorSkin->setChecked(vectorscope_->mode() == ArtifactWidgets::VectorScopeMode::Skin);
    }

    QMenu *paradeMenu = menu.addMenu(QStringLiteral("Parade Mode"));
    QAction *paradeRgb = paradeMenu->addAction(QStringLiteral("RGB"));
    QAction *paradeYCbCr = paradeMenu->addAction(QStringLiteral("YCbCr"));
    QAction *paradeYRgb = paradeMenu->addAction(QStringLiteral("YRGB"));
    if (paradeScope_) {
      paradeRgb->setCheckable(true);
      paradeYCbCr->setCheckable(true);
      paradeYRgb->setCheckable(true);
      paradeRgb->setChecked(paradeScope_->mode() == ArtifactWidgets::ParadeMode::RGB);
      paradeYCbCr->setChecked(paradeScope_->mode() == ArtifactWidgets::ParadeMode::YCbCr);
      paradeYRgb->setChecked(paradeScope_->mode() == ArtifactWidgets::ParadeMode::YRGB);
    }

    QMenu *histogramMenu = menu.addMenu(QStringLiteral("Histogram Mode"));
    QAction *histCombined = histogramMenu->addAction(QStringLiteral("Combined"));
    QAction *histLuma = histogramMenu->addAction(QStringLiteral("Luma"));
    QAction *histRgb = histogramMenu->addAction(QStringLiteral("RGB Overlay"));
    QAction *histParade = histogramMenu->addAction(QStringLiteral("RGB Parade"));
    QAction *histLog = histogramMenu->addAction(QStringLiteral("Log Scale"));
    if (histogram_) {
      for (QAction *action : {histCombined, histLuma, histRgb, histParade, histLog}) {
        action->setCheckable(true);
      }
      histCombined->setChecked(histogram_->mode() == ArtifactWidgets::HistogramMode::Combined);
      histLuma->setChecked(histogram_->mode() == ArtifactWidgets::HistogramMode::Luma);
      histRgb->setChecked(histogram_->mode() == ArtifactWidgets::HistogramMode::RGB);
      histParade->setChecked(histogram_->mode() == ArtifactWidgets::HistogramMode::Parade);
      histLog->setChecked(histogram_->logScale());
    }

    QMenu *intensityMenu = menu.addMenu(QStringLiteral("Trace Intensity"));
    QAction *intensityLow = intensityMenu->addAction(QStringLiteral("Low (50%)"));
    QAction *intensityMedium = intensityMenu->addAction(QStringLiteral("Medium (75%)"));
    QAction *intensityHigh = intensityMenu->addAction(QStringLiteral("High (100%)"));
    for (QAction *action : {intensityLow, intensityMedium, intensityHigh}) {
      action->setCheckable(true);
    }
    intensityLow->setChecked(traceIntensity_ == 50);
    intensityMedium->setChecked(traceIntensity_ == 75);
    intensityHigh->setChecked(traceIntensity_ == 100);

    QMenu *rateMenu = menu.addMenu(QStringLiteral("Refresh Rate"));
    QAction *rate10 = rateMenu->addAction(QStringLiteral("10 fps"));
    QAction *rate5 = rateMenu->addAction(QStringLiteral("5 fps"));
    QAction *rate2 = rateMenu->addAction(QStringLiteral("2 fps"));
    QAction *rate1 = rateMenu->addAction(QStringLiteral("1 fps"));
    for (QAction *action : {rate10, rate5, rate2, rate1}) {
      action->setCheckable(true);
    }
    rate10->setChecked(refreshIntervalMs_ == 100);
    rate5->setChecked(refreshIntervalMs_ == 200);
    rate2->setChecked(refreshIntervalMs_ == 500);
    rate1->setChecked(refreshIntervalMs_ == 1000);

    QMenu *qcRangeMenu = menu.addMenu(QStringLiteral("QC Signal Range"));
    QAction *qcFull = qcRangeMenu->addAction(QStringLiteral("Full Range"));
    QAction *qcLegal = qcRangeMenu->addAction(QStringLiteral("Video Legal (8-bit)"));
    qcFull->setCheckable(true);
    qcLegal->setCheckable(true);
    qcFull->setChecked(!videoLegalQc_);
    qcLegal->setChecked(videoLegalQc_);

    menu.addSeparator();
    QAction *freezeAction = menu.addAction(QStringLiteral("Freeze Scopes"));
    freezeAction->setCheckable(true);
    freezeAction->setChecked(frozen_);

    QAction *selected = menu.exec(event->globalPos());
    if (selected == gridAction) {
      applyViewMode(ViewMode::Grid);
    } else if (selected == dualAction) {
      applyViewMode(ViewMode::Dual);
    } else if (selected == coloristAction) {
      applyViewMode(ViewMode::Colorist);
    } else if (selected == paradeAction) {
      applyViewMode(ViewMode::Parade);
    } else if (selected == vectorscopeAction) {
      applyViewMode(ViewMode::Vectorscope);
    } else if (selected == waveformAction) {
      applyViewMode(ViewMode::Waveform);
    } else if (selected == histogramAction) {
      applyViewMode(ViewMode::Histogram);
    } else if (selected == waveformLuma && waveform_) {
      waveform_->setMode(ArtifactWidgets::WaveformMode::Luma);
    } else if (selected == waveformRgb && waveform_) {
      waveform_->setMode(ArtifactWidgets::WaveformMode::RGB);
    } else if (selected == waveformYCbCr && waveform_) {
      waveform_->setMode(ArtifactWidgets::WaveformMode::YCbCr);
    } else if (selected == vectorStandard && vectorscope_) {
      vectorscope_->setMode(ArtifactWidgets::VectorScopeMode::Standard);
    } else if (selected == vectorHls && vectorscope_) {
      vectorscope_->setMode(ArtifactWidgets::VectorScopeMode::HLS);
    } else if (selected == vectorSkin && vectorscope_) {
      vectorscope_->setMode(ArtifactWidgets::VectorScopeMode::Skin);
    } else if (selected == paradeRgb && paradeScope_) {
      paradeScope_->setMode(ArtifactWidgets::ParadeMode::RGB);
    } else if (selected == paradeYCbCr && paradeScope_) {
      paradeScope_->setMode(ArtifactWidgets::ParadeMode::YCbCr);
    } else if (selected == paradeYRgb && paradeScope_) {
      paradeScope_->setMode(ArtifactWidgets::ParadeMode::YRGB);
    } else if (selected == histCombined && histogram_) {
      histogram_->setMode(ArtifactWidgets::HistogramMode::Combined);
    } else if (selected == histLuma && histogram_) {
      histogram_->setMode(ArtifactWidgets::HistogramMode::Luma);
    } else if (selected == histRgb && histogram_) {
      histogram_->setMode(ArtifactWidgets::HistogramMode::RGB);
    } else if (selected == histParade && histogram_) {
      histogram_->setMode(ArtifactWidgets::HistogramMode::Parade);
    } else if (selected == histLog && histogram_) {
      histogram_->setLogScale(histLog->isChecked());
    } else if (selected == intensityLow) {
      applyTraceIntensity(50);
    } else if (selected == intensityMedium) {
      applyTraceIntensity(75);
    } else if (selected == intensityHigh) {
      applyTraceIntensity(100);
    } else if (selected == rate10) {
      applyRefreshInterval(100);
    } else if (selected == rate5) {
      applyRefreshInterval(200);
    } else if (selected == rate2) {
      applyRefreshInterval(500);
    } else if (selected == rate1) {
      applyRefreshInterval(1000);
    } else if (selected == qcFull) {
      videoLegalQc_ = false;
    } else if (selected == qcLegal) {
      videoLegalQc_ = true;
    } else if (selected == freezeAction) {
      frozen_ = freezeAction->isChecked();
    }
    if (selected) ++preferenceRevision_;
    savePreferences();
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    QWidget *widget = qobject_cast<QWidget *>(watched);
    if (event && event->type() == QEvent::ContextMenu) {
      contextMenuEvent(static_cast<QContextMenuEvent *>(event));
      return true;
    }
    if (event && event->type() == QEvent::MouseButtonDblClick) {
      QWidget *tile = widget;
      if (paradeTile_ && paradeTile_->isAncestorOf(widget)) tile = paradeTile_;
      else if (vectorscopeTile_ && vectorscopeTile_->isAncestorOf(widget)) tile = vectorscopeTile_;
      else if (waveformTile_ && waveformTile_->isAncestorOf(widget)) tile = waveformTile_;
      else if (histogramTile_ && histogramTile_->isAncestorOf(widget)) tile = histogramTile_;
      if (viewMode_ == ViewMode::Grid) {
        if (tile == paradeTile_) applyViewMode(ViewMode::Parade);
        else if (tile == vectorscopeTile_) applyViewMode(ViewMode::Vectorscope);
        else if (tile == waveformTile_) applyViewMode(ViewMode::Waveform);
        else if (tile == histogramTile_) applyViewMode(ViewMode::Histogram);
      } else {
        applyViewMode(ViewMode::Grid);
      }
      savePreferences();
      return true;
    }
    return QWidget::eventFilter(watched, event);
  }

private:
  QFrame *makeTile(const QString &title, QWidget *scope) {
    auto *tile = new QFrame(this);
    tile->setFrameShape(QFrame::StyledPanel);
    tile->setMinimumSize(220, 150);
    tile->setAccessibleName(title);
    tile->installEventFilter(this);

    auto *layout = new QVBoxLayout(tile);
    layout->setContentsMargins(6, 4, 6, 6);
    layout->setSpacing(4);

    auto *label = new QLabel(title, tile);
    label->setAccessibleName(title + QStringLiteral(" title"));
    label->installEventFilter(this);
    layout->addWidget(label);

    if (scope) {
      scope->setParent(tile);
      scope->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      scope->installEventFilter(this);
      layout->addWidget(scope, 1);
    }
    return tile;
  }

  void applyViewMode(ViewMode mode) {
    viewMode_ = mode;
    QWidget *tiles[] = {paradeTile_, vectorscopeTile_, waveformTile_, histogramTile_};
    for (QWidget *tile : tiles) {
      if (!tile) continue;
      grid_->removeWidget(tile);
      tile->hide();
    }

    if (mode == ViewMode::Grid) {
      grid_->addWidget(paradeTile_, 0, 0);
      grid_->addWidget(vectorscopeTile_, 0, 1);
      grid_->addWidget(waveformTile_, 1, 0);
      grid_->addWidget(histogramTile_, 1, 1);
      for (QWidget *tile : tiles) {
        if (tile) tile->show();
      }
      setAccessibleDescription(QStringLiteral(
          "Four-up RGB parade, vectorscope, luma waveform, and histogram dashboard"));
      return;
    }

    if (mode == ViewMode::Dual) {
      grid_->addWidget(waveformTile_, 0, 0);
      grid_->addWidget(vectorscopeTile_, 0, 1);
      waveformTile_->show();
      vectorscopeTile_->show();
      setAccessibleDescription(QStringLiteral("Dual waveform and vectorscope layout"));
      return;
    }

    if (mode == ViewMode::Colorist) {
      grid_->addWidget(paradeTile_, 0, 0, 2, 1);
      grid_->addWidget(vectorscopeTile_, 0, 1);
      grid_->addWidget(histogramTile_, 1, 1);
      paradeTile_->show();
      vectorscopeTile_->show();
      histogramTile_->show();
      setAccessibleDescription(QStringLiteral(
          "Colorist layout with large parade, vectorscope, and histogram"));
      return;
    }

    QWidget *active = nullptr;
    if (mode == ViewMode::Parade) active = paradeTile_;
    else if (mode == ViewMode::Vectorscope) active = vectorscopeTile_;
    else if (mode == ViewMode::Waveform) active = waveformTile_;
    else if (mode == ViewMode::Histogram) active = histogramTile_;

    if (active) {
      grid_->addWidget(active, 0, 0);
      active->show();
      setAccessibleDescription(
          QStringLiteral("Single expanded scope; double-click to return to the 2 x 2 grid"));
    }
  }

  void applyTraceIntensity(int percent) {
    traceIntensity_ = percent;
    const float value = static_cast<float>(percent) / 100.0f;
    if (paradeScope_) paradeScope_->setIntensity(value);
    if (vectorscope_) vectorscope_->setIntensity(value);
    if (waveform_) waveform_->setIntensity(value);
  }

  void applyRefreshInterval(int intervalMs) {
    refreshIntervalMs_ = intervalMs;
    if (refreshTimer_) refreshTimer_->setInterval(intervalMs);
  }

  void restorePreferences() {
    QSettings settings;
    settings.beginGroup(QStringLiteral("ColorScience/Scopes"));
    const auto boundedEnumValue = [&settings](const QString &key,
                                               int fallback,
                                               int minimum,
                                               int maximum) {
      const int value = settings.value(key, fallback).toInt();
      return value >= minimum && value <= maximum ? value : fallback;
    };
    const int savedLayout = settings.value(QStringLiteral("layout"),
        static_cast<int>(ViewMode::Grid)).toInt();
    viewMode_ = savedLayout >= static_cast<int>(ViewMode::Grid) &&
                        savedLayout <= static_cast<int>(ViewMode::Histogram)
                    ? static_cast<ViewMode>(savedLayout)
                    : ViewMode::Grid;
    frozen_ = settings.value(QStringLiteral("frozen"), false).toBool();
    refreshIntervalMs_ = settings.value(QStringLiteral("refreshIntervalMs"), 200).toInt();
    traceIntensity_ = settings.value(QStringLiteral("traceIntensity"), 75).toInt();
    videoLegalQc_ = settings.value(QStringLiteral("videoLegalQc"), false).toBool();
    if (refreshIntervalMs_ != 100 && refreshIntervalMs_ != 200 &&
        refreshIntervalMs_ != 500 && refreshIntervalMs_ != 1000) {
      refreshIntervalMs_ = 200;
    }
    if (traceIntensity_ != 50 && traceIntensity_ != 75 && traceIntensity_ != 100) {
      traceIntensity_ = 75;
    }
    if (waveform_) waveform_->setMode(static_cast<ArtifactWidgets::WaveformMode>(
        boundedEnumValue(QStringLiteral("waveformMode"),
                         static_cast<int>(ArtifactWidgets::WaveformMode::Luma),
                         static_cast<int>(ArtifactWidgets::WaveformMode::Luma),
                         static_cast<int>(ArtifactWidgets::WaveformMode::YCbCr))));
    if (vectorscope_) vectorscope_->setMode(static_cast<ArtifactWidgets::VectorScopeMode>(
        boundedEnumValue(QStringLiteral("vectorscopeMode"),
                         static_cast<int>(ArtifactWidgets::VectorScopeMode::Skin),
                         static_cast<int>(ArtifactWidgets::VectorScopeMode::Standard),
                         static_cast<int>(ArtifactWidgets::VectorScopeMode::Skin))));
    if (paradeScope_) paradeScope_->setMode(static_cast<ArtifactWidgets::ParadeMode>(
        boundedEnumValue(QStringLiteral("paradeMode"),
                         static_cast<int>(ArtifactWidgets::ParadeMode::RGB),
                         static_cast<int>(ArtifactWidgets::ParadeMode::RGB),
                         static_cast<int>(ArtifactWidgets::ParadeMode::YRGB))));
    if (histogram_) {
      histogram_->setMode(static_cast<ArtifactWidgets::HistogramMode>(
          boundedEnumValue(QStringLiteral("histogramMode"),
                           static_cast<int>(ArtifactWidgets::HistogramMode::Combined),
                           static_cast<int>(ArtifactWidgets::HistogramMode::Luma),
                           static_cast<int>(ArtifactWidgets::HistogramMode::Combined))));
      histogram_->setLogScale(settings.value(QStringLiteral("histogramLog"), true).toBool());
    }
    settings.endGroup();
    applyTraceIntensity(traceIntensity_);
    applyRefreshInterval(refreshIntervalMs_);
  }

  void savePreferences() const {
    QSettings settings;
    settings.beginGroup(QStringLiteral("ColorScience/Scopes"));
    settings.setValue(QStringLiteral("layout"), static_cast<int>(viewMode_));
    settings.setValue(QStringLiteral("frozen"), frozen_);
    settings.setValue(QStringLiteral("refreshIntervalMs"), refreshIntervalMs_);
    settings.setValue(QStringLiteral("traceIntensity"), traceIntensity_);
    settings.setValue(QStringLiteral("videoLegalQc"), videoLegalQc_);
    if (waveform_) settings.setValue(QStringLiteral("waveformMode"), static_cast<int>(waveform_->mode()));
    if (vectorscope_) settings.setValue(QStringLiteral("vectorscopeMode"), static_cast<int>(vectorscope_->mode()));
    if (paradeScope_) settings.setValue(QStringLiteral("paradeMode"), static_cast<int>(paradeScope_->mode()));
    if (histogram_) {
      settings.setValue(QStringLiteral("histogramMode"), static_cast<int>(histogram_->mode()));
      settings.setValue(QStringLiteral("histogramLog"), histogram_->logScale());
    }
    settings.endGroup();
  }

  QGridLayout *grid_ = nullptr;
  QFrame *paradeTile_ = nullptr;
  QFrame *vectorscopeTile_ = nullptr;
  QFrame *waveformTile_ = nullptr;
  QFrame *histogramTile_ = nullptr;
  ArtifactWidgets::ParadeScopeWidget *paradeScope_ = nullptr;
  ArtifactWidgets::VectorScopeWidget *vectorscope_ = nullptr;
  ArtifactWidgets::WaveformScopeWidget *waveform_ = nullptr;
  ArtifactWidgets::HistogramWidget *histogram_ = nullptr;
  QTimer *refreshTimer_ = nullptr;
  ViewMode viewMode_ = ViewMode::Grid;
  bool frozen_ = false;
  int refreshIntervalMs_ = 200;
  int traceIntensity_ = 75;
  bool videoLegalQc_ = false;
  quint64 preferenceRevision_ = 0;
};

} // namespace

class ArtifactColorSciencePanel::Impl {
public:
  ArtifactColorSciencePanel *owner_ = nullptr;
  struct ColorRuleRow {
    QString target;
    QString op;
    double value = 0.0;
    QString scope;
    bool enforce = true;
  };

  struct LutEntry {
    QString displayName;
    QString source;
    bool builtin = false;
  };

  ArtifactColorScienceManager *manager_ = nullptr;
  ArtifactHDRMonitor *scopeAnalyzer_ = nullptr;
  ArtifactCore::ArtifactArray<ArtifactCore::FloatColor> scopeAnalysisSamples_;
  ArtifactCore::SharedPtr<ArtifactCore::Color::ColorPaletteManager> paletteManager_;

  // UI elements
  QComboBox *inputSpaceCombo_ = nullptr;
  QComboBox *workingSpaceCombo_ = nullptr;
  QComboBox *outputSpaceCombo_ = nullptr;
  // OCIO controls
  QGroupBox *ocioGroup_ = nullptr;
  QComboBox *ocioPresetCombo_ = nullptr;
  QComboBox *ocioDisplayCombo_ = nullptr;
  QComboBox *ocioViewCombo_ = nullptr;
  QLabel *ocioStatusLabel_ = nullptr;
  QPushButton *loadConfigBtn_ = nullptr;
  QLineEdit *lutFilterEdit_ = nullptr;
  QListWidget *lutList_ = nullptr;
  QLabel *lutPreviewLabel_ = nullptr;
  QLabel *lutDetailsLabel_ = nullptr;
  QSlider *lutIntensitySlider_ = nullptr;
  QLabel *lutIntensityLabel_ = nullptr;
  QPushButton *loadLUTButton_ = nullptr;
  QPushButton *applySelectedButton_ = nullptr;
  QPushButton *clearLUTButton_ = nullptr;
  QPushButton *reloadLUTButton_ = nullptr;
  QPushButton *openLUTFolderButton_ = nullptr;
  QCheckBox *hdrCheckBox_ = nullptr;
  QTableWidget *ruleTable_ = nullptr;
  QPushButton *addRuleButton_ = nullptr;
  QPushButton *removeRuleButton_ = nullptr;
  QPushButton *snapToPaletteButton_ = nullptr;
  QLineEdit *snapColorEdit_ = nullptr;
  QLabel *snapResultLabel_ = nullptr;
  ScopeDashboard *scopeDashboard_ = nullptr;
  QLabel *scopeStatusLabel_ = nullptr;
  QLabel *scopeQcLabel_ = nullptr;
  ArtifactWidgets::HistogramWidget *histogramWidget_ = nullptr;
  ArtifactWidgets::VectorScopeWidget *vectorScopeWidget_ = nullptr;
  ArtifactWidgets::WaveformScopeWidget *waveformScopeWidget_ = nullptr;
  ArtifactWidgets::ParadeScopeWidget *paradeScopeWidget_ = nullptr;
  QTimer *scopeRefreshTimer_ = nullptr;
  QPointer<ArtifactCompositionEditor> lastScopeEditor_;
  QPointer<ArtifactCompositionEditor> scopeRequestEditor_;
  quint64 lastScopeFrameSerial_ = 0;
  quint64 lastScopePreferenceRevision_ = 0;
  quint64 scopeRequestGeneration_ = 0;
  quint64 scopeAcceptedRequests_ = 0;
  quint64 scopeDeferredRequests_ = 0;
  bool scopeReadbackPending_ = false;
  QElapsedTimer scopeReadbackElapsed_;

  std::vector<LutEntry> lutEntries_;
  std::vector<ColorRuleRow> colorRules_;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;

  void setupUI(QWidget *parent);
  void updateUI();
  void connectSignals();
  void refreshLUTBrowser();
  void updateSelectedLUTPreview();
  void setupColorRulesSection(QWidget *parent, QVBoxLayout *layout);
  void setupScopesSection(QWidget *parent, QVBoxLayout *layout);
  void refreshColorRuleTable();
  void syncColorRulesFromTable();
  QColor nearestPaletteColor(const QColor &input) const;
  ArtifactCore::ColorLUT lutForSource(const QString &source) const;
  QPixmap buildPreviewPixmap(const ArtifactCore::ColorLUT &lut,
                             const QString &title) const;
  QString lutDescriptionForSource(const QString &source) const;
  QString defaultLUTDirectory() const;
  void refreshScopesFromViewport(QWidget *parent);
  void applyScopeFrame(const QImage &frame, quint64 frameSerial,
                       quint64 requestGeneration,
                       ArtifactCompositionEditor *sourceEditor);
  void updateScopeQcSummary(const QImage &frame);
};

static ArtifactCompositionEditor *findActiveCompositionEditor(QWidget *origin) {
  QWidget *window = origin ? origin->window() : nullptr;
  if (!window) {
    return nullptr;
  }

  const auto editors = window->findChildren<ArtifactCompositionEditor *>();
  if (editors.isEmpty()) {
    return nullptr;
  }

  QWidget *focus = QApplication::focusWidget();
  for (ArtifactCompositionEditor *editor : editors) {
    if (!editor || !editor->isVisible()) {
      continue;
    }
    if (focus && (editor == focus || editor->isAncestorOf(focus))) {
      return editor;
    }
  }

  for (ArtifactCompositionEditor *editor : editors) {
    if (editor && editor->isVisible()) {
      return editor;
    }
  }

  return editors.front();
}

ArtifactColorSciencePanel::ArtifactColorSciencePanel(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  impl_->owner_ = this;
  setAccessibleName(QStringLiteral("Color science panel"));
  setAccessibleDescription(QStringLiteral("Configure color spaces, LUTs, OCIO color management, HDR, and color constraints"));
  impl_->manager_ = new ArtifactColorScienceManager();
  impl_->scopeAnalyzer_ = new ArtifactHDRMonitor();
  impl_->paletteManager_ = ArtifactCore::makeShared<ArtifactCore::Color::ColorPaletteManager>();
  impl_->setupUI(this);
  impl_->connectSignals();
  impl_->updateUI();
}

ArtifactColorSciencePanel::~ArtifactColorSciencePanel() {
  delete impl_->scopeAnalyzer_;
  impl_->scopeAnalyzer_ = nullptr;
  delete impl_->manager_;
  delete impl_;
}

void ArtifactColorSciencePanel::Impl::setupUI(QWidget *parent) {
  auto *layout = new QVBoxLayout(parent);

  // Color Space Group
  auto *colorSpaceGroup = new QGroupBox("Color Spaces");
  auto *colorSpaceLayout = new QFormLayout(colorSpaceGroup);

  inputSpaceCombo_ = new QComboBox();
  workingSpaceCombo_ = new QComboBox();
  outputSpaceCombo_ = new QComboBox();
  inputSpaceCombo_->setAccessibleName(QStringLiteral("Input color space"));
  inputSpaceCombo_->setAccessibleDescription(QStringLiteral("Choose the input color space"));
  workingSpaceCombo_->setAccessibleName(QStringLiteral("Working color space"));
  workingSpaceCombo_->setAccessibleDescription(QStringLiteral("Choose the working color space"));
  outputSpaceCombo_->setAccessibleName(QStringLiteral("Output color space"));
  outputSpaceCombo_->setAccessibleDescription(QStringLiteral("Choose the output color space"));

  colorSpaceLayout->addRow("Input:", inputSpaceCombo_);
  colorSpaceLayout->addRow("Working:", workingSpaceCombo_);
  colorSpaceLayout->addRow("Output:", outputSpaceCombo_);

  layout->addWidget(colorSpaceGroup);

  // LUT Browser Group
  auto *lutGroup = new QGroupBox("LUT Browser");
  auto *lutLayout = new QVBoxLayout(lutGroup);

  auto *browserHeaderLayout = new QHBoxLayout();
  lutFilterEdit_ = new QLineEdit();
  lutFilterEdit_->setAccessibleName(QStringLiteral("LUT search"));
  lutFilterEdit_->setAccessibleDescription(QStringLiteral("Filter the available LUTs by name"));
  lutFilterEdit_->setPlaceholderText("Search LUTs...");
  reloadLUTButton_ = new QPushButton("Rescan");
  reloadLUTButton_->setAccessibleName(QStringLiteral("Rescan LUTs"));
  reloadLUTButton_->setAccessibleDescription(QStringLiteral("Rescan the LUT folder"));
  openLUTFolderButton_ = new QPushButton("Open Folder");
  openLUTFolderButton_->setAccessibleName(QStringLiteral("Open LUT folder"));
  openLUTFolderButton_->setAccessibleDescription(QStringLiteral("Open the folder containing LUT files"));
  browserHeaderLayout->addWidget(lutFilterEdit_, 1);
  browserHeaderLayout->addWidget(reloadLUTButton_);
  browserHeaderLayout->addWidget(openLUTFolderButton_);
  lutLayout->addLayout(browserHeaderLayout);

  lutList_ = new QListWidget();
  lutList_->setAccessibleName(QStringLiteral("Available LUTs"));
  lutList_->setAccessibleDescription(QStringLiteral("Select a LUT to preview or apply"));
  lutList_->setSelectionMode(QAbstractItemView::SingleSelection);
  lutLayout->addWidget(lutList_, 1);

  lutPreviewLabel_ = new QLabel();
  lutPreviewLabel_->setAccessibleName(QStringLiteral("LUT preview"));
  lutPreviewLabel_->setAccessibleDescription(QStringLiteral("Preview of the selected LUT"));
  lutPreviewLabel_->setMinimumHeight(120);
  lutPreviewLabel_->setAlignment(Qt::AlignCenter);
  lutPreviewLabel_->setFrameShape(QFrame::StyledPanel);
  lutPreviewLabel_->setText("Select a LUT to preview");
  lutLayout->addWidget(lutPreviewLabel_);

  lutDetailsLabel_ = new QLabel();
  lutDetailsLabel_->setAccessibleName(QStringLiteral("LUT details"));
  lutDetailsLabel_->setWordWrap(true);
  lutLayout->addWidget(lutDetailsLabel_);

  auto *lutControlsLayout = new QHBoxLayout();
  lutIntensitySlider_ = new QSlider(Qt::Horizontal);
  lutIntensitySlider_->setAccessibleName(QStringLiteral("LUT intensity"));
  lutIntensitySlider_->setAccessibleDescription(QStringLiteral("Set the strength of the selected LUT"));
  lutIntensitySlider_->setRange(0, 100);
  lutIntensitySlider_->setValue(100);
  lutIntensityLabel_ = new QLabel("100%");

  lutControlsLayout->addWidget(new QLabel("Intensity:"));
  lutControlsLayout->addWidget(lutIntensitySlider_);
  lutControlsLayout->addWidget(lutIntensityLabel_);

  auto *lutButtonsLayout = new QHBoxLayout();
  loadLUTButton_ = new QPushButton("Load LUT...");
  applySelectedButton_ = new QPushButton("Apply Selected");
  clearLUTButton_ = new QPushButton("Clear");
  loadLUTButton_->setAccessibleName(QStringLiteral("Load LUT"));
  applySelectedButton_->setAccessibleName(QStringLiteral("Apply selected LUT"));
  clearLUTButton_->setAccessibleName(QStringLiteral("Clear selected LUT"));

  lutButtonsLayout->addWidget(loadLUTButton_);
  lutButtonsLayout->addWidget(applySelectedButton_);
  lutButtonsLayout->addWidget(clearLUTButton_);

  lutLayout->addLayout(lutControlsLayout);
  lutLayout->addLayout(lutButtonsLayout);

  layout->addWidget(lutGroup);

  // OCIO Config Group
  auto *ocioGroup = new QGroupBox("OCIO Color Management");
  ocioGroup_ = ocioGroup;
  auto *ocioLayout = new QFormLayout(ocioGroup);

  ocioPresetCombo_ = new QComboBox();
  ocioPresetCombo_->setAccessibleName(QStringLiteral("OCIO configuration preset"));
  ocioLayout->addRow("Config Preset:", ocioPresetCombo_);

  ocioDisplayCombo_ = new QComboBox();
  ocioDisplayCombo_->setAccessibleName(QStringLiteral("OCIO display"));
  ocioLayout->addRow("Display:", ocioDisplayCombo_);

  ocioViewCombo_ = new QComboBox();
  ocioViewCombo_->setAccessibleName(QStringLiteral("OCIO view"));
  ocioLayout->addRow("View:", ocioViewCombo_);

  loadConfigBtn_ = new QPushButton("Load OCIO Config...");
  loadConfigBtn_->setAccessibleName(QStringLiteral("Load OCIO configuration"));
  ocioLayout->addRow("", loadConfigBtn_);

  ocioStatusLabel_ = new QLabel("No OCIO config loaded");
  ocioStatusLabel_->setAccessibleName(QStringLiteral("OCIO status"));
  ocioStatusLabel_->setWordWrap(true);
  ocioLayout->addRow("Status:", ocioStatusLabel_);

  auto *exportLUTBtn = new QPushButton("Export LUT as .cube...");
  exportLUTBtn->setAccessibleName(QStringLiteral("Export LUT"));
  ocioLayout->addRow("", exportLUTBtn);

  QObject::connect(exportLUTBtn, &QPushButton::clicked, [this, parent]() {
    const auto* lut = manager_->currentLUT();
    if (!lut || !lut->isValid()) {
      return;
    }
    const QString path = QFileDialog::getSaveFileName(parent, "Export LUT",
        QString(), "Cube LUT (*.cube);;Discreet 3DL (*.3dl);;All Files (*)");
    if (path.isEmpty()) return;

    ArtifactCore::LUTFormat format = path.endsWith(".3dl", Qt::CaseInsensitive)
        ? ArtifactCore::LUTFormat::_3dl : ArtifactCore::LUTFormat::Cube;

    QString error;
    // Use rawData() + size().dimX from ColorLUT
    if (!ArtifactCore::writeLUTToFile(path, lut->rawData(), lut->size().dimX, format, &error)) {
      ocioStatusLabel_->setText(QStringLiteral("Export failed: %1").arg(error));
    } else {
      ocioStatusLabel_->setText(QStringLiteral("LUT exported to: %1").arg(path));
    }
  });

  layout->addWidget(ocioGroup);

  // HDR Group
  auto *hdrGroup = new QGroupBox("HDR");
  auto *hdrLayout = new QVBoxLayout(hdrGroup);

  hdrCheckBox_ = new QCheckBox("Enable HDR processing");
  hdrCheckBox_->setAccessibleName(QStringLiteral("Enable HDR processing"));
  hdrLayout->addWidget(hdrCheckBox_);

  layout->addWidget(hdrGroup);

  setupScopesSection(parent, layout);
  setupColorRulesSection(parent, layout);

  layout->addStretch();
}

void ArtifactColorSciencePanel::Impl::setupScopesSection(QWidget *parent, QVBoxLayout *layout) {
  auto *scopeGroup = new QGroupBox("Scopes", parent);
  auto *scopeLayout = new QVBoxLayout(scopeGroup);

  auto *scopeHeader = new QLabel(
      "Async live scopes · latest frame wins · right-click for professional controls",
      scopeGroup);
  scopeHeader->setWordWrap(true);
  scopeLayout->addWidget(scopeHeader);

  auto *scopeToolbar = new QHBoxLayout();
  auto *layoutLabel = new QLabel(QStringLiteral("Layout"), scopeGroup);
  auto *layoutHint = new QLabel(
      QStringLiteral("Right-click to choose a scope; double-click to focus or restore"),
      scopeGroup);
  layoutHint->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  scopeToolbar->addWidget(layoutLabel);
  scopeToolbar->addStretch(1);
  scopeToolbar->addWidget(layoutHint);
  scopeLayout->addLayout(scopeToolbar);

  scopeDashboard_ = new ScopeDashboard(scopeGroup);

  histogramWidget_ = new ArtifactWidgets::HistogramWidget(scopeDashboard_);
  histogramWidget_->setMode(ArtifactWidgets::HistogramMode::Combined);
  histogramWidget_->setLogScale(true);

  vectorScopeWidget_ = new ArtifactWidgets::VectorScopeWidget(scopeDashboard_);
  vectorScopeWidget_->setMode(ArtifactWidgets::VectorScopeMode::Skin);
  vectorScopeWidget_->setIntensity(1.1f);

  waveformScopeWidget_ = new ArtifactWidgets::WaveformScopeWidget(scopeDashboard_);
  waveformScopeWidget_->setMode(ArtifactWidgets::WaveformMode::Luma);
  waveformScopeWidget_->setIntensity(1.0f);

  paradeScopeWidget_ = new ArtifactWidgets::ParadeScopeWidget(scopeDashboard_);
  paradeScopeWidget_->setMode(ArtifactWidgets::ParadeMode::RGB);
  paradeScopeWidget_->setIntensity(1.0f);

  scopeDashboard_->setScopes(paradeScopeWidget_, vectorScopeWidget_,
                             waveformScopeWidget_, histogramWidget_);
  scopeLayout->addWidget(scopeDashboard_, 1);

  scopeStatusLabel_ = new QLabel(QStringLiteral("Waiting for Composition Editor preview"), scopeGroup);
  scopeStatusLabel_->setWordWrap(true);
  scopeLayout->addWidget(scopeStatusLabel_);

  scopeQcLabel_ = new QLabel(
      QStringLiteral("QC · waiting for preview RGB samples"), scopeGroup);
  scopeQcLabel_->setWordWrap(true);
  scopeQcLabel_->setAccessibleName(QStringLiteral("Scope quality control summary"));
  scopeLayout->addWidget(scopeQcLabel_);

  layout->addWidget(scopeGroup);

  scopeRefreshTimer_ = new QTimer(parent);
  scopeDashboard_->setRefreshTimer(scopeRefreshTimer_);
  scopeRefreshTimer_->setInterval(scopeDashboard_->refreshIntervalMs());
  QObject::connect(scopeRefreshTimer_, &QTimer::timeout, [this, parent]() {
    refreshScopesFromViewport(parent);
  });
  scopeRefreshTimer_->start();
}

void ArtifactColorSciencePanel::Impl::setupColorRulesSection(QWidget *parent, QVBoxLayout *layout) {
  auto *ruleGroup = new QGroupBox("Color Constraints");
  auto *ruleLayout = new QVBoxLayout(ruleGroup);

  auto *ruleHeader = new QLabel(
      "Build rules with target + operator + value. Rules are kept in-memory for now.");
  ruleHeader->setWordWrap(true);
  ruleLayout->addWidget(ruleHeader);

  ruleTable_ = new QTableWidget(ruleGroup);
  ruleTable_->setAccessibleName(QStringLiteral("Color constraints table"));
  ruleTable_->setAccessibleDescription(QStringLiteral("Edit color constraint rules"));
  ruleTable_->setColumnCount(5);
  ruleTable_->setHorizontalHeaderLabels({"Target", "Operator", "Value", "Scope", "Enforce"});
  ruleTable_->horizontalHeader()->setStretchLastSection(true);
  ruleTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  ruleTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  ruleTable_->setEditTriggers(QAbstractItemView::AllEditTriggers);
  ruleLayout->addWidget(ruleTable_);

  auto *controls = new QHBoxLayout();
  addRuleButton_ = new QPushButton("Add Rule", ruleGroup);
  removeRuleButton_ = new QPushButton("Remove Rule", ruleGroup);
  addRuleButton_->setAccessibleName(QStringLiteral("Add color constraint rule"));
  removeRuleButton_->setAccessibleName(QStringLiteral("Remove color constraint rule"));
  snapColorEdit_ = new QLineEdit(ruleGroup);
  snapColorEdit_->setAccessibleName(QStringLiteral("Palette snap color"));
  snapColorEdit_->setAccessibleDescription(QStringLiteral("Enter a hexadecimal color to snap to the palette"));
  snapColorEdit_->setPlaceholderText("#RRGGBB or #AARRGGBB");
  snapToPaletteButton_ = new QPushButton("Snap To Palette Color", ruleGroup);
  snapToPaletteButton_->setAccessibleName(QStringLiteral("Snap to palette color"));
  snapResultLabel_ = new QLabel("No snap applied", ruleGroup);
  controls->addWidget(addRuleButton_);
  controls->addWidget(removeRuleButton_);
  controls->addWidget(snapColorEdit_, 1);
  controls->addWidget(snapToPaletteButton_);
  ruleLayout->addLayout(controls);
  ruleLayout->addWidget(snapResultLabel_);

  colorRules_.push_back({"main.alpha", "==", 1.0, "background", true});
  colorRules_.push_back({"accent.hue", "==", 180.0, "palette", true});
  refreshColorRuleTable();

  layout->addWidget(ruleGroup);
}

void ArtifactColorSciencePanel::Impl::refreshColorRuleTable() {
  if (!ruleTable_) {
    return;
  }
  ruleTable_->blockSignals(true);
  ruleTable_->setRowCount(static_cast<int>(colorRules_.size()));
  for (int row = 0; row < static_cast<int>(colorRules_.size()); ++row) {
    const auto &rule = colorRules_[row];
    auto *targetItem = new QTableWidgetItem(rule.target);
    auto *opItem = new QTableWidgetItem(rule.op);
    auto *valueItem = new QTableWidgetItem(QString::number(rule.value, 'f', 3));
    auto *scopeItem = new QTableWidgetItem(rule.scope);
    auto *enforceItem = new QTableWidgetItem();
    enforceItem->setCheckState(rule.enforce ? Qt::Checked : Qt::Unchecked);
    ruleTable_->setItem(row, 0, targetItem);
    ruleTable_->setItem(row, 1, opItem);
    ruleTable_->setItem(row, 2, valueItem);
    ruleTable_->setItem(row, 3, scopeItem);
    ruleTable_->setItem(row, 4, enforceItem);
  }
  ruleTable_->blockSignals(false);
}

void ArtifactColorSciencePanel::Impl::syncColorRulesFromTable() {
  if (!ruleTable_) {
    return;
  }
  std::vector<ColorRuleRow> updated;
  updated.reserve(static_cast<size_t>(ruleTable_->rowCount()));
  for (int row = 0; row < ruleTable_->rowCount(); ++row) {
    ColorRuleRow rule;
    if (auto *item = ruleTable_->item(row, 0)) rule.target = item->text().trimmed();
    if (auto *item = ruleTable_->item(row, 1)) rule.op = item->text().trimmed();
    if (auto *item = ruleTable_->item(row, 2)) rule.value = item->text().toDouble();
    if (auto *item = ruleTable_->item(row, 3)) rule.scope = item->text().trimmed();
    if (auto *item = ruleTable_->item(row, 4)) rule.enforce = item->checkState() == Qt::Checked;
    updated.push_back(rule);
  }
  colorRules_ = std::move(updated);
}

QColor ArtifactColorSciencePanel::Impl::nearestPaletteColor(const QColor &input) const {
  if (!paletteManager_) {
    return input;
  }
  const QStringList names = paletteManager_->paletteNames();
  if (names.isEmpty()) {
    return input;
  }

  const ArtifactCore::FloatColor inputColor(input.redF(), input.greenF(), input.blueF(), input.alphaF());

  auto distanceSq = [](const ArtifactCore::FloatColor &a, const ArtifactCore::FloatColor &b) {
    const double dr = static_cast<double>(a.red()) - static_cast<double>(b.red());
    const double dg = static_cast<double>(a.green()) - static_cast<double>(b.green());
    const double db = static_cast<double>(a.blue()) - static_cast<double>(b.blue());
    const double da = static_cast<double>(a.alpha()) - static_cast<double>(b.alpha());
    return dr * dr + dg * dg + db * db + 0.5 * da * da;
  };

  QColor best = input;
  double bestDistance = std::numeric_limits<double>::max();
  for (const auto &name : names) {
    const auto *palette = paletteManager_->getPalette(name);
    if (!palette) {
      continue;
    }
    for (const auto &entry : palette->colors) {
      const double d = distanceSq(inputColor, entry.color);
      if (d < bestDistance) {
        bestDistance = d;
        best = QColor::fromRgbF(entry.color.r(), entry.color.g(), entry.color.b(), entry.color.a());
      }
    }
  }
  return best;
}

static void seedDefaultConstraintPalette(const ArtifactCore::SharedPtr<ArtifactCore::Color::ColorPaletteManager>& manager)
{
  if (!manager || !manager->paletteNames().isEmpty()) {
    return;
  }

  ArtifactCore::Color::ColorPalette palette;
  palette.name = QStringLiteral("Constraint Defaults");
  const auto toFloatColor = [](const QColor &color) {
    return ArtifactCore::FloatColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());
  };
  palette.colors.push_back({QStringLiteral("Main"), toFloatColor(QColor(QStringLiteral("#ff4e5d6c")))});
  palette.colors.push_back({QStringLiteral("Accent"), toFloatColor(QColor(QStringLiteral("#ff6c4e5d")))});
  palette.colors.push_back({QStringLiteral("Background"), toFloatColor(QColor(QStringLiteral("#ff20242a")))});
  palette.colors.push_back({QStringLiteral("Surface"), toFloatColor(QColor(QStringLiteral("#ff2b3038")))});
  palette.colors.push_back({QStringLiteral("Text"), toFloatColor(QColor(QStringLiteral("#ffe3e7ec")))});
  manager->addPalette(palette);
}

void ArtifactColorSciencePanel::Impl::updateUI() {
  if (!manager_)
    return;

  auto settings = manager_->getSettings();

  // Populate color space combos
  auto spaces = manager_->getSupportedColorSpaces();
  QStringList spaceNames;
  for (auto space : spaces) {
    switch (space) {
    case ColorSpace::sRGB:
      spaceNames << "sRGB";
      break;
    case ColorSpace::Rec709:
      spaceNames << "Rec.709";
      break;
    case ColorSpace::Rec2020:
      spaceNames << "Rec.2020";
      break;
    case ColorSpace::P3:
      spaceNames << "DCI-P3";
      break;
    case ColorSpace::ACES_AP0:
      spaceNames << "ACEScg";
      break;
    case ColorSpace::ACES_AP1:
      spaceNames << "ACEScct";
      break;
    default:
      spaceNames << "Custom";
      break;
    }
  }

  inputSpaceCombo_->clear();
  inputSpaceCombo_->addItems(spaceNames);
  inputSpaceCombo_->setCurrentIndex(static_cast<int>(settings.inputSpace));

  workingSpaceCombo_->clear();
  workingSpaceCombo_->addItems(spaceNames);
  workingSpaceCombo_->setCurrentIndex(static_cast<int>(settings.workingSpace));

  outputSpaceCombo_->clear();
  outputSpaceCombo_->addItems(spaceNames);
  outputSpaceCombo_->setCurrentIndex(static_cast<int>(settings.outputSpace));

  // LUT intensity
  int intensityPercent = static_cast<int>(manager_->getLUTIntensity() * 100);
  lutIntensitySlider_->setValue(intensityPercent);
  lutIntensityLabel_->setText(QString("%1%").arg(intensityPercent));

  // HDR
  hdrCheckBox_->setChecked(manager_->isHDREnabled());

  seedDefaultConstraintPalette(paletteManager_);

  refreshLUTBrowser();

  // OCIO section
  auto* ocio = ArtifactOCIOManager::instance();
  if (ocioPresetCombo_) {
    ocioPresetCombo_->blockSignals(true);
    const QString currentPreset = ocio ? ocio->activePresetName() : QString();
    ocioPresetCombo_->clear();
    if (ocio) {
      ocioPresetCombo_->addItems(ocio->availablePresets());
    }
    if (!currentPreset.isEmpty()) {
      ocioPresetCombo_->setCurrentText(currentPreset);
    }
    ocioPresetCombo_->blockSignals(false);
  }
  if (ocioDisplayCombo_ && ocio) {
    ocioDisplayCombo_->blockSignals(true);
    const QString currentDisplay = ocio->display();
    ocioDisplayCombo_->clear();
    ocioDisplayCombo_->addItems(ocio->availableDisplays());
    if (!currentDisplay.isEmpty()) {
      ocioDisplayCombo_->setCurrentText(currentDisplay);
    }
    ocioDisplayCombo_->blockSignals(false);
  }
  if (ocioViewCombo_ && ocio) {
    ocioViewCombo_->blockSignals(true);
    const QString currentView = ocio->view();
    ocioViewCombo_->clear();
    ocioViewCombo_->addItems(ocio->availableViews(ocio->display()));
    if (!currentView.isEmpty()) {
      ocioViewCombo_->setCurrentText(currentView);
    }
    ocioViewCombo_->blockSignals(false);
  }
  if (ocioStatusLabel_) {
    if (ocio && ocio->hasActiveConfig()) {
      ocioStatusLabel_->setText(QString("Active: %1 | Working: %2")
          .arg(ocio->activePresetName(), ocio->workingSpace()));
    } else {
      ocioStatusLabel_->setText("No OCIO config loaded");
    }
  }
}

void ArtifactColorSciencePanel::Impl::connectSignals() {
  if (!manager_)
    return;

  // Color space changes
  connect(inputSpaceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int index) {
            auto settings = manager_->getSettings();
            settings.inputSpace = static_cast<ColorSpace>(index);
            manager_->setSettings(settings);
          });

  connect(workingSpaceCombo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int index) {
            auto settings = manager_->getSettings();
            settings.workingSpace = static_cast<ColorSpace>(index);
            manager_->setSettings(settings);
          });

  connect(outputSpaceCombo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int index) {
            auto settings = manager_->getSettings();
            settings.outputSpace = static_cast<ColorSpace>(index);
            manager_->setSettings(settings);
          });

  connect(lutFilterEdit_, &QLineEdit::textChanged, [this](const QString &) {
    refreshLUTBrowser();
  });

  connect(lutList_, &QListWidget::currentItemChanged, [this](QListWidgetItem *,
                                                           QListWidgetItem *) {
    updateSelectedLUTPreview();
  });

  connect(reloadLUTButton_, &QPushButton::clicked, [this]() {
    refreshLUTBrowser();
  });

  connect(openLUTFolderButton_, &QPushButton::clicked, [this]() {
    const QString directory = defaultLUTDirectory();
    if (!directory.isEmpty()) {
      QDir().mkpath(directory);
      QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
    }
  });

  // LUT controls
  connect(lutIntensitySlider_, &QSlider::valueChanged, [this](int value) {
    manager_->setLUTIntensity(value / 100.0f);
    lutIntensityLabel_->setText(QString("%1%").arg(value));
  });

  connect(loadLUTButton_, &QPushButton::clicked, [this]() {
    ArtifactLutColorReferencePickerDialog picker(manager_, owner_);
    if (picker.exec() != QDialog::Accepted) return;
    const QString source = picker.selectedSource();
    const bool loaded = source.startsWith(QStringLiteral("builtin:"))
        ? manager_->loadBuiltinLUT(source.mid(8).toStdString())
        : manager_->loadLUT(source.toStdString());
    if (loaded) updateUI();
  });

  connect(applySelectedButton_, &QPushButton::clicked, [this]() {
    if (!lutList_) {
      return;
    }
    auto *item = lutList_->currentItem();
    if (!item) {
      return;
    }
    const QString source = item->data(Qt::UserRole).toString();
    if (source.isEmpty()) {
      return;
    }
    if (manager_->loadLUT(source.toStdString())) {
      updateUI();
    }
  });

  connect(clearLUTButton_, &QPushButton::clicked, [this]() {
    manager_->clearLUT();
    updateUI();
  });

  // HDR
  connect(hdrCheckBox_, &QCheckBox::toggled,
          [this](bool checked) { manager_->setHDREnabled(checked); });

  connect(addRuleButton_, &QPushButton::clicked, [this]() {
    colorRules_.push_back({"main.alpha", "==", 1.0, "background", true});
    refreshColorRuleTable();
  });

  connect(removeRuleButton_, &QPushButton::clicked, [this]() {
    if (!ruleTable_) {
      return;
    }
    const int row = ruleTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(colorRules_.size())) {
      return;
    }
    colorRules_.erase(colorRules_.begin() + row);
    refreshColorRuleTable();
  });

  connect(ruleTable_, &QTableWidget::itemChanged, [this](QTableWidgetItem *) {
    syncColorRulesFromTable();
  });

  connect(snapToPaletteButton_, &QPushButton::clicked, [this]() {
    if (!snapColorEdit_ || !snapResultLabel_) {
      return;
    }
    const QColor input(snapColorEdit_->text().trimmed());
    if (!input.isValid()) {
      snapResultLabel_->setText("Invalid color input");
      return;
    }
    const QColor snapped = nearestPaletteColor(input);
    snapResultLabel_->setText(
        QString("Snapped %1 -> %2").arg(input.name(QColor::HexArgb), snapped.name(QColor::HexArgb)));
  });

  // OCIO connections
  connect(ocioPresetCombo_, &QComboBox::currentTextChanged, [this](const QString& preset) {
    if (auto* ocio = ArtifactOCIOManager::instance()) {
      ocio->setActivePreset(preset);
      ocio->syncToColorScienceManager(manager_);
      updateUI();
    }
  });
  connect(ocioDisplayCombo_, &QComboBox::currentTextChanged, [this](const QString& display) {
    if (auto* ocio = ArtifactOCIOManager::instance()) {
      ocio->setDisplay(display);
    }
  });
  connect(ocioViewCombo_, &QComboBox::currentTextChanged, [this](const QString& view) {
    if (auto* ocio = ArtifactOCIOManager::instance()) {
      ocio->setView(view);
    }
  });
  connect(loadConfigBtn_, &QPushButton::clicked, [this]() {
    const QString path = QFileDialog::getOpenFileName(nullptr, "Load OCIO Config",
        QString(), "OCIO Config (*.ocio *.json);;All Files (*)");
    if (!path.isEmpty()) {
      if (auto* ocio = ArtifactOCIOManager::instance()) {
        ocio->loadConfigFile(path);
        ocio->syncToColorScienceManager(manager_);
        updateUI();
      }
    }
  });

  // Listen for OCIO changes through the internal event boundary.
  eventBusSubscriptions_.push_back(
      eventBus_.subscribe<OCIOManagerChangedEvent>(
          [this](const OCIOManagerChangedEvent& event) {
            if (event.kind != OCIOManagerChangeKind::ConfigChanged) return;
            const auto refresh = [this]() {
              if (manager_) updateUI();
            };
            if (!owner_) return;
            if (QThread::currentThread() == owner_->thread()) {
              refresh();
            } else {
              QMetaObject::invokeMethod(owner_, refresh, Qt::QueuedConnection);
            }
          }));
}

void ArtifactColorSciencePanel::Impl::refreshScopesFromViewport(QWidget *parent) {
  if (!parent || !parent->isVisible()) {
    return;
  }

  if (scopeDashboard_ && scopeDashboard_->frozen()) {
    if (scopeStatusLabel_) {
      scopeStatusLabel_->setText(QStringLiteral("Frozen · right-click to resume live scopes"));
    }
    return;
  }

  ArtifactCompositionEditor *editor = findActiveCompositionEditor(parent);
  if (!editor) {
    lastScopeEditor_.clear();
    lastScopeFrameSerial_ = 0;
    if (scopeStatusLabel_) {
      scopeStatusLabel_->setText(QStringLiteral("No visible Composition Editor"));
    }
    return;
  }

  CompositionRenderController *controller = editor->renderController();
  if (!controller) {
    if (scopeStatusLabel_) {
      scopeStatusLabel_->setText(QStringLiteral("Composition Editor has no render controller"));
    }
    return;
  }

  const quint64 frameSerial = controller->currentFrameSerial();
  const quint64 preferenceRevision =
      scopeDashboard_ ? scopeDashboard_->revision() : 0;
  if (editor == lastScopeEditor_ && frameSerial != 0 &&
      frameSerial == lastScopeFrameSerial_ &&
      preferenceRevision == lastScopePreferenceRevision_) {
    if (scopeStatusLabel_) {
      scopeStatusLabel_->setText(
          QStringLiteral("Live · frame %1 · accepted %2 · deferred %3")
              .arg(frameSerial)
              .arg(scopeAcceptedRequests_)
              .arg(scopeDeferredRequests_));
    }
    return;
  }

  if (scopeReadbackPending_ && scopeRequestEditor_ != editor) {
    scopeReadbackPending_ = false;
    scopeRequestEditor_.clear();
    ++scopeRequestGeneration_;
    ++scopeDeferredRequests_;
  }

  if (scopeReadbackPending_) {
    if (scopeReadbackElapsed_.isValid() && scopeReadbackElapsed_.elapsed() >= 2000) {
      scopeReadbackPending_ = false;
      scopeRequestEditor_.clear();
      ++scopeRequestGeneration_;
    } else {
      ++scopeDeferredRequests_;
      return;
    }
    ++scopeDeferredRequests_;
  }

  scopeReadbackPending_ = true;
  scopeRequestEditor_ = editor;
  scopeReadbackElapsed_.restart();
  const quint64 requestGeneration = ++scopeRequestGeneration_;
  QPointer<ArtifactColorSciencePanel> ownerGuard(owner_);
  QPointer<ArtifactCompositionEditor> editorGuard(editor);
  const bool accepted = controller->requestCurrentFrameImageAsync(
      [this, ownerGuard, editorGuard, requestGeneration](QImage frame,
                                                         quint64 completedSerial) mutable {
        if (!ownerGuard) {
          return;
        }
        QMetaObject::invokeMethod(
            ownerGuard.data(),
            [this, ownerGuard, editorGuard, requestGeneration, completedSerial,
             frame = std::move(frame)]() mutable {
              if (!ownerGuard) {
                return;
              }
              applyScopeFrame(frame, completedSerial, requestGeneration,
                              editorGuard.data());
            },
            Qt::QueuedConnection);
      });

  if (!accepted) {
    scopeReadbackPending_ = false;
    scopeRequestEditor_.clear();
    ++scopeDeferredRequests_;
    if (scopeStatusLabel_) {
      scopeStatusLabel_->setText(
          QStringLiteral("Live · readback ring busy · keeping the last scope frame"));
    }
  }
}

void ArtifactColorSciencePanel::Impl::applyScopeFrame(
    const QImage &frame, quint64 frameSerial, quint64 requestGeneration,
    ArtifactCompositionEditor *sourceEditor) {
  if (requestGeneration != scopeRequestGeneration_) {
    return;
  }
  scopeReadbackPending_ = false;
  scopeRequestEditor_.clear();
  if (scopeDashboard_ && scopeDashboard_->frozen()) return;
  if (frame.isNull()) {
    ++scopeDeferredRequests_;
    if (scopeStatusLabel_) {
      scopeStatusLabel_->setText(
          QStringLiteral("Live · frame readback unavailable · keeping the last scopes"));
    }
    return;
  }

  if (!sourceEditor || sourceEditor != findActiveCompositionEditor(owner_)) {
    ++scopeDeferredRequests_;
    return;
  }

  lastScopeFrameSerial_ = frameSerial;
  lastScopePreferenceRevision_ =
      scopeDashboard_ ? scopeDashboard_->revision() : 0;
  lastScopeEditor_ = sourceEditor;
  ++scopeAcceptedRequests_;

  if (histogramWidget_ &&
      (!scopeDashboard_ || scopeDashboard_->showsHistogram())) {
    histogramWidget_->updateFrame(frame);
  }
  if (vectorScopeWidget_ &&
      (!scopeDashboard_ || scopeDashboard_->showsVectorscope())) {
    vectorScopeWidget_->updateFrame(frame);
  }
  if (waveformScopeWidget_ &&
      (!scopeDashboard_ || scopeDashboard_->showsWaveform())) {
    waveformScopeWidget_->updateFrame(frame);
  }
  if (paradeScopeWidget_ &&
      (!scopeDashboard_ || scopeDashboard_->showsParade())) {
    paradeScopeWidget_->updateFrame(frame);
  }

  updateScopeQcSummary(frame);

  if (scopeStatusLabel_) {
    scopeStatusLabel_->setText(
        QStringLiteral("Live · frame %1 · %2 x %3 · accepted %4 · deferred %5")
            .arg(frameSerial)
            .arg(frame.width())
            .arg(frame.height())
            .arg(scopeAcceptedRequests_)
            .arg(scopeDeferredRequests_));
  }
}

void ArtifactColorSciencePanel::Impl::updateScopeQcSummary(const QImage &frame) {
  if (!scopeQcLabel_ || !scopeAnalyzer_ || frame.isNull()) {
    return;
  }

  const qint64 pixelCount = static_cast<qint64>(frame.width()) * frame.height();
  int sampleStep = std::max(
      1, static_cast<int>(std::ceil(std::sqrt(
             static_cast<double>(std::max<qint64>(1, pixelCount)) / 100000.0))));
  const auto sampledExtent = [&sampleStep](int extent) {
    return (static_cast<qint64>(extent) + sampleStep - 1) / sampleStep;
  };
  while (sampledExtent(frame.width()) * sampledExtent(frame.height()) > 100000) {
    ++sampleStep;
  }
  const int sampleWidth = static_cast<int>(sampledExtent(frame.width()));
  const int sampleHeight = static_cast<int>(sampledExtent(frame.height()));
  const int sampleCount = sampleWidth * sampleHeight;
  scopeAnalysisSamples_.resize(static_cast<size_t>(sampleCount));

  int sampleIndex = 0;
  for (int y = 0; y < frame.height(); y += sampleStep) {
    for (int x = 0; x < frame.width(); x += sampleStep) {
      const QColor color = frame.pixelColor(x, y);
      scopeAnalysisSamples_[static_cast<size_t>(sampleIndex++)] =
          ArtifactCore::FloatColor(color.redF(), color.greenF(), color.blueF(),
                                   color.alphaF());
    }
  }

  ScopeAnalysisDescriptor descriptor;
  descriptor.domain = ScopeSignalDomain::DisplayEncoded;
  descriptor.signalRange = scopeDashboard_
      ? scopeDashboard_->qcSignalRange() : ScopeSignalRange::Full;
  descriptor.primaries = ArtifactCore::Gamut::Rec709;
  descriptor.targetGamut = ArtifactCore::Gamut::Rec709;
  descriptor.luminanceStandard = ArtifactCore::LuminanceStandard::Rec709;
  descriptor.transferFunction = ArtifactCore::TransferFunction::sRGB;
  descriptor.referenceWhiteNits = 100.0f;
  descriptor.peakLuminanceNits = 100.0f;
  descriptor.bitDepth = 8;
  descriptor.sampleStep = 1;
  descriptor.maxOutOfGamutSamples = 0;
  descriptor.publishEvent = false;
  const HDRAnalysisResult result = scopeAnalyzer_->analyzeFrame(
      scopeAnalysisSamples_, sampleWidth, sampleHeight, descriptor);
  const double divisor =
      static_cast<double>(std::max(1, result.validSamples));
  scopeQcLabel_->setText(
      QStringLiteral("QC SDR/Rec.709 %1 · Low %2% · High %3% · Range %4% · Gamut %5% · Avg %6 nits · Invalid %7 · sample 1/%8")
          .arg(descriptor.signalRange == ScopeSignalRange::VideoLegal
                   ? QStringLiteral("Legal") : QStringLiteral("Full"))
          .arg(100.0 * static_cast<double>(result.clippedShadows) / divisor,
               0, 'f', 2)
          .arg(100.0 * static_cast<double>(result.clippedHighlights) / divisor,
               0, 'f', 2)
          .arg(100.0 * static_cast<double>(result.broadcastSafeViolations) /
                   divisor,
               0, 'f', 2)
          .arg(100.0 * static_cast<double>(result.outOfGamutSamples) / divisor,
               0, 'f', 2)
          .arg(result.avgLuminanceNits, 0, 'f', 1)
          .arg(result.nonFiniteSamples)
          .arg(sampleStep * sampleStep));
}

ArtifactColorScienceManager *
ArtifactColorSciencePanel::colorScienceManager() const {
  return impl_->manager_;
}

void ArtifactColorSciencePanel::Impl::refreshLUTBrowser() {
  if (!lutList_ || !manager_) {
    return;
  }

  const QString filter = lutFilterEdit_ ? lutFilterEdit_->text().trimmed().toLower() : QString();
  const QString activeSource = QString::fromStdString(manager_->getSettings().lutPath);

  lutEntries_.clear();
  const auto available = manager_->getAvailableLUTs();
  lutEntries_.reserve(static_cast<size_t>(available.size()));
  for (const auto &lut : available) {
    const QString source = QString::fromStdString(lut);
    const bool builtin = source.startsWith(QStringLiteral("builtin:"));
    const QString displayName = builtin
                                    ? QStringLiteral("[Built-in] %1").arg(source.mid(8))
                                    : QFileInfo(source).baseName();
    const QString searchable = (displayName + QStringLiteral(" ") + source).toLower();
    if (!filter.isEmpty() && !searchable.contains(filter)) {
      continue;
    }
    lutEntries_.push_back({displayName, source, builtin});
  }

  lutList_->blockSignals(true);
  lutList_->clear();
  for (const auto &entry : lutEntries_) {
    auto *item = new QListWidgetItem(entry.displayName, lutList_);
    item->setData(Qt::UserRole, entry.source);
    item->setToolTip(entry.source);
    if (entry.source == activeSource) {
      item->setSelected(true);
      lutList_->setCurrentItem(item);
    }
  }
  lutList_->blockSignals(false);

  if (!lutList_->currentItem() && lutList_->count() > 0) {
    lutList_->setCurrentRow(0);
  }

  updateSelectedLUTPreview();
}

ArtifactCore::ColorLUT ArtifactColorSciencePanel::Impl::lutForSource(const QString &source) const {
  if (source.startsWith(QStringLiteral("builtin:"))) {
    const QString name = source.mid(QStringLiteral("builtin:").size());
    return ArtifactCore::LUTManager::instance().getLUT(name);
  }
  return ArtifactCore::ColorLUT(source);
}

QPixmap ArtifactColorSciencePanel::Impl::buildPreviewPixmap(const ArtifactCore::ColorLUT &lut,
                                                            const QString &title) const {
  QPixmap pixmap(480, 150);
  pixmap.fill(QColor(25, 28, 34));

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QColor(235, 235, 240));
  painter.drawText(QRect(12, 10, pixmap.width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter, title);

  const std::array<QColor, 8> samples = {
      QColor::fromRgbF(0.0f, 0.0f, 0.0f),
      QColor::fromRgbF(0.18f, 0.18f, 0.18f),
      QColor::fromRgbF(0.50f, 0.50f, 0.50f),
      QColor::fromRgbF(0.80f, 0.80f, 0.80f),
      QColor::fromRgbF(0.95f, 0.95f, 0.95f),
      QColor::fromRgbF(0.82f, 0.58f, 0.42f),
      QColor::fromRgbF(0.22f, 0.46f, 0.84f),
      QColor::fromRgbF(0.20f, 0.72f, 0.56f),
  };

  const int margin = 12;
  const int top = 36;
  const int cellH = 34;
  const int cellW = (pixmap.width() - margin * 2) / samples.size();

  painter.setPen(QColor(160, 165, 175));
  painter.drawText(QRect(margin, top - 16, 120, 14), Qt::AlignLeft, "Original");
  painter.drawText(QRect(margin, top + cellH - 2, 120, 14), Qt::AlignLeft, "LUT");

  for (int i = 0; i < samples.size(); ++i) {
    const QRect cellRect(margin + i * cellW, top, cellW - 4, cellH);
    const QColor original = samples[i];
    const QColor transformed = lut.isValid() ? lut.apply(original) : original;

    painter.fillRect(cellRect, original);
    painter.setPen(QColor(40, 40, 45, 140));
    painter.drawRect(cellRect.adjusted(0, 0, -1, -1));

    const QRect transformedRect(cellRect.left(), cellRect.bottom() + 6, cellRect.width(), cellRect.height());
    painter.fillRect(transformedRect, transformed);
    painter.setPen(QColor(40, 40, 45, 140));
    painter.drawRect(transformedRect.adjusted(0, 0, -1, -1));
  }

  painter.setPen(QColor(190, 195, 205));
  painter.drawText(QRect(margin, 116, pixmap.width() - margin * 2, 18),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   lut.isValid() ? QStringLiteral("Preview generated from sample swatches")
                                 : QStringLiteral("No LUT loaded"));
  return pixmap;
}

QString ArtifactColorSciencePanel::Impl::lutDescriptionForSource(const QString &source) const {
  if (source.isEmpty()) {
    return QStringLiteral("No LUT selected");
  }

  const ArtifactCore::ColorLUT lut = lutForSource(source);
  if (!lut.isValid()) {
    return QStringLiteral("Failed to load LUT: %1").arg(lut.errorMessage());
  }

  const QString formatName = [lut]() {
    switch (lut.format()) {
    case ArtifactCore::LUTFormat::Cube:
      return QStringLiteral("CUBE");
    case ArtifactCore::LUTFormat::Csp:
      return QStringLiteral("CSP");
    case ArtifactCore::LUTFormat::_3dl:
      return QStringLiteral("3DL");
    case ArtifactCore::LUTFormat::Mga:
      return QStringLiteral("MGA");
    case ArtifactCore::LUTFormat::Look:
      return QStringLiteral("LOOK");
    case ArtifactCore::LUTFormat::PNG:
      return QStringLiteral("PNG / HaldCLUT");
    default:
      return QStringLiteral("Unknown");
    }
  }();

  const auto size = lut.size();
  const QString sourceLabel = source.startsWith(QStringLiteral("builtin:"))
                                  ? QStringLiteral("Built-in LUT: %1").arg(source.mid(8))
                                  : QStringLiteral("File LUT: %1").arg(source);
  return QStringLiteral("%1\nFormat: %2\nSize: %3 x %4 x %5")
      .arg(sourceLabel)
      .arg(formatName)
      .arg(size.dimX)
      .arg(size.dimY)
      .arg(size.dimZ);
}

QString ArtifactColorSciencePanel::Impl::defaultLUTDirectory() const {
  const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (baseDir.isEmpty()) {
    return {};
  }
  return QDir(baseDir).absoluteFilePath(QStringLiteral("LUTs"));
}

void ArtifactColorSciencePanel::Impl::updateSelectedLUTPreview() {
  if (!lutList_ || !lutPreviewLabel_ || !lutDetailsLabel_) {
    return;
  }

  const auto *item = lutList_->currentItem();
  if (!item) {
    lutPreviewLabel_->clear();
    lutPreviewLabel_->setText(QStringLiteral("Select a LUT to preview"));
    lutDetailsLabel_->setText(QStringLiteral("No LUT selected"));
    return;
  }

  const QString source = item->data(Qt::UserRole).toString();
  const ArtifactCore::ColorLUT lut = lutForSource(source);
  if (!lut.isValid()) {
    lutPreviewLabel_->clear();
    lutPreviewLabel_->setText(QStringLiteral("Failed to load LUT"));
    lutDetailsLabel_->setText(lut.errorMessage().isEmpty()
                                  ? QStringLiteral("Unable to load the selected LUT.")
                                  : lut.errorMessage());
    return;
  }

  lutPreviewLabel_->setPixmap(buildPreviewPixmap(lut, item->text()));
  lutDetailsLabel_->setText(lutDescriptionForSource(source));
}

} // namespace Artifact

W_OBJECT_IMPL(Artifact::ArtifactColorSciencePanel)

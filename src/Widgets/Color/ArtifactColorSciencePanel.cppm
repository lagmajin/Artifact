module;
#include <utility>
#include <array>

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QAbstractItemView>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QFrame>
#include <QSettings>
#include <QStandardPaths>
#include <QSlider>
#include <QUrl>
#include <limits>
#include <QVBoxLayout>
#include <wobjectimpl.h>




module Artifact.Widgets.ColorSciencePanel;
import Color.ScienceManager;
import Color.LUT;
import Artifact.Color.Palette;
namespace Artifact {

class ArtifactColorSciencePanel::Impl {
public:
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
  std::shared_ptr<ArtifactCore::Color::ColorPaletteManager> paletteManager_;

  // UI elements
  QComboBox *inputSpaceCombo_ = nullptr;
  QComboBox *workingSpaceCombo_ = nullptr;
  QComboBox *outputSpaceCombo_ = nullptr;
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

  std::vector<LutEntry> lutEntries_;
  std::vector<ColorRuleRow> colorRules_;

  void setupUI(QWidget *parent);
  void updateUI();
  void connectSignals();
  void refreshLUTBrowser();
  void updateSelectedLUTPreview();
  void setupColorRulesSection(QWidget *parent, QVBoxLayout *layout);
  void refreshColorRuleTable();
  void syncColorRulesFromTable();
  QColor nearestPaletteColor(const QColor &input) const;
  void loadColorRules();
  void saveColorRules() const;
  ArtifactCore::ColorLUT lutForSource(const QString &source) const;
  QPixmap buildPreviewPixmap(const ArtifactCore::ColorLUT &lut,
                             const QString &title) const;
  QString lutDescriptionForSource(const QString &source) const;
  QString defaultLUTDirectory() const;
};

ArtifactColorSciencePanel::ArtifactColorSciencePanel(QWidget *parent)
    : QWidget(parent), impl_(new Impl()) {
  impl_->manager_ = new ArtifactColorScienceManager();
  impl_->paletteManager_ = std::make_shared<ArtifactCore::Color::ColorPaletteManager>();
  impl_->setupUI(this);
  impl_->connectSignals();
  impl_->updateUI();
}

ArtifactColorSciencePanel::~ArtifactColorSciencePanel() {
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

  colorSpaceLayout->addRow("Input:", inputSpaceCombo_);
  colorSpaceLayout->addRow("Working:", workingSpaceCombo_);
  colorSpaceLayout->addRow("Output:", outputSpaceCombo_);

  layout->addWidget(colorSpaceGroup);

  // LUT Browser Group
  auto *lutGroup = new QGroupBox("LUT Browser");
  auto *lutLayout = new QVBoxLayout(lutGroup);

  auto *browserHeaderLayout = new QHBoxLayout();
  lutFilterEdit_ = new QLineEdit();
  lutFilterEdit_->setPlaceholderText("Search LUTs...");
  reloadLUTButton_ = new QPushButton("Rescan");
  openLUTFolderButton_ = new QPushButton("Open Folder");
  browserHeaderLayout->addWidget(lutFilterEdit_, 1);
  browserHeaderLayout->addWidget(reloadLUTButton_);
  browserHeaderLayout->addWidget(openLUTFolderButton_);
  lutLayout->addLayout(browserHeaderLayout);

  lutList_ = new QListWidget();
  lutList_->setSelectionMode(QAbstractItemView::SingleSelection);
  lutLayout->addWidget(lutList_, 1);

  lutPreviewLabel_ = new QLabel();
  lutPreviewLabel_->setMinimumHeight(120);
  lutPreviewLabel_->setAlignment(Qt::AlignCenter);
  lutPreviewLabel_->setFrameShape(QFrame::StyledPanel);
  lutPreviewLabel_->setText("Select a LUT to preview");
  lutLayout->addWidget(lutPreviewLabel_);

  lutDetailsLabel_ = new QLabel();
  lutDetailsLabel_->setWordWrap(true);
  lutLayout->addWidget(lutDetailsLabel_);

  auto *lutControlsLayout = new QHBoxLayout();
  lutIntensitySlider_ = new QSlider(Qt::Horizontal);
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

  lutButtonsLayout->addWidget(loadLUTButton_);
  lutButtonsLayout->addWidget(applySelectedButton_);
  lutButtonsLayout->addWidget(clearLUTButton_);

  lutLayout->addLayout(lutControlsLayout);
  lutLayout->addLayout(lutButtonsLayout);

  layout->addWidget(lutGroup);

  // HDR Group
  auto *hdrGroup = new QGroupBox("HDR");
  auto *hdrLayout = new QVBoxLayout(hdrGroup);

  hdrCheckBox_ = new QCheckBox("Enable HDR processing");
  hdrLayout->addWidget(hdrCheckBox_);

  layout->addWidget(hdrGroup);

  setupColorRulesSection(parent, layout);

  layout->addStretch();
}

void ArtifactColorSciencePanel::Impl::setupColorRulesSection(QWidget *parent, QVBoxLayout *layout) {
  auto *ruleGroup = new QGroupBox("Color Constraints");
  auto *ruleLayout = new QVBoxLayout(ruleGroup);

  auto *ruleHeader = new QLabel(
      "Build rules with target + operator + value. Operators and scopes are chooser-based, and rules are saved locally.");
  ruleHeader->setWordWrap(true);
  ruleLayout->addWidget(ruleHeader);

  ruleTable_ = new QTableWidget(ruleGroup);
  ruleTable_->setColumnCount(5);
  ruleTable_->setHorizontalHeaderLabels({"Target", "Operator", "Value", "Scope", "Enforce"});
  ruleTable_->horizontalHeader()->setStretchLastSection(true);
  ruleTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  ruleTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  ruleTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  ruleTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  ruleTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  ruleTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  ruleTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  ruleTable_->setEditTriggers(QAbstractItemView::AllEditTriggers);
  ruleLayout->addWidget(ruleTable_);

  auto *controls = new QHBoxLayout();
  addRuleButton_ = new QPushButton("Add Rule", ruleGroup);
  removeRuleButton_ = new QPushButton("Remove Rule", ruleGroup);
  auto *duplicateRuleButton = new QPushButton("Duplicate Rule", ruleGroup);
  auto *moveUpRuleButton = new QPushButton("Move Up", ruleGroup);
  auto *moveDownRuleButton = new QPushButton("Move Down", ruleGroup);
  auto *toggleAllEnforceButton = new QPushButton("Toggle All Enforce", ruleGroup);
  auto *seedPresetButton = new QPushButton("Seed Preset", ruleGroup);
  auto *seedHuePresetButton = new QPushButton("Seed Hue Preset", ruleGroup);
  auto *resetRulesButton = new QPushButton("Reset Defaults", ruleGroup);
  snapColorEdit_ = new QLineEdit(ruleGroup);
  snapColorEdit_->setPlaceholderText("#RRGGBB or #AARRGGBB");
  snapToPaletteButton_ = new QPushButton("Snap To Palette Color", ruleGroup);
  snapResultLabel_ = new QLabel("No snap applied", ruleGroup);
  controls->addWidget(addRuleButton_);
  controls->addWidget(removeRuleButton_);
  controls->addWidget(duplicateRuleButton);
  controls->addWidget(moveUpRuleButton);
  controls->addWidget(moveDownRuleButton);
  controls->addWidget(toggleAllEnforceButton);
  controls->addWidget(seedPresetButton);
  controls->addWidget(seedHuePresetButton);
  controls->addWidget(resetRulesButton);
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
  const QStringList ops{QStringLiteral("=="), QStringLiteral("!="), QStringLiteral(">"),
                        QStringLiteral("<"), QStringLiteral(">="), QStringLiteral("<=")};
  const QStringList scopes{QStringLiteral("background"), QStringLiteral("palette"),
                           QStringLiteral("surface"), QStringLiteral("text")};
  for (int row = 0; row < static_cast<int>(colorRules_.size()); ++row) {
    const auto &rule = colorRules_[row];
    auto *targetItem = new QTableWidgetItem(rule.target);
    auto *valueItem = new QTableWidgetItem(QString::number(rule.value, 'f', 3));
    auto *enforceItem = new QTableWidgetItem();
    auto *opItem = new QComboBox(ruleTable_);
    opItem->addItems(ops);
    opItem->setCurrentIndex(std::max(0, opItem->findText(rule.op)));
    auto *scopeItem = new QComboBox(ruleTable_);
    scopeItem->addItems(scopes);
    scopeItem->setCurrentIndex(std::max(0, scopeItem->findText(rule.scope)));
    enforceItem->setCheckState(rule.enforce ? Qt::Checked : Qt::Unchecked);
    ruleTable_->setItem(row, 0, targetItem);
    ruleTable_->setCellWidget(row, 1, opItem);
    ruleTable_->setItem(row, 2, valueItem);
    ruleTable_->setCellWidget(row, 3, scopeItem);
    ruleTable_->setItem(row, 4, enforceItem);
    QObject::connect(opItem, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     ruleTable_, [this]() {
                       syncColorRulesFromTable();
                       saveColorRules();
                     });
    QObject::connect(scopeItem, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     ruleTable_, [this]() {
                       syncColorRulesFromTable();
                       saveColorRules();
                     });
  }
  ruleTable_->blockSignals(false);
}

void ArtifactColorSciencePanel::Impl::loadColorRules() {
  QSettings settings;
  const int count = settings.beginReadArray(QStringLiteral("ColorScience/ConstraintRules"));
  std::vector<ColorRuleRow> loaded;
  loaded.reserve(std::max(0, count));
  for (int i = 0; i < count; ++i) {
    settings.setArrayIndex(i);
    ColorRuleRow rule;
    rule.target = settings.value(QStringLiteral("target"), QStringLiteral("main.alpha")).toString();
    rule.op = settings.value(QStringLiteral("op"), QStringLiteral("==")).toString();
    rule.value = settings.value(QStringLiteral("value"), 1.0).toDouble();
    rule.scope = settings.value(QStringLiteral("scope"), QStringLiteral("background")).toString();
    rule.enforce = settings.value(QStringLiteral("enforce"), true).toBool();
    loaded.push_back(rule);
  }
  settings.endArray();
  if (!loaded.empty()) {
    colorRules_ = std::move(loaded);
  }
  refreshColorRuleTable();
}

void ArtifactColorSciencePanel::Impl::saveColorRules() const {
  QSettings settings;
  settings.beginWriteArray(QStringLiteral("ColorScience/ConstraintRules"));
  for (int i = 0; i < static_cast<int>(colorRules_.size()); ++i) {
    settings.setArrayIndex(i);
    const auto &rule = colorRules_[i];
    settings.setValue(QStringLiteral("target"), rule.target);
    settings.setValue(QStringLiteral("op"), rule.op);
    settings.setValue(QStringLiteral("value"), rule.value);
    settings.setValue(QStringLiteral("scope"), rule.scope);
    settings.setValue(QStringLiteral("enforce"), rule.enforce);
  }
  settings.endArray();
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
    if (auto *combo = qobject_cast<QComboBox *>(ruleTable_->cellWidget(row, 1))) {
      rule.op = combo->currentText().trimmed();
    }
    if (auto *item = ruleTable_->item(row, 2)) rule.value = item->text().toDouble();
    if (auto *combo = qobject_cast<QComboBox *>(ruleTable_->cellWidget(row, 3))) {
      rule.scope = combo->currentText().trimmed();
    }
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

  auto distanceSq = [](const QColor &a, const QColor &b) {
    const double dr = a.redF() - b.redF();
    const double dg = a.greenF() - b.greenF();
    const double db = a.blueF() - b.blueF();
    const double da = a.alphaF() - b.alphaF();
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
      const double d = distanceSq(input, entry.color);
      if (d < bestDistance) {
        bestDistance = d;
        best = entry.color;
      }
    }
  }
  return best;
}

static void seedDefaultConstraintPalette(const std::shared_ptr<ArtifactCore::Color::ColorPaletteManager>& manager)
{
  if (!manager || !manager->paletteNames().isEmpty()) {
    return;
  }

  ArtifactCore::Color::ColorPalette palette;
  palette.name = QStringLiteral("Constraint Defaults");
  palette.colors.push_back({QStringLiteral("Main"), QColor(QStringLiteral("#ff4e5d6c"))});
  palette.colors.push_back({QStringLiteral("Accent"), QColor(QStringLiteral("#ff6c4e5d"))});
  palette.colors.push_back({QStringLiteral("Background"), QColor(QStringLiteral("#ff20242a"))});
  palette.colors.push_back({QStringLiteral("Surface"), QColor(QStringLiteral("#ff2b3038"))});
  palette.colors.push_back({QStringLiteral("Text"), QColor(QStringLiteral("#ffe3e7ec"))});
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

  loadColorRules();
  refreshLUTBrowser();
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
    QString fileName = QFileDialog::getOpenFileName(
        nullptr, "Load LUT", QString(),
        "LUT files (*.cube *.3dl *.lut);;All files (*.*)");
    if (!fileName.isEmpty()) {
      if (manager_->loadLUT(fileName.toStdString())) {
        updateUI();
      }
    }
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
    saveColorRules();
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
    saveColorRules();
  });

  connect(duplicateRuleButton, &QPushButton::clicked, [this]() {
    if (!ruleTable_) {
      return;
    }
    const int row = ruleTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(colorRules_.size())) {
      return;
    }
    const auto rule = colorRules_[static_cast<size_t>(row)];
    colorRules_.insert(colorRules_.begin() + row + 1, rule);
    refreshColorRuleTable();
    if (ruleTable_->rowCount() > row + 1) {
      ruleTable_->selectRow(row + 1);
    }
    saveColorRules();
  });

  connect(moveUpRuleButton, &QPushButton::clicked, [this]() {
    if (!ruleTable_) {
      return;
    }
    const int row = ruleTable_->currentRow();
    if (row <= 0 || row >= static_cast<int>(colorRules_.size())) {
      return;
    }
    std::swap(colorRules_[static_cast<size_t>(row)],
              colorRules_[static_cast<size_t>(row - 1)]);
    refreshColorRuleTable();
    ruleTable_->selectRow(row - 1);
    saveColorRules();
  });

  connect(moveDownRuleButton, &QPushButton::clicked, [this]() {
    if (!ruleTable_) {
      return;
    }
    const int row = ruleTable_->currentRow();
    if (row < 0 || row + 1 >= static_cast<int>(colorRules_.size())) {
      return;
    }
    std::swap(colorRules_[static_cast<size_t>(row)],
              colorRules_[static_cast<size_t>(row + 1)]);
    refreshColorRuleTable();
    ruleTable_->selectRow(row + 1);
    saveColorRules();
  });

  connect(toggleAllEnforceButton, &QPushButton::clicked, [this]() {
    if (colorRules_.empty()) {
      return;
    }
    const bool anyDisabled = std::any_of(colorRules_.begin(), colorRules_.end(),
                                         [](const ColorRuleRow &rule) {
                                           return !rule.enforce;
                                         });
    for (auto &rule : colorRules_) {
      rule.enforce = anyDisabled;
    }
    refreshColorRuleTable();
    saveColorRules();
  });

  connect(seedPresetButton, &QPushButton::clicked, [this]() {
    colorRules_.clear();
    colorRules_.push_back({"main.alpha", "==", 1.0, "background", true});
    colorRules_.push_back({"main.hue", ">=", 0.0, "palette", true});
    colorRules_.push_back({"surface.sat", ">=", 0.0, "surface", true});
    colorRules_.push_back({"text.luma", ">=", 0.0, "text", true});
    refreshColorRuleTable();
    saveColorRules();
  });

  connect(seedHuePresetButton, &QPushButton::clicked, [this]() {
    colorRules_.clear();
    colorRules_.push_back({"main.hue", ">=", 30.0, "palette", true});
    colorRules_.push_back({"accent.hue", "<=", 210.0, "palette", true});
    colorRules_.push_back({"surface.luma", ">=", 0.15, "surface", true});
    colorRules_.push_back({"text.luma", ">=", 0.75, "text", true});
    refreshColorRuleTable();
    saveColorRules();
  });

  connect(resetRulesButton, &QPushButton::clicked, [this]() {
    colorRules_.clear();
    colorRules_.push_back({"main.alpha", "==", 1.0, "background", true});
    colorRules_.push_back({"accent.hue", "==", 180.0, "palette", true});
    refreshColorRuleTable();
    saveColorRules();
  });

  connect(ruleTable_, &QTableWidget::itemChanged, [this](QTableWidgetItem *) {
    syncColorRulesFromTable();
    saveColorRules();
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

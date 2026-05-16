module;
#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QEnterEvent>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>
#include <algorithm>
#include <utility>
#include <wobjectimpl.h>


module Artifact.Widgets.LooksPresetBrowser;

import Widgets.Utils.CSS;

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
namespace {

// Generates a colored placeholder thumbnail for a preset
QPixmap makePlaceholderThumbnail(const QString &name, const QColor &tint,
                                 int w = 160, int h = 100) {
  QPixmap pm(w, h);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);

  // Dark base
  p.fillRect(0, 0, w, h, QColor(0x1a, 0x1a, 0x1a));

  // Tinted bokeh circles
  for (int i = 0; i < 8; ++i) {
    QColor c = tint;
    c.setAlpha(30 + i * 8);
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    int r = 20 + (i * 13) % 40;
    int x = (i * 47) % (w - r);
    int y = (i * 31) % (h - r);
    p.drawEllipse(x, y, r * 2, r * 2);
  }

  // Slight gradient overlay
  QLinearGradient grad(0, 0, w, h);
  QColor gc = tint;
  gc.setAlpha(40);
  grad.setColorAt(0, gc);
  grad.setColorAt(1, Qt::transparent);
  p.fillRect(0, 0, w, h, grad);

  // SAMPLE watermark
  p.setPen(QColor(255, 255, 255, 55));
  QFont f("Segoe UI", 13, QFont::Bold);
  f.setLetterSpacing(QFont::AbsoluteSpacing, 4);
  p.setFont(f);
  p.drawText(pm.rect(), Qt::AlignCenter, "SAMPLE");

  return pm;
}

// ---------------------------------------------------------------------------
//  Built-in preset data
// ---------------------------------------------------------------------------
struct PresetDef {
  const char *id;
  const char *name;
  const char *category;
  const char *pack;
  int score;
  QColor tint;
};

static const PresetDef kBuiltinPresets[] = {
    {"amber_dusk",
     "Amber Dusk",
     "Film",
     "Looks Classic",
     78,
     {0xc8, 0x7a, 0x20}},
    {"bleach_bypass",
     "Bleach Bypass",
     "Film",
     "Looks Classic",
     85,
     {0x88, 0x99, 0x88}},
    {"broadcast_safe",
     "Broadcast Safe",
     "TV & Video",
     "Looks Classic",
     55,
     {0x60, 0x80, 0xa0}},
    {"cold_day", "Cold Day", "Photo", "Cinematic", 70, {0x50, 0x70, 0xc0}},
    {"cross_process",
     "Cross Process",
     "Creative",
     "Cinematic",
     90,
     {0x80, 0x40, 0xc0}},
    {"cyberpunk", "Cyberpunk", "Creative", "Cinematic", 92, {0x80, 0x00, 0xff}},
    {"day_for_night",
     "Day For Night",
     "Film",
     "Cinematic",
     88,
     {0x20, 0x30, 0x60}},
    {"deep_forest",
     "Deep Forest",
     "Photo",
     "Studio Looks",
     80,
     {0x20, 0x50, 0x30}},
    {"fuji_velvia",
     "Fuji Velvia",
     "Film",
     "Looks Classic",
     80,
     {0xd0, 0x60, 0x30}},
    {"golden_hour",
     "Golden Hour",
     "Photo",
     "Studio Looks",
     85,
     {0xd0, 0x90, 0x20}},
    {"horror_night",
     "Horror Night",
     "Creative",
     "Studio Looks",
     95,
     {0x10, 0x10, 0x10}},
    {"hyper_real",
     "Hyper Real",
     "Photo",
     "Studio Looks",
     90,
     {0x60, 0xa0, 0xd0}},
    {"kodak_5219",
     "Kodak 5219",
     "Film",
     "Looks Classic",
     90,
     {0xb0, 0x70, 0x40}},
    {"log_c_rec709",
     "Log C Rec709",
     "TV & Video",
     "Cinematic",
     100,
     {0x70, 0x80, 0x70}},
    {"miami_vice",
     "Miami Vice",
     "TV & Video",
     "Cinematic",
     85,
     {0x00, 0xc0, 0xc0}},
    {"muted_indie",
     "Muted Indie",
     "Film",
     "Studio Looks",
     70,
     {0x80, 0x80, 0xa0}},
    {"news_footage",
     "News Footage",
     "TV & Video",
     "Looks Classic",
     60,
     {0x70, 0x70, 0x70}},
    {"noir_bw",
     "Noir B&W",
     "Creative",
     "Looks Classic",
     100,
     {0x30, 0x30, 0x30}},
    {"pastel_dream",
     "Pastel Dream",
     "Photo",
     "カスタム",
     65,
     {0xd0, 0xa0, 0xb0}},
    {"pro_mist", "Pro Mist", "Film", "カスタム", 75, {0xb0, 0xb0, 0xc0}},
    {"raw_log_flat",
     "Raw Log Flat",
     "TV & Video",
     "カスタム",
     40,
     {0x80, 0x90, 0x80}},
    {"teal_orange",
     "Teal & Orange",
     "Film",
     "カスタム",
     75,
     {0x00, 0x80, 0x80}},
    {"vintage_70s",
     "Vintage 70s",
     "Creative",
     "Looks Classic",
     80,
     {0xb0, 0x80, 0x40}},
    {"washed_out", "Washed Out", "Photo", "Cinematic", 70, {0xa0, 0xa0, 0xb0}},
};

} // anonymous namespace

// ---------------------------------------------------------------------------
//  PresetCard  –  single thumbnail tile in the grid
// ---------------------------------------------------------------------------
namespace Artifact {

class PresetCard : public QFrame {
  W_OBJECT(PresetCard)

  LooksPreset preset_;
  bool selected_{false};
  bool hovered_{false};

  QLabel *thumb_;
  QLabel *nameLabel_;
  QLabel *scoreLabel_;

public:
  explicit PresetCard(const LooksPreset &p, QWidget *parent = nullptr)
      : QFrame(parent), preset_(p) {
    setFixedSize(160, 120);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);

    auto *vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    thumb_ = new QLabel(this);
    thumb_->setFixedSize(160, 96);
    thumb_->setScaledContents(true);
    if (!p.thumbnail.isNull())
      thumb_->setPixmap(p.thumbnail);
    vl->addWidget(thumb_);

    auto *bar = new QWidget(this);
    bar->setFixedHeight(24);
    bar->setStyleSheet("background:#212121;");
    auto *hl = new QHBoxLayout(bar);
    hl->setContentsMargins(6, 0, 6, 0);

    nameLabel_ = new QLabel(p.name, bar);
    nameLabel_->setStyleSheet("color:#cccccc; font-size:10px;");

    scoreLabel_ = new QLabel(QString::number(p.score) + "%", bar);
    scoreLabel_->setStyleSheet("color:#777777; font-size:10px;");
    scoreLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    hl->addWidget(nameLabel_);
    hl->addWidget(scoreLabel_);
    vl->addWidget(bar);

    updateStyle();
  }

  const LooksPreset &preset() const { return preset_; }
  void setPreset(const LooksPreset &p) { preset_ = p; }

  void setSelected(bool s) {
    selected_ = s;
    updateStyle();
  }

signals:
  void clicked(const QString &id) W_SIGNAL(clicked, id);
  void doubleClicked(const QString &id) W_SIGNAL(doubleClicked, id);

protected:
  void enterEvent(QEnterEvent *) override {
    hovered_ = true;
    updateStyle();
  }
  void leaveEvent(QEvent *) override {
    hovered_ = false;
    updateStyle();
  }

  void mousePressEvent(QMouseEvent *e) override {
    if (e->button() == Qt::LeftButton)
      emit clicked(preset_.id);
  }

  void mouseDoubleClickEvent(QMouseEvent *e) override {
    if (e->button() == Qt::LeftButton)
      emit doubleClicked(preset_.id);
  }

private:
  void updateStyle() {
    if (selected_)
      setStyleSheet("QFrame { border: 2px solid #4a8aff; background:#1a1a1a; "
                    "border-radius:3px; }");
    else if (hovered_)
      setStyleSheet("QFrame { border: 1px solid #555555; background:#222222; "
                    "border-radius:3px; }");
    else
      setStyleSheet("QFrame { border: 1px solid #2a2a2a; background:#1a1a1a; "
                    "border-radius:3px; }");
  }
};
W_OBJECT_IMPL(PresetCard)

// ---------------------------------------------------------------------------
//  Impl
// ---------------------------------------------------------------------------
class LooksPresetBrowserDialog::Impl {
public:
  LooksPresetBrowserDialog *q;

  // Data
  QVector<LooksPreset> allPresets_;
  QString selectedId_;
  QString appliedId_;
  QString activeCategory_{"All"};
  QString activeLibrary_{"すべて"};
  QString searchText_;

  // UI – sidebar
  QListWidget *libraryList_{nullptr};

  // UI – top bar
  QLineEdit *searchEdit_{nullptr};
  QButtonGroup *catGroup_{nullptr};
  QComboBox *sortCombo_{nullptr};

  // UI – applied badge
  QLabel *appliedBadge_{nullptr};

  // UI – preview
  QLabel *beforeLabel_{nullptr};
  QLabel *afterLabel_{nullptr};
  QLabel *selectHint_{nullptr};
  QPushButton *applyBtn_{nullptr};
  QPushButton *favBtn_{nullptr};

  // UI – sliders
  QSlider *intensitySlider_{nullptr};
  QSlider *exposureSlider_{nullptr};
  QSlider *contrastSlider_{nullptr};
  QSlider *tempSlider_{nullptr};
  QLabel *intensityVal_{nullptr};
  QLabel *exposureVal_{nullptr};
  QLabel *contrastVal_{nullptr};
  QLabel *tempVal_{nullptr};

  // UI – grid
  QWidget *gridContainer_{nullptr};
  QGridLayout *gridLayout_{nullptr};
  QVector<PresetCard *> cards_;

  // Preview image
  QPixmap previewSource_;

  // -----------------------------------------------------------------------
  void init(LooksPresetBrowserDialog *dlg) {
    q = dlg;
    loadBuiltinPresets();
    buildUi();
    refreshGrid();
  }

  // -----------------------------------------------------------------------
  void loadBuiltinPresets() {
    for (const auto &d : kBuiltinPresets) {
      LooksPreset p;
      p.id = d.id;
      p.name = d.name;
      p.category = d.category;
      p.pack = d.pack;
      p.score = d.score;
      p.thumbnail = makePlaceholderThumbnail(p.name, d.tint);
      allPresets_.append(p);
    }
  }

  // -----------------------------------------------------------------------
  void buildUi() {
    q->setWindowTitle("LOOKS — PRESET BROWSER");
    q->setMinimumSize(900, 700);
    q->setAutoFillBackground(true);

    // Root layout: left sidebar + right content
    auto *rootLayout = new QHBoxLayout(q);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── Sidebar ──────────────────────────────────────────────────────
    auto *sidebar = new QWidget(q);
    sidebar->setFixedWidth(185);
    sidebar->setAutoFillBackground(true);

    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 8, 0, 8);
    sidebarLayout->setSpacing(0);

    libraryList_ = new QListWidget(sidebar);
    libraryList_->setSelectionMode(QAbstractItemView::SingleSelection);

    auto addSectionHeader = [&](const QString &title) {
      auto *item = new QListWidgetItem(title, libraryList_);
      item->setFlags(Qt::NoItemFlags);
      QFont f = item->font();
      f.setPointSize(8);
      f.setBold(true);
      item->setFont(f);
      item->setForeground(QColor(0x66, 0x66, 0x66));
    };

    auto addLibItem = [&](const QString &label, int count) {
      auto *item = new QListWidgetItem(libraryList_);
      auto *w = new QWidget(libraryList_);
      auto *hl = new QHBoxLayout(w);
      hl->setContentsMargins(12, 4, 12, 4);
      auto *lbl = new QLabel(label, w);
      QPalette pal = lbl->palette();
      pal.setColor(QPalette::WindowText,
                   QColor(ArtifactCore::currentDCCTheme().textColor));
      lbl->setPalette(pal);
      auto *cnt = new QLabel(QString::number(count), w);
      QPalette cntPal = cnt->palette();
      cntPal.setColor(
          QPalette::WindowText,
          QColor(ArtifactCore::currentDCCTheme().textColor).darker(140));
      cnt->setPalette(cntPal);
      cnt->setAlignment(Qt::AlignRight);
      hl->addWidget(lbl);
      hl->addStretch();
      hl->addWidget(cnt);
      item->setSizeHint(w->sizeHint());
      libraryList_->setItemWidget(item, w);
      item->setData(Qt::UserRole, label);
    };

    addSectionHeader("  ライブラリ");
    addLibItem("すべて", allPresets_.size());
    addLibItem("お気に入り", 0);
    addLibItem("最近使用", 0);
    addSectionHeader("  パック");

    // Count presets per pack
    QMap<QString, int> packCounts;
    for (const auto &p : allPresets_)
      packCounts[p.pack]++;
    for (auto it = packCounts.constBegin(); it != packCounts.constEnd(); ++it)
      addLibItem(it.key(), it.value());

    libraryList_->setCurrentRow(1); // "すべて" selected
    sidebarLayout->addWidget(libraryList_);

    QObject::connect(libraryList_, &QListWidget::currentItemChanged,
                     [this](QListWidgetItem *item, QListWidgetItem *) {
                       if (!item)
                         return;
                       QString label = item->data(Qt::UserRole).toString();
                       if (!label.isEmpty()) {
                         activeLibrary_ = label;
                         refreshGrid();
                       }
                     });

    rootLayout->addWidget(sidebar);

    // ── Main content ─────────────────────────────────────────────────
    auto *mainWidget = new QWidget(q);
    auto *mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top bar
    mainLayout->addWidget(buildTopBar(mainWidget));

    // Preview + sliders
    mainLayout->addWidget(buildPreviewArea(mainWidget));

    // Grid scroll area
    auto *scrollArea = new QScrollArea(mainWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    gridContainer_ = new QWidget();
    gridContainer_->setAutoFillBackground(true);
    gridLayout_ = new QGridLayout(gridContainer_);
    gridLayout_->setContentsMargins(8, 8, 8, 8);
    gridLayout_->setSpacing(6);

    scrollArea->setWidget(gridContainer_);
    mainLayout->addWidget(scrollArea, 1);

    rootLayout->addWidget(mainWidget, 1);
  }

  // -----------------------------------------------------------------------
  QWidget *buildTopBar(QWidget *parent) {
    auto *bar = new QWidget(parent);
    bar->setFixedHeight(48);
    bar->setAutoFillBackground(true);

    auto *hl = new QHBoxLayout(bar);
    hl->setContentsMargins(10, 0, 10, 0);
    hl->setSpacing(6);

    // Search
    searchEdit_ = new QLineEdit(bar);
    searchEdit_->setPlaceholderText("Search presets...");
    searchEdit_->setFixedWidth(160);
    hl->addWidget(searchEdit_);
    hl->addSpacing(4);

    QObject::connect(searchEdit_, &QLineEdit::textChanged,
                     [this](const QString &t) {
                       searchText_ = t;
                       refreshGrid();
                     });

    // Category tabs
    catGroup_ = new QButtonGroup(bar);
    catGroup_->setExclusive(true);
    for (const auto &cat : {"All", "Film", "TV & Video", "Photo", "Creative"}) {
      auto *btn = new QPushButton(cat, bar);
      btn->setCheckable(true);
      btn->setFixedHeight(28);
      QPalette pal = btn->palette();
      pal.setColor(QPalette::ButtonText,
                   QColor(ArtifactCore::currentDCCTheme().textColor));
      btn->setPalette(pal);
      catGroup_->addButton(btn);
      hl->addWidget(btn);
      if (QString(cat) == "All")
        btn->setChecked(true);
    }

    QObject::connect(catGroup_, &QButtonGroup::buttonClicked,
                     [this](QAbstractButton *btn) {
                       activeCategory_ = btn->text();
                       refreshGrid();
                     });

    hl->addStretch();

    // Sort
    sortCombo_ = new QComboBox(bar);
    sortCombo_->addItems({"名前順", "スコア順", "最近追加"});
    hl->addWidget(sortCombo_);

    // Applied badge
    appliedBadge_ = new QLabel("No preset applied", bar);
    appliedBadge_->setStyleSheet(
        "background:#1a3a6a; color:#4a8aff; border:1px solid #2a5aaa;"
        "border-radius:4px; padding:3px 10px; font-size:10px;");
    hl->addWidget(appliedBadge_);

    return bar;
  }

  // -----------------------------------------------------------------------
  QWidget *buildPreviewArea(QWidget *parent) {
    auto *panel = new QWidget(parent);
    panel->setFixedHeight(70);
    panel->setStyleSheet(
        "background:#1c1c1c; border-bottom:1px solid #2a2a2a;");

    auto *hl = new QHBoxLayout(panel);
    hl->setContentsMargins(10, 6, 10, 6);
    hl->setSpacing(8);

    // BEFORE thumbnail
    beforeLabel_ = new QLabel(panel);
    beforeLabel_->setFixedSize(88, 55);
    beforeLabel_->setScaledContents(true);
    beforeLabel_->setStyleSheet("border:1px solid #333; background:#111;");
    auto *beforeWrap = new QWidget(panel);
    auto *bvl = new QVBoxLayout(beforeWrap);
    bvl->setContentsMargins(0, 0, 0, 0);
    bvl->setSpacing(2);
    auto *bLbl = new QLabel("BEFORE", panel);
    bLbl->setStyleSheet("color:#666; font-size:9px;");
    bLbl->setAlignment(Qt::AlignCenter);
    bvl->addWidget(beforeLabel_);
    bvl->addWidget(bLbl);
    hl->addWidget(beforeWrap);

    // ► arrow
    auto *arrow = new QLabel("›", panel);
    arrow->setStyleSheet("color:#555; font-size:18px;");
    hl->addWidget(arrow);

    // AFTER thumbnail
    afterLabel_ = new QLabel(panel);
    afterLabel_->setFixedSize(88, 55);
    afterLabel_->setScaledContents(true);
    afterLabel_->setStyleSheet("border:1px solid #333; background:#111;");
    auto *afterWrap = new QWidget(panel);
    auto *avl = new QVBoxLayout(afterWrap);
    avl->setContentsMargins(0, 0, 0, 0);
    avl->setSpacing(2);
    auto *aLbl = new QLabel("AFTER", panel);
    aLbl->setStyleSheet("color:#666; font-size:9px;");
    aLbl->setAlignment(Qt::AlignCenter);
    avl->addWidget(afterLabel_);
    avl->addWidget(aLbl);
    hl->addWidget(afterWrap);

    hl->addSpacing(8);

    // Select hint
    selectHint_ = new QLabel("— プリセットを選択 —", panel);
    selectHint_->setStyleSheet("color:#666; font-size:11px;");
    hl->addWidget(selectHint_);

    hl->addStretch();

    // Sliders
    hl->addWidget(buildSliderBlock(panel, "INTENSITY", -100, 100, 80,
                                   &intensitySlider_, &intensityVal_));
    hl->addWidget(buildSliderBlock(panel, "EXPOSURE", -100, 100, 0,
                                   &exposureSlider_, &exposureVal_));
    hl->addWidget(buildSliderBlock(panel, "CONTRAST", -100, 100, 0,
                                   &contrastSlider_, &contrastVal_));
    hl->addWidget(
        buildSliderBlock(panel, "TEMP", -100, 100, 0, &tempSlider_, &tempVal_));

    hl->addStretch();

    // Apply / Favorite
    applyBtn_ = new QPushButton("適用  ►", panel);
    applyBtn_->setFixedHeight(32);
    applyBtn_->setStyleSheet(R"(
            QPushButton {
                background:#1a3a1a; border:1px solid #2a6a2a;
                border-radius:4px; color:#55cc55; font-size:12px; padding:0 16px;
            }
            QPushButton:hover  { background:#204a20; }
            QPushButton:pressed{ background:#142814; }
        )");
    QObject::connect(applyBtn_, &QPushButton::clicked, [this]() { onApply(); });

    favBtn_ = new QPushButton("★ お気に入り", panel);
    favBtn_->setFixedHeight(32);
    favBtn_->setStyleSheet(R"(
            QPushButton {
                background:#2a2a1a; border:1px solid #5a5a1a;
                border-radius:4px; color:#aaaa44; font-size:11px; padding:0 12px;
            }
            QPushButton:hover  { background:#3a3a1a; }
        )");
    QObject::connect(favBtn_, &QPushButton::clicked,
                     [this]() { onFavorite(); });

    hl->addWidget(applyBtn_);
    hl->addWidget(favBtn_);

    return panel;
  }

  // -----------------------------------------------------------------------
  QWidget *buildSliderBlock(QWidget *parent, const char *label, int min,
                            int max, int def, QSlider **sliderOut,
                            QLabel **valOut) {
    auto *w = new QWidget(parent);
    auto *vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(2);

    auto *lbl = new QLabel(label, w);
    lbl->setStyleSheet("color:#777; font-size:8px; letter-spacing:1px;");
    lbl->setAlignment(Qt::AlignCenter);
    vl->addWidget(lbl);

    auto *sl = new QSlider(Qt::Horizontal, w);
    sl->setRange(min, max);
    sl->setValue(def);
    sl->setFixedWidth(90);
    vl->addWidget(sl);

    auto *val = new QLabel(QString::number(def), w);
    val->setStyleSheet("color:#aaaaaa; font-size:9px;");
    val->setAlignment(Qt::AlignCenter);
    vl->addWidget(val);

    QObject::connect(sl, &QSlider::valueChanged,
                     [val](int v) { val->setText(QString::number(v)); });

    *sliderOut = sl;
    *valOut = val;
    return w;
  }

  // -----------------------------------------------------------------------
  QVector<LooksPreset> filteredPresets() const {
    QVector<LooksPreset> result;

    for (const auto &p : allPresets_) {
      // Library filter
      if (activeLibrary_ == "お気に入り" && !p.isFavorite)
        continue;
      if (activeLibrary_ != "すべて" && activeLibrary_ != "お気に入り" &&
          activeLibrary_ != "最近使用" && p.pack != activeLibrary_)
        continue;

      // Category filter
      if (activeCategory_ != "All" && p.category != activeCategory_)
        continue;

      // Search filter
      if (!searchText_.isEmpty() &&
          !p.name.contains(searchText_, Qt::CaseInsensitive) &&
          !p.pack.contains(searchText_, Qt::CaseInsensitive))
        continue;

      result.append(p);
    }

    // Sort
    if (sortCombo_) {
      int sortIdx = sortCombo_->currentIndex();
      if (sortIdx == 0) {
        std::sort(result.begin(), result.end(),
                  [](const LooksPreset &a, const LooksPreset &b) {
                    return a.name < b.name;
                  });
      } else if (sortIdx == 1) {
        std::sort(result.begin(), result.end(),
                  [](const LooksPreset &a, const LooksPreset &b) {
                    return a.score > b.score;
                  });
      }
    }

    return result;
  }

  // -----------------------------------------------------------------------
  void refreshGrid() {
    // Remove existing cards
    for (auto *c : cards_) {
      gridLayout_->removeWidget(c);
      c->deleteLater();
    }
    cards_.clear();

    const auto presets = filteredPresets();
    constexpr int kCols = 4;

    for (int i = 0; i < presets.size(); ++i) {
      auto *card = new PresetCard(presets[i], gridContainer_);
      card->setSelected(presets[i].id == selectedId_);

      QObject::connect(card, &PresetCard::clicked,
                       [this](const QString &id) { onCardSelected(id); });
      QObject::connect(card, &PresetCard::doubleClicked,
                       [this](const QString &id) {
                         selectedId_ = id;
                         onApply();
                       });

      gridLayout_->addWidget(card, i / kCols, i % kCols);
      cards_.append(card);
    }

    // Fill remaining cells so the grid stays left-aligned
    int rem = presets.size() % kCols;
    if (rem != 0) {
      for (int i = rem; i < kCols; ++i) {
        auto *spacer = new QWidget(gridContainer_);
        spacer->setFixedSize(160, 120);
        gridLayout_->addWidget(spacer, presets.size() / kCols, i);
      }
    }
  }

  // -----------------------------------------------------------------------
  void onCardSelected(const QString &id) {
    selectedId_ = id;

    for (auto *c : cards_)
      c->setSelected(c->preset().id == id);

    // Update preview hint
    const LooksPreset *p = findPreset(id);
    if (p) {
      selectHint_->setText(p->name);
      selectHint_->setStyleSheet("color:#cccccc; font-size:12px;");

      // Show preset thumbnail as "after"
      if (!p->thumbnail.isNull())
        afterLabel_->setPixmap(p->thumbnail);
    }
  }

  // -----------------------------------------------------------------------
  void onApply() {
    if (selectedId_.isEmpty())
      return;
    appliedId_ = selectedId_;

    const LooksPreset *p = findPreset(appliedId_);
    if (p) {
      appliedBadge_->setText(p->name);
      appliedBadge_->setStyleSheet(
          "background:#1a4a2a; color:#55cc88; border:1px solid #2a7a4a;"
          "border-radius:4px; padding:3px 10px; font-size:10px;");
    }

    emit q->presetApplied(appliedId_);
  }

  // -----------------------------------------------------------------------
  void onFavorite() {
    if (selectedId_.isEmpty())
      return;

    for (auto &p : allPresets_) {
      if (p.id == selectedId_) {
        p.isFavorite = !p.isFavorite;
        emit q->presetFavorited(p.id, p.isFavorite);
        break;
      }
    }
    refreshGrid();
  }

  // -----------------------------------------------------------------------
  const LooksPreset *findPreset(const QString &id) const {
    for (const auto &p : allPresets_)
      if (p.id == id)
        return &p;
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
//  LooksPresetBrowserDialog  –  public implementation
// ---------------------------------------------------------------------------

LooksPresetBrowserDialog::LooksPresetBrowserDialog(QWidget *parent)
    : QDialog(parent), impl_(new Impl) {
  impl_->init(this);
}

LooksPresetBrowserDialog::~LooksPresetBrowserDialog() { delete impl_; }

void LooksPresetBrowserDialog::setPreviewImage(const QPixmap &before) {
  impl_->previewSource_ = before;
  if (!before.isNull()) {
    impl_->beforeLabel_->setPixmap(before.scaled(impl_->beforeLabel_->size(),
                                                 Qt::KeepAspectRatioByExpanding,
                                                 Qt::SmoothTransformation));
  }
}

QString LooksPresetBrowserDialog::appliedPresetId() const {
  return impl_->appliedId_;
}

W_OBJECT_IMPL(LooksPresetBrowserDialog)

} // namespace Artifact

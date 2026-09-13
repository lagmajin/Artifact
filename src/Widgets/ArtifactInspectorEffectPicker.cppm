module;

#include <QIcon>
#include <QAbstractItemView>
#include <QColor>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QModelIndex>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QRectF>
#include <QSignalBlocker>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>
#include <functional>
#include <utility>

module Artifact.Widgets.InspectorEffectPicker;

import Artifact.Widgets.InspectorEffectCatalog;
import Artifact.Widgets.InspectorStyle;
import Artifact.Effect.Abstract;

namespace Artifact {
namespace {
QString effectCategoryIconPath(const QString& category) {
  if (category == QStringLiteral("All Effects"))
    return QStringLiteral(":/icons/Studio/effect_category_all.svg");
  if (category == QStringLiteral("Blur"))
    return QStringLiteral(":/icons/Studio/effect_category_blur.svg");
  if (category == QStringLiteral("Color"))
    return QStringLiteral(":/icons/Studio/effect_category_color.svg");
  if (category == QStringLiteral("Distort") || category == QStringLiteral("Geometry"))
    return QStringLiteral(":/icons/Studio/effect_category_distort.svg");
  if (category == QStringLiteral("Keying"))
    return QStringLiteral(":/icons/Studio/effect_category_keying.svg");
  if (category == QStringLiteral("Generate") || category == QStringLiteral("Generator"))
    return QStringLiteral(":/icons/Studio/effect_category_generate.svg");
  if (category == QStringLiteral("Glow"))
    return QStringLiteral(":/icons/Studio/effect_category_glow.svg");
  if (category == QStringLiteral("Light"))
    return QStringLiteral(":/icons/Studio/effect_category_light.svg");
  if (category == QStringLiteral("Stylize"))
    return QStringLiteral(":/icons/Studio/effect_category_stylize.svg");
  if (category == QStringLiteral("Time"))
    return QStringLiteral(":/icons/Studio/effect_category_time.svg");
  if (category == QStringLiteral("Transform"))
    return QStringLiteral(":/icons/Studio/effect_category_transform.svg");
  if (category == QStringLiteral("Film"))
    return QStringLiteral(":/icons/Studio/effect_category_film.svg");
  if (category == QStringLiteral("Detail"))
    return QStringLiteral(":/icons/Studio/effect_category_detail.svg");
  return QStringLiteral(":/icons/Studio/effectrack_effect.svg");
}
}

class EffectPickerPanel : public QWidget {
 public:
  enum class Surface {
    Flat,
    Inset,
    Detail,
    Swatch,
  };

  explicit EffectPickerPanel(QWidget* parent = nullptr) : QWidget(parent) {}

  void setSurface(const Surface surface) {
    surface_ = surface;
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPalette pal = palette();
    if (surface_ == Surface::Flat) {
      painter.fillRect(rect(), pal.color(QPalette::Base));
      return;
    }

    painter.setPen(pal.color(QPalette::Mid));
    painter.setBrush(surface_ == Surface::Inset
                         ? pal.color(QPalette::AlternateBase)
                         : pal.color(QPalette::Base));
    const qreal radius = surface_ == Surface::Detail ? 4.0 : 5.0;
    painter.drawRoundedRect(
        QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
  }

 private:
  Surface surface_ = Surface::Flat;
};

class EffectPickerLabel final : public QLabel {
 public:
  EffectPickerLabel(const QString& text, bool heading,
                    QWidget* parent = nullptr)
      : QLabel(text, parent), heading_(heading) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    if (heading_) {
      QFont labelFont = font();
      labelFont.setWeight(QFont::DemiBold);
      setFont(labelFont);
    }
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(font());
    painter.setPen(palette().color(QPalette::WindowText));
    int flags = alignment();
    if (!(flags & (Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter)))
      flags |= Qt::AlignLeft;
    if (!(flags & (Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter)))
      flags |= Qt::AlignVCenter;
    if (wordWrap()) flags |= Qt::TextWordWrap;
    painter.drawText(rect(), flags, text());
    if (heading_ && property("artifactEffectPickerHeadingRule").toBool()) {
      painter.setPen(palette().color(QPalette::Mid));
      painter.drawLine(rect().bottomLeft(), rect().bottomRight());
    }
  }

 private:
  bool heading_ = false;
};

class EffectCategoryList final : public QListWidget {
 public:
  explicit EffectCategoryList(QWidget* parent = nullptr) : QListWidget(parent) {
    setFrameShape(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMinimumWidth(190);
    setMaximumWidth(220);
  }

  void setEntries(const std::vector<EffectCatalogEntry>& entries,
                  EffectPipelineStage stage) {
    clear();
    QStringList categories;
    int allCount = 0;
    for (const auto& entry : entries) {
      if (entry.stage != stage) continue;
      ++allCount;
      if (!categories.contains(entry.category)) categories.append(entry.category);
    }
    addCategory(QStringLiteral("All Effects"), allCount);
    for (const auto& category : categories) {
      int count = 0;
      for (const auto& entry : entries)
        if (entry.stage == stage && entry.category == category) ++count;
      addCategory(category, count);
    }
    setCurrentRow(0);
  }

  void setAction(std::function<void(const QString&)> action) {
    action_ = std::move(action);
  }

 protected:
  void paintEvent(QPaintEvent* event) override {
    QPainter painter(viewport());
    if (event) painter.setClipRegion(event->region());
    const QPalette pal = palette();
    painter.fillRect(viewport()->rect(), pal.color(QPalette::Base));
    for (int row = 0; row < count(); ++row) {
      auto* categoryItem = item(row);
      if (!categoryItem) continue;
      const QRect area = visualItemRect(categoryItem);
      if (!area.intersects(viewport()->rect())) continue;
      const bool selected = categoryItem->isSelected();
      if (selected) {
        painter.fillRect(area, blendColor(pal.color(QPalette::Base),
                                          pal.color(QPalette::Highlight), 0.12));
        painter.fillRect(QRect(area.left(), area.top(), 3, area.height()),
                         pal.color(QPalette::Highlight));
      }
      const QString category = categoryItem->data(Qt::UserRole).toString();
      QIcon(effectCategoryIconPath(category)).paint(
          &painter, QRect(area.left() + 12, area.center().y() - 10, 20, 20));
      painter.setPen(pal.color(QPalette::Text));
      painter.drawText(area.adjusted(44, 0, -38, 0),
                       Qt::AlignLeft | Qt::AlignVCenter, category);
      painter.setPen(selected ? pal.color(QPalette::Highlight)
                              : pal.color(QPalette::PlaceholderText));
      painter.drawText(area.adjusted(0, 0, -12, 0),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(categoryItem->data(Qt::UserRole + 1).toInt()));
    }
  }

  void mouseReleaseEvent(QMouseEvent* event) override {
    QListWidget::mouseReleaseEvent(event);
    triggerCurrent();
  }

  void keyReleaseEvent(QKeyEvent* event) override {
    QListWidget::keyReleaseEvent(event);
    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down ||
        event->key() == Qt::Key_Home || event->key() == Qt::Key_End)
      triggerCurrent();
  }

 private:
  void addCategory(const QString& category, int count) {
    auto* categoryItem = new QListWidgetItem(category, this);
    categoryItem->setData(Qt::UserRole, category);
    categoryItem->setData(Qt::UserRole + 1, count);
    categoryItem->setSizeHint(QSize(0, 44));
  }

  void triggerCurrent() {
    if (action_ && currentItem()) action_(currentItem()->data(Qt::UserRole).toString());
  }

  std::function<void(const QString&)> action_;
};

class EffectDetailsPanel final : public EffectPickerPanel {
 public:
  explicit EffectDetailsPanel(QWidget* parent = nullptr)
      : EffectPickerPanel(parent) {
    setSurface(Surface::Detail);
    setMinimumWidth(240);
    setMaximumWidth(300);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 28, 24, 24);
    layout->setSpacing(12);
    icon_ = new QLabel(this);
    icon_->setFixedSize(56, 56);
    icon_->setAlignment(Qt::AlignCenter);
    layout->addWidget(icon_);
    title_ = new EffectPickerLabel(
        QStringLiteral("Select an effect"), false, this);
    QFont titleFont = title_->font();
    titleFont.setWeight(QFont::DemiBold);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 2.0);
    title_->setFont(titleFont);
    title_->setWordWrap(true);
    layout->addWidget(title_);
    description_ = new EffectPickerLabel(
        QStringLiteral("Choose an effect to view its description."), false,
        this);
    description_->setWordWrap(true);
    QPalette muted = description_->palette();
    muted.setColor(QPalette::WindowText, muted.color(QPalette::PlaceholderText));
    description_->setPalette(muted);
    layout->addWidget(description_);
    category_ = new EffectPickerLabel(QString(), false, this);
    category_->setWordWrap(true);
    layout->addWidget(category_);
    layout->addStretch();
  }

  void setEffect(const QString& name, const QString& category,
                 const QString& description) {
    const bool available = !name.isEmpty();
    icon_->setPixmap(available ? QIcon(effectCategoryIconPath(category)).pixmap(48, 48)
                               : QPixmap());
    title_->setText(available ? name : QStringLiteral("Select an effect"));
    description_->setText(
        available ? description
                  : QStringLiteral("Choose an effect to view its description."));
    category_->setText(available ? QStringLiteral("Category  ·  %1").arg(category)
                                 : QString());
  }

 private:
  QLabel* icon_ = nullptr;
  QLabel* title_ = nullptr;
  QLabel* description_ = nullptr;
  QLabel* category_ = nullptr;
};

class EffectPickerList final : public QListWidget {
 public:
  using QListWidget::QListWidget;

 protected:
  void paintEvent(QPaintEvent* event) override {
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    if (event) painter.setClipRegion(event->region());
    const QPalette pal = palette();
    painter.fillRect(viewport()->rect(), pal.color(QPalette::Base));
    const QModelIndex hoveredIndex =
        viewport()->underMouse()
            ? indexAt(viewport()->mapFromGlobal(QCursor::pos()))
            : QModelIndex{};
    for (int row = 0; row < count(); ++row) {
      auto* listItem = item(row);
      if (!listItem) continue;
      const QRect itemRect = visualItemRect(listItem);
      if (!itemRect.isValid() || !itemRect.intersects(viewport()->rect()))
        continue;
      const QModelIndex index = model()->index(row, 0);
      const bool selected = listItem->isSelected();
      const bool hovered = hoveredIndex == index;
      const bool selectable = listItem->flags().testFlag(Qt::ItemIsSelectable);
      const QRect rowRect = itemRect.adjusted(3, 1, -3, -1);
      if (selected) {
        painter.fillRect(rowRect, blendColor(pal.color(QPalette::Base),
                                             pal.color(QPalette::Highlight),
                                             0.16));
        painter.setPen(pal.color(QPalette::Highlight));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(
            QRectF(rowRect).adjusted(0.5, 0.5, -0.5, -0.5), 3.0, 3.0);
        painter.fillRect(QRect(rowRect.left(), rowRect.top(), 3,
                               rowRect.height()),
                         pal.color(QPalette::Highlight));
      } else if (hovered && selectable) {
        painter.fillRect(rowRect, blendColor(pal.color(QPalette::Base),
                                             pal.color(QPalette::Highlight),
                                             0.06));
      }
      painter.setPen(pal.color(QPalette::Mid));
      painter.drawLine(rowRect.bottomLeft(), rowRect.bottomRight());

      const QString displayName =
          listItem->data(Qt::UserRole + 1).toString().trimmed();
      const QString category =
          listItem->data(Qt::UserRole + 2).toString().trimmed();
      const QRect textRect = itemRect.adjusted(58, 5, -66, -5);
      if (displayName.isEmpty()) {
        painter.setPen(pal.color(QPalette::PlaceholderText));
        painter.drawText(textRect, Qt::AlignCenter, listItem->text());
        continue;
      }
      QFont nameFont = font();
      nameFont.setWeight(QFont::DemiBold);
      painter.setFont(nameFont);
      painter.setPen(pal.color(QPalette::Text));
      painter.drawText(QRect(textRect.left(), textRect.top(), textRect.width(),
                             22),
                       Qt::AlignLeft | Qt::AlignVCenter, displayName);
      painter.setFont(font());
      painter.setPen(pal.color(QPalette::PlaceholderText));
      const QString description =
          listItem->data(Qt::UserRole + 3).toString().trimmed();
      painter.drawText(QRect(textRect.left(), textRect.top() + 21,
                             textRect.width(), 18),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       fontMetrics().elidedText(description, Qt::ElideRight,
                                                textRect.width()));
      QIcon(effectCategoryIconPath(category)).paint(
          &painter, QRect(itemRect.left() + 14, itemRect.center().y() - 18, 36, 36));
      painter.drawText(itemRect.adjusted(0, 0, -34, 0),
                       Qt::AlignRight | Qt::AlignVCenter, category);
      painter.setPen(pal.color(QPalette::Text));
      painter.drawText(itemRect.adjusted(0, 0, -12, 0),
                       Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("+"));
    }
    painter.setClipping(false);
  }
};

class EffectPickerButton final : public QPushButton {
 public:
  EffectPickerButton(const QString& text, bool primary,
                     QWidget* parent = nullptr)
      : QPushButton(text, parent), primary_(primary) {
    setMinimumHeight(30);
    setAttribute(Qt::WA_Hover, true);
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QPalette pal = palette();
    const bool active = isDown() || isChecked();
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor base = primary_ ? blendColor(pal.color(QPalette::Button),
                                                accent, 0.72)
                                 : pal.color(QPalette::Button);
    painter.setPen(primary_ ? blendColor(accent, pal.color(QPalette::WindowText),
                                         0.18)
                            : pal.color(QPalette::Mid));
    painter.setBrush(active ? blendColor(base, accent, 0.28)
                            : underMouse() ? blendColor(base, accent, 0.14)
                                           : base);
    painter.drawRoundedRect(
        QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
    painter.setPen(isEnabled() ? pal.color(QPalette::ButtonText)
                               : pal.color(QPalette::Disabled,
                                           QPalette::ButtonText));
    painter.drawText(rect(), Qt::AlignCenter, text());
  }

 private:
  bool primary_ = false;
};

class EffectPickerDialog final : public QDialog {
public:
  EffectPickerDialog(const std::vector<EffectCatalogEntry> &entries,
                     const EffectPipelineStage stageFilter,
                     const QString &targetLabel, QWidget *parent = nullptr)
      : QDialog(parent), entries_(entries), stageFilter_(stageFilter) {
    setWindowTitle(QStringLiteral("Add Effect"));
    setModal(true);
    resize(1100, 720);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(14);

    auto* contextPanel = new EffectPickerPanel(this);
    contextPanel->setSurface(EffectPickerPanel::Surface::Flat);
    auto* contextLayout = new QHBoxLayout(contextPanel);
    contextLayout->setContentsMargins(14, 10, 14, 10);
    contextLayout->setSpacing(14);
    auto* targetSwatch = new EffectPickerPanel(contextPanel);
    targetSwatch->setSurface(EffectPickerPanel::Surface::Swatch);
    targetSwatch->setFixedSize(48, 48);
    QPalette swatchPalette = targetSwatch->palette();
    swatchPalette.setColor(QPalette::Base, QColor(238, 240, 244));
    targetSwatch->setPalette(swatchPalette);
    contextLayout->addWidget(targetSwatch);
    auto* targetColumn = new QVBoxLayout;
    targetColumn->setSpacing(1);
    auto* targetName = new EffectPickerLabel(targetLabel, true, contextPanel);
    targetName->setMinimumHeight(25);
    targetColumn->addWidget(targetName);
    auto* targetType = new EffectPickerLabel(
        targetLabel.startsWith(QStringLiteral("Composition"))
            ? QStringLiteral("Composition effects")
            : QStringLiteral("Layer effects"),
        false, contextPanel);
    applyInspectorLabelPalette(targetType, false);
    targetColumn->addWidget(targetType);
    contextLayout->addLayout(targetColumn);
    contextLayout->addStretch();
    auto* stageBadge = new EffectPickerPanel(contextPanel);
    stageBadge->setSurface(EffectPickerPanel::Surface::Inset);
    stageBadge->setMinimumWidth(142);
    auto* stageLayout = new QVBoxLayout(stageBadge);
    stageLayout->setContentsMargins(12, 6, 12, 6);
    stageLayout->setSpacing(0);
    auto* stageLabel = new EffectPickerLabel(
        stageDisplayName(stageFilter_), false, stageBadge);
    QFont stageFont = stageLabel->font();
    stageFont.setWeight(QFont::DemiBold);
    stageLabel->setFont(stageFont);
    stageLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    stageLayout->addWidget(stageLabel);
    auto* stageDetail = new EffectPickerLabel(
        QStringLiteral("Effect stage"), false, stageBadge);
    applyInspectorLabelPalette(stageDetail, false);
    stageDetail->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    stageLayout->addWidget(stageDetail);
    contextLayout->addWidget(stageBadge);
    layout->addWidget(contextPanel);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setObjectName(QStringLiteral("inspectorSearchEdit"));
    searchEdit_->setPlaceholderText(
        QStringLiteral("Search effects by name, category, or keyword"));
    searchEdit_->setAccessibleName(QStringLiteral("Effect search"));
    searchEdit_->setAccessibleDescription(
        QStringLiteral("Search effects by name, category, or keyword"));
    searchEdit_->setFrame(false);
    applyInspectorPalette(searchEdit_, true);
    auto* searchPanel = new EffectPickerPanel(this);
    searchPanel->setSurface(EffectPickerPanel::Surface::Inset);
    auto* searchLayout = new QVBoxLayout(searchPanel);
    searchLayout->setContentsMargins(12, 6, 12, 6);
    searchLayout->setSpacing(0);
    searchLayout->addWidget(searchEdit_);
    layout->addWidget(searchPanel);

    auto *contentFrame = new EffectPickerPanel(this);
    contentFrame->setSurface(EffectPickerPanel::Surface::Flat);
    contentFrame->setObjectName(QStringLiteral("inspectorContentFrame"));
    applyInspectorPalette(contentFrame, true);
    auto *contentLayout = new QHBoxLayout(contentFrame);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);

    categoryList_ = new EffectCategoryList(contentFrame);
    applyInspectorList(categoryList_);
    applyInspectorOwnerDrawScrollBars(categoryList_);
    categoryList_->setEntries(entries_, stageFilter_);
    categoryList_->setAction([this](const QString& category) {
      categoryFilter_ = category == QStringLiteral("All Effects") ? QString() : category;
      rebuildList();
    });
    contentLayout->addWidget(categoryList_);

    auto* resultsPanel = new EffectPickerPanel(contentFrame);
    resultsPanel->setSurface(EffectPickerPanel::Surface::Flat);
    auto* resultsLayout = new QVBoxLayout(resultsPanel);
    resultsLayout->setContentsMargins(8, 8, 8, 8);
    resultsLayout->setSpacing(8);

    resultSummaryLabel_ = new EffectPickerLabel(QString(), false, resultsPanel);
    resultSummaryLabel_->setWordWrap(true);
    applyInspectorLabelPalette(resultSummaryLabel_, false);
    resultsLayout->addWidget(resultSummaryLabel_);

    listWidget_ = new EffectPickerList(resultsPanel);
    listWidget_->setUniformItemSizes(false);
    listWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget_->setFrameShape(QFrame::NoFrame);
    applyInspectorList(listWidget_);
    applyInspectorOwnerDrawScrollBars(listWidget_);
    resultsLayout->addWidget(listWidget_, 1);
    contentLayout->addWidget(resultsPanel, 1);

    detailsPanel_ = new EffectDetailsPanel(contentFrame);
    contentLayout->addWidget(detailsPanel_);

    layout->addWidget(contentFrame, 1);

    auto *buttons = new QDialogButtonBox(Qt::Horizontal, this);
    addButton_ = new EffectPickerButton(
        QStringLiteral("Add Effect"), true, buttons);
    buttons->addButton(addButton_, QDialogButtonBox::AcceptRole);
    auto *cancelButton = new EffectPickerButton(
        QStringLiteral("Cancel"), false, buttons);
    buttons->addButton(cancelButton, QDialogButtonBox::RejectRole);
    auto* footer = new QHBoxLayout;
    auto* footerHint = new EffectPickerLabel(
        QStringLiteral("Double-click an effect to add it"), false, this);
    applyInspectorLabelPalette(footerHint, false);
    footer->addWidget(footerHint);
    footer->addStretch();
    footer->addWidget(buttons);
    layout->addLayout(footer);

    QObject::connect(searchEdit_, &QLineEdit::textChanged, this,
                     [this](const QString &) { rebuildList(); });
    QObject::connect(listWidget_, &QListWidget::currentItemChanged, this,
                     [this](QListWidgetItem *, QListWidgetItem *) {
                       syncButtonState();
                     });
    QObject::connect(listWidget_, &QListWidget::itemDoubleClicked, this,
                     [this](QListWidgetItem *item) {
                       if (!item || item->data(Qt::UserRole).toString().trimmed().isEmpty()) {
                         return;
                       }
                       accept();
                     });
    QObject::connect(buttons, &QDialogButtonBox::accepted, this,
                     &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this,
                     &QDialog::reject);

    rebuildList();
  }

  QString selectedEffectId() const {
    if (!listWidget_ || !listWidget_->currentItem()) {
      return {};
    }
    return listWidget_->currentItem()->data(Qt::UserRole).toString().trimmed();
  }

  QString selectedDisplayName() const {
    if (!listWidget_ || !listWidget_->currentItem()) {
      return {};
    }
    return listWidget_->currentItem()
        ->data(Qt::UserRole + 1)
        .toString()
        .trimmed();
  }

protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Window));
  }

private:
  void rebuildList() {
    if (!listWidget_) {
      return;
    }

    const QString query = searchEdit_ ? searchEdit_->text() : QString();
    const QSignalBlocker blocker(listWidget_);
    listWidget_->clear();

    int matchCount = 0;
    for (const auto &entry : entries_) {
      if (entry.stage != stageFilter_ ||
          (!categoryFilter_.isEmpty() && entry.category != categoryFilter_) ||
          !effectCatalogEntryMatches(entry, query)) {
        continue;
      }
      ++matchCount;
      auto *item = new QListWidgetItem(
          QStringLiteral("%1  |  %2").arg(entry.displayName, entry.category),
          listWidget_);
      item->setData(Qt::UserRole, entry.effectId);
      item->setData(Qt::UserRole + 1, entry.displayName);
      item->setData(Qt::UserRole + 2, entry.category);
      item->setData(Qt::UserRole + 3, entry.description);
      item->setSizeHint(QSize(0, 62));
      item->setToolTip(entry.description);
    }

    if (matchCount == 0) {
      auto *item =
          new QListWidgetItem(QStringLiteral("No effects match this search."),
                              listWidget_);
      item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
      item->setSizeHint(QSize(0, 62));
    } else {
      listWidget_->setCurrentRow(0);
    }

    if (resultSummaryLabel_) {
      resultSummaryLabel_->setText(
          matchCount > 0
              ? QStringLiteral("%1 effects   |   %2 stage")
                    .arg(matchCount)
                    .arg(stageDisplayName(stageFilter_))
              : QStringLiteral("No matching effects in %1.")
                    .arg(stageDisplayName(stageFilter_)));
    }
    syncButtonState();
  }

  void syncButtonState() {
    if (!addButton_) {
      return;
    }
    const bool hasSelection = !selectedEffectId().isEmpty();
    addButton_->setEnabled(hasSelection);
    if (detailsPanel_) {
      auto* current = listWidget_ ? listWidget_->currentItem() : nullptr;
      detailsPanel_->setEffect(
          hasSelection && current ? current->data(Qt::UserRole + 1).toString()
                                  : QString(),
          hasSelection && current ? current->data(Qt::UserRole + 2).toString()
                                  : QString(),
          hasSelection && current ? current->data(Qt::UserRole + 3).toString()
                                  : QString());
    }
  }

  std::vector<EffectCatalogEntry> entries_;
  EffectPipelineStage stageFilter_;
  QLineEdit *searchEdit_ = nullptr;
  EffectCategoryList* categoryList_ = nullptr;
  QListWidget *listWidget_ = nullptr;
  EffectDetailsPanel* detailsPanel_ = nullptr;
  QLabel *resultSummaryLabel_ = nullptr;
  QPushButton *addButton_ = nullptr;
  QString categoryFilter_;
};

QDialog* createInspectorEffectPickerDialog(
    const std::vector<EffectCatalogEntry>& entries, EffectPipelineStage stage,
    const QString& targetLabel, QWidget* parent) {
  return new EffectPickerDialog(entries, stage, targetLabel, parent);
}

QString inspectorEffectPickerSelectedEffectId(QDialog* dialog) {
  if (auto* picker = dynamic_cast<EffectPickerDialog*>(dialog)) {
    return picker->selectedEffectId();
  }
  return {};
}

} // namespace Artifact

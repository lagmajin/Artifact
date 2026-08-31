module;

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
#include <QModelIndex>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPushButton>
#include <QRectF>
#include <QSignalBlocker>
#include <QSize>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

export module Artifact.Widgets.InspectorEffectPicker;

import Artifact.Widgets.InspectorEffectCatalog;
import Artifact.Widgets.InspectorStyle;

export namespace Artifact {
class EffectPickerPanel final : public QWidget {
 public:
  using QWidget::QWidget;

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPalette pal = palette();
    painter.setPen(pal.color(QPalette::Mid));
    painter.setBrush(pal.color(QPalette::Base));
    painter.drawRoundedRect(
        QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 5.0, 5.0);
  }
};

class EffectPickerLabel final : public QLabel {
 public:
  EffectPickerLabel(const QString& text, bool heading,
                    QWidget* parent = nullptr)
      : QLabel(text, parent), heading_(heading) {
    setAttribute(Qt::WA_TranslucentBackground, true);
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
    painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter |
                                 (wordWrap() ? Qt::TextWordWrap : 0),
                     text());
    if (heading_) {
      painter.setPen(palette().color(QPalette::Mid));
      painter.drawLine(rect().bottomLeft(), rect().bottomRight());
    }
  }

 private:
  bool heading_ = false;
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
      const QRectF cardRect =
          QRectF(itemRect).adjusted(2.0, 2.0, -2.0, -2.0);
      painter.setPen(selected ? pal.color(QPalette::Highlight)
                              : pal.color(QPalette::Mid));
      painter.setBrush(selected
                           ? blendColor(pal.color(QPalette::Base),
                                        pal.color(QPalette::Highlight), 0.36)
                           : hovered && selectable
                                 ? blendColor(pal.color(QPalette::Base),
                                              pal.color(QPalette::Highlight),
                                              0.12)
                                 : pal.color(QPalette::AlternateBase));
      painter.drawRoundedRect(cardRect, 4.0, 4.0);

      const QString displayName =
          listItem->data(Qt::UserRole + 1).toString().trimmed();
      const QString category =
          listItem->data(Qt::UserRole + 2).toString().trimmed();
      const QRect textRect = itemRect.adjusted(12, 3, -10, -3);
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
      painter.drawText(QRect(textRect.left(), textRect.top() + 21,
                             textRect.width(), 18),
                       Qt::AlignLeft | Qt::AlignVCenter, category);
    }
    painter.setClipping(false);
    painter.setPen(hasFocus() ? pal.color(QPalette::Highlight)
                              : pal.color(QPalette::Mid));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(
        QRectF(viewport()->rect()).adjusted(0.5, 0.5, -0.5, -0.5),
        4.0, 4.0);
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
                                                accent, 0.48)
                                 : pal.color(QPalette::Button);
    painter.setPen(primary_ ? accent : pal.color(QPalette::Mid));
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
    resize(760, 540);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *header = new EffectPickerLabel(
        QStringLiteral("Add to %1  |  Stage: %2")
            .arg(targetLabel, stageDisplayName(stageFilter_)),
        true, this);
    header->setMinimumHeight(30);
    applyInspectorLabelPalette(header, true);
    layout->addWidget(header);

    auto *subHeader = new EffectPickerLabel(
        QStringLiteral("Search by name, category, or keyword. Double click or press Add to insert and focus the effect."),
        false, this);
    subHeader->setWordWrap(true);
    applyInspectorLabelPalette(subHeader, false);
    layout->addWidget(subHeader);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setObjectName(QStringLiteral("inspectorSearchEdit"));
    searchEdit_->setPlaceholderText(
        QStringLiteral("Search effects for this stage"));
    searchEdit_->setAccessibleName(QStringLiteral("Effect search"));
    searchEdit_->setAccessibleDescription(
        QStringLiteral("Search effects by name, category, or keyword"));
    searchEdit_->setFrame(false);
    applyInspectorPalette(searchEdit_, true);
    auto* searchPanel = new EffectPickerPanel(this);
    auto* searchLayout = new QVBoxLayout(searchPanel);
    searchLayout->setContentsMargins(7, 3, 7, 3);
    searchLayout->setSpacing(0);
    searchLayout->addWidget(searchEdit_);
    layout->addWidget(searchPanel);

    auto *contentFrame = new EffectPickerPanel(this);
    contentFrame->setObjectName(QStringLiteral("inspectorContentFrame"));
    applyInspectorPalette(contentFrame, true);
    auto *contentLayout = new QVBoxLayout(contentFrame);
    contentLayout->setContentsMargins(8, 8, 8, 8);
    contentLayout->setSpacing(8);

    resultSummaryLabel_ = new EffectPickerLabel(QString(), false, contentFrame);
    resultSummaryLabel_->setWordWrap(true);
    applyInspectorLabelPalette(resultSummaryLabel_, false);
    contentLayout->addWidget(resultSummaryLabel_);

    listWidget_ = new EffectPickerList(contentFrame);
    listWidget_->setUniformItemSizes(false);
    listWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget_->setFrameShape(QFrame::NoFrame);
    applyInspectorList(listWidget_);
    applyInspectorOwnerDrawScrollBars(listWidget_);
    contentLayout->addWidget(listWidget_, 1);

    layout->addWidget(contentFrame, 1);

    auto *buttons = new QDialogButtonBox(Qt::Horizontal, this);
    addButton_ = new EffectPickerButton(
        QStringLiteral("Add Effect"), true, buttons);
    buttons->addButton(addButton_, QDialogButtonBox::AcceptRole);
    auto *cancelButton = new EffectPickerButton(
        QStringLiteral("Cancel"), false, buttons);
    buttons->addButton(cancelButton, QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

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
      if (entry.stage != stageFilter_ || !effectCatalogEntryMatches(entry, query)) {
        continue;
      }
      ++matchCount;
      auto *item = new QListWidgetItem(
          QStringLiteral("%1  |  %2").arg(entry.displayName, entry.category),
          listWidget_);
      item->setData(Qt::UserRole, entry.effectId);
      item->setData(Qt::UserRole + 1, entry.displayName);
      item->setData(Qt::UserRole + 2, entry.category);
      item->setSizeHint(QSize(0, 46));
      item->setToolTip(entry.description);
    }

    if (matchCount == 0) {
      auto *item =
          new QListWidgetItem(QStringLiteral("No effects match this search."),
                              listWidget_);
      item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
      item->setSizeHint(QSize(0, 46));
    } else {
      listWidget_->setCurrentRow(0);
    }

    if (resultSummaryLabel_) {
      resultSummaryLabel_->setText(
          matchCount > 0
              ? QStringLiteral("%1 effect(s) available in %2.")
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
  }

  std::vector<EffectCatalogEntry> entries_;
  EffectPipelineStage stageFilter_;
  QLineEdit *searchEdit_ = nullptr;
  QListWidget *listWidget_ = nullptr;
  QLabel *resultSummaryLabel_ = nullptr;
  QPushButton *addButton_ = nullptr;
};

} // namespace Artifact

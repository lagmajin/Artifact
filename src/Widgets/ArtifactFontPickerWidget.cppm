module;

#include <functional>
#include <utility>
#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPalette>
#include <QSplitter>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <wobjectimpl.h>

module Artifact.Widgets.FontPicker;

import Artifact.Event.Types;
import Event.Bus;
import Widgets.Utils.CSS;

namespace Artifact {
namespace {

class FontSearchEdit final : public QLineEdit {
public:
  using QLineEdit::QLineEdit;
  std::function<void(const QString&)> changed;

protected:
  void keyReleaseEvent(QKeyEvent* event) override {
    QLineEdit::keyReleaseEvent(event);
    if (changed) changed(text());
  }
};

class FontFamilyList final : public QListWidget {
public:
  using QListWidget::QListWidget;
  std::function<void(const QString&)> hovered;
  std::function<void(const QString&)> accepted;

protected:
  void mouseMoveEvent(QMouseEvent* event) override {
    QListWidget::mouseMoveEvent(event);
    if (const auto* item = itemAt(event->pos()); item && hovered)
      hovered(item->text());
  }

  void mouseReleaseEvent(QMouseEvent* event) override {
    QListWidget::mouseReleaseEvent(event);
    if (const auto* item = itemAt(event->pos()); item && accepted)
      accepted(item->text());
  }

  void keyPressEvent(QKeyEvent* event) override {
    QListWidget::keyPressEvent(event);
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        currentItem() && accepted) {
      accepted(currentItem()->text());
    }
  }
};

class FontPreviewPopup final : public QFrame {
public:
  explicit FontPreviewPopup(QWidget* parent)
      : QFrame(parent, Qt::Popup),
        search_(new FontSearchEdit(this)),
        list_(new FontFamilyList(this)),
        familyLabel_(new QLabel(this)),
        specimen_(new QLabel(QStringLiteral("Create with clarity"), this)),
        alphabet_(new QLabel(
            QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
                           "abcdefghijklmnopqrstuvwxyz\n0123456789"),
            this)) {
    setFrameShape(QFrame::StyledPanel);
    setMinimumSize(700, 390);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(7);
    search_->setPlaceholderText(QStringLiteral("Search fonts…"));
    search_->setClearButtonEnabled(true);
    root->addWidget(search_);

    auto* split = new QSplitter(Qt::Horizontal, this);
    list_->setMouseTracking(true);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setAlternatingRowColors(true);
    split->addWidget(list_);

    auto* preview = new QFrame(split);
    preview->setFrameShape(QFrame::StyledPanel);
    auto* previewLayout = new QVBoxLayout(preview);
    previewLayout->setContentsMargins(22, 18, 22, 18);
    familyLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    previewLayout->addWidget(familyLabel_);
    previewLayout->addStretch();
    specimen_->setWordWrap(true);
    previewLayout->addWidget(specimen_);
    previewLayout->addSpacing(14);
    alphabet_->setWordWrap(true);
    previewLayout->addWidget(alphabet_);
    previewLayout->addStretch();
    split->addWidget(preview);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
    root->addWidget(split, 1);

    const auto& theme = ArtifactCore::currentDCCTheme();
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, QColor(theme.backgroundColor));
    palette.setColor(QPalette::Base, QColor(theme.secondaryBackgroundColor));
    palette.setColor(QPalette::AlternateBase, QColor(theme.backgroundColor));
    palette.setColor(QPalette::Text, QColor(theme.textColor));
    palette.setColor(QPalette::WindowText, QColor(theme.textColor));
    palette.setColor(QPalette::Highlight, QColor(theme.accentColor));
    palette.setColor(QPalette::HighlightedText, QColor(theme.backgroundColor));
    setPalette(palette);
    setAutoFillBackground(true);
    list_->setPalette(palette);
    search_->setPalette(palette);

    search_->changed = [this](const QString& filter) { rebuild(filter); };
    list_->hovered = [this](const QString& family) { showPreview(family); };
    list_->accepted = [this](const QString& family) {
      if (accepted) accepted(family);
      hide();
    };
  }

  void setFamilies(QStringList families) {
    families_ = std::move(families);
    rebuild(search_->text());
  }

  void openFor(const QString& currentFamily, const QPoint& position,
               int sourceWidth) {
    currentFamily_ = currentFamily;
    search_->clear();
    rebuild({});
    showPreview(currentFamily);
    resize(std::max(700, sourceWidth), 390);
    move(position);
    show();
    raise();
    search_->setFocus();
  }

  std::function<void(const QString&)> accepted;

private:
  void rebuild(const QString& filter) {
    list_->clear();
    for (const QString& family : families_) {
      if (!filter.isEmpty() &&
          !family.contains(filter, Qt::CaseInsensitive)) {
        continue;
      }
      auto* item = new QListWidgetItem(family);
      QFont font(family);
      font.setPointSize(11);
      item->setFont(font);
      item->setSizeHint(QSize(item->sizeHint().width(), 30));
      list_->addItem(item);
      if (family.compare(currentFamily_, Qt::CaseInsensitive) == 0)
        list_->setCurrentItem(item);
    }
  }

  void showPreview(const QString& family) {
    if (family.trimmed().isEmpty()) return;
    familyLabel_->setText(family);
    QFont heading = familyLabel_->font();
    heading.setBold(true);
    heading.setPointSize(13);
    familyLabel_->setFont(heading);
    QFont large(family);
    large.setPointSize(28);
    specimen_->setFont(large);
    QFont sample(family);
    sample.setPointSize(16);
    alphabet_->setFont(sample);
  }

  FontSearchEdit* search_;
  FontFamilyList* list_;
  QLabel* familyLabel_;
  QLabel* specimen_;
  QLabel* alphabet_;
  QStringList families_;
  QString currentFamily_;
};

class PreviewFontCombo final : public QComboBox {
public:
  explicit PreviewFontCombo(QWidget* parent)
      : QComboBox(parent), popup_(new FontPreviewPopup(this)) {
    popup_->accepted = [this](const QString& family) {
      const int index = findText(family, Qt::MatchExactly);
      if (index >= 0) setCurrentIndex(index);
      else setEditText(family);
    };
  }

  void setPopupFamilies(const QStringList& families) {
    popup_->setFamilies(families);
  }

  void showPopup() override {
    popup_->openFor(currentText(), mapToGlobal(QPoint(0, height())),
                    std::max(width(), 700));
  }

  void hidePopup() override { popup_->hide(); }

private:
  FontPreviewPopup* popup_;
};

} // namespace

W_OBJECT_IMPL(FontPickerWidget)

FontPickerWidget::FontPickerWidget(QWidget* parent) : QWidget(parent) {
  setupUi();
  updateFontList();
}

FontPickerWidget::~FontPickerWidget() {}

void FontPickerWidget::setupUi() {
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  fontCombo_ = new PreviewFontCombo(this);
  fontCombo_->setEditable(true);
  fontCombo_->setInsertPolicy(QComboBox::NoInsert);
  fontCombo_->setMinimumContentsLength(22);
  fontCombo_->setMinimumHeight(28);
  fontCombo_->setAccessibleName(QStringLiteral("Font family"));
  fontCombo_->setAccessibleDescription(
      QStringLiteral("Search, preview and select a font family"));
  if (fontCombo_->lineEdit()) {
    fontCombo_->lineEdit()->setPlaceholderText(QStringLiteral("Search fonts…"));
    fontCombo_->lineEdit()->setClearButtonEnabled(true);
  }

  const auto& theme = ArtifactCore::currentDCCTheme();
  QPalette palette = fontCombo_->palette();
  palette.setColor(QPalette::Base, QColor(theme.secondaryBackgroundColor));
  palette.setColor(QPalette::Text, QColor(theme.textColor));
  palette.setColor(QPalette::Button, QColor(theme.secondaryBackgroundColor));
  palette.setColor(QPalette::ButtonText, QColor(theme.textColor));
  palette.setColor(QPalette::Highlight, QColor(theme.accentColor));
  palette.setColor(QPalette::HighlightedText, QColor(theme.backgroundColor));
  fontCombo_->setPalette(palette);
  layout->addWidget(fontCombo_, 1);

  connect(fontCombo_, &QComboBox::currentTextChanged, this,
          [this](const QString& text) {
            ArtifactCore::globalEventBus().post<FontChangedEvent>(
                FontChangedEvent{text});
          });
}

void FontPickerWidget::updateFontList() {
  QStringList families = ArtifactCore::FontManager::availableFamilies();
  families.sort(Qt::CaseInsensitive);
  fontCombo_->clear();
  fontCombo_->addItems(families);
  if (auto* combo = dynamic_cast<PreviewFontCombo*>(fontCombo_))
    combo->setPopupFamilies(families);
}

void FontPickerWidget::setCurrentFont(const QString& family) {
  const int index = fontCombo_->findText(family, Qt::MatchExactly);
  if (index >= 0) fontCombo_->setCurrentIndex(index);
  else fontCombo_->setEditText(family);
  if (fontCombo_->lineEdit()) {
    QFont previewFont(family);
    previewFont.setPointSize(fontCombo_->font().pointSize());
    fontCombo_->lineEdit()->setFont(previewFont);
  }
}

QString FontPickerWidget::currentFont() const {
  return fontCombo_->currentText();
}

} // namespace Artifact

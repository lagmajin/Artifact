module;
#include <utility>
#include <QWidget>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QStringList>
#include <QFontDatabase>
#include <wobjectimpl.h>

module Artifact.Widgets.FontPicker;

import Artifact.Event.Types;
import Event.Bus;

namespace Artifact {

W_OBJECT_IMPL(FontPickerWidget)

FontPickerWidget::FontPickerWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    updateFontList();
}

FontPickerWidget::~FontPickerWidget() {
}

void FontPickerWidget::setupUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    fontCombo_ = new QComboBox(this);
    fontCombo_->setEditable(true);
    fontCombo_->setInsertPolicy(QComboBox::NoInsert);
    
    // フォントのプレビューをコンボボックス内で表示するための設定（将来用）
    // fontCombo_->setItemDelegate(new FontDelegate(this));

    layout->addWidget(fontCombo_, 1);

    connect(fontCombo_, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        Q_EMIT fontChanged(text);
        ArtifactCore::globalEventBus().post<FontChangedEvent>(FontChangedEvent{text});
    });
}

void FontPickerWidget::updateFontList() {
    QStringList families = ArtifactCore::FontManager::availableFamilies();
    fontCombo_->clear();
    fontCombo_->addItems(families);
}

void FontPickerWidget::setCurrentFont(const QString& family) {
    int index = fontCombo_->findText(family, Qt::MatchExactly);
    if (index >= 0) {
        fontCombo_->setCurrentIndex(index);
    } else {
        fontCombo_->setEditText(family);
    }
}

QString FontPickerWidget::currentFont() const {
    return fontCombo_->currentText();
}

} // namespace Artifact

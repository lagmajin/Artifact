module;
#include <utility>
#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFrame>
#include <QMouseEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QPalette>
#include <wobjectimpl.h>
#include <Widgets/Dialog/ArtifactDialogButtons.hpp>

module Artifact.Widgets.PrecomposeDialog;

import Widgets.Utils.CSS;

namespace Artifact {

W_OBJECT_IMPL(PrecomposeDialog)

// ─────────────────────────────────────────────────────────────────────────────
// Impl
// ─────────────────────────────────────────────────────────────────────────────
class PrecomposeDialog::Impl {
public:
    QLineEdit*    nameEdit             = nullptr;
    QListWidget*  layerListWidget      = nullptr;
    QLabel*       layerCountLabel      = nullptr;
    QRadioButton* moveSelectedRadio    = nullptr;
    QRadioButton* moveAllAttribsRadio  = nullptr;
    QCheckBox*    openNewCompCheck     = nullptr;
    QCheckBox*    addAdjLayerCheck     = nullptr;
    QCheckBox*    matchDurationCheck   = nullptr;

    int totalLayerCount = 0;
    QPoint dragPos;
    bool   dragging = false;

    void updateLayerCountLabel(int selectedCount)
    {
        if (!layerCountLabel) return;
        layerCountLabel->setText(
            QString(u8"%1 レイヤーを選択中 / 全 %2 レイヤー")
                .arg(selectedCount)
                .arg(totalLayerCount));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
PrecomposeDialog::PrecomposeDialog(QWidget* parent)
    : QDialog(parent), impl_(new Impl())
{
    setWindowTitle(u8"プリコンポーズ");
    setFixedSize(440, 560);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_NoChildEventsForParent);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(48);
    {
        QPalette pal = header->palette();
        pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
        header->setAutoFillBackground(true);
        header->setPalette(pal);
    }
    auto* hLay = new QHBoxLayout(header);
    hLay->setContentsMargins(15, 0, 10, 0);
    auto* titleLbl = new QLabel(u8"プリコンポーズ", header);
    {
        QPalette pal = titleLbl->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
        titleLbl->setPalette(pal);
    }
    auto* closeBtn = new QPushButton(u8"×", header);
    closeBtn->setFixedSize(30, 30);
    {
        QPalette pal = closeBtn->palette();
        pal.setColor(QPalette::Button, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
        pal.setColor(QPalette::ButtonText, QColor(ArtifactCore::currentDCCTheme().textColor));
        closeBtn->setPalette(pal);
    }
    hLay->addWidget(titleLbl);
    hLay->addStretch();
    hLay->addWidget(closeBtn);
    root->addWidget(header);

    // ── Body ──────────────────────────────────────────────────────────────
    auto* body = new QWidget(this);
    auto* bLay = new QVBoxLayout(body);
    bLay->setContentsMargins(20, 16, 20, 8);
    bLay->setSpacing(10);
    root->addWidget(body, 1);

    const auto makeSeparator = [&]() -> QFrame* {
        auto* sep = new QFrame(body);
        sep->setFrameShape(QFrame::HLine);
        sep->setFixedHeight(1);
        return sep;
    };

    // ── 新規コンポジション名 ──────────────────────────────────────────────
    {
        auto* row = new QWidget(body);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        auto* lbl = new QLabel(u8"新規コンポジション名", row);
        {
            QPalette pal = lbl->palette();
            pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
            lbl->setPalette(pal);
        }
        lbl->setFixedWidth(145);
        impl_->nameEdit = new QLineEdit(u8"プリコンプ 1", row);
        rl->addWidget(lbl);
        rl->addWidget(impl_->nameEdit, 1);
        bLay->addWidget(row);
    }

    bLay->addWidget(makeSeparator());

    // ── 選択中のレイヤーリスト ────────────────────────────────────────────
    {
        auto* secLbl = new QLabel(u8"選択中のレイヤー", body);
        {
            QPalette pal = secLbl->palette();
            pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
            secLbl->setPalette(pal);
        }
        bLay->addWidget(secLbl);

        impl_->layerListWidget = new QListWidget(body);
        impl_->layerListWidget->setFixedHeight(110);
        {
            QPalette pal = impl_->layerListWidget->palette();
            pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
            pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
            pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
            impl_->layerListWidget->setPalette(pal);
        }
        impl_->layerListWidget->setSelectionMode(QAbstractItemView::NoSelection);
        bLay->addWidget(impl_->layerListWidget);

        // Layer count
        impl_->layerCountLabel = new QLabel(body);
        impl_->layerCountLabel->setAlignment(Qt::AlignRight);
        {
            QPalette pal = impl_->layerCountLabel->palette();
            pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
            impl_->layerCountLabel->setPalette(pal);
        }
        impl_->updateLayerCountLabel(0);
        bLay->addWidget(impl_->layerCountLabel);
    }

    bLay->addWidget(makeSeparator());

    // ── 配置オプション ────────────────────────────────────────────────────
    {
        auto* secLbl = new QLabel(u8"配置オプション", body);
        {
            QPalette pal = secLbl->palette();
            pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
            secLbl->setPalette(pal);
        }
        bLay->addWidget(secLbl);

        auto* group = new QButtonGroup(this);

        // Radio 1: 選択したレイヤーのみ
        {
            auto* radioWidget = new QWidget(body);
            radioWidget->setAutoFillBackground(true);
            auto* rLay = new QVBoxLayout(radioWidget);
            rLay->setContentsMargins(10, 8, 10, 8);
            rLay->setSpacing(2);
            impl_->moveSelectedRadio = new QRadioButton(
                u8"選択したレイヤーのみを新規コンポジションに移動する", radioWidget);
            impl_->moveSelectedRadio->setChecked(true);
            {
                QPalette pal = impl_->moveSelectedRadio->palette();
                pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
                impl_->moveSelectedRadio->setPalette(pal);
            }
            auto* subLbl = new QLabel(
                u8"選択レイヤーをプリコンプに移動。他のレイヤーはそのまま残ります。", radioWidget);
            {
                QPalette pal = subLbl->palette();
                pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
                subLbl->setPalette(pal);
            }
            subLbl->setWordWrap(true);
            rLay->addWidget(impl_->moveSelectedRadio);
            rLay->addWidget(subLbl);
            group->addButton(impl_->moveSelectedRadio, 0);
            bLay->addWidget(radioWidget);
        }

        // Radio 2: すべての属性
        {
            auto* radioWidget = new QWidget(body);
            radioWidget->setAutoFillBackground(true);
            auto* rLay = new QVBoxLayout(radioWidget);
            rLay->setContentsMargins(10, 8, 10, 8);
            rLay->setSpacing(2);
            impl_->moveAllAttribsRadio = new QRadioButton(
                u8"すべての属性を新規コンポジションに移動する", radioWidget);
            impl_->moveAllAttribsRadio->setStyleSheet(
                "");
            auto* subLbl = new QLabel(
                u8"トランスフォームなどの属性もプリコンプに引き継がれます。", radioWidget);
            {
                QPalette pal = subLbl->palette();
                pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
                subLbl->setPalette(pal);
            }
            subLbl->setWordWrap(true);
            rLay->addWidget(impl_->moveAllAttribsRadio);
            rLay->addWidget(subLbl);
            group->addButton(impl_->moveAllAttribsRadio, 1);
            bLay->addWidget(radioWidget);
        }
    }

    bLay->addWidget(makeSeparator());

    // ── チェックボックス ──────────────────────────────────────────────────
    {
        impl_->openNewCompCheck = new QCheckBox(u8"新規コンポジションを開く", body);
        impl_->openNewCompCheck->setChecked(true);
        bLay->addWidget(impl_->openNewCompCheck);

        impl_->addAdjLayerCheck = new QCheckBox(u8"調整レイヤーとして追加", body);
        impl_->addAdjLayerCheck->setChecked(false);
        impl_->addAdjLayerCheck->setEnabled(false); // greyed out by default
        bLay->addWidget(impl_->addAdjLayerCheck);

        impl_->matchDurationCheck = new QCheckBox(u8"コンポジションのデュレーションをワークエリアに合わせる", body);
        impl_->matchDurationCheck->setChecked(true);
        bLay->addWidget(impl_->matchDurationCheck);
    }

    bLay->addStretch();

    // ── Footer ────────────────────────────────────────────────────────────
    auto* footer = new QWidget(this);
    auto* fLay = new QHBoxLayout(footer);
    fLay->setContentsMargins(15, 10, 15, 10);

    const DialogButtonRow buttons = createWindowsDialogButtonRow(footer, QStringLiteral("OK"), QStringLiteral("キャンセル"));
    auto* okBtn = buttons.okButton;
    auto* cancelBtn = buttons.cancelButton;
    okBtn->setFixedSize(80, 28);
    cancelBtn->setFixedSize(80, 28);
    fLay->addStretch();
    fLay->addWidget(buttons.widget);
    root->addWidget(footer);

    // ── Connections ───────────────────────────────────────────────────────
    QObject::connect(closeBtn,  &QPushButton::clicked,    this, &QDialog::reject);
    QObject::connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // Enable adjustment layer checkbox only when "move all" is chosen
    QObject::connect(impl_->moveAllAttribsRadio, &QRadioButton::toggled, this,
                     [this](bool checked) {
        if (impl_->addAdjLayerCheck) impl_->addAdjLayerCheck->setEnabled(checked);
    });
}

PrecomposeDialog::~PrecomposeDialog()
{
    delete impl_;
}

// ── Public interface ──────────────────────────────────────────────────────────
void PrecomposeDialog::setSelectedLayerNames(const QStringList& names)
{
    if (!impl_->layerListWidget) return;
    impl_->layerListWidget->clear();
    for (const auto& name : names) {
        auto* item = new QListWidgetItem(u8"🔲 " + name);
        impl_->layerListWidget->addItem(item);
    }
    impl_->updateLayerCountLabel(names.size());
}

void PrecomposeDialog::setTotalLayerCount(int total)
{
    impl_->totalLayerCount = total;
    const int selected = impl_->layerListWidget ? impl_->layerListWidget->count() : 0;
    impl_->updateLayerCountLabel(selected);
}

QString PrecomposeDialog::newCompositionName()  const { return impl_->nameEdit          ? impl_->nameEdit->text()            : QString(); }
bool    PrecomposeDialog::moveSelectedOnly()     const { return impl_->moveSelectedRadio ? impl_->moveSelectedRadio->isChecked() : true; }
bool    PrecomposeDialog::openNewComposition()   const { return impl_->openNewCompCheck  ? impl_->openNewCompCheck->isChecked()  : true; }
bool    PrecomposeDialog::addAsAdjustmentLayer() const { return impl_->addAdjLayerCheck  ? impl_->addAdjLayerCheck->isChecked()  : false; }
bool    PrecomposeDialog::matchWorkspaceDuration() const { return impl_->matchDurationCheck ? impl_->matchDurationCheck->isChecked() : true; }

// ── Drag ──────────────────────────────────────────────────────────────────────
void PrecomposeDialog::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        impl_->dragPos  = e->globalPosition().toPoint() - frameGeometry().topLeft();
        impl_->dragging = true;
        e->accept();
        return;
    }
    QDialog::mousePressEvent(e);
}

void PrecomposeDialog::mouseReleaseEvent(QMouseEvent* e)
{
    if (impl_->dragging && e->button() == Qt::LeftButton) {
        impl_->dragging = false;
        e->accept();
        return;
    }
    QDialog::mouseReleaseEvent(e);
}

void PrecomposeDialog::mouseMoveEvent(QMouseEvent* e)
{
    if (impl_->dragging && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPosition().toPoint() - impl_->dragPos);
        e->accept();
        return;
    }
    QDialog::mouseMoveEvent(e);
}

void PrecomposeDialog::showEvent(QShowEvent* e)
{
    QDialog::showEvent(e);
    QWidget* anchor = parentWidget() ? parentWidget()->window() : QApplication::activeWindow();
    QPoint pos;
    if (anchor) {
        pos = anchor->mapToGlobal(anchor->rect().center()) - QPoint(width() / 2, height() / 2);
    } else {
        pos = QGuiApplication::primaryScreen()->availableGeometry().center()
              - QPoint(width() / 2, height() / 2);
    }
    move(pos);
}

} // namespace Artifact

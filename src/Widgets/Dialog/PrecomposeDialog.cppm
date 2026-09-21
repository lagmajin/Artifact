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
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QRect>
#include <QSizePolicy>
#include <QStringList>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QFont>
#include <QFontMetrics>
#include <QColor>
#include <QVariant>
#include <QPolygonF>
#include <QIcon>
#include <wobjectimpl.h>

module Artifact.Widgets.PrecomposeDialog;

import Core.ArtifactMath;
import Artifact.Widgets.DialogButtons;
import Widgets.Utils.CSS;
import Translation.Manager;

namespace Artifact {

W_OBJECT_IMPL(PrecomposeDialog)

namespace {

// Scope the Fusion controls to this dialog: inherited application styles must
// not introduce unrelated gradients or blue native selection backgrounds.
class PrecomposeStyle final : public QProxyStyle {
public:
    PrecomposeStyle() : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))) {}
    void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                       QPainter* painter, const QWidget* widget = nullptr) const override {
        if (element == PE_IndicatorRadioButton || element == PE_IndicatorCheckBox) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            const bool enabled = option->state & State_Enabled;
            const bool checked = option->state & State_On;
            const QColor ink = enabled ? QColor(195, 201, 207) : QColor(87, 94, 101);
            const QColor amber(242, 182, 72);
            const QRectF box = QRectF(option->rect).adjusted(2, 2, -2, -2);
            painter->setPen(QPen(checked && enabled ? amber : ink, 2));
            painter->setBrush(Qt::NoBrush);
            if (element == PE_IndicatorRadioButton) {
                painter->drawEllipse(box);
                if (checked) {
                    painter->setPen(Qt::NoPen);
                    painter->setBrush(enabled ? amber : ink);
                    painter->drawEllipse(box.adjusted(5, 5, -5, -5));
                }
            } else {
                if (checked) painter->setBrush(enabled ? amber : ink);
                painter->drawRoundedRect(box, 2, 2);
                if (checked) {
                    painter->setPen(QPen(QColor(32, 36, 40), 2.3));
                    const QPointF a(box.left()+box.width()*0.2, box.center().y());
                    const QPointF b(box.left()+box.width()*0.43, box.bottom()-box.height()*0.23);
                    const QPointF c(box.right()-box.width()*0.15, box.top()+box.height()*0.23);
                    painter->drawLine(a, b);
                    painter->drawLine(b, c);
                }
            }
            painter->restore();
            return;
        }
        if (element == PE_PanelButtonCommand) {
            painter->save();
            const bool primary = widget && widget->property("precomposePrimary").toBool();
            QColor fill = primary ? QColor(242, 182, 72) : QColor(38, 42, 47);
            if (option->state & State_Sunken) fill = fill.darker(112);
            else if (option->state & State_MouseOver) fill = fill.lighter(108);
            painter->setPen(QPen(primary ? QColor(251, 201, 108) : QColor(78, 85, 92), 1));
            painter->setBrush(fill);
            painter->drawRoundedRect(option->rect.adjusted(1, 1, -1, -1), 4, 4);
            painter->restore();
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
    int pixelMetric(PixelMetric metric, const QStyleOption* option = nullptr,
                    const QWidget* widget = nullptr) const override {
        if (metric == PM_IndicatorWidth || metric == PM_IndicatorHeight ||
            metric == PM_ExclusiveIndicatorWidth || metric == PM_ExclusiveIndicatorHeight)
            return 26;
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
};

class PrecomposePreviewWidget final : public QWidget {
public:
    explicit PrecomposePreviewWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(240, 230);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAccessibleName(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.preview_a11y"), QStringLiteral("プリコンポーズ予定プレビュー")));
        setAccessibleDescription(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.preview_a11y_desc"), QStringLiteral("選択レイヤーの構成を簡略表示")));
    }

    void setLayerNames(const QStringList& names)
    {
        if (layerNames_ == names) return;
        layerNames_ = names;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QColor background(23, 27, 31);
        const QColor surface(43, 48, 54);
        const QColor border(69, 76, 83);
        const QColor text(220, 225, 230);

        painter.fillRect(rect(), background.darker(112));
        painter.setPen(QPen(border, 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));

        const int visibleCount =
            ArtifactCore::artifactMin(3, static_cast<int>(layerNames_.size()));
        const int cardWidth = ArtifactCore::artifactMax(80, width() - 54);
        for (int i = visibleCount - 1; i >= 0; --i) {
            const int y = 36 + i * 53;
            QRect card(22, y, cardWidth, 48);
            QColor cardColor = surface;
            cardColor.setAlpha(225);
            painter.setBrush(cardColor);
            painter.setPen(QPen(border.lighter(112), 1));
            QPolygonF plane;
            plane << QPointF(card.left()+16, card.top())
                  << QPointF(card.right()+10, card.top()+6)
                  << QPointF(card.right()-6, card.bottom())
                  << QPointF(card.left(), card.bottom()-6);
            painter.drawPolygon(plane);
            painter.setPen(text);
            painter.drawText(card.adjusted(10, 0, -10, 0),
                             Qt::AlignCenter,
                             painter.fontMetrics().elidedText(layerNames_.value(i), Qt::ElideRight, card.width()-20));
        }
        if (visibleCount == 0) {
            painter.setPen(text.darker(150));
            painter.drawText(rect(), Qt::AlignCenter, TranslationManager::instance().tr(QStringLiteral("dialog.precompose.no_selection"), QStringLiteral("選択レイヤーなし")));
        }
    }

private:
    QStringList layerNames_;
};

} // namespace

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
    PrecomposePreviewWidget* previewWidget = nullptr;
    QLabel*       previewLayerCount    = nullptr;

    int totalLayerCount = 0;
    QPoint dragPos;
    bool   dragging = false;

    void updateLayerCountLabel(int selectedCount)
    {
        if (!layerCountLabel) return;
        layerCountLabel->setText(
            QString(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.selection_summary"), QStringLiteral("%1 レイヤーを選択中 / 全 %2 レイヤー")))
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
    setWindowTitle(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.title"), QStringLiteral("プリコンポーズ")));
    setAccessibleName(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.dialog_a11y"), QStringLiteral("プリコンポーズダイアログ")));
    setAccessibleDescription(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.dialog_a11y_desc"), QStringLiteral("選択したレイヤーを新規コンポジションへまとめる設定")));
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_NoChildEventsForParent);
    QFont dialogFont = font();
    dialogFont.setPointSizeF(11.0);
    setFont(dialogFont);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(64);
    {
        QPalette pal = header->palette();
        pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
        header->setAutoFillBackground(true);
        header->setPalette(pal);
    }
    auto* hLay = new QHBoxLayout(header);
    hLay->setContentsMargins(26, 0, 20, 0);
    auto* titleLbl = new QLabel(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.title"), QStringLiteral("プリコンポーズ")), header);
    {
        QPalette pal = titleLbl->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
        titleLbl->setPalette(pal);
    }
    auto* closeBtn = new QPushButton(u8"×", header);
    QFont titleFont = dialogFont;
    titleFont.setPointSizeF(17.0);
    titleLbl->setFont(titleFont);
    closeBtn->setFont(titleFont);
    closeBtn->setFlat(true);
    closeBtn->setAccessibleName(TranslationManager::instance().tr(QStringLiteral("dialog.button.close"), QStringLiteral("閉じる")));
    closeBtn->setAccessibleDescription(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.close_a11y_desc"), QStringLiteral("プリコンポーズダイアログを閉じる")));
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
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(26, 22, 26, 24);
    bodyLayout->setSpacing(22);
    auto* mainBody = new QWidget(body);
    auto* bLay = new QVBoxLayout(mainBody);
    bLay->setContentsMargins(0, 0, 0, 0);
    bLay->setSpacing(16);
    mainBody->setMinimumWidth(560);
    bodyLayout->addWidget(mainBody, 7);
    auto* divider = new QFrame(body);
    divider->setFrameShape(QFrame::VLine);
    bodyLayout->addWidget(divider);

    auto* previewPane = new QWidget(body);
    previewPane->setMinimumWidth(240);
    auto* previewLayout = new QVBoxLayout(previewPane);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(14);
    auto* previewTitle = new QLabel(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.preview_title"), QStringLiteral("プリコンポーズ予定")), previewPane);
    {
        QPalette pal = previewTitle->palette();
        pal.setColor(QPalette::WindowText,
                     QColor(ArtifactCore::currentDCCTheme().accentColor));
        previewTitle->setPalette(pal);
    }
    impl_->previewWidget = new PrecomposePreviewWidget(previewPane);
    auto* previewResolution = new QLabel(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.preview_output"), QStringLiteral("出力設定: 現在のコンポジション")), previewPane);
    auto* previewDuration = new QLabel(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.preview_duration"), QStringLiteral("期間: ワークエリア")), previewPane);
    impl_->previewLayerCount = new QLabel(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.zero_layers"), QStringLiteral("0 レイヤー")), previewPane);
    auto* previewUpdateHint = new QLabel(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.preview_update_hint"), QStringLiteral("選択変更時に更新")), previewPane);
    {
        QPalette pal = previewUpdateHint->palette();
        pal.setColor(QPalette::WindowText,
                     QColor(ArtifactCore::currentDCCTheme().textColor).darker(155));
        previewUpdateHint->setPalette(pal);
    }
    previewLayout->addWidget(previewTitle);
    previewLayout->addWidget(impl_->previewWidget);
    previewLayout->addWidget(previewResolution);
    previewLayout->addWidget(previewDuration);
    previewLayout->addWidget(impl_->previewLayerCount);
    previewLayout->addSpacing(8);
    previewLayout->addWidget(previewUpdateHint);
    previewLayout->addStretch();
    bodyLayout->addWidget(previewPane, 3);
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
        auto* rl  = new QVBoxLayout(row);
        rl->setSpacing(10);
        rl->setContentsMargins(0, 0, 0, 0);
        auto* lbl = new QLabel(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.name_label"), QStringLiteral("新規コンポジション名")), row);
        {
            QPalette pal = lbl->palette();
            pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
            lbl->setPalette(pal);
        }
        lbl->setMinimumWidth(100);
        impl_->nameEdit = new QLineEdit(u8"プリコンプ 1", row);
        impl_->nameEdit->setMinimumHeight(44);
        impl_->nameEdit->setTextMargins(12, 0, 12, 0);
        lbl->setBuddy(impl_->nameEdit);
        impl_->nameEdit->setAccessibleName(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.name_label"), QStringLiteral("新規コンポジション名")));
        impl_->nameEdit->setAccessibleDescription(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.name_a11y_desc"), QStringLiteral("作成する新規コンポジションの名前")));
        rl->addWidget(lbl);
        rl->addWidget(impl_->nameEdit, 1);
        bLay->addWidget(row);
    }

    bLay->addWidget(makeSeparator());

    // ── 選択中のレイヤーリスト ────────────────────────────────────────────
    {
        auto* secLbl = new QLabel(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.selected_layers"), QStringLiteral("選択中のレイヤー")), body);
        {
            QPalette pal = secLbl->palette();
            pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
            secLbl->setPalette(pal);
        }
        auto* listHeading = new QHBoxLayout();
        listHeading->addWidget(secLbl);
        bLay->addLayout(listHeading);

        impl_->layerListWidget = new QListWidget(body);
        impl_->layerListWidget->setAccessibleName(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.selected_layers"), QStringLiteral("選択中のレイヤー")));
        impl_->layerListWidget->setAccessibleDescription(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.selected_layers_desc"), QStringLiteral("新規コンポジションへ移動するレイヤーの一覧")));
        impl_->layerListWidget->setFixedHeight(132);
        impl_->layerListWidget->setIconSize(QSize(20, 20));
        impl_->layerListWidget->setSpacing(2);
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
        listHeading->addStretch();
        listHeading->addWidget(impl_->layerCountLabel);
    }

    bLay->addWidget(makeSeparator());

    // ── 配置オプション ────────────────────────────────────────────────────
    {
        auto* secLbl = new QLabel(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.placement_options"), QStringLiteral("配置オプション")), body);
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
                TranslationManager::instance().tr(QStringLiteral("dialog.precompose.move_selected_radio"), QStringLiteral("選択したレイヤーのみを新規コンポジションに移動する")), radioWidget);
            impl_->moveSelectedRadio->setAccessibleName(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.move_selected_a11y"), QStringLiteral("選択したレイヤーのみを移動")));
            impl_->moveSelectedRadio->setAccessibleDescription(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.move_selected_a11y_desc"), QStringLiteral("選択したレイヤーだけを新規コンポジションへ移動")));
            impl_->moveSelectedRadio->setChecked(true);
            {
                QPalette pal = impl_->moveSelectedRadio->palette();
                pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
                impl_->moveSelectedRadio->setPalette(pal);
            }
            auto* subLbl = new QLabel(
                TranslationManager::instance().tr(QStringLiteral("dialog.precompose.move_selected_hint"), QStringLiteral("選択レイヤーをプリコンプに移動。他のレイヤーはそのまま残ります。")), radioWidget);
            {
                QPalette pal = subLbl->palette();
                pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
                subLbl->setPalette(pal);
            }
            subLbl->setWordWrap(true);
            subLbl->setObjectName(QStringLiteral("precomposeOptionHint"));
            subLbl->setContentsMargins(34, 0, 0, 0);
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
                TranslationManager::instance().tr(QStringLiteral("dialog.precompose.move_all_radio"), QStringLiteral("すべての属性を新規コンポジションに移動する")), radioWidget);
            impl_->moveAllAttribsRadio->setAccessibleName(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.move_all_a11y"), QStringLiteral("すべての属性を移動")));
            impl_->moveAllAttribsRadio->setAccessibleDescription(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.move_all_a11y_desc"), QStringLiteral("すべての属性を新規コンポジションへ移動")));
            auto* subLbl = new QLabel(
                TranslationManager::instance().tr(QStringLiteral("dialog.precompose.move_all_hint"), QStringLiteral("トランスフォームなどの属性もプリコンプに引き継がれます。")), radioWidget);
            {
                QPalette pal = subLbl->palette();
                pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
                subLbl->setPalette(pal);
            }
            subLbl->setWordWrap(true);
            subLbl->setObjectName(QStringLiteral("precomposeOptionHint"));
            subLbl->setContentsMargins(34, 0, 0, 0);
            rLay->addWidget(impl_->moveAllAttribsRadio);
            rLay->addWidget(subLbl);
            group->addButton(impl_->moveAllAttribsRadio, 1);
            bLay->addWidget(radioWidget);
        }
    }

    bLay->addWidget(makeSeparator());

    // ── チェックボックス ──────────────────────────────────────────────────
    {
        impl_->openNewCompCheck = new QCheckBox(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.open_new_comp"), QStringLiteral("新規コンポジションを開く")), body);
        impl_->openNewCompCheck->setAccessibleName(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.open_new_comp"), QStringLiteral("新規コンポジションを開く")));
        impl_->openNewCompCheck->setAccessibleDescription(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.open_new_comp_desc"), QStringLiteral("作成後に新規コンポジションを開く")));
        impl_->openNewCompCheck->setChecked(true);
        bLay->addWidget(impl_->openNewCompCheck);

        impl_->addAdjLayerCheck = new QCheckBox(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.add_adjustment"), QStringLiteral("調整レイヤーとして追加")), body);
        impl_->addAdjLayerCheck->setAccessibleName(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.add_adjustment"), QStringLiteral("調整レイヤーとして追加")));
        impl_->addAdjLayerCheck->setAccessibleDescription(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.add_adjustment_desc"), QStringLiteral("新規コンポジションを調整レイヤーとして追加")));
        impl_->addAdjLayerCheck->setChecked(false);
        impl_->addAdjLayerCheck->setEnabled(false); // greyed out by default
        bLay->addWidget(impl_->addAdjLayerCheck);

        impl_->matchDurationCheck = new QCheckBox(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.match_duration"), QStringLiteral("コンポジションのデュレーションをワークエリアに合わせる")), body);
        impl_->matchDurationCheck->setAccessibleName(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.match_duration_a11y"), QStringLiteral("ワークエリアにデュレーションを合わせる")));
        impl_->matchDurationCheck->setAccessibleDescription(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.match_duration_desc"), QStringLiteral("新規コンポジションのデュレーションをワークエリアに合わせる")));
        impl_->matchDurationCheck->setChecked(true);
        bLay->addWidget(impl_->matchDurationCheck);
    }

    bLay->addStretch();

    // ── Footer ────────────────────────────────────────────────────────────
    auto* footer = new QWidget(this);
    auto* fLay = new QHBoxLayout(footer);
    fLay->setContentsMargins(15, 10, 15, 10);

    const DialogButtonRow buttons = createDialogButtonRow(footer, QStringLiteral("OK"), TranslationManager::instance().tr(QStringLiteral("dialog.button.cancel"), QStringLiteral("キャンセル")));
    auto* okBtn = buttons.okButton;
    auto* cancelBtn = buttons.cancelButton;
    okBtn->setAccessibleName(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.create"), QStringLiteral("作成")));
    okBtn->setAccessibleDescription(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.create_a11y_desc"), QStringLiteral("設定した内容でプリコンポーズを作成")));
    cancelBtn->setAccessibleName(TranslationManager::instance().tr(QStringLiteral("dialog.button.cancel"), QStringLiteral("キャンセル")));
    cancelBtn->setAccessibleDescription(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.cancel_a11y_desc"), QStringLiteral("プリコンポーズをキャンセル")));
    okBtn->setFixedSize(80, 28);
    cancelBtn->setFixedSize(80, 28);
    fLay->addStretch();
    fLay->addWidget(buttons.widget);
    bLay->addWidget(footer);

    // One opaque charcoal surface for containers and labels; only input fields,
    // the layer list and the schematic preview have deliberate inset tones.
    auto* dialogStyle = new PrecomposeStyle();
    dialogStyle->setParent(this);
    QPalette unified;
    unified.setColor(QPalette::Window, QColor(35, 39, 43));
    unified.setColor(QPalette::WindowText, QColor(224, 229, 234));
    unified.setColor(QPalette::Base, QColor(29, 33, 37));
    unified.setColor(QPalette::AlternateBase, QColor(35, 39, 43));
    unified.setColor(QPalette::Text, QColor(224, 229, 234));
    unified.setColor(QPalette::Button, QColor(38, 42, 47));
    unified.setColor(QPalette::ButtonText, QColor(224, 229, 234));
    unified.setColor(QPalette::Highlight, QColor(242, 182, 72));
    unified.setColor(QPalette::HighlightedText, QColor(26, 30, 34));
    unified.setColor(QPalette::Mid, QColor(65, 71, 77));
    unified.setColor(QPalette::Dark, QColor(65, 71, 77));
    unified.setColor(QPalette::Light, QColor(65, 71, 77));
    unified.setColor(QPalette::Disabled, QPalette::WindowText, QColor(105, 113, 121));
    unified.setColor(QPalette::Disabled, QPalette::Text, QColor(105, 113, 121));
    setStyle(dialogStyle);
    setPalette(unified);
    setAutoFillBackground(true);
    const auto children = findChildren<QWidget*>();
    for (auto* child : children) {
        child->setStyle(dialogStyle);
        child->setPalette(unified);
        if (qobject_cast<QLabel*>(child)) child->setAutoFillBackground(false);
    }
    okBtn->setProperty("precomposePrimary", true);
    QPalette primaryPalette = unified;
    primaryPalette.setColor(QPalette::ButtonText, QColor(26, 30, 34));
    okBtn->setPalette(primaryPalette);
    okBtn->setFixedSize(152, 44);
    cancelBtn->setFixedSize(144, 44);
    QPalette muted = unified;
    muted.setColor(QPalette::WindowText, QColor(157, 165, 173));
    previewUpdateHint->setPalette(muted);
    impl_->layerCountLabel->setPalette(muted);
    for (auto* child : children)
        if (child->objectName() == QStringLiteral("precomposeOptionHint"))
            child->setPalette(muted);

    // ── Connections ───────────────────────────────────────────────────────
    QObject::connect(closeBtn,  &QPushButton::clicked,    this, &QDialog::reject);
    QObject::connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // Enable adjustment layer checkbox only when "move all" is chosen
    QObject::connect(impl_->moveAllAttribsRadio, &QRadioButton::toggled, this,
                     [this](bool checked) {
        if (impl_->addAdjLayerCheck) impl_->addAdjLayerCheck->setEnabled(checked);
    });

    adjustSize();
    setMinimumSize(size());
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
        auto* item = new QListWidgetItem(
            style()->standardIcon(QStyle::SP_FileIcon), name);
        item->setSizeHint(QSize(0, 38));
        impl_->layerListWidget->addItem(item);
    }
    if (impl_->previewWidget) impl_->previewWidget->setLayerNames(names);
    if (impl_->previewLayerCount) {
        impl_->previewLayerCount->setText(
            QString(TranslationManager::instance().tr(QStringLiteral("dialog.precompose.layer_count"), QStringLiteral("%1 レイヤー"))).arg(names.size()));
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

module;
#include <utility>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QDialog>
#include <QLineEdit>
#include <QFormLayout>
#include <QComboBox>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QGuiApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QTimer>
#include <QIcon>
#include <QPixmap>
#include <QColor>
#include <QPen>
#include <QPainter>
#include <wobjectimpl.h>
#include <QApplication>
#include <QCheckBox>
#include <QFrame>
#include <QScrollArea>
#include <QSet>
#include <QHash>
#include <QPalette>
module Artifact.Widgets.CreatePlaneLayerDialog;

import std;
import Widgets.Dialog.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Widgets.Utils.CSS;
import Widgets.EditableLabel;
import DragSpinBox;
import Utils.String.UniString;
import Color.Float;
import FloatColorPickerDialog;
import Artifact.Widgets.Dialog.FloatColorPickerHooks;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Composition.Settings;
import Artifact.Layers.SolidImage;

namespace {

QString nearestNamedColor(const QColor& color);

void updateColorButtonPreview(QPushButton* button, const QColor& color)
{
    if (!button) return;
    QPixmap pix(button->size().isEmpty() ? QSize(40, 24) : button->size());
    pix.fill(Qt::transparent);
    {
        QPainter painter(&pix);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(85, 85, 85), 1));
        painter.setBrush(color);
        painter.drawRoundedRect(pix.rect().adjusted(1, 1, -2, -2), 3, 3);
    }
    button->setIcon(QIcon(pix));
    button->setIconSize(pix.size());
    button->setToolTip(QStringLiteral("%1 (%2)").arg(nearestNamedColor(color), color.name()));
    button->setText({});
}

void updateHexFromColor(QLineEdit* edit, const QColor& color)
{
    edit->setText(QString("#%1%2%3%4")
        .arg(color.red(),   2, 16, QChar('0'))
        .arg(color.green(), 2, 16, QChar('0'))
        .arg(color.blue(),  2, 16, QChar('0'))
        .arg(color.alpha(), 2, 16, QChar('0'))
        .toLower());
}

// Builds a section header widget (title label + horizontal rule).
QWidget* makeSectionHeader(const QString& title, QWidget* parent)
{
    auto* w = new QWidget(parent);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 8, 0, 2);
    layout->setSpacing(2);
    auto* label = new QLabel(title, w);
    {
        QPalette pal = label->palette();
        pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor).darker(130));
        label->setPalette(pal);
    }
    auto* line = new QFrame(w);
    line->setFrameShape(QFrame::HLine);
    layout->addWidget(label);
    layout->addWidget(line);
    return w;
}

// Builds a right-label + control row.
QWidget* makeRow(QWidget* parent, const QString& labelText, int labelWidth,
                 QWidget* ctrl, QWidget* extra = nullptr)
{
    auto* row = new QWidget(parent);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(20, 2, 4, 2);
    auto* lbl = new QLabel(labelText, row);
    lbl->setFixedWidth(labelWidth);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(lbl);
    lay->addWidget(ctrl, 1);
    if (extra) lay->addWidget(extra);
    return row;
}

QString nearestNamedColor(const QColor& color)
{
    const auto names = QColor::colorNames();
    if (names.isEmpty()) {
        return color.name();
    }

    const int targetR = color.red();
    const int targetG = color.green();
    const int targetB = color.blue();
    int bestDistance = std::numeric_limits<int>::max();
    QString bestName = color.name();
    for (const QString& name : names) {
        const QColor namedColor(name);
        const int dr = namedColor.red() - targetR;
        const int dg = namedColor.green() - targetG;
        const int db = namedColor.blue() - targetB;
        const int distance = dr * dr + dg * dg + db * db;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestName = name;
        }
    }

    const QString lower = bestName.toLower();
    static const QHash<QString, QString> kLocalized = {
        {QStringLiteral("white"), QStringLiteral("ホワイト")},
        {QStringLiteral("black"), QStringLiteral("ブラック")},
        {QStringLiteral("red"), QStringLiteral("レッド")},
        {QStringLiteral("green"), QStringLiteral("グリーン")},
        {QStringLiteral("blue"), QStringLiteral("ブルー")},
        {QStringLiteral("yellow"), QStringLiteral("イエロー")},
        {QStringLiteral("cyan"), QStringLiteral("シアン")},
        {QStringLiteral("magenta"), QStringLiteral("マゼンタ")},
        {QStringLiteral("gray"), QStringLiteral("グレー")},
        {QStringLiteral("lightgray"), QStringLiteral("ライトグレー")},
        {QStringLiteral("darkgray"), QStringLiteral("ダークグレー")},
        {QStringLiteral("orange"), QStringLiteral("オレンジ")},
        {QStringLiteral("purple"), QStringLiteral("パープル")},
        {QStringLiteral("pink"), QStringLiteral("ピンク")},
        {QStringLiteral("brown"), QStringLiteral("ブラウン")}
    };
    return kLocalized.value(lower, bestName);
}

QString makeUniqueSequentialName(QString baseName, const QSet<QString>& occupied)
{
    baseName = baseName.trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("平面 1");
    }
    if (!occupied.contains(baseName)) {
        return baseName;
    }

    QString prefix = baseName;
    int startNumber = 2;
    int end = baseName.size();
    while (end > 0 && baseName.at(end - 1).isDigit()) {
        --end;
    }
    if (end < baseName.size()) {
        int start = end;
        while (start > 0 && baseName.at(start - 1).isSpace()) {
            --start;
        }
        bool ok = false;
        const int current = baseName.mid(end).toInt(&ok);
        if (ok) {
            prefix = baseName.left(start);
            startNumber = current + 1;
        }
    }
    if (prefix == baseName && !prefix.endsWith(QLatin1Char(' '))) {
        prefix += QLatin1Char(' ');
    }
    for (int index = startNumber; index < 10000; ++index) {
        const QString candidate = prefix + QString::number(index);
        if (!occupied.contains(candidate)) {
            return candidate;
        }
    }
    return baseName;
}

QSet<QString> currentLayerNames()
{
    QSet<QString> names;
    if (auto* service = Artifact::ArtifactProjectService::instance()) {
        if (auto comp = service->currentComposition().lock()) {
            for (const auto& layer : comp->allLayer()) {
                if (!layer) {
                    continue;
                }
                const QString name = layer->layerName().trimmed();
                if (!name.isEmpty()) {
                    names.insert(name);
                }
            }
        }
    }
    return names;
}

QString suggestedPlaneLayerName(const QColor& color)
{
    const QString base = QStringLiteral("%1 平面 1").arg(nearestNamedColor(color));
    return makeUniqueSequentialName(base, currentLayerNames());
}

} // namespace

namespace Artifact {

using namespace ArtifactCore;
using namespace ArtifactWidgets;

// ─────────────────────────────────────────────────────────────────────────────
//  PlaneLayerSettingPage
// ─────────────────────────────────────────────────────────────────────────────

W_OBJECT_IMPL(PlaneLayerSettingPage)

class PlaneLayerSettingPage::Impl {
public:
    Impl() = default;
    ~Impl() = default;

    DragSpinBox*  widthSpinBox    = nullptr;
    DragSpinBox*  heightSpinBox   = nullptr;
    QPushButton*  lockButton      = nullptr;
    bool          aspectLocked    = false;
    double        lockedRatio     = 16.0 / 9.0;

    QComboBox*    unitCombo         = nullptr;
    QComboBox*    pixelAspectCombo  = nullptr;
    QPushButton*  bgColorButton     = nullptr;
    QPushButton*  matchCompButton   = nullptr;
    QLineEdit*    hexColorEdit      = nullptr;
    QCheckBox*    fitToCompCheck    = nullptr;

    QColor bgColor = QColor(255, 255, 255, 255);
};

PlaneLayerSettingPage::PlaneLayerSettingPage(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    // ── Widgets ──────────────────────────────────────────────────────────────

    impl_->widthSpinBox = new DragSpinBox(this);
    impl_->widthSpinBox->setRange(1, 16384);
    impl_->widthSpinBox->setValue(1920);

    impl_->heightSpinBox = new DragSpinBox(this);
    impl_->heightSpinBox->setRange(1, 16384);
    impl_->heightSpinBox->setValue(1080);

    impl_->lockButton = new QPushButton(this);
    impl_->lockButton->setText(u8"🔒");
    impl_->lockButton->setFixedSize(20, 20);
    impl_->lockButton->setCheckable(true);
    impl_->lockButton->setChecked(false);
    impl_->lockButton->setToolTip(u8"縦横比をロック");
    impl_->lockButton->setStyleSheet(
        "QPushButton { background: transparent; border: none; font-size: 12px; }"
        "QPushButton:checked { color: #ff9900; }"
    );

    impl_->unitCombo = new QComboBox(this);
    impl_->unitCombo->addItem(u8"ピクセル");
    impl_->unitCombo->addItem(u8"ポイント");
    impl_->unitCombo->addItem(u8"パーセント");
    impl_->unitCombo->addItem(u8"ミリメートル");

    impl_->pixelAspectCombo = new QComboBox(this);
    impl_->pixelAspectCombo->addItem(u8"正方形ピクセル");
    impl_->pixelAspectCombo->addItem(u8"D1/DV NTSC");
    impl_->pixelAspectCombo->addItem(u8"D1/DV PAL");
    impl_->pixelAspectCombo->addItem(u8"D1/DV NTSC ワイドスクリーン");
    impl_->pixelAspectCombo->addItem(u8"D1/DV PAL ワイドスクリーン");
    impl_->pixelAspectCombo->addItem(u8"アナモフィック 2:1");

    impl_->bgColorButton = new QPushButton(this);
    impl_->bgColorButton->setFixedSize(40, 24);
    updateColorButtonPreview(impl_->bgColorButton, impl_->bgColor);

    impl_->hexColorEdit = new QLineEdit(this);
    impl_->hexColorEdit->setFixedWidth(140);
    updateHexFromColor(impl_->hexColorEdit, impl_->bgColor);

    impl_->matchCompButton = new QPushButton(u8"コンポジションサイズを使用", this);

    impl_->fitToCompCheck = new QCheckBox(u8"平面をコンポジションサイズに合わせる", this);
    impl_->fitToCompCheck->setChecked(false);

    // ── Layout ───────────────────────────────────────────────────────────────

    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // サイズ section
    vbox->addWidget(makeSectionHeader(u8"サイズ", this));

    // 幅 row: spinbox + "px" label + lock button
    {
        auto* ctrl = new QWidget(this);
        auto* ctrlLay = new QHBoxLayout(ctrl);
        ctrlLay->setContentsMargins(0, 0, 0, 0);
        ctrlLay->setSpacing(4);
        ctrlLay->addWidget(impl_->widthSpinBox, 1);
        ctrlLay->addWidget(new QLabel("px", this));
        ctrlLay->addWidget(impl_->lockButton);
        vbox->addWidget(makeRow(this, u8"幅", 100, ctrl));
    }

    // 高さ row: spinbox + "px" label
    {
        auto* ctrl = new QWidget(this);
        auto* ctrlLay = new QHBoxLayout(ctrl);
        ctrlLay->setContentsMargins(0, 0, 0, 0);
        ctrlLay->setSpacing(4);
        ctrlLay->addWidget(impl_->heightSpinBox, 1);
        ctrlLay->addWidget(new QLabel("px", this));
        vbox->addWidget(makeRow(this, u8"高さ", 100, ctrl));
    }

    // 単位 row
    vbox->addWidget(makeRow(this, u8"単位", 100, impl_->unitCombo));

    // コンポジションサイズを使用 button (indented to align with controls)
    {
        auto* btnRow = new QWidget(this);
        auto* btnRowLay = new QHBoxLayout(btnRow);
        btnRowLay->setContentsMargins(20, 2, 4, 2);
        btnRowLay->addSpacing(104);
        btnRowLay->addWidget(impl_->matchCompButton, 1);
        vbox->addWidget(btnRow);
    }

    // ピクセル縦横比 row
    vbox->addWidget(makeRow(this, u8"ピクセル縦横比", 100, impl_->pixelAspectCombo));

    // カラー section
    vbox->addWidget(makeSectionHeader(u8"カラー", this));

    // カラー row: color swatch + hex edit
    {
        auto* ctrl = new QWidget(this);
        auto* ctrlLay = new QHBoxLayout(ctrl);
        ctrlLay->setContentsMargins(0, 0, 0, 0);
        ctrlLay->setSpacing(6);
        ctrlLay->addWidget(impl_->bgColorButton);
        ctrlLay->addWidget(impl_->hexColorEdit);
        ctrlLay->addStretch();
        vbox->addWidget(makeRow(this, u8"カラー", 100, ctrl));
    }

    // fitToCompCheck row (indented to align with controls)
    {
        auto* checkRow = new QWidget(this);
        auto* checkRowLay = new QHBoxLayout(checkRow);
        checkRowLay->setContentsMargins(20, 2, 4, 2);
        checkRowLay->addSpacing(104);
        checkRowLay->addWidget(impl_->fitToCompCheck, 1);
        vbox->addWidget(checkRow);
    }

    vbox->addStretch();

    // ── Connections ───────────────────────────────────────────────────────────

    // Aspect-ratio lock: store ratio when engaged, enforce on spin changes.
    QObject::connect(impl_->lockButton, &QPushButton::toggled, this, [this](bool checked) {
        impl_->aspectLocked = checked;
        if (checked) {
            int w = impl_->widthSpinBox->value();
            int h = impl_->heightSpinBox->value();
            impl_->lockedRatio = (h > 0) ? static_cast<double>(w) / h : 16.0 / 9.0;
        }
    });

    QObject::connect(impl_->widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, [this](int w) {
        if (impl_->aspectLocked) {
            impl_->heightSpinBox->blockSignals(true);
            impl_->heightSpinBox->setValue(
                static_cast<int>(std::round(w / impl_->lockedRatio)));
            impl_->heightSpinBox->blockSignals(false);
        }
    });

    QObject::connect(impl_->heightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, [this](int h) {
        if (impl_->aspectLocked) {
            impl_->widthSpinBox->blockSignals(true);
            impl_->widthSpinBox->setValue(
                static_cast<int>(std::round(h * impl_->lockedRatio)));
            impl_->widthSpinBox->blockSignals(false);
        }
    });

    QObject::connect(impl_->bgColorButton, &QPushButton::clicked, this, [this]() {
        FloatColorPicker picker(this);
        Artifact::installFloatColorPickerSliderJump(&picker);
        picker.setWindowTitle(QStringLiteral("平面のカラーを選択"));
        picker.setColor(FloatColor(
            impl_->bgColor.redF(),
            impl_->bgColor.greenF(),
            impl_->bgColor.blueF(),
            impl_->bgColor.alphaF()));

        if (picker.exec() != QDialog::Accepted) return;

        const FloatColor picked = picker.getColor();
        const QColor c = QColor::fromRgbF(picked.r(), picked.g(), picked.b(), picked.a());
        if (!c.isValid()) return;

        impl_->bgColor = c;
        updateColorButtonPreview(impl_->bgColorButton, c);
        updateHexFromColor(impl_->hexColorEdit, c);
        Q_EMIT colorChanged(suggestedPlaneLayerName(c));
    });

    QObject::connect(impl_->hexColorEdit, &QLineEdit::editingFinished, this, [this]() {
        QColor c(impl_->hexColorEdit->text().trimmed());
        if (c.isValid()) {
            impl_->bgColor = c;
            updateColorButtonPreview(impl_->bgColorButton, c);
            Q_EMIT colorChanged(suggestedPlaneLayerName(c));
        }
    });

    QObject::connect(impl_->matchCompButton, &QPushButton::clicked,
                     this, &PlaneLayerSettingPage::resizeCompositionSize);
}

PlaneLayerSettingPage::~PlaneLayerSettingPage()
{
    delete impl_;
}

void PlaneLayerSettingPage::setDefaultFocus() {}

void PlaneLayerSettingPage::spouitMode() {}

void PlaneLayerSettingPage::resizeCompositionSize()
{
    auto service = ArtifactProjectService::instance();
    if (service) {
        auto compWeak = service->currentComposition();
        if (auto comp = compWeak.lock()) {
            auto size = comp->settings().compositionSize();
            if (size.width() > 0 && size.height() > 0) {
                impl_->widthSpinBox->setValue(size.width());
                impl_->heightSpinBox->setValue(size.height());
                return;
            }
        }
    }
    impl_->widthSpinBox->setValue(1920);
    impl_->heightSpinBox->setValue(1080);
}

void PlaneLayerSettingPage::setInitialParams(int p_width, int p_height, const FloatColor& color)
{
    impl_->widthSpinBox->setValue(p_width);
    impl_->heightSpinBox->setValue(p_height);
    QColor c;
    c.setRgbF(color.r(), color.g(), color.b(), color.a());
    impl_->bgColor = c;
    updateColorButtonPreview(impl_->bgColorButton, c);
    updateHexFromColor(impl_->hexColorEdit, c);
}

ArtifactSolidLayerInitParams PlaneLayerSettingPage::getInitParams(const QString& name) const
{
    ArtifactSolidLayerInitParams params(name);
    params.setWidth(impl_->widthSpinBox->value());
    params.setHeight(impl_->heightSpinBox->value());
    QColor c = impl_->bgColor;
    params.setColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
    return params;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Shared helper: build the standard dialog chrome (header + scroll + footer)
// ─────────────────────────────────────────────────────────────────────────────

namespace {

struct DialogChrome {
    QVBoxLayout* mainLayout   = nullptr;
    QScrollArea* scrollArea   = nullptr;
    QWidget*     scrollContent = nullptr;
    QVBoxLayout* scrollLayout  = nullptr;
    QPushButton* closeButton   = nullptr;
    QDialogButtonBox* buttonBox = nullptr;
};

DialogChrome buildDialogChrome(QDialog* dlg)
{
    DialogChrome chrome;

    dlg->setWindowFlags(dlg->windowFlags() | Qt::Dialog | Qt::FramelessWindowHint);
    dlg->setAttribute(Qt::WA_NoChildEventsForParent);

    chrome.mainLayout = new QVBoxLayout(dlg);
    chrome.mainLayout->setContentsMargins(0, 0, 0, 0);
    chrome.mainLayout->setSpacing(0);

    // Header bar
    auto* header = new QWidget(dlg);
    header->setFixedHeight(50);
    header->setStyleSheet("background-color: #2a2a2a;");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(15, 0, 10, 0);

    auto* titleLabel = new QLabel(u8"平面設定", header);
    titleLabel->setStyleSheet("color: #e0e0e0; font-size: 13px; font-weight: bold;");

    chrome.closeButton = new QPushButton(u8"×", header);
    chrome.closeButton->setFixedSize(30, 30);
    chrome.closeButton->setStyleSheet(
        "QPushButton { background: transparent; color: #aaaaaa; border: none; font-size: 18px; }"
        "QPushButton:hover { color: #ff4444; }"
    );

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(chrome.closeButton);
    chrome.mainLayout->addWidget(header);

    // Scroll area
    chrome.scrollArea = new QScrollArea(dlg);
    chrome.scrollArea->setWidgetResizable(true);
    chrome.scrollArea->setFrameShape(QFrame::NoFrame);
    chrome.scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    chrome.scrollContent = new QWidget(chrome.scrollArea);
    chrome.scrollLayout = new QVBoxLayout(chrome.scrollContent);
    chrome.scrollLayout->setContentsMargins(20, 10, 20, 10);
    chrome.scrollLayout->setSpacing(0);

    chrome.scrollArea->setWidget(chrome.scrollContent);
    chrome.mainLayout->addWidget(chrome.scrollArea, 1);

    // Footer
    auto* footer = new QWidget(dlg);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(15, 10, 15, 10);

    // Create button box as signal source only (not in visual layout)
    chrome.buttonBox = new QDialogButtonBox(dlg);
    chrome.buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    chrome.buttonBox->hide();

    // Visual buttons in explicit Windows order: [OK] [キャンセル]
    auto* okBtn = new QPushButton("OK", footer);
    okBtn->setFixedSize(80, 28);
    auto* cancelBtn = new QPushButton(u8"キャンセル", footer);
    cancelBtn->setFixedSize(80, 28);
    QObject::connect(okBtn,     &QPushButton::clicked,
                     chrome.buttonBox->button(QDialogButtonBox::Ok),     &QPushButton::click);
    QObject::connect(cancelBtn, &QPushButton::clicked,
                     chrome.buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::click);
    footerLayout->addStretch();
    footerLayout->addWidget(okBtn);
    footerLayout->addWidget(cancelBtn);
    chrome.mainLayout->addWidget(footer);

    return chrome;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  CreateSolidLayerSettingDialog
// ─────────────────────────────────────────────────────────────────────────────

W_OBJECT_IMPL(CreateSolidLayerSettingDialog)

class CreateSolidLayerSettingDialog::Impl
{
public:
    EditableLabel*    nameEditableLabel = nullptr;
    PlaneLayerSettingPage* settingPage  = nullptr;
    QDialogButtonBox* dialogButtonBox   = nullptr;
    QPoint m_dragPosition;
    bool   m_isDragging = false;
};

CreateSolidLayerSettingDialog::CreateSolidLayerSettingDialog(QWidget* parent)
    : QDialog(parent), impl_(new Impl())
{
    setWindowTitle(u8"平面設定");
    setFixedSize(520, 500);

    auto chrome = buildDialogChrome(this);
    impl_->dialogButtonBox = chrome.buttonBox;

    // 名前 section
    chrome.scrollLayout->addWidget(makeSectionHeader(u8"名前", chrome.scrollContent));
    {
        impl_->nameEditableLabel = new EditableLabel();
        impl_->nameEditableLabel->setText(suggestedPlaneLayerName(QColor(255, 255, 255, 255)));

        auto* nameRow = new QWidget(chrome.scrollContent);
        auto* nameRowLay = new QHBoxLayout(nameRow);
        nameRowLay->setContentsMargins(20, 2, 4, 2);
        auto* nameLabel = new QLabel(u8"名前", nameRow);
        nameLabel->setFixedWidth(100);
        nameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        nameRowLay->addWidget(nameLabel);
        nameRowLay->addWidget(impl_->nameEditableLabel, 1);
        chrome.scrollLayout->addWidget(nameRow);
    }

    // Settings page (サイズ + カラー sections)
    auto* settingPage = impl_->settingPage = new PlaneLayerSettingPage(chrome.scrollContent);
    settingPage->resizeCompositionSize();
    chrome.scrollLayout->addWidget(settingPage);
    chrome.scrollLayout->addStretch();

    // Connections
    QObject::connect(chrome.closeButton, &QPushButton::clicked, this, &QDialog::reject);

    QObject::connect(chrome.buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (impl_->nameEditableLabel) impl_->nameEditableLabel->finishEdit();
        QString name = impl_->nameEditableLabel
            ? impl_->nameEditableLabel->text()
            : suggestedPlaneLayerName(QColor(255, 255, 255, 255));
        ArtifactSolidLayerInitParams params = impl_->settingPage->getInitParams(name);
        Q_EMIT submit(params);
        accept();
    });
    QObject::connect(chrome.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QObject::connect(settingPage, &PlaneLayerSettingPage::colorChanged,
                     this, [this](const QString& name) {
        if (impl_->nameEditableLabel)
            impl_->nameEditableLabel->setText(name);
    });
}

CreateSolidLayerSettingDialog::~CreateSolidLayerSettingDialog()
{
    delete impl_;
}

void CreateSolidLayerSettingDialog::keyPressEvent(QKeyEvent* event)
{
    QDialog::keyPressEvent(event);
}

void CreateSolidLayerSettingDialog::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        impl_->m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        impl_->m_isDragging = true;
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void CreateSolidLayerSettingDialog::mouseReleaseEvent(QMouseEvent* event)
{
    if (impl_->m_isDragging && event->button() == Qt::LeftButton) {
        impl_->m_isDragging = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

void CreateSolidLayerSettingDialog::mouseMoveEvent(QMouseEvent* event)
{
    if (impl_->m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - impl_->m_dragPosition);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void CreateSolidLayerSettingDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    QWidget* anchor = parentWidget() ? parentWidget()->window() : QApplication::activeWindow();
    QPoint endPos;
    if (anchor) {
        endPos = anchor->mapToGlobal(anchor->rect().center())
                 - QPoint(width() / 2, height() / 2);
    } else {
        endPos = QGuiApplication::primaryScreen()->availableGeometry().center()
                 - QPoint(width() / 2, height() / 2);
    }
    move(endPos);
}

void CreateSolidLayerSettingDialog::showAnimated()
{
    show();
}

// ─────────────────────────────────────────────────────────────────────────────
//  EditPlaneLayerSettingDialog
// ─────────────────────────────────────────────────────────────────────────────

W_OBJECT_IMPL(EditPlaneLayerSettingDialog)

class EditPlaneLayerSettingDialog::Impl
{
public:
    EditableLabel*         nameEditableLabel = nullptr;
    PlaneLayerSettingPage* settingPage       = nullptr;
    QDialogButtonBox*      dialogButtonBox   = nullptr;
    ArtifactSolidImageLayer* targetLayer     = nullptr;
    QPoint m_dragPosition;
    bool   m_isDragging = false;
};

EditPlaneLayerSettingDialog::EditPlaneLayerSettingDialog(QWidget* parent)
    : QDialog(parent), impl_(new Impl())
{
    setWindowTitle(u8"平面設定");
    setFixedSize(520, 500);

    auto chrome = buildDialogChrome(this);
    impl_->dialogButtonBox = chrome.buttonBox;

    // 名前 section
    chrome.scrollLayout->addWidget(makeSectionHeader(u8"名前", chrome.scrollContent));
    {
        impl_->nameEditableLabel = new EditableLabel();
        impl_->nameEditableLabel->setText(suggestedPlaneLayerName(QColor(255, 255, 255, 255)));

        auto* nameRow = new QWidget(chrome.scrollContent);
        auto* nameRowLay = new QHBoxLayout(nameRow);
        nameRowLay->setContentsMargins(20, 2, 4, 2);
        auto* nameLabel = new QLabel(u8"名前", nameRow);
        nameLabel->setFixedWidth(100);
        nameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        nameRowLay->addWidget(nameLabel);
        nameRowLay->addWidget(impl_->nameEditableLabel, 1);
        chrome.scrollLayout->addWidget(nameRow);
    }

    // Settings page (サイズ + カラー sections)
    auto* settingPage = impl_->settingPage = new PlaneLayerSettingPage(chrome.scrollContent);
    chrome.scrollLayout->addWidget(settingPage);
    chrome.scrollLayout->addStretch();

    // Connections
    QObject::connect(chrome.closeButton, &QPushButton::clicked, this, &QDialog::reject);

    QObject::connect(chrome.buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (impl_->nameEditableLabel) impl_->nameEditableLabel->finishEdit();
        QString name = impl_->nameEditableLabel
            ? impl_->nameEditableLabel->text()
            : suggestedPlaneLayerName(QColor(255, 255, 255, 255));
        ArtifactSolidLayerInitParams params = impl_->settingPage->getInitParams(name);
        if (impl_->targetLayer) {
            impl_->targetLayer->setLayerName(name);
            impl_->targetLayer->setSize(params.width(), params.height());
            impl_->targetLayer->setColor(params.color());
        }
        accept();
    });
    QObject::connect(chrome.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QObject::connect(settingPage, &PlaneLayerSettingPage::colorChanged,
                     this, [this](const QString& name) {
        if (impl_->nameEditableLabel)
            impl_->nameEditableLabel->setText(name);
    });
}

EditPlaneLayerSettingDialog::~EditPlaneLayerSettingDialog()
{
    delete impl_;
}

void EditPlaneLayerSettingDialog::keyPressEvent(QKeyEvent* event)
{
    QDialog::keyPressEvent(event);
}

void EditPlaneLayerSettingDialog::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        impl_->m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        impl_->m_isDragging = true;
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void EditPlaneLayerSettingDialog::mouseReleaseEvent(QMouseEvent* event)
{
    if (impl_->m_isDragging && event->button() == Qt::LeftButton) {
        impl_->m_isDragging = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

void EditPlaneLayerSettingDialog::mouseMoveEvent(QMouseEvent* event)
{
    if (impl_->m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - impl_->m_dragPosition);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void EditPlaneLayerSettingDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    QWidget* anchor = parentWidget() ? parentWidget()->window() : QApplication::activeWindow();
    QPoint endPos;
    if (anchor) {
        endPos = anchor->mapToGlobal(anchor->rect().center())
                 - QPoint(width() / 2, height() / 2);
    } else {
        endPos = QGuiApplication::primaryScreen()->availableGeometry().center()
                 - QPoint(width() / 2, height() / 2);
    }
    move(endPos);
}

void EditPlaneLayerSettingDialog::showAnimated()
{
    show();
}

void EditPlaneLayerSettingDialog::setupEdit(std::shared_ptr<ArtifactSolidImageLayer> layer)
{
    if (!layer) return;
    impl_->targetLayer = layer.get();
    if (impl_->nameEditableLabel)
        impl_->nameEditableLabel->setText(layer->layerName());
    if (impl_->settingPage) {
        auto size = layer->sourceSize();
        impl_->settingPage->setInitialParams(size.width, size.height, layer->color());
    }
}

};

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
#include <QDoubleSpinBox>
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
#include <QSize>
#include <Layer/ArtifactSolidGradientUtil.hpp>
module Artifact.Widgets.CreatePlaneLayerDialog;

import std;
import Widgets.Dialog.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Menu.Layer;
import Widgets.Utils.CSS;
import Widgets.EditableLabel;
import DragSpinBox;
import Utils.String.UniString;
import Color.Float;
import FloatColorPickerDialog;
import Artifact.Service.Project;
import Artifact.Composition.Abstract;
import Composition.Settings;
import Artifact.Layers.SolidImage;

namespace {

class DialogCloseButton final : public QPushButton {
public:
  explicit DialogCloseButton(QWidget* parent = nullptr) : QPushButton(u8"×", parent) {
    setFixedSize(30, 30);
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
  }
protected:
  bool event(QEvent* event) override {
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave) {
      update();
    }
    return QPushButton::event(event);
  }
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    QColor textCol = underMouse() ? QColor(0xff, 0x44, 0x44) : QColor(0xaa, 0xaa, 0xaa);
    painter.setPen(textCol);
    QFont font = this->font();
    font.setPointSize(18);
    painter.setFont(font);
    painter.drawText(rect(), Qt::AlignCenter, text());
  }
};

class AspectLockButton final : public QPushButton {
public:
  explicit AspectLockButton(QWidget* parent = nullptr) : QPushButton(u8"🔒", parent) {
    setFixedSize(20, 20);
    setCheckable(true);
    setChecked(false);
    setToolTip(u8"縦横比をロック");
  }
protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    QColor textCol = isChecked() ? QColor(0xff, 0x99, 0x00) : QColor(0xaa, 0xaa, 0xaa);
    painter.setPen(textCol);
    QFont font = this->font();
    font.setPointSize(12);
    painter.setFont(font);
    painter.drawText(rect(), Qt::AlignCenter, text());
  }
};

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

struct GradientPreset {
    QString name;
    Artifact::ArtifactSolidFillType fillType;
    ArtifactCore::FloatColor startColor;
    ArtifactCore::FloatColor endColor;
    float angleDegrees;
    bool reverse;
    float centerX;
    float centerY;
    float scale;
    float offset;
};

std::vector<GradientPreset> defaultGradientPresets()
{
    return {
        {u8"ホワイト", Artifact::ArtifactSolidFillType::Solid,
         ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f), ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f),
         0, false, 0.5f, 0.5f, 1.0f, 0.0f},
        {u8"ブラック", Artifact::ArtifactSolidFillType::Solid,
         ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f), ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f),
         0, false, 0.5f, 0.5f, 1.0f, 0.0f},
        {u8"サンセット", Artifact::ArtifactSolidFillType::LinearGradient,
         ArtifactCore::FloatColor(1.0f, 0.4f, 0.0f, 1.0f), ArtifactCore::FloatColor(1.0f, 0.8f, 0.2f, 1.0f),
         -90, false, 0.5f, 0.0f, 1.0f, 0.0f},
        {u8"オーシャン", Artifact::ArtifactSolidFillType::LinearGradient,
         ArtifactCore::FloatColor(0.0f, 0.3f, 0.8f, 1.0f), ArtifactCore::FloatColor(0.0f, 0.7f, 0.4f, 1.0f),
         90, false, 0.5f, 0.5f, 1.0f, 0.0f},
        {u8"フォレスト", Artifact::ArtifactSolidFillType::LinearGradient,
         ArtifactCore::FloatColor(0.1f, 0.5f, 0.1f, 1.0f), ArtifactCore::FloatColor(0.4f, 0.8f, 0.2f, 1.0f),
         135, false, 0.5f, 0.5f, 1.0f, 0.0f},
        {u8"ファイア", Artifact::ArtifactSolidFillType::LinearGradient,
         ArtifactCore::FloatColor(1.0f, 0.0f, 0.0f, 1.0f), ArtifactCore::FloatColor(1.0f, 0.6f, 0.0f, 1.0f),
         90, false, 0.5f, 0.5f, 1.0f, 0.0f},
        {u8"スポット", Artifact::ArtifactSolidFillType::RadialGradient,
         ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f), ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 1.0f),
         0, false, 0.5f, 0.5f, 0.5f, 0.0f},
        {u8"グロー", Artifact::ArtifactSolidFillType::RadialGradient,
         ArtifactCore::FloatColor(1.0f, 0.9f, 0.5f, 1.0f), ArtifactCore::FloatColor(1.0f, 0.3f, 0.0f, 1.0f),
         0, false, 0.5f, 0.5f, 1.0f, 0.0f},
        {u8"レインボー", Artifact::ArtifactSolidFillType::ConicalGradient,
         ArtifactCore::FloatColor(1.0f, 0.0f, 0.0f, 1.0f), ArtifactCore::FloatColor(0.0f, 0.0f, 1.0f, 1.0f),
         0, false, 0.5f, 0.5f, 1.0f, 0.0f},
    };
}

QPixmap renderGradientPreview(const GradientPreset& preset)
{
    const QImage img = ArtifactSolidGradientUtil::makeSolidGradientImage(
        QSize(40, 24),
        QColor::fromRgbF(preset.startColor.r(), preset.startColor.g(),
                         preset.startColor.b(), preset.startColor.a()),
        QColor::fromRgbF(preset.endColor.r(), preset.endColor.g(),
                         preset.endColor.b(), preset.endColor.a()),
        static_cast<int>(preset.fillType),
        preset.angleDegrees,
        preset.reverse,
        preset.centerX,
        preset.centerY,
        preset.scale,
        preset.offset);
    return QPixmap::fromImage(img);
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
    AspectLockButton*  lockButton      = nullptr;
    bool          aspectLocked    = false;
    double        lockedRatio     = 16.0 / 9.0;

    QComboBox*    unitCombo         = nullptr;
    QComboBox*    pixelAspectCombo  = nullptr;
    QPushButton*  bgColorButton     = nullptr;
    QWidget*      bgColorRow        = nullptr;
    QPushButton*  gradientStartColorButton = nullptr;
    QWidget*      gradientStartRow  = nullptr;
    QPushButton*  gradientEndColorButton = nullptr;
    QWidget*      gradientEndRow    = nullptr;
    QPushButton*  matchCompButton   = nullptr;
    QLineEdit*    hexColorEdit      = nullptr;
    QComboBox*    fillModeCombo     = nullptr;
    QDoubleSpinBox* gradientAngleSpin = nullptr;
    QWidget*      gradientAngleRow  = nullptr;
    QCheckBox*    gradientReverseCheck = nullptr;
    QWidget*      gradientReverseRow = nullptr;
    QDoubleSpinBox* gradientCenterXSpin = nullptr;
    QWidget*      gradientCenterXRow = nullptr;
    QDoubleSpinBox* gradientCenterYSpin = nullptr;
    QWidget*      gradientCenterYRow = nullptr;
    QDoubleSpinBox* gradientScaleSpin = nullptr;
    QWidget*      gradientScaleRow = nullptr;
    QDoubleSpinBox* gradientOffsetSpin = nullptr;
    QWidget*      gradientOffsetRow = nullptr;
    QCheckBox*    fitToCompCheck    = nullptr;
    QWidget*      presetBar         = nullptr;

    QColor bgColor = QColor(255, 255, 255, 255);
    QColor gradientStartColor = QColor(255, 255, 255, 255);
    QColor gradientEndColor = QColor(51, 51, 51, 255);
    ArtifactSolidFillType fillType = ArtifactSolidFillType::Solid;
    bool gradientReverse = false;
    float gradientCenterX = 0.5f;
    float gradientCenterY = 0.5f;
    float gradientScale = 1.0f;
    float gradientOffset = 0.0f;
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

    impl_->lockButton = new AspectLockButton(this);

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

    impl_->gradientStartColorButton = new QPushButton(this);
    impl_->gradientStartColorButton->setFixedSize(40, 24);
    updateColorButtonPreview(impl_->gradientStartColorButton, impl_->gradientStartColor);

    impl_->gradientEndColorButton = new QPushButton(this);
    impl_->gradientEndColorButton->setFixedSize(40, 24);
    updateColorButtonPreview(impl_->gradientEndColorButton, impl_->gradientEndColor);

    impl_->hexColorEdit = new QLineEdit(this);
    impl_->hexColorEdit->setFixedWidth(140);
    updateHexFromColor(impl_->hexColorEdit, impl_->bgColor);

    impl_->fillModeCombo = new QComboBox(this);
    impl_->fillModeCombo->addItem(QStringLiteral("単色"), static_cast<int>(ArtifactSolidFillType::Solid));
    impl_->fillModeCombo->addItem(QStringLiteral("線形グラデーション"), static_cast<int>(ArtifactSolidFillType::LinearGradient));
    impl_->fillModeCombo->addItem(QStringLiteral("放射状グラデーション"), static_cast<int>(ArtifactSolidFillType::RadialGradient));
    impl_->fillModeCombo->addItem(QStringLiteral("円錐グラデーション"), static_cast<int>(ArtifactSolidFillType::ConicalGradient));

    impl_->gradientAngleSpin = new QDoubleSpinBox(this);
    impl_->gradientAngleSpin->setRange(-360.0, 360.0);
    impl_->gradientAngleSpin->setDecimals(1);
    impl_->gradientAngleSpin->setSingleStep(15.0);
    impl_->gradientAngleSpin->setValue(90.0);

    impl_->gradientReverseCheck = new QCheckBox(u8"反転", this);

    impl_->gradientCenterXSpin = new QDoubleSpinBox(this);
    impl_->gradientCenterXSpin->setRange(0.0, 1.0);
    impl_->gradientCenterXSpin->setDecimals(3);
    impl_->gradientCenterXSpin->setSingleStep(0.05);
    impl_->gradientCenterXSpin->setValue(0.5);

    impl_->gradientCenterYSpin = new QDoubleSpinBox(this);
    impl_->gradientCenterYSpin->setRange(0.0, 1.0);
    impl_->gradientCenterYSpin->setDecimals(3);
    impl_->gradientCenterYSpin->setSingleStep(0.05);
    impl_->gradientCenterYSpin->setValue(0.5);

    impl_->gradientScaleSpin = new QDoubleSpinBox(this);
    impl_->gradientScaleSpin->setRange(0.01, 16.0);
    impl_->gradientScaleSpin->setDecimals(3);
    impl_->gradientScaleSpin->setSingleStep(0.1);
    impl_->gradientScaleSpin->setValue(1.0);

    impl_->gradientOffsetSpin = new QDoubleSpinBox(this);
    impl_->gradientOffsetSpin->setRange(-1.0, 1.0);
    impl_->gradientOffsetSpin->setDecimals(3);
    impl_->gradientOffsetSpin->setSingleStep(0.05);
    impl_->gradientOffsetSpin->setValue(0.0);

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
        impl_->bgColorRow = makeRow(this, u8"カラー", 100, ctrl);
        vbox->addWidget(impl_->bgColorRow);
    }

    vbox->addWidget(makeRow(this, u8"塗り", 100, impl_->fillModeCombo));

    // ── グラデーション プリセット ────────────────────────────────────────────
    {
        impl_->presetBar = new QWidget(this);
        auto* lay = new QHBoxLayout(impl_->presetBar);
        lay->setContentsMargins(20, 4, 4, 4);
        lay->setSpacing(3);
        lay->addSpacing(104);
        const auto presets = defaultGradientPresets();
        for (const auto& preset : presets) {
            auto* btn = new QPushButton(impl_->presetBar);
            btn->setFixedSize(40, 24);
            btn->setIcon(QIcon(renderGradientPreview(preset)));
            btn->setIconSize(QSize(40, 24));
            btn->setToolTip(preset.name);
            QObject::connect(btn, &QPushButton::clicked, this, [this, preset]() {
                setInitialGradientParams(preset.fillType, preset.startColor,
                                         preset.endColor, preset.angleDegrees,
                                         preset.reverse, preset.centerX,
                                         preset.centerY, preset.scale,
                                         preset.offset);
            });
            lay->addWidget(btn);
        }
        lay->addStretch();
        vbox->addWidget(impl_->presetBar);
    }

    {
        auto* ctrl = new QWidget(this);
        auto* ctrlLay = new QHBoxLayout(ctrl);
        ctrlLay->setContentsMargins(0, 0, 0, 0);
        ctrlLay->setSpacing(6);
        ctrlLay->addWidget(impl_->gradientStartColorButton);
        ctrlLay->addStretch();
        impl_->gradientStartRow = makeRow(this, u8"開始色", 100, ctrl);
        vbox->addWidget(impl_->gradientStartRow);
    }

    {
        auto* ctrl = new QWidget(this);
        auto* ctrlLay = new QHBoxLayout(ctrl);
        ctrlLay->setContentsMargins(0, 0, 0, 0);
        ctrlLay->setSpacing(6);
        ctrlLay->addWidget(impl_->gradientEndColorButton);
        ctrlLay->addStretch();
        impl_->gradientEndRow = makeRow(this, u8"終了色", 100, ctrl);
        vbox->addWidget(impl_->gradientEndRow);
    }

    {
        auto* ctrl = new QWidget(this);
        auto* ctrlLay = new QHBoxLayout(ctrl);
        ctrlLay->setContentsMargins(0, 0, 0, 0);
        ctrlLay->setSpacing(4);
        ctrlLay->addWidget(impl_->gradientAngleSpin, 1);
        ctrlLay->addWidget(new QLabel(QStringLiteral("deg"), this));
        impl_->gradientAngleRow = makeRow(this, u8"角度", 100, ctrl);
        vbox->addWidget(impl_->gradientAngleRow);
    }

    impl_->gradientReverseRow = makeRow(this, u8"反転", 100, impl_->gradientReverseCheck);
    vbox->addWidget(impl_->gradientReverseRow);

    {
        auto* ctrl = new QWidget(this);
        auto* ctrlLay = new QHBoxLayout(ctrl);
        ctrlLay->setContentsMargins(0, 0, 0, 0);
        ctrlLay->setSpacing(4);
        ctrlLay->addWidget(impl_->gradientCenterXSpin, 1);
        ctrlLay->addWidget(new QLabel(QStringLiteral("X"), this));
        impl_->gradientCenterXRow = makeRow(this, u8"中心X", 100, ctrl);
        vbox->addWidget(impl_->gradientCenterXRow);
    }

    {
        auto* ctrl = new QWidget(this);
        auto* ctrlLay = new QHBoxLayout(ctrl);
        ctrlLay->setContentsMargins(0, 0, 0, 0);
        ctrlLay->setSpacing(4);
        ctrlLay->addWidget(impl_->gradientCenterYSpin, 1);
        ctrlLay->addWidget(new QLabel(QStringLiteral("Y"), this));
        impl_->gradientCenterYRow = makeRow(this, u8"中心Y", 100, ctrl);
        vbox->addWidget(impl_->gradientCenterYRow);
    }

    {
        auto* ctrl = new QWidget(this);
        auto* ctrlLay = new QHBoxLayout(ctrl);
        ctrlLay->setContentsMargins(0, 0, 0, 0);
        ctrlLay->setSpacing(4);
        ctrlLay->addWidget(impl_->gradientScaleSpin, 1);
        ctrlLay->addWidget(new QLabel(QStringLiteral("x"), this));
        impl_->gradientScaleRow = makeRow(this, u8"拡大", 100, ctrl);
        vbox->addWidget(impl_->gradientScaleRow);
    }

    {
        auto* ctrl = new QWidget(this);
        auto* ctrlLay = new QHBoxLayout(ctrl);
        ctrlLay->setContentsMargins(0, 0, 0, 0);
        ctrlLay->setSpacing(4);
        ctrlLay->addWidget(impl_->gradientOffsetSpin, 1);
        ctrlLay->addWidget(new QLabel(QStringLiteral("span"), this));
        impl_->gradientOffsetRow = makeRow(this, u8"オフセット", 100, ctrl);
        vbox->addWidget(impl_->gradientOffsetRow);
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
        picker.setWindowTitle(QStringLiteral("平面のカラーを選択"));
        picker.setInitialColor(FloatColor(
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

    auto openPicker = [this](QPushButton* button, QColor* targetColor, const QString& title) {
        FloatColorPicker picker(this);
        picker.setWindowTitle(title);
        picker.setInitialColor(FloatColor(
            targetColor->redF(),
            targetColor->greenF(),
            targetColor->blueF(),
            targetColor->alphaF()));

        if (picker.exec() != QDialog::Accepted) return;

        const FloatColor picked = picker.getColor();
        const QColor c = QColor::fromRgbF(picked.r(), picked.g(), picked.b(), picked.a());
        if (!c.isValid()) return;
        *targetColor = c;
        updateColorButtonPreview(button, c);
    };

    QObject::connect(impl_->gradientStartColorButton, &QPushButton::clicked, this, [this, openPicker]() {
        openPicker(impl_->gradientStartColorButton, &impl_->gradientStartColor,
                   QStringLiteral("グラデーション開始色を選択"));
    });

    QObject::connect(impl_->gradientEndColorButton, &QPushButton::clicked, this, [this, openPicker]() {
        openPicker(impl_->gradientEndColorButton, &impl_->gradientEndColor,
                   QStringLiteral("グラデーション終了色を選択"));
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

    auto syncGradientUi = [this]() {
        const bool gradientEnabled = impl_->fillModeCombo->currentData().toInt() !=
                                     static_cast<int>(ArtifactSolidFillType::Solid);
        if (impl_->bgColorRow) impl_->bgColorRow->setVisible(!gradientEnabled);
        if (impl_->gradientStartRow) impl_->gradientStartRow->setVisible(gradientEnabled);
        if (impl_->gradientEndRow) impl_->gradientEndRow->setVisible(gradientEnabled);
        if (impl_->gradientAngleRow) impl_->gradientAngleRow->setVisible(gradientEnabled);
        if (impl_->gradientReverseRow) impl_->gradientReverseRow->setVisible(gradientEnabled);
        if (impl_->gradientCenterXRow) impl_->gradientCenterXRow->setVisible(gradientEnabled);
        if (impl_->gradientCenterYRow) impl_->gradientCenterYRow->setVisible(gradientEnabled);
        if (impl_->gradientScaleRow) impl_->gradientScaleRow->setVisible(gradientEnabled);
        if (impl_->gradientOffsetRow) impl_->gradientOffsetRow->setVisible(gradientEnabled);
    };
    QObject::connect(impl_->fillModeCombo, &QComboBox::currentIndexChanged, this,
                     [this, syncGradientUi](int) {
        impl_->fillType = static_cast<ArtifactSolidFillType>(impl_->fillModeCombo->currentData().toInt());
        syncGradientUi();
    });
    syncGradientUi();
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

void PlaneLayerSettingPage::setInitialGradientParams(const ArtifactSolidFillType fillType,
                                                     const FloatColor& startColor,
                                                     const FloatColor& endColor,
                                                     const float angleDegrees,
                                                     const bool reverse,
                                                     const float centerX,
                                                     const float centerY,
                                                     const float scale,
                                                     const float offset)
{
    impl_->fillType = fillType;
    impl_->gradientStartColor = QColor::fromRgbF(startColor.r(), startColor.g(), startColor.b(), startColor.a());
    impl_->gradientEndColor = QColor::fromRgbF(endColor.r(), endColor.g(), endColor.b(), endColor.a());
    impl_->gradientReverse = reverse;
    impl_->gradientCenterX = centerX;
    impl_->gradientCenterY = centerY;
    impl_->gradientScale = scale;
    impl_->gradientOffset = offset;
    updateColorButtonPreview(impl_->gradientStartColorButton, impl_->gradientStartColor);
    updateColorButtonPreview(impl_->gradientEndColorButton, impl_->gradientEndColor);
    const int index = impl_->fillModeCombo->findData(static_cast<int>(fillType));
    if (index >= 0) {
        impl_->fillModeCombo->setCurrentIndex(index);
    }
    impl_->gradientAngleSpin->setValue(angleDegrees);
    impl_->gradientReverseCheck->setChecked(reverse);
    impl_->gradientCenterXSpin->setValue(centerX);
    impl_->gradientCenterYSpin->setValue(centerY);
    impl_->gradientScaleSpin->setValue(scale);
    impl_->gradientOffsetSpin->setValue(offset);
}

ArtifactSolidLayerInitParams PlaneLayerSettingPage::getInitParams(const QString& name) const
{
    ArtifactSolidLayerInitParams params(name);
    params.setWidth(impl_->widthSpinBox->value());
    params.setHeight(impl_->heightSpinBox->value());
    QColor c = impl_->bgColor;
    params.setColor(FloatColor(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
    params.setFillType(impl_->fillType);
    params.setGradientStartColor(FloatColor(impl_->gradientStartColor.redF(),
                                            impl_->gradientStartColor.greenF(),
                                            impl_->gradientStartColor.blueF(),
                                            impl_->gradientStartColor.alphaF()));
    params.setGradientEndColor(FloatColor(impl_->gradientEndColor.redF(),
                                          impl_->gradientEndColor.greenF(),
                                          impl_->gradientEndColor.blueF(),
                                          impl_->gradientEndColor.alphaF()));
    params.setGradientAngleDegrees(static_cast<float>(impl_->gradientAngleSpin->value()));
    params.setGradientReverse(impl_->gradientReverseCheck->isChecked());
    params.setGradientCenterX(static_cast<float>(impl_->gradientCenterXSpin->value()));
    params.setGradientCenterY(static_cast<float>(impl_->gradientCenterYSpin->value()));
    params.setGradientScale(static_cast<float>(impl_->gradientScaleSpin->value()));
    params.setGradientOffset(static_cast<float>(impl_->gradientOffsetSpin->value()));
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
    header->setAutoFillBackground(true);
    {
        QPalette pal = header->palette();
        pal.setColor(QPalette::Window, QColor("#2a2a2a"));
        header->setPalette(pal);
    }
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(15, 0, 10, 0);

    auto* titleLabel = new QLabel(u8"平面設定", header);
    {
        QFont font = titleLabel->font();
        font.setBold(true);
        font.setPointSize(13);
        titleLabel->setFont(font);
        QPalette pal = titleLabel->palette();
        pal.setColor(QPalette::WindowText, QColor("#e0e0e0"));
        titleLabel->setPalette(pal);
    }

    chrome.closeButton = new DialogCloseButton(header);

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
    QComboBox*        placementCombo    = nullptr;
    ArtifactSolidLayerInitParams submittedParams = ArtifactSolidLayerInitParams(QStringLiteral("Solid Layer"));
    LayerCreationPlacementMode submittedPlacementMode =
        LayerCreationPlacementMode::CompositionStart;
    QPoint m_dragPosition;
    bool   m_isDragging = false;
};

CreateSolidLayerSettingDialog::CreateSolidLayerSettingDialog(QWidget* parent)
    : CreateSolidLayerSettingDialog(LayerCreationPlacementMode::CompositionStart, parent)
{
}

CreateSolidLayerSettingDialog::CreateSolidLayerSettingDialog(
    const LayerCreationPlacementMode defaultPlacementMode, QWidget* parent)
    : QDialog(parent), impl_(new Impl())
{
    setWindowTitle(u8"平面設定");
    impl_->submittedPlacementMode = defaultPlacementMode;

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

    chrome.scrollLayout->addWidget(makeSectionHeader(u8"作成", chrome.scrollContent));
    // Settings page (サイズ + カラー sections)
    auto* settingPage = impl_->settingPage = new PlaneLayerSettingPage(chrome.scrollContent);
    settingPage->resizeCompositionSize();
    chrome.scrollLayout->addWidget(settingPage);

    {
        impl_->placementCombo = new QComboBox(chrome.scrollContent);
        impl_->placementCombo->addItem(
            QStringLiteral("コンポの最初"),
            static_cast<int>(LayerCreationPlacementMode::CompositionStart));
        impl_->placementCombo->addItem(
            QStringLiteral("プレイヘッド位置"),
            static_cast<int>(LayerCreationPlacementMode::Playhead));
        const int index = impl_->placementCombo->findData(static_cast<int>(defaultPlacementMode));
        impl_->placementCombo->setCurrentIndex(index >= 0 ? index : 0);
        chrome.scrollLayout->addWidget(
            makeRow(chrome.scrollContent, u8"作成位置", 100, impl_->placementCombo));
    }

    chrome.scrollLayout->addStretch();

    // Connections
    QObject::connect(chrome.closeButton, &QPushButton::clicked, this, &QDialog::reject);

    QObject::connect(chrome.buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (impl_->nameEditableLabel) impl_->nameEditableLabel->finishEdit();
        QString name = impl_->nameEditableLabel
            ? impl_->nameEditableLabel->text()
            : suggestedPlaneLayerName(QColor(255, 255, 255, 255));
        impl_->submittedParams = impl_->settingPage->getInitParams(name);
        if (impl_->placementCombo) {
            impl_->submittedPlacementMode = static_cast<LayerCreationPlacementMode>(
                impl_->placementCombo->currentData().toInt());
        }
        Q_EMIT submit(impl_->submittedParams);
        accept();
    });
    QObject::connect(chrome.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QObject::connect(settingPage, &PlaneLayerSettingPage::colorChanged,
                     this, [this](const QString& name) {
        if (impl_->nameEditableLabel)
            impl_->nameEditableLabel->setText(name);
    });

    adjustSize();
    const int preferredWidth = std::max(width(), 620);
    const int preferredHeight = std::max(height(), 720);
    resize(preferredWidth, preferredHeight);
    setMinimumSize(QSize(620, 720));
}

CreateSolidLayerSettingDialog::~CreateSolidLayerSettingDialog()
{
    delete impl_;
}

ArtifactSolidLayerInitParams CreateSolidLayerSettingDialog::submittedParams() const
{
    return impl_->submittedParams;
}

LayerCreationPlacementMode CreateSolidLayerSettingDialog::submittedPlacementMode() const
{
    return impl_->submittedPlacementMode;
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
            impl_->targetLayer->setFillType(params.fillType());
            impl_->targetLayer->setGradientStartColor(params.gradientStartColor());
            impl_->targetLayer->setGradientEndColor(params.gradientEndColor());
            impl_->targetLayer->setGradientAngleDegrees(params.gradientAngleDegrees());
            impl_->targetLayer->setGradientReverse(params.gradientReverse());
            impl_->targetLayer->setGradientCenterX(params.gradientCenterX());
            impl_->targetLayer->setGradientCenterY(params.gradientCenterY());
            impl_->targetLayer->setGradientScale(params.gradientScale());
            impl_->targetLayer->setGradientOffset(params.gradientOffset());
        }
        accept();
    });
    QObject::connect(chrome.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QObject::connect(settingPage, &PlaneLayerSettingPage::colorChanged,
                     this, [this](const QString& name) {
        if (impl_->nameEditableLabel)
            impl_->nameEditableLabel->setText(name);
    });

    adjustSize();
    resize(std::max(width(), 560), height());
    setMinimumSize(QSize(560, height()));
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
        impl_->settingPage->setInitialGradientParams(layer->fillType(),
                                                     layer->gradientStartColor(),
                                                     layer->gradientEndColor(),
                                                     layer->gradientAngleDegrees(),
                                                     layer->gradientReverse(),
                                                     layer->gradientCenterX(),
                                                     layer->gradientCenterY(),
                                                     layer->gradientScale(),
                                                     layer->gradientOffset());
    }
}

};

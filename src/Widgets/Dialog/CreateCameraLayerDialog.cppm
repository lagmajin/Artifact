module;
#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QFrame>
#include <QMouseEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QTimer>
#include <QSet>
#include <wobjectimpl.h>
#include <cmath>
#include <Widgets/Dialog/ArtifactDialogButtons.hpp>

module Artifact.Widgets.CreateCameraLayerDialog;

import Artifact.Layer.Camera;
import Artifact.Service.Project;
import Widgets.Utils.CSS;

namespace Artifact {

W_OBJECT_IMPL(CreateCameraLayerDialog)

// ─────────────────────────────────────────────────────────────────────────────
// Lens diagram widget
// ─────────────────────────────────────────────────────────────────────────────
namespace {

class LensDiagramWidget : public QWidget {
public:
    explicit LensDiagramWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(300, 120);
        setMaximumHeight(150);
        setStyleSheet("background-color: #1a2a3a; border: 1px solid #333;");
    }

    void setFocalLength(float mm) { focalLength_ = mm; update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int w = width();
        const int h = height();
        const float cx = w * 0.28f;  // camera icon x
        const float cy = h * 0.5f;
        const float focalPx = std::min(focalLength_ * 2.5f, w * 0.55f);

        // FOV cone lines
        const float halfAngle = std::atan2(h * 0.35f, focalPx);
        const float dx = focalPx * std::cos(0.0f);
        const float dy = focalPx * std::tan(halfAngle);
        QPen conePen(QColor(0x5F, 0xAA, 0xDD, 200), 1.5f);
        p.setPen(conePen);
        p.drawLine(QPointF(cx, cy), QPointF(cx + dx, cy - dy));
        p.drawLine(QPointF(cx, cy), QPointF(cx + dx, cy + dy));

        // Focal plane vertical line (dashed)
        QPen focalPen(QColor(0x5F, 0xAA, 0xDD), 1.5f, Qt::DashLine);
        p.setPen(focalPen);
        p.drawLine(QPointF(cx + focalPx, cy - h * 0.38f),
                   QPointF(cx + focalPx, cy + h * 0.38f));

        // Dashed horizontal center line
        QPen dashPen(QColor(0x5F, 0xAA, 0xDD, 100), 1.0f, Qt::DashLine);
        p.setPen(dashPen);
        p.drawLine(QPointF(cx, cy), QPointF(w * 0.92f, cy));

        // Focal length label
        p.setPen(QColor(0x5F, 0xAA, 0xDD));
        p.setFont(QFont("Consolas", 8));
        p.drawText(QRectF(cx + focalPx * 0.4f, cy + 4, 60, 18),
                   Qt::AlignLeft,
                   QString("%1mm").arg(static_cast<int>(focalLength_)));

        // Focus point crosshair
        const float fpx = cx + focalPx;
        QPen crossPen(QColor(0x44, 0xCC, 0xFF), 1.5f);
        p.setPen(crossPen);
        p.drawLine(QPointF(fpx - 6, cy), QPointF(fpx + 6, cy));
        p.drawLine(QPointF(fpx, cy - 6), QPointF(fpx, cy + 6));

        // Camera body icon
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x44, 0x88, 0xCC));
        p.drawRoundedRect(QRectF(cx - 14, cy - 10, 28, 20), 3, 3);
        p.setBrush(QColor(0x22, 0x66, 0xAA));
        p.drawEllipse(QPointF(cx, cy), 6.0, 6.0);
        p.setBrush(QColor(0x88, 0xCC, 0xFF, 180));
        p.drawEllipse(QPointF(cx, cy), 3.5, 3.5);
    }

private:
    float focalLength_ = 35.0f;
};

// Standard F-stop values
const QStringList kFStops = {
    "f/1.4", "f/2", "f/2.8", "f/4", "f/5.6", "f/8", "f/11", "f/16", "f/22"
};

// Camera presets: {name, focalLength mm}
struct CameraPreset { QString name; float focalMm; };
const QList<CameraPreset> kPresets = {
    {"カスタム",   0.0f},
    {"15mm",      15.0f},
    {"20mm",      20.0f},
    {"24mm",      24.0f},
    {"28mm",      28.0f},
    {"35mm",      35.0f},
    {"50mm",      50.0f},
    {"85mm",      85.0f},
    {"135mm",    135.0f},
    {"200mm",    200.0f},
};

float fovFromFocalLength(float mm)
{
    // AE: fov = 2 * atan(18 / mm) * (180 / pi)  (for 36mm sensor)
    if (mm <= 0.0f) return 0.0f;
    return static_cast<float>(
        2.0 * std::atan(18.0 / static_cast<double>(mm)) * 180.0 / 3.14159265358979);
}

QSet<QString> currentLayerNames()
{
    QSet<QString> names;
    if (auto* service = ArtifactProjectService::instance()) {
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

QString makeUniqueSequentialName(QString baseName, const QSet<QString>& occupied)
{
    baseName = baseName.trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("Camera 1");
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

QString uniqueCameraLayerName()
{
    return makeUniqueSequentialName(QStringLiteral("Camera 1"), currentLayerNames());
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Impl
// ─────────────────────────────────────────────────────────────────────────────
class CreateCameraLayerDialog::Impl {
public:
    // Widgets
    QLineEdit*       nameEdit        = nullptr;
    QComboBox*       presetCombo     = nullptr;
    QDoubleSpinBox*  focalLengthSpin = nullptr;
    QDoubleSpinBox*  fovSpin         = nullptr;
    LensDiagramWidget* diagram       = nullptr;
    QComboBox*       apertureFCombo  = nullptr;
    QDoubleSpinBox*  focusDistSpin   = nullptr;
    QDoubleSpinBox*  blurAmountSpin  = nullptr;
    QCheckBox*       dofCheck        = nullptr;
    QCheckBox*       motionBlurCheck = nullptr;
    QCheckBox*       lockCameraCheck = nullptr;
    QDoubleSpinBox*  zoomSpin        = nullptr;

    QPoint dragPos;
    bool   dragging = false;
    bool   updatingPreset = false;

    void syncFovFromFocalLength()
    {
        if (!focalLengthSpin || !fovSpin) return;
        const float fl = static_cast<float>(focalLengthSpin->value());
        const float fov = fovFromFocalLength(fl);
        QSignalBlocker b(fovSpin);
        fovSpin->setValue(static_cast<double>(fov));
        if (diagram) diagram->setFocalLength(fl);
    }

    void syncPresetFromFocalLength()
    {
        if (!presetCombo || !focalLengthSpin || updatingPreset) return;
        const float fl = static_cast<float>(focalLengthSpin->value());
        QSignalBlocker b(presetCombo);
        for (int i = 1; i < kPresets.size(); ++i) {
            if (std::abs(kPresets[i].focalMm - fl) < 0.5f) {
                presetCombo->setCurrentIndex(i);
                return;
            }
        }
        presetCombo->setCurrentIndex(0); // custom
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
CreateCameraLayerDialog::CreateCameraLayerDialog(QWidget* parent)
    : QDialog(parent), impl_(new Impl())
{
    setWindowTitle(u8"カメラ設定");
    setFixedSize(500, 620);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_NoChildEventsForParent);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(48);
    header->setStyleSheet("background-color: #2a2a2a;");
    auto* hLay = new QHBoxLayout(header);
    hLay->setContentsMargins(15, 0, 10, 0);
    auto* titleLbl = new QLabel(u8"カメラ設定", header);
    titleLbl->setStyleSheet("color: #e0e0e0; font-size: 13px; font-weight: bold;");
    auto* closeBtn = new QPushButton(u8"×", header);
    closeBtn->setFixedSize(30, 30);
    closeBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #aaaaaa; border: none; font-size: 18px; }"
        "QPushButton:hover { color: #ff4444; }");
    hLay->addWidget(titleLbl);
    hLay->addStretch();
    hLay->addWidget(closeBtn);
    root->addWidget(header);

    // ── Scroll body ───────────────────────────────────────────────────────
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(scroll);
    auto* bLay = new QVBoxLayout(body);
    bLay->setContentsMargins(20, 12, 20, 12);
    bLay->setSpacing(0);
    scroll->setWidget(body);
    root->addWidget(scroll, 1);

    const auto sectionHeader = [&](const QString& text) -> QWidget* {
        auto* w = new QWidget(body);
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 10, 0, 4);
        auto* lbl = new QLabel(text, w);
        lbl->setStyleSheet("color: #888888; font-size: 11px;");
        l->addWidget(lbl);
        l->addStretch();
        return w;
    };
    const auto makeSeparator = [&]() -> QFrame* {
        auto* sep = new QFrame(body);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color: #3a3a3a;");
        sep->setFixedHeight(1);
        return sep;
    };

    // ── 名前 ──────────────────────────────────────────────────────────────
    {
        auto* row = new QWidget(body);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 4, 0, 8);
        auto* lbl = new QLabel(u8"名前", row);
        lbl->setFixedWidth(80);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lbl->setStyleSheet("color: #cccccc;");
        impl_->nameEdit = new QLineEdit(uniqueCameraLayerName(), row);
        rl->addWidget(lbl);
        rl->addWidget(impl_->nameEdit, 1);
        bLay->addWidget(row);
    }

    bLay->addWidget(makeSeparator());
    bLay->addWidget(sectionHeader(u8"プリセット / レンズ"));

    // ── プリセット ────────────────────────────────────────────────────────
    {
        auto* row = new QWidget(body);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 2, 0, 2);
        auto* lbl = new QLabel(u8"プリセット", row);
        lbl->setFixedWidth(80);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lbl->setStyleSheet("color: #cccccc;");
        impl_->presetCombo = new QComboBox(row);
        for (const auto& p : kPresets) impl_->presetCombo->addItem(p.name);
        impl_->presetCombo->setCurrentIndex(5); // 35mm default
        rl->addWidget(lbl);
        rl->addWidget(impl_->presetCombo, 1);
        bLay->addWidget(row);
    }

    // ── 焦点距離 ──────────────────────────────────────────────────────────
    {
        auto* row = new QWidget(body);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 2, 0, 2);
        auto* lbl = new QLabel(u8"焦点距離", row);
        lbl->setFixedWidth(80);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lbl->setStyleSheet("color: #cccccc;");
        impl_->focalLengthSpin = new QDoubleSpinBox(row);
        impl_->focalLengthSpin->setRange(1.0, 2000.0);
        impl_->focalLengthSpin->setDecimals(2);
        impl_->focalLengthSpin->setSingleStep(1.0);
        impl_->focalLengthSpin->setValue(35.0);
        rl->addWidget(lbl);
        rl->addWidget(impl_->focalLengthSpin, 1);
        bLay->addWidget(row);
    }

    // ── 視野角 ────────────────────────────────────────────────────────────
    {
        auto* row = new QWidget(body);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 2, 0, 2);
        auto* lbl = new QLabel(u8"視野角", row);
        lbl->setFixedWidth(80);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lbl->setStyleSheet("color: #cccccc;");
        impl_->fovSpin = new QDoubleSpinBox(row);
        impl_->fovSpin->setRange(0.5, 170.0);
        impl_->fovSpin->setDecimals(2);
        impl_->fovSpin->setValue(fovFromFocalLength(35.0));
        rl->addWidget(lbl);
        rl->addWidget(impl_->fovSpin, 1);
        bLay->addWidget(row);
    }

    // ── Lens diagram ──────────────────────────────────────────────────────
    impl_->diagram = new LensDiagramWidget(body);
    impl_->diagram->setFocalLength(35.0f);
    bLay->addSpacing(6);
    bLay->addWidget(impl_->diagram);
    bLay->addSpacing(6);

    bLay->addWidget(makeSeparator());
    bLay->addWidget(sectionHeader(u8"被写界深度"));

    // ── 絞り (F値) ───────────────────────────────────────────────────────
    {
        auto* row = new QWidget(body);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 2, 0, 2);
        auto* lbl = new QLabel(u8"絞り（F値）", row);
        lbl->setFixedWidth(80);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lbl->setStyleSheet("color: #cccccc;");
        impl_->apertureFCombo = new QComboBox(row);
        for (const auto& f : kFStops) impl_->apertureFCombo->addItem(f);
        impl_->apertureFCombo->setCurrentText("f/4");
        rl->addWidget(lbl);
        rl->addWidget(impl_->apertureFCombo, 1);
        bLay->addWidget(row);
    }

    // ── 焦点距離 + ブラー量 (横並び) ─────────────────────────────────────
    {
        auto* row = new QWidget(body);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 2, 0, 2);
        auto* fdLbl = new QLabel(u8"焦点距離", row);
        fdLbl->setFixedWidth(80);
        fdLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        fdLbl->setStyleSheet("color: #cccccc;");
        impl_->focusDistSpin = new QDoubleSpinBox(row);
        impl_->focusDistSpin->setRange(0.0, 1000000.0);
        impl_->focusDistSpin->setDecimals(1);
        impl_->focusDistSpin->setValue(1000.0);
        auto* blurLbl = new QLabel(u8"ブラー量", row);
        blurLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        blurLbl->setStyleSheet("color: #cccccc;");
        blurLbl->setFixedWidth(60);
        impl_->blurAmountSpin = new QDoubleSpinBox(row);
        impl_->blurAmountSpin->setRange(0.0, 100.0);
        impl_->blurAmountSpin->setDecimals(0);
        impl_->blurAmountSpin->setValue(100.0);
        auto* pctLbl = new QLabel("%", row);
        pctLbl->setStyleSheet("color: #888888;");
        rl->addWidget(fdLbl);
        rl->addWidget(impl_->focusDistSpin, 2);
        rl->addSpacing(8);
        rl->addWidget(blurLbl);
        rl->addWidget(impl_->blurAmountSpin, 2);
        rl->addWidget(pctLbl);
        bLay->addWidget(row);
    }

    // ── 被写界深度を有効にする ────────────────────────────────────────────
    {
        auto* row = new QWidget(body);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(84, 2, 0, 8);
        impl_->dofCheck = new QCheckBox(u8"被写界深度を有効にする", row);
        impl_->dofCheck->setChecked(false);
        rl->addWidget(impl_->dofCheck);
        bLay->addWidget(row);
    }

    bLay->addWidget(makeSeparator());
    bLay->addWidget(sectionHeader(u8"その他のオプション"));

    // ── モーションブラー / カメラをロック + ズーム ────────────────────────
    {
        auto* row = new QWidget(body);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 2, 0, 8);

        auto* leftCol = new QWidget(row);
        auto* ll = new QVBoxLayout(leftCol);
        ll->setContentsMargins(0, 0, 0, 0);
        impl_->motionBlurCheck  = new QCheckBox(u8"モーションブラー", leftCol);
        impl_->lockCameraCheck  = new QCheckBox(u8"カメラをロック", leftCol);
        ll->addWidget(impl_->motionBlurCheck);
        ll->addWidget(impl_->lockCameraCheck);

        auto* rightCol = new QWidget(row);
        auto* rl2 = new QHBoxLayout(rightCol);
        rl2->setContentsMargins(0, 0, 0, 0);
        auto* zoomLbl = new QLabel(u8"ズーム", rightCol);
        zoomLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        zoomLbl->setStyleSheet("color: #cccccc;");
        zoomLbl->setFixedWidth(50);
        impl_->zoomSpin = new QDoubleSpinBox(rightCol);
        impl_->zoomSpin->setRange(1.0, 100000.0);
        impl_->zoomSpin->setDecimals(1);
        impl_->zoomSpin->setValue(666.7);
        auto* pxLbl = new QLabel("px", rightCol);
        pxLbl->setStyleSheet("color: #888888;");
        rl2->addWidget(zoomLbl);
        rl2->addWidget(impl_->zoomSpin, 1);
        rl2->addWidget(pxLbl);

        rl->addWidget(leftCol, 1);
        rl->addWidget(rightCol, 1);
        bLay->addWidget(row);
    }

    bLay->addStretch();

    // ── Footer ────────────────────────────────────────────────────────────
    auto* footer = new QWidget(this);
    auto* fLay = new QHBoxLayout(footer);
    fLay->setContentsMargins(15, 10, 15, 10);

    // Visual buttons – explicit Windows order: [OK] [キャンセル]
    const DialogButtonRow buttons = createWindowsDialogButtonRow(footer, QStringLiteral("OK"), QStringLiteral("キャンセル"));
    auto* okBtn = buttons.okButton;
    auto* cancelBtn = buttons.cancelButton;
    okBtn->setFixedSize(80, 28);
    cancelBtn->setFixedSize(80, 28);
    fLay->addStretch();
    fLay->addWidget(buttons.widget);
    root->addWidget(footer);

    // ── Connections ───────────────────────────────────────────────────────
    QObject::connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    QObject::connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    QObject::connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // Preset → focal length
    QObject::connect(impl_->presetCombo,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this, [this](int idx) {
        if (idx <= 0 || idx >= kPresets.size()) return;
        impl_->updatingPreset = true;
        {
            QSignalBlocker b(impl_->focalLengthSpin);
            impl_->focalLengthSpin->setValue(kPresets[idx].focalMm);
        }
        impl_->syncFovFromFocalLength();
        impl_->updatingPreset = false;
    });

    // Focal length → FOV + diagram + preset sync
    QObject::connect(impl_->focalLengthSpin,
                     QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     this, [this](double) {
        impl_->syncFovFromFocalLength();
        impl_->syncPresetFromFocalLength();
    });

    // Trigger initial diagram
    impl_->syncFovFromFocalLength();
}

CreateCameraLayerDialog::~CreateCameraLayerDialog()
{
    delete impl_;
}

// ── Accessors ────────────────────────────────────────────────────────────────
QString CreateCameraLayerDialog::cameraName()       const { return impl_->nameEdit       ? impl_->nameEdit->text()                                       : QString(); }
float   CreateCameraLayerDialog::focalLength()      const { return impl_->focalLengthSpin ? static_cast<float>(impl_->focalLengthSpin->value())          : 35.0f; }
float   CreateCameraLayerDialog::fov()              const { return impl_->fovSpin         ? static_cast<float>(impl_->fovSpin->value())                  : 54.0f; }
float   CreateCameraLayerDialog::zoom()             const { return impl_->zoomSpin        ? static_cast<float>(impl_->zoomSpin->value())                 : 666.7f; }
float   CreateCameraLayerDialog::focusDistance()    const { return impl_->focusDistSpin   ? static_cast<float>(impl_->focusDistSpin->value())            : 1000.0f; }
float   CreateCameraLayerDialog::blurAmount()       const { return impl_->blurAmountSpin  ? static_cast<float>(impl_->blurAmountSpin->value())           : 100.0f; }
bool    CreateCameraLayerDialog::depthOfFieldEnabled() const { return impl_->dofCheck    ? impl_->dofCheck->isChecked()                                 : false; }
bool    CreateCameraLayerDialog::motionBlur()       const { return impl_->motionBlurCheck ? impl_->motionBlurCheck->isChecked()                         : false; }
bool    CreateCameraLayerDialog::cameraLocked()     const { return impl_->lockCameraCheck ? impl_->lockCameraCheck->isChecked()                         : false; }

float CreateCameraLayerDialog::apertureF() const
{
    if (!impl_->apertureFCombo) return 4.0f;
    const QString t = impl_->apertureFCombo->currentText(); // "f/4"
    const int slash = t.indexOf('/');
    if (slash < 0) return 4.0f;
    bool ok = false;
    const float v = t.mid(slash + 1).toFloat(&ok);
    return ok ? v : 4.0f;
}

// ── Drag ─────────────────────────────────────────────────────────────────────
void CreateCameraLayerDialog::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        impl_->dragPos  = e->globalPosition().toPoint() - frameGeometry().topLeft();
        impl_->dragging = true;
        e->accept();
        return;
    }
    QDialog::mousePressEvent(e);
}

void CreateCameraLayerDialog::mouseReleaseEvent(QMouseEvent* e)
{
    if (impl_->dragging && e->button() == Qt::LeftButton) {
        impl_->dragging = false;
        e->accept();
        return;
    }
    QDialog::mouseReleaseEvent(e);
}

void CreateCameraLayerDialog::mouseMoveEvent(QMouseEvent* e)
{
    if (impl_->dragging && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPosition().toPoint() - impl_->dragPos);
        e->accept();
        return;
    }
    QDialog::mouseMoveEvent(e);
}

void CreateCameraLayerDialog::showEvent(QShowEvent* e)
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

module;

#include <algorithm>
#include <cmath>

#include <QEvent>
#include <QMouseEvent>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QRect>
#include <QPainter>
#include <QLinearGradient>
#include <QPalette>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabBar>
#include <QStackedWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QAbstractSpinBox>
#include <QSettings>
#include <QStringList>
#include <QTimerEvent>
#include <QFocusEvent>
#include <QShowEvent>
#include <QKeyEvent>
#include <functional>
#include <QVariant>
#include <QDialog>
#include <QObject>
#include <QString>
#include <QColor>
#include <QPen>
#include <QPaintEvent>
#include <QFrame>
#include <QFont>

module Artifact.Widgets.Dialog.FloatColorPickerHooks;

import FloatColorPickerDialog;
import Color.Float;
import Color.Lab;
import Color.XYZ;

namespace Artifact {

namespace {

QStyleOptionSlider makeSliderStyleOption(QSlider* slider)
{
    QStyleOptionSlider option;
    option.initFrom(slider);
    option.orientation = slider->orientation();
    option.minimum = slider->minimum();
    option.maximum = slider->maximum();
    option.sliderPosition = slider->sliderPosition();
    option.sliderValue = slider->value();
    option.singleStep = slider->singleStep();
    option.pageStep = slider->pageStep();
    option.upsideDown = slider->invertedAppearance();
    option.tickPosition = slider->tickPosition();
    option.tickInterval = slider->tickInterval();
    option.rect = slider->rect();
    option.subControls = QStyle::SC_SliderGroove | QStyle::SC_SliderHandle;
    option.activeSubControls = QStyle::SC_None;
    return option;
}

bool jumpSliderToMouse(QSlider* slider, const QPoint& point)
{
    if (!slider) {
        return false;
    }

    QStyleOptionSlider option = makeSliderStyleOption(slider);
    QStyle* style = slider->style();
    const QRect grooveRect =
        style->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, slider);
    const QRect handleRect =
        style->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, slider);
    if (!grooveRect.isValid()) {
        return false;
    }
    if (handleRect.contains(point)) {
        return false;
    }

    const int minValue = slider->minimum();
    const int maxValue = slider->maximum();
    if (maxValue <= minValue) {
        return false;
    }

    int value = slider->value();
    if (slider->orientation() == Qt::Horizontal) {
        const int span = std::max(1, grooveRect.width() - 1);
        const int position = std::clamp(point.x() - grooveRect.left(), 0, span);
        value = QStyle::sliderValueFromPosition(
            minValue, maxValue, position, span, option.upsideDown);
    } else {
        const int span = std::max(1, grooveRect.height() - 1);
        const int positionFromTop = std::clamp(point.y() - grooveRect.top(), 0, span);
        const int position = option.upsideDown ? positionFromTop : (span - positionFromTop);
        value = QStyle::sliderValueFromPosition(
            minValue, maxValue, position, span, false);
    }

    slider->setSliderDown(true);
    slider->setSliderPosition(value);
    slider->setValue(value);
    slider->setSliderDown(false);
    return true;
}

class SliderJumpFilter final : public QObject
{
public:
    explicit SliderJumpFilter(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        auto* slider = qobject_cast<QSlider*>(watched);
        if (!slider) {
            return QObject::eventFilter(watched, event);
        }

        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (event->type() != QEvent::MouseButtonPress &&
            event->type() != QEvent::MouseButtonDblClick) {
            return QObject::eventFilter(watched, event);
        }
        if (mouseEvent->button() != Qt::LeftButton) {
            return QObject::eventFilter(watched, event);
        }

        if (jumpSliderToMouse(slider, mouseEvent->position().toPoint())) {
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};

}


namespace {
QColor displayColor(ArtifactWidgets::FloatColorPicker* picker)
{
    const auto c = picker->getColor();
    return QColor::fromRgbF(std::clamp(double(c.r()), 0.0, 1.0),
        std::clamp(double(c.g()), 0.0, 1.0), std::clamp(double(c.b()), 0.0, 1.0),
        std::clamp(double(c.a()), 0.0, 1.0));
}

// UI swatches only: no scene color-space conversion or document mutation here.
void paintSwatch(QPainter& painter, const QRect& rect, QColor color)
{
    for (int y = rect.top(); y <= rect.bottom(); y += 6)
        for (int x = rect.left(); x <= rect.right(); x += 6) {
            const int shade = ((x - rect.left()) / 6 + (y - rect.top()) / 6) % 2 ? 90 : 64;
            painter.fillRect(QRect(x, y, 6, 6).intersected(rect), QColor(shade, shade, shade));
        }
    painter.fillRect(rect, color);
}

class ColorSample final : public QPushButton {
    ArtifactWidgets::FloatColorPicker* picker_;
    ArtifactCore::FloatColor color_;
    bool current_;
public:
    ColorSample(ArtifactWidgets::FloatColorPicker* picker, ArtifactCore::FloatColor color, bool current)
        : QPushButton(picker), picker_(picker), color_(color), current_(current) {
        setFixedSize(current ? 156 : 38, current ? 44 : 38);
        setAutoDefault(false);
        setAccessibleName(current ? QStringLiteral("Current color") : QColor::fromRgbF(color.r(), color.g(), color.b(), color.a()).name(QColor::HexArgb));
        setToolTip(accessibleName());
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        const QRect area = rect().adjusted(2, 2, -2, -2);
        paintSwatch(painter, area, current_ ? displayColor(picker_) : QColor::fromRgbF(color_.r(), color_.g(), color_.b(), color_.a()));
        painter.setPen(hasFocus() ? palette().color(QPalette::Highlight) : palette().color(QPalette::Mid));
        painter.drawRect(area);
    }
    void nextCheckState() override {
        if (current_) return;
        picker_->setColor(color_);
        picker_->colorChanged(color_); // Reuse the existing preview route.
    }
};

QFrame* makeSeparator(QWidget* parent)
{
    auto* separator = new QFrame(parent);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Plain);
    QPalette palette = separator->palette();
    palette.setColor(QPalette::WindowText, QColor(62, 67, 75));
    separator->setPalette(palette);
    return separator;
}

void makeMuted(QLabel* label)
{
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, QColor(174, 180, 190));
    label->setPalette(palette);
}

class ChannelPresentation final : public QObject {
    ArtifactWidgets::FloatColorPicker* picker_;
    int channel_; // HSV 0..2, RGB 3..5, HSL 6..8, alpha 9
public:
    ChannelPresentation(ArtifactWidgets::FloatColorPicker* picker, QSlider* slider, int channel)
        : QObject(slider), picker_(picker), channel_(channel) { slider->installEventFilter(this); }
protected:
    bool eventFilter(QObject* object, QEvent* event) override {
        if (event->type() != QEvent::Paint) return false;
        auto* slider = static_cast<QSlider*>(object);
        const QColor color = displayColor(picker_);
        QPainter painter(slider);
        const QRect track(7, (slider->height() - 24) / 2, std::max(1, slider->width() - 14), 24);
        QLinearGradient gradient(track.topLeft(), track.topRight());
        for (int i = 0; i <= 24; ++i) {
            const double t = i / 24.0;
            QColor stop = color;
            stop.setAlphaF(1.0);
            if (channel_ < 3) {
                const double h = std::max(0.0, double(color.hsvHueF()));
                stop = QColor::fromHsvF(channel_ == 0 ? t : h,
                    channel_ == 1 ? t : color.hsvSaturationF(), channel_ == 2 ? t : color.valueF());
            } else if (channel_ < 6) {
                if (channel_ == 3) stop.setRedF(t);
                if (channel_ == 4) stop.setGreenF(t);
                if (channel_ == 5) stop.setBlueF(t);
            } else if (channel_ < 9) {
                const double h = std::max(0.0, double(color.hslHueF()));
                stop = QColor::fromHslF(channel_ == 6 ? t : h,
                    channel_ == 7 ? t : color.hslSaturationF(), channel_ == 8 ? t : color.lightnessF());
            } else stop.setAlphaF(t);
            gradient.setColorAt(t, stop);
        }
        if (channel_ == 9) paintSwatch(painter, track, Qt::transparent);
        painter.fillRect(track, gradient);
        const int x = track.left() + QStyle::sliderPositionFromValue(slider->minimum(),
            slider->maximum(), slider->sliderPosition(), std::max(1, track.width() - 1), slider->invertedAppearance());
        painter.setPen(QPen(QColor(20, 22, 25), 4));
        painter.drawLine(x, track.top() - 2, x, track.bottom() + 2);
        painter.setPen(QPen(Qt::white, 2));
        painter.drawLine(x, track.top() - 2, x, track.bottom() + 2);
        if (slider->hasFocus()) {
            painter.setPen(slider->palette().color(QPalette::Highlight));
            painter.drawRect(slider->rect().adjusted(1, 1, -2, -2));
        }
        return true; // EnhancedSlider retains its native drag and keyboard handling.
    }
};


// Numeric edits commit through the existing picker notification route.
class ModelNumber final : public QDoubleSpinBox {
public:
    using QDoubleSpinBox::QDoubleSpinBox;
    std::function<void()> edited;
protected:
    void stepBy(int steps) override {
        QDoubleSpinBox::stepBy(steps);
        if (edited) edited();
    }
    void focusOutEvent(QFocusEvent* event) override {
        QDoubleSpinBox::focusOutEvent(event);
        if (edited) edited();
    }
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            interpretText();
            if (edited) edited();
            event->accept();
            return;
        }
        QDoubleSpinBox::keyPressEvent(event);
    }
};

class NumericColorModelPage final : public QWidget {
    ArtifactWidgets::FloatColorPicker* picker_;
    bool lab_;
    bool syncing_ = false;
    ModelNumber* inputs_[3] = {};
    float last_[4] = {};
    double shown_[3] = {};
    QLabel* status_ = nullptr;
public:
    NumericColorModelPage(ArtifactWidgets::FloatColorPicker* picker, bool lab, QWidget* parent)
        : QWidget(parent), picker_(picker), lab_(lab) {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        for (int i = 0; i < 3; ++i) {
            auto* row = new QHBoxLayout;
            const QString name = lab ? (i == 0 ? QStringLiteral("L*") : i == 1 ? QStringLiteral("a*") : QStringLiteral("b*"))
                                     : (i == 0 ? QStringLiteral("X") : i == 1 ? QStringLiteral("Y") : QStringLiteral("Z"));
            row->addWidget(new QLabel(name, this));
            inputs_[i] = new ModelNumber(this);
            inputs_[i]->setAccessibleName(name);
            inputs_[i]->setDecimals(lab ? 3 : 5);
            inputs_[i]->setRange(lab && i > 0 ? -128.0 : 0.0, lab ? (i == 0 ? 100.0 : 127.0) : 2.0);
            inputs_[i]->setSingleStep(lab ? 0.1 : 0.001);
            inputs_[i]->setMinimumHeight(34);
            inputs_[i]->setKeyboardTracking(false);
            row->addWidget(inputs_[i], 1);
            layout->addLayout(row);
            inputs_[i]->edited = [this]() { commit(); };
        }
        auto* reference = new QLabel(lab ? QStringLiteral("CIELAB · D65 · sRGB 基準")
                                         : QStringLiteral("CIE XYZ · D65 · Y = 1 · sRGB 基準"), this);
        layout->addWidget(reference);
        status_ = new QLabel(QStringLiteral("RGB範囲外の入力は 0–1 にクリップされます"), this);
        status_->setWordWrap(true);
        layout->addWidget(status_);
        layout->addStretch();
        sync();
        startTimer(50);
    }
protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        sync(); // Merely changing tabs never writes the color.
    }
    void timerEvent(QTimerEvent*) override {
        if (!isVisible()) return;
        const auto c = picker_->getColor();
        if (c.r() != last_[0] || c.g() != last_[1] || c.b() != last_[2] || c.a() != last_[3]) sync();
    }
private:
    void sync() {
        syncing_ = true;
        const auto color = picker_->getColor();
        last_[0] = color.r(); last_[1] = color.g(); last_[2] = color.b(); last_[3] = color.a();
        if (lab_) {
            const auto value = ArtifactCore::LabColor::fromFloatColor(color);
            inputs_[0]->setValue(value.L()); inputs_[1]->setValue(value.a()); inputs_[2]->setValue(value.b());
        } else {
            const auto value = ArtifactCore::XYZColor::fromFloatColor(color);
            inputs_[0]->setValue(value.X()); inputs_[1]->setValue(value.Y()); inputs_[2]->setValue(value.Z());
        }
        for (int i = 0; i < 3; ++i) shown_[i] = inputs_[i]->value();
        syncing_ = false;
    }
    void commit() {
        if (syncing_) return;
        const double x = inputs_[0]->value(), y = inputs_[1]->value(), z = inputs_[2]->value();
        if (x == shown_[0] && y == shown_[1] && z == shown_[2]) return;
        const auto original = picker_->getColor();
        auto next = lab_ ? ArtifactCore::LabColor(float(x), float(y), float(z)).toFloatColor()
                         : ArtifactCore::XYZColor(float(x), float(y), float(z)).toFloatColor();
        next.setColor(next.r(), next.g(), next.b(), original.a());
        picker_->setColor(next);
        picker_->colorChanged(next);
        sync();
        // Show the actual representable result after the existing converter clips.
        const bool clipped = std::abs(shown_[0] - x) > (lab_ ? 0.05 : 0.0005)
                          || std::abs(shown_[1] - y) > (lab_ ? 0.05 : 0.0005)
                          || std::abs(shown_[2] - z) > (lab_ ? 0.05 : 0.0005);
        status_->setText(clipped ? QStringLiteral("入力をsRGB範囲へクリップしました。表示は変換後の値です。")
                                : QStringLiteral("RGB範囲外の入力は 0–1 にクリップされます"));
    }
};

class PickerPresentation final : public QObject {
    ArtifactWidgets::FloatColorPicker* picker_;
    QColor last_;
    int timer_;
public:
    explicit PickerPresentation(ArtifactWidgets::FloatColorPicker* picker)
        : QObject(picker), picker_(picker), last_(displayColor(picker)), timer_(startTimer(50)) {
        picker->installEventFilter(this);
    }
protected:
    void timerEvent(QTimerEvent* event) override {
        if (event->timerId() != timer_ || !picker_->isVisible()) return;
        const QColor next = displayColor(picker_);
        if (next == last_) return;
        last_ = next;
        for (auto* slider : picker_->findChildren<QSlider*>()) slider->update();
        for (auto* button : picker_->findChildren<QPushButton*>()) button->update();
    }
    bool eventFilter(QObject*, QEvent* event) override {
        if (event->type() == QEvent::Hide && picker_->result() == QDialog::Accepted) {
            QSettings settings;
            auto recent = settings.value(QStringLiteral("ColorPicker/recentFloatRgba")).toStringList();
            const auto color = picker_->getColor();
            const QString value = QStringLiteral("%1,%2,%3,%4")
                .arg(double(color.r()), 0, 'g', 9).arg(double(color.g()), 0, 'g', 9)
                .arg(double(color.b()), 0, 'g', 9).arg(double(color.a()), 0, 'g', 9);
            recent.removeAll(value);
            recent.prepend(value);
            while (recent.size() > 12) recent.removeLast();
            settings.setValue(QStringLiteral("ColorPicker/recentFloatRgba"), recent);
        }
        return false;
    }
};
}

void configureFloatColorPicker(QWidget* pickerRoot, ColorSelectionPurpose purpose)
{
    auto* picker = dynamic_cast<ArtifactWidgets::FloatColorPicker*>(pickerRoot);
    if (!picker || picker->property("studioColorPresentation").toBool()) return;
    auto* tabs = picker->findChild<QTabBar*>();
    auto* pages = picker->findChild<QStackedWidget*>();
    auto* hex = picker->findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly);
    const auto buttons = picker->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly);
    if (!tabs || !pages || pages->count() != 3 || !hex || buttons.size() != 3) return;
    QSlider* alpha = nullptr;
    for (auto* slider : picker->findChildren<QSlider*>(QString(), Qt::FindDirectChildrenOnly))
        if (slider->orientation() == Qt::Horizontal) alpha = slider;
    QDoubleSpinBox* alphaSpin = picker->findChild<QDoubleSpinBox*>(QString(), Qt::FindDirectChildrenOnly);
    if (!alpha || !alphaSpin) return;
    picker->setProperty("studioColorPresentation", true);
    // Keep channel widgets and their existing connections; replace layout chrome only.
    const auto children = picker->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* child : children) child->hide();
    delete picker->layout();
    auto* layout = new QVBoxLayout(picker);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(16);
    const bool creation = purpose == ColorSelectionPurpose::Creation;
    auto* contextRow = new QHBoxLayout;
    auto* context = new QLabel(creation ? QStringLiteral("Create color") : QStringLiteral("Edit color"), picker);
    QFont contextFont = context->font();
    contextFont.setBold(true);
    contextFont.setPointSizeF(contextFont.pointSizeF() * 1.08);
    context->setFont(contextFont);
    contextRow->addWidget(context);
    contextRow->addStretch();
    auto* format = new QLabel(QStringLiteral("Linear sRGB  ·  float RGBA"), picker);
    makeMuted(format);
    contextRow->addWidget(format);
    layout->addLayout(contextRow);
    auto* preview = new QHBoxLayout;
    preview->setSpacing(14);
    if (!creation) {
        auto* previousLabel = new QLabel(QStringLiteral("Previous"), picker);
        previousLabel->setMinimumWidth(62);
        preview->addWidget(previousLabel);
        auto* previous = new ColorSample(picker, picker->getColor(), false);
        previous->setFixedSize(156, 44);
        previous->setAccessibleName(QStringLiteral("Restore previous color"));
        previous->setToolTip(previous->accessibleName());
        preview->addWidget(previous);
    }
    preview->addSpacing(18);
    auto* currentLabel = new QLabel(creation ? QStringLiteral("Color") : QStringLiteral("New"), picker);
    currentLabel->setMinimumWidth(42);
    preview->addWidget(currentLabel);
    preview->addWidget(new ColorSample(picker, picker->getColor(), true));
    preview->addStretch();
    layout->addLayout(preview);
    layout->addWidget(makeSeparator(picker));
    tabs->setExpanding(false);
    tabs->setTabVisible(0, true);
    tabs->setTabText(0, QStringLiteral("HSV"));
    tabs->setTabToolTip(0, QStringLiteral("Hue / Saturation / Value (HSB)"));
    for (auto* label : pages->widget(0)->findChildren<QLabel*>())
        if (label->text() == QStringLiteral("B")) label->setText(QStringLiteral("V"));
    tabs->setCurrentIndex(1);
    QFont tabFont = tabs->font();
    tabFont.setBold(true);
    tabs->setFont(tabFont);
    layout->addWidget(tabs); tabs->show();
    layout->addWidget(pages); pages->show();
    for (int page = 0; page < 3; ++page) {
        const auto sliders = pages->widget(page)->findChildren<QSlider*>();
        for (int channel = 0; channel < sliders.size(); ++channel) {
            sliders[channel]->setMinimumHeight(38);
            new ChannelPresentation(picker, sliders[channel], page * 3 + channel);
        }
        for (auto* spin : pages->widget(page)->findChildren<QAbstractSpinBox*>()) {
            spin->setFixedWidth(78);
            spin->setMinimumHeight(30);
        }
        if (auto* box = qobject_cast<QVBoxLayout*>(pages->widget(page)->layout())) box->setSpacing(12);
    }
    // Append to preserve the existing tab-index to stacked-page connection.
    pages->addWidget(new NumericColorModelPage(picker, true, pages));
    tabs->addTab(QStringLiteral("Lab"));
    pages->addWidget(new NumericColorModelPage(picker, false, pages));
    tabs->addTab(QStringLiteral("XYZ"));
    auto* alphaRow = new QHBoxLayout;
    alphaRow->setSpacing(12);
    auto* alphaLabel = new QLabel(QStringLiteral("A"), picker);
    alphaLabel->setFixedWidth(24);
    alphaRow->addWidget(alphaLabel);
    alphaRow->addWidget(alpha, 1); alpha->show(); alpha->setMinimumHeight(38);
    alphaSpin->setFixedWidth(78);
    alphaSpin->setMinimumHeight(30);
    alphaRow->addWidget(alphaSpin); alphaSpin->show();
    new ChannelPresentation(picker, alpha, 9);
    layout->addLayout(alphaRow);
    auto* hexRow = new QHBoxLayout;
    hexRow->setSpacing(12);
    auto* hexLabel = new QLabel(QStringLiteral("HEX"), picker);
    hexLabel->setFixedWidth(36);
    hexRow->addWidget(hexLabel);
    hex->setFixedWidth(190); hex->setMinimumHeight(32); hex->setToolTip(QStringLiteral("RRGGBB or RRGGBBAA"));
    hexRow->addWidget(hex); hex->show(); hexRow->addStretch();
    auto* numericHint = new QLabel(QStringLiteral("RGB / Alpha  0–1"), picker);
    makeMuted(numericHint);
    hexRow->addWidget(numericHint);
    layout->addLayout(hexRow);
    layout->addWidget(makeSeparator(picker));
    auto* recentLabel = new QLabel(QStringLiteral("Recent colors"), picker);
    QFont recentFont = recentLabel->font();
    recentFont.setBold(true);
    recentLabel->setFont(recentFont);
    layout->addWidget(recentLabel);
    auto* history = new QHBoxLayout;
    const auto recent = QSettings().value(QStringLiteral("ColorPicker/recentFloatRgba")).toStringList();
    int count = 0;
    for (const auto& entry : recent) {
        const auto parts = entry.split(',');
        if (parts.size() != 4) continue;
        float values[4] = {};
        bool valid = true;
        for (int i = 0; i < 4; ++i) {
            bool ok = false;
            values[i] = parts[i].toFloat(&ok);
            valid = valid && ok && std::isfinite(values[i]) && values[i] >= 0 && values[i] <= 1;
        }
        if (!valid) continue;
        history->addWidget(new ColorSample(picker,
            ArtifactCore::FloatColor(values[0], values[1], values[2], values[3]), false));
        if (++count == 12) break;
    }
    if (!count) {
        auto* emptyHistory = new QLabel(QStringLiteral("Applied colors will appear here."), picker);
        makeMuted(emptyHistory);
        history->addWidget(emptyHistory);
    }
    history->addStretch(); layout->addLayout(history);
    layout->addWidget(makeSeparator(picker));
    auto* footer = new QHBoxLayout;
    buttons[0]->setText(creation ? QStringLiteral("Reset") : QStringLiteral("Restore previous"));
    buttons[1]->setText(creation ? QStringLiteral("Use color") : QStringLiteral("Apply"));
    buttons[2]->setText(QStringLiteral("Cancel"));
    footer->addWidget(buttons[0]); footer->addStretch();
    footer->addWidget(buttons[2]); footer->addWidget(buttons[1]);
    for (auto* button : buttons) { button->show(); button->setMinimumSize(108, 36); }
    buttons[1]->setDefault(true);
    QPalette primaryPalette = buttons[1]->palette();
    primaryPalette.setColor(QPalette::Button, QColor(43, 111, 232));
    primaryPalette.setColor(QPalette::ButtonText, Qt::white);
    buttons[1]->setPalette(primaryPalette);
    buttons[1]->setAutoFillBackground(true);
    layout->addLayout(footer);
    picker->setMinimumSize(700, 540);
    picker->resize(820, 600);
    new PickerPresentation(picker);
}

void installSliderJumpBehavior(QWidget* pickerRoot)
{
    if (!pickerRoot) {
        return;
    }

    const auto sliders = pickerRoot->findChildren<QSlider*>();
    for (QSlider* slider : sliders) {
        if (!slider) {
            continue;
        }
        if (slider->property("artifactSliderJumpInstalled").toBool()) {
            continue;
        }
        auto* filter = new SliderJumpFilter(slider);
        slider->installEventFilter(filter);
        slider->setProperty("artifactSliderJumpInstalled", true);
    }
}

void installFloatColorPickerSliderJump(QWidget* pickerRoot)
{
    installSliderJumpBehavior(pickerRoot);
}

}

module;

#include <algorithm>
#include <cmath>

#include <QEvent>
#include <QMouseEvent>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QRect>

module Artifact.Widgets.Dialog.FloatColorPickerHooks;

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

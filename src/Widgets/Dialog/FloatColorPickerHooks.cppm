module;

#include <algorithm>
#include <cmath>

#include <QEvent>
#include <QMouseEvent>
#include <QSlider>
#include <QStyle>
#include <QRect>

module Artifact.Widgets.Dialog.FloatColorPickerHooks;

namespace Artifact {

namespace {

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

        const int handleLength = slider->style()->pixelMetric(QStyle::PM_SliderLength, nullptr, slider);
        const int currentValue = slider->value();
        const int minValue = slider->minimum();
        const int maxValue = slider->maximum();
        const bool upsideDown = slider->invertedAppearance();

        if (slider->orientation() == Qt::Horizontal) {
            const int span = std::max(1, slider->width() - handleLength);
            const int handlePos = QStyle::sliderPositionFromValue(minValue, maxValue, currentValue, span, upsideDown);
            const QRect handleRect(handlePos, 0, handleLength, slider->height());
            if (handleRect.contains(mouseEvent->position().toPoint())) {
                return QObject::eventFilter(watched, event);
            }
            const int position = std::clamp(
                static_cast<int>(std::lround(mouseEvent->position().x() - handleLength * 0.5)),
                0, span);
            const int value = QStyle::sliderValueFromPosition(minValue, maxValue, position, span, upsideDown);
            slider->setValue(value);
            return true;
        }

        const int span = std::max(1, slider->height() - handleLength);
        const int handlePos = QStyle::sliderPositionFromValue(minValue, maxValue, currentValue, span, upsideDown);
        const QRect handleRect(0, handlePos, slider->width(), handleLength);
        if (handleRect.contains(mouseEvent->position().toPoint())) {
            return QObject::eventFilter(watched, event);
        }
        const int position = std::clamp(
            static_cast<int>(std::lround(mouseEvent->position().y() - handleLength * 0.5)),
            0, span);
        const int value = QStyle::sliderValueFromPosition(minValue, maxValue, position, span, upsideDown);
        slider->setValue(value);
        return true;
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

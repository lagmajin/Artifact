module;

#include <QAbstractScrollArea>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPalette>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointF>
#include <QProxyStyle>
#include <QPushButton>
#include <QRectF>
#include <QScrollBar>
#include <QStyleOption>
#include <QStyleOptionSlider>
#include <QStyle>
#include <QWidget>

#include <algorithm>

export module Artifact.Widgets.InspectorStyle;

import Widgets.CommonStyle;
import Widgets.Utils.CSS;

export namespace Artifact {

QColor themeColor(const QString& value, const QColor& fallback)
{
    const QColor color(value);
    return color.isValid() ? color : fallback;
}

QColor blendColor(const QColor& a, const QColor& b, const qreal t)
{
    const qreal clamped = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(a.redF() * (1.0 - clamped) + b.redF() * clamped,
                            a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
                            a.blueF() * (1.0 - clamped) + b.blueF() * clamped,
                            a.alphaF() * (1.0 - clamped) + b.alphaF() * clamped);
}

void applyInspectorPalette(QWidget* widget, const bool elevated = false)
{
    if (!widget) {
        return;
    }
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor background =
        themeColor(theme.backgroundColor, QColor(QStringLiteral("#20242A")));
    const QColor surface = themeColor(theme.secondaryBackgroundColor,
                                      QColor(QStringLiteral("#2B3038")));
    const QColor text =
        themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
    const QColor selection =
        themeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
    const QColor border =
        themeColor(theme.borderColor, QColor(QStringLiteral("#404754")));
    const QColor accent =
        themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
    const QColor mutedText = blendColor(text, background, 0.52);
    const QColor disabledSurface = blendColor(surface, background, 0.58);

    widget->setAttribute(Qt::WA_StyledBackground, true);
    widget->setAutoFillBackground(true);
    QPalette pal = widget->palette();
    const QColor window =
        elevated ? blendColor(surface, background, 0.16) : background;
    pal.setColor(QPalette::Window, window);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Base, surface);
    pal.setColor(QPalette::AlternateBase, blendColor(surface, background, 0.12));
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Button, surface);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::Highlight, selection);
    pal.setColor(QPalette::HighlightedText, background);
    pal.setColor(QPalette::Mid, border);
    pal.setColor(QPalette::Light, accent.lighter(120));
    pal.setColor(QPalette::Disabled, QPalette::Window, background);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, mutedText);
    pal.setColor(QPalette::Disabled, QPalette::Base, disabledSurface);
    pal.setColor(QPalette::Disabled, QPalette::AlternateBase, disabledSurface);
    pal.setColor(QPalette::Disabled, QPalette::Text, mutedText);
    pal.setColor(QPalette::Disabled, QPalette::Button, disabledSurface);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, mutedText);
    pal.setColor(QPalette::Disabled, QPalette::Highlight, border);
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, mutedText);
    widget->setPalette(pal);
}

void applyInspectorLabelPalette(QLabel* label, const bool prominent = false)
{
    if (!label) {
        return;
    }
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor text =
        themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
    const QColor accent =
        themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
    const QColor heading = blendColor(text, QColor(Qt::white), 0.12);
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, prominent ? heading : text);
    label->setPalette(pal);
}

void applyInspectorSectionBox(QGroupBox* box)
{
    if (!box) {
        return;
    }
    applyInspectorPalette(box, true);
    QFont font = box->font();
    font.setPointSize(10);
    font.setWeight(QFont::DemiBold);
    box->setFont(font);
}

void applyInspectorTextEdit(QPlainTextEdit* edit)
{
    if (!edit) {
        return;
    }
    applyInspectorPalette(edit, true);
    edit->setTabChangesFocus(true);
}

void applyInspectorList(QListWidget* list)
{
    if (!list) {
        return;
    }
    applyInspectorPalette(list, true);
    list->setAlternatingRowColors(true);
}

void applyInspectorButton(QPushButton* button, const bool accent = false)
{
    if (!button) {
        return;
    }
    if (!button->text().trimmed().isEmpty()) {
        button->setAccessibleName(button->text().trimmed());
        if (!button->toolTip().trimmed().isEmpty()) {
            button->setAccessibleDescription(button->toolTip().trimmed());
        }
    }
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor background =
        themeColor(theme.backgroundColor, QColor(QStringLiteral("#20242A")));
    const QColor surface = themeColor(theme.secondaryBackgroundColor,
                                      QColor(QStringLiteral("#2B3038")));
    const QColor text =
        themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
    const QColor selection =
        themeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
    const QColor border =
        themeColor(theme.borderColor, QColor(QStringLiteral("#404754")));
    const QColor fill =
        accent ? themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")))
               : surface;
    const QColor contrast = accent ? QColor(Qt::white) : text;
    const QColor disabledText = blendColor(text, background, 0.58);
    const QColor disabledButton = blendColor(surface, background, 0.62);
    const QColor disabledWindow = background;

    button->setAttribute(Qt::WA_StyledBackground, true);
    button->setAutoFillBackground(true);
    QPalette pal = button->palette();
    pal.setColor(QPalette::Button, fill);
    pal.setColor(QPalette::ButtonText, contrast);
    pal.setColor(QPalette::Window, surface);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Highlight, selection);
    pal.setColor(QPalette::HighlightedText, background);
    pal.setColor(QPalette::Mid, border);
    pal.setColor(QPalette::Disabled, QPalette::Button, disabledButton);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    pal.setColor(QPalette::Disabled, QPalette::Window, disabledWindow);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    pal.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    pal.setColor(QPalette::Disabled, QPalette::Mid, border.darker(120));
    button->setPalette(pal);
}

void applyInspectorComponentStateButton(QPushButton* button, const bool active)
{
    if (!button) {
        return;
    }
    applyInspectorButton(button, false);
    const auto& theme = ArtifactCore::currentDCCTheme();
    const QColor background =
        themeColor(theme.backgroundColor, QColor(QStringLiteral("#20242A")));
    const QColor surface = themeColor(theme.secondaryBackgroundColor,
                                      QColor(QStringLiteral("#2B3038")));
    const QColor selection =
        themeColor(theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
    const QColor text =
        themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
    QPalette pal = button->palette();
    pal.setColor(QPalette::Button, active ? selection : surface);
    pal.setColor(QPalette::ButtonText, active ? QColor(Qt::white) : text);
    pal.setColor(QPalette::Highlight, selection);
    pal.setColor(QPalette::HighlightedText, background);
    button->setPalette(pal);
}

class InspectorScrollBarStyle final : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;

    int pixelMetric(PixelMetric metric, const QStyleOption* option,
                    const QWidget* widget) const override
    {
        if (metric == PM_ScrollBarExtent) return 10;
        if (metric == PM_ScrollBarSliderMin) return 24;
        return QProxyStyle::pixelMetric(metric, option, widget);
    }

    void drawComplexControl(ComplexControl control,
                            const QStyleOptionComplex* option,
                            QPainter* painter,
                            const QWidget* widget = nullptr) const override
    {
        if (control != CC_ScrollBar || !option || !painter) {
            QProxyStyle::drawComplexControl(control, option, painter, widget);
            return;
        }
        const auto* slider = qstyleoption_cast<const QStyleOptionSlider*>(option);
        if (!slider) return;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        const QPalette pal = slider->palette;
        const QRect groove = subControlRect(
            CC_ScrollBar, slider, SC_ScrollBarGroove, widget);
        const QRect handle = subControlRect(
            CC_ScrollBar, slider, SC_ScrollBarSlider, widget);
        painter->setPen(Qt::NoPen);
        painter->setBrush(blendColor(pal.color(QPalette::Window),
                                     pal.color(QPalette::Mid), 0.18));
        painter->drawRoundedRect(QRectF(groove).adjusted(2, 2, -2, -2),
                                 3.0, 3.0);
        const bool handleActive =
            slider->activeSubControls.testFlag(SC_ScrollBarSlider);
        const bool pressed = handleActive &&
            slider->state.testFlag(QStyle::State_Sunken);
        const QColor handleColor = pressed
            ? pal.color(QPalette::Highlight)
            : handleActive
                  ? blendColor(pal.color(QPalette::Mid),
                               pal.color(QPalette::Highlight), 0.34)
                  : pal.color(QPalette::Mid);
        painter->setBrush(handleColor);
        painter->drawRoundedRect(QRectF(handle).adjusted(2, 2, -2, -2),
                                 3.0, 3.0);
        const auto drawArrow = [&](const SubControl subControl,
                                   const bool decrement) {
            const QRect arrowRect =
                subControlRect(CC_ScrollBar, slider, subControl, widget);
            if (!arrowRect.isValid()) return;
            const bool active = slider->activeSubControls.testFlag(subControl);
            painter->setPen(Qt::NoPen);
            painter->setBrush(active ? pal.color(QPalette::Highlight)
                                     : pal.color(QPalette::PlaceholderText));
            const QPointF center = arrowRect.center();
            if (slider->orientation == Qt::Vertical) {
                const qreal direction = decrement ? -1.0 : 1.0;
                const QPointF points[] = {
                    QPointF(center.x() - 3.0, center.y() - 2.0 * direction),
                    QPointF(center.x() + 3.0, center.y() - 2.0 * direction),
                    QPointF(center.x(), center.y() + 2.5 * direction)};
                painter->drawPolygon(points, 3);
            } else {
                const qreal direction = decrement ? -1.0 : 1.0;
                const QPointF points[] = {
                    QPointF(center.x() - 2.0 * direction, center.y() - 3.0),
                    QPointF(center.x() - 2.0 * direction, center.y() + 3.0),
                    QPointF(center.x() + 2.5 * direction, center.y())};
                painter->drawPolygon(points, 3);
            }
        };
        drawArrow(SC_ScrollBarSubLine, true);
        drawArrow(SC_ScrollBarAddLine, false);
        painter->restore();
    }
};

void applyInspectorOwnerDrawScrollBars(QAbstractScrollArea* area)
{
    if (!area) return;
    for (auto* bar : {area->verticalScrollBar(), area->horizontalScrollBar()}) {
        if (!bar) continue;
        auto* style = new InspectorScrollBarStyle();
        style->setParent(bar);
        bar->setStyle(style);
    }
}

} // namespace Artifact

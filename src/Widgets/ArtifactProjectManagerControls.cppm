module;

#include <QColor>
#include <QDialog>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QWidget>

#include <functional>

export module Artifact.Widgets.ProjectManagerControls;

import FloatColorPickerDialog;

export namespace Artifact {

namespace detail {

class ProjectActionLabel final : public QLabel
{
public:
    using QLabel::QLabel;
    std::function<void()> activated;

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event && event->button() == Qt::LeftButton && activated) {
            activated();
            event->accept();
            return;
        }
        QLabel::mouseReleaseEvent(event);
    }
};

void updateCompositionColorButtonPreview(QPushButton* button, const QColor& color)
{
    if (!button) {
        return;
    }
    qreal dpr = button->devicePixelRatioF();
    if (dpr < 1.0) dpr = 1.0;
    QSize logicalSize = button->size().isEmpty() ? QSize(40, 24) : button->size();
    QSize dprSize = logicalSize * dpr;
    QPixmap pix(dprSize);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);
    {
        QPainter painter(&pix);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(85, 85, 85), 1));
        painter.setBrush(color);
        painter.drawRoundedRect(QRect(QPoint(0, 0), dprSize).adjusted(1, 1, -2, -2), 3, 3);
    }
    button->setIcon(QIcon(pix));
    button->setIconSize(logicalSize);
    button->setToolTip(QStringLiteral("Background Color: %1").arg(color.name(QColor::HexArgb)));
    button->setAccessibleName(QStringLiteral("Composition background color"));
    button->setAccessibleDescription(
        QStringLiteral("Choose the composition background color, currently %1").arg(color.name(QColor::HexArgb)));
    button->setText(QString());
}

class CompositionBackgroundColorButton final : public QPushButton
{
public:
    std::function<void(const QColor&)> previewChanged;

    explicit CompositionBackgroundColorButton(const QColor& initialColor,
                                              QWidget* parent = nullptr)
        : QPushButton(parent), color_(initialColor)
    {
        updateCompositionColorButtonPreview(this, color_);
    }

    QColor selectedColor() const
    {
        return color_;
    }

    void setSelectedColor(const QColor& color)
    {
        if (!color.isValid()) {
            return;
        }
        color_ = color;
        updateCompositionColorButtonPreview(this, color_);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event && event->button() == Qt::LeftButton) {
            const QColor originalColor = color_;
            ArtifactWidgets::FloatColorPicker picker(this);
            picker.setWindowTitle(QStringLiteral("Background Color"));
            picker.setInitialColor(ArtifactCore::FloatColor(
                color_.redF(), color_.greenF(), color_.blueF(), color_.alphaF()));
            picker.setColor(ArtifactCore::FloatColor(
                color_.redF(), color_.greenF(), color_.blueF(), color_.alphaF()));
            QObject::connect(&picker, &ArtifactWidgets::FloatColorPicker::colorChanged,
                             this, [this](const ArtifactCore::FloatColor& picked) {
                const QColor liveColor = QColor::fromRgbF(
                    picked.r(), picked.g(), picked.b(), picked.a());
                setSelectedColor(liveColor);
                if (previewChanged) {
                    previewChanged(liveColor);
                }
            });
            if (picker.exec() == QDialog::Accepted) {
                const ArtifactCore::FloatColor picked = picker.getColor();
                const QColor acceptedColor = QColor::fromRgbF(
                    picked.r(), picked.g(), picked.b(), picked.a());
                setSelectedColor(acceptedColor);
                if (previewChanged) {
                    previewChanged(acceptedColor);
                }
            } else {
                setSelectedColor(originalColor);
                if (previewChanged) {
                    previewChanged(originalColor);
                }
            }
            event->accept();
            return;
        }
        QPushButton::mousePressEvent(event);
    }

private:
    QColor color_;
};

} // namespace detail
} // namespace Artifact

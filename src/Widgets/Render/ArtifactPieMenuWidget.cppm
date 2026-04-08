module;
#include <utility>
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QIcon>
#include <cmath>
#include <algorithm>
#include <wobjectimpl.h>

module Artifact.Widgets.PieMenu;

namespace Artifact {

W_OBJECT_IMPL(ArtifactPieMenuWidget)

class ArtifactPieMenuWidget::Impl {
public:
    PieMenuModel model;
    QPoint center;
    QPoint mousePos;
    int selectedIndex = -1;
    
    // Styling
    float innerRadius = 40.0f;
    float outerRadius = 140.0f;
    float deadZoneRadius = 30.0f;
    QColor backgroundColor = QColor(45, 45, 48, 200);
    QColor highlightColor = QColor(0, 122, 204, 220);
    QColor borderColor = QColor(100, 100, 100, 150);
    QColor textColor = QColor(220, 220, 220);
    
    void updateSelection() {
        QPoint delta = mousePos - center;
        float dist = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
        
        if (dist < deadZoneRadius || model.items.empty()) {
            selectedIndex = -1;
            return;
        }
        
        float angle = std::atan2(-delta.y(), delta.x()) * 180.0f / M_PI;
        if (angle < 0) angle += 360.0f;
        
        int count = static_cast<int>(model.items.size());
        float sectorSize = 360.0f / count;
        
        // Offset angle to center the first sector at the top (90 degrees)
        float normalizedAngle = angle - (90.0f - sectorSize / 2.0f);
        while (normalizedAngle < 0) normalizedAngle += 360.0f;
        while (normalizedAngle >= 360.0f) normalizedAngle -= 360.0f;
        
        selectedIndex = static_cast<int>(normalizedAngle / sectorSize);
        if (selectedIndex >= count) selectedIndex = 0;
    }
};

ArtifactPieMenuWidget::ArtifactPieMenuWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    hide();
}

ArtifactPieMenuWidget::~ArtifactPieMenuWidget() = default;

void ArtifactPieMenuWidget::setModel(const PieMenuModel& model) {
    impl_->model = model;
    update();
}

void ArtifactPieMenuWidget::showAt(const QPoint& pos) {
    impl_->center = pos;
    impl_->mousePos = pos;
    impl_->selectedIndex = -1;
    
    // Position widget to cover the relevant area around center
    int size = static_cast<int>(impl_->outerRadius * 2.2f);
    setGeometry(pos.x() - size / 2, pos.y() - size / 2, size, size);
    impl_->center = QPoint(width() / 2, height() / 2);
    impl_->mousePos = impl_->center;
    
    show();
    raise();
    update();
}

void ArtifactPieMenuWidget::updateMousePos(const QPoint& pos) {
    impl_->mousePos = mapFromGlobal(pos);
    impl_->updateSelection();
    update();
}

QString ArtifactPieMenuWidget::confirmSelection() {
    QString result;
    if (impl_->selectedIndex >= 0 && impl_->selectedIndex < static_cast<int>(impl_->model.items.size())) {
        auto& item = impl_->model.items[impl_->selectedIndex];
        if (item.enabled) {
            result = item.commandId;
            if (item.action) {
                item.action();
            }
        }
    }
    hide();
    return result;
}

void ArtifactPieMenuWidget::cancel() {
    hide();
}

void ArtifactPieMenuWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int count = static_cast<int>(impl_->model.items.size());
    if (count == 0) return;
    
    float sectorSize = 360.0f / count;
    QPointF center(impl_->center);
    
    for (int i = 0; i < count; ++i) {
        bool isSelected = (i == impl_->selectedIndex);
        const auto& item = impl_->model.items[i];
        
        float startAngle = 90.0f - (i + 1) * sectorSize + sectorSize / 2.0f;
        
        QPainterPath path;
        path.arcMoveTo(center.x() - impl_->outerRadius, center.y() - impl_->outerRadius, 
                       impl_->outerRadius * 2, impl_->outerRadius * 2, startAngle);
        path.arcTo(center.x() - impl_->outerRadius, center.y() - impl_->outerRadius, 
                   impl_->outerRadius * 2, impl_->outerRadius * 2, startAngle, sectorSize);
        path.arcTo(center.x() - impl_->innerRadius, center.y() - impl_->innerRadius, 
                   impl_->innerRadius * 2, impl_->innerRadius * 2, startAngle + sectorSize, -sectorSize);
        path.closeSubpath();
        
        painter.setPen(QPen(impl_->borderColor, 1.5));
        painter.setBrush(isSelected ? impl_->highlightColor : impl_->backgroundColor);
        painter.drawPath(path);
        
        // Draw Icon and Text
        float midAngle = (startAngle + sectorSize / 2.0f) * M_PI / 180.0f;
        float textRadius = (impl_->innerRadius + impl_->outerRadius) / 2.0f;
        QPointF itemCenter(center.x() + std::cos(midAngle) * textRadius,
                           center.y() - std::sin(midAngle) * textRadius);
        
        if (!item.icon.isNull()) {
            QPixmap pixmap = item.icon.pixmap(24, 24, item.enabled ? QIcon::Normal : QIcon::Disabled);
            painter.drawPixmap(itemCenter.x() - 12, itemCenter.y() - 20, pixmap);
        }
        
        painter.setPen(item.enabled ? impl_->textColor : QColor(130, 130, 130));
        QFont font = painter.font();
        font.setPixelSize(10);
        font.setBold(isSelected);
        painter.setFont(font);
        
        QRectF textRect(itemCenter.x() - sectorSize * 0.8f, itemCenter.y() + 4, sectorSize * 1.6f, 20);
        painter.drawText(textRect, Qt::AlignCenter, item.label);
    }
    
    // Center Circle (Title or empty)
    painter.setBrush(impl_->backgroundColor);
    painter.setPen(QPen(impl_->borderColor, 2));
    painter.drawEllipse(center, impl_->innerRadius - 2, impl_->innerRadius - 2);
    
    if (!impl_->model.title.isEmpty()) {
        painter.setPen(impl_->textColor);
        QFont font = painter.font();
        font.setPixelSize(11);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(QRectF(center.x() - impl_->innerRadius, center.y() - impl_->innerRadius, 
                                impl_->innerRadius * 2, impl_->innerRadius * 2), 
                         Qt::AlignCenter, impl_->model.title);
    }
}

} // namespace Artifact

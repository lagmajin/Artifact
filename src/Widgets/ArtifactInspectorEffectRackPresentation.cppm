module;

#include <QFont>
#include <QFontMetrics>
#include <QModelIndex>
#include <QPainter>
#include <QRectF>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStyleOptionViewItem>

export module Artifact.Widgets.InspectorEffectRackPresentation;

import Widgets.CommonStyle;
import Artifact.Widgets.InspectorStyle;
import Widgets.Utils.CSS;

namespace Artifact {

namespace {

constexpr int kEffectRackEnabledRole = Qt::UserRole + 1;
constexpr int kEffectRackHasMaskRole = Qt::UserRole + 2;
constexpr int kEffectRackNameRole = Qt::UserRole + 3;
constexpr int kEffectRackMaskCountRole = Qt::UserRole + 4;

QColor rackColorForIndex(const int rackIndex, const QColor &base,
                         const QColor &accent) {
  switch (rackIndex) {
  case 0:
  case 1:
    return blendColor(base, accent.lighter(108), 0.16);
  case 2:
    return blendColor(base, accent, 0.10);
  case 3:
    return blendColor(base, accent.darker(108), 0.14);
  case 4:
    return blendColor(base, accent, 0.18);
  default:
    return base;
  }
}

class EffectRackItemDelegate final : public QStyledItemDelegate {
public:
  explicit EffectRackItemDelegate(const int rackIndex, QObject *parent)
      : QStyledItemDelegate(parent), rackIndex_(rackIndex) {}

  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
    return QSize(0, 34);
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!painter) {
      return;
    }

    const auto &theme = ArtifactCore::currentDCCTheme();
    const QColor background = themeColor(
        theme.backgroundColor, QColor(QStringLiteral("#20242A")));
    const QColor surface = themeColor(
        theme.secondaryBackgroundColor, QColor(QStringLiteral("#2B3038")));
    const QColor text = themeColor(theme.textColor, QColor(QStringLiteral("#E3E7EC")));
    const QColor accent = themeColor(theme.accentColor, QColor(QStringLiteral("#5E94C7")));
    const QColor selection = themeColor(
        theme.selectionColor, QColor(QStringLiteral("#3C5B76")));
    const QColor rackColor = rackColorForIndex(rackIndex_, text, accent);
    const QColor muted = blendColor(rackColor, background, 0.58);
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const bool enabled = index.data(kEffectRackEnabledRole).toBool();
    const bool hasMask = index.data(kEffectRackHasMaskRole).toBool();
    const int maskCount = index.data(kEffectRackMaskCountRole).toInt();
    const QString effectId = index.data(Qt::UserRole).toString().trimmed();
    const QString effectName = index.data(kEffectRackNameRole).toString().trimmed();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRect rowRect = option.rect.adjusted(2, 2, -2, -2);
    painter->setPen(selected ? rackColor : blendColor(surface, text, 0.18));
    painter->setBrush(selected ? selection
                               : hovered ? blendColor(surface, accent, 0.14)
                                         : surface);
    painter->drawRoundedRect(QRectF(rowRect), 3.0, 3.0);

    if (effectId.isEmpty()) {
      painter->setPen(blendColor(text, background, 0.52));
      painter->drawText(rowRect, Qt::AlignCenter, index.data(Qt::DisplayRole).toString());
      painter->restore();
      return;
    }

    const QPoint indicator(rowRect.left() + 10, rowRect.center().y());
    painter->setPen(Qt::NoPen);
    painter->setBrush(enabled ? rackColor : muted);
    painter->drawEllipse(indicator, 4, 4);

    QRect textRect = rowRect.adjusted(22, 0, -6, 0);
    if (hasMask) {
      const QString maskLabel = maskCount > 0
                                    ? QStringLiteral("Mask %1").arg(maskCount)
                                    : QStringLiteral("Mask");
      QFontMetrics metrics(option.font);
      const int chipWidth = metrics.horizontalAdvance(maskLabel) + 12;
      const QRect chipRect(rowRect.right() - chipWidth, rowRect.center().y() - 9,
                           chipWidth, 18);
      painter->setBrush(blendColor(rackColor, background, selected ? 0.28 : 0.16));
      painter->drawRoundedRect(chipRect, 4, 4);
      painter->setPen(enabled ? text : muted);
      painter->drawText(chipRect, Qt::AlignCenter, maskLabel);
      textRect.setRight(chipRect.left() - 6);
    }

    QFont nameFont = option.font;
    nameFont.setWeight(QFont::DemiBold);
    painter->setFont(nameFont);
    painter->setPen(enabled ? text : muted);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                      effectName.isEmpty() ? index.data(Qt::DisplayRole).toString()
                                           : effectName);
    painter->restore();
  }

private:
  int rackIndex_ = 0;
};

} // namespace

export QStyledItemDelegate *createInspectorEffectRackItemDelegate(
    const int rackIndex, QObject *parent) {
  return new EffectRackItemDelegate(rackIndex, parent);
}

} // namespace Artifact

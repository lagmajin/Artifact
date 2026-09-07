module;

#include <QFont>
#include <QIcon>
#include <QVariant>
#include <QFontMetrics>
#include <QModelIndex>
#include <QPainter>
#include <QRectF>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QSize>
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

  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const override {
    const auto hint = index.data(Qt::SizeHintRole).toSize();
    return hint.isValid() ? hint : QSize(0, 44);
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
    const QColor rackColor = rackColorForIndex(rackIndex_, text, accent);
    const QColor muted = blendColor(rackColor, background, 0.58);
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const bool enabled = index.data(kEffectRackEnabledRole).toBool();
    const bool hasMask = index.data(kEffectRackHasMaskRole).toBool();
    const QString effectName = index.data(kEffectRackNameRole).toString().trimmed();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRect rowRect = option.rect.adjusted(1, 1, -1, -1);
    painter->fillRect(rowRect, hovered ? blendColor(surface, accent, 0.08) : surface);
    if (selected) painter->fillRect(QRect(rowRect.left(), rowRect.top(), 2, rowRect.height()), accent);
    const QRect header(rowRect.left(), rowRect.top(), rowRect.width(), 42);
    painter->setPen(Qt::NoPen);
    painter->setBrush(muted);
    for (int y : {-4, 0, 4}) {
      painter->drawEllipse(QPoint(header.left() + 9, header.center().y() + y), 1, 1);
      painter->drawEllipse(QPoint(header.left() + 13, header.center().y() + y), 1, 1);
    }
    painter->setPen(muted);
    painter->drawText(QRect(header.left() + 21, header.top(), 26, header.height()),
                      Qt::AlignCenter, QStringLiteral("%1").arg(index.row() + 1, 2, 10, QLatin1Char('0')));
    const QIcon icon(QStringLiteral(":/icons/Studio/effectrack_effect.svg"));
    icon.paint(painter, QRect(header.left() + 51, header.center().y() - 9, 18, 18),
               Qt::AlignCenter, enabled ? QIcon::Normal : QIcon::Disabled);
    QFont nameFont = option.font;
    nameFont.setWeight(QFont::DemiBold);
    painter->setFont(nameFont);
    painter->setPen(enabled ? text : muted);
    const QRect nameRect = header.adjusted(78, 0, -78, 0);
    const QString label = effectName.isEmpty() ? index.data(Qt::DisplayRole).toString() : effectName;
    painter->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
        QFontMetrics(nameFont).elidedText(label, Qt::ElideRight, qMax(0, nameRect.width())));
    // Match the header hit regions in EffectRackList.
    const QRect toggle(header.right() - 65, header.center().y() - 7, 30, 14);
    painter->setPen(Qt::NoPen);
    painter->setBrush(enabled ? accent : muted);
    painter->drawRoundedRect(toggle, 7, 7);
    painter->setBrush(enabled ? text : background);
    painter->drawEllipse(QPoint(enabled ? toggle.right() - 7 : toggle.left() + 7,
                               toggle.center().y()), 5, 5);
    painter->setBrush(text);
    for (int y : {-4, 0, 4})
      painter->drawEllipse(QPoint(header.right() - 14, header.center().y() + y), 1, 1);
    if (hasMask) {
      painter->setPen(accent);
      painter->drawText(QRect(header.left() + 51, header.bottom() - 9, 18, 9),
                        Qt::AlignCenter, QStringLiteral("M"));
    }
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

module;
#include <QDrag>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPixmap>
#include <QVBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <wobjectimpl.h>

module Artifact.Widgets.EffectPalette;

import Artifact.Service.Effect;
import Utils.Path;
import std;

namespace Artifact {
namespace {
constexpr auto kEffectAddMime = "application/x-artifact-effect-add";

class EffectList final : public QListWidget {
public:
  explicit EffectList(QWidget* parent) : QListWidget(parent) {
    setDragEnabled(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setViewMode(QListView::ListMode);
    setUniformItemSizes(true);
  }

protected:
  void startDrag(Qt::DropActions actions) override {
    auto* item = currentItem();
    if (!item || item->data(Qt::UserRole).toString().isEmpty()) return;
    auto* mime = new QMimeData();
    mime->setData(kEffectAddMime, item->data(Qt::UserRole).toString().toUtf8());
    mime->setText(item->text());
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(item->icon().pixmap(24, 24));
    drag->exec(actions & Qt::CopyAction ? Qt::CopyAction : Qt::MoveAction);
  }
};

class EffectFilter final : public QLineEdit {
public:
  explicit EffectFilter(ArtifactEffectPalette* palette) : QLineEdit(palette), palette_(palette) {}
protected:
  void keyReleaseEvent(QKeyEvent* event) override {
    QLineEdit::keyReleaseEvent(event);
    palette_->refreshEffects();
  }
private:
  ArtifactEffectPalette* palette_;
};
}

W_OBJECT_IMPL(ArtifactEffectPalette)

class ArtifactEffectPalette::Impl {
public:
  EffectFilter* filter = nullptr;
  EffectList* list = nullptr;
};

ArtifactEffectPalette::ArtifactEffectPalette(QWidget* parent) : QWidget(parent), impl_(new Impl) {
  setObjectName(QStringLiteral("ArtifactEffectPalette"));
  setWindowTitle(QStringLiteral("Effect Palette"));
  auto* layout = new QVBoxLayout(this);
  impl_->filter = new EffectFilter(this);
  impl_->filter->setPlaceholderText(QStringLiteral("エフェクトを検索"));
  impl_->list = new EffectList(this);
  layout->addWidget(impl_->filter);
  layout->addWidget(impl_->list, 1);
  refreshEffects();
}

ArtifactEffectPalette::~ArtifactEffectPalette() { delete impl_; }

void ArtifactEffectPalette::refreshEffects() {
  if (!impl_ || !impl_->list) return;
  const QString filter = impl_->filter ? impl_->filter->text().trimmed() : QString();
  impl_->list->clear();
  auto* service = ArtifactEffectService::instance();
  if (!service) return;
  for (const auto& info : service->availableEffects()) {
    if (!filter.isEmpty() && !info.displayName.contains(filter, Qt::CaseInsensitive) &&
        !info.id.toString().contains(filter, Qt::CaseInsensitive)) continue;
    auto* item = new QListWidgetItem(info.displayName, impl_->list);
    item->setData(Qt::UserRole, info.id.toString());
    item->setIcon(QIcon(resolveIconPath(QStringLiteral("Studio/effect_ops_generate.svg"))));
    item->setToolTip(info.id.toString());
  }
}
}

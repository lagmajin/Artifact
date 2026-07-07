module;
#include <wobjectimpl.h>
#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QSizePolicy>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QSignalBlocker>
#include <QEvent>
#include <QShowEvent>
#include <QDebug>
#include <cmath>
#include <memory>

module Artifact.Widgets.CoordinateManager;

import std;
import Artifact.Widgets.RelativeSpinBox;
import Event.Bus;
import Artifact.Event.Types;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Layer.Abstract;

namespace Artifact {
using namespace ArtifactCore;

class ArtifactCoordinateManagerWidget::Impl {
public:
    explicit Impl(ArtifactCoordinateManagerWidget* parent)
        : parent_(parent) { buildUi(); subscribeEvents(); }
    ~Impl() { for (auto& s : eventBusSubscriptions_) eventBus_.unsubscribe(s); }


    void buildUi() {
        auto* main = new QHBoxLayout(parent_);
        main->setContentsMargins(4, 2, 4, 2);
        main->setSpacing(2);

        spaceBtn_ = new QToolButton(parent_);
        spaceBtn_->setText(QStringLiteral("L"));
        spaceBtn_->setToolTip(QStringLiteral("Local / World space"));
        spaceBtn_->setCheckable(true);
        spaceBtn_->setFixedSize(22, 22);
        QFont sf = spaceBtn_->font(); sf.setBold(true); sf.setPointSize(7);
        spaceBtn_->setFont(sf);
        QObject::connect(spaceBtn_, &QToolButton::clicked, parent_, [this]() { toggleSpace(); });
        main->addWidget(spaceBtn_);
        main->addSpacing(2);

        posLabel_ = addLabel(main, QStringLiteral("P"));
        posXSpin_ = addSpin(main, QStringLiteral("X"), -99999., 99999.);
        posYSpin_ = addSpin(main, QStringLiteral("Y"), -99999., 99999.);
        main->addSpacing(4);

        rotLabel_ = addLabel(main, QStringLiteral("R"));
        rotZSpin_ = addSpin(main, QString(), -3600., 3600.);
        main->addSpacing(4);

        sclLabel_ = addLabel(main, QStringLiteral("S"));
        sclXSpin_ = addSpin(main, QStringLiteral("X"), 0.001, 99999.);
        sclYSpin_ = addSpin(main, QStringLiteral("Y"), 0.001, 99999.);

        // Alt+Click defaults (C4D-style reset)
        posXSpin_->setDefaultValue(0.0);
        posYSpin_->setDefaultValue(0.0);
        rotZSpin_->setDefaultValue(0.0);
        sclXSpin_->setDefaultValue(1.0);
        sclYSpin_->setDefaultValue(1.0);

        setInputsEnabled(false);
        parent_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        parent_->setFixedHeight(30);
    }

    QLabel* addLabel(QHBoxLayout* l, const QString& t) {
        auto* lb = new QLabel(t, parent_);
        QFont f = lb->font(); f.setBold(true); f.setPointSize(7);
        lb->setFont(f); lb->setFixedWidth(12);
        lb->setAlignment(Qt::AlignCenter);
        l->addWidget(lb);
        return lb;
    }

    ArtifactRelativeDoubleSpinBox* addSpin(QHBoxLayout* l, const QString& pfx, double lo, double hi) {
        auto* sp = new ArtifactRelativeDoubleSpinBox(parent_);
        sp->setRange(lo, hi); sp->setDecimals(3);
        sp->setSingleStep(1.0); sp->setFixedWidth(64);
        sp->setAlignment(Qt::AlignRight);
        sp->setButtonSymbols(QAbstractSpinBox::NoButtons);
        sp->setKeyboardTracking(false);
        QFont sf = sp->font(); sf.setPointSize(8); sp->setFont(sf);
        if (!pfx.isEmpty()) sp->setPrefix(pfx + QStringLiteral(" "));
        QObject::connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            parent_, [this](double) { applyToSelection(); });
        l->addWidget(sp);
        return sp;
    }



    void setInputsEnabled(bool e) {
        posXSpin_->setEnabled(e); posYSpin_->setEnabled(e);
        rotZSpin_->setEnabled(e);
        sclXSpin_->setEnabled(e); sclYSpin_->setEnabled(e);
    }

    void refreshFromSelection() {
        auto* app = ArtifactApplicationManager::instance();
        auto* sel = app ? app->layerSelectionManager() : nullptr;
        if (!sel) { setInputsEnabled(false); return; }
        auto layer = sel->currentLayer();
        if (!layer) {
            setInputsEnabled(false);
            QSignalBlocker b1(posXSpin_); QSignalBlocker b2(posYSpin_);
            QSignalBlocker b3(rotZSpin_);
            QSignalBlocker b4(sclXSpin_); QSignalBlocker b5(sclYSpin_);
            posXSpin_->setValue(0.); posYSpin_->setValue(0.);
            rotZSpin_->setValue(0.);
            sclXSpin_->setValue(0.); sclYSpin_->setValue(0.);
            return;
        }
        setInputsEnabled(true);
        float px=0,py=0,sx=1,sy=1,r=0;
        layer->transform2D().position(px, py);
        layer->transform2D().scale(sx, sy);
        r = layer->transform2D().rotation();
        QSignalBlocker b1(posXSpin_); QSignalBlocker b2(posYSpin_);
        QSignalBlocker b3(rotZSpin_);
        QSignalBlocker b4(sclXSpin_); QSignalBlocker b5(sclYSpin_);
        posXSpin_->setValue(static_cast<double>(px));
        posYSpin_->setValue(static_cast<double>(py));
        rotZSpin_->setValue(static_cast<double>(r));
        sclXSpin_->setValue(static_cast<double>(sx));
        sclYSpin_->setValue(static_cast<double>(sy));
    }

    void applyToSelection() {
        auto* app = ArtifactApplicationManager::instance();
        auto* sel = app ? app->layerSelectionManager() : nullptr;
        if (!sel) return;
        auto layer = sel->currentLayer();
        if (!layer) return;
        layer->transform2D().setPosition(
            static_cast<float>(posXSpin_->value()),
            static_cast<float>(posYSpin_->value()));
        layer->transform2D().setRotation(
            static_cast<float>(rotZSpin_->value()));
        layer->transform2D().setScale(
            static_cast<float>(sclXSpin_->value()),
            static_cast<float>(sclYSpin_->value()));
    }

    void setSpace(Space s) {
        if (space_ == s) return;
        space_ = s;
        spaceBtn_->setChecked(s == Space::World);
        spaceBtn_->setText(s == Space::World ? QStringLiteral("W") : QStringLiteral("L"));
        refreshFromSelection();
    }
    void toggleSpace() { setSpace(space_ == Space::World ? Space::Local : Space::World); }
    Space space() const { return space_; }

    void subscribeEvents() {
        eventBusSubscriptions_.push_back(
            eventBus_.subscribe<LayerSelectionChangedEvent>(
                [this](const LayerSelectionChangedEvent&) { refreshFromSelection(); }));
    }

    ArtifactCoordinateManagerWidget* parent_;
    EventBus eventBus_;
    std::vector<EventBus::Subscription> eventBusSubscriptions_;
    QToolButton* spaceBtn_ = nullptr;
    QLabel *posLabel_=nullptr, *rotLabel_=nullptr, *sclLabel_=nullptr;
    ArtifactRelativeDoubleSpinBox *posXSpin_=nullptr, *posYSpin_=nullptr;
    ArtifactRelativeDoubleSpinBox *rotZSpin_=nullptr;
    ArtifactRelativeDoubleSpinBox *sclXSpin_=nullptr, *sclYSpin_=nullptr;
    Space space_ = Space::Local;
};


// ---------------------------------------------------------------------------
// ArtifactCoordinateManagerWidget public API
// ---------------------------------------------------------------------------

W_OBJECT_IMPL(ArtifactCoordinateManagerWidget)

ArtifactCoordinateManagerWidget::ArtifactCoordinateManagerWidget(QWidget* parent)
    : QWidget(parent)
    , impl_(new Impl(this))
{
    setObjectName(QStringLiteral("ArtifactCoordinateManager"));
}

ArtifactCoordinateManagerWidget::~ArtifactCoordinateManagerWidget()
{
    delete impl_;
}

void ArtifactCoordinateManagerWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    impl_->refreshFromSelection();
}

void ArtifactCoordinateManagerWidget::setSpace(Space space)
{
    impl_->setSpace(space);
}

ArtifactCoordinateManagerWidget::Space ArtifactCoordinateManagerWidget::space() const
{
    return impl_->space();
}

void ArtifactCoordinateManagerWidget::toggleSpace()
{
    impl_->toggleSpace();
}

void ArtifactCoordinateManagerWidget::refreshFromSelection()
{
    impl_->refreshFromSelection();
}

} // namespace Artifact
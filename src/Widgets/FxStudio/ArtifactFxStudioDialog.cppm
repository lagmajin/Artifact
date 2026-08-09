module;
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
module Artifact.Widgets.FxStudio.Dialog;
import Artifact.FxStudio.Session;
import Artifact.FxStudio.PresetCatalog;
import Core.ArtifactString;
namespace Artifact { namespace { QString q(const ArtifactCore::String& s){ return QString::fromStdString(ArtifactCore::toStdString(s)); } ArtifactCore::String a(const QString& s){return ArtifactCore::String(s.toStdString());} }
class ArtifactFxStudioDialog::Impl { public: FxStudio::Session session; QComboBox* presets{}; QTreeWidget* events{}; QSpinBox* start{}; QSpinBox* length{}; QDoubleSpinBox* strength{}; QSpinBox* preview{}; QTextEdit* cues{}; bool syncing=false;
void refresh(){ syncing=true; events->clear(); for(const auto& e:session.eventTrack().events()){auto*r=new QTreeWidgetItem(events);r->setData(0,Qt::UserRole,QVariant::fromValue<qulonglong>(e.id));r->setText(0,QStringLiteral("Event %1").arg(e.id));r->setText(1,QString::number(e.startFrame));r->setText(2,QString::number(e.sequence.durationFrames));r->setText(3,QString::number(e.sequence.strength,'f',2));} if(auto*e=session.selectedEvent()){start->setValue(int(e->startFrame));length->setValue(int(e->sequence.durationFrames));strength->setValue(e->sequence.strength);} syncing=false; sample(); }
void sample(){QStringList out;for(const auto&s:session.viewport().visibleSamples(session.eventTrack(),preview->value())) out<<QStringLiteral("Event %1  Cue %2  strength %3  seed %4").arg(s.eventId).arg(int(s.cue.kind)).arg(s.cue.strength,0,'f',2).arg(s.cue.seed);cues->setPlainText(out.isEmpty()?QStringLiteral("このフレームに有効な FX Cue はありません。") : out.join('\n'));}
};
ArtifactFxStudioDialog::ArtifactFxStudioDialog(QWidget* parent):QDialog(parent),impl_(new Impl){setWindowTitle(QStringLiteral("FX Studio"));resize(820,500);auto*root=new QVBoxLayout(this);root->addWidget(new QLabel(QStringLiteral("FX Studio — 時間ベースのエフェクト演出"),this));auto*row=new QHBoxLayout;impl_->presets=new QComboBox(this);for(const auto&p:FxStudio::PresetCatalog::descriptors())impl_->presets->addItem(q(p.name),q(p.id));auto*add=new QPushButton(QStringLiteral("現在フレームに追加"),this);auto*save=new QPushButton(QStringLiteral("保存…"),this);auto*load=new QPushButton(QStringLiteral("読み込む…"),this);row->addWidget(impl_->presets,1);row->addWidget(add);row->addWidget(load);row->addWidget(save);root->addLayout(row);impl_->events=new QTreeWidget(this);impl_->events->setHeaderLabels({QStringLiteral("Event"),QStringLiteral("Start"),QStringLiteral("Length"),QStringLiteral("Strength")});impl_->events->header()->setSectionResizeMode(0,QHeaderView::Stretch);root->addWidget(impl_->events,1);auto*form=new QFormLayout;impl_->start=new QSpinBox(this);impl_->length=new QSpinBox(this);impl_->strength=new QDoubleSpinBox(this);impl_->preview=new QSpinBox(this);for(auto*w:{impl_->start,impl_->length,impl_->preview})w->setRange(0,100000);impl_->length->setMinimum(1);impl_->strength->setRange(0,1);impl_->strength->setSingleStep(.05);form->addRow(QStringLiteral("Start"),impl_->start);form->addRow(QStringLiteral("Length"),impl_->length);form->addRow(QStringLiteral("Strength"),impl_->strength);form->addRow(QStringLiteral("Preview Frame"),impl_->preview);root->addLayout(form);impl_->cues=new QTextEdit(this);impl_->cues->setReadOnly(true);root->addWidget(impl_->cues);auto*buttons=new QDialogButtonBox(QDialogButtonBox::Close,this);auto*remove=buttons->addButton(QStringLiteral("選択イベントを削除"),QDialogButtonBox::DestructiveRole);root->addWidget(buttons);
connect(add,&QPushButton::clicked,this,[this]{impl_->session.insertPreset(a(impl_->presets->currentData().toString()),impl_->preview->value());impl_->refresh();});connect(impl_->events,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem*r,QTreeWidgetItem*){if(r)impl_->session.selectEvent(r->data(0,Qt::UserRole).toULongLong());impl_->refresh();});connect(impl_->start,qOverload<int>(&QSpinBox::valueChanged),this,[this](int v){if(!impl_->syncing){impl_->session.moveSelectedEvent(v);impl_->refresh();}});connect(impl_->length,qOverload<int>(&QSpinBox::valueChanged),this,[this](int v){if(!impl_->syncing){impl_->session.resizeSelectedEvent(v);impl_->refresh();}});connect(impl_->strength,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double v){if(!impl_->syncing){impl_->session.setSelectedEventStrength(float(v));impl_->refresh();}});connect(impl_->preview,qOverload<int>(&QSpinBox::valueChanged),this,[this](int){impl_->sample();});connect(remove,&QPushButton::clicked,this,[this]{if(impl_->session.removeSelectedEvent())impl_->refresh();});connect(save,&QPushButton::clicked,this,[this]{auto p=QFileDialog::getSaveFileName(this,QStringLiteral("FX Studio Session を保存"),QStringLiteral("fx.fxsession"),QStringLiteral("FX Session (*.fxsession)"));if(!p.isEmpty()){QFile f(p);if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(impl_->session.toJson()).toJson());}});connect(load,&QPushButton::clicked,this,[this]{auto p=QFileDialog::getOpenFileName(this,QStringLiteral("FX Studio Session を読み込む"),{},QStringLiteral("FX Session (*.fxsession)"));if(!p.isEmpty()){QFile f(p);if(f.open(QIODevice::ReadOnly)&&impl_->session.fromJson(QJsonDocument::fromJson(f.readAll()).object()))impl_->refresh();}});connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);impl_->refresh();}
ArtifactFxStudioDialog::~ArtifactFxStudioDialog(){delete impl_;}
}

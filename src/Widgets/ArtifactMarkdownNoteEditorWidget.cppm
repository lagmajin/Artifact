#include <wobjectimpl.h>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QFontDatabase>
#include <QShowEvent>
#include <QPalette>
#include <QColor>
#include <QString>
#include <vector>
#include <compare>

module Artifact.Widgets.MarkdownNoteEditorWidget;

import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.Project;
import Event.Bus;
import Artifact.Event.Types;
import Widgets.Utils.CSS;
import Utils.Id;

namespace Artifact {

namespace {
QString targetLabel(MarkdownNoteTarget target)
{
  switch (target) {
  case MarkdownNoteTarget::Composition:
    return QStringLiteral("Composition Note");
  case MarkdownNoteTarget::Layer:
    return QStringLiteral("Layer Note");
  }
  return QStringLiteral("Note");
}
}

class ArtifactMarkdownNoteEditorWidget::Impl {
public:
  Impl(ArtifactMarkdownNoteEditorWidget* owner, MarkdownNoteTarget target)
      : owner_(owner), target_(target) {}

  ArtifactMarkdownNoteEditorWidget* owner_ = nullptr;
  MarkdownNoteTarget target_ = MarkdownNoteTarget::Composition;
  QLabel* titleLabel_ = nullptr;
  QLabel* subtitleLabel_ = nullptr;
  QPlainTextEdit* editor_ = nullptr;
  QMetaObject::Connection noteConnection_;
  ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
  std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
  bool updating_ = false;
  CompositionID currentCompositionId_;
  LayerID currentLayerId_;

  void setupUi()
  {
    auto* layout = new QVBoxLayout(owner_);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    titleLabel_ = new QLabel(targetLabel(target_), owner_);
    titleLabel_->setObjectName(QStringLiteral("markdownNoteTitle"));
    subtitleLabel_ = new QLabel(QStringLiteral("Plain markdown editor"), owner_);
    subtitleLabel_->setObjectName(QStringLiteral("markdownNoteSubtitle"));

    editor_ = new QPlainTextEdit(owner_);
    editor_->setObjectName(QStringLiteral("markdownNoteEditor"));
    editor_->setPlaceholderText(
        target_ == MarkdownNoteTarget::Composition
            ? QStringLiteral("Write composition notes in markdown...")
            : QStringLiteral("Write layer notes in markdown..."));
    editor_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    editor_->setTabChangesFocus(false);
    editor_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    layout->addWidget(titleLabel_);
    layout->addWidget(subtitleLabel_);
    layout->addWidget(editor_, 1);

    const auto theme = ArtifactCore::currentDCCTheme();
    owner_->setAutoFillBackground(true);
    QPalette pal = owner_->palette();
    pal.setColor(QPalette::Window, QColor(theme.backgroundColor));
    pal.setColor(QPalette::WindowText, QColor(theme.textColor));
    owner_->setPalette(pal);
    editor_->setPalette(pal);

    QFont titleFont = titleLabel_->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() > 0 ? titleFont.pointSize() + 1 : 11);
    titleLabel_->setFont(titleFont);

    QPalette subtitlePal = subtitleLabel_->palette();
    subtitlePal.setColor(QPalette::WindowText, QColor(theme.textColor).darker(135));
    subtitleLabel_->setPalette(subtitlePal);

    editor_->setAutoFillBackground(true);
    QPalette editorPal = editor_->palette();
    editorPal.setColor(QPalette::Base, QColor(theme.secondaryBackgroundColor));
    editorPal.setColor(QPalette::Text, QColor(theme.textColor));
    editorPal.setColor(QPalette::PlaceholderText, QColor(theme.textColor).darker(145));
    editor_->setPalette(editorPal);

    owner_->setAutoFillBackground(true);
    owner_->setPalette(pal);

    QObject::connect(editor_, &QPlainTextEdit::textChanged, owner_, [this]() {
      if (updating_) {
        return;
      }
      commitText();
    });

    eventBusSubscriptions_.push_back(eventBus_.subscribe<ProjectChangedEvent>(
        [this](const ProjectChangedEvent&) {
          refreshBinding();
        }));
    eventBusSubscriptions_.push_back(eventBus_.subscribe<CurrentCompositionChangedEvent>(
        [this](const CurrentCompositionChangedEvent&) {
          refreshBinding();
        }));
    eventBusSubscriptions_.push_back(eventBus_.subscribe<LayerSelectionChangedEvent>(
        [this](const LayerSelectionChangedEvent&) {
          if (target_ == MarkdownNoteTarget::Layer) {
            refreshBinding();
          }
        }));
    eventBusSubscriptions_.push_back(eventBus_.subscribe<LayerChangedEvent>(
        [this](const LayerChangedEvent& event) {
          if (target_ == MarkdownNoteTarget::Layer && currentLayerId_.toString() == event.layerId) {
            refreshBinding();
          }
        }));

    refreshBinding();
  }

  void disconnectNoteConnection()
  {
    if (noteConnection_) {
      QObject::disconnect(noteConnection_);
      noteConnection_ = {};
    }
  }

  void loadText(const QString& text)
  {
    if (!editor_) {
      return;
    }
    updating_ = true;
    QSignalBlocker blocker(editor_);
    editor_->setPlainText(text);
    editor_->setEnabled(true);
    updating_ = false;
  }

  QString activePlaceholder() const
  {
    return target_ == MarkdownNoteTarget::Composition
        ? QStringLiteral("Write composition notes in markdown...")
        : QStringLiteral("Write layer notes in markdown...");
  }

  void setDisabledState(const QString& message)
  {
    disconnectNoteConnection();
    if (!editor_) {
      return;
    }
    updating_ = true;
    QSignalBlocker blocker(editor_);
    editor_->clear();
    editor_->setEnabled(false);
    editor_->setPlaceholderText(message);
    updating_ = false;
  }

  void refreshBinding()
  {
    auto* service = ArtifactProjectService::instance();
    if (!service) {
      setDisabledState(QStringLiteral("No project loaded."));
      return;
    }

    const auto comp = service->currentComposition().lock();
    if (!comp) {
      currentCompositionId_ = CompositionID();
      currentLayerId_ = LayerID();
      setDisabledState(QStringLiteral("No composition selected."));
      return;
    }

    currentCompositionId_ = comp->id();

    if (target_ == MarkdownNoteTarget::Composition) {
      disconnectNoteConnection();
      noteConnection_ = QObject::connect(comp.get(), &ArtifactAbstractComposition::compositionNoteChanged,
                                         owner_, [this](const QString& note) {
        loadText(note);
      });
      if (editor_) {
        editor_->setPlaceholderText(activePlaceholder());
      }
      loadText(comp->compositionNote());
      return;
    }

    auto* app = ArtifactApplicationManager::instance();
    auto* selection = app ? app->layerSelectionManager() : nullptr;
    const auto layer = selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    if (!layer || !comp->containsLayerById(layer->id())) {
      currentLayerId_ = LayerID();
      setDisabledState(QStringLiteral("No layer selected."));
      return;
    }

    currentLayerId_ = layer->id();
    disconnectNoteConnection();
    noteConnection_ = QObject::connect(layer.get(), &ArtifactAbstractLayer::layerNoteChanged,
                                       owner_, [this](const QString& note) {
      loadText(note);
    });
    if (editor_) {
      editor_->setPlaceholderText(activePlaceholder());
    }
    loadText(layer->layerNote());
  }

  void commitText()
  {
    auto* service = ArtifactProjectService::instance();
    if (!service || !editor_) {
      return;
    }

    const QString text = editor_->toPlainText();
    if (target_ == MarkdownNoteTarget::Composition) {
      if (auto comp = service->currentComposition().lock()) {
        if (comp->compositionNote() != text) {
          comp->setCompositionNote(text);
        }
      }
      return;
    }

    auto* app = ArtifactApplicationManager::instance();
    auto* selection = app ? app->layerSelectionManager() : nullptr;
    const auto layer = selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    if (layer && layer->layerNote() != text) {
      layer->setLayerNote(text);
    }
  }
};

W_OBJECT_IMPL(ArtifactMarkdownNoteEditorWidget)

ArtifactMarkdownNoteEditorWidget::ArtifactMarkdownNoteEditorWidget(MarkdownNoteTarget target, QWidget* parent)
    : QWidget(parent), impl_(new Impl(this, target))
{
  impl_->setupUi();
}

ArtifactMarkdownNoteEditorWidget::~ArtifactMarkdownNoteEditorWidget()
{
  delete impl_;
}

MarkdownNoteTarget ArtifactMarkdownNoteEditorWidget::target() const
{
  return impl_ ? impl_->target_ : MarkdownNoteTarget::Composition;
}

QString ArtifactMarkdownNoteEditorWidget::markdown() const
{
  return impl_ && impl_->editor_ ? impl_->editor_->toPlainText() : QString();
}

void ArtifactMarkdownNoteEditorWidget::setMarkdown(const QString& markdown)
{
  if (!impl_ || !impl_->editor_) {
    return;
  }
  impl_->loadText(markdown);
}

void ArtifactMarkdownNoteEditorWidget::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  if (impl_) {
    impl_->refreshBinding();
  }
}

}

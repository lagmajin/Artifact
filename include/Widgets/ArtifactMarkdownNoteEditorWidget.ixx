#include <wobjectdefs.h>
#include <QWidget>
#include <QPlainTextEdit>
#include <QString>
#include <QShowEvent>
export module Artifact.Widgets.MarkdownNoteEditorWidget;


export namespace Artifact {

enum class MarkdownNoteTarget {
  Composition,
  Layer
};

class ArtifactMarkdownNoteEditorWidget : public QWidget {
  W_OBJECT(ArtifactMarkdownNoteEditorWidget)
private:
  class Impl;
  Impl* impl_ = nullptr;

public:
  explicit ArtifactMarkdownNoteEditorWidget(MarkdownNoteTarget target, QWidget* parent = nullptr);
  ~ArtifactMarkdownNoteEditorWidget() override;

public:
  MarkdownNoteTarget target() const;
  QString markdown() const;
  void setMarkdown(const QString& markdown);

protected:
  void showEvent(QShowEvent* event) override;
};

}

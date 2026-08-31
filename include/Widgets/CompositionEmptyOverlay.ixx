module;

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QResizeEvent>
#include <QSize>
#include <QStringList>
#include <QWidget>

#include <functional>

class QFrame;
class QLabel;
class QPushButton;
class QVBoxLayout;

export module Artifact.Widgets.CompositionEmptyOverlay;

export namespace Artifact {

class EmptyCompositionOverlayWidget final : public QWidget {
public:
  explicit EmptyCompositionOverlayWidget(
      QWidget *parent, std::function<void()> createRequested,
      std::function<void(const QStringList &)> filesDropped);

  void setCompositionAvailable(bool hasComposition);
  QSize preferredOverlaySize(const QSize &available) const;

protected:
  void resizeEvent(QResizeEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  void updateResponsiveLayout();

  std::function<void()> createRequested_;
  std::function<void(const QStringList &)> filesDropped_;
  QVBoxLayout *rootLayout_ = nullptr;
  QFrame *card_ = nullptr;
  QVBoxLayout *cardLayout_ = nullptr;
  QLabel *titleLabel_ = nullptr;
  QLabel *bodyLabel_ = nullptr;
  QLabel *helperLabel_ = nullptr;
  QPushButton *createButton_ = nullptr;
  bool hasComposition_ = false;
  bool compositionStateInitialized_ = false;
};

}

module;
#include <utility>
#include <vector>
#include <memory>
#include <functional>

#include <wobjectdefs.h>
#include <QWidget>
#include <QIcon>
#include <QString>

export module Artifact.Widgets.PieMenu;

export namespace Artifact {

struct PieMenuItem {
    QString label;
    QIcon icon;
    QString commandId;
    bool enabled = true;
    bool checked = false;
    std::function<void()> action;
};

struct PieMenuModel {
    std::vector<PieMenuItem> items;
    QString title;
};

class ArtifactPieMenuWidget : public QWidget {
    W_OBJECT(ArtifactPieMenuWidget)
public:
    explicit ArtifactPieMenuWidget(QWidget* parent = nullptr);
    virtual ~ArtifactPieMenuWidget();

    void setModel(const PieMenuModel& model);
    void showAt(const QPoint& pos);
    void updateMousePos(const QPoint& pos);
    
    // Returns the commandId of the selected item, or empty string if nothing selected
    QString confirmSelection();
    void cancel();

protected:
    void paintEvent(QPaintEvent* event) override;
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Artifact

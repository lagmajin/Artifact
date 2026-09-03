module;

#include <QWidget>

#include <atomic>
#include <functional>

export module Artifact.Widgets.LayerEditor.RenderScheduler;

export namespace Artifact {

class LayerEditorRenderScheduler {
public:
 void request(QWidget* receiver, std::function<void()> execute);
 void cancel() noexcept;

private:
 void schedule(QWidget* receiver);

 std::atomic_bool pending_{false};
 bool scheduled_ = false;
 std::function<void()> execute_;
};

}

module;

#include <QTimer>

#include <utility>

module Artifact.Widgets.LayerEditor.RenderScheduler;

namespace Artifact {

namespace {
constexpr int kInteractiveRenderCoalesceMs = 16;
}

void LayerEditorRenderScheduler::request(
    QWidget* receiver,
    std::function<void()> execute)
{
 if (!receiver) return;
 pending_.store(true, std::memory_order_release);
 execute_ = std::move(execute);
 schedule(receiver);
}

void LayerEditorRenderScheduler::cancel() noexcept
{
 pending_.store(false, std::memory_order_release);
 scheduled_ = false;
 execute_ = nullptr;
}

void LayerEditorRenderScheduler::schedule(QWidget* receiver)
{
 if (scheduled_ || !receiver) return;
 scheduled_ = true;
 QTimer::singleShot(kInteractiveRenderCoalesceMs, receiver, [this, receiver]() {
  scheduled_ = false;
  if (!pending_.exchange(false, std::memory_order_acq_rel)) return;
  if (execute_) execute_();
  if (pending_.load(std::memory_order_acquire)) schedule(receiver);
 });
}

}

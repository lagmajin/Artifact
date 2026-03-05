
import re

path = r'c:\Users\lagma\Desktop\Artifact\Artifact\src\Widgets\ArtifactTimelineWidget.cpp'
with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

# Add clearTracks to TimelineScene
scene_clear = """
 void TimelineScene::clearTracks()
 {
  for (auto* clip : impl_->clips_) {
   removeItem(clip);
   destroyClipItem(clip);
  }
  impl_->clips_.clear();
  impl_->selectedClips_.clear();
  impl_->trackHeights_.clear();
  setSceneRect(0, 0, 2000, 0);
 }
"""
content = re.sub(r'(void\s+TimelineScene::removeTrack\(int\s+trackIndex\)\s*\{.*?\}\s*)', r'\1' + scene_clear, content, flags=re.DOTALL)

# Add clearTracks to TimelineTrackView
view_clear = """
  void TimelineTrackView::clearTracks()
  {
   if (impl_->scene_) {
    impl_->scene_->clearTracks();
    viewport()->update();
   }
  }
"""
content = re.sub(r'(void\s+TimelineTrackView::removeTrack\(int\s+trackIndex\)\s*\{.*?\}\s*)', r'\1' + view_clear, content, flags=re.DOTALL)

# Update setComposition
new_set_comp = """
  void ArtifactTimelineWidget::setComposition(const CompositionID& id)
  {
   impl_->compositionId_ = id;
   if (impl_->layerTimelinePanel_) {
    impl_->layerTimelinePanel_->setComposition(id);
   }

   if (impl_->trackView_) {
    impl_->trackView_->clearTracks();
    if (auto svc = ArtifactProjectService::instance()) {
     auto res = svc->findComposition(id);
     if (res.success && !res.ptr.expired()) {
      auto comp = res.ptr.lock();
      auto layers = comp->allLayer();
      for (const auto& l : layers) {
       if (l) onLayerCreated(l->id());
      }
     }
    }
   }
  }
"""
content = re.sub(r'void\s+ArtifactTimelineWidget::setComposition\(const\s+CompositionID&\s+id\)\s*\{.*?\}', new_set_comp, content, flags=re.DOTALL)

with open(path, 'w', encoding='utf-8') as f:
    f.write(content)
print("Updated successfully")

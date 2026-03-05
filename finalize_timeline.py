
import re

# Update ArtifactTimelineWidget.ixx
ixx_path = r'c:\Users\lagma\Desktop\Artifact\Artifact\include\Widgets\ArtifactTimelineWidget.ixx'
with open(ixx_path, 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Add clearTracks to TimelineScene
content = re.sub(r'void\s+removeTrack\(int\s+trackIndex\);', 
                 'void removeTrack(int trackIndex);\n  void clearTracks();', content)

# 2. Add clearTracks to TimelineTrackView
content = re.sub(r'int\s+addTrack\(double\s+height\s+=\s+20\.0\);', 
                 'int addTrack(double height = 20.0);\n  void clearTracks();', content)

# 3. Update ArtifactTimelineWidget's onLayer* signatures
content = re.sub(r'void\s+onLayerCreated\(const\s+LayerID&\s+id\);',
                 'void onLayerCreated(const CompositionID& compId, const LayerID& layerId);', content)
content = re.sub(r'void\s+onLayerRemoved\(const\s+LayerID&\s+id\);',
                 'void onLayerRemoved(const CompositionID& compId, const LayerID& layerId);', content)

with open(ixx_path, 'w', encoding='utf-8') as f:
    f.write(content)
print("Updated ArtifactTimelineWidget.ixx")

# Update ArtifactTimelineWidget.cpp
cpp_path = r'c:\Users\lagma\Desktop\Artifact\Artifact\src\Widgets\ArtifactTimelineWidget.cpp'
with open(cpp_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Update onLayerCreated implementation
new_on_layer_created = """
  void ArtifactTimelineWidget::onLayerCreated(const CompositionID& compId, const LayerID& lid)
  {
   if (compId != impl_->compositionId_) return;
   if (!impl_->trackView_) return;

   qDebug() << "[ArtifactTimelineWidget::onLayerCreated] Layer created:" << lid.toString();

   int trackIndex = impl_->trackView_->addTrack(28.0);
   double startFrame = 0.0;
   double duration = 100.0;
   ClipItem* clip = impl_->trackView_->addClip(trackIndex, startFrame, duration);
  }
"""
content = re.sub(r'void\s+ArtifactTimelineWidget::onLayerCreated\(const\s+LayerID&\s+id\)\s*\{.*?\}', 
                  new_on_layer_created, content, flags=re.DOTALL)

# Update onLayerRemoved implementation
new_on_layer_removed = """
  void ArtifactTimelineWidget::onLayerRemoved(const CompositionID& compId, const LayerID& lid)
  {
   if (compId != impl_->compositionId_) return;
   qDebug() << "[ArtifactTimelineWidget::onLayerRemoved] Layer removed:" << lid.toString();
  }
"""
content = re.sub(r'void\s+ArtifactTimelineWidget::onLayerRemoved\(const\s+LayerID&\s+id\)\s*\{.*?\}', 
                  new_on_layer_removed, content, flags=re.DOTALL)

# Update setComposition to pass both IDs
content = re.sub(r'onLayerCreated\(l->id\(\)\)', 'onLayerCreated(id, l->id())', content)

with open(cpp_path, 'w', encoding='utf-8') as f:
    f.write(content)
print("Updated ArtifactTimelineWidget.cpp")

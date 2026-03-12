module;
export module Tool;

export namespace Artifact {
 enum class EditMode
 {
  View,           // View-only mode (zoom and pan)
  Transform,      // Transform editing
  Mask,           // Mask editing
  Paint           // Paint mode
 };

 enum class DisplayMode
 {
  Color,          // Standard color view
  Alpha,          // Alpha channel view
  Mask,           // Mask overlay view
  Wireframe       // Guide and outline view
 };

}

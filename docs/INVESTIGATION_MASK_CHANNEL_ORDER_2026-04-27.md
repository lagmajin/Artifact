# Mask Rendering: Channel Order Investigation (2026-04-27)

## Investigation Summary

Analyzed mask rendering channel order issues related to RGB/BGR conventions across different backends.

**Status**: 🔍 INVESTIGATION (findings documented, fix deferred)

## Issue Background

When layer masks are applied, color rendering becomes fixed (entire image becomes single color). This suggests channel order mismatch or incorrect alpha channel handling.

## Relevant Code Locations

### 1. Mask Apply Implementation (`Artifact/src/Mask/LayerMask.cppm:153-173`)

**Current Implementation:**
```cpp
void LayerMask::applyToImage(int width, int height, void* imageMat,
                             float offsetX, float offsetY) const
{
    cv::Mat& img = *static_cast<cv::Mat*>(imageMat);
    if (img.empty() || img.type() != CV_32FC4) return;
    if (!impl_->enabled || impl_->paths.empty()) return;

    cv::Mat alphaMask;
    compositeAlphaMask(width, height, &alphaMask, offsetX, offsetY);

    // For CV_32FC4, OpenCV stores as BGRA in memory
    // Split preserves this order: [B, G, R, A]
    std::vector<cv::Mat> channels(4);
    cv::split(img, channels);
    
    if (channels.size() >= 4) {
        // channels[3] is the alpha channel
        cv::multiply(channels[3], alphaMask, channels[3]);
        cv::merge(channels, img);
    }
}
```

**Issue Identified**: The comment states "OpenCV stores as BGRA" but CV_32FC4 channel order depends on how the image was created and populated.

### 2. RGB/BGR Conventions Across Backends

| Backend | Channel Order | Notes |
|---------|---------------|-------|
| QImage  | RGBA (native Qt format) | 32-bit ARGB or RGBA depending on platform |
| OpenCV  | BGR (default for 8-bit) | CV_8UC3/CV_8UC4 are BGR(A); CV_32FC4 is generic |
| DiligentEngine | RGBA (D3D convention) | Direct3D uses RGBA for float formats |

### 3. Potential Channel Order Mismatches

**Scenario 1: QImage → OpenCV conversion**
- QImage provides RGBA
- OpenCV expects BGR (if 8-bit) or generic 4-channel (if 32-bit float)
- If not converted, colors will be swapped (R↔B)

**Scenario 2: Mask source**
- If mask is generated in OpenCV (BGR order)
- But applied to image in QImage order (RGBA)
- Alpha channel may be applied to wrong semantic channel

**Scenario 3: Composition rendering pipeline**
- If intermediate buffers use different channel orders
- And no explicit conversion between backends
- Colors will mismatch or become fixed

## Findings

### What We Know

1. **cv::split() behavior**:
   - For CV_32FC4 (float 4-channel), order depends on image creation
   - NOT guaranteed to follow BGR convention (that's only for CV_8UC3/CV_8UC4)
   - Comment in code may be misleading

2. **Mask compositing** (`compositeAlphaMask` function):
   - Generates CV_32FC1 (single float channel) alpha mask
   - Uses OpenCV drawing operations (MaskPath rasterization)
   - Should be channel-order agnostic (single channel)

3. **Image type assumption**:
   - Code checks `img.type() != CV_32FC4`
   - Assumes input is float 4-channel (RGBA or BGRA)
   - No explicit handling of 8-bit formats

### Likely Root Cause

When image is created/populated from QImage:
- QImage provides RGBA data
- But code comment assumes BGRA order from OpenCV convention
- cv::split() on CV_32FC4 will respect actual data layout (RGBA)
- So channels[0] = R, channels[1] = G, channels[2] = B, channels[3] = A
- BUT code treats channels[3] as alpha, which is correct

**Wait**: If channels[3] really is alpha (which it is), the logic should work...

### Alternative Theory: Color Fixation Root Cause

The "fixed color" symptom suggests:
1. **All pixels get same color** → Mask value applied uniformly?
2. **Color becomes 0 or 1** → Alpha multiply with wrong range?
3. **Specific color channel fixed** → Wrong channel being modified?

Possible causes:
- **Mask range issue**: If alphaMask is not in [0, 1] range, multiply gives wrong results
- **Premultiplication**: If image is premultiplied alpha but code treats it as straight
- **Data layout**: CV_32FC4 from QImage may have different layout than expected

## Channel Order Audit Table

```
Format         | Byte Order      | Split Order     | Channel[3] Notes
---------------|-----------------|-----------------|------------------
CV_8UC4 (BGR)  | B G R A         | [B,G,R,A]       | A at index 3 ✓
CV_8UC4 (RGBA) | R G B A         | [R,G,B,A]       | A at index 3 ✓
CV_32FC4 (RGB) | R G B A (float) | [R,G,B,A]       | A at index 3 ✓
CV_32FC4 (BGR) | B G R A (float) | [B,G,R,A]       | A at index 3 ✓
```

## Recommendation for Fix

1. **Add explicit channel verification**:
   ```cpp
   // After cv::split():
   qDebug() << "Channel count:" << channels.size();
   qDebug() << "Channel[3] mean:" << cv::mean(channels[3])[0];
   ```

2. **Verify mask range**:
   ```cpp
   cv::Mat maskMin, maskMax;
   cv::minMaxLoc(alphaMask, nullptr, nullptr, &maskMin, &maskMax);
   if (maskMax < 0.01f || maskMin > 0.99f) {
       qWarning() << "[Mask] Suspicious mask range:" << maskMin << maskMax;
   }
   ```

3. **Consider premultiplication**:
   - Check if RGB should also be multiplied by mask
   - Currently only alpha is multiplied

4. **Document explicitly**:
   ```cpp
   // Clarify: img is expected to be CV_32FC4 in RGBA memory layout
   // channels[0]=R, channels[1]=G, channels[2]=B, channels[3]=A
   cv::split(img, channels);
   ```

## Documentation to Create

1. **RGB/BGR Convention Guide** for codebase
   - Where each backend uses which convention
   - Conversion points between backends
   - Testing methodology

2. **Channel Order Audit** document
   - All image creation and conversion paths
   - Verification at conversion boundaries

## Next Steps

1. Add debug logging to identify actual channel layout
2. Verify mask alpha range is [0, 1]
3. Check if RGB channels should also be masked (alpha blending)
4. Create comprehensive RGB/BGR documentation

---

**Investigation Date**: 2026-04-27  
**Investigator**: Copilot  
**Findings**: Channel order handling appears correct but mask value range should be verified. Root cause of "fixed color" symptom may be in mask generation or alpha blending logic rather than channel order.

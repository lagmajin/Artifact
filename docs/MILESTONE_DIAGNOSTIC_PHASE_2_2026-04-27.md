# Diagnostic Phase 2: Frame Timing Logger (2026-04-27)

## Summary

Diagnostic Phase 2 implements frame-level render timing statistics with debug output. The goal is to provide real-time visibility into composition rendering performance.

**Status**: ✅ COMPLETED (audit + existing implementation verified)

## Implementation Details

### Frame Timing Measurement

Located in `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, line 3950-5519:

- **QElapsedTimer Integration** (lines 4005-4006):
  ```cpp
  QElapsedTimer frameTimer;
  frameTimer.start();
  ```

- **Phase Timing Lambda** (lines 4008-4013):
  ```cpp
  auto markPhaseMs = [&frameTimer, &phaseNs]() -> qint64 {
    const qint64 nowNs = frameTimer.nsecsElapsed();
    const qint64 phaseMs = (nowNs - phaseNs) / 1000000;
    phaseNs = nowNs;
    return phaseMs;
  };
  ```

### Performance Metrics Captured

The render pipeline measures and logs these phases:

1. **setupMs** - Viewport/canvas configuration and composition state verification
2. **basePassMs** - Background rendering (solid, Maya gradient, or checkerboard)
3. **layerPassMs** - Layer composition and blending
4. **overlayMs** - UI overlays (grid, guides, gizmo, selection)
5. **flushMs** - GPU buffer flush operations
6. **presentMs** - Swapchain present call
7. **lastSubmit2DMs** - 2D submit batch time

### Debug Output

Logs are controlled by category `compositionViewLog()` and output conditionally:

- **Every frame** (lines 5489-5504):
  - Full detailed output when viewport is interacting or detail-level changes
  - Includes: frameMs, pipelineEnabled, layersTotal, drawnLayerCount
  - All phase timings listed above

- **Every 120 frames** (lines 5505-5514):
  - Summary statistics for non-interactive sessions
  - Same metrics as above, reduced frequency

### Profiler Integration

Frame statistics are also integrated with `ArtifactCore::Profiler`:

- `Profiler::instance().beginFrame()` (lines 4065-4068) - Frame start with composition size and playback state
- `Profiler::instance().endFrame()` (line 5518) - Frame completion after all timing phases recorded
- Profile scopes for major passes (`_profSetup`, `_profLayer`, etc.)

### How to Use

**Enable Debug Output:**
```bash
# Set QT_LOGGING_RULES environment variable to show compositionViewLog output
export QT_LOGGING_RULES="artifact.*=true"

# Or in application:
QLoggingCategory::setFilterRules(QStringLiteral("artifact.*=true"));
```

**Sample Output (every 120 frames):**
```
[CompositionView][Perf] frameMs=16.5 pipelineEnabled=true layersTotal=12 layersDrawn=8 
  setupMs=2.1 basePassMs=1.2 layerPassMs=9.8 overlayMs=1.1 flushMs=0.8 submit2DMs=0.3 presentMs=1.2
```

**Trace Recording:**
Frame scopes are automatically recorded to `ArtifactCore::TraceRecorder` for analysis with external profiling tools.

## Performance Baseline

No breaking changes introduced. Frame timing measurement uses:
- Stack-allocated `QElapsedTimer` (zero-cost if not logged)
- Conditional `qCDebug` with category filtering (no I/O unless enabled)
- Existing profiler infrastructure (already active)

## Related Documentation

- **Profiler API**: `ArtifactCore/include/Utils/PerformanceProfiler.ixx` - Frame/scope statistics
- **Trace Recording**: `ArtifactCore/include/Diagnostics/Trace.ixx` - Detailed event capture
- **Composition Rendering**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` - Implementation

## Next Steps (Deferred)

Future enhancements for Diagnostic Phase 3+:

1. **GPU Stat Capture** - Query GPU timing via Diligent Engine
2. **Render Graph Overlay** - Visual profiling UI in composition viewport
3. **Per-Layer Timing** - Individual layer render cost breakdown
4. **Memory Profiling** - GPU/CPU buffer utilization tracking

---

**Audit Date**: 2026-04-27  
**Auditor**: Copilot  
**Verdict**: Implementation already complete and working; added documentation for clarity.

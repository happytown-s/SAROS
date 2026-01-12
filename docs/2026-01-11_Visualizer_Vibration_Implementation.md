# Track Vibration Effect Implementation (2026-01-11)

## Goal Description
Implement a "Track Vibration Effect" (RMS Rubber) where each track's waveform ring vibrates/expands independently based on its own volume (RMS). This replaces the current generic vibration (based on master mix) with per-track reactive internal physics, creating a more organic and dynamic "Gummy" feel.

## Implementation Details

### 1. Per-Track RMS Calculation (`LooperAudio.cpp`)
- Implemented `track.currentEffectRMS` update logic within `mixTracksToOutput`.
- Stores the post-FX RMS value into an atomic variable for thread-safe UI access.
- Uses `track.currentLevel` which includes decay smoothing suitable for visual feedback.

### 2. Physics-Based Vibration (`CircularVisualizer.h`)
- **Physics State**: Added properties `targetRms`, `currentRms`, `vibrationVelocity` to the `WaveformPath` struct.
- **Physics Engine**: Implemented a spring-mass-damper system (`force = (target - current) * stiffness`) in `timerCallback`.
    - Stiffness: 0.25 (Controls bounce speed)
    - Damping: 0.85 (Controls settling time)
- **Rendering**: Modified `paint` to apply radial distortion based on `currentRms` for each track's specific ring.
- **Bug Fix**: Restored `segmentAngles` member which was accidentally removed during refactoring.

### 3. Data Integration (`MainComponent.cpp`)
- Identified that `FXPanel` uses a separate `FilterSpectrumVisualizer`.
- Implemented the RMS data bridge in `MainComponent::timerCallback` (where `CircularVisualizer` resides).
- Iterates through all tracks and pushes the atomic RMS value to the visualizer: `visualizer.updateTrackRMS(id, level)`.

## Verification Results

### Build Status
- **Success**: Project compiles with `cmake --build build -j 4`.

### Manual Verification Required
- **Single Track**: Play a track with strong transients (kick/snare). Verify its specific ring bounces in sync with the sound.
- **Multi Track**: Play multiple tracks. Verify each ring moves independently according to its own rhythm, creating a complex, organic visual interaction.

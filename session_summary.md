# Session Summary: CPU Optimization, Timbre Refinement, ALTER & CLOUD Reverb, and Hard-Sync

This document provides a comprehensive summary of all progress, DSP changes, and synchronization fixes made up to the Hard-Sync & Parameter Renaming update.

---

## 1. Work Accomplished & Historical Context

### DSP CPU Optimization
* **Sine Table Lookup**: Replaced expensive `std::sin()` calls in the inner sample loop with a static, pre-allocated 32,768-sample lookup table (`sineTable` in C++ and `SINE_TABLE` in JS) using fast bitwise wrapping.
* **Block-Rate Precomputations**: Replaced sample-rate phase delta, panning, and ADSR parameter evaluations with block-rate precomputations outside the sample loop.
* **Active Partials Indexing**: Maintained an active indexing array (`activePartials`) containing only indices of active or decaying partials, avoiding iteration over silent harmonics.

### TIMBRE Morph Refinements
* **Smooth Morphing & Sheen Floor**: Blended a dynamic, slow-decaying high-frequency sheen baseline (`0.05f / std::sqrt(harmonicIndex)`) to ensure high partials are always present without masking the character of individual morph settings.
* **Dropout Prevention**: Updated raw shapes (like hollow square and octave double) to have minimum amplitude limits (e.g. `0.08f / harmonicIndex` instead of `0.0f`), preventing silence dropouts during TIMBRE knob sweeps.

### ALTER Phase Modulation Distortion
* **Pitch-Independent PM Drive**: Normalized the frequency distance of adjacent partials by the note fundamental:
  `float normDistance = distance / currentFundamentalFreq;`
  This makes the distortion character identical whether playing low or high pitches.
* **Enhanced Drive scaling**: Increased modulation drive scaling to ensure that phase-modulation distortion is highly audible up to its clamped limit of `2.0f`.

### Per-Partial Chorused Send Reverb
* **Per-Partial Delay Buffers**: Run a separate circular delay buffer `delayBuffers[256][4096]` (up to $93\text{ ms}$) for every single partial to model physical reflections. Circular indexing uses fast bitwise masking (`& 4095`) to keep CPU usage low.
* **LFO Chorus Modulation**: Slowly modulates individual delay sizes to create a warm, thick ensemble sound in the reverb tail.
* **RT60 Decay Time Control (SIZE / PARAM6)**: Maps PARAM6 (`"size"`) to a uniform RT60 decay time ($0.1\text{s}$ to $6.0\text{s}$). Reverb feedback is scaled precisely per-partial based on its delay length so that all partials decay synchronously.
* **Swept Gaussian Excitation (SWEEP / PARAM7)**: Maps PARAM7 (`"sweep"`) to slide a bandpass window across the harmonics, routing only the active zone into the delay lines.
* **Global Send Amount (CLOUD / PARAM8)**: Maps PARAM8 (`"cloud"`) to control the excitation input level fed into the delay lines.

### Global FDN Reverb Integration
* **8-Channel Feedback Delay Network**: Replaced the per-partial delay lines with a global 8-channel FDN for lush stereo diffusion with prime length delays `[997, 1201, 1439, 1753, 2053, 2411, 2851, 3307]` and a unitary Householder mixing matrix.
* **Partial Routing**: Active partials write into FDN send channels based on harmonic index `route = 7 - (p / 32)`.

### Parameter Renaming (FORM & PITCH)
* **Parameter Renaming**: Renamed slot 01 vertical slider from `DETUNE` to `FORM` (`"form"`) and slot 07 vertical slider from `PARAM7` to `PITCH` (`"pitch"`).
* **Continuous Transposition**: Mapped `PITCH` to continuous frequency transposition (-12 to +12 semitones, non-quantized) in both C++ and JS.

### Double-click resets
* **Control Resetting**: Double-clicking on any slider track/thumb or knob dial in the HTML UI resets its value to its default value, updating text readouts, visual positions, and audio parameters.

### Concept 1: Additive Hard-Sync (DE-SYNC)
* **True Hard-Sync**: Repurposed vertical slider 06 (`PARAM6` placeholder) as `DE-SYNC` (`"desync"`, default `0.0`). The master oscillator is the fundamental partial ($p = 0$). Higher partials ($p > 0$) run at a swept frequency (up to $2\times$ fundamental). Whenever the fundamental completes a cycle, all higher partial phases are reset to `0.0`.
* **Crystalline Visualizer**: Morph orbits from circles to sharp polygons (triangles, squares, pentagons) based on `DE-SYNC`. Draws an expanding white shockwave ring to represent phase resets.
* **DE-SYNC Rescaling**: Rescaled maximum pitch sweep range to $+12$ semitones (1 octave) at `1.00` to keep the sync sweep in a highly warm, musical range.

### Visualizer Note Sync Fix
* **Active Notes Cache**: Relocated the editor active notes cache to the WebView, resetting it during `"queryall"` initialization. This guarantees that opening the editor while a note plays displays visualizer movements immediately.

---

## 2. Serialized Code Backups
All backups are saved as zipped archives inside the workspace under the `.\backup\` folder:
* **[source_backup.zip](file:///c:/Dropbox/DSP/JUCE_projects/Tesuto/backup/source_backup.zip)**: Original optimized codebase (Build #41).
* **[source_backup_42.zip](file:///c:/Dropbox/DSP/JUCE_projects/Tesuto/backup/source_backup_42.zip)**: Pre-modification state for Build #42 (C++ & JS).
* **[source_backup_43.zip](file:///c:/Dropbox/DSP/JUCE_projects/Tesuto/backup/source_backup_43.zip)**: Pre-modification state for Build #44 (C++ & JS).
* **[source_backup_44.zip](file:///c:/Dropbox/DSP/JUCE_projects/Tesuto/backup/source_backup_44.zip)**: Pre-modification state for Build #45 (C++ & JS).

---

## 3. GitHub Build Reference
* **Build #41**: Successful cloud build containing original morph baseline and table lookup optimizations.
* **Build #42**: Successful cloud build containing TIMBRE morph sheen floor, pitch-independent ALTER PM, and JS worklet sync.
* **Build #43**: Successful cloud build containing conditional CLOUD decay smoothing.
* **Build #44**: Successful cloud build containing block-rate optimized Concept 1: Spectral Rain reverb.
* **Build #45**: Successful cloud build containing per-partial chorused send reverb with SIZE and SWEEP.
* **Build #47**: Build containing silent output issue due to missing targetAmp assignment in noteStarted.
* **Build #48**: Successful cloud build containing targetAmp fix to restore audio output.
* **Build #50**: Successful cloud build containing 8-channel global FDN send reverb with Householder mixing and per-partial routing.
* **Build #51**: Successful cloud build containing rescaled CLOUD range (max send gain 0.2) and boosted synth output volume scaleFactor.
* **Build #52**: Successful cloud build containing Web visualizer extensions: ALTER geometric web (Concept 3) and CLOUD concentric ripples (Concept 2) with SWEEP hue color tracking.
* **Build #53**: Cloud build containing visualizer extensions: ALTER geometric web (Concept 3) and CLOUD background smokey pulsation (Concept 2 - Revised) with SWEEP hue color tracking.
* **Build #54 (Renaming update)**: Successful push containing parameter renames (FORM and PITCH) and visualizer notes sync fix.
* **Build #55 (Hard-sync & Resets)**: Successful push containing double-click resets, Concept 1: Direct Phase-Reset hard-sync, and crystalline polygonal visualizer morphing.

---

## 4. Current Status & Next Steps
* **Status**: Complete hard-sync, parameter renames, note sync fix, and double-click resetting pushed to GitHub.
* **Next Steps**: Await cloud build completion, download the updated installer, and manually verify:
  1. Double-clicking any slider/knob resets it.
  2. Opening the editor while a note plays displays visualizer movements immediately.
  3. `DE-SYNC` creates an aggressive analog saw-sync sweep.
  4. At maximum `DE-SYNC`, visualizer orbits morph into crystalline polygonal shapes, and a shockwave ring continuously expands.

---

## 5. Detailed Implementation Plans History

### Implementation Plan #50 - 8-Channel Global FDN Reverb
* **Goal**: Replaced per-partial delay lines with a global 8-channel Feedback Delay Network (FDN) to generate lush, professional stereo reverberation with proper diffusion.
* **C++ DSP Changes**:
  * Removed 256 circular buffers from `KronosVoice` to save memory.
  * Added `globalSendAccum` write pointers inside voices.
  * Inside `renderNextBlock`, active sines accumulate their wet signal directly to the global FDN send channels based on harmonic index `route = 7 - (p / 32)`.
  * Added an 8-channel global FDN reverb in `processBlock` with prime-length delay lines `[997, 1201, 1439, 1753, 2053, 2411, 2851, 3307]`.
  * Multiplied outputs by a Householder mixing matrix at every sample to achieve dense, unitary spatial diffusion.
  * Decayed FDN lines matching the `SIZE` decay parameter ($0.1\text{s}$ to $6.0\text{s}$).
* **JS Worklet Changes**:
  * Mirrored the same 8-channel FDN circular buffers and Householder feedback processing inside the browser AudioWorklet script (`app.js`).

### Implementation Plan #53 - Visualizer Extensions Release
* **Goal**: Implemented rich visualizer feedback for `ALTER` and `CLOUD` controls to match the audio logic.
* **JS/HTML Changes**:
  * Created a custom visual reverb envelope `visualReverbEnv` tracking the active key state but decaying slowly according to `size` values to match the exact duration of the audio tail decay.
  * **ALTER (Concept 3)**: When `ALTER` is active, draws thin glowing connections between distant harmonics (e.g. connecting node `i` to `(i + 47) % length`). Web opacity scales dynamically with the `alter` parameter.
  * **CLOUD (Concept 2 - Revised)**: When `CLOUD` is active, renders a large, blurry, pulsating radial gradient background glow. Hues shift with `SWEEP` and size/intensity/_decay settings tracking decay values.

### Implementation Plan #55 - Parameter Renames, Double-click resets, and Hard-Sync
* **Goal**: Implemented vertical parameter renames (FORM and PITCH), double-click resets, visualizer note sync fix, and Concept 1 Direct Phase-Reset hard-sync.
* **C++ Changes**:
  * Renamed APVTS parameters: `"detune"` -> `"form"`, `"param6"` -> `"desync"`, `"param7"` -> `"pitch"`.
  * Mapped MIDI CC 1 -> `"form"`, CC 6 -> `"desync"`, CC 7 -> `"pitch"`.
  * Multiplied slave partials ($p > 0$) frequencies by `syncMultiplier = 1.0f + deSyncVal * 1.0f` (1 octave maximum sweep).
  * Reset higher partial phases to `0.0f` when the master fundamental wraps around.
  * Relocated active notes cache to WebView, resetting it during `"queryall"` message handling to fix the visualizer note initialization.
* **JS/HTML Changes**:
  * Updated layouts, text displays, worklet descriptors, and state bindings.
  * Added `dblclick` resets on `CustomSlider` and `CustomKnob` elements.
  * Synchronized Worklet slave frequency scaling and master-slave phase resets.
  * Morphed visualizer orbits to regular polygons (triangles, squares, pentagons) using polar `polyFactor` based on `desync`.
  * Drew expanding shockwave ring to represent phase resets.


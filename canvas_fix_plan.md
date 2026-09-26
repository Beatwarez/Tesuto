# Implementation Plan: Fix UI Canvas Bindings

## 1. The Root Cause
The JavaScript Web UI relies on an animation loop (nimate()) to redraw the visual canvases (drawFilterCanvas and drawSourceCanvas). 
During the parameter refactor in Build 0.75, we renamed the mod{{LANE}}_p1...p5 parameters in the HTML and C++ APVTS to explicit engine names (e.g., mod{{LANE}}_filter_cutoff).
However, we forgot to update pp.js to look for these new parameter names! 
Because drawFilterCanvas is trying to read the old modX_p1 parameters from 	his.values, it defaults to static fallback values (0.5), which is why the filter canvas no longer reacts to knob movements.

Additionally, in index.html, we missed renaming id="slider-mod{{LANE}}_p5" to slider-mod{{LANE}}_filter_morph.

## 2. Why the Source Canvas is Resized
The source canvas resizing issue is likely a cascading failure. If drawFilterCanvas throws an exception during the equestAnimationFrame loop (perhaps due to undefined parameter states interacting with math functions), the execution thread halts before 	his.drawSourceCanvas() can be called. Without continuous redrawing, the source canvas appears frozen or improperly scaled. 

## 3. The Fix

**Step 1: Fix pp.js Filter Canvas Data Bindings**
I will update drawFilterCanvas() in pp.js to read from the new specific parameter names:
- p1 -> ilter_cutoff
- p2 -> ilter_offset
- p3 -> ilter_reso
- p4 -> ilter_slope
- p5 -> ilter_morph
- ilterA -> ilter_typeA
- ilterB -> ilter_typeB

**Step 2: Fix remaining generic IDs in index.html**
I will replace id="slider-mod{{LANE}}_p5" with id="slider-mod{{LANE}}_filter_morph".

Once approved, I will implement these changes so the canvases bind properly to your knobs again!

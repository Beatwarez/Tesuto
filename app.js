// KRONOS - Experimental Additive Drone Synthesizer JS Engine & UI Controller

// ==========================================================================
// 1. AudioWorkletProcessor Code (Serialized to string to run CORS-free local)
// ==========================================================================
const workletCode = `
const MAX_VOICES = 8;
const MAX_PARTIALS = 256;
const SINE_TABLE_SIZE = 16384;

// Precalculate Sine Lookup Table for optimized DSP execution
const SINE_TABLE = new Float32Array(SINE_TABLE_SIZE);
for (let i = 0; i < SINE_TABLE_SIZE; i++) {
    SINE_TABLE[i] = Math.sin((i / SINE_TABLE_SIZE) * Math.PI * 2);
}

class DroneSynthProcessor extends AudioWorkletProcessor {
    static get parameterDescriptors() {
        return [
            { name: 'detune', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 },
            { name: 'timbre', defaultValue: 0.25, minValue: 0.0, maxValue: 1.0 },
            { name: 'cutoff', defaultValue: 0.75, minValue: 0.0, maxValue: 1.0 },
            { name: 'space', defaultValue: 0.3, minValue: 0.0, maxValue: 1.0 },
            { name: 'alter', defaultValue: 0.0, minValue: 0.0, maxValue: 1.0 }
        ];
    }

    constructor() {
        super();
        this.sampleRate = 44100;
        this.time = 0;
        
        // Initialize 8 voices
        this.voices = [];
        for (let i = 0; i < MAX_VOICES; i++) {
            this.voices.push({
                active: false,
                note: -1,
                freq: 0.0,
                currentFreq: 0.0, // smoothed
                amp: 0.0,         // current envelope amp
                targetAmp: 0.0,   // target envelope amp
                envState: 'idle', // 'idle', 'attack', 'sustain', 'release'
                phases: new Float32Array(MAX_PARTIALS),
                phaseDrifts: new Float32Array(MAX_PARTIALS),
                panLeft: new Float32Array(MAX_PARTIALS),
                panRight: new Float32Array(MAX_PARTIALS)
            });
            for (let p = 0; p < MAX_PARTIALS; p++) {
                this.voices[i].phases[p] = 0.0;
                this.voices[i].phaseDrifts[p] = Math.random() * Math.PI * 2;
                const basePan = p === 0 ? 0.5 : (p % 2 === 0 ? 0.25 : 0.75);
                this.voices[i].panLeft[p] = Math.sqrt(1 - basePan);
                this.voices[i].panRight[p] = Math.sqrt(basePan);
            }
        }

        // Receive MIDI events from main thread
        this.port.onmessage = (event) => {
            const msg = event.data;
            if (msg.type === 'note-on') {
                this.noteOn(msg.note, msg.velocity);
            } else if (msg.type === 'note-off') {
                this.noteOff(msg.note);
            } else if (msg.type === 'panic') {
                this.panic();
            }
        };
    }

    noteOn(note, velocity) {
        let voice = this.voices.find(v => v.active && v.note === note);
        if (!voice) {
            voice = this.voices.find(v => !v.active);
        }
        if (!voice) {
            // Voice steal: pick voice with lowest current envelope amplitude
            let lowestAmp = 999.0;
            let lowestIdx = 0;
            for (let i = 0; i < MAX_VOICES; i++) {
                if (this.voices[i].amp < lowestAmp) {
                    lowestAmp = this.voices[i].amp;
                    lowestIdx = i;
                }
            }
            voice = this.voices[lowestIdx];
        }

        voice.active = true;
        voice.note = note;
        const targetFreq = 440.0 * Math.pow(2, (note - 69) / 12);
        
        if (voice.envState === 'idle') {
            voice.freq = targetFreq;
            voice.currentFreq = targetFreq;
            for (let p = 0; p < MAX_PARTIALS; p++) {
                voice.phases[p] = 0.0;
            }
        } else {
            // Pitch glide if reusing active voice
            voice.freq = targetFreq;
        }

        voice.targetAmp = velocity / 127.0;
        voice.envState = 'attack';
    }

    noteOff(note) {
        const voice = this.voices.find(v => v.active && v.note === note);
        if (voice) {
            voice.targetAmp = 0.0;
            voice.envState = 'release';
        }
    }

    panic() {
        for (let i = 0; i < MAX_VOICES; i++) {
            this.voices[i].active = false;
            this.voices[i].amp = 0.0;
            this.voices[i].targetAmp = 0.0;
            this.voices[i].envState = 'idle';
        }
    }

    process(inputs, outputs, parameters) {
        // Wrap entire DSP process in try-catch to print inner errors to main thread console
        try {
            const output = outputs[0];
            if (!output) return true;
            
            const leftChannel = output[0];
            const rightChannel = output[1] || output[0]; // fallback to mono if only 1 channel
            const bufferLength = leftChannel.length;

            const sampleRate = globalThis.sampleRate || 44100;

            // 1. Calculate active voices and global normalization scale
            let activeVoicesCount = 0;
            for (let v = 0; v < MAX_VOICES; v++) {
                if (this.voices[v].active) activeVoicesCount++;
            }
            
            // Clear buffer if no active voices
            if (activeVoicesCount === 0) {
                for (let i = 0; i < bufferLength; i++) {
                    leftChannel[i] = 0;
                    if (output[1]) rightChannel[i] = 0;
                }
                return true;
            }

            const scaleFactor = 0.07 / Math.sqrt(activeVoicesCount);

            // Read parameter values for the current block
            const detuneVal = parameters.detune[0];
            const timbreVal = parameters.timbre[0];
            const cutoffVal = parameters.cutoff[0];
            const spaceVal = parameters.space[0];
            const alterVal = parameters.alter ? parameters.alter[0] : 0.0;

            // Correctly scale envelope step sizes for block-rate updates (128 samples per block)
            // Drone envelopes: smooth attack (approx 0.8s), longer release (approx 1.5s)
            const blockTime = bufferLength / sampleRate;
            const attackStep = blockTime / 0.8;
            const releaseStep = blockTime / 1.5;

            // Map Cutoff exponentially (50Hz to 12000Hz)
            const cutoffFreq = 50.0 * Math.pow(2, cutoffVal * 8.0);

            // Initialize output channels to zero
            for (let i = 0; i < bufferLength; i++) {
                leftChannel[i] = 0;
                rightChannel[i] = 0;
            }

            // Loop active voices
            for (let v = 0; v < MAX_VOICES; v++) {
                const voice = this.voices[v];
                if (!voice.active) continue;

                // Smooth glide fundamental
                voice.currentFreq += (voice.freq - voice.currentFreq) * 0.06;

                // Update voice envelope at block rate (using block-scaled speeds)
                if (voice.envState === 'attack') {
                    voice.amp += attackStep;
                    if (voice.amp >= voice.targetAmp) {
                        voice.amp = voice.targetAmp;
                        voice.envState = 'sustain';
                    }
                } else if (voice.envState === 'release') {
                    voice.amp -= releaseStep;
                    if (voice.amp <= 0.0) {
                        voice.amp = 0.0;
                        voice.active = false;
                        voice.envState = 'idle';
                        continue;
                    }
                } else if (voice.envState === 'sustain') {
                    voice.amp += (voice.targetAmp - voice.amp) * 0.01;
                }

                const currentAmp = voice.amp;
                if (currentAmp <= 0.0001) continue;

                // Compute partial frequencies and amplitudes (optimized block-rate calculation)
                const freqs = new Float32Array(MAX_PARTIALS);
                const amps = new Float32Array(MAX_PARTIALS);
                const phaseDeltas = new Float32Array(MAX_PARTIALS);
                const pL_block = new Float32Array(MAX_PARTIALS);
                const pR_block = new Float32Array(MAX_PARTIALS);
                
                const activePartials = [];

                for (let p = 0; p < MAX_PARTIALS; p++) {
                    const harmonicIndex = p + 1;

                    // Detune / Inharmonic Warp
                    const stretch = detuneVal * detuneVal * 3.5 * Math.sin(harmonicIndex * 1.57 + p * 0.1);
                    freqs[p] = voice.currentFreq * (harmonicIndex + stretch);

                    // Timbre Morphing (10 distinct shapes)
                    const getSpectralShape = (p, harmonicIndex, shapeIndex) => {
                        switch (shapeIndex) {
                            case 0: // 1. Warm Triangle/Saw
                                return 1.0 / Math.pow(harmonicIndex, 1.5);
                            case 1: // 2. Hollow Square (Odd harmonics only)
                                return (p % 2 === 0) ? (1.0 / harmonicIndex) : 0.0;
                            case 2: // 3. Comb Filter / Phased
                                return (Math.sin(p * 0.22) * 0.5 + 0.5) / Math.sqrt(harmonicIndex);
                            case 3: // 4. High Fizz (High-pass)
                                return (p / 256.0) * (1.0 / Math.sqrt(harmonicIndex));
                            case 4: // 5. Formant Vocal "Ooh" (Double peaks near H3 & H8)
                                return Math.exp(-Math.pow(harmonicIndex - 3, 2) / 2)
                                     + 0.5 * Math.exp(-Math.pow(harmonicIndex - 8, 2) / 8);
                            case 5: // 6. Formant Vocal "Aah" (Double peaks near H6 & H14)
                                return Math.exp(-Math.pow(harmonicIndex - 6, 2) / 4)
                                     + 0.4 * Math.exp(-Math.pow(harmonicIndex - 14, 2) / 16);
                            case 6: // 7. Octave Double (Even harmonics dominant)
                                return (p % 2 === 1) ? (1.0 / Math.pow(harmonicIndex, 1.2)) : (0.1 / harmonicIndex);
                            case 7: // 8. Metallic / Inharmonic (Golden ratio spacing)
                                return (Math.sin(p * 1.618) * 0.5 + 0.5) / Math.pow(harmonicIndex, 0.8);
                            case 8: // 9. Resonance Spike (Resonant peak at H12)
                                return (p === 0) ? 1.0 : (0.05 + 0.95 * Math.exp(-Math.pow(harmonicIndex - 12, 2) / 2));
                            case 9: // 10. Grit (Deterministic noise-like hash)
                                return (Math.sin(p * 123.456) * 0.5 + 0.5) / harmonicIndex;
                            default:
                                return 0.0;
                        }
                    };

                    const scaledTimbre = timbreVal * 9.0;
                    let timbreIdx = Math.floor(scaledTimbre);
                    let timbreMix = scaledTimbre - timbreIdx;
                    if (timbreIdx >= 9) {
                        timbreIdx = 8;
                        timbreMix = 1.0;
                    }

                    const baseAmp = getSpectralShape(p, harmonicIndex, timbreIdx) * (1 - timbreMix)
                                  + getSpectralShape(p, harmonicIndex, timbreIdx + 1) * timbreMix;

                    // Cutoff Filter
                    const ratio = freqs[p] / cutoffFreq;
                    const filterMult = 1.0 / (1.0 + Math.pow(ratio, 6.0));

                    // Space (Organic LFO drift)
                    const lfoDrift = Math.sin(this.time * 1.2 + voice.phaseDrifts[p]) * spaceVal * 0.3;

                    amps[p] = baseAmp * filterMult * currentAmp * (1.0 + lfoDrift);

                    // Precalculate phase delta and panning
                    phaseDeltas[p] = freqs[p] / sampleRate;

                    if (p === 0) {
                        pL_block[p] = voice.panLeft[p] * (1 - spaceVal) + 0.707 * spaceVal;
                        pR_block[p] = voice.panRight[p] * (1 - spaceVal) + 0.707 * spaceVal;
                    } else if (p % 2 === 0) {
                        pL_block[p] = voice.panLeft[p] * (1 - spaceVal) + 1.0 * spaceVal;
                        pR_block[p] = voice.panRight[p] * (1 - spaceVal) + 0.0 * spaceVal;
                    } else {
                        pL_block[p] = voice.panLeft[p] * (1 - spaceVal) + 0.0 * spaceVal;
                        pR_block[p] = voice.panRight[p] * (1 - spaceVal) + 1.0 * spaceVal;
                    }

                    // Collect active partials
                    if (amps[p] >= 0.0001) {
                        activePartials.push(p);
                    }
                }

                // Sample loop
                for (let i = 0; i < bufferLength; i++) {
                    let sumL = 0.0;
                    let sumR = 0.0;

                    for (let idx = 0; idx < activePartials.length; idx++) {
                        const p = activePartials[idx];
                        const a = amps[p];

                        // Increment phase
                        voice.phases[p] += phaseDeltas[p];
                        if (voice.phases[p] >= 1.0) {
                            voice.phases[p] -= 1.0;
                        }

                        // Lookup sine table with phase wrapped to [0, 1)
                        const sineIdx = ((voice.phases[p] * SINE_TABLE_SIZE) | 0) & (SINE_TABLE_SIZE - 1);
                        const val = SINE_TABLE[sineIdx];

                        sumL += val * a * pL_block[p];
                        sumR += val * a * pR_block[p];
                    }

                    leftChannel[i] += sumL * scaleFactor;
                    rightChannel[i] += sumR * scaleFactor;

                    this.time += 1.0 / sampleRate;
                }
            }

            // Output limiting (saturation) to prevent digital clipping
            for (let i = 0; i < bufferLength; i++) {
                leftChannel[i] = Math.tanh(leftChannel[i]);
                rightChannel[i] = Math.tanh(rightChannel[i]);
            }

            return true;
        } catch (err) {
            // Send DSP crash message back to main thread
            this.port.postMessage({
                type: 'dsp-error',
                message: err.message,
                stack: err.stack
            });
            return false;
        }
    }
}

registerProcessor('drone-synth-processor', DroneSynthProcessor);
`;

// ==========================================================================
// 2. Custom Slider Pointer Event Manager Class
// ==========================================================================
class CustomSlider {
    constructor(id, min, max, defaultValue, step, onChange) {
        this.element = document.getElementById(id);
        this.track = this.element.querySelector('.slider-track');
        this.fill = this.element.querySelector('.slider-fill');
        this.thumb = this.element.querySelector('.slider-thumb');
        
        this.min = min;
        this.max = max;
        this.value = defaultValue;
        this.step = step;
        this.onChange = onChange;
        this.isDragging = false;

        this.updateUI();

        // Pointer listeners for universal mouse/touch support
        this.element.addEventListener('pointerdown', (e) => this.onPointerDown(e));
        window.addEventListener('pointermove', (e) => this.onPointerMove(e));
        window.addEventListener('pointerup', () => this.onPointerUp());
    }

    setValue(val) {
        // Clamp and step
        let clamped = Math.max(this.min, Math.min(this.max, val));
        if (this.step) {
            clamped = Math.round(clamped / this.step) * this.step;
        }
        this.value = clamped;
        this.updateUI();
        if (this.onChange) this.onChange(this.value);
    }

    updateUI() {
        const percentage = (this.value - this.min) / (this.max - this.min);
        // Map percentage to CSS styles
        this.fill.style.height = `${percentage * 100}%`;
        // Offset center of thumb dynamically
        const halfThumbHeight = (this.thumb.offsetHeight || 14) / 2;
        this.thumb.style.bottom = `calc(${percentage * 100}% - ${halfThumbHeight}px)`;
    }

    onPointerDown(e) {
        this.isDragging = true;
        this.element.setPointerCapture(e.pointerId);
        this.handlePointer(e);
    }

    onPointerMove(e) {
        if (!this.isDragging) return;
        this.handlePointer(e);
    }

    onPointerUp() {
        this.isDragging = false;
    }

    handlePointer(e) {
        const rect = this.track.getBoundingClientRect();
        // Calculate Y relative to bottom of track
        const relativeY = rect.bottom - e.clientY;
        const percentage = Math.max(0, Math.min(1, relativeY / rect.height));
        
        const val = this.min + percentage * (this.max - this.min);
        this.setValue(val);
    }

    // Get absolute center coordinate of slider thumb relative to canvas
    getThumbCanvasPos(canvas) {
        const thumbRect = this.thumb.getBoundingClientRect();
        const canvasRect = canvas.getBoundingClientRect();
        return {
            x: thumbRect.left + thumbRect.width / 2 - canvasRect.left,
            y: thumbRect.top + thumbRect.height / 2 - canvasRect.top
        };
    }
}

// ==========================================================================
// 2b. Custom Knob Pointer Event Manager Class
// ==========================================================================
class CustomKnob {
    constructor(id, min, max, defaultValue, isSeconds, isLogarithmic, onChange) {
        this.element = document.getElementById(id);
        this.dial = this.element.querySelector('.knob-dial');
        
        this.min = min;
        this.max = max;
        this.value = defaultValue;
        this.isSeconds = isSeconds;
        this.isLogarithmic = isLogarithmic;
        this.onChange = onChange;
        this.isDragging = false;
        
        this.startY = 0;
        this.startValue = defaultValue;

        this.updateUI();

        // Pointer listeners for universal mouse/touch support
        this.element.addEventListener('pointerdown', (e) => this.onPointerDown(e));
        window.addEventListener('pointermove', (e) => this.onPointerMove(e));
        window.addEventListener('pointerup', () => this.onPointerUp());
    }

    setValue(val) {
        const clamped = Math.max(this.min, Math.min(this.max, val));
        this.value = clamped;
        this.updateUI();
        if (this.onChange) this.onChange(this.value);
    }

    updateUI() {
        let pct = 0;
        if (this.isLogarithmic) {
            pct = Math.log(this.value / this.min) / Math.log(this.max / this.min);
        } else {
            const range = this.max - this.min;
            pct = (this.value - this.min) / (range > 0 ? range : 1);
        }
        const degrees = -135 + pct * 270; // 270 degree sweep
        this.dial.style.transform = `rotate(${degrees}deg)`;
    }

    onPointerDown(e) {
        this.isDragging = true;
        this.startY = e.clientY;
        this.startValue = this.value;
        this.element.setPointerCapture(e.pointerId);
    }

    onPointerMove(e) {
        if (!this.isDragging) return;
        const deltaY = this.startY - e.clientY; // drag up increases
        const pxRange = 120.0; // 120px for full sweep
        
        let startPct = 0;
        if (this.isLogarithmic) {
            startPct = Math.log(this.startValue / this.min) / Math.log(this.max / this.min);
        } else {
            startPct = (this.startValue - this.min) / (this.max - this.min);
        }
        
        const deltaPct = deltaY / pxRange;
        const newPct = Math.max(0.0, Math.min(1.0, startPct + deltaPct));
        
        let newVal = 0;
        if (this.isLogarithmic) {
            newVal = this.min * Math.pow(this.max / this.min, newPct);
        } else {
            newVal = this.min + newPct * (this.max - this.min);
        }
        
        this.setValue(newVal);
    }

    onPointerUp() {
        this.isDragging = false;
    }
}

// ==========================================================================
// 3. Main Synthesizer Application Manager
// ==========================================================================
class KronosSynth {
    constructor() {
        this.audioContext = null;
        this.synthNode = null;

        // UI elements
        this.canvas = document.getElementById('canvas-visualizer');
        this.ctx = this.canvas.getContext('2d');

        // State variables
        this.activeKeys = new Set();
        this.notesDown = {};
        this.visualEnvelope = 0.0;

        // 12 Param Values (including new right-hand sliders and ADSR knobs)
        this.values = {
            detune: 0.00,
            timbre: 0.25,
            cutoff: 0.75,
            space: 0.30,
            alter: 0.00,
            param6: 0.25,
            param7: 0.50,
            param8: 0.30,
            attack: 0.80,
            decay: 0.30,
            sustain: 0.80,
            release: 1.50
        };

        // Initialize Custom Sliders (Left and Right symmetric panels)
        this.sliders = {
            detune: new CustomSlider('slider-detune', 0, 1, this.values.detune, 0.001, (v) => this.onSliderChange('detune', v)),
            timbre: new CustomSlider('slider-timbre', 0, 1, this.values.timbre, 0.001, (v) => this.onSliderChange('timbre', v)),
            cutoff: new CustomSlider('slider-cutoff', 0, 1, this.values.cutoff, 0.001, (v) => this.onSliderChange('cutoff', v)),
            space: new CustomSlider('slider-space', 0, 1, this.values.space, 0.001, (v) => this.onSliderChange('space', v)),
            alter: new CustomSlider('slider-alter', 0, 1, this.values.alter, 0.001, (v) => this.onSliderChange('alter', v)),
            param6: new CustomSlider('slider-param6', 0, 1, this.values.param6, 0.001, (v) => this.onSliderChange('param6', v)),
            param7: new CustomSlider('slider-param7', 0, 1, this.values.param7, 0.001, (v) => this.onSliderChange('param7', v)),
            param8: new CustomSlider('slider-param8', 0, 1, this.values.param8, 0.001, (v) => this.onSliderChange('param8', v))
        };

        // Initialize Custom Knobs (ADSR panel)
        this.knobs = {
            attack: new CustomKnob('knob-attack', 0.01, 5.0, this.values.attack, true, true, (v) => this.onKnobChange('attack', v)),
            decay: new CustomKnob('knob-decay', 0.01, 5.0, this.values.decay, true, true, (v) => this.onKnobChange('decay', v)),
            sustain: new CustomKnob('knob-sustain', 0.0, 1.0, this.values.sustain, false, false, (v) => this.onKnobChange('sustain', v)),
            release: new CustomKnob('knob-release', 0.01, 8.0, this.values.release, true, true, (v) => this.onKnobChange('release', v))
        };

        // Build UI overlays and canvas sizing
        this.resizeCanvas();
        window.addEventListener('resize', () => this.resizeCanvas());
        this.buildPianoKeyboard();
        this.setupKeyboardListeners();

        // Animation Particles
        this.particles = [];
        this.initParticles();
        this.animate();

        // Initialize Audio engine setup immediately
        this.initAudio();

        // Request initial parameter states from C++ on load
        this.sendParamToCpp("queryall", 0);
    }

    async initAudio() {
        try {
            // Bypass Web Audio when running as a native plugin
            const isNative = (window.chrome && window.chrome.webview) || (window.webkit && window.webkit.messageHandlers);
            if (isNative) {
                console.log("KRONOS running inside native host, Web Audio bypassed.");
                return;
            }

            const AudioContext = window.AudioContext || window.webkitAudioContext;
            this.audioContext = new AudioContext({ sampleRate: 44100 });

            // Create inline AudioWorkletBlob
            const blob = new Blob([workletCode], { type: 'application/javascript' });
            const workletUrl = URL.createObjectURL(blob);

            await this.audioContext.audioWorklet.addModule(workletUrl);

            this.synthNode = new AudioWorkletNode(this.audioContext, 'drone-synth-processor', {
                numberOfInputs: 0,
                numberOfOutputs: 1,
                outputChannelCount: [2]
            });

            // Listen for internal worklet console errors
            this.synthNode.port.onmessage = (event) => {
                if (event.data.type === 'dsp-error') {
                    console.error("DSP CRASHED IN WORKLET:", event.data.message, event.data.stack);
                }
            };

            // Set initial params
            this.updateNodeParameters();

            // Connect
            this.synthNode.connect(this.audioContext.destination);

            // Hide overlay if present
            if (this.overlay) {
                this.overlay.classList.add('hidden');
            }

            // Start MIDI
            this.initMIDI();

            console.log("KRONOS AudioWorklet running smoothly.");
        } catch (err) {
            console.error("Failed to start Web Audio Engine:", err);
            alert("Web Audio Worklets are not supported on this browser version.");
        }
    }

    sendParamToCpp(param, val) {
        if (window.__JUCE__ && window.__JUCE__.backend) {
            // Replicate JUCE 8's exact getNativeFunction invoke pattern by providing a resultId
            const resultId = Math.floor(Math.random() * 1000000);
            window.__JUCE__.backend.emitEvent("__juce__invoke", {
                name: "sendParamToCpp",
                params: [param, val],
                resultId: resultId
            });
        } else {
            console.log(`sendParamToCpp fallback: ${param} = ${val}`);
        }
    }

    onSliderChange(param, val) {
        this.values[param] = val;
        const displayEl = document.getElementById(`val-${param}`);
        if (displayEl) {
            displayEl.textContent = val.toFixed(2);
        }

        if (this.synthNode) {
            const audioParam = this.synthNode.parameters.get(param);
            if (audioParam) {
                audioParam.setTargetAtTime(val, this.audioContext.currentTime, 0.02);
            }
        }

        this.sendParamToCpp(param, val);
    }

    onKnobChange(param, val) {
        this.values[param] = val;
        
        let displayVal = val.toFixed(2);
        if (param === 'attack' || param === 'decay' || param === 'release') {
            displayVal += 's';
        }
        
        const displayEl = document.getElementById(`val-${param}`);
        if (displayEl) {
            displayEl.textContent = displayVal;
        }

        this.sendParamToCpp(param, val);
    }

    updateParamFromCpp(param, val) {
        if (this.sliders[param] && !this.sliders[param].isDragging) {
            this.values[param] = val;
            const valDisplay = document.getElementById(`val-${param}`);
            if (valDisplay) {
                valDisplay.textContent = val.toFixed(2);
            }
            this.sliders[param].value = val;
            this.sliders[param].updateUI();
        } else if (this.knobs[param] && !this.knobs[param].isDragging) {
            this.values[param] = val;
            let displayVal = val.toFixed(2);
            if (param === 'attack' || param === 'decay' || param === 'release') {
                displayVal += 's';
            }
            const valDisplay = document.getElementById(`val-${param}`);
            if (valDisplay) {
                valDisplay.textContent = displayVal;
            }
            this.knobs[param].value = val;
            this.knobs[param].updateUI();
        }
    }

    updateNodeParameters() {
        if (!this.synthNode) return;
        Object.keys(this.values).forEach(key => {
            const param = this.synthNode.parameters.get(key);
            if (param) {
                param.setValueAtTime(this.values[key], this.audioContext.currentTime);
            }
        });
    }

    triggerNoteOn(note, velocity = 100) {
        this.activeKeys.add(note);
        this.updatePianoUI();

        if (this.synthNode) {
            this.synthNode.port.postMessage({
                type: 'note-on',
                note: note,
                velocity: velocity
            });
        }
        
        // Notify C++ plugin of screen-played note
        this.sendParamToCpp("noteon", note);
    }

    triggerNoteOff(note) {
        this.activeKeys.delete(note);
        this.updatePianoUI();

        if (this.synthNode) {
            this.synthNode.port.postMessage({
                type: 'note-off',
                note: note
            });
        }
        
        // Notify C++ plugin of screen-released note
        this.sendParamToCpp("noteoff", note);
    }

    // ==========================================================================
    // 4. Keyboard Generator & Input Mapper
    // ==========================================================================
    buildPianoKeyboard() {
        const keyboard = document.getElementById('piano-keyboard');
        if (!keyboard) return;
        keyboard.innerHTML = '';
        
        // Notes C3 to C5 (MIDI 48 to 72)
        const keysInfo = [
            { note: 48, isBlack: false },
            { note: 49, isBlack: true },
            { note: 50, isBlack: false },
            { note: 51, isBlack: true },
            { note: 52, isBlack: false },
            { note: 53, isBlack: false },
            { note: 54, isBlack: true },
            { note: 55, isBlack: false },
            { note: 56, isBlack: true },
            { note: 57, isBlack: false },
            { note: 58, isBlack: true },
            { note: 59, isBlack: false },
            { note: 60, isBlack: false },
            { note: 61, isBlack: true },
            { note: 62, isBlack: false },
            { note: 63, isBlack: true },
            { note: 64, isBlack: false },
            { note: 65, isBlack: false },
            { note: 66, isBlack: true },
            { note: 67, isBlack: false },
            { note: 68, isBlack: true },
            { note: 69, isBlack: false },
            { note: 70, isBlack: true },
            { note: 71, isBlack: false },
            { note: 72, isBlack: false }
        ];

        keysInfo.forEach(info => {
            const keyEl = document.createElement('div');
            keyEl.className = `key ${info.isBlack ? 'black' : 'white'}`;
            keyEl.dataset.note = info.note;

            keyEl.addEventListener('mousedown', (e) => {
                e.preventDefault();
                this.triggerNoteOn(info.note);
            });
            keyEl.addEventListener('mouseenter', (e) => {
                if (e.buttons === 1) this.triggerNoteOn(info.note);
            });
            keyEl.addEventListener('mouseleave', () => this.triggerNoteOff(info.note));
            keyEl.addEventListener('mouseup', () => this.triggerNoteOff(info.note));

            keyEl.addEventListener('touchstart', (e) => {
                e.preventDefault();
                this.triggerNoteOn(info.note);
            }, { passive: false });
            keyEl.addEventListener('touchend', (e) => {
                e.preventDefault();
                this.triggerNoteOff(info.note);
            }, { passive: false });

            keyboard.appendChild(keyEl);
        });
    }

    updatePianoUI() {
        const keys = document.querySelectorAll('.piano-keyboard .key');
        keys.forEach(key => {
            const note = parseInt(key.dataset.note);
            if (this.activeKeys.has(note)) {
                key.classList.add('active');
            } else {
                key.classList.remove('active');
            }
        });
    }

    setupKeyboardListeners() {
        const keyboardMap = {
            'KeyA': 48, 'KeyW': 49, 'KeyS': 50, 'KeyE': 51, 'KeyD': 52,
            'KeyF': 53, 'KeyT': 54, 'KeyG': 55, 'KeyY': 56, 'KeyH': 57,
            'KeyU': 58, 'KeyJ': 59, 'KeyK': 60
        };

        window.addEventListener('keydown', (e) => {
            if (e.repeat || !this.audioContext) return;
            const note = keyboardMap[e.code];
            if (note !== undefined && !this.notesDown[e.code]) {
                this.notesDown[e.code] = true;
                this.triggerNoteOn(note);
            }
        });

        window.addEventListener('keyup', (e) => {
            const note = keyboardMap[e.code];
            if (note !== undefined) {
                this.notesDown[e.code] = false;
                this.triggerNoteOff(note);
            }
        });
    }

    // ==========================================================================
    // 5. MIDI Connection Handler
    // ==========================================================================
    initMIDI() {
        if (navigator.requestMIDIAccess) {
            navigator.requestMIDIAccess()
                .then(midi => this.onMIDISuccess(midi), () => this.onMIDIFailure());
        } else {
            if (this.midiStatusText) {
                this.midiStatusText.textContent = "MIDI UNSUPPORTED";
            }
        }
    }

    onMIDISuccess(midiAccess) {
        const inputs = midiAccess.inputs.values();
        let hasDevices = false;
        for (let input of inputs) {
            input.onmidimessage = (msg) => this.onMIDIMessage(msg);
            hasDevices = true;
        }

        // Avoid infinite state change loop
        if (!midiAccess.hasStateChangeListener) {
            midiAccess.onstatechange = (e) => {
                if (e.port.type === 'input') this.initMIDI();
            };
            midiAccess.hasStateChangeListener = true;
        }

        if (hasDevices) {
            if (this.midiStatusIndicator) this.midiStatusIndicator.classList.add('active');
            if (this.midiStatusText) this.midiStatusText.textContent = "MIDI READY";
        } else {
            if (this.midiStatusIndicator) this.midiStatusIndicator.classList.remove('active');
            if (this.midiStatusText) this.midiStatusText.textContent = "NO MIDI DEVICES";
        }
    }

    onMIDIFailure() {
        if (this.midiStatusText) this.midiStatusText.textContent = "MIDI BLOCKED";
        if (this.midiStatusIndicator) this.midiStatusIndicator.classList.remove('active');
    }

    onMIDIMessage(msg) {
        const data = msg.data;
        const status = data[0] & 0xf0;
        const note = data[1];
        const velocity = data[2];

        if (status === 144 && velocity > 0) {
            this.triggerNoteOn(note, velocity);
        } else if (status === 128 || (status === 144 && velocity === 0)) {
            this.triggerNoteOff(note);
        }
    }

    // ==========================================================================
    // 6. Canvas Responsive Resizer
    // ==========================================================================
    resizeCanvas() {
        const parent = this.canvas.parentElement;
        const rect = parent ? parent.getBoundingClientRect() : { width: window.innerWidth, height: window.innerHeight };
        this.canvas.width = rect.width * window.devicePixelRatio;
        this.canvas.height = rect.height * window.devicePixelRatio;
        this.ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
    }

    initParticles() {
        this.particles = [];
        for (let i = 0; i < 256; i++) {
            this.particles.push({
                index: i,
                phase: Math.random() * Math.PI * 2,
                speed: 0.008 + (i / 256) * 0.024,
                rotSpeed: 0.002 + Math.random() * 0.005,
                history: [] // stores previous coordinates for crisp trail render
            });
        }
    }

    animate() {
        requestAnimationFrame(() => this.animate());
        this.draw();
    }

    // ==========================================================================
    // 7. Hypnotic Partials Render Engine
    // ==========================================================================
    draw() {
        const w = this.canvas.width / window.devicePixelRatio;
        const h = this.canvas.height / window.devicePixelRatio;

        // Clear canvas completely to eliminate static line burn-in / smudging
        this.ctx.fillStyle = '#252528';
        this.ctx.fillRect(0, 0, w, h);

        // Center coordinates inside the middle panel container
        const centerX = w / 2;
        const centerY = h / 2;
        const maxRadius = Math.min(w * 0.42, h * 0.42);

        const detune = this.values.detune;
        const timbre = this.values.timbre;
        const cutoff = this.values.cutoff;
        const space = this.values.space;

        // Smoothly update visual envelope matching the DSP ADSR (0.8s attack, 1.5s release)
        const targetEnvelope = this.activeKeys.size > 0 ? 1.0 : 0.0;
        if (targetEnvelope > this.visualEnvelope) {
            this.visualEnvelope += (targetEnvelope - this.visualEnvelope) * 0.025;
        } else {
            this.visualEnvelope += (targetEnvelope - this.visualEnvelope) * 0.012;
        }
        const activeRatio = this.visualEnvelope;

        // 1. Draw slider organic connection lines (linked directly to slider thumb pixels)
        // Set to fade in/out with the ADSR envelope
        const keys = Object.keys(this.sliders);
        keys.forEach((key, index) => {
            const slider = this.sliders[key];
            const start = slider.getThumbCanvasPos(this.canvas);
            
            // Connect to orbital layers (modulo 4 prevents expanding orbit for right side sliders)
            const orbitIdx = index % 4;
            let phaseOffset = orbitIdx * (Math.PI / 2);
            if (index >= 4) {
                phaseOffset += Math.PI; // Connect to opposite/distinct side of orbits for right sliders
            }
            const t = Date.now() * 0.0004 + phaseOffset;
            const anchorRadius = maxRadius * (0.2 + orbitIdx * 0.2) * (1.0 - cutoff * 0.1);
            
            // Rotate anchor target on grid
            const targetX = centerX + anchorRadius * Math.cos(t);
            const targetY = centerY + anchorRadius * Math.sin(t);

            // Drag rope wobble based on detune/space macro
            const wobbleY = Math.sin(Date.now() * 0.002 + index * 2) * (detune * 25);

            // Draw curved light-gray visual guide line (always visible, floats cleanly)
            const guideOpacity = 0.45 + activeRatio * 0.55;
            this.ctx.strokeStyle = `rgba(224, 224, 230, ${(0.08 + space * 0.12) * guideOpacity})`;
            this.ctx.lineWidth = 1.0;
            this.ctx.beginPath();
            this.ctx.moveTo(start.x, start.y);
            this.ctx.bezierCurveTo(
                start.x + 80, start.y + wobbleY,
                centerX - 120, targetY - wobbleY,
                targetX, targetY
            );
            this.ctx.stroke();

            // Draw glowing anchor point on the canvas orbit
            const anchorOpacity = 0.5 + activeRatio * 0.5;
            this.ctx.fillStyle = `rgba(255, 255, 255, ${(0.22 + space * 0.28) * anchorOpacity})`;
            this.ctx.beginPath();
            this.ctx.arc(targetX, targetY, 3, 0, Math.PI * 2);
            this.ctx.fill();
        });

        // 2. Draw geometric grid background
        const gridCount = 6;
        this.ctx.lineWidth = 1.0;
        for (let i = 1; i <= gridCount; i++) {
            const rad = maxRadius * (i / gridCount) * (1.0 - cutoff * 0.15);
            this.ctx.strokeStyle = `rgba(255, 255, 255, ${0.015 + (1.0 - space * 0.5) * 0.025})`;
            this.ctx.beginPath();
            for (let angle = 0; angle <= Math.PI * 2; angle += 0.05) {
                const warp = Math.sin(angle * 5 + Date.now() * 0.0008) * detune * 14 * (i / gridCount);
                const x = centerX + (rad + warp) * Math.cos(angle);
                const y = centerY + (rad + warp) * Math.sin(angle);
                if (angle === 0) this.ctx.moveTo(x, y);
                else this.ctx.lineTo(x, y);
            }
            this.ctx.closePath();
            this.ctx.stroke();
        }

        // 3. Draw 256 partials mandala with fading trails
        let prevX = 0;
        let prevY = 0;

        for (let i = 0; i < this.particles.length; i++) {
            const p = this.particles[i];

            // Speed up drift when space is high
            p.phase += p.speed * (1.0 + space * 2.5);

            // Angle warp caused by detune slider (harmonic to chaotic Moiré)
            const angleWarp = Math.sin(i * 0.12 + p.phase * 0.05) * detune * detune * 5.0;
            const theta = (i * 0.22) + p.phase * 0.08 + angleWarp;

            // Amplitude envelope shape calculation
            let amp = 1.0;
            if (timbre < 0.5) {
                const mix = timbre * 2.0;
                const ampA = 1.0 / Math.pow(i + 1, 0.75);
                const ampB = Math.sin(i * 0.22) * 0.5 + 0.5;
                amp = ampA * (1 - mix) + ampB * mix;
            } else {
                const mix = (timbre - 0.5) * 2.0;
                const ampB = Math.sin(i * 0.22) * 0.5 + 0.5;
                const ampC = 1.0 - (i / this.particles.length);
                amp = ampB * (1 - mix) + ampC * mix;
            }

            // Cutoff limiter
            const filterCutoffIndex = cutoff * this.particles.length;
            if (i > filterCutoffIndex) {
                amp *= Math.max(0, 1.0 - (i - filterCutoffIndex) / 24.0);
            }

            const dist = maxRadius * (0.15 + 0.85 * (i / this.particles.length));
            // Tiny continuous orbit modulation even when silent, morphing to deep active waves
            const modulationScale = 8.0 + activeRatio * 37.0;
            const amplitudeScale = amp * modulationScale;
            const modulation = Math.sin(p.phase + i * 0.4) * amplitudeScale;

            const r = dist + modulation;
            const x = centerX + r * Math.cos(theta);
            const y = centerY + r * Math.sin(theta);

            // Update particle coordinate history
            p.history.push({ x: x, y: y, amp: amp });
            if (p.history.length > 5) {
                p.history.shift();
            }

            // Slowly rotate base hue over time (cycles colors dynamically)
            const baseHue = (Date.now() * 0.012) % 360;
            const hue = (baseHue + (i / this.particles.length) * 80) % 360;
            const sat = activeRatio * 85; // 0% (gray) to 85% (vibrant pastel) saturation
            const light = 42 + activeRatio * 38; // 42% (mid gray) to 80% (bright pastel) lightness

            const headOpacity = 0.45 + activeRatio * 0.45; // 0.45 (idle) to 0.90 (bright)
            const trailOpacity = 0.10 + activeRatio * 0.12; // 0.10 (idle) to 0.22 (bright)
            const lineOpacity = (0.08 + activeRatio * 0.12) * (1.0 - detune * 0.5); // 0.08 (idle) to 0.20 (bright)

            // Render fading trails from history
            for (let hIdx = 0; hIdx < p.history.length; hIdx++) {
                const hist = p.history[hIdx];
                const alpha = (hIdx + 1) / p.history.length;
                this.ctx.fillStyle = `hsla(${hue}, ${sat}%, ${light}%, ${hist.amp * trailOpacity * alpha})`;
                this.ctx.beginPath();
                this.ctx.arc(hist.x, hist.y, 0.8 + hist.amp * 2.0 * alpha, 0, Math.PI * 2);
                this.ctx.fill();
            }

            // Draw current active head particle
            this.ctx.fillStyle = `hsla(${hue}, ${sat}%, ${light}%, ${amp * headOpacity})`;
            this.ctx.beginPath();
            this.ctx.arc(x, y, 1.2 + amp * 2.2, 0, Math.PI * 2);
            this.ctx.fill();

            // Interconnecting elastic mesh strings (clean, no trails)
            if (i > 0) {
                this.ctx.strokeStyle = `hsla(${hue}, ${sat}%, ${light - 10}%, ${amp * lineOpacity})`;
                this.ctx.beginPath();
                this.ctx.moveTo(prevX, prevY);
                this.ctx.lineTo(x, y);
                this.ctx.stroke();
            }

            prevX = x;
            prevY = y;
        }
    }
}

// Start Synthesizer
window.addEventListener('DOMContentLoaded', () => {
    window.kronosSynth = new KronosSynth();
});

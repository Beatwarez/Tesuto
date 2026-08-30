#pragma once

#include <JuceHeader.h>

extern float sineTable[32768];

// ==========================================================================
// 1. Synthesiser Voice Class
// ==========================================================================
class KronosVoice : public juce::MPESynthesiserVoice {
public:
  KronosVoice() {
    for (int p = 0; p < 256; ++p) {
      phases[p] = 0.0f;
      phaseDrifts[p] = juce::Random::getSystemRandom().nextFloat() *
                       juce::MathConstants<float>::twoPi;
      float basePan = (p == 0) ? 0.5f : ((p % 2 == 0) ? 0.25f : 0.75f);
      panLeft[p] = std::sqrt(1.0f - basePan);
      panRight[p] = std::sqrt(basePan);
      p_send_gain[p] = 0.0f;
    }
  }

  void noteStarted() override {
    noteNumber = currentlyPlayingNote.initialNote;
    float targetFreq = (float)currentlyPlayingNote.getFrequencyInHertz();

    fundamentalFreq = targetFreq;
    currentFundamentalFreq = targetFreq;
    for (int p = 0; p < 256; ++p) {
      phases[p] = 0.0f;
      smoothedAmps[p] = 0.0f;
    }
    voiceActive = true;
    targetAmp = currentlyPlayingNote.noteOnVelocity.asUnsignedFloat();
    localTimbreMod = currentlyPlayingNote.timbre.asUnsignedFloat() * 0.4f;

    adsr.setSampleRate (getSampleRate() > 0.0 ? getSampleRate() : 44100.0);
    adsr.noteOn();
  }

  void noteStopped(bool allowTailOff) override {
    if (allowTailOff) {
      adsr.noteOff();
    } else {
      adsr.reset();
      clearCurrentNote();
      voiceActive = false;
    }
  }

  void notePitchbendChanged() override {
    fundamentalFreq = (float)currentlyPlayingNote.getFrequencyInHertz();
  }

  void notePressureChanged() override {
    float pressure = currentlyPlayingNote.pressure.asUnsignedFloat();
    targetAmp = currentlyPlayingNote.noteOnVelocity.asUnsignedFloat() *
                (0.3f + pressure * 0.7f);
  }

  void noteTimbreChanged() override {
    localTimbreMod = currentlyPlayingNote.timbre.asUnsignedFloat() * 0.4f;
  }

  void noteKeyStateChanged() override {}


  inline float calculateFilterMult(float freq, float fc, float rawReso, float rawSlope, int type) {
      if (fc < 1.0f) return 0.0f;
      float x = freq / fc;
      float x2 = x * x;
      
      if (type <= 3) {
          float q = 0.707f * std::exp(rawReso * 3.0f);
          float n = 1.0f + rawSlope * 3.0f;
          float D = (1.0f - x2) * (1.0f - x2) + (x2 / (q * q));
          float denom = std::pow(D, n * 0.5f);
          if (denom < 0.000001f) denom = 0.000001f;
          if (type == 0) return 1.0f / denom;
          if (type == 1) return std::pow(x / q, n) / denom;
          if (type == 2) return std::pow(x2, n) / denom;
          if (type == 3) return std::pow(std::abs(1.0f - x2), n) / denom;
      }
      if (type == 4) {
          if (freq <= fc) {
              float peakDist = 1.0f - x;
              if (peakDist < 0.1f && peakDist >= 0.0f) return 1.0f + rawReso * (0.1f - peakDist) * 20.0f;
              return 1.0f;
          } else {
              return rawSlope * rawSlope;
          }
      }
      if (type == 5) {
          float h_idx = std::round(freq / 50.0f);
          float threshH = fc / 50.0f;
          if (h_idx > threshH) {
              bool isOdd = ((int)h_idx % 2) != 0;
              bool targetOdd = rawSlope >= 0.5f;
              if (isOdd == targetOdd) return 1.0f - rawReso;
          }
          return 1.0f;
      }
      if (type == 6) {
          float phase = rawSlope * 6.283185307f;
          float comb = 0.5f - 0.5f * std::cos(freq * 6.283185307f / fc + phase);
          return 1.0f - rawReso * comb;
      }
      if (type == 7) {
          float v = rawSlope;
          float p1 = 700.0f * (1.0f - v) * (1.0f - v) + 300.0f * 2.0f * v * (1.0f - v) + 270.0f * v * v;
          float p2 = 1100.0f * (1.0f - v) * (1.0f - v) + 870.0f * 2.0f * v * (1.0f - v) + 2300.0f * v * v;
          float p3 = 2400.0f * (1.0f - v) * (1.0f - v) + 2200.0f * 2.0f * v * (1.0f - v) + 3000.0f * v * v;
          float shift = fc / 1000.0f;
          p1 *= shift; p2 *= shift; p3 *= shift;
          float width = 0.1f + (1.0f - rawReso) * 0.4f;
          auto g = [freq, width](float center) {
              float diff = (freq - center) / (center * width);
              return std::exp(-diff * diff);
          };
          float mult = g(p1) + 0.5f * g(p2) + 0.2f * g(p3);
          return 0.1f + mult * 2.0f;
      }
      if (type == 8) {
          if (freq > fc) {
              float h_val = std::fmod(freq * 12.9898f + 78.233f, 1.0f);
              h_val = std::fmod(h_val * 43758.5453f, 1.0f);
              if (h_val > rawReso) return 1.0f - rawSlope;
          }
          return 1.0f;
      }
      if (type == 9) {
          float angle = rawSlope * 2.0f - 1.0f;
          float tilt = std::pow(freq / fc, angle);
          float peak = 0.0f;
          if (rawReso > 0.01f) {
              float dist = std::abs(1.0f - x);
              if (dist < 0.2f) peak = rawReso * (0.2f - dist) * 5.0f;
          }
          return tilt + peak;
      }
      return 1.0f;
  }

  void updateParams(float form, float timbre, float typeA, float typeB, float filterMorph, float filterMorphMod, float filterCutoff, float filterCutoffMod, float filterOffset, float filterReso, float filterSlope, float filter, float space, float cloud, float size, float sweep, float desync, float pitch) {
    formVal = form;
    timbreVal = timbre;
    filterTypeAVal = typeA;
    filterTypeBVal = typeB;
    filterMorphVal = filterMorph;
    filterMorphModVal = filterMorphMod;

        filterCutoffVal = filterCutoff;
    filterCutoffModVal = filterCutoffMod;
filterOffsetVal = filterOffset;
    filterResoVal = filterReso;
    filterSlopeVal = filterSlope;
    filterVal = filter;
    spaceVal = space;
    cloudVal = cloud;
    sizeVal = size;
    sweepVal = sweep;
    deSyncVal = desync;
    pitchVal = pitch;
  }

  void updateAlter(float alter) {
    alterVal = alter;
  }

  void updateAdsr(float attack, float decay, float sustain, float release) {
    adsrParams.attack = attack;
    adsrParams.decay = decay;
    adsrParams.sustain = sustain;
    adsrParams.release = release;
    adsr.setParameters(adsrParams);
  }

  void setGlobalSendAccum(float* s0, float* s1, float* s2, float* s3, float* s4, float* s5, float* s6, float* s7) {
    globalSendAccum[0] = s0; globalSendAccum[1] = s1; globalSendAccum[2] = s2; globalSendAccum[3] = s3;
    globalSendAccum[4] = s4; globalSendAccum[5] = s5; globalSendAccum[6] = s6; globalSendAccum[7] = s7;
  }

  void renderNextBlock(juce::AudioBuffer<float> &outputBuffer, int startSample,
                       int numSamples) override {
    bool adsrActive = adsr.isActive();
    
    if (! adsrActive) {
      clearCurrentNote();
      voiceActive = false;
      return;
    }

    currentSampleRate = getSampleRate();
    if (currentSampleRate <= 0.0)
      currentSampleRate = 44100.0;

    // Exponential mapping for filter frequency (50Hz to 12000Hz)
    float currentCutoff = std::clamp(filterCutoffVal + filterVal * filterCutoffModVal, 0.0f, 1.0f);
    float cutoffA_norm = std::clamp(currentCutoff - filterOffsetVal * 0.165f, 0.0f, 1.0f);
    float cutoffB_norm = std::clamp(currentCutoff + filterOffsetVal * 0.165f, 0.0f, 1.0f);
    float fcA = 50.0f * std::pow(2.0f, cutoffA_norm * 8.0f);
    float fcB = 50.0f * std::pow(2.0f, cutoffB_norm * 8.0f);
    // float Q = ...
    // float N = ...

    // Precompute partial amplitudes, frequencies, phase deltas, panning, and build active list
    float freqs[256];
    float targetAmps[256];
    float phaseDeltas[256];
    float pL_block[256];
    float pR_block[256];

    int activePartials[256];
    int numActivePartials = 0;

    currentFundamentalFreq +=
        (fundamentalFreq - currentFundamentalFreq) * 0.06f;

    // Apply MPE timbre slide modifier
    float voiceTimbre =
        std::max(0.0f, std::min(1.0f, timbreVal + localTimbreMod));

    // Lambda for the 10 distinct spectral shapes with a baseline floor
    auto getSpectralShape = [] (int p, int harmonicIndex, int shapeIndex) -> float
    {
        float rawVal = 0.0f;
        switch (shapeIndex)
        {
            case 0: // 1. Warm Triangle/Saw
                rawVal = 1.0f / std::pow ((float)harmonicIndex, 1.3f); break;
            case 1: // 2. Hollow Square (Odd harmonics only)
                rawVal = (p % 2 == 0) ? (1.0f / (float)harmonicIndex) : (0.08f / (float)harmonicIndex); break;
            case 2: // 3. Comb Filter / Phased
                rawVal = (std::sin ((float)p * 0.22f) * 0.4f + 0.6f) / std::sqrt ((float)harmonicIndex); break;
            case 3: // 4. High Fizz (High-pass)
                rawVal = (0.1f + 0.9f * ((float)p / 256.0f)) * (1.0f / std::sqrt ((float)harmonicIndex)); break;
            case 4: // 5. Formant Vocal "Ooh" (Double peaks near H3 & H8)
                rawVal = std::exp (-std::pow ((float)harmonicIndex - 3.0f, 2.0f) / 2.0f)
                     + 0.5f * std::exp (-std::pow ((float)harmonicIndex - 8.0f, 2.0f) / 8.0f)
                     + 0.05f / (float)harmonicIndex; break;
            case 5: // 6. Formant Vocal "Aah" (Double peaks near H6 & H14)
                rawVal = std::exp (-std::pow ((float)harmonicIndex - 6.0f, 2.0f) / 4.0f)
                     + 0.4f * std::exp (-std::pow ((float)harmonicIndex - 14.0f, 2.0f) / 16.0f)
                     + 0.05f / (float)harmonicIndex; break;
            case 6: // 7. Octave Double (Even harmonics dominant)
                rawVal = (p % 2 == 1) ? (1.0f / std::pow ((float)harmonicIndex, 1.2f)) : (0.15f / (float)harmonicIndex); break;
            case 7: // 8. Metallic / Inharmonic (Golden ratio spacing)
                rawVal = (std::sin ((float)p * 1.618f) * 0.4f + 0.6f) / std::pow ((float)harmonicIndex, 0.7f); break;
            case 8: // 9. Resonance Spike (Resonant peak at H12)
                rawVal = (p == 0) ? 1.0f : (0.08f + 0.92f * std::exp (-std::pow ((float)harmonicIndex - 12.0f, 2.0f) / 2.0f)); break;
            case 9: // 10. Grit (Deterministic noise-like hash)
                rawVal = (std::sin ((float)p * 123.456f) * 0.3f + 0.7f) / (float)harmonicIndex; break;
            default:
                rawVal = 0.0f; break;
        }
        
        // Dynamic sheen baseline (adds subtle bright high frequencies without masking the shape)
        float baseline = 0.05f / std::sqrt ((float)harmonicIndex);
        return rawVal * 0.90f + baseline;
    };

    // Morph between shapes based on voiceTimbre
    float scaledTimbre = voiceTimbre * 9.0f;
    int timbreIdx = (int)scaledTimbre;
    float timbreMix = scaledTimbre - (float)timbreIdx;
    if (timbreIdx >= 9) {
      timbreIdx = 8;
      timbreMix = 1.0f;
    }

    float centerHarmonic = sweepVal * 255.0f;
    float sendWidth = 35.0f; // Width of the swept bandpass zone

    float pitchMultiplier = std::pow (2.0f, (pitchVal - 0.5f) * 2.0f);
    float pitchedFundamental = currentFundamentalFreq * pitchMultiplier;

    numActivePartials = 0;
    for (int p = 0; p < 256; ++p) {
      int harmonicIndex = p + 1;

      // Formant / Inharmonic Warp
      float stretch =
          (p == 0) ? 0.0f
                   : (formVal * formVal * 3.5f *
                      std::sin((float)harmonicIndex * 1.57f + (float)p * 0.1f));
      freqs[p] = pitchedFundamental * ((float)harmonicIndex + stretch);
      float syncMultiplier = 1.0f + deSyncVal * 1.0f;
      if (p > 0)
        freqs[p] *= syncMultiplier;

      // Timbre Morphing
      float baseAmp = getSpectralShape (p, harmonicIndex, timbreIdx) * (1.0f - timbreMix)
                    + getSpectralShape (p, harmonicIndex, timbreIdx + 1) * timbreMix;

      // filter limiter
      float multA = calculateFilterMult(freqs[p], fcA, filterResoVal, filterSlopeVal, (int)filterTypeAVal);
      float multB = calculateFilterMult(freqs[p], fcB, filterResoVal, filterSlopeVal, (int)filterTypeBVal);
      float filterMult = multA * (1.0f - filterMorphVal) + multB * filterMorphVal;

      // Space (Organic LFO drift)
      float lfoDrift =
          std::sin((float)voiceTime * 1.2f + phaseDrifts[p]) * spaceVal * 0.3f;

      targetAmps[p] = baseAmp * filterMult * targetAmp * (1.0f + lfoDrift);

      // Precalculate phase delta and panning
      phaseDeltas[p] = freqs[p] / (float)currentSampleRate;
      
      if (p == 0) {
        pL_block[p] = panLeft[p] * (1.0f - spaceVal) + 0.707f * spaceVal;
        pR_block[p] = panRight[p] * (1.0f - spaceVal) + 0.707f * spaceVal;
      } else if (p % 2 == 0) {
        pL_block[p] = panLeft[p] * (1.0f - spaceVal) + 1.0f * spaceVal;
        pR_block[p] = panRight[p] * (1.0f - spaceVal) + 0.0f * spaceVal;
      } else {
        pL_block[p] = panLeft[p] * (1.0f - spaceVal) + 0.0f * spaceVal;
        pR_block[p] = panRight[p] * (1.0f - spaceVal) + 1.0f * spaceVal;
      }

      // Algorithmic send amount based on SWEEP Gaussian bandpass
      float distance = (float)p - centerHarmonic;
      float sendAmp = std::exp(-(distance * distance) / (2.0f * sendWidth * sendWidth));
      p_send_gain[p] = cloudVal * sendAmp * 0.2f;

      // Collect active partials: active if target is audible OR if envelope is still active
      if (targetAmps[p] >= 0.0001f || smoothedAmps[p] >= 0.0001f) {
        activePartials[numActivePartials++] = p;
      }
    }

    // Mix into output buffers
    float scaleFactor = 0.090f; // Level normalization per voice

    for (int s = 0; s < numSamples; ++s) {
      float envVal = adsr.getNextSample();
      float sampleL = 0.0f;
      float sampleR = 0.0f;
      float prevVal = 0.0f;

      // 1. Update master phase first
      bool masterWrapped = false;
      phases[0] += phaseDeltas[0];
      if (phases[0] >= 1.0f) {
        phases[0] -= 1.0f;
        masterWrapped = true;
      }

      for (int i = 0; i < numActivePartials; ++i) {
        int p = activePartials[i];
        
        // Track the dry target amplitude envelope smoothly (15ms time-constant)
        float dry_target = targetAmps[p] * envVal;
        smoothedAmps[p] += (dry_target - smoothedAmps[p]) * 0.15f;
        float a = smoothedAmps[p];

        // 2. Update phase for partial p
        if (p > 0) {
          phases[p] += phaseDeltas[p];
          if (deSyncVal > 0.0f && masterWrapped) {
            phases[p] = 0.0f; // Hard-sync reset!
          } else if (phases[p] >= 1.0f) {
            phases[p] -= 1.0f;
          }
        }

        float modPhase = phases[p];

        if (i > 0) {
          int p_prev = activePartials[i - 1];
          float distance = std::abs (freqs[p] - freqs[p_prev]);
          // Normalize the distance by the fundamental frequency to make it pitch-independent
          float normDistance = distance / pitchedFundamental;
          float modIndex = (alterVal * alterVal * 1.5f * smoothedAmps[p_prev]) / (normDistance + 0.05f);
          if (modIndex > 2.0f) modIndex = 2.0f;
          modPhase += modIndex * prevVal;
        }

        // Sine table lookup with bitwise wrapping
        int idx = static_cast<int>(modPhase * 32768.0f) & 32767;
        float val = sineTable[idx];
        prevVal = val;

        float dryVal = val * a;

        // FDN send routing based on harmonic index p
        int route = 7 - (p / 32);
        if (route < 0) route = 0;
        if (route > 7) route = 7;
        if (globalSendAccum[route] != nullptr) {
          globalSendAccum[route][startSample + s] += dryVal * p_send_gain[p];
        }

        // Blend dry signal output
        float dryMix = 1.0f - cloudVal * 0.3f;
        sampleL += dryVal * dryMix * pL_block[p];
        sampleR += dryVal * dryMix * pR_block[p];
      }

      outputBuffer.addSample(0, startSample + s, sampleL * scaleFactor);
      outputBuffer.addSample(1, startSample + s, sampleR * scaleFactor);

      voiceTime += 1.0 / currentSampleRate;
    }

    if (! adsr.isActive()) {
      clearCurrentNote();
      voiceActive = false;
    }
  }

private:
  juce::ADSR adsr;
  juce::ADSR::Parameters adsrParams;
  bool voiceActive = false;

  double currentSampleRate = 44100.0;
  int noteNumber = -1;
  float targetAmp = 0.0f;
  float fundamentalFreq = 0.0f;
  float currentFundamentalFreq = 0.0f;

  float phases[256];
  float phaseDrifts[256];
  float panLeft[256];
  float panRight[256];

  float smoothedAmps[256];
  float cloudVal = 0.0f;

  float formVal = 0.0f;
  float timbreVal = 0.25f;
  float filterTypeAVal = 0.0f;
  float filterTypeBVal = 0.0f;
  float filterMorphVal = 0.0f;
  float filterMorphModVal = 0.0f;
    float filterCutoffVal = 0.75f;
  float filterCutoffModVal = 0.0f;
float filterOffsetVal = 0.0f;
  float filterResoVal = 0.2f;
  float filterSlopeVal = 0.5f;
  float filterVal = 0.75f;
  float spaceVal = 0.30f;
  float alterVal = 0.0f;
  float sizeVal = 0.5f;
  float sweepVal = 0.5f;
  float deSyncVal = 0.0f;
  float pitchVal = 0.5f;

  float localTimbreMod = 0.0f;
  double voiceTime = 0.0;

  float p_send_gain[256];
  float* globalSendAccum[8] = { nullptr };
};

// ==========================================================================
// 2. Audio Processor Class
// ==========================================================================
class KronosAudioProcessor : public juce::AudioProcessor {
public:
  KronosAudioProcessor();
  ~KronosAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  juce::AudioProcessorValueTreeState apvts;
  std::atomic<bool> activeMidiNotes[128];

  void triggerNoteOnFromEditor(int note, float velocity) {
    synth.handleMidiEvent(juce::MidiMessage::noteOn(1, note, velocity));
  }

  void triggerNoteOffFromEditor(int note) {
    synth.handleMidiEvent(juce::MidiMessage::noteOff(1, note, 0.0f));
  }

private:
  juce::MPESynthesiser synth;

  static constexpr int fdnSize = 8;
  static constexpr int fdnMask = 4095;
  float fdnBuffers[fdnSize][4096];
  int fdnIndices[fdnSize];
  int fdnDelayLengths[fdnSize] = { 997, 1201, 1439, 1753, 2053, 2411, 2851, 3307 };
  
  juce::AudioBuffer<float> sendBuffers;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KronosAudioProcessor)
};

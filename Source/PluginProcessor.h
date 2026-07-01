#pragma once

#include <JuceHeader.h>

extern float sineTable[32768];

// ==========================================================================
// 1. Synthesiser Voice Class
// ==========================================================================
class KronosVoice : public juce::MPESynthesiserVoice {
public:
  KronosVoice() {
    histWriteIdx = 0;
    lastEnvVal = 0.0f;
    for (int p = 0; p < 256; ++p) {
      phases[p] = 0.0f;
      phaseDrifts[p] = juce::Random::getSystemRandom().nextFloat() *
                       juce::MathConstants<float>::twoPi;
      float basePan = (p == 0) ? 0.5f : ((p % 2 == 0) ? 0.25f : 0.75f);
      panLeft[p] = std::sqrt(1.0f - basePan);
      panRight[p] = std::sqrt(basePan);

      // Inharmonic golden-ratio scattered delays (upward sweep)
      float scatter = std::sin((float)p * 1.618f) * 0.15f;
      p_delay_scale[p] = std::max(0.0f, std::min(1.0f, (float)p / 255.0f + scatter));

      for (int h = 0; h < envHistSize; ++h) {
        envHistory[p][h] = 0.0f;
      }
      delayedWetAmps[p] = 0.0f;
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
      delayedWetAmps[p] = 0.0f;
      for (int h = 0; h < envHistSize; ++h) {
        envHistory[p][h] = 0.0f;
      }
    }
    histWriteIdx = 0;
    lastEnvVal = 0.0f;

    targetAmp = currentlyPlayingNote.noteOnVelocity.asUnsignedFloat();
    voiceActive = true;
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

  void updateParams(float detune, float timbre, float cutoff, float space, float cloud) {
    detuneVal = detune;
    timbreVal = timbre;
    cutoffVal = cutoff;
    spaceVal = space;
    cloudVal = cloud;
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

  void renderNextBlock(juce::AudioBuffer<float> &outputBuffer, int startSample,
                       int numSamples) override {
    bool adsrActive = adsr.isActive();
    
    // Check if both ADSR is inactive and the reverb tail has fully decayed
    if (! adsrActive) {
      float maxAmp = 0.0f;
      for (int p = 0; p < 256; ++p) {
        if (smoothedAmps[p] > maxAmp)
          maxAmp = smoothedAmps[p];
      }
      if (maxAmp <= 0.001f) {
        clearCurrentNote();
        voiceActive = false;
        return;
      }
    }

    currentSampleRate = getSampleRate();
    if (currentSampleRate <= 0.0)
      currentSampleRate = 44100.0;

    // Exponential mapping for cutoff frequency (50Hz to 12000Hz)
    float cutoffFreq = 50.0f * std::pow(2.0f, cutoffVal * 8.0f);

    // Precompute partial amplitudes, frequencies, phase deltas, panning, and build active list
    float freqs[256];
    float targetAmps[256];
    float phaseDeltas[256];
    float pL_block[256];
    float pR_block[256];
    float alpha_block[256];

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

    // CLOUD decay time in seconds (0.5s at min, up to 15.0s at max)
    float decayTimeSeconds = 0.5f + cloudVal * cloudVal * 14.5f;

    // Update circular history at block rate using envelope value of the last block
    histWriteIdx = (histWriteIdx + 1) % envHistSize;

    for (int p = 0; p < 256; ++p) {
      int harmonicIndex = p + 1;

      // Detune / Inharmonic Warp
      float stretch =
          (p == 0) ? 0.0f
                   : (detuneVal * detuneVal * 3.5f *
                      std::sin((float)harmonicIndex * 1.57f + (float)p * 0.1f));
      freqs[p] = currentFundamentalFreq * ((float)harmonicIndex + stretch);

      // Timbre Morphing
      float baseAmp = getSpectralShape (p, harmonicIndex, timbreIdx) * (1.0f - timbreMix)
                    + getSpectralShape (p, harmonicIndex, timbreIdx + 1) * timbreMix;

      // Cutoff limiter
      float ratio = freqs[p] / cutoffFreq;
      float filterMult = 1.0f / (1.0f + std::pow(ratio, 6.0f));

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

      // Reverb time-constant (alpha) per partial (sample-rate independent)
      float alpha = 1.0f / (currentSampleRate * decayTimeSeconds);
      alpha_block[p] = (alpha > 1.0f) ? 1.0f : alpha;

      // Write target amplitude modulated by last envelope to history
      envHistory[p][histWriteIdx] = targetAmps[p] * lastEnvVal;

      // Query delayed wet amplitude from history (block rate query)
      int delayBlocks = static_cast<int>(p_delay_scale[p] * cloudVal * (envHistSize - 2));
      int readIdx = (histWriteIdx - delayBlocks + envHistSize) % envHistSize;
      delayedWetAmps[p] = envHistory[p][readIdx];

      // Collect active partials: active if target is audible OR if reverb tail is still ringing
      float maxExpectedAmp = std::max(targetAmps[p] * lastEnvVal, delayedWetAmps[p]);
      if (maxExpectedAmp >= 0.0001f || smoothedAmps[p] >= 0.0001f) {
        activePartials[numActivePartials++] = p;
      }
    }

    // Mix into output buffers
    float scaleFactor = 0.045f; // Level normalization per voice
    float envVal = 0.0f;

    for (int s = 0; s < numSamples; ++s) {
      envVal = adsr.getNextSample();
      float sampleL = 0.0f;
      float sampleR = 0.0f;
      float prevVal = 0.0f;

      for (int i = 0; i < numActivePartials; ++i) {
        int p = activePartials[i];
        float dry_target = targetAmps[p] * envVal;
        float wet_target = delayedWetAmps[p];
        
        // Blend dry and wet targets based on cloudVal
        float target_a = dry_target * (1.0f - cloudVal * 0.5f) + wet_target * (cloudVal * 1.2f);

        // Apply decay smoothing conditionally: instant tracking on attack, slow tracking on decay
        float alpha = 1.0f;
        if (target_a < smoothedAmps[p]) {
          alpha = alpha_block[p];
        }
        smoothedAmps[p] += (target_a - smoothedAmps[p]) * alpha;
        float a = smoothedAmps[p];

        phases[p] += phaseDeltas[p];
        if (phases[p] >= 1.0f)
          phases[p] -= 1.0f;

        float modPhase = phases[p];
        if (i > 0) {
          int p_prev = activePartials[i - 1];
          float distance = std::abs (freqs[p] - freqs[p_prev]);
          // Normalize the distance by the fundamental frequency to make it pitch-independent
          float normDistance = distance / currentFundamentalFreq;
          float modIndex = (alterVal * alterVal * 1.5f * smoothedAmps[p_prev]) / (normDistance + 0.05f);
          if (modIndex > 2.0f) modIndex = 2.0f;
          modPhase += modIndex * prevVal;
        }

        // Sine table lookup with bitwise wrapping
        int idx = static_cast<int>(modPhase * 32768.0f) & 32767;
        float val = sineTable[idx];
        prevVal = val;

        sampleL += val * a * pL_block[p];
        sampleR += val * a * pR_block[p];
      }

      outputBuffer.addSample(0, startSample + s, sampleL * scaleFactor);
      outputBuffer.addSample(1, startSample + s, sampleR * scaleFactor);

      voiceTime += 1.0 / currentSampleRate;
    }

    lastEnvVal = envVal;

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

  float detuneVal = 0.0f;
  float timbreVal = 0.25f;
  float cutoffVal = 0.75f;
  float spaceVal = 0.30f;
  float alterVal = 0.0f;

  float localTimbreMod = 0.0f;
  double voiceTime = 0.0;

  static constexpr int envHistSize = 128;
  float envHistory[256][envHistSize];
  int histWriteIdx = 0;
  float p_delay_scale[256];
  float delayedWetAmps[256];
  float lastEnvVal = 0.0f;
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

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KronosAudioProcessor)
};

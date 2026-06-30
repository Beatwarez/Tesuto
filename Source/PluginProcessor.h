#pragma once

#include <JuceHeader.h>

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
    }
  }

  void noteStarted() override {
    noteNumber = currentlyPlayingNote.initialNote;
    float targetFreq = (float)currentlyPlayingNote.getFrequencyInHertz();

    fundamentalFreq = targetFreq;
    currentFundamentalFreq = targetFreq;
    for (int p = 0; p < 256; ++p)
      phases[p] = 0.0f;

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

  void updateParams(float detune, float timbre, float cutoff, float space) {
    detuneVal = detune;
    timbreVal = timbre;
    cutoffVal = cutoff;
    spaceVal = space;
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
    if (! adsr.isActive()) {
      clearCurrentNote();
      voiceActive = false;
      return;
    }

    currentSampleRate = getSampleRate();
    if (currentSampleRate <= 0.0)
      currentSampleRate = 44100.0;

    // Exponential mapping for cutoff frequency (50Hz to 12000Hz)
    float cutoffFreq = 50.0f * std::pow(2.0f, cutoffVal * 8.0f);

    // Precompute partial amplitudes and frequencies
    float freqs[256];
    float amps[256];

    currentFundamentalFreq +=
        (fundamentalFreq - currentFundamentalFreq) * 0.06f;

    // Apply MPE timbre slide modifier
    float voiceTimbre =
        std::max(0.0f, std::min(1.0f, timbreVal + localTimbreMod));

    // Lambda for the 10 distinct spectral shapes
    auto getSpectralShape = [] (int p, int harmonicIndex, int shapeIndex) -> float
    {
        switch (shapeIndex)
        {
            case 0: // 1. Warm Triangle/Saw
                return 1.0f / std::pow ((float)harmonicIndex, 1.5f);
                
            case 1: // 2. Hollow Square (Odd harmonics only)
                return (p % 2 == 0) ? (1.0f / (float)harmonicIndex) : 0.0f;
                
            case 2: // 3. Comb Filter / Phased
                return (std::sin ((float)p * 0.22f) * 0.5f + 0.5f) / std::sqrt ((float)harmonicIndex);
                
            case 3: // 4. High Fizz (High-pass)
                return ((float)p / 256.0f) * (1.0f / std::sqrt ((float)harmonicIndex));
                
            case 4: // 5. Formant Vocal "Ooh" (Double peaks near H3 & H8)
                return std::exp (-std::pow ((float)harmonicIndex - 3.0f, 2.0f) / 2.0f)
                     + 0.5f * std::exp (-std::pow ((float)harmonicIndex - 8.0f, 2.0f) / 8.0f);
                     
            case 5: // 6. Formant Vocal "Aah" (Double peaks near H6 & H14)
                return std::exp (-std::pow ((float)harmonicIndex - 6.0f, 2.0f) / 4.0f)
                     + 0.4f * std::exp (-std::pow ((float)harmonicIndex - 14.0f, 2.0f) / 16.0f);
                     
            case 6: // 7. Octave Double (Even harmonics dominant)
                return (p % 2 == 1) ? (1.0f / std::pow ((float)harmonicIndex, 1.2f)) : (0.1f / (float)harmonicIndex);
                
            case 7: // 8. Metallic / Inharmonic (Golden ratio spacing)
                return (std::sin ((float)p * 1.618f) * 0.5f + 0.5f) / std::pow ((float)harmonicIndex, 0.8f);
                
            case 8: // 9. Resonance Spike (Resonant peak at H12)
                return (p == 0) ? 1.0f : (0.05f + 0.95f * std::exp (-std::pow ((float)harmonicIndex - 12.0f, 2.0f) / 2.0f));
                
            case 9: // 10. Grit (Deterministic noise-like hash)
                return (std::sin ((float)p * 123.456f) * 0.5f + 0.5f) / (float)harmonicIndex;
                
            default:
                return 0.0f;
        }
    };

    // Morph between shapes based on voiceTimbre
    float scaledTimbre = voiceTimbre * 9.0f;
    int timbreIdx = (int)scaledTimbre;
    float timbreMix = scaledTimbre - (float)timbreIdx;
    if (timbreIdx >= 9) {
      timbreIdx = 8;
      timbreMix = 1.0f;
    }

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

      amps[p] = baseAmp * filterMult * targetAmp * (1.0f + lfoDrift);
    }

    // Mix into output buffers
    float scaleFactor = 0.045f; // Level normalization per voice

    for (int s = 0; s < numSamples; ++s) {
      float envVal = adsr.getNextSample();
      float sampleL = 0.0f;
      float sampleR = 0.0f;

      for (int p = 0; p < 256; ++p) {
        float f = freqs[p];
        float a = amps[p] * envVal;

        phases[p] += f / (float)currentSampleRate;
        if (phases[p] >= 1.0f)
          phases[p] -= 1.0f;

        if (a < 0.0001f)
          continue;

        float val = std::sin(phases[p] * juce::MathConstants<float>::twoPi);

        float pL, pR;
        if (p == 0) {
          pL = panLeft[p] * (1.0f - spaceVal) + 0.707f * spaceVal;
          pR = panRight[p] * (1.0f - spaceVal) + 0.707f * spaceVal;
        } else if (p % 2 == 0) {
          pL = panLeft[p] * (1.0f - spaceVal) + 1.0f * spaceVal;
          pR = panRight[p] * (1.0f - spaceVal) + 0.0f * spaceVal;
        } else {
          pL = panLeft[p] * (1.0f - spaceVal) + 0.0f * spaceVal;
          pR = panRight[p] * (1.0f - spaceVal) + 1.0f * spaceVal;
        }

        sampleL += val * a * pL;
        sampleR += val * a * pR;
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

  float detuneVal = 0.0f;
  float timbreVal = 0.25f;
  float cutoffVal = 0.75f;
  float spaceVal = 0.30f;
  float alterVal = 0.0f;

  float localTimbreMod = 0.0f;
  double voiceTime = 0.0;
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

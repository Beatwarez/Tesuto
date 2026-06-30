#pragma once

#include <JuceHeader.h>

// ==========================================================================
// 1. Synthesiser Voice Class
// ==========================================================================
class KronosVoice : public juce::MPESynthesiserVoice {
public:
  KronosVoice() {
    for (int p = 0; p < 256; ++p) {
      phases[p] = juce::Random::getSystemRandom().nextFloat();
      phaseDrifts[p] = juce::Random::getSystemRandom().nextFloat() *
                       juce::MathConstants<float>::twoPi;
      float basePan = (p % 2 == 0) ? 0.25f : 0.75f;
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
      phases[p] = juce::Random::getSystemRandom().nextFloat();

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

    for (int p = 0; p < 256; ++p) {
      int harmonicIndex = p + 1;

      // Detune / Inharmonic Warp
      float stretch =
          (p == 0) ? 0.0f
                   : (detuneVal * detuneVal * 3.5f *
                      std::sin((float)harmonicIndex * 1.57f + (float)p * 0.1f));
      freqs[p] = currentFundamentalFreq * ((float)harmonicIndex + stretch);

      // Timbre Morph (modulated by voiceTimbre)
      float baseAmp = 0.0f;
      if (voiceTimbre < 0.5f) {
        float mix = voiceTimbre * 2.0f;
        float ampA = 1.0f / std::pow((float)harmonicIndex, 1.2f);
        float ampB = (std::sin((float)p * 0.22f) * 0.5f + 0.5f) /
                     std::sqrt((float)harmonicIndex);
        baseAmp = ampA * (1.0f - mix) + ampB * mix;
      } else {
        float mix = (voiceTimbre - 0.5f) * 2.0f;
        float ampB = (std::sin((float)p * 0.22f) * 0.5f + 0.5f) /
                     std::sqrt((float)harmonicIndex);
        float ampC = (1.0f - ((float)p / 256.0f)) * 0.5f;
        baseAmp = ampB * (1.0f - mix) + ampC * mix;
      }

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
        if (a < 0.0001f)
          continue;

        phases[p] += f / (float)currentSampleRate;
        if (phases[p] >= 1.0f)
          phases[p] -= 1.0f;

        float val = std::sin(phases[p] * juce::MathConstants<float>::twoPi);

        float pL = panLeft[p] * (1.0f - spaceVal) +
                   (p % 2 == 0 ? 1.0f : 0.0f) * spaceVal;
        float pR = panRight[p] * (1.0f - spaceVal) +
                   (p % 2 == 1 ? 1.0f : 0.0f) * spaceVal;

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

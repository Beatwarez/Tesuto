#pragma once

#include <JuceHeader.h>

extern float sineTable[32768];

class KronosAudioProcessor;

// ==========================================================================
// 1. Synthesiser Voice Class
// ==========================================================================
class KronosVoice : public juce::MPESynthesiserVoice {
public:
  KronosAudioProcessor* processor = nullptr;

  KronosVoice(KronosAudioProcessor* proc) : processor(proc) {
    for (int p = 0; p < 512; ++p) {
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
    for (int p = 0; p < 512; ++p) {
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
                       int numSamples) override;

private:
  juce::ADSR adsr;
  juce::ADSR::Parameters adsrParams;
  bool voiceActive = false;

  double currentSampleRate = 44100.0;
  int noteNumber = -1;
  float targetAmp = 0.0f;
  float fundamentalFreq = 0.0f;
  float currentFundamentalFreq = 0.0f;

  float phases[512];
  float phaseDrifts[512];
  float panLeft[512];
  float panRight[512];

  float smoothedAmps[512];
  float cloudVal = 0.0f;
  float localTimbreMod = 0.0f;
  double voiceTime = 0.0;

  float p_send_gain[512];
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

  // Generic Parameter Matrix Pointers
  std::atomic<float>* mod1_macro = nullptr;
  std::atomic<float>* mod1_p[8] = { nullptr };

  std::atomic<float>* mod_engine[7] = { nullptr };
  std::atomic<float>* mod_macro[7] = { nullptr };
  std::atomic<float>* mod_p[7][8] = { {nullptr} };
  std::atomic<float>* mod_pMod[7][8] = { {nullptr} };

  std::atomic<float>* attack = nullptr;
  std::atomic<float>* decay = nullptr;
  std::atomic<float>* sustain = nullptr;
  std::atomic<float>* release = nullptr;

  std::atomic<int> routingOrder[7];
  
  void updateRoutingOrder(const juce::String& routingStr) {
      juce::StringArray tokens;
      tokens.addTokens(routingStr, ",", "\"");
      for (int i = 0; i < 7 && i < tokens.size(); ++i) {
          routingOrder[i].store(tokens[i].getIntValue());
      }
  }

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

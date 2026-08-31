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

  KronosVoice(KronosAudioProcessor* p) : processor(p) {
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
    
    if (! adsrActive) {
      clearCurrentNote();
      voiceActive = false;
      return;
    }

    currentSampleRate = getSampleRate();
    if (currentSampleRate <= 0.0)
      currentSampleRate = 44100.0;

    currentFundamentalFreq += (fundamentalFreq - currentFundamentalFreq) * 0.06f;

    // --- 1. Source Engine (Lane 1) ---
    float partials_param = processor->mod1_p[0] ? processor->mod1_p[0]->load() : 0.5f;
    float balance_param  = processor->mod1_p[1] ? processor->mod1_p[1]->load() : 0.0f;
    float width_param    = processor->mod1_p[2] ? processor->mod1_p[2]->load() : 0.0f;

    int targetPartials = (int)(partials_param * 511.0f) + 1;
    float maxHarmonics = (currentSampleRate / 2.0f) / currentFundamentalFreq;
    if (maxHarmonics < 1.0f) maxHarmonics = 1.0f;
    
    float spacing = 1.0f;
    if (width_param > 0.0f) {
        float maxWidthSpacing = maxHarmonics / (float)targetPartials;
        spacing = 1.0f + width_param * (maxWidthSpacing - 1.0f);
    } else if (width_param < 0.0f) {
        spacing = 1.0f + width_param * 0.95f; // shrinks to 0.05
    }

    float totalClusterSpan = (float)targetPartials * spacing;
    float clusterStart = 1.0f;
    if (balance_param > 0.0f) {
        float maxStart = maxHarmonics - totalClusterSpan;
        if (maxStart < 1.0f) maxStart = 1.0f;
        clusterStart = 1.0f + balance_param * (maxStart - 1.0f);
    } else if (balance_param < 0.0f) {
        clusterStart = 1.0f; // Could be modified for sub harmonics later
    }

    float freqs[512];
    float targetAmps[512];
    float phaseDeltas[512];
    float pL_block[512];
    float pR_block[512];

    for (int p = 0; p < 512; ++p) {
        if (p < targetPartials) {
            float virtualHarmonicIndex = clusterStart + (float)p * spacing;
            freqs[p] = currentFundamentalFreq * virtualHarmonicIndex;
            
            if (freqs[p] >= currentSampleRate * 0.49f) {
                targetAmps[p] = 0.0f;
            } else {
                targetAmps[p] = targetAmp * (1.0f / std::sqrt(virtualHarmonicIndex + 1.0f));
            }
        } else {
            freqs[p] = 0.0f;
            targetAmps[p] = 0.0f;
        }
    }

    // --- 2. Dynamic Serial Router (Lanes 2-8) ---
    float cloudVal = 0.0f;
    float deSyncVal = 0.0f;
    float alterVal = 0.0f;

    for (int i = 0; i < 7; ++i) {
        int laneNumber = processor->routingOrder[i].load();
        if (laneNumber < 2 || laneNumber > 8) continue;
        
        int laneIdx = laneNumber - 2;
        int engineType = processor->mod_engine[laneIdx] ? processor->mod_engine[laneIdx]->load() : 0;
        float macroVal = processor->mod_macro[laneIdx] ? processor->mod_macro[laneIdx]->load() : 0.0f;
        
        if (engineType == 2) { // FILTER
            float cutoff_param = processor->mod_p[laneIdx][0] ? processor->mod_p[laneIdx][0]->load() : 0.5f;
            float cutoff_mod   = processor->mod_pMod[laneIdx][0] ? processor->mod_pMod[laneIdx][0]->load() : 0.0f;
            float offset_param = processor->mod_p[laneIdx][1] ? processor->mod_p[laneIdx][1]->load() : 0.0f;
            float offset_mod   = processor->mod_pMod[laneIdx][1] ? processor->mod_pMod[laneIdx][1]->load() : 0.0f;
            float reso_param   = processor->mod_p[laneIdx][2] ? processor->mod_p[laneIdx][2]->load() : 0.2f;
            float reso_mod     = processor->mod_pMod[laneIdx][2] ? processor->mod_pMod[laneIdx][2]->load() : 0.0f;
            float slope_param  = processor->mod_p[laneIdx][3] ? processor->mod_p[laneIdx][3]->load() : 0.5f;
            float slope_mod    = processor->mod_pMod[laneIdx][3] ? processor->mod_pMod[laneIdx][3]->load() : 0.0f;
            float morph_param  = processor->mod_p[laneIdx][4] ? processor->mod_p[laneIdx][4]->load() : 0.0f;
            float morph_mod    = processor->mod_pMod[laneIdx][4] ? processor->mod_pMod[laneIdx][4]->load() : 0.0f;
            
            int typeA = processor->mod_p[laneIdx][5] ? (int)processor->mod_p[laneIdx][5]->load() : 0;
            int typeB = processor->mod_p[laneIdx][6] ? (int)processor->mod_p[laneIdx][6]->load() : 0;

            float currentCutoff = std::clamp(cutoff_param + macroVal * cutoff_mod, 0.0f, 1.0f);
            float currentOffset = std::clamp(offset_param + macroVal * offset_mod, -1.0f, 1.0f);
            float cutoffA_norm = std::clamp(currentCutoff - currentOffset * 0.165f, 0.0f, 1.0f);
            float cutoffB_norm = std::clamp(currentCutoff + currentOffset * 0.165f, 0.0f, 1.0f);
            float fcA = 50.0f * std::pow(2.0f, cutoffA_norm * 8.0f);
            float fcB = 50.0f * std::pow(2.0f, cutoffB_norm * 8.0f);
            float currentReso = std::clamp(reso_param + macroVal * reso_mod, 0.0f, 1.0f);
            float currentSlope = std::clamp(slope_param + macroVal * slope_mod, 0.0f, 1.0f);
            float currentMorph = std::clamp(morph_param + macroVal * morph_mod, 0.0f, 1.0f);

            for (int p = 0; p < targetPartials; ++p) {
                if (targetAmps[p] > 0.0f) {
                    float multA = calculateFilterMult(freqs[p], fcA, currentReso, currentSlope, typeA);
                    float multB = calculateFilterMult(freqs[p], fcB, currentReso, currentSlope, typeB);
                    float filterMult = multA * (1.0f - currentMorph) + multB * currentMorph;
                    if (filterMult > 1.0f) filterMult = 1.0f;
                    targetAmps[p] *= filterMult;
                }
            }
        }
    }

    // --- 3. Finalization (Panning & Phase Deltas) ---
    int activePartials[512];
    int numActivePartials = 0;

    for (int p = 0; p < 512; ++p) {
        if (p < targetPartials && targetAmps[p] > 0.0f) {
            phaseDeltas[p] = freqs[p] / (float)currentSampleRate;
            pL_block[p] = panLeft[p]; 
            pR_block[p] = panRight[p];
        } else {
            phaseDeltas[p] = 0.0f;
            pL_block[p] = 0.0f;
            pR_block[p] = 0.0f;
        }

        // Send logic
        p_send_gain[p] = 0.0f;

        // Collect active partials
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

#include "PluginProcessor.h"
#include "PluginEditor.h"

float sineTable[32768];

// ==========================================================================
// Constructor & Destructor
// ==========================================================================
static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // Routing Order
    // Routing Order is stored directly in value tree or handled outside APVTS since AudioParameterString doesn't exist in base JUCE
    
    // Modulator 1 (Source)
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_macro", 1), "mod1_macro", 0.0f, 1.0f, 0.5f));
    for (int p = 1; p <= 8; ++p) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_p" + juce::String(p), 1), "mod1_p" + juce::String(p), 0.0f, 1.0f, 0.0f));
    }
    
    // Modulators 2-8
    for (int m = 2; m <= 8; ++m) {
        juce::String mStr = juce::String(m);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_engine", 1), "mod" + mStr + "_engine", 0.0f, 10.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_macro", 1), "mod" + mStr + "_macro", 0.0f, 1.0f, 0.5f));
        for (int p = 1; p <= 8; ++p) {
            juce::String pStr = juce::String(p);
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_p" + pStr, 1), "mod" + mStr + "_p" + pStr, 0.0f, 1.0f, 0.0f));
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_p" + pStr + "Mod", 1), "mod" + mStr + "_p" + pStr + "Mod", -1.0f, 1.0f, 0.0f));
        }
    }
    
    // Amp Envelope
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("attack", 1), "Attack", juce::NormalisableRange<float>(0.01f, 5.0f, 0.01f, 0.35f), 0.20f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("decay", 1), "Decay", juce::NormalisableRange<float>(0.01f, 5.0f, 0.01f, 0.35f), 0.30f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("sustain", 1), "Sustain", 0.0f, 1.0f, 0.80f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("release", 1), "Release", juce::NormalisableRange<float>(0.01f, 8.0f, 0.01f, 0.35f), 1.00f));
    
    return layout;
}

KronosAudioProcessor::KronosAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
        apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (int i = 0; i < 32768; ++i)
        sineTable[i] = std::sin (((float)i / 32768.0f) * juce::MathConstants<float>::twoPi);

    for (int i = 0; i < 128; ++i)
        activeMidiNotes[i] = false;

    // Reset FDN reverb buffers
    for (int i = 0; i < fdnSize; ++i) {
        for (int d = 0; d < 4096; ++d) {
            fdnBuffers[i][d] = 0.0f;
        }
        fdnIndices[i] = 0;
    }

    // Initialize Generic Parameter Pointers
    mod1_macro = apvts.getRawParameterValue("mod1_macro");
    for (int p = 1; p <= 8; ++p) {
        mod1_p[p-1] = apvts.getRawParameterValue("mod1_p" + juce::String(p));
    }
    
    for (int m = 2; m <= 8; ++m) {
        juce::String mStr = juce::String(m);
        mod_engine[m-2] = apvts.getRawParameterValue("mod" + mStr + "_engine");
        mod_macro[m-2] = apvts.getRawParameterValue("mod" + mStr + "_macro");
        for (int p = 1; p <= 8; ++p) {
            juce::String pStr = juce::String(p);
            mod_p[m-2][p-1] = apvts.getRawParameterValue("mod" + mStr + "_p" + pStr);
            mod_pMod[m-2][p-1] = apvts.getRawParameterValue("mod" + mStr + "_p" + pStr + "Mod");
        }
    }
    
    attack = apvts.getRawParameterValue("attack");
    decay = apvts.getRawParameterValue("decay");
    sustain = apvts.getRawParameterValue("sustain");
    release = apvts.getRawParameterValue("release");
    
    // Default routing order
    for (int i = 0; i < 7; ++i) routingOrder[i].store(i + 2);

    synth.clearVoices();
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new KronosVoice(this));
        
    apvts.state = juce::ValueTree ("KronosParams");
}

KronosAudioProcessor::~KronosAudioProcessor()
{
}

// ==========================================================================
// Basic Properties
// ==========================================================================
const juce::String KronosAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool KronosAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool KronosAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool KronosAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double KronosAudioProcessor::getTailLengthSeconds() const
{
    return 1.5;
}

int KronosAudioProcessor::getNumPrograms()
{
    return 1;
}

int KronosAudioProcessor::getCurrentProgram()
{
    return 0;
}

void KronosAudioProcessor::setCurrentProgram (int)
{
}

const juce::String KronosAudioProcessor::getProgramName (int)
{
    return {};
}

void KronosAudioProcessor::changeProgramName (int, const juce::String&)
{
}

// ==========================================================================
// Lifecycle Methods
// ==========================================================================
void KronosAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    
    // Reset FDN reverb buffers on sample rate changes
    for (int i = 0; i < fdnSize; ++i) {
        for (int d = 0; d < 4096; ++d) {
            fdnBuffers[i][d] = 0.0f;
        }
        fdnIndices[i] = 0;
    }
}

void KronosAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool KronosAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

// ==========================================================================
// Process Block
// ==========================================================================
void KronosAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Update voice parameters from host control values
    int numSamples = buffer.getNumSamples();
    sendBuffers.setSize (8, numSamples, false, true, true);
    sendBuffers.clear();

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<KronosVoice*> (synth.getVoice (i)))
        {
            if (attack && decay && sustain && release) {
                voice->updateAdsr (attack->load(), decay->load(), sustain->load(), release->load());
            }
            voice->setGlobalSendAccum(
                sendBuffers.getWritePointer(0),
                sendBuffers.getWritePointer(1),
                sendBuffers.getWritePointer(2),
                sendBuffers.getWritePointer(3),
                sendBuffers.getWritePointer(4),
                sendBuffers.getWritePointer(5),
                sendBuffers.getWritePointer(6),
                sendBuffers.getWritePointer(7)
            );
        }
    }

    // Track active midi notes for visualizer feedback and handle parameter CC modulations
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            activeMidiNotes[msg.getNoteNumber()] = true;
        else if (msg.isNoteOff())
            activeMidiNotes[msg.getNoteNumber()] = false;
        else if (msg.isAllNotesOff())
        {
            for (int i = 0; i < 128; ++i)
                activeMidiNotes[i] = false;
        }
        // Removed MIDI CC handling per user request
    }

    // Render Synth voices
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

    // Process 8-Channel Global FDN Reverb
    float size = 0.5f;
    float decayTimeSeconds = 0.1f + size * size * 5.9f;
    float decayAlpha = -6.91f / (decayTimeSeconds * getSampleRate());
    float fdnGains[8];
    for (int i = 0; i < 8; ++i) {
        fdnGains[i] = std::max(0.0f, std::min(0.98f, std::exp(decayAlpha * fdnDelayLengths[i])));
    }

    auto* mainL = buffer.getWritePointer(0);
    auto* mainR = buffer.getWritePointer(1);

    for (int s = 0; s < numSamples; ++s) {
        float outputs[8];
        // 1. Read delay line outputs
        for (int i = 0; i < 8; ++i) {
            int readIdx = (fdnIndices[i] - fdnDelayLengths[i]) & fdnMask;
            outputs[i] = fdnBuffers[i][readIdx];
        }

        // 2. Householder mixing matrix multiplication (lossless unitary diffusion)
        float sum = 0.0f;
        for (int i = 0; i < 8; ++i) sum += outputs[i];
        float mixTerm = 0.25f * sum; // 2 / N = 2 / 8 = 0.25

        float inputs[8];
        for (int i = 0; i < 8; ++i) {
            inputs[i] = outputs[i] - mixTerm;
        }

        // 3. Write feedback + input send to delay lines
        for (int i = 0; i < 8; ++i) {
            float sendIn = sendBuffers.getSample(i, s);
            fdnBuffers[i][fdnIndices[i]] = sendIn + inputs[i] * fdnGains[i];
            fdnIndices[i] = (fdnIndices[i] + 1) & fdnMask;
        }

        // 4. Mix outputs to stereo: odd/even split
        float wetL = (outputs[0] + outputs[2] + outputs[4] + outputs[6]) * 0.35f;
        float wetR = (outputs[1] + outputs[3] + outputs[5] + outputs[7]) * 0.35f;

        mainL[s] += wetL;
        mainR[s] += wetR;
    }

    // Master Saturation (tanh) to prevent clipping
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = std::tanh (channelData[sample]);
        }
    }
}

// ==========================================================================
// Editor Methods
// ==========================================================================
bool KronosAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* KronosAudioProcessor::createEditor()
{
    return new KronosAudioProcessorEditor (*this);
}

// ==========================================================================
// State Presets Persistence
// ==========================================================================
void KronosAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void KronosAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

// ==========================================================================
// JUCE Creator function
// ==========================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KronosAudioProcessor();
}

// ==========================================================================
// KronosVoice Implementation
// ==========================================================================

void KronosVoice::renderNextBlock(juce::AudioBuffer<float> &outputBuffer, int startSample, int numSamples) {
    if (! adsr.isActive()) {
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
    float maxHarmonics = ((float)currentSampleRate / 2.0f) / currentFundamentalFreq;
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
    float localCloudVal = 0.0f;
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
          float normDistance = distance / currentFundamentalFreq;
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
        float dryMix = 1.0f - localCloudVal * 0.3f;
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


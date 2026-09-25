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
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_macro", 1), "mod1_macro", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_p1", 1), "mod1_p1", juce::NormalisableRange<float>(1.0f, 256.0f, 1.0f, 1.0f), 256.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_p1_mod", 1), "mod1_p1_mod", -1.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_p2", 1), "mod1_p2", -1.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_p2_mod", 1), "mod1_p2_mod", -1.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_p3", 1), "mod1_p3", -1.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_p3_mod", 1), "mod1_p3_mod", -1.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_p4", 1), "mod1_p4", -36.0f, 36.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_p4_mod", 1), "mod1_p4_mod", -1.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_shape", 1), "mod1_shape", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_shape_mod", 1), "mod1_shape_mod", -1.0f, 1.0f, 0.0f));
    for (int p = 6; p <= 8; ++p) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_p" + juce::String(p), 1), "mod1_p" + juce::String(p), -1.0f, 1.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod1_p" + juce::String(p) + "_mod", 1), "mod1_p" + juce::String(p) + "_mod", -1.0f, 1.0f, 0.0f));
    }
    
    // Modulators 2-8
    for (int m = 2; m <= 8; ++m) {
        juce::String mStr = juce::String(m);
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_engine", 1), "mod" + mStr + "_engine", 0.0f, 10.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_macro", 1), "mod" + mStr + "_macro", 0.0f, 1.0f, 0.0f));
        juce::StringArray filterTypes = { "LP", "HP", "BP", "NOTCH", "COMB", "VOWEL" };
        for (int p = 1; p <= 8; ++p) {
            juce::String pStr = juce::String(p);
            if (p == 6) {
                layout.add(std::make_unique<juce::AudioParameterInt>(
                    juce::ParameterID("mod" + mStr + "_filterA", 1), 
                    "mod" + mStr + "_filterA", 
                    0, 5, 0
                ));
                layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filterA_mod", 1), "mod" + mStr + "_filterA_mod", -1.0f, 1.0f, 0.0f));
            } else if (p == 7) {
                layout.add(std::make_unique<juce::AudioParameterInt>(
                    juce::ParameterID("mod" + mStr + "_filterB", 1), 
                    "mod" + mStr + "_filterB", 
                    0, 5, 0
                ));
                layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_filterB_mod", 1), "mod" + mStr + "_filterB_mod", -1.0f, 1.0f, 0.0f));
            } else {
                layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_p" + pStr, 1), "mod" + mStr + "_p" + pStr, -512.0f, 512.0f, 0.0f));
                layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mod" + mStr + "_p" + pStr + "_mod", 1), "mod" + mStr + "_p" + pStr + "_mod", -1.0f, 1.0f, 0.0f));
            }
        }
    }
    
    // Amp Envelope
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("attack", 1), "Attack", juce::NormalisableRange<float>(0.001f, 7.0f, 0.001f, 0.35f), 0.003f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("decay", 1), "Decay", juce::NormalisableRange<float>(0.001f, 7.0f, 0.001f, 0.35f), 0.500f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("sustain", 1), "Sustain", 0.0f, 1.0f, 0.80f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("release", 1), "Release", juce::NormalisableRange<float>(0.001f, 7.0f, 0.001f, 0.35f), 0.500f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("drift", 1), "Drift", 0.0f, 1.0f, 0.0f));
    
    
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
    mod1_shape = apvts.getRawParameterValue("mod1_shape");
    mod1_shapeMod = apvts.getRawParameterValue("mod1_shape_mod");
    for (int p = 1; p <= 8; ++p) {
        if (p == 5) continue;
        mod1_p[p-1] = apvts.getRawParameterValue("mod1_p" + juce::String(p));
        mod1_pMod[p-1] = apvts.getRawParameterValue("mod1_p" + juce::String(p) + "_mod");
    }
    
    for (int m = 2; m <= 8; ++m) {
        juce::String mStr = juce::String(m);
        mod_engine[m-2] = apvts.getRawParameterValue("mod" + mStr + "_engine");
        mod_macro[m-2] = apvts.getRawParameterValue("mod" + mStr + "_macro");
        for (int p = 1; p <= 8; ++p) {
            juce::String pStr = juce::String(p);
            if (p == 6) {
                mod_p[m-2][5] = apvts.getRawParameterValue("mod" + mStr + "_filterA");
                mod_pMod[m-2][5] = apvts.getRawParameterValue("mod" + mStr + "_filterA_mod");
                
                mod_p[m-2][6] = apvts.getRawParameterValue("mod" + mStr + "_filterB");
                mod_pMod[m-2][6] = apvts.getRawParameterValue("mod" + mStr + "_filterB_mod");
            } else {
                mod_p[m-2][p-1] = apvts.getRawParameterValue("mod" + mStr + "_p" + pStr);
                mod_pMod[m-2][p-1] = apvts.getRawParameterValue("mod" + mStr + "_p" + pStr + "_mod");
            }
        }
    }
    
    attack = apvts.getRawParameterValue("attack");
    decay = apvts.getRawParameterValue("decay");
    sustain = apvts.getRawParameterValue("sustain");
    release = apvts.getRawParameterValue("release");
    drift = apvts.getRawParameterValue("drift");
    
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
    // oversampler.reset(new juce::dsp::Oversampling<float> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false));
    // oversampler->initProcessing (static_cast<size_t> (samplesPerBlock));
    
    // double oversampledRate = sampleRate * oversampler->getOversamplingFactor();
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
            if (attack && decay && sustain && release && drift) {
                voice->updateAdsr (attack->load(), decay->load(), sustain->load(), release->load(), drift->load());
            }
            
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
    
    /*
    juce::dsp::AudioBlock<float> audioBlock (buffer);
    juce::dsp::AudioBlock<float> oversampledBlock = oversampler->processSamplesUp (audioBlock);
    
    int numOversampledChannels = (int)oversampledBlock.getNumChannels();
    juce::Array<float*> channelPointers;
    for (int i = 0; i < numOversampledChannels; ++i) {
        channelPointers.add(oversampledBlock.getChannelPointer(i));
    }
    juce::AudioBuffer<float> oversampledBuffer (channelPointers.getRawDataPointer(), numOversampledChannels, (int)oversampledBlock.getNumSamples());
    oversampledBuffer.clear(); // Ensure buffer is clean before synth rendering
    
    juce::MidiBuffer oversampledMidi;
    int factor = (int)oversampler->getOversamplingFactor();
    for (const auto meta : midiMessages) {
        oversampledMidi.addEvent (meta.getMessage(), meta.samplePosition * factor);
    }
    
    synth.renderNextBlock (oversampledBuffer, oversampledMidi, 0, oversampledBuffer.getNumSamples());
    
    oversampler->processSamplesDown (audioBlock);
    */

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

    // Master Limiter (Transparent Soft Clipping above -4dB threshold)
    const float threshold = 0.630957f; // -4 dB
    const float ceiling = 0.89125f; // -1 dB
    const float headroom = ceiling - threshold;
    
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float val = channelData[sample];
            float absVal = std::abs(val);
            if (absVal > threshold) {
                float excess = absVal - threshold;
                float clipped = threshold + headroom * std::tanh(excess / headroom);
                channelData[sample] = (val > 0.0f) ? clipped : -clipped;
            }
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
    if (xmlState != nullptr) {
        if (xmlState->hasTagName (apvts.state.getType())) {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
            
            // Restore routing order from the state tree property
            juce::String savedRouting = apvts.state.getProperty("routingOrder", "2,3,4,5,6,7,8").toString();
            updateRoutingOrder(savedRouting);
            
            // Push updated state to the UI if it is already open
            if (auto* editor = getActiveEditor()) {
                if (auto* myEditor = dynamic_cast<KronosAudioProcessorEditor*>(editor)) {
                    myEditor->triggerQueryAll();
                }
            }
        }
    }
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
    currentSampleRate = getSampleRate();
    if (currentSampleRate <= 0.0)
      currentSampleRate = 44100.0;

    currentFundamentalFreq += (fundamentalFreq - currentFundamentalFreq) * 0.06f;

    // --- 1. Source Engine (Lane 1) ---
    float partials_param = processor->mod1_p[0] ? processor->mod1_p[0]->load() : 256.0f;
    float partials_mod   = processor->mod1_pMod[0] ? processor->mod1_pMod[0]->load() : 0.0f;
    float balance_param  = processor->mod1_p[1] ? processor->mod1_p[1]->load() : 0.0f;
    float balance_mod    = processor->mod1_pMod[1] ? processor->mod1_pMod[1]->load() : 0.0f;
    float width_param    = processor->mod1_p[2] ? processor->mod1_p[2]->load() : 0.0f;
    float width_mod      = processor->mod1_pMod[2] ? processor->mod1_pMod[2]->load() : 0.0f;

    float pitch_param    = processor->mod1_p[3] ? processor->mod1_p[3]->load() : 0.0f;
    float pitch_mod      = processor->mod1_pMod[3] ? processor->mod1_pMod[3]->load() : 0.0f;

    float shape_param    = processor->mod1_shape ? processor->mod1_shape->load() : 0.0f;
    float shape_mod      = processor->mod1_shapeMod ? processor->mod1_shapeMod->load() : 0.0f;

    float sourceMacro    = processor->mod1_macro ? processor->mod1_macro->load() : 0.0f;

    float currentPartials = std::clamp(partials_param + sourceMacro * partials_mod * 256.0f, 1.0f, 256.0f);
    float currentBalance  = std::clamp(balance_param + sourceMacro * balance_mod, -1.0f, 1.0f);
    float currentWidth    = std::clamp(width_param + sourceMacro * width_mod, -1.0f, 1.0f);
    float currentPitch    = std::clamp(pitch_param + sourceMacro * pitch_mod * 36.0f, -36.0f, 36.0f); // Range is already -36 to 36
    float currentShape    = std::clamp(shape_param + sourceMacro * shape_mod, 0.0f, 1.0f);
    
    // Convert currentPitch from semitones to frequency multiplier
    float pitchMult = std::pow(2.0f, currentPitch / 12.0f);
    float renderFreq = currentFundamentalFreq * pitchMult;

    int targetPartials = (int)currentPartials;
    float maxHarmonics = ((float)currentSampleRate / 2.0f) / renderFreq;
    if (maxHarmonics < 1.0f) maxHarmonics = 1.0f;
    
    float spacing = 1.0f;
    if (currentWidth > 0.0f) {
        float maxWidthSpacing = std::max(1.0f, maxHarmonics / (float)targetPartials);
        spacing = 1.0f + currentWidth * (maxWidthSpacing - 1.0f);
    } else if (currentWidth < 0.0f) {
        spacing = 1.0f + currentWidth * 0.95f; // shrinks to 0.05
    }

    float totalClusterSpan = (float)targetPartials * spacing;
    float clusterStart = 1.0f;
    if (currentBalance > 0.0f) {
        float maxStart = maxHarmonics - totalClusterSpan;
        if (maxStart < 1.0f) maxStart = 1.0f;
        clusterStart = 1.0f + currentBalance * (maxStart - 1.0f);
    } else if (currentBalance < 0.0f) {
        float maxStart = maxHarmonics - totalClusterSpan;
        if (maxStart < 1.0f) maxStart = 1.0f;
        clusterStart = 1.0f + currentBalance * maxStart; // Pushes down to roughly -maxStart
    }

    auto getSpectralShape = [](int p, float harmonicIndex, int shapeIndex) -> float {
        float rawVal = 0.0f;
        switch (shapeIndex) {
            case 0: rawVal = 1.0f / std::pow(harmonicIndex, 1.3f); break;
            case 1: rawVal = (p % 2 == 0) ? (1.0f / harmonicIndex) : (0.08f / harmonicIndex); break;
            case 2: rawVal = (std::sin(p * 0.22f) * 0.4f + 0.6f) / std::sqrt(harmonicIndex); break;
            case 3: rawVal = (0.1f + 0.9f * ((float)p / 256.0f)) * (1.0f / std::pow(harmonicIndex, 0.8f)); break;
            case 4: rawVal = (std::exp(-std::pow(harmonicIndex - 3.0f, 2.0f) / 2.0f) + 0.5f * std::exp(-std::pow(harmonicIndex - 8.0f, 2.0f) / 8.0f) + 0.05f) / std::pow(harmonicIndex, 0.4f); break;
            case 5: rawVal = (std::exp(-std::pow(harmonicIndex - 6.0f, 2.0f) / 4.0f) + 0.4f * std::exp(-std::pow(harmonicIndex - 14.0f, 2.0f) / 16.0f) + 0.05f) / std::sqrt(harmonicIndex); break;
            case 6: rawVal = (p % 2 == 1) ? (1.0f / std::pow(harmonicIndex, 1.2f)) : (0.15f / harmonicIndex); break;
            case 7: rawVal = (std::sin(p * 1.618f) * 0.4f + 0.6f) / std::pow(harmonicIndex, 0.7f); break;
            case 8: rawVal = (p == 0) ? 1.0f : ((0.08f + 0.92f * std::exp(-std::pow(harmonicIndex - 12.0f, 2.0f) / 2.0f)) / std::pow(harmonicIndex, 0.7f)); break;
            case 9: rawVal = (std::sin(p * 123.456f) * 0.3f + 0.7f) / harmonicIndex; break;
            default: rawVal = 0.0f; break;
        }
        float baseline = 0.05f / std::max(0.001f, std::sqrt(harmonicIndex));
        return rawVal * 0.90f + baseline;
    };

    float scaledTimbre = currentShape * 9.0f;
    int timbreIdx = (int)scaledTimbre;
    float timbreMix = scaledTimbre - (float)timbreIdx;
    if (timbreIdx >= 9) {
        timbreIdx = 8;
        timbreMix = 1.0f;
    }

    float freqs[512];
    float targetAmps[512];
    float phaseDeltas[512];
    float pL_block[512];
    float pR_block[512];

    for (int p = 0; p < 512; ++p) {
        if (p < targetPartials) {
            float virtualHarmonicIndex = clusterStart + (float)p * spacing;
            float absVH = std::abs(virtualHarmonicIndex);
            freqs[p] = renderFreq * absVH;
            
            float hostNyquist = processor->getSampleRate() * 0.49f;
            if (freqs[p] >= hostNyquist) {
                targetAmps[p] = 0.0f;
            } else {
                float absVH_clamped = std::max(0.001f, absVH);
                float baseAmp = getSpectralShape(p, absVH_clamped, timbreIdx) * (1.0f - timbreMix) 
                              + getSpectralShape(p, absVH_clamped, timbreIdx + 1) * timbreMix;
                targetAmps[p] = targetAmp * std::min(1.0f, baseAmp);
            }
        } else {
            freqs[p] = 0.0f;
            targetAmps[p] = 0.0f;
        }
        
        pL_block[p] = panLeft[p];
        pR_block[p] = panRight[p];
    }

    // --- 2. Dynamic Serial Router (Lanes 2-8) ---
        float deSyncVal = 0.0f;
    float alterVal = 0.0f;
    float pinchVal = 0.0f;
    float ringVal = 0.0f;

    for (int i = 0; i < 7; ++i) {
        int laneNumber = std::round(processor->routingOrder[i].load());
        if (laneNumber < 2 || laneNumber > 8) continue;
        
        int laneIdx = laneNumber - 2;
        int engineType = processor->mod_engine[laneIdx] ? std::round(processor->mod_engine[laneIdx]->load()) : 0;
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
            
            int typeA = processor->mod_p[laneIdx][5] ? std::round(processor->mod_p[laneIdx][5]->load()) : 0;
            int typeB = processor->mod_p[laneIdx][6] ? std::round(processor->mod_p[laneIdx][6]->load()) : 0;

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
                    targetAmps[p] *= filterMult;
                }
            }
        } else if (engineType == 8) { // FORM
            float warp_param = processor->mod_p[laneIdx][0] ? processor->mod_p[laneIdx][0]->load() : 0.0f;
            float warp_mod   = processor->mod_pMod[laneIdx][0] ? processor->mod_pMod[laneIdx][0]->load() : 0.0f;
            float currentWarp = std::clamp(warp_param + macroVal * warp_mod, 0.0f, 1.0f);
            
            float fold_param = processor->mod_p[laneIdx][1] ? processor->mod_p[laneIdx][1]->load() : 0.0f;
            float fold_mod   = processor->mod_pMod[laneIdx][1] ? processor->mod_pMod[laneIdx][1]->load() : 0.0f;
            float currentFold = std::clamp(fold_param + macroVal * fold_mod, -1.0f, 1.0f);

            float tension_param = processor->mod_p[laneIdx][2] ? processor->mod_p[laneIdx][2]->load() : 0.0f;
            float tension_mod   = processor->mod_pMod[laneIdx][2] ? processor->mod_pMod[laneIdx][2]->load() : 0.0f;
            float currentTension = std::clamp(tension_param + macroVal * tension_mod, 0.0f, 1.0f);

            float shape_param = processor->mod_p[laneIdx][3] ? processor->mod_p[laneIdx][3]->load() : 0.0f;
            float shape_mod   = processor->mod_pMod[laneIdx][3] ? processor->mod_pMod[laneIdx][3]->load() : 0.0f;
            float currentShape = std::clamp(shape_param + macroVal * shape_mod, 0.0f, 1.0f);

            float phaseShift = currentFold * 6.2831853f;
            float freqMult = 1.0f + currentTension * 3.0f;

            for (int p = 0; p < targetPartials; ++p) {
                float phase = (((float)(p + 1) * 1.57f + (float)p * 0.1f) * freqMult) + phaseShift;
                // Wrap phase to 0 - 2PI for reliable waveform calculation
                float wrappedPhase = std::fmod(phase, 6.2831853f);
                if (wrappedPhase < 0.0f) wrappedPhase += 6.2831853f;

                float sineVal = std::sin(wrappedPhase);
                float rampVal = (wrappedPhase / 3.14159265f) - 1.0f; // -1.0 to 1.0
                float squareVal = (wrappedPhase < 3.14159265f) ? 1.0f : -1.0f;

                float waveOutput = 0.0f;
                if (currentShape <= 0.33f) {
                    float morph = currentShape / 0.33f;
                    waveOutput = sineVal * (1.0f - morph) + rampVal * morph;
                } else if (currentShape <= 0.66f) {
                    float morph = (currentShape - 0.33f) / 0.33f;
                    waveOutput = rampVal * (1.0f - morph) + squareVal * morph;
                } else {
                    waveOutput = squareVal;
                }

                float stretch = currentWarp * currentWarp * 3.5f * waveOutput;
                freqs[p] += currentFundamentalFreq * stretch;
                if (freqs[p] < 0.0f) freqs[p] = std::abs(freqs[p]);
            }
        } else if (engineType == 5) { // ALTER
            float fm_param = processor->mod_p[laneIdx][0] ? processor->mod_p[laneIdx][0]->load() : 0.0f;
            float fm_mod   = processor->mod_pMod[laneIdx][0] ? processor->mod_pMod[laneIdx][0]->load() : 0.0f;
            alterVal = std::clamp(fm_param + macroVal * fm_mod, 0.0f, 1.0f);
            
            float pinch_param = processor->mod_p[laneIdx][1] ? processor->mod_p[laneIdx][1]->load() : 0.0f;
            float pinch_mod   = processor->mod_pMod[laneIdx][1] ? processor->mod_pMod[laneIdx][1]->load() : 0.0f;
            pinchVal = std::clamp(pinch_param + macroVal * pinch_mod, 0.0f, 1.0f);
            
            float desync_param = processor->mod_p[laneIdx][2] ? processor->mod_p[laneIdx][2]->load() : 0.0f;
            float desync_mod   = processor->mod_pMod[laneIdx][2] ? processor->mod_pMod[laneIdx][2]->load() : 0.0f;
            deSyncVal = std::clamp(desync_param + macroVal * desync_mod, 0.0f, 1.0f);

            float ring_param = processor->mod_p[laneIdx][3] ? processor->mod_p[laneIdx][3]->load() : 0.0f;
            float ring_mod   = processor->mod_pMod[laneIdx][3] ? processor->mod_pMod[laneIdx][3]->load() : 0.0f;
            ringVal = std::clamp(ring_param + macroVal * ring_mod, 0.0f, 1.0f);

            float curve = deSyncVal * deSyncVal * deSyncVal;
            float syncMultiplier = 1.0f + curve * 9.0f;
            if (deSyncVal > 0.0f) {
                for (int p = 1; p < targetPartials; ++p) {
                    freqs[p] *= syncMultiplier;
                }
            }
        } else if (engineType == 7) { // INFECT
            float drive_param = processor->mod_p[laneIdx][0] ? processor->mod_p[laneIdx][0]->load() : 0.0f;
            float drive_mod   = processor->mod_pMod[laneIdx][0] ? processor->mod_pMod[laneIdx][0]->load() : 0.0f;
            
            float infectVal = std::clamp(drive_param + macroVal * drive_mod, 0.0f, 1.0f);
            
            float sym_param = processor->mod_p[laneIdx][1] ? processor->mod_p[laneIdx][1]->load() : 0.0f;
            float sym_mod   = processor->mod_pMod[laneIdx][1] ? processor->mod_pMod[laneIdx][1]->load() : 0.0f;
            float amountVal = std::clamp(sym_param + macroVal * sym_mod, 0.0f, 1.0f);

            if (amountVal > 0.001f) {
                float old_freqs[512];
                for (int p = 0; p < 512; ++p) old_freqs[p] = freqs[p];
                
                float stateFloat = infectVal * 14.0f;
                int stateIndex = (int)stateFloat;
                float morph = stateFloat - (float)stateIndex;

                auto getTargetFreq = [&](int state, int p) -> float {
                    if (p >= targetPartials) return old_freqs[p];
                    
                    int target_p = p;
                    switch(state) {
                        case 0: target_p = (p % 2 == 1) ? p - 1 : p; break;
                        case 1: target_p = p - (p % 3); break;
                        case 2: return old_freqs[0] + (old_freqs[p] - old_freqs[0]) * 0.1f;
                        case 3: {
                            int oct = 0; while((1 << (oct+1)) - 1 <= p) oct++;
                            target_p = (1 << oct) - 1; 
                            break;
                        }
                        case 4: return old_freqs[p] + ((p % 2 == 1) ? old_freqs[0] * 0.5f : 0.0f);
                        case 5: {
                            int primes[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97};
                            int h = p + 1;
                            int best = 2; int min_diff = 9999;
                            for (int pr : primes) { if (std::abs(h - pr) < min_diff) { min_diff = std::abs(h - pr); best = pr; } }
                            target_p = best - 1; 
                            break;
                        }
                        case 6: return (p < 15) ? old_freqs[0] : old_freqs[p] + old_freqs[0] * 32.0f;
                        case 7: return old_freqs[p] + std::sin(p * 0.5f) * old_freqs[0] * 2.0f;
                        case 8: return old_freqs[0] * (p + 1) * 1.61803398f;
                        case 9: target_p = std::round(p / 16.0f) * 16.0f; break;
                        case 10: target_p = 31 - std::abs(31 - p); break;
                        case 11: target_p = 6; break;
                        case 12: return old_freqs[0] + std::fmod(old_freqs[p] * 3.7f, old_freqs[0] * 16.0f);
                        case 13: target_p = 511 - p; break;
                        case 14: return old_freqs[p] + std::sin(p * 12.9898f) * old_freqs[p] * 0.5f;
                    }
                    if (target_p < 0) target_p = 0;
                    if (target_p > 511) target_p = 511;
                    return old_freqs[target_p];
                };

                for (int p = 0; p < targetPartials; ++p) {
                    if (targetAmps[p] > 0.0f) {
                        float freqA = getTargetFreq(stateIndex, p);
                        float freqB = getTargetFreq(std::min(14, stateIndex + 1), p);
                        float interpFreq = freqA * (1.0f - morph) + freqB * morph;
                        freqs[p] = old_freqs[p] * (1.0f - amountVal) + interpFreq * amountVal;
                        if (freqs[p] < 0.0f) freqs[p] = std::abs(freqs[p]);
                    }
                }
            }

            

            float clone_param = processor->mod_p[laneIdx][2] ? processor->mod_p[laneIdx][2]->load() : 0.0f;
            float clone_mod   = processor->mod_pMod[laneIdx][2] ? processor->mod_pMod[laneIdx][2]->load() : 0.0f;
            float cloneVal = std::clamp(clone_param + macroVal * clone_mod, 0.0f, 1.0f);
            
            float cloneAmount_param = processor->mod_p[laneIdx][3] ? processor->mod_p[laneIdx][3]->load() : 0.0f;
            float cloneAmount_mod   = processor->mod_pMod[laneIdx][3] ? processor->mod_pMod[laneIdx][3]->load() : 0.0f;
            float cloneAmountVal = std::clamp(cloneAmount_param + macroVal * cloneAmount_mod, 0.0f, 1.0f);

            if (cloneAmountVal > 0.001f) {
                float stateFloat = cloneVal * 14.0f;
                int stateIndex = (int)stateFloat;
                float morph = stateFloat - (float)stateIndex;

                auto getCloneMask = [&](int state, int p) -> float {
                    switch(state) {
                        case 0: return (p % 2 == 0) ? 1.5f : 0.5f; // Odd/Even Alternation
                        case 1: return (p % 3 == 0) ? 1.5f : 0.5f; // Triplets Focus
                        case 2: return (((p+1) & p) == 0) ? 1.8f : 0.2f; // Octave Isolation (powers of 2)
                        case 3: return 0.5f + 0.5f * std::sin((float)p * 0.1f); // Gentle Comb Filter
                        case 4: return 0.5f + 0.5f * std::sin((float)p * 0.5f); // Aggressive Comb Filter
                        case 5: return (float)(p % 16) / 15.0f; // Fractal Clones
                        case 6: { // Prime Number Mask
                            int primes[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97};
                            for (int pr : primes) { if (p+1 == pr) return 1.5f; }
                            return 0.2f;
                        }
                        case 7: return (float)(p % 10) / 9.0f; // Sawtooth Ripple
                        case 8: return (p / 10) % 2 == 0 ? 0.2f : 1.5f; // Spectral Gapping
                        case 9: return (p < 30) ? 0.4f : 1.5f; // High-Frequency Mirror
                        case 10: return (p == 0 || p == 1) ? 0.1f : 1.2f; // Sub-Harmonic Ghosting
                        case 11: return (p / 4) % 2 == 0 ? 1.5f : 0.2f; // Spectral Checkerboard
                        case 12: { // Fibonacci Masking
                            int fibs[] = {1,2,3,5,8,13,21,34,55,89,144,233,377};
                            for (int f : fibs) { if (p+1 == f) return 1.8f; }
                            return 0.1f;
                        }
                        case 13: return (float)((p * 7) % 13) / 13.0f; // Modulo Shredding
                        case 14: return std::abs(std::sin((float)p * 42.1337f)) * 1.5f; // Amplitude Entropy
                    }
                    return 1.0f;
                };

                for (int p = 0; p < targetPartials; ++p) {
                    if (targetAmps[p] > 0.0f) {
                        float maskA = getCloneMask(stateIndex, p);
                        float maskB = getCloneMask(std::min(14, stateIndex + 1), p);
                        float mask = maskA * (1.0f - morph) + maskB * morph;
                        targetAmps[p] *= (1.0f - cloneAmountVal) + (mask * cloneAmountVal);
                    }
                }
            }

        } else if (engineType == 3) { // SPACE

            float width_param = processor->mod_p[laneIdx][0] ? processor->mod_p[laneIdx][0]->load() : 0.0f;
            float width_mod   = processor->mod_pMod[laneIdx][0] ? processor->mod_pMod[laneIdx][0]->load() : 0.0f;
            float spaceVal = std::clamp(width_param + macroVal * width_mod, 0.0f, 1.0f);
            
            float orbit_param = processor->mod_p[laneIdx][1] ? processor->mod_p[laneIdx][1]->load() : 0.0f;
            float orbit_mod   = processor->mod_pMod[laneIdx][1] ? processor->mod_pMod[laneIdx][1]->load() : 0.0f;
            float orbitVal = std::clamp(orbit_param + macroVal * orbit_mod, -1.0f, 1.0f);

            float smear_param = processor->mod_p[laneIdx][2] ? processor->mod_p[laneIdx][2]->load() : 0.0f;
            float smear_mod   = processor->mod_pMod[laneIdx][2] ? processor->mod_pMod[laneIdx][2]->load() : 0.0f;
            float smearVal = std::clamp(smear_param + macroVal * smear_mod, 0.0f, 1.0f);

            float orbitSpeedHz = orbitVal * 30.0f;
            float blockDuration = (float)targetAmps[0] == 0.0f ? 0.0f : (1.0f / (float)currentSampleRate); // approx per sample
            spaceOrbitPhase += (orbitSpeedHz * 6.2831853f) * (float)numSamples / (float)currentSampleRate; // accumulate per block

            while (spaceOrbitPhase > 6.2831853f) spaceOrbitPhase -= 6.2831853f;
            while (spaceOrbitPhase < -6.2831853f) spaceOrbitPhase += 6.2831853f;

            float orbitPanL = 0.5f + 0.5f * std::cos(spaceOrbitPhase);
            float orbitPanR = 0.5f + 0.5f * std::sin(spaceOrbitPhase);

            for (int p = 0; p < targetPartials; ++p) {
                if (targetAmps[p] > 0.0f) {
                    float lfoDrift = std::sin((float)voiceTime * 1.2f + phaseDrifts[p]) * spaceVal * 0.3f;
                    targetAmps[p] *= (1.0f + lfoDrift);
                    
                    if (smearVal > 0.0f) {
                        freqs[p] += std::sin(phaseDrifts[p]) * smearVal * ((float)p * 0.2f);
                    }
                    
                    if (p == 0) {
                        pL_block[p] = panLeft[p] * (1.0f - spaceVal) + 0.707f * spaceVal;
                        pR_block[p] = panRight[p] * (1.0f - spaceVal) + 0.707f * spaceVal;
                    } else if (p % 2 == 0) {
                        float evenLeft = orbitPanL;
                        float evenRight = 1.0f - orbitPanL;
                        pL_block[p] = panLeft[p] * (1.0f - spaceVal) + evenLeft * spaceVal;
                        pR_block[p] = panRight[p] * (1.0f - spaceVal) + evenRight * spaceVal;
                    } else {
                        float oddLeft = orbitPanR;
                        float oddRight = 1.0f - orbitPanR;
                        pL_block[p] = panLeft[p] * (1.0f - spaceVal) + oddLeft * spaceVal;
                        pR_block[p] = panRight[p] * (1.0f - spaceVal) + oddRight * spaceVal;
                    }
                }
            }
        }
    }

    // --- 2.5 Block-Rate Per-Partial ADSR ---
    bool anyEnvelopeActive = false;
    float blockDuration = (float)numSamples / currentSampleRate;
    
    for (int p = 0; p < 512; ++p) {
        if (partialEnvStates[p] == 0) {
            partialEnvLevels[p] = 0.0f;
            continue;
        }
        
        anyEnvelopeActive = true;
        
        if (partialEnvStates[p] == 1) { // Attack
            float attackT = partialAttackTimes[p];
            float step = (attackT > 0.001f) ? (blockDuration / attackT) : 1.0f;
            partialEnvLevels[p] += step;
            if (partialEnvLevels[p] >= 1.0f) {
                partialEnvLevels[p] = 1.0f;
                partialEnvStates[p] = 2; // Decay
            }
        } 
        else if (partialEnvStates[p] == 2) { // Decay
            float step = (envDecayTime > 0.001f) ? (blockDuration / envDecayTime) : 1.0f;
            partialEnvLevels[p] -= step;
            if (partialEnvLevels[p] <= envSustain) {
                partialEnvLevels[p] = envSustain;
                partialEnvStates[p] = 3; // Sustain
            }
        }
        else if (partialEnvStates[p] == 3) { // Sustain
            partialEnvLevels[p] = envSustain;
        }
        else if (partialEnvStates[p] == 4) { // Release
            float releaseT = partialReleaseTimes[p];
            float step = (releaseT > 0.001f) ? (blockDuration / releaseT) : 1.0f;
            partialEnvLevels[p] -= step;
            if (partialEnvLevels[p] <= 0.0f) {
                partialEnvLevels[p] = 0.0f;
                partialEnvStates[p] = 0; // Idle
            }
        }
    }
    
    if (!anyEnvelopeActive) {
        clearCurrentNote();
        voiceActive = false;
        return;
    }

    // --- 3. Finalization (Panning & Phase Deltas) ---
    int activePartials[512];
    int numActivePartials = 0;

    for (int p = 0; p < 512; ++p) {
        float hostNyquist = processor->getSampleRate() * 0.49f;
        if (freqs[p] >= hostNyquist) {
            targetAmps[p] = 0.0f; // Prevent aliasing based on HOST sample rate, not oversampled rate!
        }
        
        if (p < targetPartials && targetAmps[p] > 0.0f) {
            phaseDeltas[p] = freqs[p] / (float)currentSampleRate;
        } else {
            phaseDeltas[p] = 0.0f;
            pL_block[p] = 0.0f;
            pR_block[p] = 0.0f;
        }

        // Apply per-partial ADSR!
        targetAmps[p] *= partialEnvLevels[p];

        // Collect active partials
        if (targetAmps[p] >= 0.0001f || smoothedAmps[p] >= 0.0001f) {
            activePartials[numActivePartials++] = p;
        }
    }

    // Mix into output buffers
    float scaleFactor = 0.2818f; // Fixed -11dB attenuation
    float syncMix = std::clamp(deSyncVal / 0.30f, 0.0f, 1.0f);

    // Pre-calculate FM base modulation indices for all active partials to save millions of divisions
    float baseModIndices[512] = {0.0f};
    int maxModulatingIndex = 0;
    if (alterVal > 0.0f && numActivePartials > 1) {
        maxModulatingIndex = numActivePartials - 1;
        for (int i = 1; i <= maxModulatingIndex; ++i) {
            int p = activePartials[i];
            int p_prev = activePartials[i - 1];
            float distance = std::abs(freqs[p] - freqs[p_prev]);
            float normDistance = distance / currentFundamentalFreq;
            baseModIndices[i] = (alterVal * alterVal * 10.0f) / (normDistance + 0.05f);
        }
    }

    for (int s = 0; s < numSamples; ++s) {
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

      // maxModulatingIndex is now pre-calculated before the loop!

      for (int i = 0; i < numActivePartials; ++i) {
        int p = activePartials[i];
        
        // Track the dry target amplitude envelope smoothly (15ms time-constant)
        float dry_target = targetAmps[p];
        smoothedAmps[p] += (dry_target - smoothedAmps[p]) * 0.15f;
        float a = smoothedAmps[p];

        // 2. Update phase for partial p
          if (p > 0) {
            phases[p] += phaseDeltas[p];
            if (phases[p] >= 1.0f) {
              phases[p] -= 1.0f;
            }
            
            syncedPhases[p] += phaseDeltas[p];
            if (deSyncVal > 0.0f && masterWrapped) {
              // Clickless subsample precision sync
              float overshoot = phases[0];
              syncedPhases[p] = overshoot * (phaseDeltas[p] / phaseDeltas[0]);
              if (syncedPhases[p] >= 1.0f) {
                  syncedPhases[p] -= std::floor(syncedPhases[p]);
              }
            } else if (syncedPhases[p] >= 1.0f) {
              syncedPhases[p] -= 1.0f;
            }
          }

        float modOffset = 0.0f;
        if (i > 0 && i <= maxModulatingIndex) {
          int p_prev = activePartials[i - 1];
          float modIndex = baseModIndices[i] * smoothedAmps[p_prev];
          if (modIndex > 5.0f) modIndex = 5.0f;
          modOffset = modIndex * prevVal;
        }

        // Unsynced phase calculation (using fast bitwise wrapping instead of std::floor)
        float modPhaseUnsync = phases[p] + modOffset;
        int idxUnsync = static_cast<int>((modPhaseUnsync + 1024.0f) * 32768.0f) & 32767;
        float valUnsync = sineTable[idxUnsync];

        float val = valUnsync;

          // Bypass heavy sync calculations if mix is 0
          if (syncMix > 0.0f) {
              float modPhaseSync = (p > 0) ? syncedPhases[p] + modOffset : modPhaseUnsync;
              int idxSync = static_cast<int>((modPhaseSync + 1024.0f) * 32768.0f) & 32767;
              float valSync = sineTable[idxSync];
              
              // Windowed sync (VOSIM) smoothing driven by the fundamental phase
              float windowPhase = phases[0];
              float syncWindow = std::min(1.0f, std::sin(windowPhase * 3.14159265f) * 4.0f);
              valSync *= syncWindow;
              
              val = valUnsync * (1.0f - syncMix) + valSync * syncMix;
          }
        prevVal = val;

        float dryVal = val * a;

        // FDN send routing based on harmonic index p
        sampleL += dryVal * pL_block[p];
        sampleR += dryVal * pR_block[p];
      }

      outputBuffer.addSample(0, startSample + s, sampleL * scaleFactor);
      outputBuffer.addSample(1, startSample + s, sampleR * scaleFactor);

      voiceTime += 1.0 / currentSampleRate;
    }
}


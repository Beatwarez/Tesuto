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
    layout.add(std::make_unique<juce::AudioParameterString>(juce::ParameterID("routingOrder", 1), "RoutingOrder", "1,2,3,4,5,6,7,8"));
    
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

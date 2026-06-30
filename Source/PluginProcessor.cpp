#include "PluginProcessor.h"
#include "PluginEditor.h"

float sineTable[32768];

// ==========================================================================
// Constructor & Destructor
// ==========================================================================
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
       apvts (*this, nullptr, "PARAMETERS", {
           std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("detune", 1), "Detune", 0.0f, 1.0f, 0.0f),
           std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("timbre", 1), "Timbre", 0.0f, 1.0f, 0.25f),
           std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("cutoff", 1), "Cutoff", 0.0f, 1.0f, 0.75f),
           std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("space", 1), "Space",  0.0f, 1.0f, 0.30f),
           std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("alter", 1), "Alter",  0.0f, 1.0f, 0.0f),
           std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("attack", 1), "Attack", juce::NormalisableRange<float> (0.01f, 5.0f, 0.01f, 0.35f), 0.20f),
           std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("decay", 1), "Decay",  juce::NormalisableRange<float> (0.01f, 5.0f, 0.01f, 0.35f), 0.30f),
           std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("sustain", 1), "Sustain", 0.0f, 1.0f, 0.80f),
           std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("release", 1), "Release", juce::NormalisableRange<float> (0.01f, 8.0f, 0.01f, 0.35f), 1.00f)
       })
{
    for (int i = 0; i < 32768; ++i)
        sineTable[i] = std::sin (((float)i / 32768.0f) * juce::MathConstants<float>::twoPi);

    for (int i = 0; i < 128; ++i)
        activeMidiNotes[i] = false;

    synth.clearVoices();
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new KronosVoice());
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
void KronosAudioProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
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
    float detune = *apvts.getRawParameterValue ("detune");
    float timbre = *apvts.getRawParameterValue ("timbre");
    float cutoff = *apvts.getRawParameterValue ("cutoff");
    float space  = *apvts.getRawParameterValue ("space");
    float alter  = *apvts.getRawParameterValue ("alter");
    float attack = *apvts.getRawParameterValue ("attack");
    float decay  = *apvts.getRawParameterValue ("decay");
    float sustain = *apvts.getRawParameterValue ("sustain");
    float release = *apvts.getRawParameterValue ("release");

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<KronosVoice*> (synth.getVoice (i)))
        {
            voice->updateParams (detune, timbre, cutoff, space);
            voice->updateAlter (alter);
            voice->updateAdsr (attack, decay, sustain, release);
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
        else if (msg.isController())
        {
            int ccNumber = msg.getControllerNumber();
            float ccValue = (float) msg.getControllerValue() / 127.0f; // Normalized 0.0 to 1.0

            if (ccNumber == 1)
            {
                if (auto* rawVal = apvts.getRawParameterValue ("detune"))
                    rawVal->store (ccValue);
            }
            else if (ccNumber == 2)
            {
                if (auto* rawVal = apvts.getRawParameterValue ("timbre"))
                    rawVal->store (ccValue);
            }
            else if (ccNumber == 3)
            {
                if (auto* rawVal = apvts.getRawParameterValue ("cutoff"))
                    rawVal->store (ccValue);
            }
            else if (ccNumber == 4)
            {
                if (auto* rawVal = apvts.getRawParameterValue ("space"))
                    rawVal->store (ccValue);
            }
            else if (ccNumber == 5)
            {
                if (auto* rawVal = apvts.getRawParameterValue ("alter"))
                    rawVal->store (ccValue);
            }
        }
    }

    // Render Synth voices
    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

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

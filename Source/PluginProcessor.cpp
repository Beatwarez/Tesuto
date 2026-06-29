#include "PluginProcessor.h"
#include "PluginEditor.h"

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
           std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("space", 1), "Space",  0.0f, 1.0f, 0.30f)
       })
{
    synth.clearVoices();
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new KronosVoice());

    synth.clearSounds();
    synth.addSound (new KronosSound());
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

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<KronosVoice*> (synth.getVoice (i)))
        {
            voice->updateParams (detune, timbre, cutoff, space);
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

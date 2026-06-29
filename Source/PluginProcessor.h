#pragma once

#include <JuceHeader.h>

// ==========================================================================
// 1. Synthesiser Voice Class
// ==========================================================================
class KronosVoice : public juce::SynthesiserVoice
{
public:
    KronosVoice()
    {
        for (int p = 0; p < 256; ++p)
        {
            phases[p] = juce::Random::getSystemRandom().nextFloat();
            phaseDrifts[p] = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
            float basePan = (p % 2 == 0) ? 0.25f : 0.75f;
            panLeft[p] = std::sqrt(1.0f - basePan);
            panRight[p] = std::sqrt(basePan);
        }
    }

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return sound != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        noteNumber = midiNoteNumber;
        float targetFreq = 440.0f * std::pow(2.0f, (float)(midiNoteNumber - 69) / 12.0f);
        
        if (envState == EnvState::idle)
        {
            fundamentalFreq = targetFreq;
            currentFundamentalFreq = targetFreq;
            for (int p = 0; p < 256; ++p)
                phases[p] = juce::Random::getSystemRandom().nextFloat();
        }
        else
        {
            fundamentalFreq = targetFreq; // Glide
        }

        targetAmp = velocity;
        envState = EnvState::attack;
        voiceActive = true;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            targetAmp = 0.0f;
            envState = EnvState::release;
        }
        else
        {
            envState = EnvState::idle;
            clearCurrentNote();
            voiceActive = false;
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void updateParams (float detune, float timbre, float cutoff, float space)
    {
        detuneVal = detune;
        timbreVal = timbre;
        cutoffVal = cutoff;
        spaceVal = space;
    }

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (envState == EnvState::idle)
            return;

        currentSampleRate = getSampleRate();
        if (currentSampleRate <= 0.0) currentSampleRate = 44100.0;

        float blockTime = (float)numSamples / (float)currentSampleRate;
        float attackStep = blockTime / 0.8f;
        float releaseStep = blockTime / 1.5f;

        // Exponential mapping for cutoff frequency (50Hz to 12000Hz)
        float cutoffFreq = 50.0f * std::pow(2.0f, cutoffVal * 8.0f);

        // Precompute partial amplitudes and frequencies
        float freqs[256];
        float amps[256];

        currentFundamentalFreq += (fundamentalFreq - currentFundamentalFreq) * 0.06f;

        // Envelope update for the block
        if (envState == EnvState::attack)
        {
            currentAmp += attackStep;
            if (currentAmp >= targetAmp)
            {
                currentAmp = targetAmp;
                envState = EnvState::sustain;
            }
        }
        else if (envState == EnvState::release)
        {
            currentAmp -= releaseStep;
            if (currentAmp <= 0.0f)
            {
                currentAmp = 0.0f;
                envState = EnvState::idle;
                clearCurrentNote();
                voiceActive = false;
                return;
            }
        }
        else if (envState == EnvState::sustain)
        {
            currentAmp += (targetAmp - currentAmp) * 0.01f;
        }

        if (currentAmp <= 0.0001f)
            return;

        for (int p = 0; p < 256; ++p)
        {
            int harmonicIndex = p + 1;

            // Detune / Inharmonic Warp
            float stretch = detuneVal * detuneVal * 3.5f * std::sin((float)harmonicIndex * 1.57f + (float)p * 0.1f);
            freqs[p] = currentFundamentalFreq * ((float)harmonicIndex + stretch);

            // Timbre Morph
            float baseAmp = 0.0f;
            if (timbreVal < 0.5f)
            {
                float mix = timbreVal * 2.0f;
                float ampA = 1.0f / std::pow((float)harmonicIndex, 1.2f);
                float ampB = (std::sin((float)p * 0.22f) * 0.5f + 0.5f) / std::sqrt((float)harmonicIndex);
                baseAmp = ampA * (1.0f - mix) + ampB * mix;
            }
            else
            {
                float mix = (timbreVal - 0.5f) * 2.0f;
                float ampB = (std::sin((float)p * 0.22f) * 0.5f + 0.5f) / std::sqrt((float)harmonicIndex);
                float ampC = (1.0f - ((float)p / 256.0f)) * 0.5f;
                baseAmp = ampB * (1.0f - mix) + ampC * mix;
            }

            // Cutoff limiter
            float ratio = freqs[p] / cutoffFreq;
            float filterMult = 1.0f / (1.0f + std::pow(ratio, 6.0f));

            // Space (Organic LFO drift)
            float lfoDrift = std::sin((float)voiceTime * 1.2f + phaseDrifts[p]) * spaceVal * 0.3f;

            amps[p] = baseAmp * filterMult * currentAmp * (1.0f + lfoDrift);
        }

        // Mix into output buffers
        float scaleFactor = 0.045f; // Level normalization per voice

        for (int s = 0; s < numSamples; ++s)
        {
            float sampleL = 0.0f;
            float sampleR = 0.0f;

            for (int p = 0; p < 256; ++p)
            {
                float f = freqs[p];
                float a = amps[p];
                if (a < 0.0001f) continue;

                phases[p] += f / (float)currentSampleRate;
                if (phases[p] >= 1.0f) phases[p] -= 1.0f;

                float val = std::sin(phases[p] * juce::MathConstants<float>::twoPi);

                float pL = panLeft[p] * (1.0f - spaceVal) + (p % 2 == 0 ? 1.0f : 0.0f) * spaceVal;
                float pR = panRight[p] * (1.0f - spaceVal) + (p % 2 == 1 ? 1.0f : 0.0f) * spaceVal;

                sampleL += val * a * pL;
                sampleR += val * a * pR;
            }

            outputBuffer.addSample (0, startSample + s, sampleL * scaleFactor);
            outputBuffer.addSample (1, startSample + s, sampleR * scaleFactor);

            voiceTime += 1.0 / currentSampleRate;
        }
    }

private:
    enum class EnvState { idle, attack, sustain, release };
    EnvState envState = EnvState::idle;
    bool voiceActive = false;

    double currentSampleRate = 44100.0;
    int noteNumber = -1;
    float currentAmp = 0.0f;
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

    double voiceTime = 0.0;
};

// ==========================================================================
// 2. Synthesiser Sound Class
// ==========================================================================
class KronosSound : public juce::SynthesiserSound
{
public:
    KronosSound() {}
    bool canPlayNote (int) override { return true; }
    bool canPlayChannel (int) override { return true; }
};

// ==========================================================================
// 3. Audio Processor Class
// ==========================================================================
class KronosAudioProcessor : public juce::AudioProcessor
{
public:
    KronosAudioProcessor();
    ~KronosAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::Synthesiser synth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KronosAudioProcessor)
};

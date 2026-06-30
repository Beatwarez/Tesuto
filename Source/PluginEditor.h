#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// ==========================================================================
// Custom WebBrowserComponent Subclass for JS-to-C++ Parameter Bridging
// ==========================================================================
class KronosWebView : public juce::WebBrowserComponent
{
public:
    KronosWebView (KronosAudioProcessor& p)
        : juce::WebBrowserComponent (juce::WebBrowserComponent::Options()
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2()
                .withUserDataFolder (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                     .getChildFile ("KronosSynth/WebView2Data")))
            .withNativeIntegrationEnabled (true)
            .withNativeFunction ("sendParamToCpp", [this](const juce::var& args, std::function<void (juce::var)> completion)
            {
                if (args.size() >= 2)
                {
                    juce::String paramName = args[0].toString();
                    float paramValue = (float)args[1];

                    if (paramName == "noteon")
                    {
                        processor.triggerNoteOnFromEditor ((int)paramValue, 0.8f);
                    }
                    else if (paramName == "noteoff")
                    {
                        processor.triggerNoteOffFromEditor ((int)paramValue);
                    }
                    else
                    {
                        // 1. Force raw parameter update directly to guarantee instant audio thread response
                        if (auto* rawVal = processor.apvts.getRawParameterValue (paramName))
                        {
                            rawVal->store (paramValue);
                        }
                        
                        // 2. Notify the host of the parameter change for automation recording
                        if (auto* param = processor.apvts.getParameter (paramName))
                        {
                            param->beginChangeGesture();
                            param->setValueNotifyingHost (paramValue);
                            param->endChangeGesture();
                        }
                    }
                }
                completion (juce::var (true));
            })),
          processor (p)
    {
    }

private:
    KronosAudioProcessor& processor;
};

// ==========================================================================
// Plugin Editor Class
// ==========================================================================
class KronosAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::Timer
{
public:
    KronosAudioProcessorEditor (KronosAudioProcessor&);
    ~KronosAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    KronosAudioProcessor& audioProcessor;
    KronosWebView webView;
    bool localActiveNotes[128] = { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KronosAudioProcessorEditor)
};

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// ==========================================================================
// Custom WebBrowserComponent Subclass for JS-to-C++ Parameter Bridging
// ==========================================================================
class KronosWebView : public juce::WebBrowserComponent
{
public:
    static void logToFile (const juce::String& message)
    {
        auto logFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("KronosSynthDebug.txt");
        logFile.appendText (message + "\n");
    }

    static juce::WebBrowserComponent::Options getOptions (KronosAudioProcessor& p)
    {
        // 1. Force clear WebView2 cache folder to bypass any aggressive local caching
        auto folder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("KronosSynth/WebView2Data");
        folder.deleteRecursively();

        logToFile ("--- WebView Initialized ---");

        return juce::WebBrowserComponent::Options()
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2()
                .withUserDataFolder (folder))
            .withNativeIntegrationEnabled (true)
            .withNativeFunction ("sendParamToCpp", [&p](const juce::var& args, std::function<void (juce::var)> completion)
            {
                logToFile ("C++: sendParamToCpp called. args size = " + juce::String (args.size()));
                if (args.size() >= 2)
                {
                    juce::String paramName = args[0].toString();
                    float paramValue = (float)args[1];
                    logToFile ("C++: Received Parameter: " + paramName + " = " + juce::String (paramValue));

                    if (paramName == "noteon")
                    {
                        p.triggerNoteOnFromEditor ((int)paramValue, 0.8f);
                    }
                    else if (paramName == "noteoff")
                    {
                        p.triggerNoteOffFromEditor ((int)paramValue);
                    }
                    else
                    {
                        // 1. Force raw parameter update directly to guarantee instant audio thread response
                        if (auto* rawVal = p.apvts.getRawParameterValue (paramName))
                        {
                            rawVal->store (paramValue);
                        }
                        
                        // 2. Notify the host of the parameter change for automation recording
                        if (auto* param = p.apvts.getParameter (paramName))
                        {
                            param->beginChangeGesture();
                            param->setValueNotifyingHost (paramValue);
                            param->endChangeGesture();
                        }
                    }
                }
                completion (juce::var (true));
            });
    }

    KronosWebView (KronosAudioProcessor& p)
        : juce::WebBrowserComponent (getOptions (p)),
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
    float localParams[4] = { -1.0f, -1.0f, -1.0f, -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KronosAudioProcessorEditor)
};

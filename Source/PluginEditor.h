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
                                     .getChildFile ("KronosSynth/WebView2Data")))),
          processor (p)
    {
    }

    bool pageAboutToLoad (const juce::String& newURL) override
    {
        // Intercept parameter bridge scheme: kronos://param?name=X&value=Y
        if (newURL.startsWith ("kronos://"))
        {
            juce::URL url (newURL);
            auto names = url.getParameterNames();
            auto values = url.getParameterValues();
            
            juce::String paramName;
            float paramValue = 0.0f;
            
            for (int i = 0; i < names.size(); ++i)
            {
                if (names[i] == "name")
                    paramName = values[i];
                else if (names[i] == "value")
                    paramValue = values[i].getFloatValue();
            }
            
            // Set parameter value in APVTS
            // Set parameter value in APVTS or handle Note triggers
            if (paramName.isNotEmpty())
            {
                if (paramName == "noteon")
                {
                    processor.triggerNoteOnFromEditor ((int)paramValue, 0.8f);
                }
                else if (paramName == "noteoff")
                {
                    processor.triggerNoteOffFromEditor ((int)paramValue);
                }
                else if (auto* param = processor.apvts.getParameter (paramName))
                {
                    param->beginChangeGesture();
                    param->setValueNotifyingHost (paramValue);
                    param->endChangeGesture();
                }
            }
            
            return false; // Block actual browser page redirect
        }
        return true;
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

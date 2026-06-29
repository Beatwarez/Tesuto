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
        : juce::WebBrowserComponent (juce::WebBrowserComponent::Options().withResourceProvider (nullptr)),
          processor (p)
    {
    }

    bool pageAboutToLoad (const juce::String& newURL) override
    {
        // Intercept parameter bridge scheme: kronos://param?name=X&value=Y
        if (newURL.startsWith ("kronos://"))
        {
            juce::URL url (newURL);
            auto name = url.getParameterValue ("name");
            auto value = url.getParameterValue ("value").getFloatValue();
            
            // Set parameter value in APVTS
            if (auto* param = processor.apvts.getParameter (name))
            {
                param->setValueNotifyingHost (value);
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
class KronosAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    KronosAudioProcessorEditor (KronosAudioProcessor&);
    ~KronosAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    KronosAudioProcessor& audioProcessor;
    KronosWebView webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KronosAudioProcessorEditor)
};

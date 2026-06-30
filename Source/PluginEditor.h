#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// ==========================================================================
// Custom WebBrowserComponent Subclass for JS-to-C++ Parameter Bridging
// ==========================================================================
class KronosWebView : public juce::WebBrowserComponent
{
public:
    float localParams[10] = { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };

    static void logToFile (const juce::String& message)
    {
        auto logFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("KronosSynthDebug.txt");
        logFile.appendText (message + "\n");
    }

    static juce::WebBrowserComponent::Options getOptions (KronosWebView* webViewInstance, KronosAudioProcessor& p)
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
            .withResourceProvider ([] (const juce::String& url) -> std::optional<juce::WebBrowserComponent::Resource>
            {
                auto retrieveResource = [] (const char* data, int size, const juce::String& mime)
                {
                    std::vector<std::byte> vec;
                    vec.resize ((size_t) size);
                    std::memcpy (vec.data(), data, (size_t) size);
                    return juce::WebBrowserComponent::Resource { std::move (vec), mime };
                };

                const auto urlToRetrieve = url == "/" ? juce::String ("index.html")
                                                      : url.fromFirstOccurrenceOf ("/", false, false);

                // Serve from memory over the native resource provider root virtual origin
                if (urlToRetrieve == "index.html")
                    return retrieveResource (BinaryData::index_html, BinaryData::index_htmlSize, "text/html");
                if (urlToRetrieve.contains ("app.js"))
                    return retrieveResource (BinaryData::app_js, BinaryData::app_jsSize, "application/javascript");
                if (urlToRetrieve.contains ("styles.css"))
                    return retrieveResource (BinaryData::styles_css, BinaryData::styles_cssSize, "text/css");

                return std::nullopt;
            })
            .withNativeFunction ("sendParamToCpp", [webViewInstance, &p](const juce::var& args, std::function<void (juce::var)> completion)
            {
                logToFile ("C++: sendParamToCpp called. args size = " + juce::String (args.size()));
                if (args.size() >= 2)
                {
                    juce::String paramName = args[0].toString();
                    float paramValue = (float)args[1];
                    logToFile ("C++: Received Parameter: " + paramName + " = " + juce::String (paramValue));

                    if (paramName == "queryall")
                    {
                        for (int i = 0; i < 10; ++i)
                            webViewInstance->localParams[i] = -1.0f;
                    }
                    else if (paramName == "noteon")
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
                            if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
                                rangedParam->setValueNotifyingHost (rangedParam->getNormalisableRange().convertTo0to1 (paramValue));
                            else
                                param->setValueNotifyingHost (paramValue);
                            param->endChangeGesture();
                        }
                    }
                }
                completion (juce::var (true));
            });
    }

    KronosWebView (KronosAudioProcessor& p)
        : juce::WebBrowserComponent (getOptions (this, p)),
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

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <unordered_map>

// ==========================================================================
// Custom WebBrowserComponent Subclass for JS-to-C++ Parameter Bridging
// ==========================================================================
class KronosWebView : public juce::WebBrowserComponent
{
public:
    std::unordered_map<juce::String, float> localParams;
    bool localActiveNotes[128];

    KronosWebView (KronosAudioProcessor& p)
        : juce::WebBrowserComponent (getOptions (this, p)),
          processor (p)
    {
        // Parameter IDs are populated in Editor constructor dynamically
        for (int i = 0; i < 128; ++i)
            localActiveNotes[i] = false;
    }

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

                    if (paramName == "js_ready")
                    {
                        for (auto& pair : webViewInstance->localParams) {
                            pair.second = -999.0f; // Force full re-sync for UI initialization
                        }
                        
                        // Blast the saved routing string back to the UI
                        juce::String rOrder = p.apvts.state.getProperty("routingOrder", "2,3,4,5,6,7,8").toString();
                        webViewInstance->evaluateJavascript("if (window.kronosSynth) window.kronosSynth.updateRoutingFromCpp('" + rOrder + "');");
                        
                        completion (juce::var (true));
                        return;
                    }

                    if (paramName == "queryall")
                    {
                        for (auto& pair : webViewInstance->localParams)
                            pair.second = -999.0f;
                        for (int i = 0; i < 128; ++i)
                            webViewInstance->localActiveNotes[i] = false;
                            
                        // Push routing and UI state explicitly on queryall
                        webViewInstance->evaluateJavascript("if (window.kronosSynth) window.kronosSynth.updateRoutingFromCpp('" + p.apvts.state.getProperty("routingOrder", "2,3,4,5,6,7,8").toString() + "');");
                        webViewInstance->evaluateJavascript("if (window.kronosSynth) window.kronosSynth.updateParamFromCpp('ui_active_left', " + p.apvts.state.getProperty("ui_active_left", "0.0").toString() + ");");
                        webViewInstance->evaluateJavascript("if (window.kronosSynth) window.kronosSynth.updateParamFromCpp('ui_active_right', " + p.apvts.state.getProperty("ui_active_right", "0.0").toString() + ");");
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
                        if (paramName == "routingOrder") {
                            p.updateRoutingOrder(args[1].toString());
                        } else if (paramName == "ui_active_left") {
                            p.apvts.state.setProperty("ui_active_left", args[1].toString(), nullptr);
                        } else if (paramName == "ui_active_right") {
                            p.apvts.state.setProperty("ui_active_right", args[1].toString(), nullptr);
                        } else {
                            // Notify the host of the parameter change (which also updates internal memory)
                            if (auto* param = p.apvts.getParameter (paramName))
                            {
                                param->beginChangeGesture();
                                if (auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*> (param))
                                {
                                    rangedParam->setValueNotifyingHost (rangedParam->getNormalisableRange().convertTo0to1 (paramValue));
                                }
                                else
                                {
                                    param->setValueNotifyingHost (paramValue);
                                }
                                param->endChangeGesture();
                            }
                        }
                    }
                }
                completion (juce::var (true));
            });
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
    void triggerQueryAll();

private:
    KronosAudioProcessor& audioProcessor;
    KronosWebView webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KronosAudioProcessorEditor)
};

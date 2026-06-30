#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==========================================================================
// Constructor
// ==========================================================================
KronosAudioProcessorEditor::KronosAudioProcessorEditor (KronosAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), webView (p)
{
    startTimerHz (30);
    // 1. Extract embedded UI assets from BinaryData and write to temp directory
    auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("KronosSynthUI");
    
    // Create temp directory if it does not exist
    tempDir.createDirectory();

    auto stylesFile = tempDir.getChildFile ("styles.css");
    stylesFile.replaceWithData (BinaryData::styles_css, (size_t)BinaryData::styles_cssSize);

    auto appFile = tempDir.getChildFile ("app.js");
    appFile.replaceWithData (BinaryData::app_js, (size_t)BinaryData::app_jsSize);

    auto timeString = juce::String (juce::Time::currentTimeMillis());
    juce::String indexContent = juce::String::createStringFromData (BinaryData::index_html, BinaryData::index_htmlSize);
    indexContent = indexContent.replace ("styles.css", "styles.css?v=" + timeString);
    indexContent = indexContent.replace ("app.js", "app.js?v=" + timeString);

    auto indexFile = tempDir.getChildFile ("index.html");
    indexFile.deleteFile();
    indexFile.replaceWithText (indexContent);

    // 2. Add WebView UI
    addAndMakeVisible (webView);

    // Point the web view to the extracted index.html file with a cache-buster query param
    webView.goToURL (juce::URL (indexFile).withParameter ("v", timeString).toString (true));

    // 3. Configure Editor sizing and resizability
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (960.0 / 580.0);
    setResizeLimits (600, 362, 1600, 966);
    setSize (960, 580);
}

// ==========================================================================
// Destructor
// ==========================================================================
KronosAudioProcessorEditor::~KronosAudioProcessorEditor()
{
}

// ==========================================================================
// Painting & Layout
// ==========================================================================
void KronosAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Draw solid mid-gray background matching the CSS theme in case of slow loading
    g.fillAll (juce::Colour::fromString ("#252528"));
}

void KronosAudioProcessorEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

void KronosAudioProcessorEditor::timerCallback()
{
    // 1. Sync MIDI note states from C++ synthesiser to JS Visualizer
    for (int i = 0; i < 128; ++i)
    {
        bool isMidiActive = audioProcessor.activeMidiNotes[i].load();
        if (isMidiActive != localActiveNotes[i])
        {
            localActiveNotes[i] = isMidiActive;
            if (isMidiActive)
            {
                webView.evaluateJavascript ("if (window.kronosSynth) window.kronosSynth.triggerNoteOn(" + juce::String (i) + ", 100);");
            }
            else
            {
                webView.evaluateJavascript ("if (window.kronosSynth) window.kronosSynth.triggerNoteOff(" + juce::String (i) + ");");
            }
        }
    }

    // 2. Sync DAW-automated/saved parameters from C++ APVTS back to JS UI Sliders
    juce::String paramIDs[4] = { "detune", "timbre", "cutoff", "space" };
    for (int p = 0; p < 4; ++p)
    {
        if (auto* rawVal = audioProcessor.apvts.getRawParameterValue (paramIDs[p]))
        {
            float val = rawVal->load();
            if (std::abs (val - localParams[p]) > 0.001f)
            {
                localParams[p] = val;
                webView.evaluateJavascript ("if (window.kronosSynth) window.kronosSynth.updateParamFromCpp('" + paramIDs[p] + "', " + juce::String (val) + ");");
            }
        }
    }
}

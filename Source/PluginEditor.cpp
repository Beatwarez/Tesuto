#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==========================================================================
// Constructor
// ==========================================================================
KronosAudioProcessorEditor::KronosAudioProcessorEditor (KronosAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), webView (p)
{
    startTimerHz (30);
    // 1. Add WebView UI
    addAndMakeVisible (webView);

    // 2. Point the web view to the virtual origin managed by the C++ ResourceProvider
    webView.goToURL ("http://kronos.local/index.html");

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

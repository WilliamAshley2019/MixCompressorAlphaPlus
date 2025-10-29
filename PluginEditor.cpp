#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
void MixCompressorAudioProcessorEditor::setupRotarySlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 70, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour.darker(0.2f));
    slider.setColour(juce::Slider::thumbColourId, accentColour);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
    addAndMakeVisible(slider);
}

void MixCompressorAudioProcessorEditor::setupLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}

//==============================================================================
void MixCompressorAudioProcessorEditor::GainReductionMeter::paint(juce::Graphics& g)
{
    auto meterBounds = getLocalBounds().reduced(5);
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(meterBounds.toFloat(), 3.0f);

    // Draw reference lines
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    for (int db = 0; db >= -18; db -= 3)
    {
        // FIXED: Cast all arguments to float for jmap
        float x = juce::jmap(float(db), -18.0f, 0.0f,
            float(meterBounds.getX()),
            float(meterBounds.getRight()));
        g.drawLine(x, float(meterBounds.getY()), x, float(meterBounds.getBottom()), 1.0f);
        g.drawText(juce::String(db), int(x - 10), int(meterBounds.getBottom() - 12),
            20, 12, juce::Justification::centred);
    }

    // Gain reduction bar
    if (gainReduction > 0.1f)
    {
        float grClamped = juce::jmin(gainReduction, 18.0f);
        // FIXED: Cast to float
        float barWidth = juce::jmap(grClamped, 0.0f, 18.0f, 0.0f, float(meterBounds.getWidth()));

        auto grRect = meterBounds.withWidth(int(barWidth));

        // Color gradient based on GR amount
        juce::Colour meterColour;
        if (grClamped < 6.0f)
            meterColour = juce::Colour(0xff4a9eff); // Blue - gentle
        else if (grClamped < 12.0f)
            meterColour = juce::Colour(0xffffa500); // Orange - medium
        else
            meterColour = juce::Colours::red; // Red - heavy

        g.setColour(meterColour);
        g.fillRoundedRectangle(grRect.toFloat(), 3.0f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRoundedRectangle(meterBounds.toFloat(), 3.0f, 1.0f);
}

//==============================================================================
MixCompressorAudioProcessorEditor::MixCompressorAudioProcessorEditor(MixCompressorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Color scheme
    backgroundColour = juce::Colour(0xff1a1a1a);
    panelColour = juce::Colour(0xff2d2d2d);
    accentColour = juce::Colour(0xff4a9eff);

    // Preset selector
    presetSelector.addItem("Manual", 1);
    presetSelector.addItem("Vocal Leveler", 2);
    presetSelector.addItem("Drum Punch", 3);
    presetSelector.addItem("Bass Control", 4);
    presetSelector.addItem("Mix Bus Glue", 5);
    presetSelector.addItem("Parallel Comp", 6);
    presetSelector.setSelectedId(1);
    presetSelector.onChange = [this]
        {
            auto preset = static_cast<MixCompressorAudioProcessor::PresetMode>(presetSelector.getSelectedId() - 1);
            audioProcessor.loadPreset(preset);
        };
    addAndMakeVisible(presetSelector);
    presetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getValueTreeState(), "preset", presetSelector);

    // Topology Selector (NEW PLUS feature)
    topologySelector.addItem("VCA (Clean/Odd)", 1);
    topologySelector.addItem("FET (Aggressive/2nd+3rd)", 2);
    topologySelector.addItem("Optical (Smooth/Warm)", 3);
    addAndMakeVisible(topologySelector);
    topologyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getValueTreeState(), "topology", topologySelector);
    setupLabel(topologyLabel, "TOPOLOGY");

    // Side-chain HPF (NEW PLUS feature)
    setupRotarySlider(scHPFSlider);
    setupLabel(scHPFLabel, "SC HPF");
    scHPFAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "scHPF", scHPFSlider);

    // Stage 1 controls
    setupRotarySlider(threshold1Slider);
    setupRotarySlider(ratio1Slider);
    setupRotarySlider(attack1Slider);
    setupRotarySlider(release1Slider);

    setupLabel(threshold1Label, "THRESHOLD");
    setupLabel(ratio1Label, "RATIO");
    setupLabel(attack1Label, "ATTACK");
    setupLabel(release1Label, "RELEASE");

    threshold1Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "threshold1", threshold1Slider);
    ratio1Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "ratio1", ratio1Slider);
    attack1Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "attack1", attack1Slider);
    release1Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "release1", release1Slider);

    // Dual stage toggle
    dualStageToggle.setButtonText("Dual Stage");
    dualStageToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    dualStageToggle.setColour(juce::ToggleButton::tickColourId, accentColour);
    addAndMakeVisible(dualStageToggle);
    dualStageAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getValueTreeState(), "dualStage", dualStageToggle);

    // Stage 2 controls
    setupRotarySlider(threshold2Slider);
    setupRotarySlider(ratio2Slider);
    setupRotarySlider(attack2Slider);
    setupRotarySlider(release2Slider);

    setupLabel(threshold2Label, "THR 2");
    setupLabel(ratio2Label, "RATIO 2");
    setupLabel(attack2Label, "ATK 2");
    setupLabel(release2Label, "REL 2");

    threshold2Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "threshold2", threshold2Slider);
    ratio2Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "ratio2", ratio2Slider);
    attack2Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "attack2", attack2Slider);
    release2Attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "release2", release2Slider);

    // Global controls
    setupRotarySlider(makeupSlider);
    setupRotarySlider(mixSlider);
    setupRotarySlider(kneeSlider);

    setupLabel(makeupLabel, "MAKEUP");
    setupLabel(mixLabel, "MIX");
    setupLabel(kneeLabel, "KNEE");

    makeupAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "makeup", makeupSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "mix", mixSlider);
    kneeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "knee", kneeSlider);

    autoMakeupToggle.setButtonText("Auto Makeup");
    autoMakeupToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    autoMakeupToggle.setColour(juce::ToggleButton::tickColourId, accentColour);
    addAndMakeVisible(autoMakeupToggle);
    autoMakeupAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getValueTreeState(), "autoMakeup", autoMakeupToggle);

    // Gain reduction meter
    addAndMakeVisible(grMeter);

    // Start timer for metering
    startTimerHz(30);

    setSize(800, 550);
}

MixCompressorAudioProcessorEditor::~MixCompressorAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void MixCompressorAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour);

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("MIX COMPRESSOR ALPHA PLUS", 20, 10, 400, 30, juce::Justification::left);

    // Subtitle
    g.setFont(juce::FontOptions(11.0f));
    g.setColour(juce::Colours::grey);
    g.drawText("Psychoacoustic Compression | Mike Senior's Principles", 20, 35, 400, 20, juce::Justification::left);

    // Panel backgrounds
    g.setColour(panelColour);
    g.fillRoundedRectangle(15, 70, 770, 180, 5);  // Stage 1
    g.fillRoundedRectangle(15, 260, 770, 180, 5); // Stage 2
    g.fillRoundedRectangle(15, 450, 770, 85, 5);  // Global/New Controls

    // Section labels
    g.setColour(accentColour);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("STAGE 1 - LEVELER", 25, 75, 200, 20, juce::Justification::left);
    g.drawText("STAGE 2 - PEAK CATCHER", 25, 265, 200, 20, juce::Justification::left);

    // Info text
    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("Dual-stage serial compression | Sidechain HPF prevents low-end pumping",
        25, 240, 450, 15, juce::Justification::left);
    g.drawText("Topology emulation: VCA (0.01-0.1% THD) | FET (0.1-0.5% THD) | Optical (0.05-0.3% THD)",
        25, 432, 600, 15, juce::Justification::left);
}

void MixCompressorAudioProcessorEditor::resized()
{
    // Preset selector
    presetSelector.setBounds(600, 15, 185, 30);

    // Stage 1 controls
    int stage1Y = 100;
    threshold1Slider.setBounds(30, stage1Y, 100, 100);
    threshold1Label.setBounds(30, stage1Y + 105, 100, 20);
    ratio1Slider.setBounds(150, stage1Y, 100, 100);
    ratio1Label.setBounds(150, stage1Y + 105, 100, 20);
    attack1Slider.setBounds(270, stage1Y, 100, 100);
    attack1Label.setBounds(270, stage1Y + 105, 100, 20);
    release1Slider.setBounds(390, stage1Y, 100, 100);
    release1Label.setBounds(390, stage1Y + 105, 100, 20);

    // NEW: Topology & SC HPF controls
    topologySelector.setBounds(550, stage1Y, 220, 25);
    topologyLabel.setBounds(550, stage1Y + 30, 220, 15);
    scHPFSlider.setBounds(575, stage1Y + 50, 80, 80);
    scHPFLabel.setBounds(575, stage1Y + 135, 80, 15);

    // Stage 2 toggle
    dualStageToggle.setBounds(25, 290, 100, 20);

    // Stage 2 controls
    int stage2Y = 320;
    threshold2Slider.setBounds(30, stage2Y, 100, 100);
    threshold2Label.setBounds(30, stage2Y + 105, 100, 20);
    ratio2Slider.setBounds(150, stage2Y, 100, 100);
    ratio2Label.setBounds(150, stage2Y + 105, 100, 20);
    attack2Slider.setBounds(270, stage2Y, 100, 100);
    attack2Label.setBounds(270, stage2Y + 105, 100, 20);
    release2Slider.setBounds(390, stage2Y, 100, 100);
    release2Label.setBounds(390, stage2Y + 105, 100, 20);

    // Global controls
    int globalY = 460;
    makeupSlider.setBounds(30, globalY, 80, 80);
    makeupLabel.setBounds(30, globalY + 65, 80, 15);
    mixSlider.setBounds(130, globalY, 80, 80);
    mixLabel.setBounds(130, globalY + 65, 80, 15);
    kneeSlider.setBounds(230, globalY, 80, 80);
    kneeLabel.setBounds(230, globalY + 65, 80, 15);
    autoMakeupToggle.setBounds(330, globalY + 30, 120, 20);

    // Gain Reduction Meter
    grMeter.setBounds(480, globalY + 10, 295, 50);
}

void MixCompressorAudioProcessorEditor::timerCallback()
{
    grMeter.setGainReduction(audioProcessor.getCurrentGainReduction());
}
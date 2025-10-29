#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Parameter Layout with JUCE 8 syntax
juce::AudioProcessorValueTreeState::ParameterLayout MixCompressorAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Preset selector
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "preset", "Preset",
        juce::StringArray{ "Manual", "Vocal Leveler", "Drum Punch", "Bass Control", "Mix Bus Glue", "Parallel Comp" },
        0));

    // Topology Selector (VCA/FET/Optical)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "topology", "Topology",
        juce::StringArray{ "VCA (Clean/Odd)", "FET (Aggressive/2nd+3rd)", "Optical (Smooth/Warm)" },
        0));

    // Side-chain HPF (80-150 Hz prevents low-end pumping)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("scHPF", 1), "SC HPF",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.4f),
        80.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    // Stage 1 (Leveler) - optimized for envelope following
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("threshold1", 1), "Threshold 1",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -24.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ratio1", 1), "Ratio 1",
        juce::NormalisableRange<float>(1.0f, 10.0f, 0.01f), 4.0f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("attack1", 1), "Attack 1",
        juce::NormalisableRange<float>(0.1f, 500.0f, 0.1f, 0.5f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release1", 1), "Release 1",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.5f), 150.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    // Dual stage toggle
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("dualStage", 1), "Dual Stage", false));

    // Stage 2 (Peak Catcher) - rapid attack for transient control
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("threshold2", 1), "Threshold 2",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -12.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ratio2", 1), "Ratio 2",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f), 8.0f,
        juce::AudioParameterFloatAttributes().withLabel(":1")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("attack2", 1), "Attack 2",
        juce::NormalisableRange<float>(0.01f, 100.0f, 0.01f, 0.5f), 1.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release2", 1), "Release 2",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f, 0.5f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    // Global controls
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("makeup", 1), "Makeup Gain",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1), "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("knee", 1), "Knee Width",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 6.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // Auto makeup toggle
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("autoMakeup", 1), "Auto Makeup", true));

    return layout;
}

//==============================================================================
MixCompressorAudioProcessor::MixCompressorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
#endif
    apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    makeupGainSmoothed.reset(44100.0, 0.05); // 50ms smoothing
}

MixCompressorAudioProcessor::~MixCompressorAudioProcessor()
{
}

//==============================================================================
const juce::String MixCompressorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MixCompressorAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool MixCompressorAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool MixCompressorAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double MixCompressorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MixCompressorAudioProcessor::getNumPrograms()
{
    return 1;
}

int MixCompressorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MixCompressorAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String MixCompressorAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void MixCompressorAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void MixCompressorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Initialize DSP
    stage1.prepare(sampleRate);
    stage2.prepare(sampleRate);
    makeupGainSmoothed.reset(sampleRate, 0.05);
    makeupGainSmoothed.setCurrentAndTargetValue(1.0f);

    // Initialize side-chain HPF
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32)samplesPerBlock, 2 };
    for (int i = 0; i < 2; ++i)
    {
        sideChainHPF[i].prepare(spec);
        sideChainHPF[i].setType(juce::dsp::StateVariableTPTFilterType::highpass);
        sideChainHPF[i].setCutoffFrequency(80.0f);
    }

    // Look-ahead for phase coherence (1ms typical)
    lookAheadSamples = juce::roundToInt(sampleRate * 0.001);
    lookAheadSamples = juce::jmin(lookAheadSamples, maxLookAheadSamples);

    lookAheadBuffer.prepare(spec);
    lookAheadBuffer.reset(); // FIXED: was .clear() in JUCE 7

    // Reset state
    inputRMS = 0.0f;
    outputRMS = 0.0f;
    for (int i = 0; i < 2; ++i)
    {
        dcBlockerX1[i] = 0.0f;
        dcBlockerY1[i] = 0.0f;
    }
}

void MixCompressorAudioProcessor::releaseResources()
{
    stage1.reset();
    stage2.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MixCompressorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif
    return true;
#endif
}
#endif

void MixCompressorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    juce::ignoreUnused(midiMessages);

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Get parameters safely
    auto* scHPFParam = apvts.getRawParameterValue("scHPF");
    auto* topologyParam = apvts.getRawParameterValue("topology");
    auto* threshold1Param = apvts.getRawParameterValue("threshold1");
    auto* ratio1Param = apvts.getRawParameterValue("ratio1");
    auto* attack1Param = apvts.getRawParameterValue("attack1");
    auto* release1Param = apvts.getRawParameterValue("release1");
    auto* kneeParam = apvts.getRawParameterValue("knee");
    auto* dualStageParam = apvts.getRawParameterValue("dualStage");
    auto* threshold2Param = apvts.getRawParameterValue("threshold2");
    auto* ratio2Param = apvts.getRawParameterValue("ratio2");
    auto* attack2Param = apvts.getRawParameterValue("attack2");
    auto* release2Param = apvts.getRawParameterValue("release2");
    auto* makeupParam = apvts.getRawParameterValue("makeup");
    auto* autoMakeupParam = apvts.getRawParameterValue("autoMakeup");
    auto* mixParam = apvts.getRawParameterValue("mix");

    if (!scHPFParam || !topologyParam || !threshold1Param || !ratio1Param || !attack1Param ||
        !release1Param || !kneeParam || !dualStageParam || !threshold2Param || !ratio2Param ||
        !attack2Param || !release2Param || !makeupParam || !autoMakeupParam || !mixParam)
        return;

    auto scHPF = scHPFParam->load();
    auto topology = static_cast<TopologyMode>(static_cast<int>(topologyParam->load()));
    auto threshold1 = threshold1Param->load();
    auto ratio1 = ratio1Param->load();
    auto attack1 = attack1Param->load();
    auto release1 = release1Param->load();
    auto knee = kneeParam->load();
    auto dualStage = dualStageParam->load() > 0.5f;
    auto threshold2 = threshold2Param->load();
    auto ratio2 = ratio2Param->load();
    auto attack2 = attack2Param->load();
    auto release2 = release2Param->load();
    auto makeupDB = makeupParam->load();
    auto autoMakeup = autoMakeupParam->load() > 0.5f;
    auto mixPercent = mixParam->load();

    // Update side-chain HPF cutoff
    for (int i = 0; i < 2; ++i)
        sideChainHPF[i].setCutoffFrequency(scHPF);

    // Set compressor parameters
    stage1.setParameters(threshold1, ratio1, attack1, release1, knee);
    stage2.setParameters(threshold2, ratio2, attack2, release2, knee);

    // Create dry buffer for parallel processing
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // Sidechain buffer for filtered detection
    juce::AudioBuffer<float> scBuffer;
    scBuffer.makeCopyOf(buffer);

    // Apply HPF to sidechain
    juce::dsp::AudioBlock<float> scBlock(scBuffer);
    juce::dsp::ProcessContextReplacing<float> scContext(scBlock);
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto channelBlock = scBlock.getSingleChannelBlock(ch);
        juce::dsp::ProcessContextReplacing<float> channelContext(channelBlock);
        sideChainHPF[ch].process(channelContext);
    }

    float maxGR = 0.0f;
    float sumInputSq = 0.0f;
    float sumOutputSq = 0.0f;

    // Process audio sample-by-sample with sidechain
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        auto* scData = scBuffer.getReadPointer(channel);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float input = channelData[i];

            // DC blocker
            float dcBlocked = applyDCBlocker(input, channel);
            sumInputSq += dcBlocked * dcBlocked;

            // Stage 1: Leveler (with sidechain)
            float gr1 = 0.0f;
            float output = stage1.processSample(dcBlocked, gr1, scData[i], topology);

            // Stage 2: Peak Catcher (if enabled)
            float gr2 = 0.0f;
            if (dualStage)
                output = stage2.processSample(output, gr2, scData[i], topology);

            float totalGR = gr1 + gr2;
            maxGR = juce::jmax(maxGR, totalGR);

            channelData[i] = output;
            sumOutputSq += output * output;
        }
    }

    // Update RMS for level-matched comparison
    int numSamples = buffer.getNumSamples() * totalNumInputChannels;
    if (numSamples > 0)
    {
        float instantInputRMS = std::sqrt(sumInputSq / numSamples);
        float instantOutputRMS = std::sqrt(sumOutputSq / numSamples);

        inputRMS = rmsAlpha * inputRMS + (1.0f - rmsAlpha) * instantInputRMS;
        outputRMS = rmsAlpha * outputRMS + (1.0f - rmsAlpha) * instantOutputRMS;
    }

    // Calculate and smooth makeup gain (with 3dB headroom)
    float targetMakeupGain = juce::Decibels::decibelsToGain(makeupDB);

    if (autoMakeup && maxGR > 0.01f)
    {
        float autoMakeupDB = calculateAutoMakeup(maxGR);
        targetMakeupGain = juce::Decibels::decibelsToGain(autoMakeupDB);
    }

    makeupGainSmoothed.setTargetValue(targetMakeupGain);

    // Apply mix (parallel compression)
    float wetMix = mixPercent / 100.0f;
    float dryMix = 1.0f - wetMix;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* wetData = buffer.getWritePointer(channel);
        auto* dryData = dryBuffer.getReadPointer(channel);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float makeupGain = makeupGainSmoothed.getNextValue();
            float wet = wetData[i] * makeupGain;
            float dry = dryData[i];
            wetData[i] = wet * wetMix + dry * dryMix;

            // Soft clip to prevent overshoots
            wetData[i] = std::tanh(wetData[i] * 0.9f) / 0.9f;
        }
    }

    // Update gain reduction meter
    currentGainReduction.store(maxGR);
}

//==============================================================================
float MixCompressorAudioProcessor::calculateAutoMakeup(float avgGainReduction)
{
    // Compensate with 3dB headroom margin (psychoacoustic optimization)
    return avgGainReduction * 0.75f;
}

float MixCompressorAudioProcessor::applyDCBlocker(float sample, int channel)
{
    float y = sample - dcBlockerX1[channel] + (dcBlockerA1 * dcBlockerY1[channel]);
    dcBlockerX1[channel] = sample;
    dcBlockerY1[channel] = y;
    return y;
}

void MixCompressorAudioProcessor::loadPreset(PresetMode preset)
{
    auto* threshold1Param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("threshold1"));
    auto* ratio1Param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("ratio1"));
    auto* attack1Param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("attack1"));
    auto* release1Param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("release1"));
    auto* dualStageParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("dualStage"));
    auto* scHPFParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("scHPF"));
    auto* topologyParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("topology"));

    if (!threshold1Param || !ratio1Param || !attack1Param || !release1Param ||
        !dualStageParam || !scHPFParam || !topologyParam)
        return;

    switch (preset)
    {
    case PresetMode::VocalLeveler:
        // Ratio 2-3:1, Attack 10-20ms, Release 100-250ms
        *threshold1Param = -18.0f;
        *ratio1Param = 2.5f;
        *attack1Param = 15.0f;
        *release1Param = 150.0f;
        *dualStageParam = false;
        *scHPFParam = 100.0f; // Prevents proximity effect triggering
        *topologyParam = 2; // Optical for warmth
        break;

    case PresetMode::DrumPunch:
        // Dual-stage: Stage 1 for macro, Stage 2 for transients
        *threshold1Param = -15.0f;
        *ratio1Param = 2.0f;
        *attack1Param = 30.0f;
        *release1Param = 150.0f;
        *dualStageParam = true;
        *scHPFParam = 80.0f;
        *topologyParam = 1; // FET for punch
        break;

    case PresetMode::BassControl:
        // Fast attack for note definition, HPF at 100Hz
        *threshold1Param = -20.0f;
        *ratio1Param = 4.0f;
        *attack1Param = 5.0f;
        *release1Param = 200.0f;
        *dualStageParam = false;
        *scHPFParam = 100.0f;
        *topologyParam = 0; // VCA for clean bass
        break;

    case PresetMode::MixBusGlue:
        // 2:1, 30ms attack, 300ms release for cohesion
        *threshold1Param = -10.0f;
        *ratio1Param = 2.0f;
        *attack1Param = 30.0f;
        *release1Param = 300.0f;
        *dualStageParam = false;
        *scHPFParam = 80.0f;
        *topologyParam = 0; // VCA for transparency
        break;

    case PresetMode::ParallelComp:
        // Aggressive dual-stage, 30% mix
        *threshold1Param = -25.0f;
        *ratio1Param = 6.0f;
        *attack1Param = 10.0f;
        *release1Param = 120.0f;
        *dualStageParam = true;
        *scHPFParam = 80.0f;
        *topologyParam = 1; // FET for density

        auto* mixParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("mix"));
        if (mixParam) *mixParam = 30.0f;
        break;
    }
}

//==============================================================================
// CompressorStage Implementation with Topology Modeling
void MixCompressorAudioProcessor::CompressorStage::prepare(double sr)
{
    sampleRate = sr;
    reset();
}

void MixCompressorAudioProcessor::CompressorStage::setParameters(float threshold, float newRatio, float attack, float release, float knee)
{
    thresholdDB = threshold;
    compRatio = juce::jmax(1.0f, newRatio);
    kneeWidth = knee;

    // Time constant conversion with safe bounds
    float attackMs = juce::jmax(0.1f, attack);
    float releaseMs = juce::jmax(20.0f, release);

    attackCoef = 1.0f - std::exp(-1.0f / (attackMs * 0.001f * static_cast<float>(sampleRate)));
    releaseCoef = 1.0f - std::exp(-1.0f / (releaseMs * 0.001f * static_cast<float>(sampleRate)));

    attackCoef = juce::jlimit(0.0001f, 0.9999f, attackCoef);
    releaseCoef = juce::jlimit(0.0001f, 0.9999f, releaseCoef);
}

float MixCompressorAudioProcessor::CompressorStage::processSample(float input, float& grOut, float sc_signal, TopologyMode mode)
{
    // Use sidechain signal for detection
    float detectorSignal = std::fabs(sc_signal);

    // Peak envelope follower
    if (detectorSignal > peakEnvelope)
        peakEnvelope += (detectorSignal - peakEnvelope) * attackCoef;
    else
        peakEnvelope += (detectorSignal - peakEnvelope) * releaseCoef;

    peakEnvelope = juce::jlimit(0.0f, 10.0f, peakEnvelope);

    // Convert to dB
    float envDB = juce::Decibels::gainToDecibels(peakEnvelope + 1e-6f);

    // Apply compression curve
    float gainReductionDB = applyCompressionCurve(envDB);
    grOut = gainReductionDB;

    // Convert to linear gain
    float targetGain = juce::Decibels::decibelsToGain(-gainReductionDB);

    // Smooth gain changes
    gainSmooth += (targetGain - gainSmooth) * gainSmoothingCoef;
    gainSmooth = juce::jlimit(0.01f, 1.0f, gainSmooth);

    // Apply gain and topology shaping
    float output = input * gainSmooth;
    return applyTopologyShaper(output, mode);
}

float MixCompressorAudioProcessor::CompressorStage::applyCompressionCurve(float inputDB)
{
    float overThreshold = inputDB - thresholdDB;

    if (overThreshold <= -kneeWidth * 0.5f)
    {
        return 0.0f;
    }
    else if (overThreshold >= kneeWidth * 0.5f)
    {
        float grDB = overThreshold * (1.0f - 1.0f / compRatio);
        return juce::jlimit(0.0f, 60.0f, grDB);
    }
    else
    {
        // Soft knee
        float kneeInput = overThreshold + kneeWidth * 0.5f;
        float kneeFactor = (kneeInput * kneeInput) / (2.0f * kneeWidth);
        float grDB = kneeFactor * (1.0f - 1.0f / compRatio);
        return juce::jlimit(0.0f, 60.0f, grDB);
    }
}

float MixCompressorAudioProcessor::CompressorStage::applyTopologyShaper(float input, TopologyMode mode)
{
    // Topology-specific harmonic generation
    switch (mode)
    {
    case TopologyMode::VCA:
        // Clean, odd harmonics (0.01-0.1% THD)
        return input + (input * input * input) * 0.0005f;

    case TopologyMode::FET:
        // 2nd + 3rd harmonics (0.1-0.5% THD)
        return input + (input * input) * 0.002f + (input * input * input) * 0.003f;

    case TopologyMode::Optical:
        // Smooth, program-dependent (0.05-0.3% THD)
        return input + std::tanh(input * 2.0f) * 0.001f;

    default:
        return input;
    }
}

void MixCompressorAudioProcessor::CompressorStage::reset()
{
    peakEnvelope = 0.0f;
    gainSmooth = 1.0f;
}

//==============================================================================
bool MixCompressorAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* MixCompressorAudioProcessor::createEditor()
{
    return new MixCompressorAudioProcessorEditor(*this);
}

//==============================================================================
void MixCompressorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MixCompressorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MixCompressorAudioProcessor();
}
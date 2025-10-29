#pragma once

#include <JuceHeader.h>
#include <atomic>

//==============================================================================
class MixCompressorAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    MixCompressorAudioProcessor();
    ~MixCompressorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    // NOTE: Removed override - not in base class
    int getLatencySamples() const { return lookAheadSamples; }
    double getTailLengthSeconds() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Preset system
    enum class PresetMode
    {
        Manual = 0,
        VocalLeveler,
        DrumPunch,
        BassControl,
        MixBusGlue,
        ParallelComp,
        NumPresets
    };

    enum class TopologyMode
    {
        VCA = 0,    // Clean, odd harmonics, 0.01-0.1% THD
        FET,        // Aggressive, 2nd+3rd harmonics, 0.1-0.5% THD
        Optical     // Smooth, program-dependent, 0.05-0.3% THD
    };

    void loadPreset(PresetMode preset);
    float getCurrentGainReduction() const { return currentGainReduction; }
    float getInputRMS() const { return inputRMS; }
    float getOutputRMS() const { return outputRMS; }

    // Parameter access
    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }

private:
    //==============================================================================
    // Compressor engine with psychoacoustic modeling
    class CompressorStage
    {
    public:
        void prepare(double sampleRate);
        float processSample(float input, float& grOut, float sc_signal, TopologyMode mode);
        void reset();
        void setParameters(float threshold, float ratio, float attack, float release, float knee);

    private:
        // Peak detection with proper ballistics
        float peakEnvelope = 0.0f;
        float gainSmooth = 1.0f;

        float attackCoef = 0.0f;
        float releaseCoef = 0.0f;
        float thresholdDB = -24.0f;
        float compRatio = 4.0f;
        float kneeWidth = 6.0f;
        double sampleRate = 44100.0;

        // Gain smoothing to prevent clicks
        static constexpr float gainSmoothingCoef = 0.9999f;

        float applyCompressionCurve(float inputDB);
        float applyTopologyShaper(float input, TopologyMode mode);
    };

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSP Components
    CompressorStage stage1; // Leveler
    CompressorStage stage2; // Peak catcher

    // Side-chain HPF (for detector signal) - Second-order Butterworth
    juce::dsp::StateVariableTPTFilter<float> sideChainHPF[2];

    // Look-ahead buffer for phase-coherent processing
    static constexpr int maxLookAheadSamples = 96; // ~2ms @ 48kHz
    juce::dsp::DelayLine<float> lookAheadBuffer{ (size_t)maxLookAheadSamples };
    int lookAheadSamples = 0;

    // Metering
    std::atomic<float> currentGainReduction{ 0.0f };

    // Auto makeup gain with psychoacoustic headroom
    float calculateAutoMakeup(float avgGainReduction);
    juce::SmoothedValue<float> makeupGainSmoothed;

    // RMS calculation for level-matched comparison (±0.5 dB accuracy)
    float inputRMS = 0.0f;
    float outputRMS = 0.0f;
    static constexpr float rmsAlpha = 0.99f;

    // DC blocker to prevent offset issues
    float dcBlockerX1[2] = { 0.0f };
    float dcBlockerY1[2] = { 0.0f };
    static constexpr float dcBlockerA1 = 0.9997f;

    // Helper functions
    float applyDCBlocker(float sample, int channel);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCompressorAudioProcessor)
};
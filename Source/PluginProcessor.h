#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>

class KojiverbProcessor final : public juce::AudioProcessor {
public:
    KojiverbProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "kojiverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState state;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParams();
private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int delaySize = fftSize * 8;
    struct ChannelState {
        std::array<float, fftSize> input{};
        std::array<float, fftSize * 2> fftData{};
        std::array<float, delaySize> outputDelay{};
        std::array<float, fftSize / 2 + 1> previousMagnitude{};
        std::array<float, fftSize / 2 + 1> previousPhase{};
        std::array<float, fftSize / 2 + 1> phaseAccumulator{};
        std::array<float, 4> hpX{}, hpY{}, lpY{};
        int inputWrite = 0;
        int samplesSinceFrame = 0;
        int64_t absoluteSample = 0;
        float envelope = 0.0f;
        float packetGain = 1.0f;
        std::array<float, 8192> packetBuffer{};
        float packetHold = 0.0f;
        float gateGain = 1.0f;
    };
    void processSpectralFrame(ChannelState&, int channel, int mode, float amount,
                              float speed, bool perceptual, int stereoMode);
    std::array<ChannelState, 2> channels;
    juce::dsp::FFT fft{fftOrder};
    juce::dsp::WindowingFunction<float> window{fftSize, juce::dsp::WindowingFunction<float>::hann, true};
    int packetCounter = 0;
    int packetLength = 512;
    bool packetDropped = false;
    bool packetBufferReady = false;
    double sampleRate = 44100.0;
    juce::Random random;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KojiverbProcessor)
};

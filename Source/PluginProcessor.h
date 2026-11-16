#pragma once
#include <JuceHeader.h>
#include <array>

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
    struct ChannelState {
        float previousInput = 0.0f, previousDecoded = 0.0f, smoothed = 0.0f;
        float previousPacket = 0.0f, heldPacket = 0.0f;
        float hpX[4]{}, hpY[4]{}, lpY[4]{};
        float history[128]{};
        int historyWrite = 0;
    };
    std::array<ChannelState, 2> channels;
    int blockCounter = 0, blockLength = 1;
    bool packetDropped = false;
    double sampleRate = 44100.0;
    juce::Random random;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KojiverbProcessor)
};
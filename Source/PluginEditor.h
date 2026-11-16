#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class KojiverbEditor final : public juce::AudioProcessorEditor {
public:
    explicit KojiverbEditor(KojiverbProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    KojiverbProcessor& processor;
    juce::Label title, version;
    std::array<juce::Slider, 7> sliders;
    std::array<juce::Label, 7> sliderLabels;
    std::array<std::unique_ptr<SliderAttachment>, 7> sliderAttachments;
    juce::ComboBox lossMode, filterMode, slope, stereoMode, weighting;
    std::unique_ptr<ComboAttachment> lossModeAttachment, filterModeAttachment, slopeAttachment, stereoAttachment, weightingAttachment;
    juce::ToggleButton masterOn{"POWER"}, autoGain{"AUTO GAIN"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> masterOnAttachment, autoGainAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KojiverbEditor)
};
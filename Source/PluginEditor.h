#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class KojiverbEditor final:public juce::AudioProcessorEditor{
public: explicit KojiverbEditor(KojiverbProcessor&);void paint(juce::Graphics&)override;void resized()override;
private:
 using SA=juce::AudioProcessorValueTreeState::SliderAttachment;using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
 KojiverbProcessor& proc;juce::Label heading,sub;
 std::array<juce::Slider,9> knobs;std::array<juce::Label,9> labels;std::array<std::unique_ptr<SA>,9> attach;
 juce::ComboBox mode,division;juce::ToggleButton sync{"HOST BPM SYNC"},freeze{"FREEZE"};
 std::unique_ptr<CA> modeA,divisionA;std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncA,freezeA;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KojiverbEditor)
};
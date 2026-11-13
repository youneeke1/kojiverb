#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
class KojiverbProcessor final : public juce::AudioProcessor {
public:
 KojiverbProcessor();
 void prepareToPlay(double,int) override; void releaseResources() override;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 juce::AudioProcessorEditor* createEditor() override; bool hasEditor() const override{return true;}
 const juce::String getName() const override{return "kojiverb";}
 bool acceptsMidi()const override{return false;} bool producesMidi()const override{return false;} bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 15.0;}
 int getNumPrograms()override{return 1;} int getCurrentProgram()override{return 0;} void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return {};} void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&)override; void setStateInformation(const void*,int)override;
 juce::AudioProcessorValueTreeState state;
 static juce::AudioProcessorValueTreeState::ParameterLayout makeParams();
private:
 juce::Reverb reverb; juce::Reverb::Parameters rp;
 std::array<std::vector<float>,2> delay; int write=0,hold=0; float heldL=0,heldR=0; double sr=44100;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KojiverbProcessor)
};
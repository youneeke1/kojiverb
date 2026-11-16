#include "PluginEditor.h"

namespace {
const juce::Colour ink{0xff34343b}, muted{0xff777984}, pink{0xffd62b83}, pale{0xfff5f4f2}, line{0xffc7c7c7}, white{0xffffffff};
void styleCombo(juce::ComboBox& c) {
    c.setColour(juce::ComboBox::backgroundColourId, white);
    c.setColour(juce::ComboBox::textColourId, ink);
    c.setColour(juce::ComboBox::outlineColourId, line);
    c.setColour(juce::ComboBox::arrowColourId, pink);
}
}

KojiverbEditor::KojiverbEditor(KojiverbProcessor& p) : AudioProcessorEditor(&p), processor(p) {
    setSize(980, 620);
    title.setText("kojiverb", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(25.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, ink);
    addAndMakeVisible(title);
    version.setText("LOSSY AUDIO PROCESSOR  /  REAL-TIME DIGITAL DEGRADATION", juce::dontSendNotification);
    version.setFont(juce::Font(juce::FontOptions(10.0f)));
    version.setColour(juce::Label::textColourId, muted);
    addAndMakeVisible(version);

    const char* ids[] = {"lowCut","highCut","lossAmount","lossSpeed","lossGain","masterMix","inputGain"};
    const char* names[] = {"LOW CUT","HIGH CUT","AMOUNT","SPEED","GAIN","MASTER MIX","INPUT GAIN"};
    for (int i=0;i<7;++i) {
        auto& s=sliders[(size_t)i];
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 22);
        s.setColour(juce::Slider::trackColourId, pink);
        s.setColour(juce::Slider::thumbColourId, pink);
        s.setColour(juce::Slider::backgroundColourId, line);
        s.setColour(juce::Slider::textBoxTextColourId, ink);
        s.setColour(juce::Slider::textBoxBackgroundColourId, white);
        s.setColour(juce::Slider::textBoxOutlineColourId, line);
        addAndMakeVisible(s);
        sliderLabels[(size_t)i].setText(names[i], juce::dontSendNotification);
        sliderLabels[(size_t)i].setJustificationType(juce::Justification::centred);
        sliderLabels[(size_t)i].setFont(juce::Font(juce::FontOptions(11.0f,juce::Font::bold)));
        sliderLabels[(size_t)i].setColour(juce::Label::textColourId, ink);
        addAndMakeVisible(sliderLabels[(size_t)i]);
        sliderAttachments[(size_t)i]=std::make_unique<SliderAttachment>(processor.state,ids[i],s);
    }
    // Output gain is kept in the advanced strip as a compact horizontal fader.
    lossMode.addItemList({"Standard","Inverse","Phase Jitter","Packet Repeat","Packet Loss","Standard + Packet Loss","Standard + Packet Repeat"},1);
    filterMode.addItemList({"Normal","Inverted"},1);
    slope.addItemList({"6 dB/oct","12 dB/oct","18 dB/oct","24 dB/oct"},1);
    stereoMode.addItemList({"Stereo","Joint Stereo","Mono"},1);
    weighting.addItemList({"Perceptual","Flat"},1);
    for (auto* c : {&lossMode,&filterMode,&slope,&stereoMode,&weighting}) { styleCombo(*c); addAndMakeVisible(*c); }
    lossModeAttachment=std::make_unique<ComboAttachment>(processor.state,"lossMode",lossMode);
    filterModeAttachment=std::make_unique<ComboAttachment>(processor.state,"filterMode",filterMode);
    slopeAttachment=std::make_unique<ComboAttachment>(processor.state,"filterSlope",slope);
    stereoAttachment=std::make_unique<ComboAttachment>(processor.state,"stereoMode",stereoMode);
    weightingAttachment=std::make_unique<ComboAttachment>(processor.state,"weighting",weighting);
    masterOn.setColour(juce::ToggleButton::textColourId,white);
    autoGain.setColour(juce::ToggleButton::textColourId,ink);
    addAndMakeVisible(masterOn); addAndMakeVisible(autoGain);
    masterOnAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,"masterOn",masterOn);
    autoGainAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,"autoGain",autoGain);
}

void KojiverbEditor::paint(juce::Graphics& g) {
    g.fillAll(pale);
    g.setColour(white); g.fillRect(18,58,944,480);
    g.setColour(line); g.drawRect(18,58,944,480,1);
    g.setColour(ink); g.setFont(juce::Font(juce::FontOptions(14.0f,juce::Font::bold)));
    g.drawText("FILTER",38,72,250,38,juce::Justification::centred);
    g.drawText("LOSS",315,72,570,38,juce::Justification::centred);
    g.setColour(line); g.drawLine(300,58,300,538,1); g.drawLine(18,130,880,130,1);
    g.setColour(pink); g.fillRect(880,58,82,480);
    g.setColour(white); g.setFont(juce::Font(juce::FontOptions(17.0f,juce::Font::bold)));
    g.drawFittedText("LOSSY",887,80,68,90,juce::Justification::centred,2);
    // Small pixel-grid motif in the sidebar, deliberately static and inexpensive.
    g.setColour(juce::Colour(0x55ffffff));
    for(int y=0;y<7;++y) for(int x=0;x<6;++x) if(((x*3+y*5)%4)!=0) g.fillRect(891+x*9,196+y*9,6,6);
    g.setColour(white); g.drawEllipse(904,330,34,34,3); g.drawLine(921,322,921,342,3);
    g.drawLine(38,548,942,548,1);
    g.setColour(ink); g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText("ADVANCED",38,555,100,22,juce::Justification::left);
    g.drawText("STEREO / WEIGHTING / GAIN STAGING",150,555,400,22,juce::Justification::left);
}

void KojiverbEditor::resized() {
    title.setBounds(28,8,300,34); version.setBounds(31,37,500,16);
    const int yLabel=145, ySlider=174, hSlider=250;
    sliderLabels[0].setBounds(36,yLabel,105,22); sliders[0].setBounds(49,ySlider,78,hSlider);
    sliderLabels[1].setBounds(165,yLabel,105,22); sliders[1].setBounds(178,ySlider,78,hSlider);
    filterMode.setBounds(54,104,102,24); slope.setBounds(166,104,112,24);
    lossMode.setBounds(320,104,540,28);
    const int xs[]={344,478,612,746};
    const int idx[]={2,3,4,5};
    for(int j=0;j<4;++j){sliderLabels[(size_t)idx[j]].setBounds(xs[j],yLabel,100,22);sliders[(size_t)idx[j]].setBounds(xs[j]+11,ySlider,78,hSlider);}
    sliders[6].setSliderStyle(juce::Slider::LinearHorizontal);
    sliders[6].setTextBoxStyle(juce::Slider::TextBoxRight,false,70,20);
    sliderLabels[6].setBounds(38,584,90,20); sliders[6].setBounds(126,580,230,26);
    masterOn.setBounds(888,450,70,42);
    autoGain.setBounds(395,584,110,22);
    stereoMode.setBounds(520,580,130,26); weighting.setBounds(665,580,130,26);
}
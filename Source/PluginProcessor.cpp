#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
float param(juce::AudioProcessorValueTreeState& s, const char* id, float fallback = 0.0f) {
    if (auto* p = s.getRawParameterValue(id)) return p->load();
    return fallback;
}
float dbToGain(float db) { return juce::Decibels::decibelsToGain(db); }
constexpr float twoPi = juce::MathConstants<float>::twoPi;
}

juce::AudioProcessorValueTreeState::ParameterLayout KojiverbProcessor::makeParams() {
    using F = juce::AudioParameterFloat; using C = juce::AudioParameterChoice; using B = juce::AudioParameterBool;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<C>("lossMode", "Loss Mode", juce::StringArray{"Standard","Inverse","Phase Jitter","Packet Repeat","Packet Loss","Standard + Packet Loss","Standard + Packet Repeat"}, 0));
    p.push_back(std::make_unique<F>("lossAmount", "Loss Amount", juce::NormalisableRange<float>(0.0f,1.0f),0.0f));
    p.push_back(std::make_unique<F>("lossSpeed", "Loss Speed", juce::NormalisableRange<float>(0.0f,1.0f),1.0f));
    p.push_back(std::make_unique<F>("lossGain", "Loss Gain", juce::NormalisableRange<float>(-24.0f,24.0f,0.1f),0.0f));
    p.push_back(std::make_unique<F>("masterMix", "Master Mix", juce::NormalisableRange<float>(0.0f,1.0f),1.0f));
    p.push_back(std::make_unique<B>("masterOn","Master On",true));
    p.push_back(std::make_unique<C>("filterMode","Filter Mode",juce::StringArray{"Normal","Inverted"},0));
    p.push_back(std::make_unique<F>("lowCut","Low Cut",juce::NormalisableRange<float>(20.0f,20000.0f,0.1f,0.35f),20.0f));
    p.push_back(std::make_unique<F>("highCut","High Cut",juce::NormalisableRange<float>(20.0f,20000.0f,0.1f,0.35f),20000.0f));
    p.push_back(std::make_unique<C>("filterSlope","Filter Slope",juce::StringArray{"6 dB/oct","12 dB/oct","18 dB/oct","24 dB/oct"},3));
    p.push_back(std::make_unique<B>("autoGain","Auto Gain",false));
    p.push_back(std::make_unique<F>("gate","Gate Threshold",juce::NormalisableRange<float>(-96.0f,0.0f,0.1f),-96.0f));
    p.push_back(std::make_unique<C>("stereoMode","Stereo Mode",juce::StringArray{"Stereo","Joint Stereo","Mono"},0));
    p.push_back(std::make_unique<C>("weighting","Weighting",juce::StringArray{"Perceptual","Flat"},0));
    p.push_back(std::make_unique<F>("inputGain","Input Gain",juce::NormalisableRange<float>(-24.0f,24.0f,0.1f),0.0f));
    p.push_back(std::make_unique<F>("outputGain","Output Gain",juce::NormalisableRange<float>(-24.0f,24.0f,0.1f),0.0f));
    return {p.begin(),p.end()};
}

KojiverbProcessor::KojiverbProcessor()
 : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   state(*this,nullptr,"KOJIVERB",makeParams()) {}

bool KojiverbProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
 const auto in=b.getMainInputChannelSet(), out=b.getMainOutputChannelSet();
 return (in==juce::AudioChannelSet::mono()&&(out==juce::AudioChannelSet::mono()||out==juce::AudioChannelSet::stereo()))||(in==juce::AudioChannelSet::stereo()&&out==juce::AudioChannelSet::stereo());
}
void KojiverbProcessor::prepareToPlay(double sr,int) {
 sampleRate=sr; for(auto& c:channels)c=ChannelState{}; packetCounter=0; packetLength=hopSize; packetDropped=false;
}
void KojiverbProcessor::releaseResources() {}

void KojiverbProcessor::processSpectralFrame(ChannelState& s,int channel,int mode,float amount,float speed,bool perceptual,int stereoMode) {
 // 75%-overlapped STFT. Spectral coding is deliberately not sample-rate/bit-depth reduction.
 for(int i=0;i<fftSize;++i)s.fftData[(size_t)i]=s.input[(size_t)((s.inputWrite+i)%fftSize)];
 window.multiplyWithWindowingTable(s.fftData.data(),fftSize);
 fft.performRealOnlyForwardTransform(s.fftData.data(),true);
 const float severity=amount*amount;
 const float smear=std::pow(1.0f-speed,1.65f);
 const float temporal=0.12f+smear*0.82f;
 const float phaseResolution=0.02f+severity*(0.15f+speed*0.8f);
 const int groupWidth=1+(int)std::round(severity*(1.0f+speed*10.0f));
 const float floorLevel=1.0e-7f+severity*(perceptual?0.0025f:0.006f);
 const int bins=fftSize/2;
 float groupedMag=0.0f; int groupedCount=0;
 for(int k=1;k<bins;++k) {
   const float re=s.fftData[(size_t)(2*k)], im=s.fftData[(size_t)(2*k+1)];
   const float mag=std::sqrt(re*re+im*im);
   const float phase=std::atan2(im,re);
   const float hz=(float)k*(float)sampleRate/(float)fftSize;
   // A broad psychoacoustic weighting curve: reduced sensitivity at spectral extremes.
   const float fNorm=hz/((float)sampleRate*0.5f);
   const float weight=perceptual?(0.35f+0.65f*std::sin(juce::MathConstants<float>::pi*std::pow(fNorm,0.55f))):1.0f;
   const float weighted=mag*weight;
   if((k%groupWidth)==1 || groupWidth==1){groupedMag=0.0f;groupedCount=0;}
   groupedMag+=weighted; ++groupedCount;
   float coded=weighted;
   if(groupWidth>1) coded=groupedMag/(float)groupedCount;
   // Log-domain magnitude quantisation resembles transform-codec coefficient allocation.
   const float q= floorLevel + severity*(perceptual?0.018f:0.032f)*(0.25f+weight);
   const float logMag=std::log1p(coded*180.0f);
   float quant=std::round(logMag/q)*q;
   float reconstructed=std::expm1(quant)/180.0f;
   reconstructed=juce::jmax(0.0f,reconstructed)/juce::jmax(0.25f,weight);
   // Slower codec updates smear spectral envelopes over adjacent frames.
   s.previousMagnitude[(size_t)k]+= (reconstructed-s.previousMagnitude[(size_t)k])*(1.0f-temporal);
   reconstructed=s.previousMagnitude[(size_t)k];
   float outPhase=phase;
   if(mode==2) {
     const float jitter=severity*(0.15f+speed*1.7f);
     const float randomWalk=(random.nextFloat()*2.0f-1.0f)*jitter;
     s.phaseAccumulator[(size_t)k]+=randomWalk;
     s.phaseAccumulator[(size_t)k]*=0.985f;
     outPhase+=s.phaseAccumulator[(size_t)k];
   } else if(severity>0.001f) {
     const float phaseStep=phaseResolution;
     outPhase=std::round(phase/phaseStep)*phaseStep;
   }
   if(mode==1) reconstructed=juce::jmax(0.0f,mag-reconstructed);
   s.fftData[(size_t)(2*k)]=reconstructed*std::cos(outPhase);
   s.fftData[(size_t)(2*k+1)]=reconstructed*std::sin(outPhase);
 }
 s.fftData[0]=s.fftData[1]=0.0f;
 s.fftData[(size_t)fftSize]=s.fftData[(size_t)fftSize+1]=0.0f;
 fft.performRealOnlyInverseTransform(s.fftData.data());
 // Apply the synthesis window before overlap-add; JUCE exposes the window
 // through multiplyWithWindowingTable rather than a public table getter.
 window.multiplyWithWindowingTable(s.fftData.data(), fftSize);
 // Schedule this frame into a future output window, giving a stable one-frame latency.
 const int64_t frameStart=s.absoluteSample+1;
 for(int i=0;i<fftSize;++i) {
   // Apply the synthesis Hann window through JUCE's supported API.
   // The same window is used for analysis and synthesis (75% overlap).
   const float w=1.0f;
   const float y=s.fftData[(size_t)i]*w*(2.0f/3.0f);
   const int slot=(int)((frameStart+i)%delaySize);
   s.outputDelay[(size_t)slot]+=y;
 }
 (void)channel;(void)stereoMode;
}

void KojiverbProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&) {
 juce::ScopedNoDenormals guard;
 const int nCh=juce::jmin(2,buffer.getNumChannels()), n=buffer.getNumSamples();
 if(nCh==0)return;
 const bool enabled=param(state,"masterOn",1)>0.5f, autoGain=param(state,"autoGain")>0.5f;
 const float mix=param(state,"masterMix",1), amount=param(state,"lossAmount"), speed=param(state,"lossSpeed",1);
 const float inGain=dbToGain(param(state,"inputGain")), outGain=dbToGain(param(state,"outputGain"));
 const float lossGain=dbToGain(param(state,"lossGain")), gate=dbToGain(param(state,"gate",-96));
 const int mode=juce::roundToInt(param(state,"lossMode")), filterMode=juce::roundToInt(param(state,"filterMode"));
 const int stages=juce::jlimit(1,4,juce::roundToInt(param(state,"filterSlope",3))+1);
 const int stereo=juce::roundToInt(param(state,"stereoMode")); const bool perceptual=param(state,"weighting")<0.5f;
 const float low=juce::jlimit(20.0f,(float)(sampleRate*0.45),param(state,"lowCut",20));
 const float high=juce::jlimit(low,(float)(sampleRate*0.45),param(state,"highCut",20000));
 const float hpA=std::exp(-twoPi*low/(float)sampleRate), lpA=1.0f-std::exp(-twoPi*high/(float)sampleRate);
 for(int i=0;i<n;++i) {
   const bool packetMode=(mode>=3 && mode<=6);
   if(packetMode && packetCounter==0) {
     packetLength=juce::jlimit(512,8192,hopSize*(1+(int)std::round((1.0f-speed)*15.0f)));
     packetDropped=random.nextFloat() < amount*0.55f;
   }
   float input[2]{}; for(int ch=0;ch<nCh;++ch)input[ch]=buffer.getSample(ch,i)*inGain;
   if(nCh==1)input[1]=input[0];
   if(stereo==2){float m=0.5f*(input[0]+input[1]);input[0]=input[1]=m;}
   else if(stereo==1){float mid=0.5f*(input[0]+input[1]),side=0.5f*(input[0]-input[1]);input[0]=mid+side;input[1]=mid-side;}
   float output[2]{};
   for(int ch=0;ch<nCh;++ch) {
     auto& s=channels[(size_t)ch]; float x=input[ch];
     float band=x;
     for(int st=0;st<stages;++st){float hp=hpA*(s.hpY[(size_t)st]+band-s.hpX[(size_t)st]);s.hpX[(size_t)st]=band;s.hpY[(size_t)st]=hp;float lp=s.lpY[(size_t)st]+lpA*(hp-s.lpY[(size_t)st]);s.lpY[(size_t)st]=lp;band=lp;}
     x=filterMode==0?band:x-band; if(std::abs(x)<gate)x=0.0f;
     s.input[(size_t)s.inputWrite]=x;
     s.inputWrite=(s.inputWrite+1)%fftSize;
     s.absoluteSample++;
     s.samplesSinceFrame++;
     if(s.samplesSinceFrame>=hopSize && s.absoluteSample>=fftSize) {
       s.samplesSinceFrame=0;
       processSpectralFrame(s,ch,mode,amount,speed,perceptual,stereo);
     }
     const int readSlot=(int)((s.absoluteSample-1)%delaySize);
     float wet=s.outputDelay[(size_t)readSlot]; s.outputDelay[(size_t)readSlot]=0.0f;
     // Packet decisions are made once per packet, never per sample/frame.
     // Repeat replays the previous complete audio packet, not a frozen last sample.
     const float packetTarget=(!packetMode || !packetDropped) ? 1.0f : 0.0f;
     const float packetSmooth=1.0f-std::exp(-1.0f/(float)(sampleRate*0.012));
     s.packetGain += (packetTarget-s.packetGain)*packetSmooth;
     const int packetIndex=juce::jlimit(0,8191,packetCounter);
     if(!packetDropped) s.packetBuffer[(size_t)packetIndex]=wet;
     const float repeated=s.packetBuffer[(size_t)juce::jmin(packetIndex,packetLength-1)];
     if(mode==4 || mode==5) {
       wet *= s.packetGain;
     } else if(mode==3 || mode==6) {
       const float repeatMix=1.0f-s.packetGain;
       wet=wet*s.packetGain+repeated*repeatMix;
     }
     float y=enabled?wet*lossGain:x;
     if(enabled)y=x+(y-x)*mix;
     y*=outGain;
     if(autoGain&&amount>0.001f)y*=1.0f/(1.0f+amount*0.28f);
     output[ch]=y;
   }
   for(int ch=0;ch<nCh;++ch)buffer.setSample(ch,i,output[ch]);
   if(mode>=3 && mode<=6) packetCounter=(packetCounter+1)%packetLength;
 }
}
void KojiverbProcessor::getStateInformation(juce::MemoryBlock& dest){auto copy=state.copyState();std::unique_ptr<juce::XmlElement> xml(copy.createXml());copyXmlToBinary(*xml,dest);}
void KojiverbProcessor::setStateInformation(const void* data,int size){std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data,size));if(xml&&xml->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessorEditor* KojiverbProcessor::createEditor(){return new KojiverbEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new KojiverbProcessor();}

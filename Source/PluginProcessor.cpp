#include "PluginProcessor.h"
#include "PluginEditor.h"
namespace { float P(juce::AudioProcessorValueTreeState& s,const char* id,float d=0){auto* p=s.getRawParameterValue(id);return p?p->load():d;} }
juce::AudioProcessorValueTreeState::ParameterLayout KojiverbProcessor::makeParams(){
 using F=juce::AudioParameterFloat; using C=juce::AudioParameterChoice; using B=juce::AudioParameterBool;
 std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
 p.push_back(std::make_unique<F>("crush","Lossy",juce::NormalisableRange<float>(0,1),0));
 p.push_back(std::make_unique<F>("bits","Bits",juce::NormalisableRange<float>(4,16,1),12));
 p.push_back(std::make_unique<F>("rate","Rate Reduce",juce::NormalisableRange<float>(1,32,1),1));
 p.push_back(std::make_unique<F>("room","Room Size",juce::NormalisableRange<float>(0,1),.45f));
 p.push_back(std::make_unique<F>("damp","Damping",juce::NormalisableRange<float>(0,1),.35f));
 p.push_back(std::make_unique<F>("rev","Reverb Mix",juce::NormalisableRange<float>(0,1),.25f));
 p.push_back(std::make_unique<B>("freeze","Freeze",false));
 p.push_back(std::make_unique<F>("delayMix","Delay Mix",juce::NormalisableRange<float>(0,1),0));
 p.push_back(std::make_unique<F>("feedback","Feedback",juce::NormalisableRange<float>(0,.95f),.35f));
 p.push_back(std::make_unique<C>("mode","Delay Mode",juce::StringArray{"Mono","Stereo","Ping-Pong"},1));
 p.push_back(std::make_unique<C>("division","Division",juce::StringArray{"1/1","1/2","1/4","1/8","1/16","1/8T","1/16T","1/4D","1/8D","1/16D"},3));
 p.push_back(std::make_unique<B>("sync","Host BPM Sync",true));
 p.push_back(std::make_unique<F>("freeTime","Free Delay (ms)",juce::NormalisableRange<float>(10,2000,1),500));
 return {p.begin(),p.end()};
}
KojiverbProcessor::KojiverbProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),state(*this,nullptr,"KOJIVERB",makeParams()){}
bool KojiverbProcessor::isBusesLayoutSupported(const BusesLayout& b)const{return b.getMainInputChannelSet()==juce::AudioChannelSet::stereo()&&b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
void KojiverbProcessor::prepareToPlay(double s,int){sr=s;auto n=(size_t)(s*8.1);for(auto& c:delay)c.assign(n,0);write=hold=0;heldL=heldR=0;reverb.reset();}
void KojiverbProcessor::releaseResources(){reverb.reset();}
void KojiverbProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&){
 juce::ScopedNoDenormals guard;int n=b.getNumSamples();if(b.getNumChannels()<2||delay[0].empty())return;
 float crush=P(state,"crush"),bits=P(state,"bits",12),rate=P(state,"rate",1),mix=P(state,"delayMix"),fb=P(state,"feedback",.35f);
 int mode=juce::roundToInt(P(state,"mode",1)),div=juce::jlimit(0,9,juce::roundToInt(P(state,"division",3)));
 double bpm=120; if(auto* h=getPlayHead())if(auto pos=h->getPosition())if(auto v=pos->getBpm())bpm=*v;
 const double beats[]={4,2,1,.5,.25,1.0/3,1.0/6,1.5,.75,.375};
 double secs=P(state,"sync",1)>.5?(60.0/juce::jlimit(20.0,300.0,bpm))*beats[div]:P(state,"freeTime",500)/1000.0;
 int cap=(int)delay[0].size(),ds=juce::jlimit(1,cap-1,(int)(secs*sr));
 rp.roomSize=P(state,"room",.45f);rp.damping=P(state,"damp",.35f);rp.wetLevel=P(state,"rev",.25f);rp.dryLevel=1.0f-rp.wetLevel*.65f;rp.width=1;rp.freezeMode=P(state,"freeze")>.5?1.0f:0.0f;reverb.setParameters(rp);
 float q=std::pow(2.0f,bits-1);
 for(int i=0;i<n;++i){float l=b.getSample(0,i),r=b.getSample(1,i);
  if(hold<=0){heldL=std::round(l*q)/q;heldR=std::round(r*q)/q;hold=juce::roundToInt(rate);}--hold;
  float a=l+(heldL-l)*crush,c=r+(heldR-r)*crush;int read=(write-ds+cap)%cap;float dl=delay[0][read],dr=delay[1][read];
  if(mode==0){float m=(a+c)*.5f;dl=dr=m;delay[0][write]=m+dl*fb;delay[1][write]=m+dr*fb;}
  else if(mode==2){delay[0][write]=a+dr*fb;delay[1][write]=c+dl*fb;}
  else{delay[0][write]=a+dl*fb;delay[1][write]=c+dr*fb;}
  write=(write+1)%cap;b.setSample(0,i,a+dl*mix);b.setSample(1,i,c+dr*mix);
 }
 reverb.processStereo(b.getWritePointer(0),b.getWritePointer(1),n);
}
void KojiverbProcessor::getStateInformation(juce::MemoryBlock& d){auto v=state.copyState();std::unique_ptr<juce::XmlElement>x(v.createXml());copyXmlToBinary(*x,d);}
void KojiverbProcessor::setStateInformation(const void* d,int n){std::unique_ptr<juce::XmlElement>x(getXmlFromBinary(d,n));if(x&&x->hasTagName(state.state.getType()))state.replaceState(juce::ValueTree::fromXml(*x));}
juce::AudioProcessorEditor* KojiverbProcessor::createEditor(){return new KojiverbEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new KojiverbProcessor();}
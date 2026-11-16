#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
float param(juce::AudioProcessorValueTreeState& s, const char* id, float fallback = 0.0f) {
    if (auto* p = s.getRawParameterValue(id)) return p->load();
    return fallback;
}
float dbToGain(float db) { return juce::Decibels::decibelsToGain(db); }
}

juce::AudioProcessorValueTreeState::ParameterLayout KojiverbProcessor::makeParams() {
    using F = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<C>("lossMode", "Loss Mode", juce::StringArray{
        "Standard", "Inverse", "Phase Jitter", "Packet Repeat", "Packet Loss",
        "Standard + Packet Loss", "Standard + Packet Repeat"}, 0));
    p.push_back(std::make_unique<F>("lossAmount", "Loss Amount", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    p.push_back(std::make_unique<F>("lossSpeed", "Loss Speed", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    p.push_back(std::make_unique<F>("lossGain", "Loss Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    p.push_back(std::make_unique<F>("masterMix", "Master Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    p.push_back(std::make_unique<B>("masterOn", "Master On", true));

    p.push_back(std::make_unique<C>("filterMode", "Filter Mode", juce::StringArray{"Normal", "Inverted"}, 0));
    p.push_back(std::make_unique<F>("lowCut", "Low Cut", juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.35f), 20.0f));
    p.push_back(std::make_unique<F>("highCut", "High Cut", juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.35f), 20000.0f));
    p.push_back(std::make_unique<C>("filterSlope", "Filter Slope", juce::StringArray{"6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct"}, 3));

    p.push_back(std::make_unique<B>("autoGain", "Auto Gain", false));
    p.push_back(std::make_unique<F>("gate", "Gate Threshold", juce::NormalisableRange<float>(-96.0f, 0.0f, 0.1f), -96.0f));
    p.push_back(std::make_unique<C>("stereoMode", "Stereo Mode", juce::StringArray{"Stereo", "Joint Stereo", "Mono"}, 0));
    p.push_back(std::make_unique<C>("weighting", "Weighting", juce::StringArray{"Perceptual", "Flat"}, 0));
    p.push_back(std::make_unique<F>("inputGain", "Input Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    p.push_back(std::make_unique<F>("outputGain", "Output Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    return {p.begin(), p.end()};
}

KojiverbProcessor::KojiverbProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "KOJIVERB", makeParams()) {}

bool KojiverbProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    const auto in = b.getMainInputChannelSet();
    const auto out = b.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()))
        || (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::stereo());
}

void KojiverbProcessor::prepareToPlay(double sr, int) {
    sampleRate = sr;
    for (auto& c : channels) c = ChannelState{};
    blockCounter = 0;
    blockLength = 1;
    packetDropped = false;
}
void KojiverbProcessor::releaseResources() {}

void KojiverbProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals guard;
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numChannels == 0) return;

    const bool enabled = param(state, "masterOn", 1.0f) > 0.5f;
    const float mix = param(state, "masterMix", 1.0f);
    const float amount = param(state, "lossAmount");
    const float speed = param(state, "lossSpeed", 1.0f);
    const float lossGain = dbToGain(param(state, "lossGain"));
    const float inputGain = dbToGain(param(state, "inputGain"));
    const float outputGain = dbToGain(param(state, "outputGain"));
    const float gateGain = dbToGain(param(state, "gate", -96.0f));
    const int mode = juce::roundToInt(param(state, "lossMode"));
    const int filterMode = juce::roundToInt(param(state, "filterMode"));
    const int stages = juce::jlimit(1, 4, juce::roundToInt(param(state, "filterSlope", 3)) + 1);
    const int stereoMode = juce::roundToInt(param(state, "stereoMode"));
    const bool autoGain = param(state, "autoGain") > 0.5f;
    const bool perceptual = param(state, "weighting") < 0.5f;

    const float low = juce::jlimit(20.0f, (float)(sampleRate * 0.45), param(state, "lowCut", 20.0f));
    const float high = juce::jlimit(low, (float)(sampleRate * 0.45), param(state, "highCut", 20000.0f));
    const float hpA = std::exp(-juce::MathConstants<float>::twoPi * low / (float)sampleRate);
    const float lpA = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * high / (float)sampleRate);

    const int requestedBlock = 1 + juce::roundToInt((1.0f - speed) * 47.0f);
    if (requestedBlock != blockLength) { blockLength = requestedBlock; blockCounter = 0; }

    const float quantStep = 0.00002f + amount * amount * (perceptual ? 0.12f : 0.20f);
    const float smear = 0.02f + (1.0f - speed) * 0.92f;
    const float packetChance = amount * 0.42f;
    const float jitterDepth = amount * (1.0f - speed) * 0.012f;
    const bool usePacketLoss = mode == 4 || mode == 5;
    const bool usePacketRepeat = mode == 3 || mode == 6;

    float peakIn = 0.0f, peakOut = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float in[2]{};
        for (int ch = 0; ch < juce::jmin(2, numChannels); ++ch) in[ch] = buffer.getSample(ch, i) * inputGain;
        if (numChannels == 1) in[1] = in[0];

        if (stereoMode == 2) {
            const float mono = 0.5f * (in[0] + in[1]);
            in[0] = in[1] = mono;
        } else if (stereoMode == 1 && numChannels > 1) {
            const float mid = 0.5f * (in[0] + in[1]);
            const float side = 0.5f * (in[0] - in[1]) * 0.5f;
            in[0] = mid + side;
            in[1] = mid - side;
        }

        float processed[2]{};
        if (blockCounter == 0) packetDropped = (random.nextFloat() < packetChance);
        for (int ch = 0; ch < juce::jmin(2, numChannels); ++ch) {
            auto& s = channels[(size_t)ch];
            float x = in[ch];
            peakIn = juce::jmax(peakIn, std::abs(x));

            // Cascaded one-pole high/low cuts. Inverted mode removes the selected band.
            float band = x;
            for (int st = 0; st < stages; ++st) {
                float hp = hpA * (s.hpY[st] + band - s.hpX[st]);
                s.hpX[st] = band; s.hpY[st] = hp;
                float lp = s.lpY[st] + lpA * (hp - s.lpY[st]);
                s.lpY[st] = lp;
                band = lp;
            }
            x = filterMode == 0 ? band : x - band;

            if (std::abs(x) < gateGain) x = 0.0f;
            const float dry = x;
            float decoded = x;

            if (mode == 2) {
                // Clock/phase instability: variable fractional delay with interpolated history.
                const float wobble = std::sin((float)(s.historyWrite + i) * 0.071f) * jitterDepth * (float)sampleRate;
                const int back = juce::jlimit(1, 120, 1 + (int)std::abs(wobble));
                const int idx = (s.historyWrite - back + 128) % 128;
                decoded = s.history[idx];
            } else if (mode == 3 || mode == 4 || mode == 5 || mode == 6) {
                if (blockCounter == 0) {
                    s.heldPacket = (usePacketLoss && packetDropped) ? (usePacketRepeat ? s.previousPacket : 0.0f) : x;
                    s.previousPacket = s.heldPacket;
                }
                decoded = s.heldPacket;
            } else {
                if (blockCounter == 0) {
                    const float delta = x - s.previousDecoded;
                    const float weightedStep = quantStep * (perceptual ? (0.65f + 0.7f * std::sqrt(std::abs(x) + 0.001f)) : 1.0f);
                    const float encodedDelta = std::round(delta / weightedStep) * weightedStep;
                    s.heldPacket = s.previousDecoded + encodedDelta;
                    s.previousDecoded = s.heldPacket;
                }
                decoded = s.heldPacket;
                if (mode == 1) decoded = dry - decoded;
                s.smoothed += (decoded - s.smoothed) * (1.0f - smear);
                decoded = s.smoothed;
            }

            if (mode == 2 || mode >= 3) s.history[s.historyWrite] = x;
            else s.history[s.historyWrite] = decoded;
            s.historyWrite = (s.historyWrite + 1) % 128;
            processed[ch] = decoded * lossGain;
        }
        if (numChannels > 1) blockCounter = (blockCounter + 1) % blockLength;
        else blockCounter = (blockCounter + 1) % blockLength;

        for (int ch = 0; ch < juce::jmin(2, numChannels); ++ch) {
            const float wet = enabled ? processed[ch] : in[ch];
            float out = in[ch] + (wet - in[ch]) * (enabled ? mix : 0.0f);
            out *= outputGain;
            if (autoGain && amount > 0.01f) out *= juce::jlimit(0.5f, 2.0f, 1.0f / (1.0f + amount * 0.35f));
            peakOut = juce::jmax(peakOut, std::abs(out));
            buffer.setSample(ch, i, out);
        }
    }
}

void KojiverbProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto copy = state.copyState();
    std::unique_ptr<juce::XmlElement> xml(copy.createXml());
    copyXmlToBinary(*xml, dest);
}
void KojiverbProcessor::setStateInformation(const void* data, int size) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* KojiverbProcessor::createEditor() { return new KojiverbEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KojiverbProcessor(); }

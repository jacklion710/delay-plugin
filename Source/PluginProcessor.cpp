/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#define VARY_DRY_WET 1 // Switch between different dry wet implementations

//==============================================================================
DelayAudioProcessor::DelayAudioProcessor() :
    AudioProcessor(
                   BusesProperties()
                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
                   .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                   ),
                   params(apvts)
{
}

static juce::String stringFromMilliseconds(float value, int)
{
    if (value < 10.0f) {
        return juce::String(value, 2) + " ms";
    } else if (value < 100.0f) {
        return juce::String(int(value)) + " ms";
    } else {
        return juce::String(value * 0.001f, 2) + " s";
    }
}

static juce::String stringFromDecibels(float value, int)
{
    return juce::String(value, 1) + " db";
}

static juce::String stringFromPercent(float value, int)
{
    return juce::String(int(value)) + " %";
}

DelayAudioProcessor::~DelayAudioProcessor()
{
}

//==============================================================================
const juce::String DelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DelayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DelayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DelayAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DelayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DelayAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DelayAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String DelayAudioProcessor::getProgramName (int index)
{
    return {};
}

void DelayAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void DelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    params.prepareToPlay(sampleRate);
    params.reset();
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = juce::uint32(samplesPerBlock);
    spec.numChannels = 2;
    
    delayLine.prepare(spec);
    
    double numSamples = Parameters::maxDelayTime / 1000.0 * sampleRate;
    int maxDelayInSamples = int(std::ceil(numSamples));
    delayLine.setMaximumDelayInSamples(maxDelayInSamples);
    delayLine.reset();
}

void DelayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DelayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts)const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
#endif

void DelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, [[maybe_unused]]juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    params.update();
    
    float sampleRate = float(getSampleRate());
    
    float* channelDataL = buffer.getWritePointer(0);
    float* channelDataR = buffer.getWritePointer(1);
        
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        params.smoothen();
        
        float delayInSamples = params.delayTime / 1000.0f * sampleRate;
        delayLine.setDelay(delayInSamples);

        float dryL = channelDataL[sample];
        float dryR = channelDataR[sample];
        
        delayLine.pushSample(0, dryL);
        delayLine.pushSample(1, dryR);
        
        float wetL = delayLine.popSample(0);
        float wetR = delayLine.popSample(1);
        
        // Dry/wet mixing
        #if VARY_DRY_WET
                // Dry/wet where dry and wet are both varied
                float mixL = dryL * (1.0f - params.mix) + wetL * params.mix;
                float mixR = dryR * (1.0f - params.mix) + wetR * params.mix;
        #else
                // Dry/wet where dry is constant and wet is varied
                float mixL = dryL + wetL * params.mix;
                float mixR = dryR + wetR * params.mix;
        #endif
        
        channelDataL[sample] = mixL * params.gain;
        channelDataR[sample] = mixR * params.gain;
        
    }
}

//==============================================================================
bool DelayAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DelayAudioProcessor::createEditor()
{
    return new DelayAudioProcessorEditor (*this);
}

//==============================================================================
void DelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void DelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DelayAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout
    Parameters::createParameterLayout()
{
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        
        // Gain parameter
        layout.add(std::make_unique<juce::AudioParameterFloat>(gainParamID, "Output Gain", juce::NormalisableRange<float> { -12.0f, 12.0f }, 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromDecibels)));
        
        // Delay Time parameter
        layout.add(std::make_unique<juce::AudioParameterFloat>(delayTimeParamID, "Delay Time", juce::NormalisableRange<float> { minDelayTime, maxDelayTime, 0.001f, 0.25f }, 100.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromMilliseconds)));
        
        // Dry/Wet Mix parameter
        layout.add(std::make_unique<juce::AudioParameterFloat>(mixParamID, "Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromPercent)));
        
        return layout;
}

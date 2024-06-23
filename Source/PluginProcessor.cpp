#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ProtectYourEars.h"

#define VARY_DRY_WET 0 // Switch between different dry wet implementations

DelayAudioProcessor::DelayAudioProcessor() :
    AudioProcessor(
                   BusesProperties()
                   .withInput("Input", juce::AudioChannelSet::stereo(), true)
                   .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                   ),
                    params(apvts)
{
    lowCutFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    highCutFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
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

static float millisecondsFromString(const juce::String& text)
{
    float value = text.getFloatValue();
    
    if (!text.endsWithIgnoreCase("ms")) {
        if (text.endsWithIgnoreCase("s") || value < Parameters::minDelayTime) {
            return value * 1000.0f;
        }
    }
    return value;
}

static juce::String stringFromDecibels(float value, int)
{
    return juce::String(value, 1) + " db";
}

static juce::String stringFromPercent(float value, int)
{
    return juce::String(int(value)) + " %";
}

static juce::String stringFromHz(float value, int)
{
    if (value < 1000.0f) {
        return juce::String(int(value)) + " Hz";
    } else if (value < 10000.0f) {
        return juce::String(value / 1000.0f, 2) + " k";
    } else {
        return juce::String(value / 1000.0f, 1) + " k";
    }
}

static float hzFromString(const juce::String& str)
{
    float value = str.getFloatValue();
    if (value < 10.0f) {
        return value * 1000.0f;
    }
    return value;
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
    tempo.reset();
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = juce::uint32(samplesPerBlock);
    spec.numChannels = 2;
    
    lowCutFilter.prepare(spec);
    lowCutFilter.reset();

    highCutFilter.prepare(spec);
    highCutFilter.reset();
    
    feedbackL = 0.0f;
    feedbackR = 0.0f;
    
    lastLowCut = -1.0f;
    lastHighCut = -1.0f;
    
    double numSamples = Parameters::maxDelayTime / 1000.0 * sampleRate;
    int maxDelayInSamples = int(std::ceil(numSamples));
    delayLineL.setMaximumDelayInSamples(maxDelayInSamples);
    delayLineR.setMaximumDelayInSamples(maxDelayInSamples);
    delayLineL.reset();
    delayLineR.reset();
    
    levelL.store(0.0f);
    levelR.store(0.0f);
}

void DelayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DelayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    
    if (mainIn == mono && mainOut == mono) { return true; }
    if (mainIn == mono && mainOut == stereo) { return true; }
    if (mainIn == stereo && mainOut == stereo) { return true; }
    
    return false;
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
    tempo.update(getPlayHead());
    
    float syncedTime = float(tempo.getMillisecondsForNoteLength(params.delayNote));
    if (syncedTime > Parameters::maxDelayTime) {
        syncedTime = Parameters::maxDelayTime;
    }
    
    float sampleRate = float(getSampleRate());
    
    auto mainInput = getBusBuffer(buffer, true, 0);
    auto mainInputChannels = mainInput.getNumChannels();
    auto isMainInputStereo = mainInputChannels > 1;
    const float* inputDataL = mainInput.getReadPointer(0);
    const float* inputDataR = mainInput.getReadPointer(isMainInputStereo ? 1 : 0);
    
    auto mainOutput = getBusBuffer(buffer, false, 0);
    auto mainOutputChannels = mainOutput.getNumChannels();
    auto isMainOutputStereo = mainOutputChannels > 1;
    float* outputDataL = mainOutput.getWritePointer(0);
    float* outputDataR = mainOutput.getWritePointer(isMainOutputStereo ? 1 : 0);
    
    float maxL = 0.0f;
    float maxR = 0.0f;
    float max = 0.0f;
    
    if (isMainOutputStereo) {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            params.smoothen();
            
            float delayTime = params.tempoSync ? syncedTime : params.delayTime;
            float delayInSamples = delayTime / 1000.0f * sampleRate;
            
            if (params.lowCut != lastLowCut) {
                lowCutFilter.setCutoffFrequency(params.lowCut);
                lastLowCut = params.lowCut;
            }
            if (params.highCut != lastHighCut) {
                highCutFilter.setCutoffFrequency(params.highCut);
                lastHighCut = params.highCut;
            }
            
            float dryL = inputDataL[sample];
            float dryR = inputDataR[sample];
            
            float mono = (dryL + dryR) * 0.5; // Convert stereo to mono
            
            delayLineL.write(mono*params.panL + feedbackR);
            delayLineR.write(mono*params.panR + feedbackL);
            
            float wetL = delayLineL.read(delayInSamples);
            float wetR = delayLineR.read(delayInSamples);
            
            feedbackL = wetL * params.feedback;
            feedbackL = lowCutFilter.processSample(0, feedbackL);
            feedbackL = highCutFilter.processSample(0, feedbackL);

            feedbackR = wetR * params.feedback;
            feedbackR = lowCutFilter.processSample(1, feedbackR);
            feedbackR = highCutFilter.processSample(1, feedbackR);

            // Dry/wet where dry is constant and wet is varied
            float mixL = dryL + wetL * params.mix;
            float mixR = dryR + wetR * params.mix;
            
            float outL = mixL * params.gain;
            float outR = mixR * params.gain;
            
            outputDataL[sample] = outL;
            outputDataR[sample] = outR;
            
            maxL = std::max(maxL, std::abs(outL));
            maxR = std::max(maxR, std::abs(outR));
        }
    } else {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            params.smoothen();
            
            float delayTime = params.tempoSync ? syncedTime : params.delayTime;
            float delayInSamples = delayTime / 1000.0f * sampleRate;
            
            lowCutFilter.setCutoffFrequency(params.lowCut);
            highCutFilter.setCutoffFrequency(params.highCut);
            
            float dry = inputDataL[sample];
            delayLineL.write(dry + feedbackL);
            
            float wet = delayLineL.read(delayInSamples);
            feedbackL = wet * params.feedback;
            feedbackL = lowCutFilter.processSample(0, feedbackL);
            feedbackL = highCutFilter.processSample(0, feedbackL);

            float mix = dry + wet * params.mix;
            
            float out = mix * params.gain;
            outputDataL[sample] = out;
            
            max = std::max(max, std::abs(out));
        }
    }
    
    levelL.store(maxL);
    levelR.store(maxR);
    
    #if JUCE_DEBUG
        protectYourEars(buffer);
    #endif
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
        layout.add(std::make_unique<juce::AudioParameterFloat>(delayTimeParamID, "Delay Time", juce::NormalisableRange<float> { minDelayTime, maxDelayTime, 0.001f, 0.25f }, 100.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromMilliseconds).withValueFromStringFunction(millisecondsFromString)));
        
        // Dry/Wet Mix parameter
        layout.add(std::make_unique<juce::AudioParameterFloat>(mixParamID, "Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromPercent)));
        
        // Feedback parameter
        layout.add(std::make_unique<juce::AudioParameterFloat>(feedbackParamID, "Feedback", juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromPercent)));
        
        // Stereo parameter
        layout.add(std::make_unique<juce::AudioParameterFloat>(stereoParamID, "Stereo", juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromPercent)));
        
        // Lowcut parameter
        layout.add(std::make_unique<juce::AudioParameterFloat>(lowCutParamID, "Low Cut", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 20.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromHz).withValueFromStringFunction(hzFromString)));
        
        // Highcut parameter
        layout.add(std::make_unique<juce::AudioParameterFloat>(highCutParamID, "High Cut", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 20000.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(stringFromHz).withValueFromStringFunction(hzFromString)));
        
        // Tempo sync parameter
        layout.add(std::make_unique<juce::AudioParameterBool>(tempoSyncParamID, "Tempo Sync", false));
        
        juce::StringArray noteLengths = {
            "1/32",
            "1/15 trip",
            "1/32 dot",
            "1/16",
            "1/8 trip",
            "1/16 dot",
            "1/8",
            "1/4 trip",
            "1/8 dot",
            "1/4",
            "1/2 trip",
            "1/4 dot",
            "1/2",
            "1/1 trip",
            "1/2 dot",
            "1/1",
        };
        
        layout.add(std::make_unique<juce::AudioParameterChoice>(delayNoteParamID, "Delay Note", noteLengths, 9));
        
        return layout;
}

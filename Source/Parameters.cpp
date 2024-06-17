#include "Parameters.h"

template<typename T>
static void castParameter(juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id, T& destination)
{
    destination = dynamic_cast<T>(apvts.getParameter(id.getParamID()));
    jassert(destination);
}

Parameters::Parameters(juce::AudioProcessorValueTreeState& apvts)
{
    castParameter(apvts, gainParamID, gainParam);
    castParameter(apvts, delayTimeParamID, delayTimeParam);
}

void Parameters::update() noexcept
{
    gainSmoother.setTargetValue(juce::Decibels::decibelsToGain(gainParam->get()));
    
    targetDelayTime = delayTimeParam->get();
    if (delayTime == 0.0f) {
        delayTime = targetDelayTime;
    }
}

void Parameters::prepareToPlay(double sampleRate) noexcept
{
    double duration = 0.02;
    gainSmoother.reset(sampleRate, duration);
    coeff = 1.0f - std::exp(-1.0f / (0.2f * float(sampleRate)));
}

void Parameters::reset() noexcept
{
    gain = 0.0f;
    delayTime = 0.0f;
    
    gainSmoother.setCurrentAndTargetValue(
      juce::Decibels::decibelsToGain(gainParam->get()));
}

void Parameters::smoothen() noexcept
{
    gain = gainSmoother.getNextValue();
    delayTime += (targetDelayTime - delayTime) * coeff;
}
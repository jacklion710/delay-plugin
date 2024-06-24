#include "Parameters.h"
#include "DSP.h"

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
    castParameter(apvts, mixParamID, mixParam);
    castParameter(apvts, feedbackParamID, feedbackParam);
    castParameter(apvts, stereoParamID, stereoParam);
    castParameter(apvts, lowCutParamID, lowCutParam);
    castParameter(apvts, highCutParamID, highCutParam);
    castParameter(apvts, tempoSyncParamID, tempoSyncParam);
    castParameter(apvts, delayNoteParamID, delayNoteParam);
    castParameter(apvts, bypassParamID, bypassParam);
}

void Parameters::update() noexcept
{
    gainSmoother.setTargetValue(juce::Decibels::decibelsToGain(gainParam->get()));
    feedbackSmoother.setTargetValue(feedbackParam->get() * 0.01f);
    mixSmoother.setTargetValue(mixParam->get() * 0.01f);
    lowCutSmoother.setCurrentAndTargetValue(lowCutParam->get());
    highCutSmoother.setCurrentAndTargetValue(highCutParam->get());

    targetDelayTime = delayTimeParam->get();
    if (delayTime == 0.0f) {
        delayTime = targetDelayTime;
    }
    delayNote = delayNoteParam->getIndex();
    tempoSync = tempoSyncParam->get();
    bypassed = bypassParam->get();
}

void Parameters::prepareToPlay(double sampleRate) noexcept
{
    double duration = 0.02;
    gainSmoother.reset(sampleRate, duration);
    feedbackSmoother.reset(sampleRate, duration);
    coeff = 1.0f - std::exp(-1.0f / (0.2f * float(sampleRate)));
    mixSmoother.reset(sampleRate, duration);
    stereoSmoother.reset(sampleRate, duration);
    lowCutSmoother.reset(sampleRate, duration);
    highCutSmoother.reset(sampleRate, duration);
}

void Parameters::reset() noexcept
{
    gain = 0.0f;
    delayTime = 0.0f;
    mix = 1.0f;
    feedback = 0.0f;
    lowCut = 20.0f;
    highCut = 20000.0f;
    
    gainSmoother.setCurrentAndTargetValue(
      juce::Decibels::decibelsToGain(gainParam->get()));
    mixSmoother.setCurrentAndTargetValue(mixParam->get() * 0.01f);
    feedbackSmoother.setCurrentAndTargetValue(feedbackParam->get() * 0.01f);
    stereoSmoother.setCurrentAndTargetValue(stereoParam->get() * 0.01f);
    lowCutSmoother.setCurrentAndTargetValue(lowCutParam->get());
    highCutSmoother.setCurrentAndTargetValue(highCutParam->get());
}

void Parameters::smoothen() noexcept
{
    gain = gainSmoother.getNextValue();
//    delayTime += (targetDelayTime - delayTime) * coeff; // Repitch mode
    delayTime = targetDelayTime; // Fade mode
    mix = mixSmoother.getNextValue();
    feedback = feedbackSmoother.getNextValue();
    panningEqualPower(stereoSmoother.getNextValue(), panL, panR);
    lowCut = lowCutSmoother.getNextValue();
    highCut = highCutSmoother.getNextValue();
}

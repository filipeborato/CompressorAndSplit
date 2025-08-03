/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

    The original version was auto‑generated.  This version fixes a few
    oversights (such as assigning AttackTime to zero in prepareToPlay) and
    retains a simple parameter interface for use by the custom editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CompreezorAudioProcessor::CompreezorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
    #if ! JucePlugin_IsMidiEffect
    #if ! JucePlugin_IsSynth
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
    #endif
                        .withOutput ("Output", AudioChannelSet::stereo(), true)
    #endif
    )
#endif
{
}

CompreezorAudioProcessor::~CompreezorAudioProcessor() = default;

//==============================================================================
const String CompreezorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CompreezorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool CompreezorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool CompreezorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double CompreezorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================
int CompreezorAudioProcessor::getNumPrograms()
{
    return 1;   // some hosts require at least one program
}

int CompreezorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void CompreezorAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }

const String CompreezorAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void CompreezorAudioProcessor::changeProgramName (int index, const String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void CompreezorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    // Initialise envelope detectors with current settings.  Do not modify
    // AttackTime here – it is configured via the editor and updated in real
    // time.  DigitalAnalogue toggles between digital and analogue time constants.
    const bool analogueTC = DigitalAnalogue;
    const bool logDetect  = true;
    const UINT detectMode = DETECT_MODE_RMS; // default to RMS; may be changed by UI
    m_LeftDetector.init  (static_cast<float> (sampleRate), AttackTime,
                          ReleaseTime, analogueTC, detectMode, logDetect);
    m_RightDetector.init (static_cast<float> (sampleRate), AttackTime,
                          ReleaseTime, analogueTC, detectMode, logDetect);
}

void CompreezorAudioProcessor::releaseResources()
{
    // You can use this as an opportunity to free up spare memory
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CompreezorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // Only mono or stereo is supported in this example
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;
   #if ! JucePlugin_IsSynth
    // The input layout must match the output layout
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif
    return true;
#endif
}
#endif

//==============================================================================
void CompreezorAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that didn't contain input data
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Apply compression per channel
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // Apply input gain
            float inSample = channelData[sample] * DetGain;
            // Detect level using the left detector (we only use one detector for simplicity)
            float detectorValue = m_LeftDetector.detect (inSample);
            // Calculate gain reduction
            float fGn = calcCompressorGain (detectorValue, Threshold, Ratio, KneeWidth, false);
            // Apply gain reduction and make up gain
            channelData[sample] = fGn * inSample * OutputGain;
        }
    }
}

//==============================================================================
bool CompreezorAudioProcessor::hasEditor() const
{
    return true;
}

AudioProcessorEditor* CompreezorAudioProcessor::createEditor()
{
    return new CompreezorAudioProcessorEditor (*this);
}

//==============================================================================
void CompreezorAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // Save parameters here (placeholder for future implementation)
    juce::ignoreUnused (destData);
}

void CompreezorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Load parameters here (placeholder for future implementation)
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
float CompreezorAudioProcessor::calcCompressorGain (float fDetectorValue, float fThreshold,
                                                    float fRatio, float fKneeWidth, bool bLimit)
{
    float CS = 1.0f - 1.0f / fRatio;
    // Soft‑knee interpolation
    if (fKneeWidth > 0.0f && fDetectorValue > (fThreshold - fKneeWidth / 2.0f) &&
        fDetectorValue < fThreshold + fKneeWidth / 2.0f)
    {
        double x[2];
        double y[2];
        x[0] = fThreshold - fKneeWidth / 2.0f;
        x[1] = juce::jmin (0.0f, fThreshold + fKneeWidth / 2.0f);
        y[0] = 0.0;
        y[1] = CS;
        CS = static_cast<float> (lagrpol (&x[0], &y[0], 2, fDetectorValue));
    }
    // Gain computation; clamp to a maximum of 0 dB reduction
    float yG = CS * (fThreshold - fDetectorValue);
    yG = juce::jmin (0.0f, yG);
    return static_cast<float> (pow (10.0, yG / 20.0f));
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompreezorAudioProcessor();
}

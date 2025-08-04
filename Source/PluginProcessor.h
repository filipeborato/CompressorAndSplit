/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

    The original file was auto‑generated.  The current version includes minor
    clean‑ups and bug fixes such as removing unintended assignments in
    prepareToPlay().  It also preserves user variables for use by the editor.

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "EnvelopeDetector.h"

//==============================================================================
class CompreezorAudioProcessor  : public AudioProcessor
{
public:
    //==============================================================================
    CompreezorAudioProcessor();
    ~CompreezorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
#endif
    void processBlock (AudioSampleBuffer&, MidiBuffer&) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect () const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Public parameters exposed to the UI.  These are not ideal for a final
    // product (use AudioProcessorValueTreeState instead) but suffice for this
    // educational example.
    float DetGain      = 1.0f;   // Input gain in linear scale
    float Threshold    = 0.0f;   // Compressor threshold in dB
    float AttackTime   = 10.0f;  // Attack time in milliseconds
    float ReleaseTime  = 200.0f; // Release time in milliseconds
    float Ratio        = 4.0f;   // Compression ratio
    float OutputGain   = 1.0f;   // Output gain in linear scale
    float KneeWidth    = 0.0f;   // Compressor knee width
    bool  DigitalAnalogue = false; // Analogue time constant if true, digital otherwise

    // Envelope detectors for stereo processing
    CEnvelopeDetector m_LeftDetector;
    CEnvelopeDetector m_RightDetector;

    // Current gain reduction in decibels (positive value).  This is updated
    // every processBlock call and can be used by the editor to display a
    // gain reduction meter.
    float GainReduction = 0.0f;

    // Detection mode codes for convenience
    static constexpr UINT DETECT_MODE_PEAK = 0;
    static constexpr UINT DETECT_MODE_MS   = 1;
    static constexpr UINT DETECT_MODE_RMS  = 2;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompreezorAudioProcessor)

    float calcCompressorGain (float fDetectorValue, float fThreshold,
                              float fRatio, float fKneeWidth, bool bLimit);
};

/*
  ==============================================================================

    This file implements the user interface for the compressor/splitter plug‑in.

    It has been refactored to use modern C++ memory management, expose new
    features (digital/analogue time constant toggle and detector mode selection)
    and improve robustness in the file upload/download routines.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "API_Set_File_Upload.h"
#include "Downloader.h"

using juce::AlertWindow;

//==============================================================================
CompreezorAudioProcessorEditor::CompreezorAudioProcessorEditor (CompreezorAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // --------------------------------------------------------------------------
    // Create sliders.  Each slider is configured with sensible ranges and
    // appearance options.  We attach listeners so we can update the processor
    // state when the user moves them.
    detGainSlider     = std::make_unique<Slider> ("Det Gain");
    thresholdSlider   = std::make_unique<Slider> ("Threshold");
    attackTimeSlider  = std::make_unique<Slider> ("Attack Time");
    releaseTimeSlider = std::make_unique<Slider> ("Release Time");
    ratioSlider       = std::make_unique<Slider> ("Ratio");
    outputGainSlider  = std::make_unique<Slider> ("Makeup Gain");
    kneeWidthSlider   = std::make_unique<Slider> ("Knee Width");

    // Configure detection gain slider
    detGainSlider->setRange (-12.0f, 12.0f, 0.01f);
    detGainSlider->setValue (p.DetGain);
    detGainSlider->setSliderStyle (Slider::RotaryVerticalDrag);
    detGainSlider->setTextBoxStyle (Slider::TextBoxLeft, false, 80, 20);
    detGainSlider->setColour (Slider::thumbColourId, Colour (0xffb5b5b5));
    detGainSlider->setSkewFactorFromMidPoint (0.5);
    detGainSlider->addListener (this);

    // Threshold slider
    thresholdSlider->setRange (-60.0f, 0.0f, 0.01f);
    thresholdSlider->setValue (p.Threshold);
    thresholdSlider->setSliderStyle (Slider::RotaryVerticalDrag);
    thresholdSlider->setTextBoxStyle (Slider::TextBoxLeft, false, 80, 20);
    thresholdSlider->setColour (Slider::thumbColourId, Colour (0xffb5b5b5));
    thresholdSlider->setSkewFactor (2.0);
    thresholdSlider->addListener (this);

    // Attack time slider
    attackTimeSlider->setRange (0.02f, 300.0f, 0.01f);
    attackTimeSlider->setValue (p.AttackTime);
    attackTimeSlider->setSliderStyle (Slider::RotaryVerticalDrag);
    attackTimeSlider->setTextBoxStyle (Slider::TextBoxLeft, false, 80, 20);
    attackTimeSlider->setColour (Slider::thumbColourId, Colour (0xffb5b5b5));
    attackTimeSlider->setSkewFactor (0.5);
    attackTimeSlider->addListener (this);

    // Release time slider
    releaseTimeSlider->setRange (10.0f, 5000.0f, 0.01f);
    releaseTimeSlider->setValue (p.ReleaseTime);
    releaseTimeSlider->setSliderStyle (Slider::RotaryVerticalDrag);
    releaseTimeSlider->setTextBoxStyle (Slider::TextBoxLeft, false, 80, 20);
    releaseTimeSlider->setColour (Slider::thumbColourId, Colour (0xffb5b5b5));
    releaseTimeSlider->setSkewFactor (0.5);
    releaseTimeSlider->addListener (this);

    // Ratio slider
    ratioSlider->setRange (1.0f, 20.0f, 0.01f);
    ratioSlider->setValue (p.Ratio);
    ratioSlider->setSliderStyle (Slider::RotaryVerticalDrag);
    ratioSlider->setTextBoxStyle (Slider::TextBoxLeft, false, 80, 20);
    ratioSlider->setColour (Slider::thumbColourId, Colour (0xffb5b5b5));
    ratioSlider->addListener (this);

    // Output gain slider
    outputGainSlider->setRange (0.0f, 40.0f, 0.01f);
    outputGainSlider->setValue (p.OutputGain);
    outputGainSlider->setSliderStyle (Slider::RotaryVerticalDrag);
    outputGainSlider->setTextBoxStyle (Slider::TextBoxLeft, false, 80, 20);
    outputGainSlider->setColour (Slider::thumbColourId, Colour (0xffb5b5b5));
    outputGainSlider->addListener (this);

    // Knee width slider
    kneeWidthSlider->setRange (0.0f, 20.0f, 0.01f);
    kneeWidthSlider->setValue (p.KneeWidth);
    kneeWidthSlider->setSliderStyle (Slider::RotaryVerticalDrag);
    kneeWidthSlider->setTextBoxStyle (Slider::TextBoxLeft, false, 80, 20);
    kneeWidthSlider->setColour (Slider::thumbColourId, Colour (0xffb5b5b5));
    kneeWidthSlider->addListener (this);

    // Add sliders to the editor
    addAndMakeVisible (detGainSlider.get());
    addAndMakeVisible (thresholdSlider.get());
    addAndMakeVisible (attackTimeSlider.get());
    addAndMakeVisible (releaseTimeSlider.get());
    addAndMakeVisible (ratioSlider.get());
    addAndMakeVisible (outputGainSlider.get());
    addAndMakeVisible (kneeWidthSlider.get());

    // --------------------------------------------------------------------------
    // Digital/Analogue toggle button
    digitalAnalogButton = std::make_unique<ToggleButton> ("Analogue TC");
    digitalAnalogButton->setToggleState (p.DigitalAnalogue, dontSendNotification);
    digitalAnalogButton->addListener (this);
    addAndMakeVisible (digitalAnalogButton.get());

    // --------------------------------------------------------------------------
    // Detector mode combo box
    detectModeCombo = std::make_unique<ComboBox> ("Detector Mode");
    detectModeCombo->addItem ("Peak", 1);
    detectModeCombo->addItem ("MS",   2);
    detectModeCombo->addItem ("RMS",  3);
    detectModeCombo->setSelectedId (3, dontSendNotification); // default to RMS
    detectModeCombo->addListener (this);
    addAndMakeVisible (detectModeCombo.get());

    // --------------------------------------------------------------------------
    // Upload and download buttons
    uploadButton   = std::make_unique<TextButton> ("Upload");
    uploadButton->addListener (this);
    addAndMakeVisible (uploadButton.get());

    downloadButton = std::make_unique<TextButton> ("Download Split");
    downloadButton->addListener (this);
    addAndMakeVisible (downloadButton.get());

    // --------------------------------------------------------------------------
    // Set a comfortable size for the UI.  We've increased the height to
    // accommodate the additional controls.
    setSize (880, 360);
}

CompreezorAudioProcessorEditor::~CompreezorAudioProcessorEditor()
{
    // Unique pointers automatically clean up
}

//==============================================================================
void CompreezorAudioProcessorEditor::paint (Graphics& g)
{
    // Fill the background with the host's background colour
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

    // Draw labels for the controls.  These use fixed coordinates and sizes.
    g.setColour (Colour (0xffd4d4d4));
    g.setFont (Font (23.70f, Font::plain).withTypefaceStyle ("Regular"));
    g.drawText ("Input Gain",    36,  44, 200, 30, Justification::centred, true);
    g.drawText ("Threshold",     228, 44, 200, 30, Justification::centred, true);
    g.drawText ("Attack Time",   444, 44, 200, 30, Justification::centred, true);
    g.drawText ("Release Time",  636, 44, 200, 30, Justification::centred, true);

    g.drawText ("Ratio",         36,  172, 200, 30, Justification::centred, true);
    g.drawText ("Makeup Gain",   236, 172, 200, 30, Justification::centred, true);
    g.drawText ("Knee Width",    444, 172, 200, 30, Justification::centred, true);
    g.drawText ("Upload/Download", 636, 172, 200, 30, Justification::centred, true);

    // Draw labels for new controls
    g.drawText ("Analogue",      36,  300, 200, 30, Justification::centred, true);
    g.drawText ("Detector",      236, 300, 200, 30, Justification::centred, true);
}

void CompreezorAudioProcessorEditor::resized()
{
    // Lay out the subcomponents in rows.  We offset the y positions slightly
    // lower than in the stock editor to make room for the new controls.
    detGainSlider->setBounds     ( 56,  56, 160, 112);
    thresholdSlider->setBounds   (256,  56, 160, 112);
    attackTimeSlider->setBounds  (464,  56, 160, 112);
    releaseTimeSlider->setBounds (656,  56, 160, 112);

    ratioSlider->setBounds       ( 56, 184, 160, 112);
    outputGainSlider->setBounds  (256, 184, 160, 112);
    kneeWidthSlider->setBounds   (464, 184, 160, 112);

    // Place upload and download buttons below knee width slider
    uploadButton->setBounds      (656, 210, 160, 25);
    downloadButton->setBounds    (656, 255, 160, 25);

    // New controls on the bottom row
    digitalAnalogButton->setBounds ( 56, 320, 160, 24);
    detectModeCombo->setBounds    (256, 320, 160, 24);
}

//==============================================================================
void CompreezorAudioProcessorEditor::sliderValueChanged (Slider* sliderThatWasMoved)
{
    // Update the processor's parameters when sliders move.  Note that the
    // processor stores its parameters in linear or dB form as appropriate.
    if (sliderThatWasMoved == detGainSlider.get())
    {
        processor.DetGain = static_cast<float> (pow (10.0, sliderThatWasMoved->getValue() / 20.0));
    }
    else if (sliderThatWasMoved == thresholdSlider.get())
    {
        processor.Threshold = static_cast<float> (sliderThatWasMoved->getValue());
    }
    else if (sliderThatWasMoved == attackTimeSlider.get())
    {
        processor.AttackTime = static_cast<float> (sliderThatWasMoved->getValue());
        processor.m_LeftDetector.setAttackTime (processor.AttackTime);
        processor.m_RightDetector.setAttackTime (processor.AttackTime);
    }
    else if (sliderThatWasMoved == releaseTimeSlider.get())
    {
        processor.ReleaseTime = static_cast<float> (sliderThatWasMoved->getValue());
        processor.m_LeftDetector.setReleaseTime (processor.ReleaseTime);
        processor.m_RightDetector.setReleaseTime (processor.ReleaseTime);
    }
    else if (sliderThatWasMoved == ratioSlider.get())
    {
        processor.Ratio = static_cast<float> (sliderThatWasMoved->getValue());
    }
    else if (sliderThatWasMoved == outputGainSlider.get())
    {
        processor.OutputGain = static_cast<float> (pow (10.0, sliderThatWasMoved->getValue() / 20.0));
    }
    else if (sliderThatWasMoved == kneeWidthSlider.get())
    {
        processor.KneeWidth = static_cast<float> (sliderThatWasMoved->getValue());
    }
}

//==============================================================================
void CompreezorAudioProcessorEditor::buttonClicked (Button* buttonThatWasClicked)
{
    // Handle digital/analogue toggle.  When toggled the processor's flag and
    // envelope detectors are updated on the fly.
    if (buttonThatWasClicked == digitalAnalogButton.get())
    {
        bool analogue = digitalAnalogButton->getToggleState();
        processor.DigitalAnalogue = analogue;
        processor.m_LeftDetector.setTCModeAnalog (analogue);
        processor.m_RightDetector.setTCModeAnalog (analogue);
        DBG (String ("Analogue TC mode set to ") + (analogue ? "ON" : "OFF"));
        return;
    }

    // Handle upload button
    if (buttonThatWasClicked == uploadButton.get())
    {
        const String hostName = "http://127.0.0.1:5000/";
        FileChooser chooser ("Select audio file for upload and split…", {}, "*.wav; *.mp3; *.aiff");
        if (chooser.browseForFileToOpen())
        {
            File file = chooser.getResult();
            API_Set_File_Upload fileUpload (file, hostName);
            if (fileUpload.runThread())
            {
                DBG ("Finished uploading file " + file.getFileName());
            }
            else
            {
                DBG ("File upload was cancelled by user");
            }
        }
        return;
    }

    // Handle download button
    if (buttonThatWasClicked == downloadButton.get())
    {
        URL fileUrl ("http://127.0.0.1:5000/download");
        std::unique_ptr<InputStream> fileStream (fileUrl.createInputStream (false));
        if (fileStream != nullptr)
        {
            File localFile (File::getSpecialLocation (File::userDesktopDirectory).getChildFile ("Separate.zip"));
            localFile.deleteFile();
            MemoryBlock mem;
            fileStream->readIntoMemoryBlock (mem);
            FileOutputStream out (localFile);
            out.write (mem.getData(), mem.getSize());
            DBG ("Downloaded zip file to " + localFile.getFullPathName());
        }
        else
        {
            AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon,
                                              "Download Error",
                                              "Failed to connect to server for download.");
        }
        return;
    }
}

//==============================================================================
void CompreezorAudioProcessorEditor::comboBoxChanged (ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == detectModeCombo.get())
    {
        int id = detectModeCombo->getSelectedId();
        // Convert the ID (1..3) into detector code (0..2)
        const UINT detectMode = static_cast<UINT> (juce::jlimit (0, 2, id - 1));
        processor.m_LeftDetector.setDetectMode (detectMode);
        processor.m_RightDetector.setDetectMode (detectMode);
        DBG (String ("Detector mode changed to ") + detectModeCombo->getText());
    }
}

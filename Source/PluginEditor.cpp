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

//==============================================================================
// Custom look‑and‑feel for drawing vintage‑style rotary knobs
class CompreezorAudioProcessorEditor::VintageLookAndFeel : public LookAndFeel_V4
{
public:
    void drawRotarySlider (Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, Slider& slider) override
    {
        auto radius  = (float) jmin (width / 2, height / 2) - 2.0f;
        auto centreX = (float) x + (float) width  * 0.5f;
        auto centreY = (float) y + (float) height * 0.5f;
        auto angle   = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // draw knob body with a radial gradient for a vintage look
        ColourGradient gradient (Colour (0xff333333), centreX, centreY - radius,
                                 Colour (0xff555555), centreX, centreY + radius, false);
        g.setGradientFill (gradient);
        g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

        // draw outline
        g.setColour (Colour (0xffaaaaaa));
        g.drawEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // draw pointer
        auto pointerLength   = radius * 0.8f;
        auto pointerThickness = 2.5f;
        Path p;
        p.addRectangle (-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength);
        p.applyTransform (AffineTransform::rotation (angle).translated (centreX, centreY));
        g.setColour (Colour (0xffdddddd));
        g.fillPath (p);
    }
};

//==============================================================================
// Simple gain reduction meter.  Polls the processor's GainReduction value and
// draws a horizontal bar proportional to the reduction (0–24 dB range).
class CompreezorAudioProcessorEditor::GainReductionMeter : public Component,
                                                           private Timer
{
public:
    explicit GainReductionMeter (CompreezorAudioProcessor& proc)
        : processor (proc)
    {
        startTimerHz (30); // update ~30 times per second
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff202020));

        // Draw frame
        g.setColour (Colour (0xff555555));
        g.drawRect (getLocalBounds().toFloat(), 1.0f);

        // Compute bar width based on level (clamped to 0–24 dB)
        const float maxDb  = 24.0f;
        const float fraction = juce::jlimit (0.0f, 1.0f, level / maxDb);

        auto bounds = getLocalBounds().reduced (2);
        auto barWidth = bounds.getWidth() * fraction;

        // Draw background
        g.setColour (Colour (0xff333333));
        g.fillRect (bounds);

        // Draw bar
        g.setColour (Colour (0xff4caf50)); // green bar
        g.fillRect (bounds.withWidth ((int) barWidth));

        // Draw dB text
        g.setColour (Colour (0xffcccccc));
        g.setFont (14.0f);
        g.drawText (String (level, 1) + " dB", bounds, Justification::centredRight, false);
    }

    void timerCallback() override
    {
        // Read current gain reduction from the processor and trigger repaint
        level = processor.GainReduction;
        repaint();
    }

private:
    CompreezorAudioProcessor& processor;
    float level = 0.0f;
};

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
    // Set up custom look‑and‑feel for vintage rotary knobs
    lookAndFeel = new VintageLookAndFeel();
    // Assign the custom look to all rotary sliders
    detGainSlider->setLookAndFeel (lookAndFeel);
    thresholdSlider->setLookAndFeel (lookAndFeel);
    attackTimeSlider->setLookAndFeel (lookAndFeel);
    releaseTimeSlider->setLookAndFeel (lookAndFeel);
    ratioSlider->setLookAndFeel (lookAndFeel);
    outputGainSlider->setLookAndFeel (lookAndFeel);
    kneeWidthSlider->setLookAndFeel (lookAndFeel);

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
    // Labels for analogue toggle and detector mode.  We create dedicated
    // Label components rather than drawing text in paint() so we can control
    // their layout in resized() and avoid overlapping other elements.
    analogueLabel = std::make_unique<Label>();
    analogueLabel->setText ("Analogue", dontSendNotification);
    analogueLabel->setJustificationType (Justification::centred);
    addAndMakeVisible (analogueLabel.get());

    detectorLabel = std::make_unique<Label>();
    detectorLabel->setText ("Detector", dontSendNotification);
    detectorLabel->setJustificationType (Justification::centred);
    addAndMakeVisible (detectorLabel.get());

    gainReductionLabel = std::make_unique<Label>();
    gainReductionLabel->setText ("Gain Reduction", dontSendNotification);
    gainReductionLabel->setJustificationType (Justification::centred);
    addAndMakeVisible (gainReductionLabel.get());

    // --------------------------------------------------------------------------
    // Upload and download buttons
    uploadButton   = std::make_unique<TextButton> ("Upload");
    uploadButton->addListener (this);
    addAndMakeVisible (uploadButton.get());

    downloadButton = std::make_unique<TextButton> ("Download Split");
    downloadButton->addListener (this);
    addAndMakeVisible (downloadButton.get());

    // --------------------------------------------------------------------------
    // Create the gain reduction meter component and add it to the editor
    gainReductionMeter = std::make_unique<GainReductionMeter> (processor);
    addAndMakeVisible (gainReductionMeter.get());

    // --------------------------------------------------------------------------
    // Set a comfortable size for the UI.  Increase the height to give the bottom
    // row extra breathing room for the analogue toggle and detector combo.
    setSize (880, 420);
}

CompreezorAudioProcessorEditor::~CompreezorAudioProcessorEditor()
{
    // Reset look‑and‑feel on sliders before deleting our custom look
    if (lookAndFeel != nullptr)
    {
        detGainSlider->setLookAndFeel (nullptr);
        thresholdSlider->setLookAndFeel (nullptr);
        attackTimeSlider->setLookAndFeel (nullptr);
        releaseTimeSlider->setLookAndFeel (nullptr);
        ratioSlider->setLookAndFeel (nullptr);
        outputGainSlider->setLookAndFeel (nullptr);
        kneeWidthSlider->setLookAndFeel (nullptr);
        delete lookAndFeel;
        lookAndFeel = nullptr;
    }

    // Unique pointers automatically clean up other owned objects
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

    // The analogue and detector captions are handled by Label components created in
    // the constructor and positioned in resized(), so we do not draw them here.
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

    // Labels and controls on the bottom row
    analogueLabel->setBounds     ( 56, 300, 160, 20);
    digitalAnalogButton->setBounds ( 56, 320, 160, 24);
    detectorLabel->setBounds      (256, 300, 160, 20);
    detectModeCombo->setBounds    (256, 320, 160, 24);

    // Gain reduction meter positioned on the right of the bottom row
    gainReductionLabel->setBounds (656, 300, 160, 20);
    if (gainReductionMeter)
        gainReductionMeter->setBounds (656, 320, 160, 44);
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

    // Handle upload button using asynchronous file chooser.  When the user selects
    // a file, we launch the upload thread and check for cancellation after it
    // completes.  The fileChooser member persists while the chooser is open.
    if (buttonThatWasClicked == uploadButton.get())
    {
        fileChooser = std::make_unique<FileChooser> ("Select audio file for upload and split…",
                                                     File{},
                                                     "*.wav; *.mp3; *.aiff");

        auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync (chooserFlags, [this] (const FileChooser& chooser)
        {
            File file = chooser.getResult();

            if (file != File{})
            {
                const String hostName = "http://127.0.0.1:5000/";
                API_Set_File_Upload fileUpload (file, hostName);

                // launchThread() é uma chamada bloqueante que exibe uma janela de progresso.
                // Retorna void, então não podemos usá-la diretamente em um if.
                fileUpload.launchThread();

                // Após a janela da thread ser fechada, verificamos se foi cancelada.
                if (!fileUpload.wasCancelled())
                {
                    DBG ("Finished uploading file " + file.getFileName());
                }
                else
                {
                    DBG ("File upload was cancelled by user");
                }
            }
        });
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

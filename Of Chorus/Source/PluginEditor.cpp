/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
OfChorusAudioProcessorEditor::OfChorusAudioProcessorEditor (OfChorusAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);
    
    auto& params = processor.getParameters();
    
    // Setting up dry/wet slider
    juce::AudioParameterFloat* dryWetParameter = (juce::AudioParameterFloat*) params.getUnchecked(0);
    
    mDryWetSlider.setBounds(0, 0, 100, 100);
    mDryWetSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    mDryWetSlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    mDryWetSlider.setRange(dryWetParameter->range.start, dryWetParameter->range.end);
    mDryWetSlider.setValue(*dryWetParameter);
    addAndMakeVisible(mDryWetSlider);
    
    mDryWetSlider.onValueChange = [this, dryWetParameter] {
        *dryWetParameter = mDryWetSlider.getValue();
    };
    mDryWetSlider.onDragStart = [dryWetParameter] {
        dryWetParameter->beginChangeGesture();
    };
    mDryWetSlider.onDragEnd = [dryWetParameter] {
        dryWetParameter->endChangeGesture();
    };
    
    // Setting up feedback slider
    juce::AudioParameterFloat* feedbackParameter = (juce::AudioParameterFloat*) params.getUnchecked(1);
    
    mFeedbackSlider.setBounds(100, 0, 100, 100);
    mFeedbackSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    mFeedbackSlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    mFeedbackSlider.setRange(feedbackParameter->range.start, feedbackParameter->range.end);
    mFeedbackSlider.setValue(*feedbackParameter);
    addAndMakeVisible(mFeedbackSlider);
    
    mFeedbackSlider.onValueChange = [this, feedbackParameter] {
        *feedbackParameter = mFeedbackSlider.getValue();
    };
    mFeedbackSlider.onDragStart = [feedbackParameter] {
        feedbackParameter->beginChangeGesture();
    };
    mFeedbackSlider.onDragEnd = [feedbackParameter] {
        feedbackParameter->endChangeGesture();
    };
}

OfChorusAudioProcessorEditor::~OfChorusAudioProcessorEditor()
{
}

//==============================================================================
void OfChorusAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void OfChorusAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}

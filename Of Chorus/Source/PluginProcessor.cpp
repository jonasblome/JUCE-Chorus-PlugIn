/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
OfChorusAudioProcessor::OfChorusAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    addParameter(mDryWetParameter = new juce::AudioParameterFloat("drywet", "Dry/Wet", 0.0, 1.0, 0.5));
    addParameter(mDepthParameter = new juce::AudioParameterFloat("depth", "Depth", 0.0, 1.0, 0.5));
    addParameter(mRateParameter = new juce::AudioParameterFloat("rate", "Rate", 0.1f, 20.0f, 10.0f));
    addParameter(mPhaseOffsetParameter = new juce::AudioParameterFloat("phaseoffset", "Phase Offset", 0.0f, 1.0f, 0.0f));
    addParameter(mFeedbackParameter = new juce::AudioParameterFloat("feedback", "Feedback", 0.0, 0.98, 0.5));
    addParameter(mTypeParameter = new juce::AudioParameterInt ("type", "Type", 0, 1, 0));
    
    mLFOPhase = 0;
    
    mCircularBufferLeft = nullptr;
    mCircularBufferRight = nullptr;
    
    mCircularBufferWriteHead = 0;
    mCircularBufferLength = 0;
    
    mFeedbackLeft = 0;
    mFeedbackRight = 0;
}

OfChorusAudioProcessor::~OfChorusAudioProcessor()
{
    if(mCircularBufferLeft != nullptr) {
        delete [] mCircularBufferLeft;
        mCircularBufferLeft = nullptr;
    }
    if(mCircularBufferRight != nullptr) {
        delete [] mCircularBufferRight;
        mCircularBufferRight = nullptr;
    }
}

//==============================================================================
const juce::String OfChorusAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool OfChorusAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool OfChorusAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool OfChorusAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double OfChorusAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int OfChorusAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int OfChorusAudioProcessor::getCurrentProgram()
{
    return 0;
}

void OfChorusAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String OfChorusAudioProcessor::getProgramName (int index)
{
    return {};
}

void OfChorusAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void OfChorusAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mLFOPhase = 0;
    
    mCircularBufferLength = sampleRate * MAX_DELAY_TIME;
    
    if(mCircularBufferLeft == nullptr) {
        mCircularBufferLeft = new float[mCircularBufferLength];
    }
    
    juce::zeromem(mCircularBufferLeft, mCircularBufferLength * sizeof(float));
    
    if(mCircularBufferRight == nullptr) {
        mCircularBufferRight = new float[mCircularBufferLength];
    }
    
    juce::zeromem(mCircularBufferRight, mCircularBufferLength * sizeof(float));

    mCircularBufferWriteHead = 0;
}

void OfChorusAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool OfChorusAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void OfChorusAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);
    
    for(int i = 0; i < buffer.getNumSamples(); i++) {
        // Writing to buffer and adding feedback
        mCircularBufferLeft[mCircularBufferWriteHead] = leftChannel[i] + mFeedbackLeft;
        mCircularBufferRight[mCircularBufferWriteHead] = rightChannel[i] + mFeedbackRight;
        
        // Calculating delay for left channel with LFO
        float lfoOutLeft = sin(2 * M_PI * mLFOPhase);
        lfoOutLeft *= *mDepthParameter;
        
        // Calculating delay for right channel with LFO + offset
        float lfoPhaseRight = mLFOPhase + *mPhaseOffsetParameter;
        
        if(lfoPhaseRight > 1) {
            lfoPhaseRight -= 1;
        }
        
        float lfoOutRight = sin(2 * M_PI * lfoPhaseRight);
        lfoOutRight *= *mDepthParameter;
        
        float lfoOutMappedLeft = 0;
        float lfoOutMappedRight = 0;
        
        // Updating LFO phase
        mLFOPhase += *mRateParameter / getSampleRate();
        
        // Chorus effect
        if (*mTypeParameter == 0) {
            lfoOutMappedLeft = juce::jmap<float>(lfoOutLeft, -1.f, 1.f, 0.005f, 0.03f);
            lfoOutMappedRight = juce::jmap<float>(lfoOutRight, -1.f, 1.f, 0.005f, 0.03f);
        }
        // Flanger effect
        else {
            lfoOutMappedLeft = juce::jmap<float>(lfoOutLeft, -1.f, 1.f, 0.001f, 0.005f);
            lfoOutMappedRight = juce::jmap<float>(lfoOutRight, -1.f, 1.f, 0.001f, 0.005f);
        }
        
        float delayTimeSamplesLeft = getSampleRate() * lfoOutMappedLeft;
        float delayTimeSamplesRight = getSampleRate() * lfoOutMappedRight;
        
        if(mLFOPhase > 1) {
            mLFOPhase -= 1;
        }
        
        // Calculating read head for left channel delay sample
        float delayReadHeadLeft = mCircularBufferWriteHead - delayTimeSamplesLeft;
        
        if(delayReadHeadLeft < 0) {
            delayReadHeadLeft += mCircularBufferLength;
        }
        
        int readHeadLeft_x = (int) delayReadHeadLeft;
        int readHeadLeft_x1 = readHeadLeft_x + 1;
        float readHeadFloatLeft = delayReadHeadLeft - readHeadLeft_x;
        
        if(readHeadLeft_x1 >= mCircularBufferLength) {
            readHeadLeft_x1 -= mCircularBufferLength;
        }
        
        // Calculating read head for right channel delay sample
        float delayReadHeadRight = mCircularBufferWriteHead - delayTimeSamplesRight;
        
        if(delayReadHeadRight < 0) {
            delayReadHeadRight += mCircularBufferLength;
        }
        
        int readHeadRight_x = (int) delayReadHeadRight;
        int readHeadRight_x1 = readHeadRight_x + 1;
        float readHeadFloatRight = delayReadHeadRight - readHeadRight_x;
        
        if(readHeadRight_x1 >= mCircularBufferLength) {
            readHeadRight_x1 -= mCircularBufferLength;
        }
        
        // Interpolating delay samples from current read head positions for both channels
        float delay_sample_left = lin_interp(mCircularBufferLeft[readHeadLeft_x], mCircularBufferLeft[readHeadLeft_x1], readHeadFloatLeft);
        float delay_sample_right = lin_interp(mCircularBufferRight[readHeadRight_x], mCircularBufferRight[readHeadRight_x1], readHeadFloatRight);
        
        // Calculating feedback samples
        mFeedbackLeft = delay_sample_left * *mFeedbackParameter;
        mFeedbackRight = delay_sample_right * *mFeedbackParameter;
        
        // Updating buffer write head
        mCircularBufferWriteHead++;

        if(mCircularBufferWriteHead >= mCircularBufferLength) {
            mCircularBufferWriteHead = 0;
        }
        
        // Mixing sample between dry and wet signal
        float dryAmount = 1 - *mDryWetParameter;
        float wetAmount = *mDryWetParameter;
        
        buffer.setSample(0, i, buffer.getSample(0, i) * dryAmount + delay_sample_left * wetAmount);
        buffer.setSample(1, i, buffer.getSample(1, i) * dryAmount + delay_sample_right * wetAmount);
    }
}

//==============================================================================
bool OfChorusAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* OfChorusAudioProcessor::createEditor()
{
    return new OfChorusAudioProcessorEditor (*this);
}

//==============================================================================
void OfChorusAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    std::unique_ptr<juce::XmlElement> xml(new juce::XmlElement("FlangerChorus"));
    
    xml->setAttribute("DryWet", *mDryWetParameter);
    xml->setAttribute("Depth", *mDepthParameter);
    xml->setAttribute("Rate", *mRateParameter);
    xml->setAttribute("PhaseOffset", *mPhaseOffsetParameter);
    xml->setAttribute("Feedback", *mFeedbackParameter);
    xml->setAttribute("Type", *mTypeParameter);
    
    copyXmlToBinary(*xml, destData);
}

void OfChorusAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    
    if(xml.get() != nullptr && xml->hasTagName("FlangerChorus")) {
        *mDryWetParameter = xml->getDoubleAttribute("DryWet");
        *mDepthParameter = xml->getDoubleAttribute("Depth");
        *mRateParameter = xml->getDoubleAttribute("Rate");
        *mPhaseOffsetParameter = xml->getDoubleAttribute("PhaseOffset");
        *mFeedbackParameter = xml->getDoubleAttribute("Feedback");
        *mTypeParameter = xml->getIntAttribute("Type");
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OfChorusAudioProcessor();
}

float OfChorusAudioProcessor::lin_interp(float sample_x, float sample_x1, float inPhase)
{
    return (1 - inPhase) * sample_x + inPhase * sample_x1;
}

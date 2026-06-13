#include "TextEditProcessor.h"

#include "TextEditEditor.h"

namespace {

constexpr auto kStateTag = "TEXTEDIT_STATE";
constexpr auto kTextProperty = "text";

juce::String defaultDocumentText()
{
    return "@ textedit voicegroup editor\n"
           "@ Language service: embedded\n\n"
           "\tvoice_directsound 60, 0, DirectSoundWaveData_piano, 255, 0, 255, 127\n";
}

} // namespace

TextEditProcessor::TextEditProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      documentText(defaultDocumentText())
{
}

void TextEditProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void TextEditProcessor::releaseResources()
{
}

bool TextEditProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainInput = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    return mainInput == mainOutput
        && (mainOutput == juce::AudioChannelSet::mono()
            || mainOutput == juce::AudioChannelSet::stereo());
}

void TextEditProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    processAudio(buffer, midiMessages);
}

void TextEditProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    processAudio(buffer, midiMessages);
}

template <typename FloatType>
void TextEditProcessor::processAudio(juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* TextEditProcessor::createEditor()
{
    return new TextEditEditor(*this);
}

bool TextEditProcessor::hasEditor() const
{
    return true;
}

const juce::String TextEditProcessor::getName() const
{
    return "textedit";
}

bool TextEditProcessor::acceptsMidi() const
{
    return false;
}

bool TextEditProcessor::producesMidi() const
{
    return false;
}

bool TextEditProcessor::isMidiEffect() const
{
    return false;
}

double TextEditProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TextEditProcessor::getNumPrograms()
{
    return 1;
}

int TextEditProcessor::getCurrentProgram()
{
    return 0;
}

void TextEditProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String TextEditProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void TextEditProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void TextEditProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree state(kStateTag);
    state.setProperty(kTextProperty, getDocumentText(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void TextEditProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(kStateTag))
        return;

    juce::ValueTree state = juce::ValueTree::fromXml(*xml);
    if (state.isValid())
        setDocumentText(state.getProperty(kTextProperty, defaultDocumentText()).toString());
}

juce::String TextEditProcessor::getDocumentText() const
{
    const juce::ScopedLock lock(documentLock);
    return documentText;
}

void TextEditProcessor::setDocumentText(const juce::String& newText)
{
    {
        const juce::ScopedLock lock(documentLock);
        if (documentText == newText)
            return;

        documentText = newText;
    }

    sendChangeMessage();
}

void TextEditProcessor::addDocumentChangeListener(juce::ChangeListener* listener)
{
    addChangeListener(listener);
}

void TextEditProcessor::removeDocumentChangeListener(juce::ChangeListener* listener)
{
    removeChangeListener(listener);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TextEditProcessor();
}

#include "VoicegroupTokeniser.h"

#include "GruvboxTheme.h"

namespace {

bool isIdentifierStart(juce::juce_wchar c)
{
    return juce::CharacterFunctions::isLetter(c) || c == '_' || c == '.';
}

bool isIdentifierBody(juce::juce_wchar c)
{
    return juce::CharacterFunctions::isLetterOrDigit(c) || c == '_' || c == '.';
}

bool isMacroName(const juce::String& token)
{
    static const char* const names[] = {
        "voice_directsound",
        "voice_directsound_no_resample",
        "voice_directsound_alt",
        "voice_square_1",
        "voice_square_2",
        "voice_programmable_wave",
        "voice_noise",
        "keysplit",
        "voicegroup",
        "DirectSound"
    };

    for (auto* name : names)
        if (token == name)
            return true;

    return token.startsWith("voice_");
}

bool isDirective(const juce::String& token)
{
    return token == ".include" || token == ".global" || token == ".section"
        || token == ".align" || token == ".byte" || token == ".2byte"
        || token == ".4byte" || token == ".word" || token == ".end";
}

} // namespace

int VoicegroupTokeniser::readNextToken(juce::CodeDocument::Iterator& source)
{
    auto c = source.peekNextChar();

    if (c == 0)
        return tokenType_identifier;

    if (juce::CharacterFunctions::isWhitespace(c))
    {
        source.skipWhitespace();
        return tokenType_identifier;
    }

    if (c == '@' || c == ';')
    {
        source.skipToEndOfLine();
        return tokenType_comment;
    }

    if (c == '"')
    {
        source.skip();
        while (!source.isEOF())
        {
            const auto next = source.nextChar();
            if (next == '\\' && !source.isEOF())
            {
                source.skip();
                continue;
            }
            if (next == '"')
                break;
        }
        return tokenType_string;
    }

    if (juce::CharacterFunctions::isDigit(c) || c == '-' || c == '+')
    {
        bool sawDigit = false;
        if (c == '-' || c == '+')
            source.skip();

        while (juce::CharacterFunctions::isLetterOrDigit(source.peekNextChar())
               || source.peekNextChar() == 'x')
        {
            sawDigit = true;
            source.skip();
        }

        return sawDigit ? tokenType_number : tokenType_operator;
    }

    if (isIdentifierStart(c))
    {
        juce::String token;
        while (isIdentifierBody(source.peekNextChar()))
            token << juce::String::charToString(source.nextChar());

        if (isMacroName(token))
            return tokenType_macro;
        if (isDirective(token))
            return tokenType_keyword;
        return tokenType_identifier;
    }

    source.skip();

    if (c == ',' || c == ':' || c == '(' || c == ')' || c == '[' || c == ']')
        return tokenType_punctuation;

    return tokenType_operator;
}

juce::CodeEditorComponent::ColourScheme VoicegroupTokeniser::getDefaultColourScheme()
{
    return GruvboxTheme::codeColourScheme();
}

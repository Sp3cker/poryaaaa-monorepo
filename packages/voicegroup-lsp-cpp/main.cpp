#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

struct Json {
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;

    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value;

    Json() : value(nullptr) {}
    Json(std::nullptr_t) : value(nullptr) {}
    Json(bool v) : value(v) {}
    Json(int v) : value(static_cast<double>(v)) {}
    Json(double v) : value(v) {}
    Json(std::string v) : value(std::move(v)) {}
    Json(const char *v) : value(std::string(v)) {}
    Json(Array v) : value(std::move(v)) {}
    Json(Object v) : value(std::move(v)) {}

    const Object *object() const { return std::get_if<Object>(&value); }
    const Array *array() const { return std::get_if<Array>(&value); }
    const std::string *string() const { return std::get_if<std::string>(&value); }
    const double *number() const { return std::get_if<double>(&value); }

    const Json *get(const std::string &key) const
    {
        const auto *obj = object();
        if (!obj) {
            return nullptr;
        }
        auto it = obj->find(key);
        return it == obj->end() ? nullptr : &it->second;
    }
};

class JsonParser {
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    std::optional<Json> parse()
    {
        skipWhitespace();
        auto value = parseValue();
        skipWhitespace();
        if (!value || pos_ != text_.size()) {
            return std::nullopt;
        }
        return value;
    }

private:
    std::string text_;
    size_t pos_ = 0;

    std::optional<Json> parseValue()
    {
        skipWhitespace();
        if (pos_ >= text_.size()) {
            return std::nullopt;
        }
        char c = text_[pos_];
        if (c == '"') {
            return parseString();
        }
        if (c == '{') {
            return parseObject();
        }
        if (c == '[') {
            return parseArray();
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            return parseNumber();
        }
        if (consume("true")) {
            return Json(true);
        }
        if (consume("false")) {
            return Json(false);
        }
        if (consume("null")) {
            return Json(nullptr);
        }
        return std::nullopt;
    }

    std::optional<Json> parseObject()
    {
        ++pos_;
        Json::Object object;
        skipWhitespace();
        if (peek('}')) {
            ++pos_;
            return Json(std::move(object));
        }
        while (pos_ < text_.size()) {
            auto key = parseString();
            if (!key || !key->string()) {
                return std::nullopt;
            }
            skipWhitespace();
            if (!peek(':')) {
                return std::nullopt;
            }
            ++pos_;
            auto value = parseValue();
            if (!value) {
                return std::nullopt;
            }
            object.emplace(*key->string(), std::move(*value));
            skipWhitespace();
            if (peek('}')) {
                ++pos_;
                return Json(std::move(object));
            }
            if (!peek(',')) {
                return std::nullopt;
            }
            ++pos_;
            skipWhitespace();
        }
        return std::nullopt;
    }

    std::optional<Json> parseArray()
    {
        ++pos_;
        Json::Array array;
        skipWhitespace();
        if (peek(']')) {
            ++pos_;
            return Json(std::move(array));
        }
        while (pos_ < text_.size()) {
            auto value = parseValue();
            if (!value) {
                return std::nullopt;
            }
            array.push_back(std::move(*value));
            skipWhitespace();
            if (peek(']')) {
                ++pos_;
                return Json(std::move(array));
            }
            if (!peek(',')) {
                return std::nullopt;
            }
            ++pos_;
            skipWhitespace();
        }
        return std::nullopt;
    }

    std::optional<Json> parseString()
    {
        if (!peek('"')) {
            return std::nullopt;
        }
        ++pos_;
        std::string result;
        while (pos_ < text_.size()) {
            char c = text_[pos_++];
            if (c == '"') {
                return Json(result);
            }
            if (c != '\\') {
                result.push_back(c);
                continue;
            }
            if (pos_ >= text_.size()) {
                return std::nullopt;
            }
            char escaped = text_[pos_++];
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                result.push_back(escaped);
                break;
            case 'b':
                result.push_back('\b');
                break;
            case 'f':
                result.push_back('\f');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            case 'u':
                if (pos_ + 4 > text_.size()) {
                    return std::nullopt;
                }
                result.push_back('?');
                pos_ += 4;
                break;
            default:
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<Json> parseNumber()
    {
        size_t start = pos_;
        if (peek('-')) {
            ++pos_;
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
        if (peek('.')) {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }
        return Json(std::strtod(text_.c_str() + start, nullptr));
    }

    void skipWhitespace()
    {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    bool peek(char c) const { return pos_ < text_.size() && text_[pos_] == c; }

    bool consume(const char *word)
    {
        const size_t length = std::char_traits<char>::length(word);
        if (text_.compare(pos_, length, word) != 0) {
            return false;
        }
        pos_ += length;
        return true;
    }
};

std::string jsonEscape(const std::string &value)
{
    std::string result;
    for (char c : value) {
        switch (c) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result.push_back(c);
            break;
        }
    }
    return result;
}

std::string jsonStringify(const Json &json)
{
    if (std::holds_alternative<std::nullptr_t>(json.value)) {
        return "null";
    }
    if (const auto *value = std::get_if<bool>(&json.value)) {
        return *value ? "true" : "false";
    }
    if (const auto *value = json.number()) {
        long asInteger = static_cast<long>(*value);
        if (static_cast<double>(asInteger) == *value) {
            return std::to_string(asInteger);
        }
        return std::to_string(*value);
    }
    if (const auto *value = json.string()) {
        return "\"" + jsonEscape(*value) + "\"";
    }
    if (const auto *array = json.array()) {
        std::string result = "[";
        for (size_t i = 0; i < array->size(); ++i) {
            if (i != 0) {
                result += ",";
            }
            result += jsonStringify((*array)[i]);
        }
        result += "]";
        return result;
    }
    const auto *object = json.object();
    std::string result = "{";
    bool first = true;
    for (const auto &[key, value] : *object) {
        if (!first) {
            result += ",";
        }
        first = false;
        result += "\"" + jsonEscape(key) + "\":" + jsonStringify(value);
    }
    result += "}";
    return result;
}

struct SourcePosition {
    int line = 0;
    int character = 0;
};

struct SourceRange {
    SourcePosition start;
    SourcePosition end;
};

struct Diagnostic {
    SourceRange range;
    int severity = 1;
    std::string code;
    std::string message;
};

enum class ArgumentKind {
    Integer,
    DirectSoundSymbol,
    ProgrammableWaveSymbol,
    VoicegroupSymbol,
    KeysplitSymbol,
};

struct NumericRange {
    int min = 0;
    int max = 0;
};

struct MacroArgument {
    std::string name;
    ArgumentKind kind;
    std::optional<NumericRange> validRange;
};

struct MacroDefinition {
    std::string name;
    std::vector<MacroArgument> arguments;
};

struct VoiceArgument {
    std::string text;
    SourceRange range;
};

struct VoiceMacroCall {
    std::string macroName;
    SourceRange macroRange;
    std::vector<VoiceArgument> arguments;
    int slot = 0;
};

struct VoicegroupDeclaration {
    std::string name;
    int startingNote = 0;
    SourceRange range;
};

struct ParsedDocument {
    std::vector<VoicegroupDeclaration> declarations;
    std::vector<VoiceMacroCall> calls;
    std::vector<Diagnostic> diagnostics;
};

struct IndexedSymbol {
    std::string name;
    SourceRange range;
};

struct WorkspaceIndex {
    std::unordered_map<std::string, IndexedSymbol> directSound;
    std::unordered_map<std::string, IndexedSymbol> programmableWave;
    std::unordered_map<std::string, IndexedSymbol> keysplits;
    std::unordered_map<std::string, IndexedSymbol> voicegroups;
};

std::string trim(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

bool startsWith(const std::string &value, const std::string &prefix)
{
    return value.rfind(prefix, 0) == 0;
}

std::optional<int> parseInteger(const std::string &text)
{
    char *end = nullptr;
    long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

std::vector<std::string> splitLines(const std::string &text)
{
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return lines;
}

SourceRange rangeFor(int line, int startColumn, int endColumn)
{
    return {{line, startColumn}, {line, endColumn}};
}

const std::unordered_map<std::string, MacroDefinition> &macroCatalog()
{
    static const std::unordered_map<std::string, MacroDefinition> catalog = [] {
        std::unordered_map<std::string, MacroDefinition> result;
        auto integer = [](std::string name, int min, int max) {
            return MacroArgument{std::move(name), ArgumentKind::Integer, NumericRange{min, max}};
        };
        const auto baseKey = integer("base_midi_key", 0, 127);
        const auto pan = integer("pan", 0, 127);
        const std::vector<MacroArgument> envelope = {
            integer("attack", 0, 7),
            integer("decay", 0, 7),
            integer("sustain", 0, 15),
            integer("release", 0, 7),
        };
        auto add = [&](std::string name, std::vector<MacroArgument> args) {
            result.emplace(name, MacroDefinition{name, std::move(args)});
        };
        auto directSound = [&](const std::string &name) {
            add(name, {
                          baseKey,
                          pan,
                          {"sample_data_pointer", ArgumentKind::DirectSoundSymbol, std::nullopt},
                          integer("attack", 0, 255),
                          integer("decay", 0, 255),
                          integer("sustain", 0, 255),
                          integer("release", 0, 255),
                      });
        };
        auto square1 = [&](const std::string &name) {
            std::vector<MacroArgument> args = {baseKey, pan, integer("sweep", 0, 255), integer("duty_cycle", 0, 3)};
            args.insert(args.end(), envelope.begin(), envelope.end());
            add(name, args);
        };
        auto square2 = [&](const std::string &name) {
            std::vector<MacroArgument> args = {baseKey, pan, integer("duty_cycle", 0, 3)};
            args.insert(args.end(), envelope.begin(), envelope.end());
            add(name, args);
        };
        auto progWave = [&](const std::string &name) {
            std::vector<MacroArgument> args = {baseKey, pan, {"wave_samples_pointer", ArgumentKind::ProgrammableWaveSymbol, std::nullopt}};
            args.insert(args.end(), envelope.begin(), envelope.end());
            add(name, args);
        };
        auto noise = [&](const std::string &name) {
            std::vector<MacroArgument> args = {baseKey, pan, integer("period", 0, 1)};
            args.insert(args.end(), envelope.begin(), envelope.end());
            add(name, args);
        };
        directSound("voice_directsound_no_resample");
        directSound("voice_directsound_alt");
        directSound("voice_directsound");
        square1("voice_square_1_alt");
        square1("voice_square_1");
        square2("voice_square_2_alt");
        square2("voice_square_2");
        progWave("voice_programmable_wave_alt");
        progWave("voice_programmable_wave");
        noise("voice_noise_alt");
        noise("voice_noise");
        add("voice_keysplit_all", {{"voice_group_pointer", ArgumentKind::VoicegroupSymbol, std::nullopt}});
        add("voice_keysplit", {
                                  {"voice_group_pointer", ArgumentKind::VoicegroupSymbol, std::nullopt},
                                  {"keysplit_table_pointer", ArgumentKind::KeysplitSymbol, std::nullopt},
                              });
        add("cry_reverse", {{"sample", ArgumentKind::DirectSoundSymbol, std::nullopt}});
        add("cry", {{"sample", ArgumentKind::DirectSoundSymbol, std::nullopt}});
        return result;
    }();
    return catalog;
}

std::vector<VoiceArgument> splitArguments(const std::string &text, int line, int startColumn)
{
    std::vector<VoiceArgument> result;
    size_t argStart = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i != text.size() && text[i] != ',') {
            continue;
        }
        std::string raw = text.substr(argStart, i - argStart);
        std::string value = trim(raw);
        if (!value.empty() || (i == text.size() && !text.empty() && text.back() == ',')) {
            size_t leading = 0;
            while (leading < raw.size() && std::isspace(static_cast<unsigned char>(raw[leading]))) {
                ++leading;
            }
            int start = startColumn + static_cast<int>(argStart + leading);
            result.push_back({value, rangeFor(line, start, start + static_cast<int>(value.size()))});
        }
        argStart = i + 1;
    }
    return result;
}

ParsedDocument parseVoicegroupDocument(const std::string &text)
{
    ParsedDocument document;
    int voiceIndex = 0;
    const auto &macros = macroCatalog();
    auto lines = splitLines(text);

    for (size_t lineNumber = 0; lineNumber < lines.size(); ++lineNumber) {
        const std::string &line = lines[lineNumber];
        std::string code = line.substr(0, line.find('@'));
        size_t trimmedStart = 0;
        while (trimmedStart < code.size() && std::isspace(static_cast<unsigned char>(code[trimmedStart]))) {
            ++trimmedStart;
        }
        std::string trimmed = trim(code.substr(trimmedStart));
        if (trimmed.empty()) {
            continue;
        }

        int startColumn = static_cast<int>(trimmedStart);
        if (startsWith(trimmed, "voice_group")) {
            SourceRange range = rangeFor(static_cast<int>(lineNumber), startColumn, static_cast<int>(code.size()));
            std::string rest = trim(trimmed.substr(std::string("voice_group").size()));
            if (rest.empty()) {
                document.diagnostics.push_back({range, 1, "malformed-voice-group", "voice_group requires a label."});
                continue;
            }
            size_t comma = rest.find(',');
            std::string name = trim(rest.substr(0, comma));
            int startingNote = 0;
            if (comma != std::string::npos) {
                auto parsed = parseInteger(trim(rest.substr(comma + 1)));
                if (!parsed || *parsed < 0 || *parsed >= 128) {
                    document.diagnostics.push_back({range, 1, "invalid-starting-note", "voice_group starting note must be in 0...127."});
                    continue;
                }
                startingNote = *parsed;
            }
            document.declarations.push_back({name, startingNote, range});
            voiceIndex = startingNote;
            continue;
        }

        size_t macroEnd = 0;
        while (macroEnd < trimmed.size() && !std::isspace(static_cast<unsigned char>(trimmed[macroEnd]))) {
            ++macroEnd;
        }
        if (macroEnd == trimmed.size()) {
            document.diagnostics.push_back({rangeFor(static_cast<int>(lineNumber), startColumn, static_cast<int>(code.size())), 1, "unknown-line", "Expected a voice macro call."});
            continue;
        }

        std::string macroName = trimmed.substr(0, macroEnd);
        if (macros.find(macroName) == macros.end()) {
            document.diagnostics.push_back({rangeFor(static_cast<int>(lineNumber), startColumn, startColumn + static_cast<int>(macroName.size())), 1, "unknown-macro", "Unknown voice macro '" + macroName + "'."});
            continue;
        }

        size_t argsStart = macroEnd;
        while (argsStart < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[argsStart]))) {
            ++argsStart;
        }
        document.calls.push_back({
            macroName,
            rangeFor(static_cast<int>(lineNumber), startColumn, startColumn + static_cast<int>(macroName.size())),
            splitArguments(trimmed.substr(argsStart), static_cast<int>(lineNumber), startColumn + static_cast<int>(argsStart)),
            voiceIndex,
        });
        ++voiceIndex;
    }

    return document;
}

std::vector<Diagnostic> analyzeDocument(const ParsedDocument &document, const WorkspaceIndex &workspace)
{
    std::vector<Diagnostic> diagnostics = document.diagnostics;
    const auto &macros = macroCatalog();

    for (const auto &call : document.calls) {
        auto definition = macros.find(call.macroName);
        if (definition == macros.end()) {
            continue;
        }
        if (call.arguments.size() != definition->second.arguments.size()) {
            diagnostics.push_back({call.macroRange, 1, "wrong-argument-count", call.macroName + " expects " + std::to_string(definition->second.arguments.size()) + " arguments, got " + std::to_string(call.arguments.size()) + "."});
            continue;
        }
        if (call.slot < 0 || call.slot >= 128) {
            diagnostics.push_back({call.macroRange, 1, "slot-out-of-range", "Voice slot " + std::to_string(call.slot) + " is outside the 0...127 voicegroup range."});
        }

        for (size_t i = 0; i < call.arguments.size(); ++i) {
            const auto &argument = call.arguments[i];
            const auto &expected = definition->second.arguments[i];
            switch (expected.kind) {
            case ArgumentKind::Integer: {
                auto value = parseInteger(argument.text);
                if (!value) {
                    diagnostics.push_back({argument.range, 1, "invalid-integer", expected.name + " must be an integer."});
                } else if (expected.validRange && (*value < expected.validRange->min || *value > expected.validRange->max)) {
                    diagnostics.push_back({argument.range, 1, "invalid-range", expected.name + " should be in " + std::to_string(expected.validRange->min) + "..." + std::to_string(expected.validRange->max) + "."});
                }
                break;
            }
            case ArgumentKind::DirectSoundSymbol:
                if (!workspace.directSound.empty() && workspace.directSound.find(argument.text) == workspace.directSound.end()) {
                    diagnostics.push_back({argument.range, 1, "unknown-directsound-symbol", "Unknown DirectSound symbol '" + argument.text + "'."});
                }
                break;
            case ArgumentKind::ProgrammableWaveSymbol:
                if (!workspace.programmableWave.empty() && workspace.programmableWave.find(argument.text) == workspace.programmableWave.end()) {
                    diagnostics.push_back({argument.range, 1, "unknown-programmable-wave-symbol", "Unknown programmable wave symbol '" + argument.text + "'."});
                }
                break;
            case ArgumentKind::KeysplitSymbol:
                if (!workspace.keysplits.empty() && workspace.keysplits.find(argument.text) == workspace.keysplits.end()) {
                    diagnostics.push_back({argument.range, 1, "unknown-keysplit-symbol", "Unknown keysplit table '" + argument.text + "'."});
                }
                break;
            case ArgumentKind::VoicegroupSymbol: {
                std::string normalized = startsWith(argument.text, "voicegroup_") ? argument.text.substr(std::string("voicegroup_").size()) : argument.text;
                if (!workspace.voicegroups.empty() && workspace.voicegroups.find(argument.text) == workspace.voicegroups.end() && workspace.voicegroups.find(normalized) == workspace.voicegroups.end()) {
                    diagnostics.push_back({argument.range, 1, "unknown-voicegroup-symbol", "Unknown voicegroup '" + argument.text + "'."});
                }
                break;
            }
            }
        }
    }

    return diagnostics;
}

std::optional<std::string> readTextFile(const std::filesystem::path &path)
{
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::unordered_map<std::string, IndexedSymbol> parseIncbinSymbols(const std::string &text)
{
    std::unordered_map<std::string, IndexedSymbol> result;
    std::optional<IndexedSymbol> pending;
    auto lines = splitLines(text);
    for (size_t lineNumber = 0; lineNumber < lines.size(); ++lineNumber) {
        const auto &line = lines[lineNumber];
        std::string trimmed = trim(line);
        size_t labelEnd = trimmed.find("::");
        if (labelEnd != std::string::npos && labelEnd > 0) {
            size_t column = line.find_first_not_of(" \t");
            if (column == std::string::npos) {
                column = 0;
            }
            std::string name = trimmed.substr(0, labelEnd);
            pending = IndexedSymbol{name, rangeFor(static_cast<int>(lineNumber), static_cast<int>(column), static_cast<int>(column + name.size()))};
            continue;
        }
        if (pending && trimmed.find(".incbin") != std::string::npos) {
            result.emplace(pending->name, *pending);
            pending.reset();
        }
    }
    return result;
}

std::unordered_map<std::string, IndexedSymbol> parseKeysplits(const std::string &text)
{
    std::unordered_map<std::string, IndexedSymbol> result;
    auto lines = splitLines(text);
    for (size_t lineNumber = 0; lineNumber < lines.size(); ++lineNumber) {
        const auto &line = lines[lineNumber];
        std::string trimmed = trim(line);
        if (startsWith(trimmed, "keysplit ")) {
            std::string rest = trim(trimmed.substr(std::string("keysplit ").size()));
            std::string name = "keysplit_" + trim(rest.substr(0, rest.find(',')));
            result.emplace(name, IndexedSymbol{name, rangeFor(static_cast<int>(lineNumber), 0, static_cast<int>(name.size()))});
        } else if (startsWith(trimmed, ".set ")) {
            std::string rest = trim(trimmed.substr(std::string(".set ").size()));
            std::string name = trim(rest.substr(0, rest.find(',')));
            result.emplace(name, IndexedSymbol{name, rangeFor(static_cast<int>(lineNumber), 0, static_cast<int>(name.size()))});
        }
    }
    return result;
}

std::string percentDecode(const std::string &text)
{
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            std::string hex = text.substr(i + 1, 2);
            char *end = nullptr;
            long value = std::strtol(hex.c_str(), &end, 16);
            if (end != hex.c_str() && *end == '\0') {
                result.push_back(static_cast<char>(value));
                i += 2;
                continue;
            }
        }
        result.push_back(text[i]);
    }
    return result;
}

std::filesystem::path fileUriToPath(const std::string &uri)
{
    const std::string prefix = "file://";
    if (!startsWith(uri, prefix)) {
        return uri;
    }
    return percentDecode(uri.substr(prefix.size()));
}

WorkspaceIndex loadWorkspace(const std::filesystem::path &root)
{
    WorkspaceIndex workspace;
    if (auto text = readTextFile(root / "sound" / "direct_sound_data.inc")) {
        workspace.directSound = parseIncbinSymbols(*text);
    }
    if (auto text = readTextFile(root / "sound" / "programmable_wave_data.inc")) {
        workspace.programmableWave = parseIncbinSymbols(*text);
    }
    if (auto text = readTextFile(root / "sound" / "keysplit_tables.inc")) {
        workspace.keysplits = parseKeysplits(*text);
    }

    std::filesystem::path voicegroupRoot = root / "sound" / "voicegroups";
    std::error_code error;
    if (!std::filesystem::exists(voicegroupRoot, error)) {
        return workspace;
    }
    for (std::filesystem::recursive_directory_iterator it(voicegroupRoot, error), end; !error && it != end; it.increment(error)) {
        if (!it->is_regular_file(error)) {
            continue;
        }
        auto extension = it->path().extension().string();
        if (extension != ".inc" && extension != ".s") {
            continue;
        }
        auto text = readTextFile(it->path());
        if (!text) {
            continue;
        }
        auto document = parseVoicegroupDocument(*text);
        for (const auto &declaration : document.declarations) {
            workspace.voicegroups.emplace(declaration.name, IndexedSymbol{declaration.name, declaration.range});
        }
    }
    return workspace;
}

Json diagnosticToJson(const Diagnostic &diagnostic)
{
    return Json::Object{
        {"range", Json::Object{
                      {"start", Json::Object{{"line", diagnostic.range.start.line}, {"character", diagnostic.range.start.character}}},
                      {"end", Json::Object{{"line", diagnostic.range.end.line}, {"character", diagnostic.range.end.character}}},
                  }},
        {"severity", diagnostic.severity},
        {"code", diagnostic.code},
        {"message", diagnostic.message},
    };
}

std::optional<Json> readMessage()
{
    std::string line;
    size_t contentLength = 0;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            break;
        }
        const std::string header = "Content-Length:";
        if (startsWith(line, header)) {
            contentLength = static_cast<size_t>(std::strtoul(trim(line.substr(header.size())).c_str(), nullptr, 10));
        }
    }
    if (contentLength == 0 || !std::cin) {
        return std::nullopt;
    }
    std::string body(contentLength, '\0');
    std::cin.read(body.data(), static_cast<std::streamsize>(contentLength));
    if (std::cin.gcount() != static_cast<std::streamsize>(contentLength)) {
        return std::nullopt;
    }
    return JsonParser(body).parse();
}

void writeMessage(const Json &message)
{
    std::string body = jsonStringify(message);
    std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    std::cout.flush();
}

class LanguageServer {
public:
    void run()
    {
        while (auto message = readMessage()) {
            handle(*message);
        }
    }

private:
    WorkspaceIndex workspace_;
    std::unordered_map<std::string, std::string> documents_;

    void handle(const Json &message)
    {
        const auto *method = message.get("method");
        const auto *methodName = method ? method->string() : nullptr;
        if (!methodName) {
            return;
        }
        const Json *id = message.get("id");

        if (*methodName == "initialize") {
            if (const auto *params = message.get("params")) {
                if (const auto *rootUri = params->get("rootUri")) {
                    if (const auto *rootUriString = rootUri->string()) {
                        workspace_ = loadWorkspace(fileUriToPath(*rootUriString));
                    }
                }
            }
            respond(id, Json::Object{
                            {"capabilities", Json::Object{
                                                 {"textDocumentSync", 1},
                                             }},
                        });
        } else if (*methodName == "textDocument/didOpen" || *methodName == "textDocument/didChange") {
            updateDocument(message);
        } else if (*methodName == "shutdown") {
            respond(id, nullptr);
        } else if (*methodName == "exit") {
            std::exit(0);
        } else if (id) {
            respond(id, nullptr);
        }
    }

    void updateDocument(const Json &message)
    {
        const auto *params = message.get("params");
        if (!params) {
            return;
        }
        const auto *textDocument = params->get("textDocument");
        const auto *uriValue = textDocument ? textDocument->get("uri") : nullptr;
        const auto *uri = uriValue ? uriValue->string() : nullptr;
        if (!uri) {
            return;
        }

        const std::string *text = nullptr;
        if (const auto *documentText = textDocument->get("text")) {
            text = documentText->string();
        } else if (const auto *changesValue = params->get("contentChanges")) {
            if (const auto *changes = changesValue->array(); changes && !changes->empty()) {
                if (const auto *changeText = changes->back().get("text")) {
                    text = changeText->string();
                }
            }
        }
        if (!text) {
            return;
        }

        documents_[*uri] = *text;
        publishDiagnostics(*uri, *text);
    }

    void publishDiagnostics(const std::string &uri, const std::string &text)
    {
        Json::Array diagnostics;
        for (const auto &diagnostic : analyzeDocument(parseVoicegroupDocument(text), workspace_)) {
            diagnostics.push_back(diagnosticToJson(diagnostic));
        }
        writeMessage(Json::Object{
            {"jsonrpc", "2.0"},
            {"method", "textDocument/publishDiagnostics"},
            {"params", Json::Object{{"uri", uri}, {"diagnostics", diagnostics}}},
        });
    }

    void respond(const Json *id, Json result)
    {
        if (!id) {
            return;
        }
        writeMessage(Json::Object{
            {"jsonrpc", "2.0"},
            {"id", *id},
            {"result", std::move(result)},
        });
    }
};

int main()
{
    LanguageServer().run();
    return 0;
}

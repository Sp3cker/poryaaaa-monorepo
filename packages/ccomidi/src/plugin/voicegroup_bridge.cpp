#include "plugin/voicegroup_bridge.h"
#include "projects_json_path.h"

#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace ccomidi
{

namespace
{

std::FILE* open_file_read_binary(const std::string& path)
{
#if defined(_WIN32)
    std::FILE* file = nullptr;
    if (fopen_s(&file, path.c_str(), "rb") != 0)
        return nullptr;
    return file;
#else
    return std::fopen(path.c_str(), "rb");
#endif
}

// Fixed per-user location, independent of whether poryaaaa/ccomidi were
// installed as CLAP, VST3, etc. Both plugins must agree on this path.
std::string state_path()
{
    char path[700];
    return poryaaaa_projects_json_default_path(path, sizeof(path)) ? std::string(path) : std::string{};
}

long long mtime_ns(const std::string& path)
{
    struct stat st;
    if (path.empty() || stat(path.c_str(), &st) != 0)
        return 0;
#if defined(__APPLE__)
    return static_cast<long long>(st.st_mtimespec.tv_sec) * 1000000000LL + st.st_mtimespec.tv_nsec;
#elif defined(_WIN32)
    return static_cast<long long>(st.st_mtime) * 1000000000LL;
#else
    return static_cast<long long>(st.st_mtim.tv_sec) * 1000000000LL + st.st_mtim.tv_nsec;
#endif
}

// Decode a JSON string body into dst (without surrounding quotes). Expects src
// to point at the opening '"'; advances src past the closing '"'. Returns true
// on success.
bool parse_json_string(const char*& src, std::string& dst)
{
    if (*src != '"')
        return false;
    ++src;
    dst.clear();
    while (*src && *src != '"')
    {
        if (*src == '\\' && src[1])
        {
            dst.push_back(src[1]);
            src += 2;
        }
        else
        {
            dst.push_back(*src++);
        }
    }
    if (*src != '"')
        return false;
    ++src;
    return true;
}

// Finds `"<key>"` in src (starting at or after src), advances src past the
// matching colon, and returns true. Skips over any nested strings so the
// search doesn't match keys buried inside string values.
bool find_key(const char*& src, const char* key)
{
    const std::string needle = std::string("\"") + key + "\"";
    const char* p = std::strstr(src, needle.c_str());
    if (!p)
        return false;
    p += needle.size();
    while (*p && *p != ':')
        ++p;
    if (*p != ':')
        return false;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        ++p;
    src = p;
    return true;
}

void skip_ws(const char*& src)
{
    while (*src == ' ' || *src == '\t' || *src == '\n' || *src == '\r')
        ++src;
}

bool parse_json_int(const char*& src, int& out)
{
    skip_ws(src);
    if (*src != '-' && (*src < '0' || *src > '9'))
        return false;
    char* end = nullptr;
    const long value = std::strtol(src, &end, 10);
    if (end == src)
        return false;
    out = static_cast<int>(value);
    src = end;
    return true;
}

bool read_next_object(const char*& cursor, std::string& out)
{
    if (*cursor != '{')
        return false;

    const char* objStart = cursor;
    int depth = 0;
    const char* objEnd = cursor;
    while (*objEnd)
    {
        if (*objEnd == '{')
            ++depth;
        else if (*objEnd == '}')
        {
            --depth;
            if (depth == 0)
            {
                ++objEnd;
                out.assign(objStart, static_cast<size_t>(objEnd - objStart));
                cursor = objEnd;
                return true;
            }
        }
        else if (*objEnd == '"')
        {
            ++objEnd;
            while (*objEnd && *objEnd != '"')
            {
                if (*objEnd == '\\' && objEnd[1])
                    objEnd += 2;
                else
                    ++objEnd;
            }
            if (*objEnd == '"')
                ++objEnd;
            continue;
        }
        ++objEnd;
    }

    return false;
}

bool is_filtered_sample_name(const std::string& name)
{
    return name == "Square 1";
}

void parse_drumset(const std::string& obj, std::vector<DrumPad>& out)
{
    const char* cursor = obj.c_str();
    if (!find_key(cursor, "drumset"))
        return;
    while (*cursor && *cursor != '[')
        ++cursor;
    if (*cursor != '[')
        return;
    ++cursor;

    while (*cursor)
    {
        skip_ws(cursor);
        if (*cursor == ',')
        {
            ++cursor;
            continue;
        }
        if (*cursor == ']' || *cursor == '\0')
            break;
        if (*cursor != '{')
            break;

        std::string padObj;
        if (!read_next_object(cursor, padObj))
            break;

        int note = -1;
        std::string name;
        const char* pc = padObj.c_str();
        const bool hasNote = find_key(pc, "note") && parse_json_int(pc, note);
        pc = padObj.c_str();
        const bool hasName = find_key(pc, "name") && parse_json_string(pc, name);
        if (hasNote && hasName && note >= 0 && note < 128 && !name.empty() && !is_filtered_sample_name(name))
            out.push_back(DrumPad{note, name});
    }
}

bool read_file_to_string(const std::string& path, std::string& out)
{
    std::FILE* f = open_file_read_binary(path);
    if (!f)
        return false;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 0 || size > 4 * 1024 * 1024)
    { // cap at 4MB
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(size));
    size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return got == out.size();
}

} // namespace

long long voicegroup_bridge_state_mtime()
{
    return mtime_ns(state_path());
}

VoiceSlotLoad voicegroup_bridge_parse_state_body(const std::string& body)
{
    VoiceSlotLoad result;

    const char* cursor = body.c_str();
    std::string voicegroupName;
    std::string ignoredRoot;
    if (find_key(cursor, "root"))
        parse_json_string(cursor, ignoredRoot);
    if (find_key(cursor, "bank"))
        parse_json_string(cursor, voicegroupName);

    if (!find_key(cursor, "slots"))
    {
        result.error = "projects.json missing 'slots' array.";
        return result;
    }
    while (*cursor && *cursor != '[')
        ++cursor;
    if (*cursor != '[')
    {
        result.error = "projects.json 'slots' is not an array.";
        return result;
    }
    ++cursor;

    while (*cursor)
    {
        skip_ws(cursor);
        if (*cursor == ',')
        {
            ++cursor;
            continue;
        }
        if (*cursor == ']' || *cursor == '\0')
            break;
        if (*cursor != '{')
            break;

        std::string obj;
        if (!read_next_object(cursor, obj))
            break;

        const char* oc = obj.c_str();
        int program = -1;
        int typeCode = 0;
        std::string name;
        if (find_key(oc, "program"))
            parse_json_int(oc, program);
        oc = obj.c_str();
        if (find_key(oc, "name"))
            parse_json_string(oc, name);
        oc = obj.c_str();
        if (find_key(oc, "typeCode"))
            parse_json_int(oc, typeCode);

        if (program >= 0 && program < 128 && !name.empty() && !is_filtered_sample_name(name))
        {
            VoiceSlot slot;
            slot.program = program;
            slot.typeCode = typeCode;
            slot.name = name;
            parse_drumset(obj, slot.drumset);
            result.slots.push_back(std::move(slot));
        }
    }

    if (result.slots.empty())
        result.error = voicegroupName.empty() ? std::string("projects.json has no slots.")
                                              : "Voicegroup '" + voicegroupName + "' has no sample-bearing slots.";

    return result;
}

VoiceSlotLoad voicegroup_bridge_load_state()
{
    VoiceSlotLoad result;

    result.statePath = state_path();
    if (result.statePath.empty())
    {
        result.error = "Can't resolve state path — $HOME is not set.";
        return result;
    }

    result.mtimeNs = mtime_ns(result.statePath);
    if (result.mtimeNs == 0)
    {
        result.error = "poryaaaa hasn't written its state yet — load poryaaaa in the DAW.";
        return result;
    }

    std::string body;
    if (!read_file_to_string(result.statePath, body))
    {
        result.error = "Could not read projects.json.";
        return result;
    }

    VoiceSlotLoad parsed = voicegroup_bridge_parse_state_body(body);
    parsed.statePath = result.statePath;
    parsed.mtimeNs = result.mtimeNs;
    return parsed;
}

} // namespace ccomidi

#include "voicegroup/voicegroup_project_state.h"
#include "voicegroup/vg_paths.h"

#include <node_api.h>
#include <string.h>

static bool get_string(napi_env env, napi_value value, char* out, size_t outSize)
{
    napi_valuetype type;
    if (napi_typeof(env, value, &type) != napi_ok || type != napi_string)
        return false;
    size_t len = 0;
    if (napi_get_value_string_utf8(env, value, out, outSize, &len) != napi_ok)
        return false;
    return len < outSize;
}

static napi_value make_bool(napi_env env, bool value)
{
    napi_value out;
    napi_get_boolean(env, value, &out);
    return out;
}

static napi_value make_string(napi_env env, const char* text)
{
    napi_value out;
    napi_create_string_utf8(env, text, NAPI_AUTO_LENGTH, &out);
    return out;
}

static void set_named(napi_env env, napi_value obj, const char* name, napi_value value)
{
    napi_set_named_property(env, obj, name, value);
}

static napi_value make_diagnostics(napi_env env, const char* text)
{
    napi_value array;
    napi_create_array_with_length(env, 1, &array);
    napi_set_element(env, array, 0, make_string(env, text));
    return array;
}

static napi_value make_slots(napi_env env, const VoicegroupProjectState* state)
{
    napi_value array;
    napi_create_array_with_length(env, VOICEGROUP_SIZE, &array);
    napi_value nullValue;
    napi_get_null(env, &nullValue);
    for (int i = 0; i < VOICEGROUP_SIZE; i++)
    {
        const VoicegroupProjectStateSlot* slot = &state->slots[i];
        if (!slot->defined)
        {
            napi_set_element(env, array, (uint32_t)i, nullValue);
            continue;
        }
        napi_value obj;
        napi_create_object(env, &obj);
        set_named(env, obj, "name", make_string(env, slot->name));
        napi_value typeCode;
        napi_create_uint32(env, slot->typeCode, &typeCode);
        set_named(env, obj, "typeCode", typeCode);
        napi_set_element(env, array, (uint32_t)i, obj);
    }
    return array;
}

static napi_value parse_voicegroup(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[2];
    if (napi_get_cb_info(env, info, &argc, argv, NULL, NULL) != napi_ok || argc < 2)
    {
        napi_throw_type_error(env, NULL, "parseVoicegroup(root, bank) expects two string arguments");
        return NULL;
    }

    char root[VG_MAX_PATH_LEN];
    char bank[VG_MAX_SYMBOL_LEN];
    if (!get_string(env, argv[0], root, sizeof(root)) || !get_string(env, argv[1], bank, sizeof(bank)))
    {
        napi_throw_type_error(env, NULL, "parseVoicegroup(root, bank) expects two string arguments");
        return NULL;
    }

    VoicegroupProjectState state;
    memset(&state, 0, sizeof(state));
    LoadedVoiceGroup* vg = voicegroup_load(root, bank, NULL);
    bool parsed = vg && voicegroup_project_state_collect(root, bank, NULL, &state);
    voicegroup_free(vg);

    napi_value result;
    napi_create_object(env, &result);
    set_named(env, result, "ok", make_bool(env, parsed));
    if (parsed)
        set_named(env, result, "slots", make_slots(env, &state));
    else
        set_named(env, result, "diagnostics", make_diagnostics(env, "failed to parse voicegroup"));

    voicegroup_project_state_free(&state);
    return result;
}

static napi_value init(napi_env env, napi_value exports)
{
    napi_value fn;
    napi_create_function(env, "parseVoicegroup", NAPI_AUTO_LENGTH, parse_voicegroup, NULL, &fn);
    set_named(env, exports, "parseVoicegroup", fn);
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)

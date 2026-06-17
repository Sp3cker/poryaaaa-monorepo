#ifndef VOICEGROUP_PROJECT_STATE_H
#define VOICEGROUP_PROJECT_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "voicegroup_loader.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        uint8_t note;
        char name[VG_MAX_VOICE_SAMPLE_NAME];
    } VoicegroupProjectStateDrumPad;

    typedef struct
    {
        bool used;
        uint8_t typeCode;
        char name[VG_MAX_VOICE_SAMPLE_NAME];
        VoicegroupProjectStateDrumPad* drumset;
        int drumsetCount;
    } VoicegroupProjectStateSlot;

    typedef struct
    {
        VoicegroupProjectStateSlot slots[VOICEGROUP_SIZE];
    } VoicegroupProjectState;

    bool voicegroup_project_state_collect(const char* projectRoot,
                                          const char* voicegroupName,
                                          const VoicegroupLoaderConfig* config,
                                          VoicegroupProjectState* out);

    bool voicegroup_project_state_write_default(const char* projectRoot,
                                                const char* voicegroupName,
                                                const VoicegroupProjectState* state);

    bool voicegroup_project_state_default_path(char* out, size_t outSize);

    void voicegroup_project_state_free(VoicegroupProjectState* state);

#ifdef __cplusplus
}
#endif

#endif /* VOICEGROUP_PROJECT_STATE_H */

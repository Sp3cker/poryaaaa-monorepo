#include <stddef.h>

void* textedit_voicegroup_service_create(const char* projectRoot)
{
    (void)projectRoot;
    return (void*)1;
}

void textedit_voicegroup_service_destroy(void* service)
{
    (void)service;
}

int textedit_voicegroup_service_set_project_root(void* service, const char* projectRoot)
{
    (void)service;
    (void)projectRoot;
    return 1;
}

int textedit_voicegroup_service_sync_document(void* service, const char* uri, const char* text)
{
    (void)service;
    (void)uri;
    (void)text;
    return 1;
}

int textedit_voicegroup_service_complete(
    void* service,
    int line,
    int character,
    void (*callback)(const char*, const char*, const char*, int, int, int, int, void*),
    void* userData)
{
    (void)service;
    (void)line;
    (void)character;
    (void)callback;
    (void)userData;
    return 1;
}

int textedit_voicegroup_service_hover(
    void* service, int line, int character, void (*callback)(const char*, void*), void* userData)
{
    (void)service;
    (void)line;
    (void)character;
    (void)callback;
    (void)userData;
    return 1;
}

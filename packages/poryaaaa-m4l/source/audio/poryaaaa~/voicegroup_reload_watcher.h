#pragma once

using VoicegroupReloadCallback = void (*)(void*);

class VoicegroupReloadWatcher
{
public:
    VoicegroupReloadWatcher();
    ~VoicegroupReloadWatcher();

    VoicegroupReloadWatcher(const VoicegroupReloadWatcher&) = delete;
    VoicegroupReloadWatcher& operator=(const VoicegroupReloadWatcher&) = delete;

    bool start(const char* root, VoicegroupReloadCallback callback, void* userData);
    void stop();

    const char* last_error() const;

private:
    struct Impl;

    Impl* impl_;
};

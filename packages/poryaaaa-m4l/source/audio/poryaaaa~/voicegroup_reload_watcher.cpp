#include "voicegroup_reload_watcher.h"

#if defined(__APPLE__) || defined(_WIN32) || defined(_WIN64)
#    include "third_party/SaneCppLibraries/Libraries/FileSystemWatcher/FileSystemWatcher.h"

#    include <cstdio>
#    include <cstring>

struct VoicegroupReloadWatcher::Impl
{
    SC::FileSystemWatcher watcher;
    SC::FileSystemWatcher::ThreadRunner runner;
    SC::FileSystemWatcher::FolderWatcher folderWatcher;
    VoicegroupReloadCallback callback = nullptr;
    void* userData = nullptr;
    bool initialized = false;
    bool watching = false;
    char lastError[256] = {};

    void set_error(const char* message)
    {
        snprintf(lastError, sizeof(lastError), "%s", message ? message : "unknown watcher error");
    }
};
#else
#    include <cstdio>

struct VoicegroupReloadWatcher::Impl
{
    char lastError[256] = {};

    void set_error(const char* message)
    {
        snprintf(lastError, sizeof(lastError), "%s", message ? message : "unsupported platform");
    }
};
#endif

VoicegroupReloadWatcher::VoicegroupReloadWatcher() : impl_(new Impl()) {}

VoicegroupReloadWatcher::~VoicegroupReloadWatcher()
{
    stop();
    delete impl_;
}

bool VoicegroupReloadWatcher::start(const char* root, VoicegroupReloadCallback callback, void* userData)
{
    stop();
    if (!root || !root[0])
    {
        impl_->set_error("empty root path");
        return false;
    }
    if (!callback)
    {
        impl_->set_error("missing reload callback");
        return false;
    }
#if defined(__APPLE__) || defined(_WIN32) || defined(_WIN64)
    auto initResult = impl_->watcher.init(impl_->runner);
    if (!initResult)
    {
        impl_->set_error(initResult.message);
        return false;
    }
    impl_->initialized = true;
    impl_->callback = callback;
    impl_->userData = userData;
    impl_->folderWatcher.notifyCallback = [this](const SC::FileSystemWatcher::Notification&)
    {
        if (impl_->callback)
            impl_->callback(impl_->userData);
    };

    auto watchResult =
        impl_->watcher.watch(impl_->folderWatcher, SC::StringSpan::fromNullTerminated(root, SC::StringEncoding::Utf8));
    if (!watchResult)
    {
        impl_->set_error(watchResult.message);
        stop();
        return false;
    }
    impl_->watching = true;
    impl_->lastError[0] = '\0';
    return true;
#else
    (void)callback;
    (void)userData;
    impl_->set_error("voicegroup reload watcher is only supported on macOS and Windows");
    return false;
#endif
}

void VoicegroupReloadWatcher::stop()
{
#if defined(__APPLE__) || defined(_WIN32) || defined(_WIN64)
    if (impl_->initialized)
    {
        if (impl_->watching)
        {
            (void)impl_->folderWatcher.stopWatching();
            impl_->watching = false;
        }
        (void)impl_->watcher.close();
        impl_->initialized = false;
    }
    impl_->callback = nullptr;
    impl_->userData = nullptr;
#endif
}

const char* VoicegroupReloadWatcher::last_error() const
{
    return impl_->lastError;
}

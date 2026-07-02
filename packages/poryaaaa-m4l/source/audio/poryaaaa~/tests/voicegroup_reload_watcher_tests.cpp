#include "voicegroup_reload_watcher.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace
{
std::atomic<int> callbackCount = 0;

void reload_requested(void*)
{
    ++callbackCount;
}

bool spin_until_callback_count(int expected, std::chrono::milliseconds timeout)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (callbackCount >= expected)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return callbackCount >= expected;
}

std::filesystem::path test_root()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::current_path() / ("porya-watcher-test-" + std::to_string(now));
}
} // namespace

int main()
{
    auto root = test_root();
    auto voicegroupDir = root / "sound" / "voicegroups";
    std::filesystem::create_directories(voicegroupDir);

    VoicegroupReloadWatcher watcher;
    if (!watcher.start(root.string().c_str(), reload_requested, nullptr))
    {
        std::cerr << "watcher failed to start: " << watcher.last_error() << "\n";
        return 1;
    }

    {
        std::ofstream file(voicegroupDir / "voicegroup001.inc");
        file << "voicegroup001::\n";
    }

    if (!spin_until_callback_count(1, std::chrono::seconds(5)))
    {
        std::cerr << "watcher did not report a file creation\n";
        return 1;
    }

    watcher.stop();
    callbackCount = 0;
    {
        std::ofstream file(voicegroupDir / "voicegroup002.inc");
        file << "voicegroup002::\n";
    }
    if (spin_until_callback_count(1, std::chrono::seconds(1)))
    {
        std::cerr << "stopped watcher still reported file changes\n";
        return 1;
    }

    std::filesystem::remove_all(root);
    return 0;
}

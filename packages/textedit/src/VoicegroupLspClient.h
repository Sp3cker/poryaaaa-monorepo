#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

class VoicegroupLspClient
{
public:
    VoicegroupLspClient();
    ~VoicegroupLspClient();

    bool start();
    void stop();

    bool isRunning() const;
    void openDocument(const juce::String& text);
    void changeDocument(const juce::String& text);
    void requestCompletion(int line, int character);
    void requestHover(int line, int character);
    void requestSignatureHelp(int line, int character);

    juce::String getStatusText() const;

private:
    enum class RequestKind
    {
        completion,
        hover,
        signatureHelp
    };

    int nextRequestId();
    void sendNotification(const juce::String& method, juce::DynamicObject::Ptr params);
    void sendRequest(const juce::String& method, juce::DynamicObject::Ptr params, RequestKind kind);
    void sendMessage(const juce::var& message);
    juce::DynamicObject::Ptr textDocumentIdentifier() const;
    juce::DynamicObject::Ptr positionParams(int line, int character) const;
    void setStatus(juce::String newStatus);

#if JUCE_MAC || JUCE_LINUX
    bool startProcess();
    void readerLoop();
    void parseIncoming();
    void handleMessage(const juce::var& message);
    void handleResponse(const juce::var& message);
    void handleNotification(const juce::var& message);
    void writeBytes(const juce::String& text);

    int childPid = -1;
    int childStdin = -1;
    int childStdout = -1;
    std::thread readerThread;
    std::string inputBuffer;
#endif

    mutable juce::CriticalSection lock;
    juce::HashMap<int, RequestKind> pendingRequests;
    juce::String statusText = "LSP: not started";
    int requestCounter = 0;
    bool running = false;
    bool documentOpen = false;

    const juce::String serverPath = TEXTEDIT_VOICEGROUP_LSP_PATH;
    const juce::String documentUri = "file:///textedit/voicegroup.inc";
};

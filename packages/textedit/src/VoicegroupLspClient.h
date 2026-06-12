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

    /*
     * Mirror the editor's full document text into the LSP session. The client
     * owns all open/change sequencing: text arriving before the initialize
     * response is stashed and flushed afterwards, the first sync sends
     * didOpen, and later syncs send versioned didChange notifications.
     */
    void syncDocument(const juce::String& text);
    void requestCompletion(int line, int character);
    void requestHover(int line, int character);
    void requestSignatureHelp(int line, int character);

    juce::String getStatusText() const;

private:
    enum class RequestKind
    {
        completion,
        hover,
        initialize,
        shutdown,
        signatureHelp
    };

    bool isDocumentSynced() const;
    int nextRequestId();
    void sendNotification(const juce::String& method, juce::DynamicObject::Ptr params);
    void sendRequest(const juce::String& method, juce::DynamicObject::Ptr params, RequestKind kind);
    void sendMessage(const juce::var& message);
    void sendDidOpen(const juce::String& text, int version);
    void sendDidChange(const juce::String& text, int version);
    void handleInitializeResponse();
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
    void closePipes();
    void markDisconnected(juce::String status);
    void waitForChildProcess();
    void writeBytes(const juce::String& text);

    int childPid = -1;
    int childStdin = -1;
    int childStdout = -1;
    std::thread readerThread;
    std::string inputBuffer;
#endif

    mutable juce::CriticalSection lock;
    /* Serialises whole frames onto the child's stdin. Separate from `lock`
     * (state) so a blocked pipe write can never stall the reader thread's
     * state access. Both the message thread and the reader thread (pending
     * flush after initialize) write frames. */
    juce::CriticalSection writeLock;
    juce::HashMap<int, RequestKind> pendingRequests;
    juce::String statusText = "LSP: not started";
    int requestCounter = 0;
    bool running = false;

    /* LSP session state, all guarded by `lock`. */
    bool initialized = false;
    bool documentOpen = false;
    int documentVersion = 0;
    juce::String pendingDocumentText;
    bool hasPendingDocumentText = false;

    const juce::String serverPath;
    const juce::String documentUri = "file:///textedit/voicegroup.inc";
};

#include "VoicegroupLspClient.h"

#include "VoicegroupLspConfig.h"

#if JUCE_MAC || JUCE_LINUX
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

juce::DynamicObject::Ptr object()
{
    return new juce::DynamicObject();
}

juce::var makeMessage(const juce::String& method, juce::DynamicObject::Ptr params)
{
    auto message = object();
    message->setProperty("jsonrpc", "2.0");
    message->setProperty("method", method);
    message->setProperty("params", juce::var(params.get()));
    return juce::var(message.get());
}

juce::String valueFromMarkupContent(const juce::var& contents)
{
    if (auto* objectValue = contents.getDynamicObject())
        return objectValue->getProperty("value").toString();

    return contents.toString();
}

} // namespace

VoicegroupLspClient::VoicegroupLspClient()
    : serverPath(resolveVoicegroupLspServerPath(TEXTEDIT_VOICEGROUP_LSP_PATH))
{
}

VoicegroupLspClient::~VoicegroupLspClient()
{
    stop();
}

bool VoicegroupLspClient::start()
{
    {
        const juce::ScopedLock scopedLock(lock);
        if (running)
            return true;
    }

#if JUCE_MAC || JUCE_LINUX
    if (!startProcess())
        return false;

    setStatus("LSP: starting voicegroup-lsp");

    auto params = object();
    params->setProperty("processId", juce::var());
    params->setProperty("rootUri", juce::var());
    sendRequest("initialize", params, RequestKind::initialize);
    return true;
#else
    setStatus("LSP: stdio client is not implemented on this platform yet");
    return false;
#endif
}

void VoicegroupLspClient::stop()
{
#if JUCE_MAC || JUCE_LINUX
    auto shouldSendShutdown = false;

    {
        const juce::ScopedLock scopedLock(lock);
        if (!running && childPid <= 0)
            return;

        shouldSendShutdown = running;
    }

    if (shouldSendShutdown)
    {
        auto shutdownParams = object();
        sendRequest("shutdown", shutdownParams, RequestKind::shutdown);
        sendNotification("exit", object());
    }

    {
        const juce::ScopedLock scopedLock(lock);
        running = false;
        initialized = false;
        documentOpen = false;
        documentVersion = 0;
        pendingDocumentText.clear();
        hasPendingDocumentText = false;
    }

    {
        const juce::ScopedLock scopedLock(lock);
        if (childPid > 0)
            ::kill(childPid, SIGTERM);
    }

    closePipes();

    if (readerThread.joinable())
        readerThread.join();

    waitForChildProcess();
#else
    const juce::ScopedLock scopedLock(lock);
    running = false;
#endif
}

bool VoicegroupLspClient::isRunning() const
{
    const juce::ScopedLock scopedLock(lock);
    return running;
}

void VoicegroupLspClient::syncDocument(const juce::String& text)
{
    enum class Action { none, open, change };
    auto action = Action::none;
    int version = 0;

    {
        const juce::ScopedLock scopedLock(lock);
        if (!running)
            return;

        if (!initialized)
        {
            pendingDocumentText = text;
            hasPendingDocumentText = true;
            return;
        }

        if (!documentOpen)
        {
            documentOpen = true;
            documentVersion = 1;
            version = documentVersion;
            action = Action::open;
        }
        else
        {
            version = ++documentVersion;
            action = Action::change;
        }
    }

    if (action == Action::open)
        sendDidOpen(text, version);
    else if (action == Action::change)
        sendDidChange(text, version);
}

void VoicegroupLspClient::sendDidOpen(const juce::String& text, int version)
{
    auto textDocument = object();
    textDocument->setProperty("uri", documentUri);
    textDocument->setProperty("languageId", "voicegroup-inc");
    textDocument->setProperty("version", version);
    textDocument->setProperty("text", text);

    auto params = object();
    params->setProperty("textDocument", juce::var(textDocument.get()));
    sendNotification("textDocument/didOpen", params);
}

void VoicegroupLspClient::sendDidChange(const juce::String& text, int version)
{
    auto textDocument = object();
    textDocument->setProperty("uri", documentUri);
    textDocument->setProperty("version", version);

    auto change = object();
    change->setProperty("text", text);

    juce::Array<juce::var> changes;
    changes.add(juce::var(change.get()));

    auto params = object();
    params->setProperty("textDocument", juce::var(textDocument.get()));
    params->setProperty("contentChanges", changes);
    sendNotification("textDocument/didChange", params);
}

void VoicegroupLspClient::requestCompletion(int line, int character)
{
    if (isDocumentSynced())
        sendRequest("textDocument/completion", positionParams(line, character), RequestKind::completion);
}

void VoicegroupLspClient::requestHover(int line, int character)
{
    if (isDocumentSynced())
        sendRequest("textDocument/hover", positionParams(line, character), RequestKind::hover);
}

void VoicegroupLspClient::requestSignatureHelp(int line, int character)
{
    if (isDocumentSynced())
        sendRequest("textDocument/signatureHelp", positionParams(line, character), RequestKind::signatureHelp);
}

bool VoicegroupLspClient::isDocumentSynced() const
{
    const juce::ScopedLock scopedLock(lock);
    return running && documentOpen;
}

juce::String VoicegroupLspClient::getStatusText() const
{
    const juce::ScopedLock scopedLock(lock);
    return statusText;
}

int VoicegroupLspClient::nextRequestId()
{
    const juce::ScopedLock scopedLock(lock);
    return ++requestCounter;
}

void VoicegroupLspClient::sendNotification(const juce::String& method, juce::DynamicObject::Ptr params)
{
    sendMessage(makeMessage(method, params));
}

void VoicegroupLspClient::sendRequest(const juce::String& method,
                                      juce::DynamicObject::Ptr params,
                                      RequestKind kind)
{
    const auto id = nextRequestId();

    auto message = object();
    message->setProperty("jsonrpc", "2.0");
    message->setProperty("id", id);
    message->setProperty("method", method);
    message->setProperty("params", juce::var(params.get()));

    {
        const juce::ScopedLock scopedLock(lock);
        pendingRequests.set(id, kind);
    }

    sendMessage(juce::var(message.get()));
}

void VoicegroupLspClient::sendMessage(const juce::var& message)
{
    const auto body = juce::JSON::toString(message, true, false);
    const auto bytes = body.toRawUTF8();
    const auto header = "Content-Length: " + juce::String(std::strlen(bytes)) + "\r\n\r\n";
    writeBytes(header + body);
}

juce::DynamicObject::Ptr VoicegroupLspClient::textDocumentIdentifier() const
{
    auto textDocument = object();
    textDocument->setProperty("uri", documentUri);
    return textDocument;
}

juce::DynamicObject::Ptr VoicegroupLspClient::positionParams(int line, int character) const
{
    auto position = object();
    position->setProperty("line", line);
    position->setProperty("character", character);

    auto params = object();
    params->setProperty("textDocument", juce::var(textDocumentIdentifier().get()));
    params->setProperty("position", juce::var(position.get()));
    return params;
}

void VoicegroupLspClient::setStatus(juce::String newStatus)
{
    const juce::ScopedLock scopedLock(lock);
    statusText = std::move(newStatus);
}

#if JUCE_MAC || JUCE_LINUX

bool VoicegroupLspClient::startProcess()
{
    if (!juce::File(serverPath).existsAsFile())
    {
        setStatus("LSP: missing " + serverPath);
        return false;
    }

    int stdinPipe[2] = { -1, -1 };
    int stdoutPipe[2] = { -1, -1 };

    if (::pipe(stdinPipe) != 0 || ::pipe(stdoutPipe) != 0)
    {
        setStatus("LSP: pipe failed");
        return false;
    }

    const auto pid = ::fork();
    if (pid == 0)
    {
        ::dup2(stdinPipe[0], STDIN_FILENO);
        ::dup2(stdoutPipe[1], STDOUT_FILENO);
        ::close(stdinPipe[0]);
        ::close(stdinPipe[1]);
        ::close(stdoutPipe[0]);
        ::close(stdoutPipe[1]);
        ::execl(serverPath.toRawUTF8(), serverPath.toRawUTF8(), static_cast<char*>(nullptr));
        _exit(127);
    }

    ::close(stdinPipe[0]);
    ::close(stdoutPipe[1]);

    if (pid < 0)
    {
        ::close(stdinPipe[1]);
        ::close(stdoutPipe[0]);
        setStatus("LSP: fork failed");
        return false;
    }

    childPid = pid;
    childStdin = stdinPipe[1];
    childStdout = stdoutPipe[0];

    {
        const juce::ScopedLock scopedLock(lock);
        running = true;
    }

    readerThread = std::thread([this] { readerLoop(); });
    return true;
}

void VoicegroupLspClient::readerLoop()
{
    char chunk[4096] = {};

    while (isRunning())
    {
        int fd = -1;
        {
            const juce::ScopedLock scopedLock(lock);
            fd = childStdout;
        }

        if (fd < 0)
            break;

        const auto bytesRead = ::read(fd, chunk, sizeof(chunk));
        if (bytesRead > 0)
        {
            inputBuffer.append(chunk, static_cast<size_t>(bytesRead));
            parseIncoming();
            continue;
        }

        if (bytesRead == 0 || errno != EINTR)
        {
            markDisconnected("LSP: disconnected");
            break;
        }
    }
}

void VoicegroupLspClient::parseIncoming()
{
    for (;;)
    {
        const auto headerEnd = inputBuffer.find("\r\n\r\n");
        if (headerEnd == std::string::npos)
            return;

        const auto header = inputBuffer.substr(0, headerEnd);
        const auto lengthPos = header.find("Content-Length:");
        if (lengthPos == std::string::npos)
        {
            inputBuffer.erase(0, headerEnd + 4);
            continue;
        }

        const auto lengthStart = lengthPos + std::strlen("Content-Length:");
        const auto length = static_cast<size_t>(std::strtoul(header.c_str() + lengthStart, nullptr, 10));
        const auto bodyStart = headerEnd + 4;
        if (inputBuffer.size() < bodyStart + length)
            return;

        const auto body = inputBuffer.substr(bodyStart, length);
        inputBuffer.erase(0, bodyStart + length);

        auto parsed = juce::JSON::parse(juce::String::fromUTF8(body.data(), static_cast<int>(body.size())));
        if (!parsed.isVoid())
            handleMessage(parsed);
    }
}

void VoicegroupLspClient::handleMessage(const juce::var& message)
{
    if (message.hasProperty("id"))
        handleResponse(message);
    else if (message.hasProperty("method"))
        handleNotification(message);
}

void VoicegroupLspClient::handleResponse(const juce::var& message)
{
    const int id = static_cast<int>(message["id"]);
    RequestKind kind = RequestKind::hover;

    {
        const juce::ScopedLock scopedLock(lock);
        if (pendingRequests.contains(id))
        {
            kind = pendingRequests[id];
            pendingRequests.remove(id);
        }
    }

    const auto result = message["result"];

    if (kind == RequestKind::initialize)
    {
        if (!result.isVoid())
            setStatus("LSP: initialized");
        handleInitializeResponse();
        return;
    }

    if (kind == RequestKind::shutdown)
        return;

    if (kind == RequestKind::completion)
    {
        if (auto* resultObject = result.getDynamicObject())
        {
            auto* items = resultObject->getProperty("items").getArray();
            if (items != nullptr && !items->isEmpty())
            {
                auto first = items->getReference(0);
                auto* item = first.getDynamicObject();
                if (item != nullptr)
                {
                    setStatus("LSP completion: " + item->getProperty("label").toString()
                              + " - " + item->getProperty("detail").toString());
                    return;
                }
            }
        }

        setStatus("LSP completion: no items");
        return;
    }

    if (kind == RequestKind::signatureHelp)
    {
        if (auto* resultObject = result.getDynamicObject())
        {
            auto* signatures = resultObject->getProperty("signatures").getArray();
            if (signatures != nullptr && !signatures->isEmpty())
            {
                auto* signature = signatures->getReference(0).getDynamicObject();
                if (signature != nullptr)
                {
                    setStatus("LSP signature: " + signature->getProperty("label").toString());
                    return;
                }
            }
        }

        return;
    }

    if (auto* resultObject = result.getDynamicObject())
    {
        const auto hover = valueFromMarkupContent(resultObject->getProperty("contents"));
        if (hover.isNotEmpty())
            setStatus("LSP hover: " + hover);
    }
}

void VoicegroupLspClient::handleInitializeResponse()
{
    juce::String stashedText;
    bool shouldFlush = false;

    {
        const juce::ScopedLock scopedLock(lock);
        if (!running || initialized)
            return;

        initialized = true;
        if (hasPendingDocumentText)
        {
            stashedText = std::move(pendingDocumentText);
            pendingDocumentText.clear();
            hasPendingDocumentText = false;
            shouldFlush = true;
        }
    }

    /* Per the LSP spec, `initialized` must follow the initialize response and
     * precede any other notification (including didOpen). */
    sendNotification("initialized", object());

    if (shouldFlush)
        syncDocument(stashedText);
}

void VoicegroupLspClient::handleNotification(const juce::var& message)
{
    if (message["method"].toString() != "textDocument/publishDiagnostics")
        return;

    auto* params = message["params"].getDynamicObject();
    if (params == nullptr)
        return;

    auto* diagnostics = params->getProperty("diagnostics").getArray();
    if (diagnostics == nullptr || diagnostics->isEmpty())
    {
        setStatus("LSP diagnostics: clean");
        return;
    }

    if (auto* first = diagnostics->getReference(0).getDynamicObject())
        setStatus("LSP diagnostic: " + first->getProperty("message").toString());
}

void VoicegroupLspClient::writeBytes(const juce::String& text)
{
    /* writeLock keeps frames from two threads (message thread, reader thread's
     * post-initialize flush) from interleaving on the pipe. It is never taken
     * while holding `lock`, and `lock` is only taken briefly inside the loop,
     * so a blocked write cannot stall state access elsewhere. */
    const juce::ScopedLock scopedWriteLock(writeLock);

    const auto* data = text.toRawUTF8();
    auto bytesRemaining = std::strlen(data);

    while (bytesRemaining > 0)
    {
        int fd = -1;
        {
            const juce::ScopedLock scopedLock(lock);
            fd = childStdin;
        }

        if (fd < 0)
            break;

        const auto written = ::write(fd, data, bytesRemaining);
        if (written <= 0)
        {
            if (errno == EINTR)
                continue;

            markDisconnected("LSP: disconnected");
            break;
        }

        data += written;
        bytesRemaining -= static_cast<size_t>(written);
    }
}

void VoicegroupLspClient::closePipes()
{
    const juce::ScopedLock scopedLock(lock);

    if (childStdin >= 0)
    {
        ::close(childStdin);
        childStdin = -1;
    }

    if (childStdout >= 0)
    {
        ::close(childStdout);
        childStdout = -1;
    }
}

void VoicegroupLspClient::markDisconnected(juce::String status)
{
    const juce::ScopedLock scopedLock(lock);

    if (!running)
        return;

    running = false;
    initialized = false;
    documentOpen = false;
    documentVersion = 0;
    pendingDocumentText.clear();
    hasPendingDocumentText = false;
    statusText = std::move(status);

    if (childStdin >= 0)
    {
        ::close(childStdin);
        childStdin = -1;
    }

    if (childStdout >= 0)
    {
        ::close(childStdout);
        childStdout = -1;
    }
}

void VoicegroupLspClient::waitForChildProcess()
{
    auto pid = -1;
    {
        const juce::ScopedLock scopedLock(lock);
        pid = childPid;
    }

    if (pid <= 0)
        return;

    ::kill(pid, SIGTERM);

    for (auto attempt = 0; attempt < 20; ++attempt)
    {
        if (::waitpid(pid, nullptr, WNOHANG) == pid)
        {
            const juce::ScopedLock scopedLock(lock);
            if (childPid == pid)
                childPid = -1;
            return;
        }

        ::usleep(10000);
    }

    ::kill(pid, SIGKILL);
    ::waitpid(pid, nullptr, 0);

    const juce::ScopedLock scopedLock(lock);
    if (childPid == pid)
        childPid = -1;
}

#else

void VoicegroupLspClient::writeBytes(const juce::String&)
{
}

#endif

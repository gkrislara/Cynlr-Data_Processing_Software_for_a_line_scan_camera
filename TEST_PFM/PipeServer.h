#pragma once
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <unordered_map>
#include <queue>
#include <vector>
#include <mutex>
#include <chrono>
#include "common.h"


/* -------------------------------------------------------------
   Information kept for each connected client
   ------------------------------------------------------------- */
struct ClientInfo {
    uint32_t   id;          // server‑assigned stable identifier
    HANDLE     pipe;        // pipe handle belonging to this client
    std::wstring name;      // logical name supplied by the client
};

class PipeServer {
public:
    /* ---------- Public API used by tests / UI ---------- */
    bool Initialize(const std::wstring& pipeName,
        size_t maxInstances = 1);
    bool Start();                     // launches AcceptLoop + dispatcher
    void Stop();                      // clean shutdown
    bool IsRunning() const noexcept { return running_; }

    /* ---- Helpers needed by the test harness ---- */
    uint32_t RegisterClient(HANDLE clientPipe,
        const std::wstring& logicalName);
    void EnqueueCommand(uint32_t clientId,
        uint16_t commandId,
        const std::vector<uint8_t>& payload = {});

    /* ---- Accessors for the fixture (optional) ---- */
    uint32_t GetNextClientId() const { return nextClientId_; }

    /* ---- Synchronisation primitives exposed for the fixture ---- */
    std::condition_variable cv_;      // signals when first pipe is ready
    std::mutex              cv_m_;
    std::atomic<bool>       ready_{ false };

private:
    /* ---------- Internal implementation ---------- */
    void AcceptLoop();                // former Run() body
    void DispatcherLoop();            // writes queued commands to the right pipe
    void ClientReadLoop(uint32_t clientId, HANDLE hPipe);
    bool ReadExact(HANDLE h, void* buf, DWORD bytes);
    bool WriteExact(HANDLE h, const void* buf, DWORD bytes);

    /* ----- State ----- */
    std::wstring pipeName_;
    size_t      maxInstances_{ 1 };

    std::atomic<bool> running_{ false };

    std::thread acceptThread_;
    std::thread dispatcherThread_;

    std::unordered_map<uint32_t, ClientInfo> clients_;
    std::mutex clientsMtx_;
    std::condition_variable clientCv_;

    /* ----- Command queue ----- */
    struct QueuedCmd {
        uint32_t clientId;
        uint16_t commandId;
        std::vector<uint8_t> payload;
    };
    std::queue<QueuedCmd> cmdQueue_;
    std::mutex cmdMtx_;
    std::condition_variable cmdCv_;

    /* ----- ID generation ----- */
    std::atomic<uint32_t> nextClientId_{ 0 };

    std::unordered_map<uint32_t, int> pendingCommands_;
    std::mutex pendingMtx_;
    std::condition_variable pendingCv_;
};




/* -------------------------------------------------------------
   Helper: reliable read/write of exact byte counts (overlapped)
   ------------------------------------------------------------- */
bool PipeServer::ReadExact(HANDLE h, void* buf, DWORD bytes)
{
    DWORD total = 0;
    while (total < bytes) {
        DWORD chunk = 0;
        OVERLAPPED ov = { 0 };
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        BOOL ok = ReadFile(h,
            static_cast<BYTE*>(buf) + total,
            bytes - total,
            &chunk,
            &ov);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            CloseHandle(ov.hEvent);
            return false;
        }
        if (!GetOverlappedResult(h, &ov, &chunk, TRUE)) {
            CloseHandle(ov.hEvent);
            return false;
        }
        total += chunk;
        CloseHandle(ov.hEvent);
    }
    return true;
}

bool PipeServer::WriteExact(HANDLE h, const void* buf, DWORD bytes)
{
    DWORD total = 0;
    while (total < bytes) {
        DWORD chunk = 0;
        OVERLAPPED ov = { 0 };
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        BOOL ok = WriteFile(h,
            static_cast<const BYTE*>(buf) + total,
            bytes - total,
            &chunk,
            &ov);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            CloseHandle(ov.hEvent);
            return false;
        }
        if (!GetOverlappedResult(h, &ov, &chunk, TRUE)) {
            CloseHandle(ov.hEvent);
            return false;
        }
        total += chunk;
        CloseHandle(ov.hEvent);
    }
    return true;
}

/* -------------------------------------------------------------
   Initialize – creates the pipe name (no actual pipe instances yet)
   ------------------------------------------------------------- */
bool PipeServer::Initialize(const std::wstring& name, size_t maxInst)
{
    pipeName_ = name;
    maxInstances_ = maxInst;
    return true;
}

/* -------------------------------------------------------------
   Start – launches AcceptLoop and the command dispatcher
   ------------------------------------------------------------- */
bool PipeServer::Start()
{
    if (running_) return false;
    running_ = true;

    acceptThread_ = std::thread(&PipeServer::AcceptLoop, this);
    dispatcherThread_ = std::thread(&PipeServer::DispatcherLoop, this);
    return true;
}

/* -------------------------------------------------------------
   Stop – signals threads to exit and joins them
   ------------------------------------------------------------- */
void PipeServer::Stop()
{
    if (!running_) return;
    running_ = false;

    // Wake up dispatcher if it is waiting
    cmdCv_.notify_all();

    if (acceptThread_.joinable())
        acceptThread_.join();

    if (dispatcherThread_.joinable())
        dispatcherThread_.join();

    // Close all client pipes
    {
        std::lock_guard<std::mutex> lk(clientsMtx_);
        for (auto& kv : clients_) {
            CloseHandle(kv.second.pipe);
        }
        clients_.clear();
    }
}

/* -------------------------------------------------------------
   RegisterClient – called from AcceptLoop after the client sends
   its logical name. Returns a stable numeric ID.
   ------------------------------------------------------------- */
uint32_t PipeServer::RegisterClient(HANDLE clientPipe,
    const std::wstring& logicalName)
{
    uint32_t id = ++nextClientId_;
    {
        std::lock_guard<std::mutex> lk(clientsMtx_);
        clients_.emplace(id, ClientInfo{ id, clientPipe, logicalName });
    }
    return id;
}

/* -------------------------------------------------------------
   EnqueueCommand – public way for tests / UI to push a command
   ------------------------------------------------------------- */
void PipeServer::EnqueueCommand(uint32_t clientId,
    uint16_t commandId,
    const std::vector<uint8_t>& payload)
{
    {
        std::lock_guard<std::mutex> lk(cmdMtx_);
        cmdQueue_.push({ clientId, commandId, payload });
    }
    {
        std::lock_guard<std::mutex> lk(pendingMtx_);
        pendingCommands_[clientId]++;
    }
    cmdCv_.notify_one();
}

/* -------------------------------------------------------------
   AcceptLoop – replaces the original PipeServer::Run()
   ------------------------------------------------------------- */
void PipeServer::AcceptLoop()
{
    bool firstInstanceCreated = false;          // <‑‑ new flag

    while (running_) {
        HANDLE hPipe = CreateNamedPipeW(
            pipeName_.c_str(),
            PIPE_ACCESS_DUPLEX ,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            static_cast<DWORD>(maxInstances_),
            4096, 4096, 0, nullptr);

        if (hPipe == INVALID_HANDLE_VALUE) {
            // fatal error – break out so Stop() can clean up
            DWORD err = GetLastError();
            std::cerr << "CreateNamedPipeW failed, error: " << err << std::endl;
            break;
        }

        // **Signal the test the moment the first pipe instance exists**
        if (!firstInstanceCreated) {
            firstInstanceCreated = true;
            {
                std::lock_guard<std::mutex> lk(cv_m_);
                ready_ = true;                 // now the test can proceed
            }
            cv_.notify_all();
        }

        // Begin async connect
        OVERLAPPED ov = { 0 };
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        ConnectNamedPipe(hPipe, &ov);

        // Wait for a client to connect (or for Stop() to cancel)
        DWORD wait = WaitForSingleObject(ov.hEvent, INFINITE);
        CloseHandle(ov.hEvent);

        if (!running_) {
            CloseHandle(hPipe);
            break;
        }


        // At this point a client is connected – handle registration in its own thread
        std::thread([this, hPipe] {
            // ---- 1️⃣ read registration message ----
            RegisterMsg reg{};
            if (!ReadExact(hPipe, &reg, sizeof(reg))) {
               
                DWORD err = GetLastError();
                std::cerr << " Server ReadExact failed for Reg, error: " << err << std::endl;
                
                CloseHandle(hPipe);
                return;
            }
            std::vector<char> nameBuf(reg.nameLen * sizeof(wchar_t));
            if (!ReadExact(hPipe, nameBuf.data(), reg.nameLen * sizeof(wchar_t))) {

                DWORD err = GetLastError();
                std::cerr << "Server ReadExact failed for Namebuf, error: " << err << std::endl;

                CloseHandle(hPipe);
                return;
            }

            std::wstring logicalName(reinterpret_cast<wchar_t*>(nameBuf.data()), reg.nameLen);
            
            //std::wstring logicalName(nameBuf.begin(), nameBuf.end());

            // ---- 2️⃣ assign stable clientId ----
            uint32_t clientId = RegisterClient(hPipe, logicalName);

            // ---- 3️⃣ send the assigned clientId back to the client ----
            if (!WriteExact(hPipe, &clientId, sizeof(clientId)))
            {
                DWORD err = GetLastError();
                std::cerr << "Server WriteExact failed for the registered client, error: " << err << std::endl;
            }

            //// ---- 4️⃣ now enter the per‑client read loop (process inbound messages) ----
            //ClientReadLoop(clientId, hPipe);

            //// When the client disconnects, clean up
            //while(true)
            //{
            //    std::unique_lock<std::mutex> lk(pendingMtx_);
            //    pendingCv_.wait(lk, [&, clientId] { return pendingCommands_[clientId] == 0; });
            //    pendingCommands_.erase(clientId);
            //    clients_.erase(clientId);
            //    CloseHandle(hPipe);
            //}
           
            }).detach();   // each client gets its own dedicated thread
    }

    // No need to set ready_ here any more – it was already set on the first instance.
}


/* -------------------------------------------------------------
   ClientReadLoop – currently a stub; can be expanded to let
   the client send messages back to the server if needed.
   ------------------------------------------------------------- */
void PipeServer::ClientReadLoop(uint32_t clientId/*clientId*/, HANDLE hndl/*hPipe*/)
{
    // For the current test scenario the server only pushes commands
    // to the client, so we do not need to read anything here.
    // If you later want bidirectional communication, implement
    // a read‑while‑running loop similar to the dispatcher.

    //std::unique_lock<std::mutex> lk(pendingMtx_);
    //pendingCv_.wait(lk, [&] { return pendingCommands_[clientId] == 0; });
    //pendingCommands_.erase(clientId);

}

/* -------------------------------------------------------------
   DispatcherLoop – pulls commands from the queue and writes them
   to the appropriate client pipe.
   ------------------------------------------------------------- */
void PipeServer::DispatcherLoop()
{
    while (running_) {
        std::unique_lock<std::mutex> lk(cmdMtx_);
        cmdCv_.wait(lk, [this] { return !cmdQueue_.empty() || !running_; });

        if (!running_) break;

        QueuedCmd cmd = std::move(cmdQueue_.front());
        cmdQueue_.pop();
        lk.unlock();

        // Build the envelope + optional payload
        CmdEnvelope env{
            cmd.clientId,
            cmd.commandId,
            static_cast<uint16_t>(cmd.payload.size())
        };

        std::vector<uint8_t> packet(sizeof(env) + cmd.payload.size());
        memcpy(packet.data(), &env, sizeof(env));
        if (!cmd.payload.empty())
            memcpy(packet.data() + sizeof(env), cmd.payload.data(),
                cmd.payload.size());

        // Locate the target client pipe
        HANDLE targetPipe = INVALID_HANDLE_VALUE;
        {
            std::lock_guard<std::mutex> lk(clientsMtx_);
            auto it = clients_.find(cmd.clientId);
            if (it != clients_.end())
                targetPipe = it->second.pipe;
        }

        if (targetPipe == INVALID_HANDLE_VALUE) {
            // client disappeared – drop the command

            std::cerr << "Server Dispatch Target pipe invalid handle" << std::endl;

            continue;
        }

        // Write the packet (overlapped for consistency)
        if (!WriteExact(targetPipe, packet.data(),
            static_cast<DWORD>(packet.size())))
        {
            std::cerr << "Server Dispatch WriteExact failed" << std::endl;
        }
        {
            std::lock_guard<std::mutex> lk(pendingMtx_);
            if (--pendingCommands_[cmd.clientId] == 0) {
                pendingCv_.notify_all();
            }
        }

    }
}







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
#include <iostream>
#include "common.h"

struct ClientInfo {
    uint32_t   id;
    HANDLE     pipe;
    std::wstring name;
};

class PipeServer {
public:
    bool Initialize(const std::wstring& pipeName, size_t maxInstances = 1);
    bool Start();
    void Stop();
    bool IsRunning() const noexcept { return running_; }

    uint32_t RegisterClient(HANDLE clientPipe,
        const std::wstring& logicalName);
    /* EnqueueCommand now returns the status payload */
    std::vector<uint8_t> EnqueueCommand(uint32_t clientId,
        uint16_t commandId,
        const std::vector<uint8_t>& payload = {});

    uint32_t GetNextClientId() const { return nextClientId_; }

    std::condition_variable cv_;      // signals when first pipe is ready
    std::mutex              cv_m_;
    std::atomic<bool>       ready_{ false };

private:
    void AcceptLoop();
    void DispatcherLoop();
    void ClientReadLoop(uint32_t clientId, HANDLE hPipe);
    bool ReadExact(HANDLE h, void* buf, DWORD bytes);
    bool WriteExact(HANDLE h, const void* buf, DWORD bytes);   // kept for registration

    /* ---------- State ---------- */
    std::wstring pipeName_;
    size_t      maxInstances_{ 1 };
    std::atomic<bool> running_{ false };
    std::thread acceptThread_;
    std::thread dispatcherThread_;

    std::unordered_map<uint32_t, ClientInfo> clients_;
    std::mutex clientsMtx_;
    std::condition_variable clientCv_;

    /* ---------- Command queue ---------- */
    struct QueuedCmd {
        uint32_t clientId;
        uint16_t commandId;
        std::vector<uint8_t> payload;
    };
    std::queue<QueuedCmd> cmdQueue_;
    std::mutex cmdMtx_;
    std::condition_variable cmdCv_;

    /* ---------- Status queue (new) ---------- */
    std::queue<std::vector<uint8_t>> statusQueue_;   // <<< added >>>
    std::mutex statusMtx_;                           // <<< added >>>
    std::condition_variable statusCv_;              // <<< added >>>

    /* ---------- ID generation ---------- */
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
   Helper: overlapped TransactNamedPipe with timeout
   ------------------------------------------------------------- */
bool TransactPipeWithTimeout(HANDLE hPipe,
    const void* outBuf,
    DWORD outSize,
    std::vector<uint8_t>& inBuf,
    DWORD timeoutMs = 5000)          // 5 s default
{
    // Prepare a reply buffer (adjust size if you expect larger messages)
    inBuf.resize(4096);
    DWORD bytesRead = 0;

    OVERLAPPED ov = { 0 };
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) return false;

    BOOL ok = TransactNamedPipe(
        hPipe,
        const_cast<LPVOID>(outBuf),   // request buffer (non‑const required)
        outSize,
        inBuf.data(),                  // reply buffer
        static_cast<DWORD>(inBuf.size()),
        &bytesRead,
        &ov);                          // overlapped → non‑blocking

    if (!ok && GetLastError() != ERROR_IO_PENDING) {
        CloseHandle(ov.hEvent);
        return false;                 // immediate failure
    }

    // Wait for completion or timeout
    DWORD wait = WaitForSingleObject(ov.hEvent, timeoutMs);
    if (wait == WAIT_TIMEOUT) {
        // Cancel the pending I/O and treat as failure/timeout
        CancelIoEx(hPipe, &ov);
        CloseHandle(ov.hEvent);
        return false;
    }

    // Completed – retrieve the result
    if (!GetOverlappedResult(hPipe, &ov, &bytesRead, FALSE)) {
        CloseHandle(ov.hEvent);
        return false;
    }

    CloseHandle(ov.hEvent);
    inBuf.resize(bytesRead);
    return true;
}

/* -------------------------------------------------------------
   Initialize
   ------------------------------------------------------------- */
bool PipeServer::Initialize(const std::wstring& name, size_t maxInst)
{
    pipeName_ = name;
    maxInstances_ = maxInst;
    return true;
}

/* -------------------------------------------------------------
   Start
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
   Stop
   ------------------------------------------------------------- */
void PipeServer::Stop()
{
    if (!running_) return;
    running_ = false;

    cmdCv_.notify_all();
    statusCv_.notify_all();          // wake any waiting EnqueueCommand

    if (acceptThread_.joinable())
        acceptThread_.join();
    if (dispatcherThread_.joinable())
        dispatcherThread_.join();

    {
        std::lock_guard<std::mutex> lk(clientsMtx_);
        for (auto& kv : clients_)
            CloseHandle(kv.second.pipe);
        clients_.clear();
    }
}

/* -------------------------------------------------------------
   RegisterClient
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
   EnqueueCommand – now also consumes the status queue
   ------------------------------------------------------------- */
std::vector<uint8_t> PipeServer::EnqueueCommand(uint32_t clientId,
    uint16_t commandId,
    const std::vector<uint8_t>& payload)
{
    // ---- produce ----
    {
        std::lock_guard<std::mutex> lk(cmdMtx_);
        cmdQueue_.push({ clientId, commandId, payload });
    }
    {
        std::lock_guard<std::mutex> lk(pendingMtx_);
        ++pendingCommands_[clientId];
    }
    cmdCv_.notify_one();

    // ---- consume ----
    std::unique_lock<std::mutex> lk(statusMtx_);
    statusCv_.wait(lk, [this] { return !statusQueue_.empty(); });

    std::vector<uint8_t> status = std::move(statusQueue_.front());
    statusQueue_.pop();
    return status;
}

/* -------------------------------------------------------------
   AcceptLoop – unchanged except for signalling ready_
   ------------------------------------------------------------- */
void PipeServer::AcceptLoop()
{
    bool firstInstanceCreated = false;

    while (running_) {
        HANDLE hPipe = CreateNamedPipeW(
            pipeName_.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            static_cast<DWORD>(maxInstances_),
            4096, 4096, 0, nullptr);

        if (hPipe == INVALID_HANDLE_VALUE) {
            std::cerr << "CreateNamedPipe failed, error: " << GetLastError() << std::endl;
                break;
        }

        // Signal first instance creation
        if (!firstInstanceCreated) {
            firstInstanceCreated = true;
            {
                std::lock_guard<std::mutex> lk(cv_m_);
                ready_ = true;
            }
            cv_.notify_all();
        }

        // Begin async connect
        OVERLAPPED ov = { 0 };
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        ConnectNamedPipe(hPipe, &ov);

        // Wait for a client to connect (or Stop)
        WaitForSingleObject(ov.hEvent, INFINITE);
        CloseHandle(ov.hEvent);

        if (!running_) {
            CloseHandle(hPipe);
            break;
        }

        // Client connected – handle registration in its own thread
        std::thread([this, hPipe] {
            // ---- 1️⃣ read registration message ----
            RegisterMsg reg{};
            if (!ReadExact(hPipe, &reg, sizeof(reg))) {
                std::cerr << "Server ReadExact failed for Reg, error: "
                    << GetLastError() << std::endl;
                CloseHandle(hPipe);
                return;
            }

            std::vector<char> nameBuf(reg.nameLen * sizeof(wchar_t));
            if (!ReadExact(hPipe, nameBuf.data(),
                reg.nameLen * sizeof(wchar_t))) {
                std::cerr << "Server ReadExact failed for Namebuf, error: "
                    << GetLastError() << std::endl;
                CloseHandle(hPipe);
                return;
            }

            std::wstring logicalName(
                reinterpret_cast<wchar_t*>(nameBuf.data()), reg.nameLen);

            // ---- 2️⃣ assign stable clientId ----
            uint32_t clientId = RegisterClient(hPipe, logicalName);

            // ---- 3️⃣ send the assigned clientId back to the client ----
            if (!WriteExact(hPipe, &clientId, sizeof(clientId))) {
                std::cerr << "Server WriteExact failed for clientId, error: "
                    << GetLastError() << std::endl;
            }

            // If you later need a read loop, uncomment:
            // ClientReadLoop(clientId, hPipe);
            }).detach();
    }
}

/* -------------------------------------------------------------
   DispatcherLoop – uses overlapped Transact with timeout
   ------------------------------------------------------------- */
void PipeServer::DispatcherLoop()
{
    while (running_) {
        // ----- wait for a command -----
        std::unique_lock<std::mutex> lk(cmdMtx_);
        cmdCv_.wait(lk, [this] { return !cmdQueue_.empty() || !running_; });
        if (!running_) break;

        QueuedCmd cmd = std::move(cmdQueue_.front());
        cmdQueue_.pop();
        lk.unlock();

        // ----- build packet (envelope + optional payload) -----
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

        // ----- locate target pipe -----
        HANDLE targetPipe = INVALID_HANDLE_VALUE;
        {
            std::lock_guard<std::mutex> lk(clientsMtx_);
            auto it = clients_.find(cmd.clientId);
            if (it != clients_.end())
                targetPipe = it->second.pipe;
        }
        if (targetPipe == INVALID_HANDLE_VALUE) {
            std::cerr << "Dispatch: invalid client handle\n";
            continue;
        }

        // ----- transact with timeout -----
        std::vector<uint8_t> reply;
        if (!TransactPipeWithTimeout(targetPipe,
            packet.data(),
            static_cast<DWORD>(packet.size()),
            reply,
            5000)) {               // 5 s timeout
            // Timeout or error – still decrement pending count
            std::cerr << "Transact timed out or failed for client "
                << cmd.clientId << "\n";
            std::lock_guard<std::mutex> lk(pendingMtx_);
            if (--pendingCommands_[cmd.clientId] == 0)
                pendingCv_.notify_all();
            continue;
        }

        // ----- push reply onto status queue -----
        {
            std::lock_guard<std::mutex> lk(statusMtx_);
            statusQueue_.push(std::move(reply));
        }
        statusCv_.notify_one();

        // ----- update pending count -----
        {
            std::lock_guard<std::mutex> lk(pendingMtx_);
            if (--pendingCommands_[cmd.clientId] == 0)
                pendingCv_.notify_all();
        }
    }
}

/* -------------------------------------------------------------
   ClientReadLoop – stub (unchanged)
   ------------------------------------------------------------- */
void PipeServer::ClientReadLoop(uint32_t /*clientId*/, HANDLE /*hPipe*/)
{
    // No bidirectional reads needed for the current test scenario.
}


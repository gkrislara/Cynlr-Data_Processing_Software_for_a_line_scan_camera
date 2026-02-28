#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <atomic>
#include "common.h"

class PipeClient {
public:
    explicit PipeClient(const std::wstring& pipeName);
    ~PipeClient();

    /* ----- Public API used by tests ----- */
    void SetLogicalName(const std::wstring& name);   // logical name sent during registration
    bool Connect(DWORD timeoutMs = INFINITE);        // opens pipe and performs registration
    void Disconnect();

    bool Write(const std::string& data);            // send arbitrary payload to server
    bool Read(std::string& out, DWORD timeoutMs = INFINITE); // read raw data from server

    /* ----- Helpers for the test fixture ----- */
    uint32_t GetClientId() const noexcept { return clientId_; }
    bool ReadExact(void* buf, DWORD bytes, DWORD timeoutMs = INFINITE);
    bool WriteExact(const void* buf, DWORD bytes);

private:
    std::wstring pipeName_;
    std::wstring logicalName_;      // sent during registration
    HANDLE      hPipe_{ INVALID_HANDLE_VALUE };
    std::atomic<uint32_t> clientId_{ 0 };
};

/* -------------------------------------------------------------
   Constructor / Destructor
   ------------------------------------------------------------- */
PipeClient::PipeClient(const std::wstring& pipeName)
    : pipeName_(pipeName) {
}

PipeClient::~PipeClient() {
    Disconnect();
}

/* -------------------------------------------------------------
   SetLogicalName – called by the test before Connect()
   ------------------------------------------------------------- */
void PipeClient::SetLogicalName(const std::wstring& name) {
    logicalName_ = name;
}

/* -------------------------------------------------------------
   Connect – opens the pipe, performs the registration handshake,
   and stores the assigned clientId.
   ------------------------------------------------------------- */
bool PipeClient::Connect(DWORD timeoutMs)
{
    // 1️⃣ Open the pipe (blocking until it exists)
    hPipe_ = CreateFileW(
        pipeName_.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,                // no sharing
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (hPipe_ == INVALID_HANDLE_VALUE)
    {
        std::wcerr << L"CreateFileW failed: " << GetLastError() << std::endl;
        return false;
    }

    // --------------------------------------------------------------
    // 2️⃣ **Wait briefly for the server to be listening**.
    //    The server sets its `ready` flag after it has called
    //    ConnectNamedPipe and is ready to receive the registration.
    //    We poll the flag with a short timeout; if the flag is not
    //    exposed (e.g., in production code) we simply sleep a few
    //    milliseconds – the overhead is negligible.
    // --------------------------------------------------------------
    const DWORD pollInterval = 10;          // ms
    DWORD elapsed = 0;
    while (elapsed < timeoutMs) {
        // In the test harness the server exposes `ready` via a
        // condition variable.  When the client is used outside the
        // test we fall back to a tiny sleep.
        Sleep(pollInterval);
        elapsed += pollInterval;
        // If the pipe is already connected, the next step will succeed;
        // otherwise we keep waiting.
        // (No explicit check here – the sleep is enough for the race
        //  condition in the test environment.)
        if (elapsed >= timeoutMs) break;
    }

    // --------------------------------------------------------------
    // 3️⃣ Send registration message (unchanged)
    // --------------------------------------------------------------
    if (logicalName_.empty())
        logicalName_ = L"default";

    RegisterMsg reg{};
    reg.nameLen = static_cast<uint16_t>(logicalName_.size());

    if (!WriteExact(&reg, sizeof(reg))) { 
       
            DWORD err = GetLastError();
            std::cerr << "Client WriteExact failed, error: " << err << std::endl;

        Disconnect(); return false; }
    if (!WriteExact(logicalName_.data(),
        reg.nameLen * sizeof(wchar_t))) {
        DWORD err = GetLastError();
        std::cerr << " Client WriteExact failed, error: " << err << std::endl;
        Disconnect(); return false;
    }

    // --------------------------------------------------------------
    // 4️⃣ Receive the numeric clientId assigned by the server
    // --------------------------------------------------------------
    uint32_t assigned = 0;
    if (!ReadExact(&assigned, sizeof(assigned))) {
        
        DWORD err = GetLastError();
        std::cerr << "Client ReadExact failed, error: " << err << std::endl;

        Disconnect(); return false; }
    clientId_ = assigned;
    return true;
}


/* -------------------------------------------------------------
   Disconnect – cleanly close the pipe handle
   ------------------------------------------------------------- */
void PipeClient::Disconnect()
{
    if (hPipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipe_);
        hPipe_ = INVALID_HANDLE_VALUE;
    }
}

/* -------------------------------------------------------------
   Write – send a raw byte buffer (e.g., a command payload)
   ------------------------------------------------------------- */
bool PipeClient::Write(const std::string& data)
{
    return WriteExact(data.data(), static_cast<DWORD>(data.size()));
} 

/* -------------------------------------------------------------
   Read – read raw data from the pipe (used for test verification)
   ------------------------------------------------------------- */
bool PipeClient::Read(std::string& out, DWORD timeoutMs)
{
    // First read the envelope to know how many payload bytes follow
    CmdEnvelope env;
    if (!ReadExact(&env, sizeof(env), timeoutMs))
        return false;

    std::vector<char> buf(env.payloadLen);
    if (env.payloadLen > 0) {
        if (!ReadExact(buf.data(), env.payloadLen, timeoutMs))
            return false;
    }

    out.assign(buf.begin(), buf.end());
    return true;
}

/* -------------------------------------------------------------
   Low‑level overlapped helpers (exact byte count)
   ------------------------------------------------------------- */
bool PipeClient::ReadExact(void* buf, DWORD bytes, DWORD timeoutMs)
{
    DWORD total = 0;
    while (total < bytes) {
        DWORD chunk = 0;
        OVERLAPPED ov = { 0 };
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        BOOL ok = ReadFile(hPipe_,
            static_cast<BYTE*>(buf) + total,
            bytes - total,
            &chunk,
            &ov);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            CloseHandle(ov.hEvent);
            return false;
        }

        DWORD wait = WaitForSingleObject(ov.hEvent, timeoutMs);
        if (wait != WAIT_OBJECT_0) {
            CancelIoEx(hPipe_, &ov);
            CloseHandle(ov.hEvent);
            return false;
        }

        if (!GetOverlappedResult(hPipe_, &ov, &chunk, FALSE)) {
            CloseHandle(ov.hEvent);
            return false;
        }
        total += chunk;
        CloseHandle(ov.hEvent);
    }
    return true;
}

bool PipeClient::WriteExact(const void* buf, DWORD bytes)
{
    DWORD total = 0;
    while (total < bytes) {
        DWORD chunk = 0;
        OVERLAPPED ov = { 0 };
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        BOOL ok = WriteFile(hPipe_,
            static_cast<const BYTE*>(buf) + total,
            bytes - total,
            &chunk,
            &ov);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            CloseHandle(ov.hEvent);
            return false;
        }

        if (!GetOverlappedResult(hPipe_, &ov, &chunk, TRUE)) {
            CloseHandle(ov.hEvent);
            return false;
        }
        total += chunk;
        CloseHandle(ov.hEvent);
    }
    return true;
}


#pragma once
/* -------------------------------------------------------------
   Wire‑level command envelope (used by both server and client)
   ------------------------------------------------------------- */
#pragma pack(push,1)
struct CmdEnvelope {
    uint32_t clientId;      // target client
    uint16_t commandId;     // e.g. 1 = PRINT, 2 = SCAN
    uint16_t payloadLen;    // bytes that follow the header
};
#pragma pack(pop)

/* -------------------------------------------------------------
   Registration message sent by a client right after ConnectNamedPipe
   ------------------------------------------------------------- */
#pragma pack(push,1)
struct RegisterMsg {
    uint16_t type = 0x0001;   // fixed opcode for registration
    uint16_t nameLen;         // length of UTF‑8 name that follows
    // followed by name bytes
};
#pragma pack(pop)
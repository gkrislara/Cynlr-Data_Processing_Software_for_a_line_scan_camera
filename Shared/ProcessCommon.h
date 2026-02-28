#pragma once
#include <map>
#include <string>
#include <functional>
#include <windows.h>
#include <vector>
/* commom utilies for process manager(Observer FE) and process (BE) agnostic of platform */

/* -------------------------------------------------------------
   Wire‑level command envelope (used by both server and client)
   ------------------------------------------------------------- */
#pragma pack(push,1)
template<typename T>
struct CmdEnvelope {
	DWORD clientId;      // target client
	T commandId;     // e.g. 1 = PRINT, 2 = SCAN
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

template<typename T>
struct QueuedCmd {
	DWORD clientId;
	T commandId;
	std::vector<uint8_t> payload;
};


typedef enum class ProcessState {
	INIT,
	SETUP,
	IDLE,
	RUNNING,
	ERR
}ProcessState_e;

/* base containers for accomodating different clients */
//template<typename T>
//class ProcessBase {
//	 std::map<T, std::function<void()>> methodMap;
//
//public:
//    virtual void update(T value) = 0;
//	//virtual void setInputMap(/* TODO: FIX ON PARAMETER */) = 0;
//	//virtual void getInputMap(/* TODO: FIX ON PARAMETER */) = 0;
//
//	void execute(T command)
//	{
//		auto it = methodMap.find(command);
//		if (it != methodMap.end()) {
//			it->second(); // Invoke the stored function
//		}
//	}
//
//	void registerMethod(const T& value, std::function<void()> func) {
//		methodMap[value] = func;
//	}
//};

template<typename T>
class PlatformBase {
public:
	virtual bool attachProcesses() = 0;
};
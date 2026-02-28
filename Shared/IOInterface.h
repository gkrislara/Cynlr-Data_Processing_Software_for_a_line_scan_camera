#pragma once
#include <string>
#include <vector>
#include <iostream>

#ifdef _WIN32

#include <windows.h>

#define DEFAULT_NAMED_PIPE_SERVER L"\\\\.\\pipe\\WIOPIPESERVER"

typedef enum class IOInterfaceType{
	NAMED_PIPE_SERVER,
	NAMED_PIPE_CLIENT,
	NUM_INTERFACE_TYPES
}IOInterfaceType_e;
/*
	Add other interface
	parameters
	here
   */
typedef struct {
	std::wstring pipeName;
	uint32_t accessType;
	uint32_t maxClients;
	DWORD pipeBufferSize;
	DWORD connectTimeout;
	/* other createnamedpipe params */
}NamedPipeServerParam_s;

typedef struct {
	std::wstring pipeName;
	DWORD Id;
	uint32_t accessType;
	DWORD connectTimeoutMs;
	DWORD maxReconnectAttempts;
	DWORD retryBackoffMs;
	/* other createfile params */
}NamedPipeClientParam_s;


typedef struct {
	IOInterfaceType_e interfaceType;
	NamedPipeServerParam_s namedPipeServerParam;
	NamedPipeClientParam_s namedPipeClientParam;
}InterfaceParam_s;

#endif

template <typename T, size_t dataWidth>
class IOInterfaceBase{
public:
	virtual void setup(InterfaceParam_s* param) = 0;
	virtual bool start() = 0;
	virtual bool sendData(void* data) = 0; // essentially a subject behaviour
	virtual bool receiveData(T& commandId, std::vector<T>& data) = 0; // commandId is explicit and breaks abstraction. ideally should go into payload a decoder needs to parse the payload data; done in haste for submission
	virtual bool stop() = 0;
	virtual ~IOInterfaceBase() = default;
};

	
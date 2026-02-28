#pragma once

#include <windows.h>
#include "IOInterface.h"

/* TODO: Move this to separate Unified Error Code header*/
typedef enum class IOClientError {
	SUCCESS,
	ERR_IOCLIENT_INTERFACE_CONNECT_FAIL,
	ERR_IOCLIENT_GLE_INTERFACE_ACK_FAIL,
	ERR_IOCLIENT_GLE_READFILE_FAIL,
	NUM_IOCLIENT_ERR
}IOClientError_e;

template<typename T,size_t dataWidth>
class IOClient : public IOInterfaceBase<T, dataWidth> {

private:
	InterfaceParam_s clientParameter;
	HANDLE interfaceHandle;
	DWORD trf;
	IOClientError_e currentError;
	HANDLE hPipe;

	DWORD clientId;

#pragma pack(push, 1)
	struct registerMsg {
		uint32_t type;      // expected 0x0001
		DWORD Id;  // length of logical name in wchar_t units
		// followed by nameLen wchar_t characters (no terminating L'\0')
	};
#pragma pack(pop)
	
	OVERLAPPED ov = {};


	HANDLE connectIntfWithTimeout()
	{
		if (clientParameter.interfaceType == IOInterfaceType::NAMED_PIPE_CLIENT)
		{
				interfaceHandle = CreateFileW(
					clientParameter.namedPipeClientParam.pipeName.c_str(),
					GENERIC_READ | GENERIC_WRITE,
					0,
					NULL,
					OPEN_EXISTING,
					FILE_FLAG_OVERLAPPED,
					NULL
				);

			if (interfaceHandle == INVALID_HANDLE_VALUE) {
				return INVALID_HANDLE_VALUE;
			}

			DWORD dwMode = PIPE_READMODE_MESSAGE;
			if (!SetNamedPipeHandleState(interfaceHandle, &dwMode, NULL, NULL)) {
				CloseHandle(interfaceHandle);
				return INVALID_HANDLE_VALUE; //check
			}

			OVERLAPPED ov = {};
			ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
			if (!ConnectNamedPipe(interfaceHandle, &ov) && GetLastError() != ERROR_IO_PENDING)
			{
				CloseHandle(ov.hEvent);
				return interfaceHandle;
			}

			DWORD wait = WaitForSingleObject(ov.hEvent,
				clientParameter.namedPipeClientParam.connectTimeoutMs);
			CloseHandle(ov.hEvent);
			if (wait != WAIT_OBJECT_0)
			{
				CloseHandle(interfaceHandle);
				return INVALID_HANDLE_VALUE;
			}

		}

		return interfaceHandle;
	}

public:

	IOClient() {
		interfaceHandle = INVALID_HANDLE_VALUE;
		trf = 0;
		hPipe = INVALID_HANDLE_VALUE;
		clientId = 0;
	}


	~IOClient() {
		stop();
	}

	void setup(InterfaceParam_s* clientParam) override
	{
		memcpy(&clientParameter, clientParam, sizeof(clientParameter));
	}


	bool start() override
	{
		int attempt = 0;
		
		currentError = IOClientError::SUCCESS;

		if (clientParameter.interfaceType == IOInterfaceType::NAMED_PIPE_CLIENT)
		{
			registerMsg reg{};
			while (attempt < clientParameter.namedPipeClientParam.maxReconnectAttempts)
			{
				++attempt;

				hPipe = connectIntfWithTimeout();

				if (hPipe == INVALID_HANDLE_VALUE)
				{
					currentError = IOClientError::ERR_IOCLIENT_INTERFACE_CONNECT_FAIL;
					continue;
				}

				/* Handshake */
				
				reg.type = 0x0001;

				/* send id instead of name*/
				reg.Id = static_cast<DWORD>(clientParameter.namedPipeClientParam.Id);


				DWORD bytesReturned = 0;

				BOOL ok = TransactNamedPipe(
					hPipe,
					&reg, sizeof(reg),
					&clientId, sizeof(clientId),
					&bytesReturned,
					nullptr);

				if (!ok)
				{
					currentError = IOClientError::ERR_IOCLIENT_GLE_INTERFACE_ACK_FAIL;
					CloseHandle(hPipe);
					hPipe = INVALID_HANDLE_VALUE;
					Sleep(clientParameter.namedPipeClientParam.retryBackoffMs);
					continue;
				}
				break;
			}
			if ((hPipe == INVALID_HANDLE_VALUE) || (clientId != reg.Id)) return false;
			else
			{
				
				ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
				if (!ov.hEvent) {
					std::cerr << "CreateEvent failed, error " << GetLastError() << '\n';
					CloseHandle(hPipe);
					return false;
				}
				return true;
			}
		}
		
	}



	bool sendData(void* message) override
	{
		/*  not implemented*/
		BOOL ok = WriteFile(
			hPipe,
			message,
			sizeof(message),
			&trf,
			nullptr);

		if (ok)
			return true;
		else
			return false;
	}


	bool receiveData(T& commandId,std::vector<T>& payload) override
	{
		// First read the envelope to know how many payload bytes follow
		//TODO: Follow IOServer x IOClient Messaging Protocol ; 
		// at this moment it follows the command protocol which sits in logic layer i.e process client
		CmdEnvelope<T> env;
		/*if (!ReadExact(&env, sizeof(env), 0))
			return false;*/
		BOOL ok = ReadFile(
			hPipe,
			&env,
			sizeof(env),
			&trf,
			&ov);

		if (!ok && GetLastError() != ERROR_IO_PENDING) {
			std::cerr << "ReadFile failed, error " << GetLastError() << '\n';
			ResetEvent(ov.hEvent);
		}


		WaitForSingleObject(ov.hEvent, INFINITE);
		DWORD bytesRead = 0;
		GetOverlappedResult(hPipe, &ov, &bytesRead, FALSE);

		if (bytesRead > 0) 
		{
			/* verify client id*/
			if (env.clientId == clientId)
			{
				commandId = static_cast<T>(env.commandId);

				if (env.payloadLen > 0) {

					payload.resize(env.payloadLen);

					if (!ReadExact(payload.data(), env.payloadLen, 0))
						//TODO: SetErrorCode
						return false;
				}
			}
			else
			{
				ResetEvent(ov.hEvent);
				/* TODO: SetErrorCode*/
				return false;
			}


			ResetEvent(ov.hEvent);
			return true;
		}

		ResetEvent(ov.hEvent);
		return false;
	}

	/* overlapped helper */
	bool ReadExact(void* buf, DWORD bytes, DWORD timeoutMs)
	{
		DWORD total = 0;
		while (total < bytes) {
			DWORD chunk = 0;
			OVERLAPPED ov = { 0 };
			ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

			BOOL ok = ReadFile(hPipe,
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
				CancelIoEx(hPipe, &ov);
				CloseHandle(ov.hEvent);
				return false;
			}

			if (!GetOverlappedResult(hPipe, &ov, &chunk, FALSE)) {
				CloseHandle(ov.hEvent);
				return false;
			}
			total += chunk;
			CloseHandle(ov.hEvent);
		}
		return true;
	}

	bool stop() override
	{
		if (interfaceHandle != INVALID_HANDLE_VALUE) {
			CloseHandle(interfaceHandle);
		}
		return true;
	}
};

#include <iostream>
#include "LS_DLB.h"

std::wstring GetOwnPath()
{
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::wstring(buf, len);
}

std::wstring GetOwnFileName()
{
    std::wstring path = GetOwnPath();
    size_t pos = path.find_last_of(L"\\/");

    std::wstring fileName = (pos == std::wstring::npos)
        ? path
        : path.substr(pos + 1);

    std::size_t dotPos = fileName.find_last_of(L'.');
    if (dotPos != std::wstring::npos)
        fileName.erase(dotPos);

    return fileName;
}

int main(int argc, char* argv[])
{
    InterfaceParam_s clientParam;
    clientParam.interfaceType = IOInterfaceType::NAMED_PIPE_CLIENT;
    clientParam.namedPipeClientParam.pipeName = DEFAULT_NAMED_PIPE_SERVER;
    clientParam.namedPipeClientParam.Id = GetCurrentProcessId();
    clientParam.namedPipeClientParam.connectTimeoutMs = 5000;
    clientParam.namedPipeClientParam.maxReconnectAttempts = 4;
    clientParam.namedPipeClientParam.retryBackoffMs = 2000;

    std::wcout << L"Starting " << GetOwnFileName() << std::endl;
    std::wcout << L"Args: " << argv[0] << std::endl;

    DataLabellingBlock DLB(&clientParam, argv[0]);
    if (DLB.connectToPlatform())
    {
        std::cout << " process connected to platform " << std::endl;
        DLB.startListeningToPlatform();
        std::cin.get();

    }
    DLB.disconnectFromPlatform();


    return 0;
}


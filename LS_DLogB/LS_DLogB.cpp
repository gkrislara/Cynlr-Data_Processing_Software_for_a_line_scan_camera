
#include "LS_DLogB.h"
#include <iostream>


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
    std::cout << "Hello World!\n";

    InterfaceParam_s clientParam;
    clientParam.interfaceType = IOInterfaceType::NAMED_PIPE_CLIENT;
    clientParam.namedPipeClientParam.pipeName = DEFAULT_NAMED_PIPE_SERVER;
    clientParam.namedPipeClientParam.Id = GetCurrentProcessId();

    clientParam.namedPipeClientParam.connectTimeoutMs = INFINITE;
    clientParam.namedPipeClientParam.maxReconnectAttempts = 4;
    clientParam.namedPipeClientParam.retryBackoffMs = INFINITE;

    std::wcout << L"Starting " << GetOwnFileName() << std::endl;
    std::wcout << L"Args: " << argv[0] << std::endl;

    //std::filesystem::path config = argv[0];

    //std::wstring args = config.wstring();

    DataLoggingBlock DLogB(&clientParam, argv[0] /*other args*/);
    if (DLogB.connectToPlatform())
    {
        std::cout << " process connected to platform " << std::endl;
        DLogB.startListeningToPlatform();
        std::cin.get();

    }
    DLogB.disconnectFromPlatform();
    return 0;

}
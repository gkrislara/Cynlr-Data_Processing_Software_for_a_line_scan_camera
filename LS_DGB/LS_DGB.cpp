#include <iostream>
#include "LS_DGB.h"

//TODO: Profile with DFB and DLB to check for bottlenecks and optimize for 
// 1.comm latency(ui and apps)
//2. per block latency
//3. jitter
//4.overall execution time
//5. max throughput


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

    DataGeneratorBlock DGB(&clientParam, argv[0]);
    if (DGB.connectToPlatform())
    {
        std::cout << " process connected to platform " << std::endl;
        DGB.startListeningToPlatform();
        std::cin.get();

    }
    DGB.disconnectFromPlatform();


    return 0;
}


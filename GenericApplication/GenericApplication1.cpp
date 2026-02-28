// GenericApplication.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "GenericApplication1.h"
#include <filesystem>

//{ Ai-Gen
#ifndef FORCE_UTF8
#   define FORCE_UTF8 0          // default: do not force UTF‑8
#endif

#if FORCE_UTF8
#   define USE_UTF8 1
#else
/*  If the console output code page is UTF‑8, treat the input as UTF‑8.
 *  Otherwise fall back to the system ANSI code page (GetACP()).   */
#   define USE_UTF8 (GetConsoleOutputCP() == CP_UTF8)
#endif

 /*--------------------------------------------------------------
  *  Conversion function
  *--------------------------------------------------------------*/
std::wstring argvToWString(const char* arg, bool abs)
{
    if (!arg) return L"";

    // Choose the appropriate code page
    UINT cp = USE_UTF8 ? CP_UTF8 : GetACP();   // CP_UTF8 = 65001

    // First call obtains the required buffer size (including terminating L'\0')
    int len = MultiByteToWideChar(cp, 0, arg, -1, nullptr, 0);
    if (len == 0) {
        // Conversion failed – you can inspect GetLastError() for details
        return L"";
    }

    // Allocate space and perform the conversion
    std::wstring wstr(len, L'\0');
    MultiByteToWideChar(cp, 0, arg, -1, &wstr[0], len);

    std::size_t pos = wstr.find_last_of(L".");
    wstr = wstr.substr(0, pos);


    std::wstring retstr;

    if (!abs)
    {
        std::size_t pos = wstr.find_last_of(L"\\/");

        // If no separator is found, the whole string is already a file name
        if (!(pos == std::wstring::npos))
            retstr = wstr.substr(pos + 1);
        else
            retstr = wstr;
    }
    else retstr = wstr;

    return retstr;
}

// }AI-GEN

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
    // Platform Connection Arguments
    InterfaceParam_s clientParam;
    clientParam.interfaceType = IOInterfaceType::NAMED_PIPE_CLIENT;
    clientParam.namedPipeClientParam.pipeName = DEFAULT_NAMED_PIPE_SERVER;
    clientParam.namedPipeClientParam.Id = GetCurrentProcessId();
    clientParam.namedPipeClientParam.connectTimeoutMs = 5000;
    clientParam.namedPipeClientParam.maxReconnectAttempts = 4;
    clientParam.namedPipeClientParam.retryBackoffMs = 2000;

    std::wcout << L"Starting " << GetOwnFileName() << std::endl;
    std::wcout << L"Args: " << argv[0] << std::endl;
    
    //std::filesystem::path config = argv[0];

    //std::wstring args = config.wstring();


    GA1 ga1(&clientParam,argv[0]);
    if (ga1.connectToPlatform())
    {
        std::cout << " process connected to platform " << std::endl;
        ga1.startListeningToPlatform();
        std::cin.get(); // CHECK: The application runs only if this is present. OG ProcessClient works perfectly fine as intended without this

    }
    ga1.disconnectFromPlatform();

    return 0;
}


//  ProcessClient<std::wstring> processClient(argv[0], &clientParam);
//  if (processClient.connectToPlatform(/*regParam*/)
//      // Platform/ Process Client - Register Defined Objects  
//      // Connect Stream Objects to PFM
//      // LS_DGB.connectToPlatform(/* cmdLine + Args */);
//      // LS_DLogB.connectToPlatform(/* cmdLine + Args */);
//      )
//  {   
//      //Connect Stream Objects as per Json - Commanded or Automatic - verified on PFM - Also based on availability ? 
//      // does process.startListening(); and updates the core logic parameters based on command received
      //// and interfaces core logic with the upstream and downstream interfaces 
//      // process.start()
//      // While Logic Actuation based on User Inputs else break
//  }

//  processClient.disconnectFromPlatform();
//  // On Error  / User shutdown 
//  // Disconnect Stream Object
//  // Disconnect from Platform
//  // Deinitialise Stream Object
//  // Exit StreamProcess/ Process Client 


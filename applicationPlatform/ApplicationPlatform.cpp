// applicationPlatform.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "ApplicationPlatform.h"

#define SCHEMA_PATH "E:\\LS_PROD_BASELINE\\Shared\\schema.json"

int main()
{
    
    // Output Large CYNLR Banner

	std::cout << R"(
     -----   --   --  --    --   --     -----    
    |         -- --   |  |   |   |      |   |
    |          ---    |   |  |   |      |----
    |           -     |    | |   |      | - 
     -----      -     --    --   ------ --  -
    )" << std::endl;
    
    
    
    ApplicationPlatformParams platformParam;

    platformParam.userInterfaceSchema = SCHEMA_PATH;

    ApplicationPlatformW applicationPlatform(&platformParam);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::cout << "Spawning and registering processes..." << std::endl;

    if (!applicationPlatform.setupProcesses())
    {
        std::cerr << "unable to setup process; issue with setting up server" << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // The server needs to  be up and running before clients can connect.
    std::cout << "Platform has started and clients are connected; waiting for user to issue commands" << std::endl;

    applicationPlatform.run();
    
    return 0;
   
}
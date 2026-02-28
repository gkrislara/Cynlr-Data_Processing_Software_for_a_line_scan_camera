#include "pch.h"
#include "..\Shared\Platform.h"
#include "..\Shared\json.hpp"
#include "..\Shared\UserInterfacePlatform.h"

#include <thread>
#include <future>
//#include "../Shared/IOClient.h"
//#include "../Shared/IOServer.h"
#include "PipeClient.h"
#include "PipeServer.h"

#define SCHEMA_PATH "E:\\LS_PROD_BASELINE\\Shared\\schema.json"

TEST(UI, ParseSchema) {
	bool result;
	UserInterfacePlatform UI;
	result = UI.parseSchema(SCHEMA_PATH, nullptr);

	EXPECT_TRUE(result);
}


TEST(UI, JsonParse) {
	std::ifstream jsonFile(SCHEMA_PATH);
	nlohmann::json parsed_json = nlohmann::json::parse(jsonFile);

	//std::cout<< parsed_json["services"] << std::endl;

	for (auto service : parsed_json["services"])
	{
		std::string name = service["name"];
		std::cout << name << std::endl;
	}

	std::cout << parsed_json["args"] << std::endl;
}


// IOServer and IOClient test

// Helper to run the server in a background thread

class MultiClientPipeTest : public ::testing::Test {
protected:
    PipeServer server;
    PipeClient client1{ L"\\\\.\\pipe\\TestPipe" };
    PipeClient client2{ L"\\\\.\\pipe\\TestPipe" };

    void SetUp() override {
        ASSERT_TRUE(server.Initialize(L"\\\\.\\pipe\\TestPipe", 4));
        ASSERT_TRUE(server.Start());

        // Wait for server ready (same cv as before)
        {
            std::unique_lock<std::mutex> lk(server.cv_m_);
            server.cv_.wait(lk, [&] { return server.ready_.load(); });
        }

        // Give each client a logical name that matches its intended command
        client1.SetLogicalName(L"printer");   // helper that stores name_
        client2.SetLogicalName(L"scanner");

        ASSERT_TRUE(client1.Connect(100));
        ASSERT_TRUE(client2.Connect(100));

        std::cout << " Both the clients are connected " << std::endl;
    }

    void TearDown() override {
        client1.Disconnect();
        client2.Disconnect();
        server.Stop();
    }
};


TEST_F(MultiClientPipeTest, ExclusiveCommands) {
    // 1 = PRINT, 2 = SCAN (defined as enum elsewhere)
    std::cout << " Test_F Starts: " << std::endl;
    // Each client reads its command and sets a flag
    bool printerGotCmd = false ;
    bool scannerGotCmd = false ;

    // Launch read loops in background threads
    std::thread t1([&] {
        CmdEnvelope env;
        while (!client1.ReadExact(&env, sizeof(env))) {
        }
        if (env.commandId == 1)
        {
            printerGotCmd = true;
            std::cout << " printer cmd success " << std::endl;
            EXPECT_TRUE(printerGotCmd);
        }
     });


    std::thread t2([&] {
        CmdEnvelope env;
        while (!client2.ReadExact(&env, sizeof(env))) {
        }
        if (env.commandId == 2) 
        {
            scannerGotCmd = true;
            std::cout << " Scanner cmd success " << std::endl;
            EXPECT_TRUE(scannerGotCmd);
        }
        });

    std::cout << "sending commands:"<< std::endl;

    server.EnqueueCommand(client1.GetClientId(), 1);   // print → client 1
    server.EnqueueCommand(client2.GetClientId(), 2);   // scan  → client 2
    
    t1.join();
    t2.join();
}








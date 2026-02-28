#pragma once
#include "UserInterfaceCommon.h"
#include <fstream>
#include <string>

typedef struct {
	std::vector<ProcessConfig_s> processConfig;
	std::map<std::string, std::string> args;
}PlatformConfig_s;

class UserInterfacePlatform: public UserInterfaceBase{
	/* app - paramname - input(integer)*/
	std::vector<UserInput_s> userInput;
	PlatformConfig_s platformConfig;
	nlohmann::json parsed_json;
public:
	bool parseSchema(std::string schemaPath, UserInterfaceErrorCode_e* uiErrorCode) 
	{

		if (schemaPath.empty())
		{
			*(uiErrorCode) = UserInterfaceErrorCode::ERR_USER_INTERFACE_SCHEMA_PATH_EMPTY;
			return false;
		}
		/* parse schema*/ 
		std::ifstream jsonFile(schemaPath);
		parsed_json = nlohmann::json::parse(jsonFile);

		if (parsed_json.contains("services") && parsed_json["services"].is_array())
		{
			const auto& services = parsed_json["services"];
			for (const auto& service : services)
			{
				if (service.contains("name"))
				{
					ProcessConfig_s processConfig;
					std::string name = service["name"];
					processConfig.serviceName = std::wstring(name.begin(), name.end());

					//type 
					if (service.contains("type"))
					{
						std::string type = service["type"];
						processConfig.serviceType = std::wstring(type.begin(), type.end());
					}

					//interfaces
					
					if (service.contains("interfaces") && service["interfaces"].is_array())
					{
						const auto& interfaces = service["interfaces"];
						for (const auto& interface_ : interfaces)
						{
							InterfaceConfig_s interfaceConfig;
							if (interface_.contains("type"))
							{
								std::string interfaceType = interface_["type"];
								interfaceConfig.interfaceType = std::wstring(interfaceType.begin(), interfaceType.end());
							}
							if (interface_.contains("upstream"))
							{
								std::string upstream = interface_["upstream"];
								interfaceConfig.upstream = std::wstring(upstream.begin(), upstream.end());
							}
							if (interface_.contains("downstream"))
							{
								std::string downstream = interface_["downstream"];
								interfaceConfig.downstream = std::wstring(downstream.begin(), downstream.end());
							}
							processConfig.interfaces.push_back(interfaceConfig);
						}
					}

					if (service.contains("inputs") && service["inputs"].is_object())
					{
						const auto& inputs_json = service["inputs"];
						for (nlohmann::json::const_iterator it = inputs_json.begin(); it != inputs_json.end(); ++it)
						{
								std::string key = it.key();
								std::string value = it.value();
								processConfig.inputs.emplace(std::wstring(key.begin(), key.end()), std::wstring(value.begin(), value.end()));
							// supports only string types
						}
					}

					platformConfig.processConfig.push_back(processConfig);
				}
			}
		}
		if (parsed_json.contains("args") && parsed_json["args"].is_object())
		{
			const auto& args_json = parsed_json["args"];

			// Iterate over the JSON object and populate the map
			for (nlohmann::json::const_iterator it = args_json.begin(); it != args_json.end(); ++it)
			{
				// The key() returns the key as a std::string
				// For the value, check the type to handle it appropriately
				if (it.value().is_string())
				{
					platformConfig.args[it.key()] = it.value().get<std::string>();
				}
				else
				{
					// Use dump() to convert any non-string type (like int) to a string
					platformConfig.args[it.key()] = it.value().dump();
				}
			}
		}
		return true;
	}

	void registerInput()
	{
		UserInput_s usrIn;
		for (ProcessConfig_s pcfg : platformConfig.processConfig)
		{
			usrIn.processName = pcfg.serviceName;
			for (auto it = pcfg.inputs.begin(); it != pcfg.inputs.end(); it++)
			{
				usrIn.parameterName = it->first;
				usrIn.paramShort = it->second[0];

				userInput.push_back(usrIn);

				//std::wcout << "ProcessName: " << usrIn.processName << " ParamShort: " << usrIn.paramShort << " ParameterName: " << usrIn.parameterName << std::endl;
			}
		}
		std::wcout << "No of user inputs: " << userInput.size() << std::endl;
	}

	bool inputContext(UserInput_s& input)
	{
		/* get user input */

		std::wstring app;
		wchar_t userIn;

		/* accompany this with print statement in the parent */
		std::wcout << "Enter App to input:";
		std::wcin >> app;

		std::wcout << "Enter input:";
		std::wcin >> userIn;

		/* check with registered inputs */
		for (UserInput_s in : userInput)
		{
			if (app == in.processName)
			{
				if (userIn == in.paramShort)
				{
					input = in;
					return true;
				}
			}
		}
		return false;
	}

	void getPlatformConfig(PlatformConfig_s* pfmCfg)
	{
		pfmCfg->processConfig = platformConfig.processConfig;
		pfmCfg->args = platformConfig.args;
	}

	void serialiseForCommandLine(std::wstring processName, std::wstring& outStr)
	{
		// parse the subjson denoted by the process from the parsed Json and prepare the commandline string for CreateProcessW api . ideally should be in userinterface common file
		if (parsed_json.contains("services") && parsed_json["services"].is_array())
		{
			const auto& services = parsed_json["services"];
			for (const auto& service : services)
			{
				if (service.contains("name"))
				{
					std::string name = service["name"];
					std::wstring wname = std::wstring(name.begin(), name.end());
					if (wname == processName)
					{
						// found the process
						nlohmann::json subjson = service;
						// remove name from subjson
						
						subjson.erase("name");

						// convert to string
						std::string jsonString = subjson.dump();
						// convert to wstring
						outStr = std::wstring(jsonString.begin(), jsonString.end());
					}
				}
			}
			
			//append the final args subjson to the outstr from the parsedjson
			if (parsed_json.contains("args") && parsed_json["args"].is_object())
			{
				const auto& args_json = parsed_json["args"];
				// convert to string
				std::string jsonString = args_json.dump();
				// convert to wstring
				std::wstring argsWStr = std::wstring(jsonString.begin(), jsonString.end());
				outStr += L";args:"+argsWStr;
			}
		}
	}

	void deSerialiseFromCommandLine(std::wstring processName, std::wstring cmdLine, nlohmann::json& outJson, ProcessConfig_s& pConfig, std::map<std::string, std::string> args)
	{
		// This is a support frunction for each process to parse its commandline args into json and struct. Ideally this should be in a userinterfacecommon file

		// split at ;args:
		size_t pos = cmdLine.find(L";args:");
		std::wstring processJsonStr, argsJsonStr;
		if (pos != std::wstring::npos)
		{
			processJsonStr = cmdLine.substr(0, pos);
			argsJsonStr = cmdLine.substr(pos + 6); // 6 is length of ;args:
		}
		else
		{
			processJsonStr = cmdLine;
		}
		// convert processJsonStr to json
		std::string processJsonStdStr(processJsonStr.begin(), processJsonStr.end());
		nlohmann::json processJson = nlohmann::json::parse(processJsonStdStr);
		// add process name
		
		processJson["name"] = std::string(processName.begin(), processName.end());

		// if argsJsonStr is not empty, convert to json and add to processJson
		if (!argsJsonStr.empty())
		{
			std::string argsJsonStdStr(argsJsonStr.begin(), argsJsonStr.end());
			nlohmann::json argsJson = nlohmann::json::parse(argsJsonStdStr);
			processJson["args"] = argsJson;
		}
		outJson = processJson;

		// populate pConfig and args map
		pConfig.serviceName = processName;
		if (processJson.contains("type"))
		{
			std::string type = processJson["type"];
			pConfig.serviceType = std::wstring(type.begin(), type.end());
		}
		//interfaces
		if (processJson.contains("interfaces") && processJson["interfaces"].is_array())
		{
			const auto& interfaces = processJson["interfaces"];
			for (const auto& interface_ : interfaces)
			{
				InterfaceConfig_s interfaceConfig;
				if (interface_.contains("type"))
				{
					std::string interfaceType = interface_["type"];
					interfaceConfig.interfaceType = std::wstring(interfaceType.begin(), interfaceType.end());
				}
				if (interface_.contains("upstream"))
				{
					std::string upstream = interface_["upstream"];
					interfaceConfig.upstream = std::wstring(upstream.begin(), upstream.end());
				}
				if (interface_.contains("downstream"))
				{
					std::string downstream = interface_["downstream"];
					interfaceConfig.downstream = std::wstring(downstream.begin(), downstream.end());
				}
				pConfig.interfaces.push_back(interfaceConfig);
			}
		}
		if (processJson.contains("inputs") && processJson["inputs"].is_object())
		{
			const auto& inputs_json = processJson["inputs"];
			for (nlohmann::json::const_iterator it = inputs_json.begin(); it != inputs_json.end(); ++it)
			{
				if (it.value().is_string())
				{
					std::string key = it.key();
					std::string value = it.value();
					pConfig.inputs.emplace(std::wstring(key.begin(), key.end()), std::wstring(value.begin(), value.end()));
				} // supports only string types
			}
		}

		// args
		args.clear();
		if (processJson.contains("args") && processJson["args"].is_object())
		{
			const auto& args_json = processJson["args"];
			// Iterate over the JSON object and populate the map
			for (nlohmann::json::const_iterator it = args_json.begin(); it != args_json.end(); ++it)
			{
				// The key() returns the key as a std::string
				// For the value, check the type to handle it appropriately
				if (it.value().is_string())
				{
					args[it.key()] = it.value().get<std::string>();
				}
				else
				{
					// Use dump() to convert any non-string type (like int) to a string
					args[it.key()] = it.value().dump();
				}
			}
		}
	}

};

/*

logic on process end

std::wstring json_w = argv[0];
int needed = WideCharToMultiByte(CP_UTF8, 0,
									json_w.c_str(),
									static_cast<int>(json_w.size()),
									nullptr, 0, nullptr, nullptr);
std::string json_utf8(needed, 0);
WideCharToMultiByte(CP_UTF8, 0,
					json_w.c_str(),
					static_cast<int>(json_w.size()),
					&json_utf8[0], needed, nullptr, nullptr);

nlohmann::json cfg = nlohmann::json::parse(json_utf8);

*/
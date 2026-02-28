#pragma once

#include "..\Shared\UserInterfaceCommon.h"
#include <windows.h>
#include <fstream>

class ProcessCLI {
	ProcessConfig_s pConfig;
	std::map<std::string, std::string> args;
	nlohmann::json parsed_json;

public:
	bool parseJson(char* filePath)
	{
		if (!filePath)
		{
			return false;
		}
		/* parse schema*/
		std::ifstream jsonFile(filePath);
		parsed_json = nlohmann::json::parse(jsonFile);

		return true;
	}

	bool findProcessConfigByName(std::wstring processName)
	{
		if (parsed_json.empty())
			return false;

		if (parsed_json.contains("services") && parsed_json["services"].is_array())
		{
			const auto& services = parsed_json["services"];
			for (const auto& service : services)
			{
				if (service.contains("name"))
				{
					std::string name = service["name"];
					std::wstring wname(name.begin(), name.end());
					if (wname == processName)
					{
						pConfig.serviceName = wname;

						// type
						if (service.contains("type"))
						{
							std::string type = service["type"];
							pConfig.serviceType = std::wstring(type.begin(), type.end());
						}

						// interfaces
						if (service.contains("interfaces") && service["interfaces"].is_array())
						{
							for (const auto& interface_ : service["interfaces"])
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

						// inputs
						if (service.contains("inputs") && service["inputs"].is_object())
						{
							for (auto it = service["inputs"].begin(); it != service["inputs"].end(); ++it)
							{
								std::string key = it.key();
								std::string value = it.value();
								pConfig.inputs.emplace(
									std::wstring(key.begin(), key.end()),
									std::wstring(value.begin(), value.end())
								);
							}
						}

						// Found the service, break out
						return true;
					}
				}
			}
		}
	}

	void parseArgs()
	{
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



	// get functions
	ProcessConfig_s getProcessConfig()
	{
		return pConfig;
	}

	std::map<std::string, std::string> getArgs()
	{
		return args;
	}

};


/* other process specific UI APIs Standalone or in Conjuction with UIPFM */



//#pragma once
//
// 
	
//	{
//	const auto& services = parsed_json["services"];
//	for (const auto& service : services)
//	{
//		if (service.contains("name"))
//		{
//			ProcessConfig_s processConfig;
//			std::string name = service["name"];
//			processConfig.serviceName = std::wstring(name.begin(), name.end());
//
//			//type 
//			if (service.contains("type"))
//			{
//				std::string type = service["type"];
//				processConfig.serviceType = std::wstring(type.begin(), type.end());
//			}
//
//			//interfaces
//
//			if (service.contains("interfaces") && service["interfaces"].is_array())
//			{
//				const auto& interfaces = service["interfaces"];
//				for (const auto& interface_ : interfaces)
//				{
//					InterfaceConfig_s interfaceConfig;
//					if (interface_.contains("type"))
//					{
//						std::string interfaceType = interface_["type"];
//						interfaceConfig.interfaceType = std::wstring(interfaceType.begin(), interfaceType.end());
//					}
//					if (interface_.contains("upstream"))
//					{
//						std::string upstream = interface_["upstream"];
//						interfaceConfig.upstream = std::wstring(upstream.begin(), upstream.end());
//					}
//					if (interface_.contains("downstream"))
//					{
//						std::string downstream = interface_["downstream"];
//						interfaceConfig.downstream = std::wstring(downstream.begin(), downstream.end());
//					}
//					processConfig.interfaces.push_back(interfaceConfig);
//				}
//			}
//
//			if (service.contains("inputs") && service["inputs"].is_object())
//			{
//				const auto& inputs_json = service["inputs"];
//				for (nlohmann::json::const_iterator it = inputs_json.begin(); it != inputs_json.end(); ++it)
//				{
//					if (it.value().is_string())
//					{
//						std::string key = it.key();
//						std::string value = it.value();
//						processConfig.inputs.emplace(std::wstring(key.begin(), key.end()), std::wstring(value.begin(), value.end()));
//					} // supports only string types
//				}
//			}
//
//			platformConfig.processConfig.push_back(processConfig);
//		}
//	}
//	}
//if (parsed_json.contains("args") && parsed_json["args"].is_object())
//{
//	const auto& args_json = parsed_json["args"];
//
//	// Iterate over the JSON object and populate the map
//	for (nlohmann::json::const_iterator it = args_json.begin(); it != args_json.end(); ++it)
//	{
//		// The key() returns the key as a std::string
//		// For the value, check the type to handle it appropriately
//		if (it.value().is_string())
//		{
//			platformConfig.args[it.key()] = it.value().get<std::string>();
//		}
//		else
//		{
//			// Use dump() to convert any non-string type (like int) to a string
//			platformConfig.args[it.key()] = it.value().dump();
//		}
//	}
//}
//return true;
//}
//	}
//
//
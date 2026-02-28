#pragma once
#include <map>
#include <string>
#include "..\Shared\json.hpp"

typedef struct {
	std::wstring processName;
	std::wstring parameterName;
	wchar_t paramShort; //can be other datatypes as well
}UserInput_s;


/* user input (name,value)*/
class UserInterfaceBase {
public:
	virtual void setUserInputField() {}
	virtual void getUserInputField() {}
	virtual void getProcessName() {}
};


/* TODO: Move this to separate Unified Error Code header*/
typedef enum class UserInterfaceErrorCode {
	ERR_USER_INTERFACE_SCHEMA_PATH_EMPTY,
	NUM_USER_INTERFACE_ERR
}UserInterfaceErrorCode_e;

typedef struct {
	std::wstring interfaceType;
	std::wstring upstream;
	std::wstring downstream;
}InterfaceConfig_s;

typedef struct {
	std::wstring serviceName;
	std::wstring serviceType;
	std::vector<InterfaceConfig_s> interfaces;
	std::map<std::wstring, std::wstring> inputs;
}ProcessConfig_s;





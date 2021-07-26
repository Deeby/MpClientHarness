#include <iostream>

#include <cstring>
#include <Windows.h>
#include <string>
#include <bitset>

#include "MpClient.h"

//struct CallbackInfo {
//	void exec(DWORD) {
//		std::cout << "Hello\n";
//	}
//};
//using MPCALLBACK_INFO = CallbackInfo;

using MPCALLBACK_INFO = struct _GUID; // this is taken (guessed) from MpClient.dll disassembly
using PMPCALLBACK_INFO = MPCALLBACK_INFO*;

const wchar_t* winDefLibPath = L"C:\\Program Files\\Windows Defender\\MpClient.dll";

bool FileExists(const wchar_t* fileName)
{
	WIN32_FIND_DATA ffd;
	HANDLE handle = FindFirstFileW(fileName, &ffd);
	bool found = handle != INVALID_HANDLE_VALUE;

	if (found) FindClose(handle);

	return found;
}

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string GetLastErrorAsString()
{
	//Get the error message, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0)
		return std::string{}; //No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, // source
		errorMessageID,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // language settings
		(LPSTR)&messageBuffer,
		0,
		NULL // misc args
	);

	std::string message(messageBuffer, size);

	//Free the buffer.
	LocalFree(messageBuffer);

	return message;
}


using MPHANDLE = HANDLE;
using PMPHANDLE = HANDLE*;

using MpManagerOpen_t = HRESULT WINAPI(DWORD, MPHANDLE*);
MpManagerOpen_t* MpManagerOpen = nullptr;

using MpHandleClose_t = HRESULT WINAPI(MPHANDLE);
MpHandleClose_t* MpHandleClose = nullptr;

using MpErrorMessageFormat_t = HRESULT WINAPI(MPHANDLE, HRESULT, LPWSTR*);
MpErrorMessageFormat_t* MpErrorMessageFormat = nullptr;

using MpFreeMemory_t = void WINAPI(PVOID);
MpFreeMemory_t* MpFreeMemory;

using MpScanStart_t = HRESULT WINAPI(
	MPHANDLE,
	MPSCAN_TYPE,
	DWORD,
	PMPSCAN_RESOURCES,
	PMPCALLBACK_INFO,
	PMPHANDLE
);
MpScanStart_t* MpScanStart = nullptr;

void WINAPI handler(DWORD, DWORD) {
	std::cout << "Hello\n";
}

void* loadFunc(HMODULE moduleHandle, const char* funcName)
{
	auto fptr = GetProcAddress(moduleHandle, funcName);

	if (!fptr) {
		std::cerr << "Error: GetProcAddress:" << funcName << "\n";
		std::exit(EXIT_FAILURE);
	}
	std::clog << "GetProcAddress: " << funcName << ": OK\n";
	return fptr;
}

void MpTestResult(HRESULT hres, const char* funcName, MPHANDLE managerHandle)
{
	if (FAILED(hres)) {
		std::cerr << "Error: " << funcName << ": ";
		wchar_t* msg = nullptr;

		MpErrorMessageFormat(managerHandle, hres, &msg);

		if (msg) {
			int bufferSize = WideCharToMultiByte(CP_UTF8, 0, msg, -1, NULL, 0, NULL, NULL);
			char* m = new char[bufferSize];
			WideCharToMultiByte(CP_UTF8, 0, msg, -1, m, bufferSize, NULL, NULL);
			wprintf(L"%S\n", m);
			//std::wcerr << m << '\n';
			MpFreeMemory(msg);
		}
		else {
			std::string msg = GetLastErrorAsString();
			if (msg.empty())
				std::cerr << "Unknown error: " << std::hex << hres << '\n';
			else
				std::cerr << "Win32 error: " << msg << '\n';
		}
		std::exit(EXIT_FAILURE);
	}
	else
		std::clog << funcName << ": OK\n";
}

int main(int argc, wchar_t* cmd[])
{
	// settings
	SetConsoleOutputCP(CP_UTF8);
	// end settings

	if (argc < 2)
	{
		std::cout << "USAGE: " << "mpharness.exe" << " [file or directory]\n";
		return 1;
	}

	HMODULE winDefLibHandle = LoadLibrary(winDefLibPath);

	if (winDefLibHandle == NULL) {
		std::cerr << "Error: LoadLibrary: " << winDefLibPath << '\n';
		return EXIT_FAILURE;
	}
	std::clog << "LoadLibrary: OK\n";

	MpManagerOpen = (MpManagerOpen_t*)loadFunc(winDefLibHandle, "MpManagerOpen");
	MpHandleClose = (MpHandleClose_t*)loadFunc(winDefLibHandle, "MpHandleClose");
	MpErrorMessageFormat = (MpErrorMessageFormat_t*)loadFunc(winDefLibHandle, "MpErrorMessageFormat");
	MpFreeMemory = (MpFreeMemory_t*)loadFunc(winDefLibHandle, "MpFreeMemory");
	MpScanStart = (MpScanStart_t*)loadFunc(winDefLibHandle, "MpScanStart");


	HRESULT hres = NULL;
	MPHANDLE managerHandle = NULL;

	hres = MpManagerOpen(0, &managerHandle);

	if (FAILED(hres)) {
		std::cerr << "Error: MpManagerOpen: " << std::hex << hres << '\n';
		return EXIT_FAILURE;
	}
	else std::clog << "MpManagerOpen: OK\n";

	MPRESOURCE_INFO resInfo;
	resInfo.Class = MP_RESOURCE_CLASS::MP_RESOURCE_CLASS_SAMPLE_FILE;
	resInfo.Path = cmd[1];

	MPSCAN_RESOURCES res;
	res.dwResourceCount = 1;
	res.pResourceList = &resInfo;

	MPHANDLE scanHandle = NULL;

	MPCALLBACK_INFO clbkInfo; memset((void*)&clbkInfo, 0, sizeof(MPCALLBACK_INFO));
	clbkInfo.Data1 = (DWORD)&handler;

	hres = MpScanStart(
		managerHandle,
		MPSCAN_TYPE::MPSCAN_TYPE_RESOURCE,
		0,
		&res,
		&clbkInfo,
		&scanHandle
	);
	MpTestResult(hres, "MpScanStart", managerHandle);

	hres = MpHandleClose(managerHandle);
	MpTestResult(hres, "MpHandleClose", managerHandle);

	if (!FreeLibrary(winDefLibHandle)) std::cerr << "Error: FreeLibrary\n";
	else std::clog << "FreeLibrary: OK\n";
	return 0;
}
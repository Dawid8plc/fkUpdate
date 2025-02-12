#include "pch.h"
#define WIN32_LEAN_AND_MEAN
#include <winInet.h>
#include <string>
#include <windows.h>	
#include <iostream>
#include <sstream>
#include <vector>
#include <iterator>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib,"Version.lib")

HMODULE self_module;
HANDLE main_thread;

bool is_version(const std::string& str) {
	std::size_t found = str.find_first_not_of("0123456789. ");
	if (found != std::string::npos && str[found] != 10 && str[found] != 13) //10 = /n, 13= /r
	{
		return false;
	}
	else {
		return true;
	}
}
class Version
{
	// An internal utility structure just used to make the std::copy in the constructor easy to write.
	struct VersionDigit
	{
		int value;
		operator int() const { return value; }
	};
	friend std::istream& operator>>(std::istream& str, Version::VersionDigit& digit);
public:
	Version(std::string const& versionStr)
	{
		// To Make processing easier in VersionDigit prepend a '.'
		std::stringstream   versionStream(std::string(".") + versionStr);

		// Copy all parts of the version number into the version Info vector.
		std::copy(std::istream_iterator<VersionDigit>(versionStream),
			std::istream_iterator<VersionDigit>(),
			std::back_inserter(versionInfo)
		);
	}

	// Test if two version numbers are the same. 
	bool operator<(Version const& rhs) const
	{
		return std::lexicographical_compare(versionInfo.begin(), versionInfo.end(), rhs.versionInfo.begin(), rhs.versionInfo.end());
	}

private:
	std::vector<int>    versionInfo;
};

// Read a single digit from the version. 
std::istream& operator>>(std::istream& str, Version::VersionDigit& digit)
{
	str.get();
	str >> digit.value;
	return str;
}
std::string getVersionCurrent() {
	std::string moduleName = "fkUpdate.dll";
	DWORD  zero = 0;
	int verInfoLen = 0;
	BYTE* verInfo = NULL;
	VS_FIXEDFILEINFO* fileInfo = NULL;
	UINT len = 0;

	/* Get the size of FileVersionInfo structure */
	verInfoLen = GetFileVersionInfoSize(moduleName.c_str(), &zero);
	if (verInfoLen == 0) {
		printf("GetFileVersionInfoSize() Failed!");
		return "0";
	}

	/* Get FileVersionInfo structure */
	verInfo = new BYTE[verInfoLen];
	if (!GetFileVersionInfo(moduleName.c_str(), zero, verInfoLen, verInfo)) {
		printf("GetFileVersionInfo Failed!");
		return "0";
	}

	/* Query for File version details. */
	if (!VerQueryValue(verInfo, "\\", (LPVOID*)&fileInfo, &len)) {
		printf("VerQueryValue Failed!");
		return "0";
	}

	if (len > 0)
	{
		char buf[16];
		int len = sprintf_s(buf, "%d.%d.%d.%d",
			HIWORD(fileInfo->dwFileVersionMS),
			LOWORD(fileInfo->dwFileVersionMS),
			HIWORD(fileInfo->dwFileVersionLS),
			LOWORD(fileInfo->dwFileVersionLS)
		);

		return std::string(buf, len);
	}
	else
	{
		return std::string("0");
	}
}

bool userRegCheck(const std::string& regKey) {
	bool badKey = false;
	HKEY hKey;
	DWORD val;
	DWORD dataSize = sizeof(val);
	std::string subKey = "Software\\Team17SoftwareLTD\\Worms2";
	if (RegOpenKeyEx(HKEY_CURRENT_USER, subKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		DWORD type;
		if (RegQueryValueEx(hKey, regKey.c_str(), NULL, &type, (LPBYTE)&val, &dataSize) == ERROR_SUCCESS)
		{
			if (val != 46 || type != REG_SZ) { //Character code 46 = "."
				badKey = true;
			}
		}
		else {
			badKey = true;
		}
		RegCloseKey(hKey);
	}
	else {
		badKey = true;
	}
	if (badKey) {
		DWORD dwDisposition;
		if (RegCreateKeyEx(HKEY_CURRENT_USER,
			subKey.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, &dwDisposition) == ERROR_SUCCESS) {
			LPCTSTR value = TEXT(regKey.c_str());
			LONG setRes = RegSetValueEx(hKey, regKey.c_str(), 0, REG_SZ, (LPBYTE)".", 1);
			LONG closeOut = RegCloseKey(hKey);
			return true;
		}
		else{
			return false;
		}
	}
	else {
		return false;
	}
}

int updateCheck() {
	bool restartRequired = false;
	if (userRegCheck("W2PATH")){
		restartRequired = true;
	}
	if (userRegCheck("CD")){
		restartRequired = true;
	}
	if (restartRequired){
		MessageBox(NULL, "Settings have been updated and the game will now close. Please start the game again.", "Worms 2 Plus", MB_OK);
		exit(0);
	}
	
	//Set error status
	int updateError = 0;
	//Attempt to open an internet connection
	HINTERNET hInternet = InternetOpen(0, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
	if (hInternet != NULL) {
		//Update URL
		const char* versionURL = "https://raw.githubusercontent.com/Carlmundo/Worms2-Website/main/version.txt";
		const char* openWebsite = "explorer \"https://github.com/Carlmundo/W2-Plus/releases/latest\"";

		//Attempt to open URL
		HINTERNET hFile = InternetOpenUrl(hInternet, versionURL, NULL, 0, INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, NULL);
		//Set var in advance for MessageBox
		int result;
		if (hFile != NULL) {
			//Constant to hold the read size(1k)
			const int downloadBufferSize = 1024;
			//Store all of the downloaded data
			std::string downloadContents = "";

			char* downloadBuffer = new char[downloadBufferSize];
			DWORD availableSize = 0;
			DWORD bytesRead = 0;
			do {
				//Get size of data
				InternetQueryDataAvailable(hFile, &availableSize, 0, 0);
				//Read only as much data as is available or the downloadBufferSize
				if (availableSize > downloadBufferSize)
					availableSize = downloadBufferSize;
				//Read the current bytes from the online file
				InternetReadFile(hFile, downloadBuffer, availableSize, &bytesRead);
				//Append the buffer to the download contents
				downloadContents.append(downloadBuffer, availableSize);
			} while (bytesRead != 0);

			//Close the handles.
			InternetCloseHandle(hFile);
			InternetCloseHandle(hInternet);

			//Download contents must be less than 16 characters and only contain permitted characters (eg: 1.2.3.4)
			if (downloadContents.size() <= 16 && is_version(downloadContents)) {
				std::string versionCurrent = getVersionCurrent();
				if (versionCurrent != "0") {
					Version vLocal(versionCurrent);
					Version vRemote(downloadContents);
					if (vLocal < vRemote){
						std::string strUpdate = "A new update is available: " + downloadContents + "\n" + "Do you want to download it now?";
						result = MessageBox(NULL, strUpdate.c_str(), "Update Available", MB_YESNO);
						if (result == IDYES)
						{
							WinExec(openWebsite, SW_HIDE);
							return 0;
						}
					}
				}
				else {
					updateError = 1;
				}
			}
			else {
				updateError = 1;
			}
		}
		else {
			updateError = 2; //Likely no working internet connection or OS with outdated ciphers
			InternetCloseHandle(hInternet);
		}
		if (updateError == 1) {
			LPCTSTR strError = "Unable to check for updates. Do you want to check the website yourself?";
			result = MessageBox(NULL, strError, "Error", MB_YESNO);
			if (result == IDYES)
			{
				WinExec(openWebsite, SW_HIDE);
			}
		}
	}
}

BOOL WINAPI DllMain(HMODULE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			self_module = hinstDLL;
			main_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)updateCheck, NULL, 0, NULL);
			if (main_thread == NULL) {
				return false;
			}
		}
		break;

		case DLL_PROCESS_DETACH:
			break;
		}
	return TRUE;
}
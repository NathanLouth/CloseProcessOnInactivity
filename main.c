#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <stdio.h>

ULONGLONG GetIdleTimeSeconds(void)
{
    LASTINPUTINFO lii;
    lii.cbSize = sizeof(LASTINPUTINFO);

    if (!GetLastInputInfo(&lii))
        return (ULONGLONG)-1;

    ULONGLONG tickCount = GetTickCount64();
    ULONGLONG lastInputTick = (ULONGLONG)lii.dwTime;

    if (tickCount < lastInputTick)
        return 0;

    return (tickCount - lastInputTick) / 1000;
}

BOOL KillProcessByName(const wchar_t* processName)
{
    HANDLE hSnap;
    PROCESSENTRY32 pe32;
    BOOL killedAny = FALSE;

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return FALSE;

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &pe32))
    {
        do
        {
            if (_wcsicmp(pe32.szExeFile, processName) == 0)
            {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess)
                {
                    TerminateProcess(hProcess, 1);
                    CloseHandle(hProcess);
                    killedAny = TRUE;
                }
            }
        } while (Process32Next(hSnap, &pe32));
    }

    CloseHandle(hSnap);
    return killedAny;
}

DWORD GetDWORDFromRegistry(HKEY hRoot, const wchar_t* subKey, const wchar_t* valueName)
{
    HKEY hKey;
    DWORD data = 0;
    DWORD dataSize = sizeof(DWORD);

    if (RegOpenKeyExW(hRoot, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
    {
        return 0;
    }

    DWORD type = 0;
    if (RegQueryValueExW(hKey, valueName, NULL, &type, (LPBYTE)&data, &dataSize) != ERROR_SUCCESS || type != REG_DWORD)
    {
        RegCloseKey(hKey);
        return 0;
    }

    RegCloseKey(hKey);
    return data;
}

BOOL GetSZFromRegistry(HKEY hRootKey, const wchar_t* subKey, const wchar_t* valueName, wchar_t* outBuffer, DWORD bufferSizeInBytes)
{
    HKEY hKey;
    DWORD type = 0;
    LONG result;
    DWORD requiredSizeInBytes = 0;

    result = RegOpenKeyExW(hRootKey, subKey, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return FALSE;
    }

    result = RegQueryValueExW(hKey, valueName, NULL, &type, NULL, &requiredSizeInBytes);
    if (result != ERROR_SUCCESS || type != REG_SZ) {
        RegCloseKey(hKey);
        return FALSE;
    }

    if (requiredSizeInBytes > bufferSizeInBytes) {
        RegCloseKey(hKey);
        return FALSE;
    }

    result = RegQueryValueExW(hKey, valueName, NULL, NULL, (LPBYTE)outBuffer, &requiredSizeInBytes);
    RegCloseKey(hKey);

    if (result != ERROR_SUCCESS) {
        return FALSE;
    }

    return TRUE;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    const wchar_t* softwareKeyLocation = L"SOFTWARE\\CloseOnInactivity";

    DWORD autoRun = GetDWORDFromRegistry(HKEY_LOCAL_MACHINE, softwareKeyLocation, L"Run");
    if (autoRun != 1) return 1;

    DWORD idleLimit = GetDWORDFromRegistry(HKEY_LOCAL_MACHINE, softwareKeyLocation, L"IdleTime");
    if (idleLimit == 0) return 2;

    wchar_t targetProcess[1024];
    if (!GetSZFromRegistry(HKEY_LOCAL_MACHINE, softwareKeyLocation, L"TargetProcess", targetProcess, sizeof(targetProcess))) {
        return 3;
    }
    
    while (1)
    {
        ULONGLONG idleTime = GetIdleTimeSeconds();

        if (idleTime >= (ULONGLONG)idleLimit)
        {
            KillProcessByName(targetProcess);
        }

        Sleep(5000);
    }

    return 0;
}

//  Additional Dependencies:
//  advapi32.lib;wtsapi32.lib;userenv.lib;shlwapi.lib

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <tlhelp32.h>
#include <strsafe.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "shlwapi.lib")

#define SERVICE_NAME     L"CloseProcessOnInactivityService"
#define TARGET_EXE_NAME  L"CloseProcessOnInactivity.exe"
#define TARGET_EXE_PATH  L"C:\\Program Files\\CloseProcessOnInactivity\\CloseProcessOnInactivity.exe"
#define MONITOR_INTERVAL 10000

static SERVICE_STATUS        g_ServiceStatus;
static SERVICE_STATUS_HANDLE g_hStatus = NULL;
static HANDLE                g_hStopEvent = NULL;

static void WINAPI ServiceMain(DWORD argc, LPWSTR* argv);
static void WINAPI ServiceCtrlHandler(DWORD Opcode);
static void RunMonitorLoop(void);
static BOOL IsProcessRunningInSession(DWORD sessionId, LPCWSTR processName);
static BOOL LaunchProcessInSession(DWORD sessionId, LPCWSTR appPath);
static void UpdateServiceStatus(DWORD state, DWORD win32ExitCode);

int wmain(int argc, wchar_t* argv[]) {
    const SERVICE_TABLE_ENTRYW ServiceTable[] = {
        { SERVICE_NAME, ServiceMain },
        { NULL, NULL }
    };

    StartServiceCtrlDispatcherW(ServiceTable);
    return 0;
}

static void UpdateServiceStatus(DWORD state, DWORD win32ExitCode) {
    g_ServiceStatus.dwCurrentState = state;
    g_ServiceStatus.dwWin32ExitCode = win32ExitCode;
    g_ServiceStatus.dwWaitHint = 1000;
    g_ServiceStatus.dwCheckPoint = (state == SERVICE_RUNNING || state == SERVICE_STOPPED) ? 0 : 1;
    SetServiceStatus(g_hStatus, &g_ServiceStatus);
}

static void WINAPI ServiceMain(DWORD argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    g_hStatus = RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_hStatus) return;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwWaitHint = 0;

    UpdateServiceStatus(SERVICE_START_PENDING, NO_ERROR);

    g_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_hStopEvent) {
        UpdateServiceStatus(SERVICE_STOPPED, GetLastError());
        return;
    }

    UpdateServiceStatus(SERVICE_RUNNING, NO_ERROR);
    RunMonitorLoop();

    CloseHandle(g_hStopEvent);
    UpdateServiceStatus(SERVICE_STOPPED, NO_ERROR);
}

static void WINAPI ServiceCtrlHandler(DWORD Opcode) {
    if (Opcode == SERVICE_CONTROL_STOP || Opcode == SERVICE_CONTROL_SHUTDOWN) {
        if (g_hStopEvent) SetEvent(g_hStopEvent);
    }
}

static void RunMonitorLoop(void) {
    while (1) {
        DWORD waitResult = WaitForSingleObject(g_hStopEvent, MONITOR_INTERVAL);
        if (waitResult == WAIT_OBJECT_0) break;

        DWORD sessionId = WTSGetActiveConsoleSessionId();
        if (sessionId == 0xFFFFFFFF) continue;

        if (!IsProcessRunningInSession(sessionId, TARGET_EXE_NAME)) {
            LaunchProcessInSession(sessionId, TARGET_EXE_PATH);
        }
    }
}

static BOOL IsProcessRunningInSession(DWORD sessionId, LPCWSTR processName) {
    PWTS_PROCESS_INFO pInfo = NULL;
    DWORD count = 0;
    BOOL found = FALSE;

    if (WTSEnumerateProcessesW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pInfo, &count)) {
        for (DWORD i = 0; i < count; i++) {
            if (pInfo[i].SessionId == sessionId &&
                _wcsicmp(pInfo[i].pProcessName, processName) == 0) {
                found = TRUE;
                break;
            }
        }
        WTSFreeMemory(pInfo);
    }

    return found;
}

static BOOL LaunchProcessInSession(DWORD sessionId, LPCWSTR appPath) {
    HANDLE hToken = NULL, hDupToken = NULL;
    LPVOID envBlock = NULL;
    BOOL result = FALSE;

    if (WTSQueryUserToken(sessionId, &hToken)) {
        if (DuplicateTokenEx(hToken, TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID, NULL, SecurityIdentification, TokenPrimary, &hDupToken)) {
            if (CreateEnvironmentBlock(&envBlock, hDupToken, FALSE)) {

                STARTUPINFOW si = { sizeof(si) };
                si.lpDesktop = L"winsta0\\default";

                PROCESS_INFORMATION pi = { 0 };
                wchar_t cmdLine[MAX_PATH];
                swprintf(cmdLine, MAX_PATH, L"\"%s\"", appPath);

                if (CreateProcessAsUserW(
                    hDupToken,
                    appPath,
                    cmdLine,
                    NULL, NULL, FALSE,
                    CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT,
                    envBlock, NULL,
                    &si, &pi)) {

                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    result = TRUE;
                }

                DestroyEnvironmentBlock(envBlock);
            }
            CloseHandle(hDupToken);
        }
        CloseHandle(hToken);
    }

    return result;
}

#include <Windows.h>
#include <iostream>
#include <string>
#include "sendmail.h"

#define SERVICE_NAME L"MailSender"

SERVICE_STATUS_HANDLE g_ServiceStatusHandle = NULL;
HANDLE g_ServiceStopEvent = NULL;

void WINAPI ServiceCtrlHandler(DWORD ctrl) {
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        if (g_ServiceStatusHandle) {
            SERVICE_STATUS status = { 0 };
            status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
            status.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(g_ServiceStatusHandle, &status);
        }
        SetEvent(g_ServiceStopEvent);
        break;
    default:
        break;
    }
}

void WINAPI ServiceMain(DWORD argc, LPWSTR* argv) {
    SERVICE_STATUS status = { 0 };
    g_ServiceStatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    status.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(g_ServiceStatusHandle, &status);

    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_ServiceStatusHandle, &status);

    sendmail();
    sendMailTask();

    status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_ServiceStatusHandle, &status);
}

bool isServiceInstalled() {
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (hSCM) {
        SC_HANDLE hService = OpenService(hSCM, SERVICE_NAME, SERVICE_QUERY_STATUS);
        if (hService) {
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCM);
            return true;
        }
        CloseServiceHandle(hSCM);
    }
    return false;
}

void InstallService(const wchar_t* exePath) {
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (hSCM) {
        SC_HANDLE hService = CreateService(
            hSCM, SERVICE_NAME, SERVICE_NAME, SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
            exePath, NULL, NULL, NULL, NULL, NULL
        );
        if (hService) {
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
}

void StartServiceFunc() {
    SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (hSCM) {
        SC_HANDLE hService = OpenService(hSCM, SERVICE_NAME, SERVICE_START);
        if (hService) {
            StartService(hService, 0, NULL);
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
}



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    
    if (argc >= 2 && wcscmp(argv[1], L"/sendmail") == 0) {
        sendmail();
        return 0;
    }

    
    if (argc >= 2 && wcscmp(argv[1], L"/service") == 0) {
        SERVICE_TABLE_ENTRY ServiceTable[] = {
            {(LPWSTR)SERVICE_NAME, ServiceMain},
            {NULL, NULL}
        };
        StartServiceCtrlDispatcher(ServiceTable);
        return 0;
    }

    
    if (!isServiceInstalled()) {
        std::wstring exePath = getExePath();
        InstallService(exePath.c_str());
        StartServiceFunc();
    }
    else {
        SERVICE_TABLE_ENTRY ServiceTable[] = {
            {(LPWSTR)SERVICE_NAME, ServiceMain},
            {NULL, NULL}
        };
        StartServiceCtrlDispatcher(ServiceTable);
    }
    return 0;
}

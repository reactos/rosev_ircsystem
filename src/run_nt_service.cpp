/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

#include <messages.h>
#include <version.h>

static SERVICE_STATUS ServiceStatus;
static SERVICE_STATUS_HANDLE ServiceStatusHandle;

static void
_AddEventSource()
{
    /* Prepare the values we need to set */
    char EventMessageFile[MAX_PATH];
    if(!GetModuleFileNameA(NULL, EventMessageFile, MAX_PATH))
        BOOST_THROW_EXCEPTION(Error("Failed to add the event source!") << boost::errinfo_api_function("GetModuleFileNameA") << Win32Error_Info(GetLastError()));

    DWORD TypesSupported = EVENTLOG_ERROR_TYPE;

    /* Create the event source registry key */
    HKEY RegistryKeyHandle;
    LONG Result = RegCreateKeyExA(
        HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\" PRODUCT_NAME,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,
        NULL,
        &RegistryKeyHandle,
        NULL
    );
    if(Result != ERROR_SUCCESS)
        BOOST_THROW_EXCEPTION(Error("Failed to add the event source!") << boost::errinfo_api_function("RegOpenKeyExA") << Win32Error_Info(Result));

    /* Set the required values */
    Result = RegSetValueExA(RegistryKeyHandle, "EventMessageFile", 0, REG_SZ, reinterpret_cast<const BYTE*>(EventMessageFile), sizeof(EventMessageFile));
    if(Result != ERROR_SUCCESS)
    {
        RegCloseKey(RegistryKeyHandle);
        BOOST_THROW_EXCEPTION(Error("Failed to add the event source!") << boost::errinfo_api_function("RegSetValueExA") << Win32Error_Info(Result));
    }

    Result = RegSetValueExA(RegistryKeyHandle, "TypesSupported", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&TypesSupported), sizeof(TypesSupported));
    RegCloseKey(RegistryKeyHandle);

    if(Result != ERROR_SUCCESS)
        BOOST_THROW_EXCEPTION(Error("Failed to add the event source!") << boost::errinfo_api_function("RegSetValueExA") << Win32Error_Info(Result));
}

static void
_ReportServiceStatus(DWORD NewState)
{
    ServiceStatus.dwCurrentState = NewState;
    SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
}

static DWORD WINAPI
_ServiceControlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
    switch(dwControl)
    {
        case SERVICE_CONTROL_INTERROGATE:
            /* "The handler should simply return NO_ERROR; the SCM is aware of the current state of the service."
               (http://msdn.microsoft.com/en-us/library/ms683241%28VS.85%29.aspx) */
            return NO_ERROR;

        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_STOP:
            _ReportServiceStatus(SERVICE_STOP_PENDING);
            ShutdownServer();

            return NO_ERROR;
    }

    return ERROR_CALL_NOT_IMPLEMENTED;
}

static VOID WINAPI
_ServiceMainA(DWORD dwArgc, LPSTR* lpszArgv)
{
    ServiceStatusHandle = RegisterServiceCtrlHandlerExA(APPLICATION_NAME, _ServiceControlHandlerEx, NULL);

    /* Report initial SERVICE_START_PENDING status */
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    ServiceStatus.dwServiceSpecificExitCode = 0;
    ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ServiceStatus.dwWaitHint = 4000;
    ServiceStatus.dwWin32ExitCode = NO_ERROR;
    _ReportServiceStatus(SERVICE_START_PENDING);

    /* Get us an event log handle for logging errors */
    HANDLE EventLogHandle = RegisterEventSourceA(NULL, PRODUCT_NAME);

    /* Start the server task */
    try
    {
        InitializeServer();
        _ReportServiceStatus(SERVICE_RUNNING);
        RunServerEventLoop();
    }
    catch(std::exception& Exception)
    {
        /* Log all exceptions with diagnostic information to the event log */
        std::string ExceptionMessage(boost::diagnostic_information(Exception));
        const char* InsertStrings[] = {ExceptionMessage.c_str()};
        ReportEventA(EventLogHandle, EVENTLOG_ERROR_TYPE, 0, GENERAL_EXCEPTION, NULL, 1, NULL, InsertStrings, NULL);
    }

    /* We're done */
    _ReportServiceStatus(SERVICE_STOPPED);
    DeregisterEventSource(EventLogHandle);
}

void
InstallNTService(CConfiguration& Configuration)
{
    /* Get the path to our executable file and append the corresponding arguments to it */
    char ExecutableFile[MAX_PATH];
    if(!GetModuleFileNameA(NULL, ExecutableFile, MAX_PATH))
        BOOST_THROW_EXCEPTION(Error("Failed to install the service!") << boost::errinfo_api_function("GetModuleFileNameA") << Win32Error_Info(GetLastError()));

    std::string FullPathName(ExecutableFile);
    FullPathName.append(" --service \"");
    FullPathName.append(Configuration.GetConfigPath());
    FullPathName.append("\"");

    /* Get access to the service manager */
    SC_HANDLE SCManagerHandle = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if(!SCManagerHandle)
        BOOST_THROW_EXCEPTION(Error("Failed to install the service!") << boost::errinfo_api_function("OpenSCManagerA") << Win32Error_Info(GetLastError()));

    /* Add the service */
    SC_HANDLE ServiceHandle = CreateServiceA(
        SCManagerHandle,
        APPLICATION_NAME,
        PRODUCT_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        FullPathName.c_str(),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );
    CloseServiceHandle(SCManagerHandle);

    if(!ServiceHandle)
        BOOST_THROW_EXCEPTION(Error("Failed to install the service!") << boost::errinfo_api_function("CreateServiceA") << Win32Error_Info(GetLastError()));

    CloseServiceHandle(ServiceHandle);

    /* Set the service description using the registry */
    HKEY RegistryKeyHandle;
    LONG Result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\" APPLICATION_NAME, 0, KEY_SET_VALUE, &RegistryKeyHandle);
    if(Result != ERROR_SUCCESS)
        BOOST_THROW_EXCEPTION(Error("Could not set the service description!") << boost::errinfo_api_function("RegOpenKeyExA") << Win32Error_Info(Result));

    Result = RegSetValueExA(RegistryKeyHandle, "Description", 0, REG_SZ, reinterpret_cast<const BYTE*>(VERSION_ID), sizeof(VERSION_ID));
    RegCloseKey(RegistryKeyHandle);

    if(Result != ERROR_SUCCESS)
        BOOST_THROW_EXCEPTION(Error("Could not set the service description!") << boost::errinfo_api_function("RegSetValueExA") << Win32Error_Info(Result));

    /* Also add the event source. */
    _AddEventSource();

    std::cout << "The service has been installed successfully!\n";
}

void
RunAsNTService()
{
    char ServiceName[] = APPLICATION_NAME;
    SERVICE_TABLE_ENTRYA ServiceTable[] =
    {
        {ServiceName, _ServiceMainA},
        {NULL, NULL}
    };

    if(!StartServiceCtrlDispatcherA(ServiceTable))
        BOOST_THROW_EXCEPTION(Error("Failed to start the service control dispatcher!") << Win32Error_Info(GetLastError()));
}

void
UninstallNTService()
{
    /* Get access to the service manager */
    SC_HANDLE SCManagerHandle = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if(!SCManagerHandle)
        BOOST_THROW_EXCEPTION(Error("Failed to uninstall the service!") << boost::errinfo_api_function("OpenSCManagerA") << Win32Error_Info(GetLastError()));

    /* Find the service */
    SC_HANDLE ServiceHandle = OpenServiceA(SCManagerHandle, APPLICATION_NAME, SERVICE_ALL_ACCESS);
    if(!ServiceHandle)
    {
        CloseServiceHandle(SCManagerHandle);
        BOOST_THROW_EXCEPTION(Error("Failed to uninstall the service!") << boost::errinfo_api_function("OpenServiceA") << Win32Error_Info(GetLastError()));
    }

    /* Delete it */
    if(!DeleteService(ServiceHandle))
    {
        CloseServiceHandle(ServiceHandle);
        CloseServiceHandle(SCManagerHandle);
        BOOST_THROW_EXCEPTION(Error("Failed to uninstall the service!") << boost::errinfo_api_function("DeleteService") << Win32Error_Info(GetLastError()));
    }

    /* Also delete the event source. */
    LONG Result = RegDeleteKeyA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\" PRODUCT_NAME);
    if(Result != ERROR_SUCCESS)
        BOOST_THROW_EXCEPTION(Error("Failed to delete the event source!") << boost::errinfo_api_function("RegDeleteKeyA") << Win32Error_Info(Result));

    std::cout << "The service has been uninstalled successfully!\n";

    CloseServiceHandle(ServiceHandle);
    CloseServiceHandle(SCManagerHandle);
}

/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010-2011 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>
#include <version.h>

#ifndef WIN32
static bool DeletePidFile = false;
#endif
static CConfiguration Configuration;
static boost::asio::io_service IOService;
static CIRCServer IRCServer(IOService, Configuration);
static CNickServ NickServ(IRCServer);
static CLogBot LogBot(IRCServer);
static CVoteBotManager VoteBotManager(IRCServer);

static int
_PrintVersion()
{
    std::cout << PRODUCT_NAME "\n";
    std::cout << VERSION_COPYRIGHT "\n\n";
    std::cout << "This is " VERSION_ID " built on " VERSION_DATE "\n";

    return 0;
}

static void
_PrintUsage()
{
    std::cout << PRODUCT_NAME "\n";
    std::cout << "Usage: rosev_ircsystem [options] <configuration directory>\n\n";
    std::cout << Configuration.GetCommandLineOptions();
}

#ifndef WIN32
static void
_SigHupHandler(int Signal)
{
    /* Do nothing as long as we don't implement reloading the configuration. This prevents abnormal terminations at least. */
}
#endif

void
Info(const char* Message, ...)
{
    if(Configuration.BeVerbose())
    {
        va_list Arguments;
        va_start(Arguments, Message);
        vprintf(Message, Arguments);
        va_end(Arguments);
    }
}

void
InitializeServer()
{
    Configuration.ReadConfigFiles();

    /* Check if an instance of us is already running */
#ifdef WIN32
    /* Use a native global mutex */
    CreateMutexA(NULL, FALSE, "Global\\" APPLICATION_NAME);
    DWORD LastError = GetLastError();

    /* ERROR_ACCESS_DENIED is returned if the mutex was created by the service in a SYSTEM context.
       Checking for it as well is easier than creating an ACL with global read access. */
    if(LastError == ERROR_ALREADY_EXISTS || LastError == ERROR_ACCESS_DENIED)
        BOOST_THROW_EXCEPTION(Error("The process is already running!"));
#else
    /* Use a pidfile */
    const std::string& PidFile = Configuration.GetPidFile();
    if(std::ifstream(PidFile.c_str()))
        BOOST_THROW_EXCEPTION(Error("The process is already running!"));

    std::ofstream PidStream(PidFile.c_str());
    if(!PidStream)
        BOOST_THROW_EXCEPTION(Error("Could not create the pidfile!") << boost::errinfo_file_name(PidFile));

    DeletePidFile = true;
    PidStream << getpid() << "\n";
    PidStream.close();
#endif

    IRCServer.Init();
    IRCServer.AddVirtualClient(&NickServ);
    IRCServer.AddVirtualClient(&LogBot);
    VoteBotManager.Init();
}

void
RunServerEventLoop()
{
    IOService.run();
}

int
main(int argc, char* argv[])
{
    int ReturnValue = 1;

    try
    {
        if(Configuration.ParseParameters(argc, argv))
        {
            if(Configuration.DoPrintVersion())
                ReturnValue = _PrintVersion();
#ifdef WIN32
            else if(Configuration.DoInstallService())
                ReturnValue = InstallNTService(Configuration);
            else if(Configuration.DoUninstallService())
                ReturnValue = UninstallNTService();
            else if(Configuration.DoRunAsDaemonService())
                ReturnValue = RunAsNTService();
#else
            else if(Configuration.DoRunAsDaemonService())
                ReturnValue = RunAsPosixDaemon();
#endif
            else
                ReturnValue = RunInConsole();
        }
        else
        {
            _PrintUsage();
        }
    }
    catch(std::exception& Exception)
    {
        /* If the current running mode hasn't defined an own general exception handler, we will print them to the console. */
        std::cerr << boost::diagnostic_information(Exception) << "\n";
    }

#ifndef WIN32
    if(DeletePidFile)
        unlink(Configuration.GetPidFile().c_str());
#endif

    return ReturnValue;
}

void
SetSignals()
{
#ifndef WIN32
    /* SIGHUP is only supported on Unix machines and e.g. "service rosev_ircsystem reload" emits it. */
    signal(SIGHUP, _SigHupHandler);
#endif

    signal(SIGINT, ShutdownServer);
    signal(SIGTERM, ShutdownServer);
}

void
ShutdownServer(int Signal)
{
    /* Signal is ignored and only needed to use this as a callback for signal() */

    Info("Shutting down...\n");
    IRCServer.Shutdown();
}

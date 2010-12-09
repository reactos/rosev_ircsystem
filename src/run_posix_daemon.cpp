/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include "precomp.h"

/**
 * Does most tasks described in http://www.enderunix.org/docs/eng/daemon.php before initializing the server.
 */
void
RunAsPosixDaemon()
{
    /* Fork the process to create a background one. */
    pid_t ProcessId = fork();
    if(ProcessId < 0)
    {
        /* fork() has failed */
        BOOST_THROW_EXCEPTION("Failed to create the daemon process!") << boost::errinfo_api_function("fork") << boost::errinfo_errno(errno);
    }
    else if(ProcessId > 0)
    {
        /* We're the parent process, exit here */
        return 0;
    }

    /* Detach from the terminal that started us */
    setsid();

    /* We're the child process and can continue now. */
    SetSignals();
    openlog(APPLICATION_NAME, LOG_PID, LOG_DAEMON);

    try
    {
        InitializeServer();
        RunServerEventLoop();
    }
    catch(std::exception& Exception)
    {
        /* Log all exceptions with diagnostic information to the syslog */
        syslog(LOG_ERR, boost::diagnostic_information(Exception).c_str());
    }

    closelog();
}

/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <cassert>
#include <csignal>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/assign.hpp>
#include <boost/date_time.hpp>
#include <boost/exception/all.hpp>
#include <boost/format.hpp>
#include <boost/preprocessor.hpp>
#include <boost/program_options.hpp>
#include <boost/ptr_container/ptr_container.hpp>
#include <boost/smart_ptr.hpp>
#include <openssl/sha.h>

#include "exceptions.h"
#include "irc_constants.h"

#ifdef WIN32
    #define ENVIRONMENT_NAME      "Windows"
    #define PATH_SEPARATOR        "\\"

    #include <windows.h>
#else
    #define ENVIRONMENT_NAME  "Posix"
    #define PATH_SEPARATOR    "/"

    #include <syslog.h>
    #include <unistd.h>
#endif

#define CHANNELS_FILE           PATH_SEPARATOR "Channels.ini"
#define CHANNEL_USERS_FILE      PATH_SEPARATOR "Channel_Users.ini"
#define LOGBOT_FILE             PATH_SEPARATOR "LogBot.ini"
#define MAIN_CONFIG_FILE        PATH_SEPARATOR "MainConfig.ini"
#define MOTD_FILE               PATH_SEPARATOR "Motd.txt"
#define NICKSERV_USERS_FILE     PATH_SEPARATOR "NickServ_Users.ini"
#define VOTEBOTMANAGER_FILE     PATH_SEPARATOR "VoteBotManager.ini"
#define VOTEBOT_INDIVIDUAL_FILE PATH_SEPARATOR "VoteBot_%s.ini"

/* Using typeid just in debug mode for checking the correct pointer types is still better than
   a dynamic_cast, which requires RTTI also for the Release build.
   We need this macro, because our method pointers can only use general CClient pointers. */
#define TO_NETWORKCLIENT(ClientPointer, OutputPointer)  \
    assert(ClientPointer->IsNetworkClient());  \
    CNetworkClient* OutputPointer = static_cast<CNetworkClient*>(ClientPointer);

/* Forward declarations */
class CChannel;
class CClient;
class CConfiguration;
class CIRCServer;
class CLogBot;
class CNetworkClient;
class CNickServ;
class CPlainNetworkClient;
class CSSLNetworkClient;
class CVirtualClient;
class CVoteBot;
class CVoteBotManager;

#include "CChannel.h"
#include "CClient.h"
#include "CConfiguration.h"
#include "CIRCServer.h"
#include "CNetworkClient.h"
#include "CPlainNetworkClient.h"
#include "CSSLNetworkClient.h"
#include "CVirtualClient.h"
#include "CLogBot.h"
#include "CNickServ.h"
#include "CVoteBotManager.h"
#include "CVoteBot.h"

/* main.cpp */
extern void Info(const char* Message, ...);
extern void InitializeServer();
extern void RunServerEventLoop();
extern void SetSignals();
extern void ShutdownServer(int Signal = 0);

/* run_console.cpp */
extern void RunInConsole();

/* run_nt_service.cpp */
extern void InstallNTService(CConfiguration& Configuration);
extern void RunAsNTService();
extern void UninstallNTService();

/* run_posix_daemon.cpp */
extern void RunAsPosixDaemon();

/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010-2011 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

CLogBot::CLogBot(CIRCServer& IRCServer)
    : CVirtualClient(IRCServer, "LogBot")
{
    m_CommandHandlers = boost::assign::map_list_of
        ("JOIN", &CLogBot::_LogMessage_JOIN)
        ("PART", &CLogBot::_LogMessage_PART)
        ("PRIVMSG", &CLogBot::_LogMessage_PRIVMSG)
        ("QUIT", &CLogBot::_LogMessage_QUIT);
}

std::string
CLogBot::_GetLogTimestamp()
{
    boost::posix_time::ptime Now(boost::posix_time::second_clock::local_time());
    return boost::str(boost::format("[%02u:%02u]") % Now.time_of_day().hours() % Now.time_of_day().minutes());
}

void
CLogBot::_LogMessage_JOIN(CClient* Sender, const std::vector<std::string>& Parameters)
{
    /* Expect a channel as the first and only parameter */
    assert(Parameters.size() == 1);
    assert(Parameters[0][0] == '#');

    /* Lowercase it for searching */
    std::string ChannelNameLowercased(Parameters[0].substr(1));
    std::transform(ChannelNameLowercased.begin(), ChannelNameLowercased.end(), ChannelNameLowercased.begin(), tolower);

    /* Check if we log this channel */
    boost::ptr_map<std::string, std::ofstream>::iterator ChannelStreamIt = m_ChannelStreamMap.find(ChannelNameLowercased);
    if(ChannelStreamIt == m_ChannelStreamMap.end())
        return;

    /* Determine the status of this client */
    const boost::ptr_map<std::string, CChannel>& Channels = m_IRCServer.GetChannels();
    boost::ptr_map<std::string, CChannel>::const_iterator ChannelIt = Channels.find(ChannelNameLowercased);
    assert(ChannelIt != Channels.end());

    const std::map<CClient*, CChannel::ClientStatus>& Clients = ChannelIt->second->GetClients();
    std::map<CClient*, CChannel::ClientStatus>::const_iterator ClientIt = Clients.find(Sender);
    assert(ClientIt != Clients.end());

    /* Finally log the message */
    (*ChannelStreamIt->second) << boost::str(boost::format("%s %s has joined %s%s\n")
        % _GetLogTimestamp()
        % Sender->GetNickname()
        % Parameters[0]
        % (ClientIt->second == CChannel::Voice ? " with voice status" : "")
    );
}

void
CLogBot::_LogMessage_PART(CClient* Sender, const std::vector<std::string>& Parameters)
{
    /* Expect a channel as the first and only parameter */
    assert(Parameters.size() == 1);
    assert(Parameters[0][0] == '#');

    /* Lowercase it for searching */
    std::string ChannelNameLowercased(Parameters[0].substr(1));
    std::transform(ChannelNameLowercased.begin(), ChannelNameLowercased.end(), ChannelNameLowercased.begin(), tolower);

    /* Check if we log this channel and add a message in this case */
    boost::ptr_map<std::string, std::ofstream>::iterator ChannelStreamIt = m_ChannelStreamMap.find(ChannelNameLowercased);
    if(ChannelStreamIt != m_ChannelStreamMap.end())
        (*ChannelStreamIt->second) << boost::str(boost::format("%s %s has left %s\n") % _GetLogTimestamp() % Sender->GetNickname() % Parameters[0]);
}

void
CLogBot::_LogMessage_PRIVMSG(CClient* Sender, const std::vector<std::string>& Parameters)
{
    /* Expect two parameters (channel and message)
       Even though the PRIVMSG command is also used for real private messages, they can only be handled by
       overwriting CClient::SendPrivateMessage in a virtual client. */
    assert(Parameters.size() == 2);
    assert(Parameters[0][0] == '#');

    /* Lowercase the channel name for searching */
    std::string ChannelNameLowercased(Parameters[0].substr(1));
    std::transform(ChannelNameLowercased.begin(), ChannelNameLowercased.end(), ChannelNameLowercased.begin(), tolower);

    /* Check if we log this channel and add a message in this case */
    boost::ptr_map<std::string, std::ofstream>::iterator ChannelStreamIt = m_ChannelStreamMap.find(ChannelNameLowercased);
    if(ChannelStreamIt != m_ChannelStreamMap.end())
        (*ChannelStreamIt->second) << boost::str(boost::format("%s <%s> %s\n") % _GetLogTimestamp() % Sender->GetNickname() % Parameters[1]);
}

void
CLogBot::_LogMessage_QUIT(CClient* Sender, const std::vector<std::string>& Parameters)
{
    /* Expect one parameter */
    assert(Parameters.size() == 1);

    /* Report the QUIT for every logged channel this user is a member of */
    const boost::ptr_map<std::string, CChannel>& Channels = m_IRCServer.GetChannels();
    std::string LogMessage(boost::str(boost::format("%s %s has quit the server (%s)\n") % _GetLogTimestamp() % Sender->GetNickname() % Parameters[0]));

    for(boost::ptr_map<std::string, std::ofstream>::iterator ChannelStreamIt = m_ChannelStreamMap.begin(); ChannelStreamIt != m_ChannelStreamMap.end(); ++ChannelStreamIt)
    {
        /* Get the respective CChannel pointer */
        boost::ptr_map<std::string, CChannel>::const_iterator ChannelIt = Channels.find(ChannelStreamIt->first);
        assert(ChannelIt != Channels.end());

        /* Check if this pointer is on the list of joined channels of the sender */
        const std::set<CChannel*>& JoinedChannels = Sender->GetJoinedChannels();
        if(JoinedChannels.find(const_cast<CChannel*>(ChannelIt->second)) != JoinedChannels.end())
        {
            /* The client is a member of this channel */
            (*ChannelStreamIt->second) << LogMessage;
        }
    }
}

bool
CLogBot::Init()
{
    /* Load the LogBot-specific configuration */
    std::string ConfigFile(m_IRCServer.GetConfiguration().GetConfigPath());
    ConfigFile.append(LOGBOT_FILE);

    if(!std::ifstream(ConfigFile.c_str()))
    {
        /* Assume the user has disabled LogBot deliberately */
        Info("LogBot is disabled, because the configuration file does not exist.\n");
        return false;
    }

    std::vector<std::string> ChannelNames;
    std::string LogPath;
    boost::program_options::options_description ConfigOptions;
    ConfigOptions.add_options()
        ("channels", boost::program_options::value< std::vector<std::string> >(&ChannelNames)->default_value(std::vector<std::string>(), ""))
        ("logpath", boost::program_options::value<std::string>(&LogPath)->default_value(""));

    boost::program_options::variables_map TemporaryVariablesMap;
    boost::program_options::store(boost::program_options::parse_config_file<char>(ConfigFile.c_str(), ConfigOptions), TemporaryVariablesMap);
    boost::program_options::notify(TemporaryVariablesMap);

    /* Sanity checks */
    if(ChannelNames.empty())
        BOOST_THROW_EXCEPTION(Error("You have to set the channel names for LogBot!"));

    if(LogPath.empty())
        BOOST_THROW_EXCEPTION(Error("You have to set the log path for LogBot!"));

    /* Prepend a current timestamp to the log file names */
    boost::posix_time::ptime Now(boost::posix_time::second_clock::local_time());
    std::string PrependString = boost::str(boost::format("%s_%s - ") % boost::gregorian::to_iso_extended_string(Now.date()) % boost::posix_time::to_iso_string(Now.time_of_day()));

    const boost::ptr_map<std::string, CChannel>& Channels = m_IRCServer.GetChannels();
    for(std::vector<std::string>::const_iterator it = ChannelNames.begin(); it != ChannelNames.end(); ++it)
    {
        std::string ChannelNameLowercased(*it);
        std::transform(ChannelNameLowercased.begin(), ChannelNameLowercased.end(), ChannelNameLowercased.begin(), tolower);

        /* Does this channel even exist? */
        if(Channels.find(ChannelNameLowercased) == Channels.end())
            BOOST_THROW_EXCEPTION(Error("LogBot configuration contains an invalid channel name!") << ChannelName_Info(*it));

        /* Open a logfile for this channel */
        std::ostringstream FileName;
        FileName << LogPath << PATH_SEPARATOR << PrependString << *it << ".log";

        std::auto_ptr<std::ofstream> LogStream(new std::ofstream(FileName.str().c_str()));
        if(!*LogStream)
            BOOST_THROW_EXCEPTION(Error("Could not open the log file for writing!") << boost::errinfo_file_name(FileName.str()));

        /* Add the channel (lowercased for comparison purposes) */
        m_ChannelStreamMap.insert(ChannelNameLowercased, LogStream);
    }

    Info("LogBot is enabled.\n");
    return true;
}

void
CLogBot::PostInit()
{
    /* Join all channels we log */
    for(boost::ptr_map<std::string, std::ofstream>::const_iterator it = m_ChannelStreamMap.begin(); it != m_ChannelStreamMap.end(); ++it)
    {
        std::vector<std::string> Parameters;
        Parameters.push_back(it->first);
        m_IRCServer.ReceiveMessage_JOIN(this, Parameters);
    }
}

void
CLogBot::SendIRCMessage(const std::string& Message)
{
    /* All received messages should begin with a prefix */
    assert(Message[0] == ':');

    /* Extract the lowercased sender name.
       We make use of the fact that the user name is always the lowercased sender name. */
    size_t Offset = Message.find_first_of('!');

    /* We may find no offset if the message comes from the server (like MODE messages) */
    if(Offset == std::string::npos)
        return;

    ++Offset;
    size_t Position = Message.find_first_of('@', Offset);
    assert(Position != std::string::npos);
    std::string SenderNameLowercased(Message.substr(Offset, Position - Offset));

    /* Get the respective CClient pointer */
    const std::map<std::string, CClient*>& Nicknames = m_IRCServer.GetNicknames();
    std::map<std::string, CClient*>::const_iterator ClientIt = Nicknames.find(SenderNameLowercased);
    assert(ClientIt != Nicknames.end());

    /* Extract the command */
    Offset = Message.find_first_of(' ', Position);
    assert(Offset != std::string::npos);
    ++Offset;
    Position = Message.find_first_of(' ', Offset);
    std::string Command(Message.substr(Offset, Position - Offset));

    /* Extract all parameters into the vector */
    std::vector<std::string> Parameters;
    while(Position != std::string::npos)
    {
        Offset = Position + 1;

        if(Message[Offset] == ':')
        {
            /* A parameter starting with ':' is always the last parameter and consists of all
               following characters. */
            Parameters.push_back(Message.substr(Offset + 1));
            break;
        }

        /* Find the end of this parameter */
        Position = Message.find_first_of(' ', Offset);
        Parameters.push_back(Message.substr(Offset, Position - Offset));
    }

    /* Find a command handler, ignore unknown commands */
    std::map<std::string, void (CLogBot::*)(CClient*, const std::vector<std::string>&)>::const_iterator HandlerIt = m_CommandHandlers.find(Command);
    if(HandlerIt != m_CommandHandlers.end())
    {
        /* Call the handler */
        (this->*(HandlerIt->second))(ClientIt->second, Parameters);
    }
}

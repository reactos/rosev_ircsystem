/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010-2011 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>
#include <version.h>

CIRCServer::CIRCServer(boost::asio::io_service& IOService, CConfiguration& Configuration)
    : m_Configuration(Configuration), m_IOService(IOService)
{
}

inline void
CIRCServer::_Accept(boost::asio::ip::tcp::acceptor* Acceptor)
{
    /* Clumsy call */
    Acceptor->async_accept(
        m_NextNetworkClient->GetSocket(),
        boost::bind(&CIRCServer::_HandleNewConnection, this, Acceptor, boost::asio::placeholders::error)
    );
}

void
CIRCServer::_AddAcceptor(const boost::asio::ip::tcp::acceptor::protocol_type& Protocol)
{
    boost::asio::ip::tcp::endpoint Endpoint(Protocol, m_Configuration.GetPort());
    std::auto_ptr<boost::asio::ip::tcp::acceptor> Acceptor(new boost::asio::ip::tcp::acceptor(m_IOService, Endpoint));

    /* Disable dual-stack mode for the IPv6 acceptor as IPv4 addresses are already handled by our IPv4 acceptor. */
    if(Protocol.family() == PF_INET6)
        Acceptor->set_option(boost::asio::ip::v6_only(true));

    boost::system::error_code ErrorCode;
    Acceptor->listen(boost::asio::socket_base::max_connections, ErrorCode);
    if(ErrorCode)
        BOOST_THROW_EXCEPTION(Error(ErrorCode.message().c_str()) << boost::errinfo_api_function("boost::asio::ip::tcp::acceptor::listen"));

    _Accept(Acceptor.get());

    m_Acceptors.push_back(Acceptor);
}

void
CIRCServer::_CheckForPresetNickname(CNetworkClient* Client)
{
    /* If this is a preset nickname, the user needs to identify within a given timeframe. */
    const std::map< std::string, boost::array<char, SHA512_DIGEST_LENGTH> >& UserPasshashMap = m_Configuration.GetUserPasshashMap();
    if(UserPasshashMap.find(Client->GetNicknameLowercased()) != UserPasshashMap.end())
    {
        Client->SendNotice(NULL, "This nickname is protected.");
        Client->SendNotice(NULL, "Please identify with your password in the next " BOOST_PP_STRINGIZE(IDENTIFY_TIMEOUT) " seconds or you will be disconnected.");
        Client->SendNotice(NULL, "Use the command /NS IDENTIFY <password> to do so.");
        Client->RestartIdentifyTimer();
    }
    else
    {
        /* It isn't, so cancel the identify timer and force the client to respond to regular pings again. */
        Client->RestartPingTimer();
    }
}

void
CIRCServer::_HandleNewConnection(boost::asio::ip::tcp::acceptor* Acceptor, const boost::system::error_code& ErrorCode)
{
    if(ErrorCode)
        return;

    /* Handle this connection ... */
    m_NextNetworkClient->Init();
    m_NetworkClients.insert(m_NextNetworkClient);

    /* ... and accept new ones for this acceptor again */
    if(m_Configuration.DoUseSSL())
        m_NextNetworkClient.reset(new CSSLNetworkClient(m_IOService, *this, *m_SSLContext.get()));
    else
        m_NextNetworkClient.reset(new CPlainNetworkClient(m_IOService, *this));

    _Accept(Acceptor);
}

std::string
CIRCServer::_HandleSSLPassword() const
{
    /* Don't support password-protected SSL certificates */
    BOOST_THROW_EXCEPTION(Error("Your SSL certificate must not be password-protected!"));
}

inline bool
CIRCServer::_IsUserRegistered(const CClient::UserState& ClientUserState)
{
    return (ClientUserState.HasSentNickMessage && ClientUserState.HasSentUserMessage);
}

void
CIRCServer::_WelcomeClient(CNetworkClient* Client)
{
    /* COMPATIBILITY: Send some common welcome replies to the client.
       For example, ChatZilla won't show you connected till it has received RPL_WELCOME. */
    Client->SendNumericReply(RPL_WELCOME) % m_Configuration.GetName() % Client->GetNickname();
    Client->SendNumericReply(RPL_YOURHOST) % m_Configuration.GetName();
    Client->SendNumericReply(RPL_CREATED);
    Client->SendNumericReply(RPL_MYINFO) % m_Configuration.GetName();

    /* Send him our MOTD */
    ReceiveMessage_MOTD(Client, std::vector<std::string>());

    /* COMPATIBILITY: Send a dummy +i mode to the client, so that it doesn't get confused when it expects a MODE response here.
       For example, ChatZilla won't start the lagtimer till it received the MODE response.
       Due to some reason, ircd-seven and other IRC servers use the nickname instead of a full prefix here. */
    Client->SendIRCMessage(boost::str(boost::format(":%1% MODE %1% :+i") % Client->GetNickname()));

    /* At the end of the initial login, the user should know whether he needs to identify or not. */
    _CheckForPresetNickname(Client);
}

void
CIRCServer::AddVirtualClient(CVirtualClient* Client)
{
    if(Client->Init())
    {
        /* Trust our virtual clients and add them without checking the nickname
           (as it would be done in CIRCServer::ReceiveMessage_NICK) */
        m_Nicknames.insert(std::make_pair(Client->GetNicknameLowercased(), Client));
        Client->PostInit();
    }
}

void
CIRCServer::DisconnectNetworkClient(CNetworkClient* Client, const std::string& Reason)
{
    /* If the client disconnects or needs to be disconnected, several operations may fail and lead to a
       CIRCServer::DisconnectNetworkClient call. */
    if(Client->HasCompletedShutdown())
        return;

    if(!Client->GetNickname().empty())
    {
        const std::set<CChannel*>& JoinedChannels = Client->GetJoinedChannels();
        if(!JoinedChannels.empty())
        {
            std::set<CClient*> HandledClients;
            std::string Response(boost::str(boost::format(":%s QUIT :%s") % Client->GetPrefix() % Reason));

            for(std::set<CChannel*>::const_iterator ChannelIt = JoinedChannels.begin(); ChannelIt != JoinedChannels.end(); ++ChannelIt)
            {
                /* Send a QUIT response to all members of all channels, in which this client is a member of,
                   but only once for each member. */
                const std::map<CClient*, CChannel::ClientStatus>& Clients = (*ChannelIt)->GetClients();
                for(std::map<CClient*, CChannel::ClientStatus>::const_iterator ClientIt = Clients.begin(); ClientIt != Clients.end(); ++ClientIt)
                {
                    if(HandledClients.find(ClientIt->first) == HandledClients.end())
                    {
                        /* This client has not received the QUIT message yet */
                        ClientIt->first->SendIRCMessage(Response);
                        HandledClients.insert(ClientIt->first);
                    }
                }

                /* Remove the client from this channel */
                (*ChannelIt)->RemoveClient(Client);
            }
        }

        /* Remove the nickname */
        m_Nicknames.erase(Client->GetNicknameLowercased());
    }

    /* Send the ERROR if the client has a chance to receive it */
    if(Client->IsInitialized())
    {
        /* ROSEV-SPECIFIC: The string preceding the brackets is done differently in every IRC Server,
           so we are free to use the result of CClient::GetNicknameAsTarget here.
           We cannot leave it out entirely for COMPATIBILITY with clients like ChatZilla. */
        Client->SendIRCMessage(boost::str(boost::format("ERROR :Closing Link: %s (%s)") % Client->GetNicknameAsTarget() % Reason));
    }

    /* Finally remove the client */
    Client->Shutdown();
    m_NetworkClients.erase(Client->shared_from_this());
}

void
CIRCServer::Init()
{
    /* Prepare the SSL stuff (if necessary) and a first network client */
    if(m_Configuration.DoUseSSL())
    {
        m_SSLContext.reset(new boost::asio::ssl::context(m_IOService, boost::asio::ssl::context::sslv23_server));
        m_SSLContext->set_options(boost::asio::ssl::context_base::default_workarounds | boost::asio::ssl::context_base::no_sslv2);
        m_SSLContext->set_password_callback(boost::bind(&CIRCServer::_HandleSSLPassword, this));
        m_SSLContext->use_certificate_file(m_Configuration.GetSSLCertificateFile(), boost::asio::ssl::context::pem);
        m_SSLContext->use_private_key_file(m_Configuration.GetSSLPrivateKeyFile(), boost::asio::ssl::context::pem);

        m_NextNetworkClient.reset(new CSSLNetworkClient(m_IOService, *this, *m_SSLContext.get()));
        Info("SSL is enabled.\n");
    }
    else
    {
        m_NextNetworkClient.reset(new CPlainNetworkClient(m_IOService, *this));
        Info("SSL is disabled.\n");
    }

    /* Get the channel -> users mappings */
    std::string ConfigPath(m_Configuration.GetConfigPath());
    std::map< std::string, std::set<std::string> > ChannelUsersMap;

    boost::program_options::parsed_options ChannelUsersParsedOptions(boost::program_options::parse_config_file<char>(std::string(ConfigPath).append(CHANNEL_USERS_FILE).c_str(), NULL, true));
    for(std::vector< boost::program_options::basic_option<char> >::const_iterator OptionIt = ChannelUsersParsedOptions.options.begin(); OptionIt != ChannelUsersParsedOptions.options.end(); ++OptionIt)
    {
        /* All config information is used for comparison, so lowercase it */
        std::string ChannelNameLowercased(OptionIt->string_key);
        std::string UserNameLowercased(OptionIt->value[0]);
        std::transform(ChannelNameLowercased.begin(), ChannelNameLowercased.end(), ChannelNameLowercased.begin(), tolower);
        std::transform(UserNameLowercased.begin(), UserNameLowercased.end(), UserNameLowercased.begin(), tolower);

        std::map< std::string, std::set<std::string> >::iterator MapIt = ChannelUsersMap.find(ChannelNameLowercased);
        if(MapIt == ChannelUsersMap.end())
        {
            std::set<std::string> AllowedUsers;
            AllowedUsers.insert(UserNameLowercased);
            ChannelUsersMap.insert(std::make_pair(ChannelNameLowercased, AllowedUsers));
        }
        else
        {
            MapIt->second.insert(UserNameLowercased);
        }
    }

    /* Check which channels allow observers. */
    std::set<std::string> ChannelObserversSet;

    boost::program_options::parsed_options ChannelObserversParsedOptions(boost::program_options::parse_config_file<char>(std::string(ConfigPath).append(CHANNEL_OBSERVERS_FILE).c_str(), NULL, true));
    for(std::vector< boost::program_options::basic_option<char> >::const_iterator OptionIt = ChannelObserversParsedOptions.options.begin(); OptionIt != ChannelObserversParsedOptions.options.end(); ++OptionIt)
    {
        /* Lowercase the channel name for comparison */
        std::string ChannelNameLowercased(OptionIt->string_key);
        std::transform(ChannelNameLowercased.begin(), ChannelNameLowercased.end(), ChannelNameLowercased.begin(), tolower);

        if(OptionIt->value[0] == "true")
            ChannelObserversSet.insert(ChannelNameLowercased);
    }

    /* Add the preset channels */
    boost::program_options::parsed_options ChannelsParsedOptions(boost::program_options::parse_config_file<char>(std::string(ConfigPath).append(CHANNELS_FILE).c_str(), NULL, true));
    if(ChannelsParsedOptions.options.empty())
        BOOST_THROW_EXCEPTION(Error("You need to specify at least one channel!"));

    for(std::vector< boost::program_options::basic_option<char> >::const_iterator OptionIt = ChannelsParsedOptions.options.begin(); OptionIt != ChannelsParsedOptions.options.end(); ++OptionIt)
    {
        /* ROSEV-SPECIFIC: We only allow A-Z, a-z, 0-9, -, _ for channel names */
        const char* p = OptionIt->string_key.c_str();
        while(*p)
        {
            if(!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || (*p == '_') || (*p == '_')))
                BOOST_THROW_EXCEPTION(Error("Illegal channel name!") << ChannelName_Info(OptionIt->string_key));

            ++p;
        }

        /* Find the corresponding users set */
        std::string ChannelNameLowercased(OptionIt->string_key);
        std::transform(ChannelNameLowercased.begin(), ChannelNameLowercased.end(), ChannelNameLowercased.begin(), tolower);

        std::map< std::string, std::set<std::string> >::const_iterator ChannelUsersIt = ChannelUsersMap.find(ChannelNameLowercased);
        if(ChannelUsersIt == ChannelUsersMap.end())
            BOOST_THROW_EXCEPTION(Error("No allowed users were set for this channel!") << ChannelName_Info(OptionIt->string_key));

        /* Check whether observers are allowed for this channel. */
        std::set<std::string>::const_iterator ChannelObserversIt = ChannelObserversSet.find(ChannelNameLowercased);
        bool AllowObservers = (ChannelObserversIt != ChannelObserversSet.end());

        /* Insert the channel name lowercased for comparisons, but leave the original spelling in the object */
        m_Channels.insert(ChannelNameLowercased, new CChannel(OptionIt->string_key, OptionIt->value[0], ChannelUsersIt->second, AllowObservers));
    }

    /* Set up an acceptor for every IP stack (if desired) */
    if(m_Configuration.DoUseIPv4())
    {
        _AddAcceptor(boost::asio::ip::tcp::v4());
        Info("Listening on port %u for IPv4 connections.\n", m_Configuration.GetPort());
    }

    if(m_Configuration.DoUseIPv6())
    {
        _AddAcceptor(boost::asio::ip::tcp::v6());
        Info("Listening on port %u for IPv6 connections.\n", m_Configuration.GetPort());
    }

    /* Now we're online! */
    boost::posix_time::ptime Now(boost::posix_time::second_clock::local_time());
    m_OnlineDateString = boost::str(boost::format("%s %s % 2u %s %u") % Now.date().day_of_week() % Now.date().month() % Now.date().day() % Now.time_of_day() % Now.date().year());
}

/**
 * RFC2812, 3.4.10 - Info command
 *
 * Probably not really required, but we want our credits! :-)
 */
void
CIRCServer::ReceiveMessage_INFO(CClient* Sender, const std::vector<std::string>& Parameters)
{
    Sender->SendNumericReply(RPL_INFO) % PRODUCT_NAME;
    Sender->SendNumericReply(RPL_INFO) % VERSION_COPYRIGHT;
    Sender->SendNumericReply(RPL_INFO) % "";
    Sender->SendNumericReply(RPL_INFO) % VERSION_ID " (" ENVIRONMENT_NAME ")";
    Sender->SendNumericReply(RPL_INFO) % "";
    Sender->SendNumericReply(RPL_INFO) % "Birth Date: " VERSION_DATE;
    Sender->SendNumericReply(RPL_INFO) % std::string("On-line since ").append(m_OnlineDateString);
}

/**
 * RFC2812, 3.2.1 - Join message
 */
void
CIRCServer::ReceiveMessage_JOIN(CClient* Sender, const std::vector<std::string>& Parameters)
{
    if(!_IsUserRegistered(Sender->GetUserState()))
        return;

    if(Parameters.empty())
    {
        Sender->SendNumericReply(ERR_NEEDMOREPARAMS) % "JOIN";
        return;
    }

    if(Parameters[0] == "0")
    {
        /* Leave all joined channels
           RFC2812, 3.2.1 - "The server will process this message as if the user had
           sent a PART command (See Section 3.2.2) for each channel he is a member of." */
        std::set<CChannel*> JoinedChannels = Sender->GetJoinedChannels();

        for(std::set<CChannel*>::const_iterator it = JoinedChannels.begin(); it != JoinedChannels.end(); ++it)
        {
            std::vector<std::string> CallParameters;
            CallParameters.push_back((*it)->GetName());

            ReceiveMessage_PART(Sender, CallParameters);
        }
    }

    /* ROSEV-SPECIFIC: Don't allow joining if the user still has to identify. */
    const std::map< std::string, boost::array<char, SHA512_DIGEST_LENGTH> >& UserPasshashMap = m_Configuration.GetUserPasshashMap();
    if(UserPasshashMap.find(Sender->GetNicknameLowercased()) != UserPasshashMap.end() && !Sender->GetUserState().IsIdentified)
    {
        Sender->SendNotice(NULL, "Please identify first!");
        return;
    }

    /* Process all given channels */
    const std::string& Channels = Parameters[0];
    size_t Offset = 0;
    size_t Position;

    do
    {
        /* Skip the hash character in the channel name (if any) */
        if(Channels[Offset] == '#')
            ++Offset;

        /* Extract the lowercased channel up to the next delimiter (or the end if none was found) */
        Position = Channels.find_first_of(',', Offset);
        std::string Channel(Channels.substr(Offset, Position - Offset));
        Offset = Position + 1;
        std::transform(Channel.begin(), Channel.end(), Channel.begin(), tolower);

        /* ROSEV-SPECIFIC: Only allow joining existing channels */
        boost::ptr_map<std::string, CChannel>::iterator it = m_Channels.find(Channel);
        if(it == m_Channels.end())
        {
            Sender->SendNumericReply(ERR_NOSUCHCHANNEL) % Channel;
        }
        else
        {
            /* ROSEV-SPECIFIC: Virtual clients and clients on the list of allowed users automatically get voice.
               If we don't allow observers, only these clients may even join. */
            const std::set<std::string>& AllowedUsers = it->second->GetAllowedUsers();
            bool IsVoicedClient = (!Sender->IsNetworkClient() || AllowedUsers.find(Sender->GetNicknameLowercased()) != AllowedUsers.end());

            if(!IsVoicedClient && !it->second->DoAllowObservers())
            {
                Sender->SendNotice(NULL, "You are not allowed to join this channel!");
            }
            else
            {
                /* Check if the user is already part of this channel */
                const std::map<CClient*, CChannel::ClientStatus>& Clients = it->second->GetClients();
                if(Clients.find(Sender) == Clients.end())
                {
                    /* Join this channel */
                    it->second->AddClient(Sender, (IsVoicedClient ? CChannel::Voice : CChannel::NoStatus));
                    Sender->AddJoinedChannel(it->second);

                    /* Send a JOIN response to all channel members (including this new one) */
                    std::string Response(boost::str(boost::format(":%s JOIN #%s") % Sender->GetPrefix() % it->second->GetName()));

                    for(std::map<CClient*, CChannel::ClientStatus>::const_iterator ClientIt = Clients.begin(); ClientIt != Clients.end(); ++ClientIt)
                        ClientIt->first->SendIRCMessage(Response);

                    /* Not very nice, but no nice workaround exists. We have no specifically look for ChanServ here and let it
                       set the user mode. It can't just parse the SendIRCMessage call above, because we have to ensure that this
                       happens after all other clients have received the JOIN message.
                       At least, this allows us to directly pass parameters and not extract them from SendIRCMessage in CChanServ. */
                    std::map<std::string, CClient*>::const_iterator NicknameIt = m_Nicknames.find("chanserv");
                    assert(NicknameIt != m_Nicknames.end());
                    CChanServ* ChanServ = static_cast<CChanServ*>(NicknameIt->second);
                    ChanServ->SetClientModeInChannel(Sender, it->second);

                    /* Send the results of respective TOPIC and NAMES commands */
                    std::vector<std::string> CallParameters;
                    CallParameters.push_back(it->second->GetName());

                    ReceiveMessage_TOPIC(Sender, CallParameters);
                    ReceiveMessage_NAMES(Sender, CallParameters);
                }
            }
        }
    }
    while(Position != std::string::npos);
}

/**
 * RFC2812, 3.4.1 - Motd message
 */
void
CIRCServer::ReceiveMessage_MOTD(CClient* Sender, const std::vector<std::string>& Parameters)
{
    if(!_IsUserRegistered(Sender->GetUserState()))
        return;

    /* Respond with our line-based MOTD */
    const std::vector<std::string>& Motd = m_Configuration.GetMotd();
    Sender->SendNumericReply(RPL_MOTDSTART) % m_Configuration.GetName();

    for(std::vector<std::string>::const_iterator it = Motd.begin(); it != Motd.end(); ++it)
        Sender->SendNumericReply(RPL_MOTD) % *it;

    Sender->SendNumericReply(RPL_ENDOFMOTD);
}

/**
 * RFC2812, 3.2.5 - Names message
 */
void
CIRCServer::ReceiveMessage_NAMES(CClient* Sender, const std::vector<std::string>& Parameters)
{
    if(!_IsUserRegistered(Sender->GetUserState()))
        return;

    /* ROSEV-SPECIFIC: We don't return anything if no parameters were given.
       According to RFC2812, a list of all channels with all nicknames shall be returned here, but
       some popular IRC servers (like ircd-seven) don't do either :-) */
    if(Parameters.empty())
        return;

    /* Process all given channels.
       ROSEV-SPECIFIC: Without a server-to-server architecture, we obviously don't support the
       <target> parameter. */
    const std::string& Channels = Parameters[0];
    size_t Offset = 0;
    size_t Position;

    do
    {
        /* Skip the hash character in the channel name */
        if(Channels[Offset] == '#')
            ++Offset;

        /* Extract the lowercased channel up to the next delimiter (or the end if none was found) */
        Position = Channels.find_first_of(',', Offset);
        std::string Channel(Channels.substr(Offset, Position - Offset));
        Offset = Position + 1;
        std::transform(Channel.begin(), Channel.end(), Channel.begin(), tolower);

        /* Check if this channel exists */
        boost::ptr_map<std::string, CChannel>::const_iterator it = m_Channels.find(Channel);
        if(it != m_Channels.end())
        {
            /* Return a string list of joined nicknames.
               ROSEV-SPECIFIC: We don't support any operators or voiced people. */
            const std::map<CClient*, CChannel::ClientStatus>& Clients = it->second->GetClients();
            std::string NicknameList;

            for(std::map<CClient*, CChannel::ClientStatus>::const_iterator ClientIt = Clients.begin(); ClientIt != Clients.end(); ++ClientIt)
            {
                if(!NicknameList.empty())
                    NicknameList.append(" ");

                if(ClientIt->second == CChannel::Voice)
                    NicknameList.append("+");

                NicknameList.append(ClientIt->first->GetNickname());
            }

            Sender->SendNumericReply(RPL_NAMREPLY) % Channel % NicknameList;
        }

        /* RFC2812, 3.2.5 - "There is no error reply for bad channel names."
           ircd-seven still returns RPL_ENDOFNAMES in this case, so we do as well. */
        Sender->SendNumericReply(RPL_ENDOFNAMES) % Channel;
    }
    while(Position != std::string::npos);
}

/**
 * RFC2812, 3.1.2 - Nick message
 *
 * Must only be used with CNetworkClient pointers! CVirtualClient ones set their nickname
 * in the constructor!
 */
void
CIRCServer::ReceiveMessage_NICK(CClient* Sender, const std::vector<std::string>& Parameters)
{
    TO_NETWORKCLIENT(Sender, NetSender);

    /* Did we even get a nickname? */
    if(Parameters.empty())
    {
        NetSender->SendNumericReply(ERR_NONICKNAMEGIVEN);
        return;
    }

    const std::string& CurrentNickname = NetSender->GetNickname();
    const std::string& NewNickname = Parameters[0];

    /* Silently ignore this message if the client already has a nickname and
       wants to change it to exactly the same one. */
    if(NewNickname == CurrentNickname)
        return;

    /* Is the nickname not too long? */
    if(NewNickname.length() > NICKNAME_LENGTH)
    {
        NetSender->SendNumericReply(ERR_ERRONEUSNICKNAME) % NewNickname;
        return;
    }

    /* Does the nickname only contain valid characters? */
    const char* p = NewNickname.c_str();
    while(*p)
    {
        /* ROSEV-SPECIFIC: We only allow A-Z, a-z, _ for nicknames */
        if(!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p == '_')))
        {
            NetSender->SendNumericReply(ERR_ERRONEUSNICKNAME) % NewNickname;
            return;
        }

        ++p;
    }

    /* Do we already have a user with the same nickname, and is this user not the client trying to
       change the nickname?
       We have to check the latter in case the new nickname just differs in lowercased/uppercased
       characters. */
    std::string NewNicknameLowercased(NewNickname);
    std::transform(NewNicknameLowercased.begin(), NewNicknameLowercased.end(), NewNicknameLowercased.begin(), tolower);
    std::map<std::string, CClient*>::const_iterator it = m_Nicknames.find(NewNicknameLowercased);

    if(it != m_Nicknames.end() && it->second != Sender)
    {
        NetSender->SendNumericReply(ERR_NICKNAMEINUSE) % NewNickname;
        return;
    }

    /* Change the nickname if applicable */
    if(!CurrentNickname.empty())
    {
        const std::string& CurrentNicknameLowercased = NetSender->GetNicknameLowercased();
        std::map<std::string, CClient*>::iterator it = m_Nicknames.find(CurrentNicknameLowercased);

        /* ROSEV-SPECIFIC: Don't allow changing the nickname if the user has already identified */
        if(it->second->GetUserState().IsIdentified)
        {
            NetSender->SendNotice(NULL, "You cannot change your nickname after having identified!");
            return;
        }

        /* ROSEV-SPECIFIC: Don't allow changing the nickname if the user has already joined a channel */
        if(!it->second->GetJoinedChannels().empty())
        {
            NetSender->SendNotice(NULL, "You cannot change your nickname after having joined a channel!");
            return;
        }

        m_Nicknames.erase(it);
        NetSender->SendIRCMessage(boost::str(boost::format(":%s NICK %s") % NetSender->GetPrefix() % NewNickname));
    }

    /* Add the nickname (lowercased for comparison purposes) */
    m_Nicknames.insert(std::make_pair(NewNicknameLowercased, NetSender));
    NetSender->SetNickname(NewNickname, NewNicknameLowercased);

    /* Check if we have already registered with a nickname before. */
    CClient::UserState State = NetSender->GetUserState();
    if(_IsUserRegistered(State))
    {
        /* The nickname has just been changed, so we need to recheck whether it's a preset one. */
        _CheckForPresetNickname(NetSender);
    }
    else
    {
        /* We have not, so store this and send the welcome messages if the USER message has also been sent. */
        State.HasSentNickMessage = true;
        NetSender->SetUserState(State);

        if(State.HasSentUserMessage)
            _WelcomeClient(NetSender);
    }
}

/**
 * Commonly known server-specific message
 * Just an abbreviation for PRIVMSG NickServ <...>
 */
void
CIRCServer::ReceiveMessage_NS(CClient* Sender, const std::vector<std::string>& Parameters)
{
    if(!_IsUserRegistered(Sender->GetUserState()))
        return;

    std::vector<std::string> CallParameters;
    CallParameters.push_back("nickserv");

    /* Concatenate all parameters into one big parameter for the private message */
    std::string Parameter;
    for(std::vector<std::string>::const_iterator it = Parameters.begin(); it != Parameters.end(); ++it)
    {
        if(!Parameter.empty())
            Parameter.append(" ");

        Parameter.append(*it);
    }

    CallParameters.push_back(Parameter);

    ReceiveMessage_PRIVMSG(Sender, CallParameters);
}

/**
 * RFC2812, 3.2.2 - Part message
 */
void
CIRCServer::ReceiveMessage_PART(CClient* Sender, const std::vector<std::string>& Parameters)
{
    if(!_IsUserRegistered(Sender->GetUserState()))
        return;

    if(Parameters.empty())
    {
        Sender->SendNumericReply(ERR_NEEDMOREPARAMS) % "PART";
        return;
    }

    /* Process all given channels */
    const std::string& Channels = Parameters[0];
    size_t Offset = 0;
    size_t Position;

    do
    {
        /* Skip the hash character in the channel name */
        if(Channels[Offset] == '#')
            ++Offset;

        /* Extract the lowercased channel up to the next delimiter (or the end if none was found) */
        Position = Channels.find_first_of(',', Offset);
        std::string Channel(Channels.substr(Offset, Position - Offset));
        Offset = Position + 1;
        std::transform(Channel.begin(), Channel.end(), Channel.begin(), tolower);

        /* Check if this channel exists */
        boost::ptr_map<std::string, CChannel>::iterator it = m_Channels.find(Channel);
        if(it == m_Channels.end())
        {
            Sender->SendNumericReply(ERR_NOSUCHCHANNEL) % Channel;
        }
        else
        {
            /* Check if the user is part of this channel */
            const std::map<CClient*, CChannel::ClientStatus>& Clients = it->second->GetClients();
            if(Clients.find(Sender) == Clients.end())
            {
                Sender->SendNumericReply(ERR_NOTONCHANNEL) % Channel;
                return;
            }

            /* Send a PART response to all channel members (including this one)
               ROSEV-SPECIFIC: Messages supplied to the PART command are ignored. */
            std::string Response(boost::str(boost::format(":%s PART #%s") % Sender->GetPrefix() % it->second->GetName()));

            for(std::map<CClient*, CChannel::ClientStatus>::const_iterator ClientIt = Clients.begin(); ClientIt != Clients.end(); ++ClientIt)
                ClientIt->first->SendIRCMessage(Response);

            /* Leave the channel */
            it->second->RemoveClient(Sender);
            Sender->RemoveJoinedChannel(it->second);
        }
    }
    while(Position != std::string::npos);
}

/**
 * RFC2812, 3.7.2 - Ping message
 *
 * The PING command is documented in RFC2812, but you only get it right from capturing IRC traffic.
 * Actually, it is just necessary to reply with a PONG using the first parameter (at least for
 * an IRC server with a single-server architecture).
 *
 * Must only be used with CNetworkClient pointers!
 */
void
CIRCServer::ReceiveMessage_PING(CClient* Sender, const std::vector<std::string>& Parameters)
{
    TO_NETWORKCLIENT(Sender, NetSender);

    if(!_IsUserRegistered(NetSender->GetUserState()))
        return;

    if(Parameters.empty())
    {
        /* This error cannot be found for this message in RFC2812 either, but ircd-seven does it this way. */
        NetSender->SendNumericReply(ERR_NEEDMOREPARAMS) % "PING";
        return;
    }

    NetSender->SendIRCMessage(boost::str(boost::format(":%1% PONG %1% :%2%") % m_Configuration.GetName() % Parameters[0]));
}

/**
 * RFC2812, 3.7.3 - Pong message
 *
 * This is the client's reply to a PING message sent by us.
 * Must only be used with CNetworkClient pointers!
 */
void
CIRCServer::ReceiveMessage_PONG(CClient* Sender, const std::vector<std::string>& Parameters)
{
    TO_NETWORKCLIENT(Sender, NetSender);

    if(!_IsUserRegistered(NetSender->GetUserState()))
        return;

    /* This is also a VERY simple implementation of this command, just used to detect
       whether a client is still alive.
       ircd-seven also doesn't check the parameters for restarting the ping timer. */
    NetSender->RestartPingTimer();
}

/**
 * RFC2812, 3.3.1 - Private messages
 *
 * (PRIVMSG is used for both private messages from user to user and from user to a channel)
 */
void
CIRCServer::ReceiveMessage_PRIVMSG(CClient* Sender, const std::vector<std::string>& Parameters)
{
    if(!_IsUserRegistered(Sender->GetUserState()))
        return;

    if(Parameters.empty())
    {
        Sender->SendNumericReply(ERR_NORECIPIENT) % "PRIVMSG";
        return;
    }

    if(Parameters.size() == 1)
    {
        Sender->SendNumericReply(ERR_NOTEXTTOSEND);
        return;
    }

    if(Parameters[0][0] == '#')
    {
        /* Send a message to a channel */
        std::string Channel(Parameters[0].substr(1));
        std::transform(Channel.begin(), Channel.end(), Channel.begin(), tolower);

        /* Does this channel exist? */
        boost::ptr_map<std::string, CChannel>::const_iterator it = m_Channels.find(Channel);
        if(it == m_Channels.end())
        {
            Sender->SendNumericReply(ERR_NOSUCHCHANNEL) % Channel;
            return;
        }

        /* ROSEV-SPECIFIC: Only joined clients with voice status may send anything. */
        const std::map<CClient*, CChannel::ClientStatus>& Clients = it->second->GetClients();
        std::map<CClient*, CChannel::ClientStatus>::const_iterator ClientIt = Clients.find(Sender);

        if(ClientIt == Clients.end() || ClientIt->second == CChannel::NoStatus)
        {
            Sender->SendNumericReply(ERR_CANNOTSENDTOCHAN) % Channel;
            return;
        }

        /* Send the message to all channel members but not back to this client */
        std::string Response(boost::str(boost::format(":%s PRIVMSG #%s :%s") % Sender->GetPrefix() % it->second->GetName() % Parameters[1]));

        for(std::map<CClient*, CChannel::ClientStatus>::const_iterator ClientIt = Clients.begin(); ClientIt != Clients.end(); ++ClientIt)
            if(ClientIt->first != Sender)
                ClientIt->first->SendIRCMessage(Response);
    }
    else
    {
        /* Send a message to a user */
        std::string Nickname(Parameters[0]);
        std::transform(Nickname.begin(), Nickname.end(), Nickname.begin(), tolower);

        /* Does this nickname exist? */
        std::map<std::string, CClient*>::const_iterator it = m_Nicknames.find(Nickname);
        if(it == m_Nicknames.end())
        {
            Sender->SendNumericReply(ERR_NOSUCHNICK) % Nickname;
            return;
        }

        /* Send the message */
        it->second->SendPrivateMessage(Sender, Parameters[1]);
    }
}

/**
 * RFC2812, 3.1.7 - Quit
 */
void
CIRCServer::ReceiveMessage_QUIT(CClient* Sender, const std::vector<std::string>& Parameters)
{
    TO_NETWORKCLIENT(Sender, NetSender);

    if(!_IsUserRegistered(NetSender->GetUserState()))
        return;

    /* ROSEV-SPECIFIC: Messages supplied to the QUIT command are ignored */
    DisconnectNetworkClient(NetSender, "Quit");
}

/**
 * RFC2812, 3.2.4 - Topic message
 *
 * Must only be used with CNetworkClient pointers!
 */
void
CIRCServer::ReceiveMessage_TOPIC(CClient* Sender, const std::vector<std::string>& Parameters)
{
    if(!_IsUserRegistered(Sender->GetUserState()))
        return;

    if(Parameters.empty())
    {
        Sender->SendNumericReply(ERR_NEEDMOREPARAMS) % "TOPIC";
        return;
    }

    /* ROSEV-SPECIFIC: Topic are preset, we don't support changing them */
    if(Parameters.size() >= 2)
        return;

    /* Get the lowercased channel without the hash character (if any) */
    size_t Offset = 0;
    if(Parameters[0][0] == '#')
        ++Offset;

    std::string Channel(Parameters[0].substr(Offset));
    std::transform(Channel.begin(), Channel.end(), Channel.begin(), tolower);

    /* Find the channel */
    boost::ptr_map<std::string, CChannel>::const_iterator it = m_Channels.find(Channel);
    if(it == m_Channels.end())
    {
        Sender->SendNumericReply(ERR_NOSUCHCHANNEL) % Channel;
        return;
    }

    /* Return the current topic of this channel */
    if(it->second->GetTopic().empty())
        Sender->SendNumericReply(RPL_NOTOPIC) % Channel;
    else
        Sender->SendNumericReply(RPL_TOPIC) % Channel % it->second->GetTopic();
}

/**
 * RFC2812, 3.1.3 - User message
 *
 * We have preset user and host names, so we can ignore all parameters
 * here.
 *
 * Must only be used with CNetworkClient pointers!
 */
void
CIRCServer::ReceiveMessage_USER(CClient* Sender, const std::vector<std::string>& Parameters)
{
    TO_NETWORKCLIENT(Sender, NetSender);

    /* Ignore everything, this method can only success :-) */

    /* Modify and check the user state */
    CClient::UserState State = NetSender->GetUserState();
    if(!_IsUserRegistered(State))
    {
        /* We have not yet registered, so store this and send the welcome messages if the NICK message has also been sent. */
        State.HasSentUserMessage = true;
        NetSender->SetUserState(State);

        if(State.HasSentNickMessage)
            _WelcomeClient(NetSender);
    }
}

/**
 * RFC2812, 3.4.3 - Version message
 *
 * Probably not really required, but we want our credits! :-)
 */
void
CIRCServer::ReceiveMessage_VERSION(CClient* Sender, const std::vector<std::string>& Parameters)
{
    Sender->SendNumericReply(RPL_VERSION) % m_Configuration.GetName();
}

void
CIRCServer::Shutdown()
{
    /* For the acceptors, we can just clear the vector and the destructors will do all cleanup stuff */
    m_Acceptors.clear();

    /* For the network clients, we first have to call CNetworkClient::Shutdown for each client to stop some
       objects from holding a shared_ptr to the client. */
    for(std::set< boost::shared_ptr<CNetworkClient> >::iterator it = m_NetworkClients.begin(); it != m_NetworkClients.end(); ++it)
        (*it)->Shutdown();

    m_NetworkClients.clear();
}

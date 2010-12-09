/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

CVoteBot::CVoteBot(CIRCServer& IRCServer, const std::string& Nickname, const std::string& ConfigID)
    : CVirtualClient(IRCServer, Nickname), m_ConfigID(ConfigID), m_Timer(IRCServer.GetIOService())
{
    _Reset();
}

void
CVoteBot::_CheckVotes()
{
    if(m_VoteCount == m_Votes.size())
    {
        /* All votes are in. Calculate the results. */
        boost::shared_array<size_t> Results(new size_t[m_Options.size()]);
        memset(Results.get(), 0, m_Options.size() * sizeof(size_t));

        for(std::map<CClient*, CVoteBot::VoteParameters>::const_iterator it = m_Votes.begin(); it != m_Votes.end(); ++it)
        {
            ++Results[it->second.Option];
            it->first->SendPrivateMessage(this, boost::str(boost::format("All votes are in. The results are available in #%s.") % m_Channel->GetName()));
        }

        _SendToChannel("The vote is over. Here are the results!");
        _SendToChannel(std::string("Question: ").append(m_Question));
        _SendToChannel("Answers:");

        for(size_t i = 0; i < m_Options.size(); ++i)
            _SendToChannel(boost::str(boost::format("   %s - %u votes") % m_Options[i] % Results[i]));

        _SendToChannel(boost::str(boost::format("Total number of votes: %u") % m_Votes.size()));
        _Reset();
    }
}

void
CVoteBot::_ReceiveCommand_CANCEL(CClient* Sender)
{
    if(m_CurrentAdminNickname.empty())
    {
        Sender->SendPrivateMessage(this, "There is nothing to cancel.");
        return;
    }

    if(m_VoteStarted)
    {
        /* The channel should be informed when a running vote is canceled. */
        _SendToChannel(std::string("The running vote has been canceled by ").append(Sender->GetNickname()));
    }

    _Reset();
    Sender->SendPrivateMessage(this, "Everything has been reset.");
}

void
CVoteBot::_ReceiveCommand_HELP(CClient* Sender)
{
    Sender->SendNotice(this, "***** VoteBot Help *****");
    Sender->SendNotice(this, "VoteBot enables all users on a channel to secretly vote on a question.");
    Sender->SendNotice(this, boost::str(boost::format("This VoteBot is responsible for the channel #%s.") % m_Channel->GetName()));
    Sender->SendNotice(this, "");
    Sender->SendNotice(this, "As an administrator of this VoteBot, you can set up the questions.");
    Sender->SendNotice(this, "Just type \"NEW\" and I will ask you about your question and the possible vote options.");
    Sender->SendNotice(this, boost::str(boost::format("The \"%s\" option will automatically be added to the available options.") % m_AbstentionTranslation));
    Sender->SendNotice(this, "");
    Sender->SendNotice(this, "When you're done, type \"START\" and I will put this question to all channel members in private messages.");
    Sender->SendNotice(this, boost::str(boost::format("They have %u minutes to answer, otherwise their vote will be counted as \"%s\".") % m_TimeLimitInMinutes % m_AbstentionTranslation));
    Sender->SendNotice(this, "");
    Sender->SendNotice(this, "You can always cancel the question setup and even the running vote by typing \"CANCEL\".");
    Sender->SendNotice(this, "***** End of Help *****");
}

void
CVoteBot::_ReceiveCommand_NEW(CClient* Sender)
{
    if(m_VoteStarted)
    {
        Sender->SendPrivateMessage(this, "A vote is already running. You have to cancel it first if you want to prepare a new one.");
        return;
    }

    if(!m_Question.empty())
    {
        Sender->SendPrivateMessage(this, "A vote is currently being prepared. You have to cancel it first if you want to prepare a new one.");
        return;
    }

    Sender->SendPrivateMessage(this, boost::str(boost::format("Please enter the question you want to vote on in #%s.") % m_Channel->GetName()));
    m_Options.push_back(m_AbstentionTranslation);

    /* Lock the VoteBot administrator access to this administrator and indicate that we're waiting for the question by doing this */
    m_CurrentAdminNickname = Sender->GetNickname();
}

void
CVoteBot::_ReceiveCommand_START(CClient* Sender)
{
    if(m_Question.empty())
    {
        Sender->SendPrivateMessage(this, "Please enter a question first. Type \"HELP\" for more information.");
        return;
    }

    /* Two options plus abstention are the minimum */
    if(m_Options.size() < 3)
    {
        Sender->SendPrivateMessage(this, "Please enter at least two voting options. Type \"HELP\" for more information.");
        return;
    }

    /* Save a copy of the currently present network clients, so that no one else can vote.
       If a client leaves during the vote, he is also excluded (see CVoteBot::SendIRCMessage). */
    const std::set<CClient*>& Clients = m_Channel->GetClients();
    for(std::set<CClient*>::const_iterator it = Clients.begin(); it != Clients.end(); ++it)
    {
        if((*it)->IsNetworkClient())
        {
            /* Preselect the "Abstention" option for the client.
               If he casts his vote, it is simply overwritten. */
            CVoteBot::VoteParameters VoteParameter = {0};
            m_Votes.insert(std::make_pair(*it, VoteParameter));

            /* Inform him about the question and the possible options.
               Deliberately use private messages instead of notices as latter ones can appear in channels when using clients like ChatZilla. */
            (*it)->SendPrivateMessage(this, std::string(m_CurrentAdminNickname).append(" has set up a vote and I want your opinion."));
            (*it)->SendPrivateMessage(this, std::string("Question: ").append(m_Question));
            (*it)->SendPrivateMessage(this, "Possible options:");

            for(size_t i = 0; i < m_Options.size(); ++i)
                (*it)->SendPrivateMessage(this, boost::str(boost::format("   %u - %s") % i % m_Options[i]));

            (*it)->SendPrivateMessage(this, "Please send me the number of your option.");
            (*it)->SendPrivateMessage(this, boost::str(boost::format("If you don't answer within %u minutes, your vote will be counted as \"%s\".") % m_TimeLimitInMinutes % m_AbstentionTranslation));
        }
    }

    if(m_Votes.empty())
    {
        Sender->SendPrivateMessage(this, "The channel has no members.");
        return;
    }

    /* Finally report this to the channel as well */
    _SendToChannel(std::string(m_CurrentAdminNickname).append(" has set up a vote and I'm asking all of you in private messages now."));

    /* The vote process officially starts... now! */
    m_Timer.expires_from_now(boost::posix_time::minutes(m_TimeLimitInMinutes));
    m_Timer.async_wait(boost::bind(&CVoteBot::_VoteDeadline, this, boost::asio::placeholders::error));
    m_VoteStarted = true;
}

void
CVoteBot::_ReceiveVote(CClient* Sender, const std::string& PrivateMessage)
{
    /* Check if the user is even allowed to vote */
    std::map<CClient*, CVoteBot::VoteParameters>::iterator it = m_Votes.find(Sender);
    if(it == m_Votes.end())
    {
        Sender->SendPrivateMessage(this, "You're not allowed to participate in this vote.");
        return;
    }

    /* Convert the vote into a 1-byte number */
    std::istringstream VoteStream(PrivateMessage);
    size_t Number;

    if(PrivateMessage.find_first_not_of("0123456789") != std::string::npos)
    {
        Sender->SendPrivateMessage(this, "Please enter a number.");
        return;
    }

    VoteStream >> Number;
    if(Number >= m_Options.size())
    {
        Sender->SendPrivateMessage(this, "This number is out of range.");
        return;
    }

    /* Cast the vote */
    it->second.Option = Number;

    if(it->second.HasVoted)
    {
        Sender->SendPrivateMessage(this, "Your vote has been changed. You can still change your decision again as long as the others are not yet done.");
    }
    else
    {
        it->second.HasVoted = true;
        ++m_VoteCount;
        Sender->SendPrivateMessage(this, "Your vote has been cast. You can still change your decision as long as the others are not yet done.");
    }

    _CheckVotes();
}

void
CVoteBot::_Reset()
{
    m_CurrentAdminNickname.clear();
    m_Options.clear();
    m_Question.clear();
    m_Timer.cancel();
    m_VoteCount = 0;
    m_Votes.clear();
    m_VoteStarted = false;
}

void
CVoteBot::_SendToChannel(const std::string& Message)
{
    std::vector<std::string> Parameters;
    Parameters.push_back(std::string("#").append(m_Channel->GetName()));
    Parameters.push_back(Message);

    m_IRCServer.ReceiveMessage_PRIVMSG(this, Parameters);
}

void
CVoteBot::_VoteDeadline(const boost::system::error_code& ErrorCode)
{
    if(ErrorCode != boost::asio::error::operation_aborted)
    {
        /* The deadline has expired, so our vote ends here. */
        m_VoteCount = m_Votes.size();
        _CheckVotes();
    }
}

bool
CVoteBot::Init()
{
    /* Load the configuration of this particular VoteBot instance */
    std::string ConfigFile(m_IRCServer.GetConfiguration().GetConfigPath());
    ConfigFile.append(boost::str(boost::format(VOTEBOT_INDIVIDUAL_FILE) % m_ConfigID));

    if(!std::ifstream(ConfigFile.c_str()))
    {
        /* Assume the user has disabled this VoteBot deliberately */
        Info("VoteBot \"%s\" is disabled, because the configuration file does not exist.\n", m_ConfigID.c_str());
        return false;
    }

    std::vector<std::string> Admins;
    std::string ChannelName;
    boost::program_options::options_description ConfigOptions;
    ConfigOptions.add_options()
        ("abstention_translation", boost::program_options::value<std::string>(&m_AbstentionTranslation)->default_value("Abstention"))
        ("admins", boost::program_options::value< std::vector<std::string> >(&Admins)->default_value(std::vector<std::string>(), ""))
        ("channel", boost::program_options::value<std::string>(&ChannelName)->default_value(""))
        ("timelimit", boost::program_options::value<unsigned int>(&m_TimeLimitInMinutes)->default_value(0));

    boost::program_options::variables_map TemporaryVariablesMap;
    boost::program_options::store(boost::program_options::parse_config_file<char>(ConfigFile.c_str(), ConfigOptions), TemporaryVariablesMap);
    boost::program_options::notify(TemporaryVariablesMap);

    /* Sanity checks */
    if(m_AbstentionTranslation.empty())
        BOOST_THROW_EXCEPTION(Error("The abstention_translation value may not be empty!") << boost::errinfo_file_name(ConfigFile));

    if(Admins.empty())
        BOOST_THROW_EXCEPTION(Error("You have to set at least one administrator for this VoteBot!") << boost::errinfo_file_name(ConfigFile));

    if(ChannelName.empty())
        BOOST_THROW_EXCEPTION(Error("You have to set the channel name for this VoteBot!") << boost::errinfo_file_name(ConfigFile));

    if(m_TimeLimitInMinutes == 0)
        BOOST_THROW_EXCEPTION(Error("You have to set a time limit for this VoteBot!") << boost::errinfo_file_name(ConfigFile));

    /* Does this channel even exist? */
    std::string ChannelNameLowercased(ChannelName);
    std::transform(ChannelNameLowercased.begin(), ChannelNameLowercased.end(), ChannelNameLowercased.begin(), tolower);

    const boost::ptr_map<std::string, CChannel>& Channels = m_IRCServer.GetChannels();
    boost::ptr_map<std::string, CChannel>::const_iterator ChannelIt = Channels.find(ChannelNameLowercased);
    if(ChannelIt == Channels.end())
        BOOST_THROW_EXCEPTION(Error("This VoteBot configuration contains an invalid channel name!") << boost::errinfo_file_name(ConfigFile) << ChannelName_Info(ChannelName));

    m_Channel = const_cast<CChannel*>(ChannelIt->second);

    /* Lowercase the admin users and put them into a set for easier searching */
    for(std::vector<std::string>::iterator AdminIt = Admins.begin(); AdminIt != Admins.end(); ++AdminIt)
    {
        std::transform(AdminIt->begin(), AdminIt->end(), AdminIt->begin(), tolower);
        m_AdminUsers.insert(*AdminIt);
    }

    Info("VoteBot \"%s\" is enabled.\n", m_ConfigID.c_str());
    return true;
}

void
CVoteBot::PostInit()
{
    /* Join our channel */
    std::vector<std::string> Parameters;
    Parameters.push_back(m_Channel->GetName());
    m_IRCServer.ReceiveMessage_JOIN(this, Parameters);
}

void
CVoteBot::SendIRCMessage(const std::string& Message)
{
    /* All clients that leave during the vote process need to be removed from m_Votes */
    if(m_VoteStarted)
    {
        /* All received messages should begin with a prefix */
        assert(Message[0] == ':');

        /* Extract the lowercased sender name.
           We make use of the fact that the user name is always the lowercased sender name. */
        size_t Offset = Message.find_first_of('!');
        assert(Offset != std::string::npos);
        ++Offset;
        size_t Position = Message.find_first_of('@', Offset);
        assert(Position != std::string::npos);
        std::string SenderNameLowercased(Message.substr(Offset, Position - Offset));

        /* Extract the command */
        Offset = Message.find_first_of(' ', Position);
        assert(Offset != std::string::npos);
        ++Offset;
        Position = Message.find_first_of(' ', Offset);
        std::string Command(Message.substr(Offset, Position - Offset));

        if(Command == "PART" || Command == "QUIT")
        {
            /* The client has surely left the vote channel in both cases.
               This VoteBot has only joined the vote channel, so it doesn't receive PART messages of other channels. */

            /* Get the respective CClient pointer */
            const std::map<std::string, CClient*>& Nicknames = m_IRCServer.GetNicknames();
            std::map<std::string, CClient*>::const_iterator ClientIt = Nicknames.find(SenderNameLowercased);
            assert(ClientIt != Nicknames.end());

            /* Find and remove him if he was one of the allowed clients to vote */
            std::map<CClient*, CVoteBot::VoteParameters>::iterator VoteIt = m_Votes.find(ClientIt->second);
            if(VoteIt != m_Votes.end())
            {
                if(VoteIt->second.HasVoted)
                    --m_VoteCount;

                m_Votes.erase(VoteIt);
                _CheckVotes();
            }
        }
    }
}

void
CVoteBot::SendPrivateMessage(CClient* Sender, const std::string& PrivateMessage)
{
    if(!Sender->GetUserState().IsIdentified)
    {
        Sender->SendPrivateMessage(this, "Please identify first!");
        return;
    }

    /* Is this user an administrator of this VoteBot? */
    if(m_AdminUsers.find(Sender->GetNicknameLowercased()) != m_AdminUsers.end())
    {
        /* VoteBot commands are case-insensitive */
        std::string Command(PrivateMessage);
        std::transform(Command.begin(), Command.end(), Command.begin(), toupper);

        /* These commands can be run by multiple administrators */
        if(Command == "CANCEL")
        {
            _ReceiveCommand_CANCEL(Sender);
            return;
        }
        else if(Command == "HELP")
        {
            _ReceiveCommand_HELP(Sender);
            return;
        }

        /* The following stuff must be handled by a single administrator */
        if(!m_CurrentAdminNickname.empty() && m_CurrentAdminNickname != Sender->GetNickname())
        {
            Sender->SendPrivateMessage(this, boost::str(boost::format("This VoteBot is currently being used by %s.") % m_CurrentAdminNickname));
            Sender->SendPrivateMessage(this, "You have to type \"CANCEL\" if you want to cancel all running actions and use it yourself.");
            return;
        }

        if(Command == "NEW")
        {
            _ReceiveCommand_NEW(Sender);
            return;
        }
        else if(Command == "START")
        {
            _ReceiveCommand_START(Sender);
            return;
        }

        /* If we're here, the administrator may have sent values for one of the commands. */
        if(!m_VoteStarted)
        {
            if(m_Question.empty() && !m_CurrentAdminNickname.empty())
            {
                /* The admin has just sent a question for a new vote */
                m_Question = PrivateMessage;
                Sender->SendPrivateMessage(this, boost::str(boost::format("Please enter a vote option now. The \"%s\" option will automatically be added to the available options.") % m_AbstentionTranslation));
            }
            else if(!m_Question.empty())
            {
                /* The admin has just sent a voting option for the current vote */
                m_Options.push_back(PrivateMessage);
                Sender->SendPrivateMessage(this, "This option has been added. Enter another one or \"START\" to start the vote.");
            }
            else
            {
                Sender->SendPrivateMessage(this, "Invalid command. Type \"HELP\" for more information.");
            }

            return;
        }
    }

    if(m_VoteStarted)
    {
        _ReceiveVote(Sender, PrivateMessage);
    }
    else
    {
        /* The non-administrator user tried to contact this bot */
        Sender->SendPrivateMessage(this, boost::str(boost::format("I'm VoteBot for #%s, and you're not my administrator :-P") % m_Channel->GetName()));
    }
}

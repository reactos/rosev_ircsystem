/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

CNickServ::CNickServ(CIRCServer& IRCServer)
    : CVirtualClient(IRCServer, "NickServ")
{
    m_CommandHandlers = boost::assign::map_list_of
        ("HELP", &CNickServ::_ReceiveCommand_HELP)
        ("IDENTIFY", &CNickServ::_ReceiveCommand_IDENTIFY);
}

bool
CNickServ::Init()
{
    /* Load the NickServ-specific configuration (just the extra Users file for now) */
    std::string ConfigFile(m_IRCServer.GetConfiguration().GetConfigPath());
    ConfigFile.append(NICKSERV_USERS_FILE);

    boost::program_options::parsed_options UsersParsedOptions(boost::program_options::parse_config_file<char>(ConfigFile.c_str(), NULL, true));
    for(std::vector< boost::program_options::basic_option<char> >::const_iterator it = UsersParsedOptions.options.begin(); it != UsersParsedOptions.options.end(); ++it)
    {
        /* Convert the hexadecimal string passhash into a binary one */
        const std::string& HexPasshash = it->value[0];
        boost::array<char, SHA512_DIGEST_LENGTH> BinaryPasshash;

        if(HexPasshash.length() != 2 * SHA512_DIGEST_LENGTH)
            BOOST_THROW_EXCEPTION(Error("Length of a passhash must be 128 characters!") << Passhash_Info(HexPasshash));

        for(int i = 0; i < SHA512_DIGEST_LENGTH; ++i)
            BinaryPasshash[i] = static_cast<char>(strtol(HexPasshash.substr(2 * i, 2).c_str(), NULL, 16));

        m_UserPasshashMap.insert(std::make_pair(it->string_key, BinaryPasshash));
    }

    Info("NickServ is enabled.\n");
    return true;
}

void
CNickServ::_ReceiveCommand_HELP(CNetworkClient* Sender, const std::vector<std::string>& Parameters)
{
    if(Parameters.empty())
    {
        /* General help message */
        Sender->SendNotice(this, "***** NickServ Help *****");
        Sender->SendNotice(this, "General Help:");
        Sender->SendNotice(this, "");
        Sender->SendNotice(this, "NickServ allows you to identify with your nickname using the given password.");
        Sender->SendNotice(this, "You cannot join a channel before having identified!");
        Sender->SendNotice(this, "");
        Sender->SendNotice(this, "For more information about a command, type:");
        Sender->SendNotice(this, "/NS HELP <command>");
        Sender->SendNotice(this, "");
        Sender->SendNotice(this, "This NickServ supports the following commands:");
        Sender->SendNotice(this, "IDENTIFY    Identifies using a password.");
        Sender->SendNotice(this, "***** End of Help *****");
    }
    else
    {
        std::string ParameterUppercased(Parameters[0]);
        std::transform(ParameterUppercased.begin(), ParameterUppercased.end(), ParameterUppercased.begin(), toupper);

        if(ParameterUppercased == "IDENTIFY")
        {
            Sender->SendNotice(this, "***** NickServ Help *****");
            Sender->SendNotice(this, "Help for IDENTIFY:");
            Sender->SendNotice(this, "");
            Sender->SendNotice(this, "IDENTIFY identifies you with the IRC Server, so that you can join channels.");
            Sender->SendNotice(this, "If you don't identify, you will be disconnected after " BOOST_PP_STRINGIZE(IDENTIFY_TIMEOUT) " seconds.");
            Sender->SendNotice(this, "");
            Sender->SendNotice(this, "Syntax: IDENTIFY <password>");
            Sender->SendNotice(this, "");
            Sender->SendNotice(this, "Example:");
            Sender->SendNotice(this, "   /NS IDENTIFY ThisIsMyRandomPassword");
            Sender->SendNotice(this, "***** End of Help *****");
        }
    }
}

void
CNickServ::_ReceiveCommand_IDENTIFY(CNetworkClient* Sender, const std::vector<std::string>& Parameters)
{
    if(Sender->GetUserState().IsIdentified)
    {
        Sender->SendNotice(this, "You are already identified!");
        return;
    }

    if(Parameters.empty())
    {
        Sender->SendNotice(this, "You need to specify your password as the first parameter!");
        return;
    }

    /* Find the user */
    std::map< std::string, boost::array<char, SHA512_DIGEST_LENGTH> >::const_iterator it = m_UserPasshashMap.find(Sender->GetNickname());

    if(it == m_UserPasshashMap.end())
    {
        Sender->SendNotice(this, "No password is known for this nickname!");
        Sender->SendNotice(this, "Ensure that your nickname is spelled correctly (they are case-sensitive).");
        return;
    }

    /* Build the SHA512 hash of the given password */
    unsigned char Passhash[SHA512_DIGEST_LENGTH];
    SHA512(reinterpret_cast<const unsigned char*>(Parameters[0].c_str()), Parameters[0].length(), Passhash);

    /* Compare the hashes */
    if(memcmp(it->second.data(), Passhash, SHA512_DIGEST_LENGTH))
    {
        Sender->SendNotice(this, "Invalid password!");
        Sender->SendNotice(this, "Ensure that your password is spelled correctly (it is case-sensitive).");
        return;
    }

    /* The password is correct, so this user has successfully identified! */
    CClient::UserState State = Sender->GetUserState();
    State.IsIdentified = true;
    Sender->SetUserState(State);

    Sender->SendNotice(this, "You have successfully identified!");
}

void
CNickServ::SendPrivateMessage(CClient* Sender, const std::string& PrivateMessage)
{
    /* NickServ must only be used by network clients, so convert the pointer */
    TO_NETWORKCLIENT(Sender, NetSender);

    /* Extract the command */
    size_t Position = PrivateMessage.find_first_of(' ');
    std::string Command(PrivateMessage.substr(0, Position));
    std::transform(Command.begin(), Command.end(), Command.begin(), toupper);

    /* Extract all parameters into the vector */
    std::vector<std::string> Parameters;
    while(Position != std::string::npos)
    {
        size_t Offset = Position + 1;

        /* Find the end of this parameter */
        Position = PrivateMessage.find_first_of(' ', Offset);
        Parameters.push_back(PrivateMessage.substr(Offset, Position - Offset));
    }

    /* Find a command handler */
    std::map<std::string, void (CNickServ::*)(CNetworkClient*, const std::vector<std::string>&)>::const_iterator it = m_CommandHandlers.find(Command);
    if(it == m_CommandHandlers.end())
    {
        /* Show an advice */
        Sender->SendNotice(this, "Invalid command. Use /NS HELP for a command listing.");
    }
    else
    {
        /* Call the handler */
        (this->*(it->second))(NetSender, Parameters);
    }
}

/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010-2011 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

CNickServ::CNickServ(CIRCServer& IRCServer)
    : CVirtualClient(IRCServer, "NickServ")
{
    m_CommandHandlers = boost::assign::map_list_of
        ("GHOST", &CNickServ::_ReceiveCommand_GHOST)
        ("HELP", &CNickServ::_ReceiveCommand_HELP)
        ("IDENTIFY", &CNickServ::_ReceiveCommand_IDENTIFY);
}

bool
CNickServ::Init()
{
    Info("NickServ is enabled.\n");
    return true;
}

void
CNickServ::_ReceiveCommand_GHOST(CNetworkClient* Sender, const std::vector<std::string>& Parameters)
{
    if(Parameters.size() != 2)
    {
        Sender->SendNotice(this, "You need to supply the nickname and its password!");
        return;
    }

    /* Check if this nickname is online */
    std::string NicknameLowercased(Parameters[0]);
    std::transform(NicknameLowercased.begin(), NicknameLowercased.end(), NicknameLowercased.begin(), tolower);

    const std::map<std::string, CClient*>& Nicknames = m_IRCServer.GetNicknames();
    std::map<std::string, CClient*>::const_iterator it = Nicknames.find(NicknameLowercased);
    if(it == Nicknames.end())
    {
        Sender->SendNotice(this, "This nickname is not online!");
        return;
    }

    /* Additional sanity checks */
    if(!it->second->IsNetworkClient() || it->second == Sender)
    {
        Sender->SendNotice(this, "You cannot ghost this nickname!");
        return;
    }

    if(!_VerifyCredentials(Sender, NicknameLowercased, Parameters[1]))
        return;

    /* The password is correct, so disconnect the user */
    m_IRCServer.DisconnectNetworkClient(static_cast<CNetworkClient*>(it->second), "Disconnected by GHOST command");
    Sender->SendNotice(this, "The nickname has been ghosted!");
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
        Sender->SendNotice(this, "GHOST       Reclaims a used nickname.");
        Sender->SendNotice(this, "IDENTIFY    Identifies using a password.");
        Sender->SendNotice(this, "***** End of Help *****");
    }
    else
    {
        std::string ParameterUppercased(Parameters[0]);
        std::transform(ParameterUppercased.begin(), ParameterUppercased.end(), ParameterUppercased.begin(), toupper);

        if(ParameterUppercased == "GHOST")
        {
            Sender->SendNotice(this, "***** NickServ Help *****");
            Sender->SendNotice(this, "Help for GHOST:");
            Sender->SendNotice(this, "");
            Sender->SendNotice(this, "GHOST reclaims a lost nickname by disconnecting its session.");
            Sender->SendNotice(this, "This can be useful if you were unexpectedly disconected or");
            Sender->SendNotice(this, "someone else is using your nickname.");
            Sender->SendNotice(this, "");
            Sender->SendNotice(this, "Syntax: GHOST <nickname> <password>");
            Sender->SendNotice(this, "");
            Sender->SendNotice(this, "Example:");
            Sender->SendNotice(this, "   /NS GHOST Arthur ThisIsMyRandomPassword");
            Sender->SendNotice(this, "***** End of Help *****");
        }
        else if(ParameterUppercased == "IDENTIFY")
        {
            Sender->SendNotice(this, "***** NickServ Help *****");
            Sender->SendNotice(this, "Help for IDENTIFY:");
            Sender->SendNotice(this, "");
            Sender->SendNotice(this, "IDENTIFY identifies you with the IRC Server, so that you can join channels.");
            Sender->SendNotice(this, "If you don't identify, you will be disconnected after " BOOST_PP_STRINGIZE(IDENTIFY_TIMEOUT) " seconds.");
            Sender->SendNotice(this, "");
            Sender->SendNotice(this, "Syntax: IDENTIFY <password>");
            Sender->SendNotice(this, "");
            Sender->SendNotice(this, "Like on FreeNode, you can also supply the password as the second parameter.");
            Sender->SendNotice(this, "Note that the first parameter is ignored then as you can only identify for");
            Sender->SendNotice(this, "your current nickname.");
            Sender->SendNotice(this, "");
            Sender->SendNotice(this, "Syntax: IDENTIFY <ignored> <password>");
            Sender->SendNotice(this, "");
            Sender->SendNotice(this, "Example:");
            Sender->SendNotice(this, "   /NS IDENTIFY ThisIsMyRandomPassword");
            Sender->SendNotice(this, "   /NS IDENTIFY BlaBlaBla ThisIsMyRandomPassword");
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

    /* Support FreeNode's alternative /NS IDENTIFY <nickname> <password> syntax instead of just
       /NS IDENTIFY <password>, but only verify the supplied password against the current nickname. */
    const std::string& Password = (Parameters.size() == 1 ? Parameters[0] : Parameters[1]);
    if(!_VerifyCredentials(Sender, Sender->GetNicknameLowercased(), Password))
        return;

    /* The password is correct, so this user has successfully identified! */
    CClient::UserState State = Sender->GetUserState();
    State.IsIdentified = true;
    Sender->SetUserState(State);

    Sender->SendNotice(this, "You have successfully identified!");
}

bool
CNickServ::_VerifyCredentials(CNetworkClient* Sender, const std::string& NicknameLowercased, const std::string& Password)
{
    /* Find the user */
    const std::map< std::string, boost::array<char, SHA512_DIGEST_LENGTH> >& UserPasshashMap = m_IRCServer.GetConfiguration().GetUserPasshashMap();
    std::map< std::string, boost::array<char, SHA512_DIGEST_LENGTH> >::const_iterator it = UserPasshashMap.find(NicknameLowercased);

    if(it == UserPasshashMap.end())
    {
        Sender->SendNotice(this, "No password is known for this nickname!");
        return false;
    }

    /* Build the SHA512 hash of the given password */
    unsigned char Passhash[SHA512_DIGEST_LENGTH];
    SHA512(reinterpret_cast<const unsigned char*>(Password.c_str()), Password.length(), Passhash);

    /* Compare the hashes */
    if(memcmp(it->second.data(), Passhash, SHA512_DIGEST_LENGTH))
    {
        Sender->SendNotice(this, "Invalid password!");
        Sender->SendNotice(this, "Ensure that your password is spelled correctly (it is case-sensitive).");
        return false;
    }

    return true;
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

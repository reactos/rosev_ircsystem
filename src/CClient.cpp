/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>
#include <version.h>

static const std::map<short, std::string> NumericReplyMessages = boost::assign::map_list_of
    (RPL_WELCOME, ":Welcome to the %s Internet Relay Chat Network %s")
    (RPL_YOURHOST, ":Your host is %s, running version " VERSION_ID)
    (RPL_CREATED, ":This server was created " VERSION_DATE)
    (RPL_MYINFO, "%s " VERSION_ID " iv i")                  /* Some dummy modes for COMPATIBILITY plus the real voice user mode. */
    (RPL_NOTOPIC, "#%s :No topic is set")
    (RPL_TOPIC, "#%s :%s")
    (RPL_VERSION, VERSION_ID ". %s :" PRODUCT_NAME)
    (RPL_NAMREPLY, "= #%s :%s")                             /* We limit us to public channels here */
    (RPL_ENDOFNAMES, "#%s :End of NAMES list")
    (RPL_INFO, ":%s")
    (RPL_MOTD, ":- %s")
    (RPL_ENDOFINFO, ":End of INFO list")
    (RPL_MOTDSTART, ":- %s Message of the day - ")
    (RPL_ENDOFMOTD, ":End of MOTD command.")
    (ERR_NOSUCHNICK, "%s :No such nick/channel")
    (ERR_NOSUCHCHANNEL, "#%s :No such channel")
    (ERR_CANNOTSENDTOCHAN, "#%s :Cannot send to channel")
    (ERR_NORECIPIENT, ":No recipient given (%s)")
    (ERR_NOTEXTTOSEND, ":No text to send")
    (ERR_NONICKNAMEGIVEN, ":No nickname given")
    (ERR_ERRONEUSNICKNAME, "%s :Erroneous Nickname")
    (ERR_NICKNAMEINUSE, "%s :Nickname is already in use")
    (ERR_NOTONCHANNEL, "#%s :You're not on that channel")
    (ERR_NEEDMOREPARAMS, "%s :Not enough parameters");

/* CClient::CNumericReplyFormatter class */
CClient::CNumericReplyFormatter::CNumericReplyFormatter(CClient* Client, short Code)
    : m_Client(Client)
{
    /* A given client indicates that we shall collect the arguments passed with %, format
       the message string and later on pass it to CClient::SendIRCMessage.
       Otherwise this class just serves as a dummy for CVirtualClient implementations. */
    if(Client)
    {
        /* Get the format message */
        std::map<short, std::string>::const_iterator it = NumericReplyMessages.find(Code);
        assert(it != NumericReplyMessages.end());

        /* RFC2812, 2.4 - "The numeric reply MUST be sent as one message consisting of the sender prefix, the three-digit numeric, and the target of the reply." */
        m_Format.parse(std::string(":%s %03d %s ").append(it->second));
        m_Format % Client->GetIRCServer().GetConfiguration().GetName() % Code % Client->GetNicknameAsTarget();
        _CheckRemainingArguments();
    }
}

void
CClient::CNumericReplyFormatter::_CheckRemainingArguments()
{
    if(m_Format.remaining_args() == 0)
    {
        /* We're done, call SendIRCMessage with the formatted message. */
        m_Client->SendIRCMessage(boost::str(m_Format));
    }
}

/* CClient class */
CClient::CClient(CIRCServer& IRCServer)
    : m_IRCServer(IRCServer)
{
}

void
CClient::AddJoinedChannel(CChannel* Channel)
{
    m_JoinedChannels.insert(Channel);
}

const char*
CClient::GetNicknameAsTarget() const
{
    /* I've found no specification about this, but from capturing network traffic, you see that
       IRC servers use an asterisk as the target when no nickname has been set yet. */
    if(m_Nickname.empty())
        return "*";
    else
        return m_Nickname.c_str();
}

void
CClient::RemoveJoinedChannel(CChannel* Channel)
{
    m_JoinedChannels.erase(Channel);
}

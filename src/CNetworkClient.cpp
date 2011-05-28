/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010-2011 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

/* ROSEV-SPECIFIC: Only the following IRC commands are supported.
   Don't panic, the second template value is just a method pointer to one of the CIRCServer::ReceiveMessage_* functions ;-) */
static const std::map<std::string, void (CIRCServer::*)(CClient*, const std::vector<std::string>&)> NetworkClientCommandHandlers = boost::assign::map_list_of
    ("INFO", &CIRCServer::ReceiveMessage_INFO)
    ("JOIN", &CIRCServer::ReceiveMessage_JOIN)
    /* "MODE" isn't supported for real, though we send dummy supported modes and a first MODE response for COMPATIBILITY. */
    ("MOTD", &CIRCServer::ReceiveMessage_MOTD)
    ("NAMES", &CIRCServer::ReceiveMessage_NAMES)
    ("NICK", &CIRCServer::ReceiveMessage_NICK)
    /* "NOTICE" was deliberately left out here, we don't want to receive notices from network clients. */
    ("NS", &CIRCServer::ReceiveMessage_NS)
    ("PART", &CIRCServer::ReceiveMessage_PART)
    ("PING", &CIRCServer::ReceiveMessage_PING)
    ("PONG", &CIRCServer::ReceiveMessage_PONG)
    ("PRIVMSG", &CIRCServer::ReceiveMessage_PRIVMSG)
    ("QUIT", &CIRCServer::ReceiveMessage_QUIT)
    ("TOPIC", &CIRCServer::ReceiveMessage_TOPIC)
    ("USER", &CIRCServer::ReceiveMessage_USER)
    ("VERSION", &CIRCServer::ReceiveMessage_VERSION);

CNetworkClient::CNetworkClient(boost::asio::io_service& IOService, CIRCServer& IRCServer)
    : CClient(IRCServer), m_InputBufferPointer(m_InputBuffer), m_SendBufferSize(0), m_ShutdownCompleted(false), m_Timer(IOService)
{
    m_UserState.HasSentNickMessage = false;
    m_UserState.HasSentUserMessage = false;
    m_UserState.IsIdentified = false;
}

void
CNetworkClient::_HandleNewData(const boost::system::error_code& ErrorCode, size_t BytesTransferred)
{
    if(ErrorCode)
    {
        m_IRCServer.DisconnectNetworkClient(this, ErrorCode.message());
        return;
    }

    /* The new data has been concatenated to the remainder of the last call.
       NUL-terminate the input so that we can work with it. */
    m_InputBufferPointer[BytesTransferred] = 0;

    /* The input buffer is organized so that new messages always begin at the first character (if they are valid) */
    char* IRCMessage = m_InputBuffer;

    /* p looks for message endings, so it can start after the remainder of the last call.
       (if the remainder would have had line endings, it would have been processed and wouldn't be here) */
    char* p = m_InputBufferPointer;

    while(*p)
    {
        /* COMPATIBILITY: Actually, we would only need to check for CRLF line endings here...
           But even popular IRC clients like ChatZilla or mIRC don't follow RFC2812 and terminate all messages with LF.
           As an example, X-Chat does it correctly and uses CRLF.

           So we have to look for both endings here :-( */
        unsigned char LineEndingLength = 0;
        if(*p == '\r' && *(p + 1) == '\n')
            LineEndingLength = 2;
        else if(*p == '\n')
            LineEndingLength = 1;

        if(LineEndingLength)
        {
            /* Discard all messages longer than IRC_MESSAGE_LENGTH. */
            if(p - IRCMessage + LineEndingLength > IRC_MESSAGE_LENGTH)
            {
                m_IRCServer.DisconnectNetworkClient(this, "Message too long");
                return;
            }

            /* We have a valid message, terminate it and pass it to the handling function. */
            *p = 0;
            _ReceiveMessage(IRCMessage);

            /* The last message could have resulted in a client disconnection.
               Check this and don't process any further commands if we were disconnected. */
            if(!GetSocket().is_open())
                return;

            /* Skip the line ending and begin the next message. */
            IRCMessage = p + LineEndingLength;

            /* In case of a CRLF line ending, we have to skip the LF in our walking pointer as well */
            if(LineEndingLength == 2)
                ++p;
        }

        ++p;
    }

    /* Messages split between several calls must be smaller than IRC_MESSAGE_LENGTH, otherwise they would have been processed above.
       Larger messages are invalid and need to be discarded.

       This also ensures that we don't run into buffer overflows in m_WorkBuffer.
       2 * IRC_MESSAGE_LENGTH is large enough to hold up to IRC_MESSAGE_LENGTH -1 from this call, IRC_MESSAGE_LENGTH from the next call
       and a final terminating NUL character. */
    if(p - IRCMessage >= IRC_MESSAGE_LENGTH)
    {
        m_IRCServer.DisconnectNetworkClient(this, "Message too long");
        return;
    }

    /* Copy the remainder to the beginning of m_InputBuffer for the next call. */
    memcpy(m_InputBuffer, IRCMessage, p - IRCMessage);

    /* m_InputBufferPointer needs to start after the remainder for the concatenation above. */
    m_InputBufferPointer = &m_InputBuffer[p - IRCMessage];

    _Receive();
}

void
CNetworkClient::_HandleSend(const boost::system::error_code& ErrorCode)
{
    if(ErrorCode)
    {
        m_IRCServer.DisconnectNetworkClient(this, ErrorCode.message());
        return;
    }

    /* Indicate that the send operation has completed and process the next data in the queue. */
    m_SendBufferSize = 0;
    _ProcessSendQueue();
}

void
CNetworkClient::_IdentifyDeadline(const boost::system::error_code& ErrorCode)
{
    if(ErrorCode != boost::asio::error::operation_aborted)
    {
        /* The client failed to identify within the given timeframe. */
        m_IRCServer.DisconnectNetworkClient(this, "Identify timeout: " BOOST_PP_STRINGIZE(IDENTIFY_TIMEOUT) " seconds");
    }
}

void
CNetworkClient::_NickDeadline(const boost::system::error_code& ErrorCode)
{
    if(ErrorCode != boost::asio::error::operation_aborted)
    {
        /* If we reach this point, the timer has not been canceled, which means that the
           client has not issued a NICK command within the given timeframe. */
        m_IRCServer.DisconnectNetworkClient(this, "Nick timeout: " BOOST_PP_STRINGIZE(REGISTRATION_TIMEOUT) " seconds");
    }
}

void
CNetworkClient::_PingDeadline(const boost::system::error_code& ErrorCode)
{
    if(ErrorCode != boost::asio::error::operation_aborted)
    {
        /* Time to ping this client to check if it's still active */
        SendIRCMessage(std::string("PING ").append(m_IRCServer.GetConfiguration().GetName()));
        m_Timer.expires_from_now(boost::posix_time::seconds(PING_TIMEOUT));
        m_Timer.async_wait(boost::bind(&CNetworkClient::_PongDeadline, shared_from_this(), boost::asio::placeholders::error));
    }
}

void
CNetworkClient::_PongDeadline(const boost::system::error_code& ErrorCode)
{
    if(ErrorCode != boost::asio::error::operation_aborted)
    {
        /* If we reach this point, the timer has not been canceled, which means that the
           client failed to respond to our PING within the given timeframe. */
        m_IRCServer.DisconnectNetworkClient(this, "Ping timeout: " BOOST_PP_STRINGIZE(PING_TIMEOUT) " seconds");
    }
}

void
CNetworkClient::_ProcessSendQueue()
{
    /* Check if there is data in the queue and if we're not currently sending any. */
    if(!m_SendQueue.empty() && !m_SendBufferSize)
    {
        /* Pop the next buffer from the queue and send it. */
        std::string& FrontBuffer = m_SendQueue.front();
        m_SendBufferSize = FrontBuffer.size();
        m_SendBuffer.reset(new char[m_SendBufferSize]);
        memcpy(m_SendBuffer.get(), FrontBuffer.c_str(), m_SendBufferSize);
        m_SendQueue.pop();

        _Send();
    }
}

/**
 * Does string processing on a received message from the IRC Client and forwards it to the
 * appropriate handler in CIRCServer.
 * This message format is only used by CNetworkClient, other (virtual) clients should call
 * the handlers in CIRCServer directly.
 */
void
CNetworkClient::_ReceiveMessage(const std::string& Message)
{
    /* Remove the prefix (if necessary) */
    size_t Offset = 0;

    if(Message[0] == ':')
    {
        /* RFC2812, 2.3 - "Clients SHOULD NOT use a prefix when sending a message;
           if they use one, the only valid prefix is the registered nickname
           associated with the client.
           We already store the nickname internally, so we can just drop the prefix here. */
        Offset = Message.find_first_of(' ');
        if(Offset == std::string::npos)
        {
            /* Invalid message */
            m_IRCServer.DisconnectNetworkClient(this, "Invalid message");
            return;
        }

        ++Offset;
    }

    /* Extract the command */
    size_t Position = Message.find_first_of(' ', Offset);
    std::string Command(Message.substr(Offset, Position - Offset));
    std::transform(Command.begin(), Command.end(), Command.begin(), toupper);

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
    std::map<std::string, void (CIRCServer::*)(CClient*, const std::vector<std::string>&)>::const_iterator it = NetworkClientCommandHandlers.find(Command);
    if(it != NetworkClientCommandHandlers.end())
    {
        /* Call the handler */
        (m_IRCServer.*(it->second))(this, Parameters);
    }
}

void
CNetworkClient::Init()
{
    /* The client has to register successfully within REGISTRATION_TIMEOUT seconds */
    m_Timer.expires_from_now(boost::posix_time::seconds(REGISTRATION_TIMEOUT));
    m_Timer.async_wait(boost::bind(&CNetworkClient::_NickDeadline, shared_from_this(), boost::asio::placeholders::error));

    _Receive();
}

void
CNetworkClient::RestartIdentifyTimer()
{
    m_Timer.cancel();
    m_Timer.expires_from_now(boost::posix_time::seconds(IDENTIFY_TIMEOUT));
    m_Timer.async_wait(boost::bind(&CNetworkClient::_IdentifyDeadline, shared_from_this(), boost::asio::placeholders::error));
}

void
CNetworkClient::RestartPingTimer()
{
    /* Set the timer to issue the next PING command in PING_INTERVAL seconds */
    m_Timer.cancel();
    m_Timer.expires_from_now(boost::posix_time::seconds(PING_INTERVAL));
    m_Timer.async_wait(boost::bind(&CNetworkClient::_PingDeadline, shared_from_this(), boost::asio::placeholders::error));
}

void
CNetworkClient::SendIRCMessage(const std::string& Message)
{
    m_SendQueue.push(std::string(Message).append("\r\n"));
    _ProcessSendQueue();
}

/**
 * Send a notice (RFC2812, 3.3.2) from a given client to this client.
 * You can also set Sender to NULL to use the server name as the sender.
 */
void
CNetworkClient::SendNotice(CClient* Sender, const std::string& Notice)
{
    /* For SendNotice, we support a NULL value for Sender, meaning that the server itself shall be the sender.
       This wouldn't make sense for other functions accepting CClient pointers. */
    const std::string& SenderPrefix = (Sender ? Sender->GetPrefix() : m_IRCServer.GetConfiguration().GetName());

    SendIRCMessage(boost::str(boost::format(":%s NOTICE %s :%s") % SenderPrefix % GetNicknameAsTarget() % Notice));
}

/**
 * Send one of the replies in RFC2812, 5.
 */
CClient::CNumericReplyFormatter
CNetworkClient::SendNumericReply(short Code)
{
    return CNumericReplyFormatter(this, Code);
}

/**
 * Send a private message (RFC2812, 3.3.1) from a given client to this client.
 */
void
CNetworkClient::SendPrivateMessage(CClient* Sender, const std::string& PrivateMessage)
{
    SendIRCMessage(boost::str(boost::format(":%s PRIVMSG %s :%s") % Sender->GetPrefix() % GetNicknameAsTarget() % PrivateMessage));
}

void
CNetworkClient::SetNickname(const std::string& Nickname, const std::string& NicknameLowercased)
{
    m_Nickname = Nickname;
    m_NicknameLowercased = NicknameLowercased;
    m_Prefix = std::string(Nickname).append("!").append(NicknameLowercased).append("@network");
}

void
CNetworkClient::SetUserState(const CClient::UserState& NewUserState)
{
    m_UserState = NewUserState;

    /* As the last call of SetUserState occurs when the user has identified, it is safe to stop the identify timer here */
    if(NewUserState.IsIdentified)
        RestartPingTimer();
}

void
CNetworkClient::Shutdown()
{
    /* This method has to kill all references to the shared_from_this() shared_ptrs we use throughout the class */
    m_Timer.cancel();

    boost::asio::ip::tcp::socket::lowest_layer_type& Socket = GetSocket();

    /* Use the second overload of shutdown() to ignore any errors while shutting down the socket
       (like endpoint being closed already). */
    boost::system::error_code ErrorCode;
    Socket.shutdown(boost::asio::socket_base::shutdown_both, ErrorCode);

    Socket.close();

    m_ShutdownCompleted = true;
}

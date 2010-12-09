/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

class CNetworkClient : public CClient, public boost::enable_shared_from_this<CNetworkClient>
{
public:
    virtual boost::asio::ip::tcp::socket::lowest_layer_type& GetSocket() = 0;
    bool HasCompletedShutdown() const { return m_ShutdownCompleted; }
    void Init();
    virtual bool IsInitialized() const = 0;
    bool IsNetworkClient() const { return true; }
    void RestartPingTimer();
    void SetNickname(const std::string& Nickname, const std::string& NicknameLowercased);
    void SetUserState(const CClient::UserState& NewUserState);
    void SendIRCMessage(const std::string& Message);
    void SendNotice(CClient* Sender, const std::string& Notice);
    CNumericReplyFormatter SendNumericReply(short Code);
    void SendPrivateMessage(CClient* Sender, const std::string& PrivateMessage);
    void Shutdown();
    void StartIdentifyTimer();

protected:
    char m_InputBuffer[2 * IRC_MESSAGE_LENGTH];
    char* m_InputBufferPointer;
    boost::shared_array<char> m_SendBuffer;
    size_t m_SendBufferSize;
    std::queue<std::string> m_SendQueue;
    bool m_ShutdownCompleted;
    boost::asio::deadline_timer m_Timer;         /** For user registration, then identification, then ping timeouts */

    CNetworkClient(boost::asio::io_service& IOService, CIRCServer& IRCServer);

    void _HandleNewData(const boost::system::error_code& ErrorCode, size_t BytesTransferred);
    void _HandleSend(const boost::system::error_code& ErrorCode);
    void _IdentifyDeadline(const boost::system::error_code& ErrorCode);
    void _NickDeadline(const boost::system::error_code& ErrorCode);
    void _PingDeadline(const boost::system::error_code& ErrorCode);
    void _PongDeadline(const boost::system::error_code& ErrorCode);
    void _ProcessSendQueue();
    virtual void _Receive() = 0;
    void _ReceiveMessage(const std::string& Message);
    virtual void _Send() = 0;
};

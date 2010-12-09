/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

class CIRCServer : private boost::noncopyable
{
public:
    CIRCServer(boost::asio::io_service& IOService, CConfiguration& Configuration);

    void AddVirtualClient(CVirtualClient* Client);
    void DisconnectNetworkClient(CNetworkClient* Client, const std::string& Reason);
    const boost::ptr_map<std::string, CChannel>& GetChannels() const { return m_Channels; }
    CConfiguration& GetConfiguration() const { return m_Configuration; }
    boost::asio::io_service& GetIOService() const { return m_IOService; }
    const std::map<std::string, CClient*>& GetNicknames() const { return m_Nicknames; }
    void Init();
    void ReceiveMessage_INFO(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_JOIN(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_MOTD(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_NAMES(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_NICK(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_NS(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_PART(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_PING(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_PONG(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_PRIVMSG(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_QUIT(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_TOPIC(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_USER(CClient* Sender, const std::vector<std::string>& Parameters);
    void ReceiveMessage_VERSION(CClient* Sender, const std::vector<std::string>& Parameters);
    void Shutdown();

private:
    boost::ptr_vector<boost::asio::ip::tcp::acceptor> m_Acceptors;
    boost::ptr_map<std::string, CChannel> m_Channels;
    CConfiguration& m_Configuration;
    boost::asio::io_service& m_IOService;
    std::string m_OnlineDateString;
    std::set< boost::shared_ptr<CNetworkClient> > m_NetworkClients;
    boost::shared_ptr<CNetworkClient> m_NextNetworkClient;
    std::map<std::string, CClient*> m_Nicknames;
    std::auto_ptr<boost::asio::ssl::context> m_SSLContext;

    inline void _Accept(boost::asio::ip::tcp::acceptor* Acceptor);
    void _AddAcceptor(const boost::asio::ip::tcp::acceptor::protocol_type& Protocol);
    void _HandleNewConnection(boost::asio::ip::tcp::acceptor* Acceptor, const boost::system::error_code& ErrorCode);
    std::string _HandleSSLPassword() const;
    inline bool _IsUserRegistered(const CClient::UserState& ClientUserState);
    void _WelcomeClient(CNetworkClient* Client);
};

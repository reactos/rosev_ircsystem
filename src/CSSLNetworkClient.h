/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

class CSSLNetworkClient : public CNetworkClient
{
public:
    CSSLNetworkClient(boost::asio::io_service& IOService, CIRCServer& IRCServer, boost::asio::ssl::context& SSLContext);

    boost::asio::ip::tcp::socket::lowest_layer_type& GetSocket() { return m_SSLStream.lowest_layer(); }
    bool IsInitialized() const;

    /* Support shared_from_this() for CSSLNetworkClient pointers.
       Credits go to http://stackoverflow.com/questions/657155/how-to-enable-shared-from-this-of-both-parend-and-derived */
    boost::shared_ptr<CSSLNetworkClient> shared_from_this() { return boost::static_pointer_cast<CSSLNetworkClient>(CNetworkClient::shared_from_this()); }

protected:
    void _Receive();
    void _Send();

private:
    bool m_HandshakeHappened;
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_SSLStream;

    void _HandleHandshake(const boost::system::error_code& ErrorCode);
};

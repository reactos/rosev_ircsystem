/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

class CPlainNetworkClient : public CNetworkClient
{
public:
    CPlainNetworkClient(boost::asio::io_service& IOService, CIRCServer& IRCServer);

    boost::asio::ip::tcp::socket::lowest_layer_type& GetSocket() { return m_Socket; }
    bool IsInitialized() const;

protected:
    void _Receive();
    void _Send();

private:
    boost::asio::ip::tcp::socket m_Socket;
};

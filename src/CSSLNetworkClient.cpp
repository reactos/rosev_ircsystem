/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010-2013 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

CSSLNetworkClient::CSSLNetworkClient(boost::asio::io_service& IOService, CIRCServer& IRCServer, boost::asio::ssl::context& SSLContext)
    : CNetworkClient(IOService, IRCServer), m_HandshakeHappened(false), m_SSLStream(IOService, SSLContext)
{
}

void
CSSLNetworkClient::_HandleHandshake(const boost::system::error_code& ErrorCode)
{
    if(ErrorCode)
    {
#ifdef _DEBUG
        /* If the handshake fails, ErrorCode.message() will neither contain an informational
           error message (you have to get that from OpenSSL) nor we can send it to the client.
           So output it in our console.
           Note that this error can also be triggered when someone tries to login to an
           SSL-enabled server without enabling SSL in his client. */
        char OpenSSLErrorString[1024];
        ERR_error_string_n(ErrorCode.value(), OpenSSLErrorString, sizeof(OpenSSLErrorString));
        std::cerr << "DEBUG: OpenSSL Error at handshake: " << OpenSSLErrorString << "\n";
#endif

        m_IRCServer.DisconnectNetworkClient(this, "");
        return;
    }

    /* Handshake was successful, we can do a real _Receive now */
    m_HandshakeHappened = true;
    _Receive();
}

void
CSSLNetworkClient::_Receive()
{
    if(m_HandshakeHappened)
    {
        m_SSLStream.async_read_some(
            boost::asio::buffer(m_InputBufferPointer, sizeof(m_InputBuffer) - sizeof('\0') - (m_InputBufferPointer - m_InputBuffer)),
            boost::bind(&CSSLNetworkClient::_HandleNewData, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
        );
    }
    else
    {
        /* We have to do the SSL handshake first */
        m_SSLStream.async_handshake(
            boost::asio::ssl::stream_base::server,
            boost::bind(&CSSLNetworkClient::_HandleHandshake, shared_from_this(), boost::asio::placeholders::error)
        );
    }
}

void
CSSLNetworkClient::_Send()
{
    boost::asio::async_write(
        m_SSLStream,
        boost::asio::buffer(m_SendBuffer.get(), m_SendBufferSize),
        boost::bind(&CSSLNetworkClient::_HandleSend, shared_from_this(), boost::asio::placeholders::error)
    );
}

bool
CSSLNetworkClient::IsInitialized() const
{
    /* An SSL socket is initialized and available for Send/Receive operations as soon as the handshake has happened. */
    return m_HandshakeHappened;
}

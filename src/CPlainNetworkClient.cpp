/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010-2013 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

CPlainNetworkClient::CPlainNetworkClient(boost::asio::io_service& IOService, CIRCServer& IRCServer)
    : CNetworkClient(IOService, IRCServer), m_Socket(IOService)
{
}

void
CPlainNetworkClient::_Receive()
{
    m_Socket.async_read_some(
        boost::asio::buffer(m_InputBufferPointer, sizeof(m_InputBuffer) - sizeof('\0') - (m_InputBufferPointer - m_InputBuffer)),
        boost::bind(&CPlainNetworkClient::_HandleNewData, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
    );
}

void
CPlainNetworkClient::_Send()
{
    boost::asio::async_write(
        m_Socket,
        boost::asio::buffer(m_SendBuffer.get(), m_SendBufferSize),
        boost::bind(&CPlainNetworkClient::_HandleSend, shared_from_this(), boost::asio::placeholders::error)
    );
}

bool
CPlainNetworkClient::IsInitialized() const
{
    /* An unencrypted socket is always initialized after it has been created. */
    return true;
}

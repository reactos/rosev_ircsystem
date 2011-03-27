/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2011 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

CChanServ::CChanServ(CIRCServer& IRCServer)
    : CVirtualClient(IRCServer, "ChanServ")
{
}

bool
CChanServ::Init()
{
    Info("Dummy ChanServ is enabled.\n");
    return true;
}

void
CChanServ::PostInit()
{
    /* Join all channels */
    const boost::ptr_map<std::string, CChannel*>& Channels = m_IRCServer.GetChannels();
    for(boost::ptr_map<std::string, CChannel*>::const_iterator it = Channels.begin(); it != Channels.end(); ++it)
    {
        std::vector<std::string> Parameters;
        Parameters.push_back(it->first);
        m_IRCServer.ReceiveMessage_JOIN(this, Parameters);
    }
}

void
CChanServ::SetClientModeInChannel(CClient* Client, CChannel* Channel)
{
    /* Get the client status */
    const std::map<CClient*, CChannel::ClientStatus>& Clients = Channel->GetClients();
    std::map<CClient*, CChannel::ClientStatus>::const_iterator ClientIt = Clients.find(Client);
    assert(ClientIt != Clients.end());

    /* Publicly give this status */
    if(ClientIt->second == CChannel::Voice)
    {
        std::string Response(boost::str(boost::format(":%s MODE #%s +v %s") % GetPrefix() % Channel->GetName() % Client->GetNickname()));

        for(ClientIt = Clients.begin(); ClientIt != Clients.end(); ++ClientIt)
            ClientIt->first->SendIRCMessage(Response);
    }
}

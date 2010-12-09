/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

CChannel::CChannel(const std::string& Name, const std::string& Topic, const std::set<std::string>& AllowedUsers)
    : m_Name(Name), m_Topic(Topic), m_AllowedUsers(AllowedUsers)
{
}

void
CChannel::AddClient(CClient* Client)
{
    m_Clients.insert(Client);
}

void
CChannel::RemoveClient(CClient* Client)
{
    m_Clients.erase(Client);
}

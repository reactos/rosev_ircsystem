/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

CVirtualClient::CVirtualClient(CIRCServer& IRCServer, const std::string& Nickname)
    : CClient(IRCServer)
{
    m_Nickname = Nickname;
    m_NicknameLowercased = Nickname;
    std::transform(m_NicknameLowercased.begin(), m_NicknameLowercased.end(), m_NicknameLowercased.begin(), tolower);
    m_Prefix = std::string(m_Nickname).append("!").append(m_NicknameLowercased).append("@virtual");

    m_UserState.HasSentNickMessage = true;
    m_UserState.HasSentUserMessage = true;
    m_UserState.IsIdentified = true;
}

/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010-2011 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

class CLogBot : public CVirtualClient
{
public:
    CLogBot(CIRCServer& IRCServer);

    bool Init();
    void PostInit();
    void SendIRCMessage(const std::string& Message);

private:
    boost::ptr_map<std::string, std::ofstream> m_ChannelStreamMap;
    std::map<std::string, void (CLogBot::*)(CClient*, const std::vector<std::string>&)> m_CommandHandlers;

    std::string _GetLogTimestamp();
    void _LogMessage_JOIN(CClient* Sender, const std::vector<std::string>& Parameters);
    void _LogMessage_PART(CClient* Sender, const std::vector<std::string>& Parameters);
    void _LogMessage_PRIVMSG(CClient* Sender, const std::vector<std::string>& Parameters);
    void _LogMessage_QUIT(CClient* Sender, const std::vector<std::string>& Parameters);
};

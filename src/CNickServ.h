/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

class CNickServ : public CVirtualClient
{
public:
    CNickServ(CIRCServer& IRCServer);

    bool Init();
    void SendPrivateMessage(CClient* Sender, const std::string& PrivateMessage);

private:
    std::map<std::string, void (CNickServ::*)(CNetworkClient*, const std::vector<std::string>&)> m_CommandHandlers;
    std::map< std::string, boost::array<char, SHA512_DIGEST_LENGTH> > m_UserPasshashMap;

    void _ReceiveCommand_HELP(CNetworkClient* Client, const std::vector<std::string>& Parameters);
    void _ReceiveCommand_IDENTIFY(CNetworkClient* Client, const std::vector<std::string>& Parameters);
};
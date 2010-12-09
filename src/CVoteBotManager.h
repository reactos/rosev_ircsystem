/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

class CVoteBotManager
{
public:
    CVoteBotManager(CIRCServer& IRCServer);

    void Init();

private:
    CIRCServer& m_IRCServer;
    boost::ptr_vector<CVoteBot> m_VoteBots;
};

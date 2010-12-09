/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

class CVirtualClient : public CClient
{
public:
    /** First initialization to check whether the virtual client shall even be added. (e.g. by reading config files) */
    virtual bool Init() = 0;

    bool IsNetworkClient() const { return false; }

    /** Optional post initialization steps after the client has been added to the list of nicknames. (e.g. joining channels) */
    virtual void PostInit() {}

protected:
    CVirtualClient(CIRCServer& IRCServer, const std::string& Nickname);
};

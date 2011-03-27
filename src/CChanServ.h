/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2011 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

/* Really dummy implementation of a ChanServ which just exists for compatibility with IRC clients.
   If a client internally has voice status, our dummy ChanServ implementation needs to publicly give him
   this status, so that others can see the status without issueing a manual /NAMES command.
*/
class CChanServ : public CVirtualClient
{
public:
    CChanServ(CIRCServer& IRCServer);

    bool Init();
    void PostInit();

    /** ChanServ is the only virtual client, which has such a function directly called from CIRCServer. Avoid this wherever possible. */
    void SetClientModeInChannel(CClient* Client, CChannel* Channel);
};

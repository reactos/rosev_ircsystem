/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010-2011 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

class CChannel : private boost::noncopyable
{
public:
    enum ClientStatus
    {
        NoStatus,
        Voice
    };

    CChannel(const std::string& Name, const std::string& Topic, const std::set<std::string>& AllowedUsers, bool AllowObservers);

    void AddClient(CClient* Client, CChannel::ClientStatus Status);
    bool DoAllowObservers() const { return m_AllowObservers; }
    const std::set<std::string>& GetAllowedUsers() const { return m_AllowedUsers; }
    const std::map<CClient*, ClientStatus>& GetClients() const { return m_Clients; }
    const std::string& GetName() const { return m_Name; }
    const std::string& GetTopic() const { return m_Topic; }
    void RemoveClient(CClient* Client);

private:
    std::set<std::string> m_AllowedUsers;
    bool m_AllowObservers;
    std::map<CClient*, ClientStatus> m_Clients;
    std::string m_Name;
    std::string m_Topic;
};

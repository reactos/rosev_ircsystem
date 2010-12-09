/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

class CVoteBot : public CVirtualClient
{
public:
    CVoteBot(CIRCServer& IRCServer, const std::string& Nickname, const std::string& ConfigID);

    bool Init();
    void PostInit();
    void SendIRCMessage(const std::string& Message);
    void SendPrivateMessage(CClient* Sender, const std::string& PrivateMessage);

private:
    struct VoteParameters
    {
        bool HasVoted;
        size_t Option;
    };

    std::string m_AbstentionTranslation;
    std::set<std::string> m_AdminUsers;
    CChannel* m_Channel;
    std::string m_ConfigID;
    std::string m_CurrentAdminNickname;
    std::vector<std::string> m_Options;
    std::string m_Question;
    unsigned int m_TimeLimitInMinutes;
    boost::asio::deadline_timer m_Timer;
    size_t m_VoteCount;
    std::map<CClient*, CVoteBot::VoteParameters> m_Votes;
    bool m_VoteStarted;

    void _CheckVotes();
    void _ReceiveCommand_CANCEL(CClient* Sender);
    void _ReceiveCommand_HELP(CClient* Sender);
    void _ReceiveCommand_NEW(CClient* Sender);
    void _ReceiveCommand_START(CClient* Sender);
    void _ReceiveVote(CClient* Sender, const std::string& PrivateMessage);
    void _Reset();
    void _SendToChannel(const std::string& Message);
    void _VoteDeadline(const boost::system::error_code& ErrorCode);
};

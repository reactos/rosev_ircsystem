/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

class CClient : private boost::noncopyable
{
public:
    /* Helper class for CClient::SendNumericReply */
    class CNumericReplyFormatter
    {
        public:
            CNumericReplyFormatter(CClient* Client = NULL, short Code = 0);

            /* Function is fully defined here, because at least MSVC's linker doesn't seem to find
               templatized function implementations in external files. */
            template<class Type> CNumericReplyFormatter& operator%(const Type& Argument)
            {
                if(m_Client)
                {
                    assert(m_Format.remaining_args());
                    m_Format % Argument;
                    _CheckRemainingArguments();
                }

                return *this;
            }

        private:
            CClient* m_Client;
            boost::format m_Format;

            void _CheckRemainingArguments();
    };

    struct UserState
    {
        bool HasSentNickMessage : 1;
        bool HasSentUserMessage : 1;
        bool IsIdentified       : 1;
    };

    void AddJoinedChannel(CChannel* Channel);
    CIRCServer& GetIRCServer() const { return m_IRCServer; }
    const std::set<CChannel*>& GetJoinedChannels() const { return m_JoinedChannels; }
    const std::string& GetNickname() const { return m_Nickname; }
    const char* GetNicknameAsTarget() const;
    const std::string& GetNicknameLowercased() const { return m_NicknameLowercased; }
    const std::string& GetPrefix() const { return m_Prefix; }
    CClient::UserState GetUserState() const { return m_UserState; }
    virtual bool IsNetworkClient() const = 0;
    void RemoveJoinedChannel(CChannel* Channel);

    /* Virtual clients can overwrite these functions if they want to handle these messages sent to them */
    virtual void SendIRCMessage(const std::string& Message) {}
    virtual void SendNotice(CClient* Sender, const std::string& Notice) {}
    virtual CClient::CNumericReplyFormatter SendNumericReply(short Code) { return CNumericReplyFormatter(); }
    virtual void SendPrivateMessage(CClient* Sender, const std::string& PrivateMessage) {}

protected:
    CIRCServer& m_IRCServer;
    std::set<CChannel*> m_JoinedChannels;
    std::string m_Nickname;
    std::string m_NicknameLowercased;           /** For comparison purposes (nicknames must be unique case-insensitively) */
    std::string m_Prefix;                       /** Nickname would suffice according to RFC2812, 2.3.1, but clients like ChatZilla only support full prefixes */
    CClient::UserState m_UserState;

    CClient(CIRCServer& IRCServer);
};

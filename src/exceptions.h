/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

/* General class for a Boost-compatible exception with a message */
class Error : public virtual boost::exception, public virtual std::exception
{
public:
    Error(const char* Message) throw()
    {
        try
        {
            m_Message = Message;
        }
        catch(...)
        {
            /* Avoid causing even more exceptions. */
        }
    }

    /* Empty non-throw destructor to please G++ */
    ~Error() throw() {}

    const char*
    what() const throw()
    {
        return m_Message.c_str();
    }

private:
    std::string m_Message;
};

/* Types for additional error information */
typedef boost::error_info<struct ChannelNameTag, const std::string> ChannelName_Info;
typedef boost::error_info<struct PasshashTag, const std::string> Passhash_Info;

#ifdef WIN32
typedef boost::error_info<struct Win32ErrorTag, ULONG> Win32Error_Info;

inline std::string
to_string(Win32Error_Info const& Exception)
{
    char* MessageBuffer;
    std::ostringstream MessageStream;

    MessageStream << Exception.value();

    if(FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, Exception.value(), 0, reinterpret_cast<char*>(&MessageBuffer), 0, NULL))
    {
        MessageStream << ", " << MessageBuffer;
        LocalFree(MessageBuffer);
    }

    return MessageStream.str();
}
#endif

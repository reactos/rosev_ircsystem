/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010-2017 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

class CConfiguration : private boost::noncopyable
{
public:
    CConfiguration();

#ifdef WIN32
    bool DoInstallService() const { return m_InstallService; }
    bool DoUninstallService() const { return m_UninstallService; }
#else
    const std::string& GetPidFile() const { return m_PidFile; }
#endif
    bool BeVerbose() const { return m_Verbose; }
    bool DoPrintVersion() const { return m_PrintVersion; }
    bool DoRunAsDaemonService() const { return m_RunAsDaemonService; }
    bool DoUseIPv6() const { return m_UseIPv6; }
    bool DoUseSSL() const { return m_UseSSL; }
    const boost::program_options::options_description& GetCommandLineOptions() const { return m_CommandLineOptions; }
    const std::string& GetConfigPath() const { return m_ConfigPath; }
    const std::vector<std::string>& GetMotd() const { return m_Motd; }
    const std::string& GetName() const { return m_Name; }
    unsigned short GetPort() const { return m_Port; }
    const std::string& GetSSLCertificateFile() const { return m_SSLCertificateFile; }
    const std::string& GetSSLPrivateKeyFile() const { return m_SSLPrivateKeyFile; }
    const std::map< std::string, boost::array<char, SHA512_DIGEST_LENGTH> >& GetUserPasshashMap() const { return m_UserPasshashMap; }
    bool ParseParameters(int argc, char* argv[]);
    void ReadConfigFiles();

private:
#ifdef WIN32
    bool m_InstallService;
    bool m_UninstallService;
#else
    std::string m_PidFile;
#endif
    boost::program_options::options_description m_CommandLineOptions;
    std::string m_ConfigPath;
    std::vector<std::string> m_Motd;
    std::string m_Name;
    unsigned short m_Port;
    bool m_PrintVersion;
    bool m_RunAsDaemonService;
    std::string m_SSLCertificateFile;
    std::string m_SSLPrivateKeyFile;
    bool m_Verbose;
    bool m_UseIPv6;
    std::map< std::string, boost::array<char, SHA512_DIGEST_LENGTH> > m_UserPasshashMap;
    bool m_UseSSL;
};

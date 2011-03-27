/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010-2011 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

CConfiguration::CConfiguration()
    : m_CommandLineOptions("Options")
{
    m_CommandLineOptions.add_options()
#ifdef WIN32
        ("install-service", boost::program_options::value<bool>(&m_InstallService)->zero_tokens()->default_value(false), "Install the NT service with the supplied configuration directory")
        ("uninstall-service", boost::program_options::value<bool>(&m_UninstallService)->zero_tokens()->default_value(false), "Uninstall the NT service")
        ("service", boost::program_options::value<bool>(&m_RunAsDaemonService)->zero_tokens()->default_value(false), "Run the program as an NT service (used by the installed service)")
#else
        ("daemon", boost::program_options::value<bool>(&m_RunAsDaemonService)->zero_tokens()->default_value(false), "Run the program as a daemon")
#endif
        ("verbose", boost::program_options::value<bool>(&m_Verbose)->zero_tokens()->default_value(false), "Be verbose, ignored in daemon/service mode")
        ("version", boost::program_options::value<bool>(&m_PrintVersion)->zero_tokens()->default_value(false), "Prints the version number and exits");
}

bool
CConfiguration::ParseParameters(int argc, char* argv[])
{
    /* Command-line options
       We need these hidden options to parse the positional options below. */
    boost::program_options::options_description HiddenOptions;
    HiddenOptions.add_options()
        ("path", boost::program_options::value<std::string>(&m_ConfigPath)->default_value(""));

    /* Interpret the path given after the options as the configuration directory. */
    boost::program_options::positional_options_description PositionalOptions;
    PositionalOptions.add("path", 1);

    /* Combine both option_descriptions for the parser */
    boost::program_options::options_description OptionsToParse;
    OptionsToParse.add(m_CommandLineOptions);
    OptionsToParse.add(HiddenOptions);

    /* We have to call notify() to set the variables assigned to the options, and this requires this TemporaryVariablesMap :-( */
    boost::program_options::variables_map TemporaryVariablesMap;

    try
    {
        boost::program_options::store(
            boost::program_options::command_line_parser(argc, argv).options(OptionsToParse).positional(PositionalOptions).run(),
            TemporaryVariablesMap
        );
    }
    catch(boost::program_options::unknown_option&)
    {
        /* Show the usage when we encounter unknown options. */
        return false;
    }

    boost::program_options::notify(TemporaryVariablesMap);

    /* Show the usage if the parameters are not correct */
    if(m_ConfigPath.empty() && !m_PrintVersion)
    {
#ifdef WIN32
        /* The --uninstall-service option of the Win32 version is another option which does not require an argument */
        if(!m_UninstallService)
        {
            return false;
        }
#else
        return false;
#endif
    }

    /* Disable verbosity if we're in daemon/service mode */
    if(m_RunAsDaemonService)
        m_Verbose = false;

    return true;
}

void
CConfiguration::ReadConfigFiles()
{
    /* Main config file */
    boost::program_options::options_description MainConfigOptions;
    MainConfigOptions.add_options()
#ifdef WIN32
        /* Don't fail if the pidfile option is specified anyway (even if it's irrelevant for the Win32 version) */
        ("general.pidfile", boost::program_options::value<std::string>())
#else
        ("general.pidfile", boost::program_options::value<std::string>(&m_PidFile)->default_value(""))
#endif
        ("general.name", boost::program_options::value<std::string>(&m_Name)->default_value(""))
        ("general.port", boost::program_options::value<unsigned short>(&m_Port)->default_value(0))
        ("general.use_ipv4", boost::program_options::value<bool>(&m_UseIPv4)->default_value(false))
        ("general.use_ipv6", boost::program_options::value<bool>(&m_UseIPv6)->default_value(false))
        ("ssl.certificate", boost::program_options::value<std::string>(&m_SSLCertificateFile)->default_value(""))
        ("ssl.privatekey", boost::program_options::value<std::string>(&m_SSLPrivateKeyFile)->default_value(""))
        ("ssl.use", boost::program_options::value<bool>(&m_UseSSL)->default_value(false));

    boost::program_options::variables_map TemporaryVariablesMap;
    boost::program_options::store(boost::program_options::parse_config_file<char>(std::string(m_ConfigPath).append(MAIN_CONFIG_FILE).c_str(), MainConfigOptions), TemporaryVariablesMap);
    boost::program_options::notify(TemporaryVariablesMap);

    /* Motd file (Message of the day) */
    std::string MotdFileName(m_ConfigPath);
    MotdFileName.append(MOTD_FILE);

    std::ifstream MotdStream(MotdFileName.c_str());
    if(!MotdStream)
        BOOST_THROW_EXCEPTION(Error("Could not open the Motd file!") << boost::errinfo_file_name(MotdFileName));

    std::string MotdLine;
    while(std::getline(MotdStream, MotdLine))
    {
        if(MotdLine.length() > MOTD_LINE_LENGTH)
            BOOST_THROW_EXCEPTION(Error("Motd file exceeds line length of " BOOST_PP_STRINGIZE(MOTD_LINE_LENGTH) " characters!"));

        m_Motd.push_back(MotdLine);
    }

    MotdStream.close();

    /* Users file */
    boost::program_options::parsed_options UsersParsedOptions(boost::program_options::parse_config_file<char>(std::string(m_ConfigPath).append(USERS_FILE).c_str(), NULL, true));
    for(std::vector< boost::program_options::basic_option<char> >::const_iterator it = UsersParsedOptions.options.begin(); it != UsersParsedOptions.options.end(); ++it)
    {
        /* Convert the hexadecimal string passhash into a binary one */
        const std::string& HexPasshash = it->value[0];
        boost::array<char, SHA512_DIGEST_LENGTH> BinaryPasshash;

        if(HexPasshash.length() != 2 * SHA512_DIGEST_LENGTH)
            BOOST_THROW_EXCEPTION(Error("Length of a passhash must be 128 characters!") << Passhash_Info(HexPasshash));

        for(int i = 0; i < SHA512_DIGEST_LENGTH; ++i)
            BinaryPasshash[i] = static_cast<char>(strtol(HexPasshash.substr(2 * i, 2).c_str(), NULL, 16));

        /* Lowercase the nickname for comparison purposes */
        std::string NicknameLowercased(it->string_key);
        std::transform(NicknameLowercased.begin(), NicknameLowercased.end(), NicknameLowercased.begin(), tolower);

        m_UserPasshashMap.insert(std::make_pair(NicknameLowercased, BinaryPasshash));
    }

    /* Sanity checks */
    if(m_Name.empty())
        BOOST_THROW_EXCEPTION(Error("You need to specify a server name!"));

    if(m_Port == 0)
        BOOST_THROW_EXCEPTION(Error("You need to specify a port to listen on!"));

    if(!m_UseIPv4 && !m_UseIPv6)
        BOOST_THROW_EXCEPTION(Error("You need to enable either IPv4 or IPv6 (or both)!"));

#ifndef WIN32
    if(m_PidFile.empty())
        BOOST_THROW_EXCEPTION(Error("You need to specify a pidfile under Posix environments!"));
#endif
}

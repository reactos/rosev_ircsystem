/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010-2013 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

#include <precomp.h>

CVoteBotManager::CVoteBotManager(CIRCServer& IRCServer)
    : m_IRCServer(IRCServer)
{
}

void
CVoteBotManager::Init()
{
    std::string ConfigPath(m_IRCServer.GetConfiguration().GetConfigPath());

    /* Load the VoteBotManager-specific configuration.
       It associates VoteBot nicknames to config file IDs. */
    std::string ConfigFile(ConfigPath);
    ConfigFile.append(VOTEBOTMANAGER_FILE);

    /* Disable VoteBotManager if the config does not exist */
    if(!std::ifstream(ConfigFile.c_str()))
    {
        Info("All VoteBots are disabled.\n");
        return;
    }

    /* Create VoteBots for all entries in the VoteBotManager configuration */
    boost::program_options::parsed_options ParsedOptions(boost::program_options::parse_config_file<char>(ConfigFile.c_str(), 0, true));
    for(std::vector< boost::program_options::basic_option<char> >::const_iterator it = ParsedOptions.options.begin(); it != ParsedOptions.options.end(); ++it)
    {
        std::auto_ptr<CVoteBot> Bot(new CVoteBot(m_IRCServer, it->string_key, it->value[0]));
        m_IRCServer.AddVirtualClient(Bot.get());
        m_VoteBots.push_back(Bot);
    }
}

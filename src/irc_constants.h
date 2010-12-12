/*
 * PROJECT:    ReactOS Deutschland e.V. IRC System
 * LICENSE:    GNU GPL v2 or any later version as published by the Free Software Foundation
 *             with the additional exemption that compiling, linking, and/or using OpenSSL is allowed
 * COPYRIGHT:  Copyright 2010 ReactOS Deutschland e.V. <deutschland@reactos.org>
 * AUTHORS:    Colin Finck <colin@reactos.org>
 */

/* RFC2812, 2.3 - "[...] these messages shall not exceed 512 characters in length, counting all characters including the trailing CR-LF" */
#define IRC_MESSAGE_LENGTH   512

/* RFC2812, 5.1 Command Responses */
#define RPL_WELCOME          001
#define RPL_YOURHOST         002
#define RPL_CREATED          003
#define RPL_MYINFO           004
#define RPL_NOTOPIC          331
#define RPL_TOPIC            332
#define RPL_VERSION          351
#define RPL_NAMREPLY         353
#define RPL_ENDOFNAMES       366
#define	RPL_INFO             371
#define	RPL_MOTD             372
#define	RPL_ENDOFINFO        374
#define RPL_MOTDSTART        375
#define RPL_ENDOFMOTD        376

/* RFC2812, 5.2 Error Replies */
#define ERR_NOSUCHNICK       401
#define ERR_NOSUCHCHANNEL    403
#define ERR_CANNOTSENDTOCHAN 404
#define ERR_NORECIPIENT      411
#define ERR_NOTEXTTOSEND     412
#define ERR_NONICKNAMEGIVEN  431
#define ERR_ERRONEUSNICKNAME 432
#define ERR_NICKNAMEINUSE    433
#define ERR_NOTONCHANNEL     442
#define ERR_NEEDMOREPARAMS   461

/* ROSEV-SPECIFIC Settings */
#define IDENTIFY_TIMEOUT     240
#define MOTD_LINE_LENGTH     80
#define NICKNAME_LENGTH      30
#define PING_INTERVAL        120
#define PING_TIMEOUT         60
#define REGISTRATION_TIMEOUT 120

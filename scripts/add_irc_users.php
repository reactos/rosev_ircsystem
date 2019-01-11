#!/usr/bin/php
<?php
	// For editors: charset=utf8
	//
	// Create random passwords for all people in the corresponding LDAP group,
	// send them an appropriate E-Mail and set up the corresponding rosev_ircsystem
	// configuration files.

	define("CHANNEL", "meeting");
	define("CONFDIR", "/srv/irc");
	define("MAIL_FROM", "ReactOS IRC Server <administrator@reactos.org>");
	define("MAIL_SUBJECT", "Credentials for the upcoming IRC meeting");
	define("PASSWORD_CHARACTERS", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNPQRSTUVWXYZ123456789");        // Avoid special characters, IRC clients like to mess them up
	define("PASSWORD_LENGTH", 16);
	require("ldap_config.inc.php");

	$mail_replacements = array(
		"NICKNAME" => &$nickname,
		"PASSWORD" => &$password,
	);

	$nickname_replacements = array(
		" " => "_",
		"-" => "_",
		"ä" => "ae",
		"ö" => "oe",
		"ü" => "ue",
		"ß" => "ss",
	);

	// Get the mail template
	$mail_fp = fopen("Mail.txt", "r");
	$mailtemplate = fread($mail_fp, filesize("Mail.txt"));
	fclose($mail_fp);

	// Open the IRC Server configuration files for writing
	$channel_users_fp = fopen(CONFDIR . "/Channel_Users.ini", "w");
	$output_users_fp = fopen(CONFDIR . "/Users.ini", "w");

	// Get our participants from the LDAP server
	($ds = ldap_connect(LDAP_HOST)) || die("Couldn't connect to the LDAP server!\n");
	ldap_set_option($ds, LDAP_OPT_PROTOCOL_VERSION, 3);
	ldap_bind($ds, LDAP_BIND_DN, LDAP_BIND_PW) || die("Couldn't bind to the LDAP server!\n");
	$sr = ldap_search($ds, LDAP_BASE_DN, LDAP_FILTER);
	$info = ldap_get_entries($ds, $sr);

	for($i = 0; $i < $info["count"]; $i++)
	{
		// Convert all special characters in the Common Name to IRC-compatible ASCII
		$nickname = strtr($info[$i]["displayname"][0], $nickname_replacements);
		$nickname = iconv("UTF-8", "ASCII//TRANSLIT", $nickname);
		$nickname = preg_replace("#[^A-Za-z_]#", "", $nickname);

		$password = "";
		for($j = 0; $j < PASSWORD_LENGTH; $j++)
			$password .= substr(PASSWORD_CHARACTERS, mt_rand(0, strlen(PASSWORD_CHARACTERS) - 1), 1);

		$passhash = hash("sha512", $password);
		fwrite($channel_users_fp, CHANNEL . "=$nickname\n");
		fwrite($output_users_fp, "$nickname=$passhash\n");

		echo "Sending mail for $nickname...\n";
		$mail = strtr($mailtemplate, $mail_replacements);
		mail($info[$i]["mail"][0], MAIL_SUBJECT, $mail, "From: " . MAIL_FROM);
	}

	ldap_close($ds);
	fclose($channel_users_fp);
	fclose($output_users_fp);
?>

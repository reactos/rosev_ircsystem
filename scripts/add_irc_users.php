#!/usr/bin/php
<?php
	// Create random passwords for all nicknames in "Users.txt", send them
	// an appropriate E-Mail and set up the corresponding rosev_ircsystem
	// configuration files.
	
	define("CHANNEL", "meeting");
	define("CONFDIR", "/srv/irc");
	define("MAIL_FROM", "ReactOS IRC Server <administrator@reactos.org>");
	define("MAIL_SUBJECT", "Credentials for the upcoming IRC meeting");
	define("PASSWORD_CHARACTERS", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNPQRSTUVWXYZ123456789");        // Avoid special characters, IRC clients like to mess them up
	define("PASSWORD_LENGTH", 16);


	$mail_replacements = array(
		"NICKNAME" => &$nickname,
		"PASSWORD" => &$password
	);

	$mail_fp = fopen("Mail.txt", "r");
	$mailtemplate = fread($mail_fp, filesize("Mail.txt"));
	fclose($mail_fp);

	$input_users_fp = fopen("Users.csv", "r");
	$channel_users_fp = fopen(CONFDIR . "/Channel_Users.ini", "w");
	$output_users_fp = fopen(CONFDIR . "/Users.ini", "w");
	
	while($set = fgetcsv($input_users_fp, 1024))
	{
		$nickname = $set[0];
		
		$password = "";
		for($i = PASSWORD_LENGTH; --$i >= 0;)
			$password .= substr(PASSWORD_CHARACTERS, mt_rand(0, strlen(PASSWORD_CHARACTERS) - 1), 1);
		
		$passhash = hash("sha512", $password);
		fwrite($channel_users_fp, CHANNEL . "=$nickname\n");
		fwrite($output_users_fp, "$nickname=$passhash\n");
		
		echo "Sending mail for $nickname...\n";
		$mail = strtr($mailtemplate, $mail_replacements);
		mail($set[1], MAIL_SUBJECT, $mail, "From: " . MAIL_FROM);
	}
	
	fclose($input_users_fp);
	fclose($channel_users_fp);
	fclose($output_users_fp);
?>

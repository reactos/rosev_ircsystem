<?php
	// LDAP settings for add_irc_users.php
	define("LDAP_HOST", "localhost");
	define("LDAP_BIND_DN", "cn=binder,dc=acme,dc=com");
	define("LDAP_BIND_PW", "MySafePassword");
	define("LDAP_BASE_DN", "ou=users,dc=acme,dc=com");
	define("LDAP_FILTER", "(&(memberof=cn=participants,ou=groups,dc=acme,dc=com))");
?>

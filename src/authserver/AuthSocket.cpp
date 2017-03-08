#include "Common.h"
#include "AuthSocket.h"

#include "Database/DatabaseEnv.h"
#include "Config/Config.h"
#include "Log.h"
#include "RealmList.h"
#include <openssl/md5.h>
#include "Util.h"

#include <stdio.h>
#include <cstdio>

extern DatabaseType LoginDatabase;

enum eStatus
{
	STATUS_CONNECTED = 0,
	STATUS_AUTHED
};

enum AccountFlags
{
	ACCOUNT_FLAG_GM = 0x00000001,
	ACCOUNT_FLAG_TRIAL = 0x00000008,
	ACCOUNT_FLAG_PROPASS = 0x00800000,
};

#pragma pack(push,1)

typedef struct LOGIN
{
	uint8 cmd;
	std::string		username;
	char			password[33];
	uint32			build;
}LOGIN;

typedef struct LOGIN_RESULT
{
	uint8		cmd;
	AuthResult	login_result;
}LOGIN_RESULT;

typedef struct REALM_RESULT
{
	uint8		cmd;
	uint8		size;
	Realm		realm[9];
}REALM_RESULT;

typedef struct AuthHandler
{
	eAuthCmd cmd;
	uint32 status;
	bool (AuthSocket::*handler)(void);
} AuthHandler;

#pragma pack(pop)

/// Constructor - set the N and g values for SRP6
AuthSocket::AuthSocket(boost::asio::io_service &service, std::function<void(Socket *)> closeHandler)
	: Socket(service, closeHandler), _authed(false), _build(0), _accountSecurityLevel(SEC_PLAYER)
{
}
bool AuthSocket::_HandleOnLogin()
{
	if (ReadLengthRemaining() < sizeof(LOGIN))
	{
		sLog.outError("data size invalid");
		return false;
	}
	std::vector<uint8> buf;
	buf.resize(sizeof(LOGIN));

	Read((char*)&buf[0], sizeof(LOGIN));
	
	LOGIN *stru = (LOGIN*)&buf[0];

	if (stru != NULL)
	{
		LOGIN_RESULT Loginresult;

		Loginresult.cmd = CMD_AUTH_LOGIN;

		_login = stru->username;
		_safelogin = stru->username;
		_localizationName = "frFr";
		_build = stru->build;
		_accountSecurityLevel = AccountTypes::SEC_PLAYER;
		printf("password: %S", stru->password);
		sLog.outDetail("Password: %s\n", stru->password);
		/// get account from database
		QueryResult* result = LoginDatabase.PQuery("SELECT unbandate FROM ip_banned WHERE (unbandate = bandate OR unbandate > UNIX_TIMESTAMP()) AND ip = '%s'", m_address.c_str());
		if (result)
		{
			sLog.outDebug("[AuthChallenge] Banned ip %s tries to login!", m_address.c_str());
			Loginresult.login_result = ORIGIN_FAIL_BANNED;
			delete result;
		}
		else
		{
			///- Get the account details from the account table
			result = LoginDatabase.PQuery("SELECT md5,id,locked,last_ip,gmlevel FROM account WHERE username = '%s'", _safelogin.c_str());
			if (result)
			{
				///- If the IP is 'locked', check that the player comes indeed from the correct IP address
				bool locked = false;
				if ((*result)[2].GetUInt8() == 1)               // if ip is locked
				{
					sLog.outDebug("[AuthChallenge] Account '%s' is locked to IP - '%s'", _login.c_str(), (*result)[3].GetString());
					sLog.outDebug("[AuthChallenge] Player address is '%s'", m_address.c_str());
					if (strcmp((*result)[3].GetString(), m_address.c_str()))
					{
						sLog.outDebug("[AuthChallenge] Account IP differs");
						locked = true;
						Loginresult.login_result = ORIGIN_FAIL_SUSPENDED;
					}
					else
					{
						sLog.outDebug("[AuthChallenge] Account IP matches");
					}
				}
				if (!locked)
				{
					///- If the account is banned, reject the logon attempt
					QueryResult* banresult = LoginDatabase.PQuery("SELECT bandate,unbandate FROM account_banned WHERE "
						"id = %u AND active = 1 AND (unbandate > UNIX_TIMESTAMP() OR unbandate = bandate)", (*result)[1].GetUInt32());
					if (banresult)
					{
						if ((*banresult)[0].GetUInt64() == (*banresult)[1].GetUInt64())
						{
							sLog.outDebug("[AuthChallenge] Banned account %s tries to login!", _login.c_str());
							Loginresult.login_result = ORIGIN_FAIL_BANNED;
						}
						else
						{
							sLog.outDebug("[AuthChallenge] Temporarily banned account %s tries to login!", _login.c_str());
							Loginresult.login_result = ORIGIN_FAIL_SUSPENDED;
						}
						delete banresult;
					}
					else
					{
						std::string password_md5 = (*result)[0].GetCppString();

						if (password_md5 == stru->password)
						{
							uint8 secLevel = (*result)[4].GetUInt8();
							_authed = true;
							_build = stru->build;
							Loginresult.login_result = ORIGIN_SUCCESS;
							_accountSecurityLevel = secLevel <= SEC_ADMINISTRATOR ? AccountTypes(secLevel) : SEC_ADMINISTRATOR;
							_localizationName = "FRfr";
						}
						else
						{
							Loginresult.login_result = ORIGIN_FAIL_UNKNOWN_ACCOUNT;
						}
					}
				}
			}
			else     // no account
			{
				Loginresult.login_result = ORIGIN_FAIL_UNKNOWN_ACCOUNT;
				sLog.outDebug("[AuthChallenge] account %s not found!", _login.c_str());
			}
		}
		Write((const char *)&Loginresult, sizeof(LOGIN_RESULT));
	}
	return true;
}
/// Read the packet from the client
bool AuthSocket::ProcessIncomingData()
{
	/// benchmarking has demonstrated that this lookup method is faster than std::map
	const static AuthHandler table[] =
	{
		{ CMD_AUTH_LOGIN,				STATUS_CONNECTED, &AuthSocket::_HandleOnLogin				},
		{ CMD_REALM_LIST,               STATUS_AUTHED,    &AuthSocket::_HandleRealmList				}		
	};

	const int tableLength = sizeof(table) / sizeof(AuthHandler);
	sLog.outDebug("Incomming data... '%d'", ReadLengthRemaining());
	/// the purpose of this loop is to handle multiple opcodes in the same tcp packet,
	/// which presumably the client will never do, but lets support it anyway! \o/
	while (ReadLengthRemaining() > 0)
	{
		const eAuthCmd cmd = static_cast<eAuthCmd>(*InPeak());
		int i;
		///- Circle through known commands and call the correct command handler
		for (i = 0; i < tableLength; ++i)
		{
			if (table[i].cmd != cmd)
				continue;

			/// unauthorized
			if (!(table[i].status == STATUS_CONNECTED || (_authed && table[i].status == STATUS_AUTHED)))
			{
				sLog.outDebug("[Auth] Received unauthorized command %u length %u", cmd, ReadLengthRemaining());
				return false;
			}

			sLog.outDebug("[Auth] Got data for cmd %u recv length %u", cmd, ReadLengthRemaining());

			if (!(*this.*table[i].handler)())
			{
				sLog.outDebug("[Auth] Command handler failed for cmd %u recv length %u", cmd, ReadLengthRemaining());
				return false;
			}

			break;
		}

		/// did we iterate over the entire command table, finding nothing? if so, punt!
		if (i == tableLength)
		{
			sLog.outDebug("[Auth] Got unknown packet %u", cmd);
			return false;
		}

		/// if we reach here, it means that a valid opcode was found and the handler completed successfully
	}
	return true;
}

/// %Realm List command handler
bool AuthSocket::_HandleRealmList()
{
	//DEBUG_LOG("Entering _HandleRealmList");
	if (ReadLengthRemaining() < 5)
	{
		sLog.outError("Error ReadLengthRemaining() < 5");
		return false;
	}
	ReadSkip(5);
	///- Get the user id (else close the connection)
	// No SQL injection (escaped user name)

	QueryResult* result = LoginDatabase.PQuery("SELECT id,md5 FROM account WHERE username = '%s'", _safelogin.c_str());
	if (!result)
	{
		sLog.outErrorDb("[ERROR] user %s tried to login and we cannot find him in the database.", _login.c_str());
		Close();
		return false;
	}
	uint32 id = (*result)[0].GetUInt32();
	std::string rI = (*result)[1].GetCppString();
	delete result;

	///- Update realm list if need
	sRealmList.UpdateIfNeed();
	///- Circle through realms in the RealmList and construct the return packet (including # of user characters in each realm)
	REALM_RESULT realm;
	realm.cmd = CMD_REALM_LIST;
	LoadRealmlist(realm, id);
	Write((const char *)&realm, sizeof(REALM_RESULT));

	return true;
}

void AuthSocket::LoadRealmlist(REALM_RESULT& realm, uint32 acctid)
{
	switch (_build)
	{
		case 0:
			break;
		default :                                          // 0.1.0
		{
			realm.size = uint8(sRealmList.size());
			int realmIndex = 0;
			for (RealmList::RealmMap::const_iterator i = sRealmList.begin(); i != sRealmList.end(); realmIndex++, ++i)
			{
				uint8 AmountOfCharacters;

				// No SQL injection. id of realm is controlled by the database.
				QueryResult* result = LoginDatabase.PQuery("SELECT numchars FROM realmcharacters WHERE realmid = '%d' AND acctid='%u'", i->second.m_ID, acctid);
				if (result)
				{
					Field* fields = result->Fetch();
					AmountOfCharacters = fields[0].GetUInt8();
					delete result;
				}
				else
					AmountOfCharacters = 0;

				bool ok_build = std::find(i->second.realmbuilds.begin(), i->second.realmbuilds.end(), _build) != i->second.realmbuilds.end();

				RealmBuildInfo const* buildInfo = ok_build ? FindBuildInfo(_build) : nullptr;
				if (!buildInfo)
					buildInfo = &i->second.realmBuildInfo;

				RealmFlags realmflags = static_cast<RealmFlags>(i->second.realmflags);

				// 1.x clients not support explicitly REALM_FLAG_SPECIFYBUILD, so manually form similar name as show in more recent clients
				std::string name = i->first;
				if (realmflags & REALM_FLAG_SPECIFYBUILD)
				{
					char buf[20];
					snprintf(buf, 20, " (%u,%u,%u)", buildInfo->major_version, buildInfo->minor_version, buildInfo->bugfix_version);
					name += buf;
				}

				// Show offline state for unsupported client builds and locked realms (1.x clients not support locked state show)
				if (!ok_build || (i->second.allowedSecurityLevel > _accountSecurityLevel))
					realmflags = RealmFlags(realmflags/* | REALM_FLAG_OFFLINE*/);

				realm.realm[realmIndex].icon = uint32(i->second.icon);              // realm type
				realm.realm[realmIndex].realmflags = realmflags;                   // realmflags
				realm.realm[realmIndex].name = name;                                // name
				realm.realm[realmIndex].address = i->second.address;                   // address
				realm.realm[realmIndex].populationLevel = float(i->second.populationLevel);
				realm.realm[realmIndex].AmountOfCharacters = uint8(AmountOfCharacters);
				realm.realm[realmIndex].timezone = uint8(i->second.timezone);           // realm category
				realm.realm[realmIndex].m_ID = i->second.m_ID;
			}
			break;
		}
	}
}

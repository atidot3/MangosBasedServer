#include "Common.h"
#include "RealmList.h"
#include "AuthCodes.h"
#include "Util.h"                                           // for Tokens typedef
#include "Config/Singleton.h"
#include "Database/DatabaseEnv.h"

INSTANTIATE_SINGLETON_1(RealmList);

extern DatabaseType LoginDatabase;

static const RealmBuildInfo ExpectedRealmdClientBuilds[] =
{
	{ 1499,  0, 1, 0, ' ' },
	{ 0,  0, 0, 0, ' ' }                                   // terminator
};

RealmBuildInfo const* FindBuildInfo(uint16 _build)
{
	// first build is low bound of always accepted range
	if (_build >= ExpectedRealmdClientBuilds[0].build)
		return &ExpectedRealmdClientBuilds[0];

	// continue from 1 with explicit equal check
	for (int i = 1; ExpectedRealmdClientBuilds[i].build; ++i)
		if (_build == ExpectedRealmdClientBuilds[i].build)
			return &ExpectedRealmdClientBuilds[i];

	// none appropriate build
	return nullptr;
}

RealmList::RealmList() : m_UpdateInterval(0), m_NextUpdateTime(time(nullptr))
{
}

RealmList& sRealmList
{
	static RealmList realmlist;
return realmlist;
}

/// Load the realm list from the database
void RealmList::Initialize(uint32 updateInterval)
{
	m_UpdateInterval = updateInterval;

	///- Get the content of the realmlist table in the database
	UpdateRealms(true);
}

void RealmList::UpdateRealm(uint32 ID, const std::string& name, const std::string& address, uint32 port, uint8 icon, RealmFlags realmflags, uint8 timezone, AccountTypes allowedSecurityLevel, float popu, const std::string& builds)
{
	///- Create new if not exist or update existed
	Realm& realm = m_realms[name];

	realm.m_ID = ID;
	realm.icon = icon;
	realm.realmflags = realmflags;
	realm.timezone = timezone;
	realm.allowedSecurityLevel = allowedSecurityLevel;
	realm.populationLevel = popu;

	Tokens tokens = StrSplit(builds, " ");
	Tokens::iterator iter;

	for (iter = tokens.begin(); iter != tokens.end(); ++iter)
	{
		uint32 build = atol((*iter).c_str());
		realm.realmbuilds.insert(build);
	}

	uint16 first_build = !realm.realmbuilds.empty() ? *realm.realmbuilds.begin() : 0;

	realm.realmBuildInfo.build = first_build;
	realm.realmBuildInfo.major_version = 0;
	realm.realmBuildInfo.minor_version = 0;
	realm.realmBuildInfo.bugfix_version = 0;
	realm.realmBuildInfo.hotfix_version = ' ';

	if (first_build)
		if (RealmBuildInfo const* bInfo = FindBuildInfo(first_build))
			if (bInfo->build == first_build)
				realm.realmBuildInfo = *bInfo;

	///- Append port to IP address.
	std::ostringstream ss;
	ss << address << ":" << port;
	realm.address = ss.str();
}

void RealmList::UpdateIfNeed()
{
	// maybe disabled or updated recently
	if (!m_UpdateInterval || m_NextUpdateTime > time(nullptr))
		return;

	m_NextUpdateTime = time(nullptr) + m_UpdateInterval;

	// Clears Realm list
	m_realms.clear();

	// Get the content of the realmlist table in the database
	UpdateRealms(false);
}

void RealmList::UpdateRealms(bool init)
{
	sLog.outString("Updating Realm List...");

	////                                               0   1     2        3     4     5           6         7                     8           9
	QueryResult* result = LoginDatabase.Query("SELECT id, name, address, port, icon, realmflags, timezone, allowedSecurityLevel, population, realmbuilds FROM realmlist WHERE (realmflags & 1) = 0 ORDER BY name");

	///- Circle through results and add them to the realm map
	if (result)
	{
		do
		{
			Field* fields = result->Fetch();

			uint32 Id = fields[0].GetUInt32();
			std::string name = fields[1].GetCppString();
			uint8 realmflags = fields[5].GetUInt8();
			uint8 allowedSecurityLevel = fields[7].GetUInt8();

			if (realmflags & ~(REALM_FLAG_OFFLINE | REALM_FLAG_NEW_PLAYERS | REALM_FLAG_RECOMMENDED | REALM_FLAG_SPECIFYBUILD))
			{
				sLog.outDebug("Realm (id %u, name '%s') can only be flagged as OFFLINE (mask 0x02), NEWPLAYERS (mask 0x20), RECOMMENDED (mask 0x40), or SPECIFICBUILD (mask 0x04) in DB", Id, name.c_str());
				realmflags &= (REALM_FLAG_OFFLINE | REALM_FLAG_NEW_PLAYERS | REALM_FLAG_RECOMMENDED | REALM_FLAG_SPECIFYBUILD);
			}

			UpdateRealm(
				Id, name, fields[2].GetCppString(), fields[3].GetUInt32(),
				fields[4].GetUInt8(), RealmFlags(realmflags), fields[6].GetUInt8(),
				(allowedSecurityLevel <= SEC_ADMINISTRATOR ? AccountTypes(allowedSecurityLevel) : SEC_ADMINISTRATOR),
				fields[8].GetFloat(), fields[9].GetCppString());

			if (init)
				sLog.outString("Added realm id %u, name '%s' flag '%d'", Id, name.c_str(), realmflags);
		} while (result->NextRow());
		delete result;
	}
}

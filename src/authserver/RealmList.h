#ifndef _REALMLIST_H
#define _REALMLIST_H

#include "Common.h"

struct RealmBuildInfo
{
	int build;
	int major_version;
	int minor_version;
	int bugfix_version;
	int hotfix_version;
};

RealmBuildInfo const* FindBuildInfo(uint16 _build);

typedef std::set<uint32> RealmBuilds;
#pragma pack(push,1)
/// Storage object for a realm
struct Realm
{
	std::string address;
	std::string name;
	uint8 AmountOfCharacters;
	uint8 icon;
	uint8 realmflags;                                  // realmflags
	uint8 timezone;
	uint32 m_ID;
	AccountTypes allowedSecurityLevel;                      // current allowed join security level (show as locked for not fit accounts)
	float populationLevel;
	RealmBuilds realmbuilds;                                // list of supported builds (updated in DB by worldsd)
	RealmBuildInfo realmBuildInfo;                          // build info for show version in list
};
#pragma pack(pop)
/// Storage object for the list of realms on the server
class RealmList
{
public:
	typedef std::map<std::string, Realm> RealmMap;

	static RealmList& Instance();

	RealmList();
	~RealmList() {}

	void Initialize(uint32 updateInterval);

	void UpdateIfNeed();

	RealmMap::const_iterator begin() const { return m_realms.begin(); }
	RealmMap::const_iterator end() const { return m_realms.end(); }
	uint32 size() const { return m_realms.size(); }
private:
	void UpdateRealms(bool init);
	void UpdateRealm(uint32 ID, const std::string& name, const std::string& address, uint32 port, uint8 icon, RealmFlags realmflags, uint8 timezone, AccountTypes allowedSecurityLevel, float popu, const std::string& builds);
private:
	RealmMap m_realms;                                  ///< Internal map of realms
	uint32   m_UpdateInterval;
	time_t   m_NextUpdateTime;
};

#define sRealmList RealmList::Instance()

#endif
/// @}

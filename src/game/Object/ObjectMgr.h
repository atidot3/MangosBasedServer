#ifndef _OBJECTMGR_H
#define _OBJECTMGR_H

#include "Common.h"
#include "Object.h"
//#include "Bag.h"
//#include "Creature.h"
#include "Player.h"
//#include "GameObject.h"
//#include "QuestDef.h"
//#include "ItemPrototype.h"
//#include "NPCHandler.h"
#include "Database/DatabaseEnv.h"
#include "Map.h"
//#include "MapPersistentStateMgr.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"

#include <map>
#include <climits>

//class Group;
//class Item;
class SQLStorage;

class ObjectMgr
{
	//friend class PlayerDumpReader;
public:
	ObjectMgr();
	~ObjectMgr();

	PlayerClassInfo const* GetPlayerClassInfo(uint32 class_) const
	{
		if (class_ >= MAX_CLASSES) return nullptr;
		return &playerClassInfo[class_];
	}
	void GetPlayerClassLevelInfo(uint32 class_, uint32 level, PlayerClassLevelInfo* info) const;

	PlayerInfo const* GetPlayerInfo(uint32 class_) const
	{
		if (class_ >= MAX_CLASSES) return nullptr;
		PlayerInfo const* info = &playerInfo[class_];
		if (info->displayId_m == 0 || info->displayId_f == 0) return nullptr;
		return info;
	}
	ObjectGuid GetPlayerGuidByName(std::string name) const;
	bool GetPlayerNameByGUID(ObjectGuid guid, std::string& name) const;
	uint32 GetPlayerAccountIdByGUID(ObjectGuid guid) const;
	uint32 GetPlayerAccountIdByPlayerName(const std::string& name) const;

	// Static wrappers for various accessors
	static Player* GetPlayer(const char* name);         ///< Wrapper for ObjectAccessor::FindPlayerByName
	static Player* GetPlayer(ObjectGuid guid, bool inWorld = true);             ///< Wrapper for ObjectAccessor::FindPlayer
	

	void LoadPlayerInfo();

	/// @param _map Map* of the map for which to load active entities. If nullptr active entities on continents are loaded
	void LoadActiveEntities(Map* _map);

	uint32 GetBaseXP(uint32 level) const;
	uint32 GetXPForLevel(uint32 level) const;

	void SetHighestGuids();

	// used for set initial guid counter for map local guids

	uint32 GeneratePlayerLowGuid() { return m_CharGuids.Generate(); }
	
	// name with valid structure and symbols
	static uint8 CheckPlayerName(const std::string& name, bool create = false);
	static bool IsValidCharterName(const std::string& name);

protected:
	// first free low guid for selected guid type
	ObjectGuidGenerator<HIGHGUID_PLAYER>     m_CharGuids;
	// character reserved names
	typedef std::set<std::wstring> ReservedNamesMap;
	ReservedNamesMap    m_ReservedNames;

private:
	PlayerClassInfo playerClassInfo[MAX_CLASSES];
	PlayerInfo playerInfo[MAX_CLASSES];
};

#define sObjectMgr Origin::Singleton<ObjectMgr>::Instance()

#endif

#ifndef _PLAYER_H
#define _PLAYER_H

#include "Common.h"
#include "Unit.h"
#include "Database/DatabaseEnv.h"
#include "../Server/WorldSession.h"
#include "MapReference.h"
#include "Util.h"                                           // for Tokens typedef
#include "../Server/SharedDefine.h"

#include <vector>

#define MAX_MONEY_AMOUNT        (0x7FFFFFFF-1)

struct PlayerClassLevelInfo
{
	PlayerClassLevelInfo() : basehealth(0), baseenergy(0) {}
	uint16 basehealth;
	uint16 baseenergy;
};
struct PlayerClassInfo
{
	PlayerClassInfo() : levelInfo(nullptr) { }

	PlayerClassLevelInfo* levelInfo;                        //[level-1] 0..MaxPlayerLevel-1
};
struct PlayerInfo
{
	// existence checked by displayId != 0             // existence checked by displayId != 0
	PlayerInfo() : mapId(0), areaId(0), positionX(0.f), positionY(0.f), positionZ(0.f), orientation(0.f), displayId_m(0), displayId_f(0)
	{
	}

	uint32 mapId;
	uint32 areaId;
	float positionX;
	float positionY;
	float positionZ;
	float orientation;
	uint16 displayId_m;
	uint16 displayId_f;
	//PlayerCreateInfoItems item;
	//PlayerCreateInfoSpells spell;
	//PlayerCreateInfoActions action;
};
struct sPC_SHAPE
{
	uint8			hair;
	uint8			hairColor;
	uint8			jaw;
	uint8			skin;
	uint8			nose;
	uint8			eyes;
	uint8			outfitId;
};
struct sPC_DATA
{
	uint64			guid;
	std::string		name;

	uint8			byClass;
	uint32			dwEXP;
	uint8			byLevel;
	uint32			health;
	uint32			energy;
	uint32			power1;
	uint32			dwMoney;
	uint32			dwSpPoint;
	uint32			flag;
	sPC_SHAPE		sShape;
};
// used at player loading query list preparing, and later result selection
enum PlayerLoginQueryIndex
{
	PLAYER_LOGIN_QUERY_LOADFROM,
	PLAYER_LOGIN_QUERY_LOADGROUP,
	PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES,
	PLAYER_LOGIN_QUERY_LOADAURAS,
	PLAYER_LOGIN_QUERY_LOADSPELLS,
	PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,
	PLAYER_LOGIN_QUERY_LOADHONORCP,
	PLAYER_LOGIN_QUERY_LOADREPUTATION,
	PLAYER_LOGIN_QUERY_LOADINVENTORY,
	PLAYER_LOGIN_QUERY_LOADITEMLOOT,
	PLAYER_LOGIN_QUERY_LOADACTIONS,
	PLAYER_LOGIN_QUERY_LOADSOCIALLIST,
	PLAYER_LOGIN_QUERY_LOADHOMEBIND,
	PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,
	PLAYER_LOGIN_QUERY_LOADGUILD,
	PLAYER_LOGIN_QUERY_LOADBGDATA,
	PLAYER_LOGIN_QUERY_LOADSKILLS,
	PLAYER_LOGIN_QUERY_LOADSTATS,
	PLAYER_LOGIN_QUERY_LOADMAILS,
	PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,

	MAX_PLAYER_LOGIN_QUERY
};

enum PlayerFlags
{
	PLAYER_FLAGS_NONE = 0x00000000,
	PLAYER_FLAGS_GROUP_LEADER = 0x00000001,
	PLAYER_FLAGS_AFK = 0x00000002,
	PLAYER_FLAGS_DND = 0x00000004,
	PLAYER_FLAGS_GM = 0x00000008,
	PLAYER_FLAGS_GHOST = 0x00000010,
	PLAYER_FLAGS_RESTING = 0x00000020,
	PLAYER_FLAGS_FFA_PVP = 0x00000040,
	PLAYER_FLAGS_CONTESTED_PVP = 0x00000080,       // Player has been involved in a PvP combat and will be attacked by contested guards
	PLAYER_FLAGS_IN_PVP = 0x00000100,
	PLAYER_FLAGS_HIDE_HELM = 0x00000200,
	PLAYER_FLAGS_HIDE_CLOAK = 0x00000400,
	PLAYER_FLAGS_PARTIAL_PLAY_TIME = 0x00000800,       // played long time
	PLAYER_FLAGS_NO_PLAY_TIME = 0x00001000,       // played too long time
	PLAYER_FLAGS_SANCTUARY = 0x00002000,       // player entered sanctuary
};
class Player : public Unit
{
	friend class WorldSession;
public:
	explicit Player(WorldSession* session);
	~Player();

	void CleanupsBeforeDelete() override;

	void AddToWorld() override;
	void RemoveFromWorld() override;

	bool Create(uint32 guidlow, const std::string& name, uint8 class_, uint8 gender, uint8 hair, uint8 hairColor, uint8 jaw, uint8 skin, uint8 nose, uint8 eyes, uint8 outfitId);
	void Update(uint32 update_diff, uint32 time) override;

	WorldSession* GetSession() const { return m_session; }
	void SetSession(WorldSession* s) { m_session = s; }

	MapReference& GetMapRef() { return m_mapRef; }
	// Played Time Stuff
	time_t m_logintime;
	time_t m_Last_tick;
	/*********************************************************/
	/***                   LOAD SYSTEM                     ***/
	/*********************************************************/
	uint32 GetMoney() const
	{
		return GetUInt32Value(PLAYER_FIELD_COINAGE);
	}
	void SetMoney(uint32 val)
	{ 
		SetUInt32Value(PLAYER_FIELD_COINAGE, val);
	}
	void ModifyMoney(uint32 d)
	{
		if (d < 0)
			SetMoney(GetMoney() > uint32(-d) ? GetMoney() + d : 0);
		else
			SetMoney(GetMoney() < uint32(MAX_MONEY_AMOUNT - d) ? GetMoney() + d : MAX_MONEY_AMOUNT);
	}
	bool LoadFromDB(ObjectGuid guid, SqlQueryHolder* holder);
	void _LoadSkills(QueryResult* result);
	void _LoadStats(QueryResult* result);

	uint32 GetPlayerXp()const { return GetUInt32Value(PLAYER_XP); }
	void SetPlayerXp(uint32 val) { SetUInt32Value(PLAYER_XP, val); }
	void SetPlayerNextXpLevel(uint32 val) { SetUInt32Value(PLAYER_NEXT_LEVEL_XP, val); }
	uint32 GetNextLevelXp()const { return GetUInt32Value(PLAYER_NEXT_LEVEL_XP); }
	static uint32 GetLevelFromDB(uint32 guid);
	static bool   LoadPositionFromDB(ObjectGuid guid, uint32& mapid, float& x, float& y, float& z, float& o, bool& in_flight);

	void InitStatsForLevel(bool reapplyMods = false);
	void outDebugStatsValues() const;
	/*********************************************************/
	/***                   SAVE SYSTEM                     ***/
	/*********************************************************/

	void SaveToDB();
	void _SaveStats();

	static void SavePositionInDB(ObjectGuid guid, uint32 mapid, float x, float y, float z, float o, uint32 zone);

	static void DeleteFromDB(uint32 playerguid, uint32 accountId, bool updateRealmChars = true, bool deleteFinally = false);
	static void DeleteOldCharacters();
	static void DeleteOldCharacters(uint32 keepDays);

	bool IsInWater() const override { return false; }
	bool IsUnderWater() const override { return false; }


	void RegenerateAll();
	void Regenerate(Powers power);
	void RegenerateHealth();

	WorldSession* m_session;


	/*********************************************************/
	/***                  LIST  SYSTEM                     ***/
	/*********************************************************/
	std::mutex		mutexPlayerList;
	void			LogoutList();
	bool			isInList(uint64 guid);
	void			addToList(Player*);
	void			removeFromList(uint64);
	//void			checkListClear();
	void			SendToOther(WorldPacket &packet, bool immediate = false);

	/*********************************************************/
	/***                UPDATE  SYSTEM                     ***/
	/*********************************************************/
public:
	void BuildCreateUpdateBlockForPlayer(Player* target) const override;

	void CreateObject() override;
	void UpdateObject() override;
	void DeleteObject() override;

private:
	MapReference								m_mapRef;
	std::map<uint64, Player*>					plrList;
	uint32										guildID;

	/*********************************************************/
	/***                MOVE   SYSTEM                      ***/
	/*********************************************************/

};

#endif
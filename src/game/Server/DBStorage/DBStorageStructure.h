#include "Common.h"
#include "../Define.h"
#include "../Server/SharedDefine.h"

struct MapEntry
{
	uint32  MapID;                                          // 0        m_ID
	uint32	internalname;									// 1        m_Directory
	uint32	map_flag;										// 2		flag
	uint32  map_type;                                       // 3		type
	uint32  is_battleground;								// 4		battleground
	uint32	name;											// 5		name
	uint32  linked_zone;                                    // 6		areaid
	uint32	mapDesc;										// 7		description
	uint32  loadingScreen;
	uint32  timeofday;
	uint32  expansion;
	uint32  maxPlayer;

	bool IsDungeon() const { return map_type == MAP_INSTANCE || map_type == MAP_RAID; }
	bool IsNonRaidDungeon() const { return map_type == MAP_INSTANCE; }
	bool Instanceable() const { return map_type == MAP_INSTANCE || map_type == MAP_RAID || map_type == MAP_BATTLEGROUND; }
	bool IsRaid() const { return map_type == MAP_RAID; }
	bool IsBattleGround() const { return map_type == MAP_BATTLEGROUND; }
	bool IsMountAllowed() const
	{
		return !IsDungeon() ||
			MapID == 309 || MapID == 209 || MapID == 509 || MapID == 269;
	}
	bool IsContinent() const
	{
		return MapID == 0 || MapID == 1;
	}
};
struct AreaTable
{
	uint32	id;
	uint32	mapID;
	uint32	exploreFlag;
	uint32	flags;
	uint32	soundAmbienceID;
	uint32	areaLevel;
	uint32	areaName;
};
struct ClassLevelStats
{
	uint32	id;
	uint32	classe;
	uint32	level;
	uint32	basehp;
	uint32	baseenergy;
};
struct LevelStats
{
	uint32	id;
	uint32	classe;
	uint32	level;
	uint32	str;
	uint32	con;
	uint32	dex;
	uint32	inte;
	uint32	eng;
};
struct XpForLevel
{
	uint32 lvl;
	uint32 xpForNextLevel;
};
struct playerCreateInfo
{
	uint32	id;
	uint32	classe;
	uint32	map;
	uint32	zone;
	float	x;
	float	y;
	float	z;
	float	o;
};
struct playerCreateInfoItem
{
	uint32	id;
	uint32	classe;
	uint32	itemID;
	uint32	amount;
};
struct playerCreateInfoSpell
{
	uint32	id;
	uint32	classemask;
	uint32	spell;
	uint32	note;
};
/*
* This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Config/Singleton.h"

#include "DBStorage/SQLStorages.h"
#include "Log.h"
#include "MapManager.h"
#include "ObjectGuid.h"
//#include "ScriptMgr.h"
//#include "SpellMgr.h"
#include "World.h"
//#include "Group.h"
//#include "Transports.h"
//#include "ProgressBar.h"
//#include "Language.h"
//#include "PoolManager.h"
//#include "GameEventMgr.h"
//#include "Chat.h"
//#include "MapPersistentStateMgr.h"
//#include "SpellAuras.h"
#include "Util.h"
//#include "GossipDef.h"
//#include "Mail.h"
//#include "Formulas.h"
//#include "InstanceData.h"
//#include "ItemEnchantmentMgr.h"
//#include "LootMgr.h"

#include <limits>
#include <cstdarg>
#include <boost\regex.hpp>

INSTANTIATE_SINGLETON_1(ObjectMgr);

ObjectMgr::ObjectMgr()
{
}

ObjectMgr::~ObjectMgr()
{
	// free only if loaded
	for (int class_ = 0; class_ < MAX_CLASSES; ++class_)
		delete[] playerClassInfo[class_].levelInfo;
}

// name must be checked to correctness (if received) before call this function
ObjectGuid ObjectMgr::GetPlayerGuidByName(std::string name) const
{
	ObjectGuid guid;

	CharacterDatabase.escape_string(name);

	// Player name safe to sending to DB (checked at login) and this function using
	QueryResult* result = CharacterDatabase.PQuery("SELECT guid FROM characters WHERE name = '%s'", name.c_str());
	if (result)
	{
		guid = ObjectGuid(HIGHGUID_PLAYER, (*result)[0].GetUInt32());

		delete result;
	}

	return guid;
}

bool ObjectMgr::GetPlayerNameByGUID(ObjectGuid guid, std::string& name) const
{
	// prevent DB access for online player
	if (Player* player = GetPlayer(guid))
	{
		name = player->GetName();
		return true;
	}

	uint32 lowguid = guid.GetCounter();

	QueryResult* result = CharacterDatabase.PQuery("SELECT name FROM characters WHERE guid = '%u'", lowguid);

	if (result)
	{
		name = (*result)[0].GetCppString();
		delete result;
		return true;
	}

	return false;
}

uint32 ObjectMgr::GetPlayerAccountIdByGUID(ObjectGuid guid) const
{
	if (!guid.IsPlayer())
		return 0;

	// prevent DB access for online player
	if (Player* player = GetPlayer(guid))
		return player->GetSession()->GetAccountId();

	uint32 lowguid = guid.GetCounter();

	QueryResult* result = CharacterDatabase.PQuery("SELECT account FROM characters WHERE guid = '%u'", lowguid);
	if (result)
	{
		uint32 acc = (*result)[0].GetUInt32();
		delete result;
		return acc;
	}

	return 0;
}

uint32 ObjectMgr::GetPlayerAccountIdByPlayerName(const std::string& name) const
{
	QueryResult* result = CharacterDatabase.PQuery("SELECT account FROM characters WHERE name = '%s'", name.c_str());
	if (result)
	{
		uint32 acc = (*result)[0].GetUInt32();
		delete result;
		return acc;
	}

	return 0;
}

void ObjectMgr::LoadPlayerInfo()
{
	// Load playercreate
	{
		uint32 count = 0;

		sCreateInfoEntry.Load();
		for (uint32 i = 1; i < sCreateInfoEntry.GetMaxEntry(); ++i)
		{
			playerCreateInfo const* createInfo = sCreateInfoEntry.LookupEntry<playerCreateInfo>(i);
			if (!createInfo)
			{
				sLog.outErrorDb("ObjectMgr::playerCreateInfo: bad template! for '%d'", i);
				sCreateInfoEntry.EraseEntry(i);
				continue;
			}

			uint32 current_class = createInfo->classe;
			PlayerInfo* pInfo = &playerInfo[current_class];

			pInfo->mapId = createInfo->map;
			pInfo->areaId = createInfo->zone;
			pInfo->positionX = createInfo->x;
			pInfo->positionY = createInfo->y;
			pInfo->positionZ = createInfo->z;
			pInfo->orientation = createInfo->o;

			pInfo->displayId_m = -1;
			pInfo->displayId_f = -1;
			++count;
		}
		sLog.outString();
		sLog.outString(">> Loaded %u player create definitions", count);
	}

	// Load playercreate items
	{
		uint32 count = 0;
		sPlayerCreateInfoItem.Load();
		for (uint32 i = 1; i < sPlayerCreateInfoItem.GetMaxEntry(); ++i)
		{
			playerCreateInfoItem const* createInfoitem = sPlayerCreateInfoItem.LookupEntry<playerCreateInfoItem>(i);
			if (!createInfoitem)
			{
				sLog.outErrorDb("ObjectMgr::playerCreateInfoItem: bad template! for '%d'", i);
				sPlayerCreateInfoItem.EraseEntry(i);
				continue;
			}

			uint32 current_class = createInfoitem->classe;
			PlayerInfo* pInfo = &playerInfo[current_class];
			uint32 item_id = createInfoitem->itemID;
			/*if (!GetItemPrototype(item_id))
			{
				sLog.outErrorDb("Item id %u (class %u) in `playercreateinfo_item` table but not listed in `item_template`, ignoring.", item_id, current_class);
				continue;
			}*/
			uint32 amount = createInfoitem->amount;
			if (!amount)
			{
				sLog.outErrorDb("Item id %u (class %u ) have amount==0 in `playercreateinfo_item` table, ignoring.", item_id, current_class);
				continue;
			}
			//pInfo->item.push_back(PlayerCreateInfoItem(item_id, amount));
			++count;
		}
		sLog.outString();
		sLog.outString(">> Loaded %u custom player create items", count);
	}


	// Load playercreate spells
	{
		uint32 count = 0;

		sPlayerCreateInfoSpellEntry.Load();
		for (uint32 i = 1; i < sPlayerCreateInfoSpellEntry.GetMaxEntry(); ++i)
		{
			playerCreateInfoSpell const* createInfospell = sPlayerCreateInfoSpellEntry.LookupEntry<playerCreateInfoSpell>(i);
			if (!createInfospell)
			{
				sLog.outErrorDb("ObjectMgr::playerCreateInfoSpell: bad template! for '%d'", i);
				sPlayerCreateInfoSpellEntry.EraseEntry(i);
				continue;
			}

			uint32 current_class = createInfospell->classemask;

			uint32 spell_id = createInfospell->spell;
/*			if (!sSpellStore.LookupEntry(spell_id))
			{
				sLog.outErrorDb("Non existing spell %u in `playercreateinfo_spell` table, ignoring.", spell_id);
				continue;
			}
*/
			PlayerInfo* pInfo = &playerInfo[current_class];
			//pInfo->spell.push_back(spell_id);

			++count;
		}

		sLog.outString();
		sLog.outString(">> Loaded %u player create spells", count);
	}

	// Load playercreate actions
	{
	}

	// Loading levels data (class only dependent)
	{
		uint32 count = 0;

		sClassLevelStatsEntry.Load();
		for (uint32 p = 1; p < sClassLevelStatsEntry.GetMaxEntry(); ++p)
		{
			ClassLevelStats const* classEntry = sClassLevelStatsEntry.LookupEntry<ClassLevelStats>(p);
			if (!classEntry)
			{
				sLog.outErrorDb("ObjectMgr::ClassLevelStats: bad template! for '%d'", p);
				sClassLevelStatsEntry.EraseEntry(p);
				continue;
			}


			uint32 current_class = classEntry->classe;
			if (current_class >= MAX_CLASSES)
			{
				sLog.outErrorDb("Wrong class %u in `player_classlevelstats` table, ignoring.", current_class);
				continue;
			}

			uint32 current_level = classEntry->level;
			if (current_level == 0)
			{
				sLog.outErrorDb("Wrong level %u in `player_classlevelstats` table, ignoring.", current_level);
				continue;
			}
			else if (current_level > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
			{
				if (current_level > 255)       // hardcoded level maximum
					sLog.outErrorDb("Wrong (> %u) level %u in `player_classlevelstats` table, ignoring.", 255, current_level);
				else
				{
					DETAIL_LOG("Unused (> MaxPlayerLevel in world.conf) level %u in `player_classlevelstats` table, ignoring.", current_level);
					++count;                                // make result loading percent "expected" correct in case disabled detail mode for example.
				}
				continue;
			}

			PlayerClassInfo* pClassInfo = &playerClassInfo[current_class];

			if (!pClassInfo->levelInfo)
				pClassInfo->levelInfo = new PlayerClassLevelInfo[sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL)];

			PlayerClassLevelInfo* pClassLevelInfo = &pClassInfo->levelInfo[current_level - 1];

			pClassLevelInfo->basehealth = classEntry->basehp;
			pClassLevelInfo->baseenergy = classEntry->baseenergy;
			++count;
		}
		sLog.outString();
		sLog.outString(">> Loaded %u level health/mana definitions", count);
	}

	// Fill gaps and check integrity
	for (int class_ = 1; class_ < MAX_CLASSES; ++class_)
	{
		PlayerClassInfo* pClassInfo = &playerClassInfo[class_];

		// fatal error if no level 1 data
		if (!pClassInfo->levelInfo || pClassInfo->levelInfo[0].basehealth == 0)
		{
			sLog.outErrorDb("Class %i Level 1 does not have health/mana data!", class_);
			Log::WaitBeforeContinueIfNeed();
			exit(1);
		}

		// fill level gaps
		for (uint32 level = 1; level < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL); ++level)
		{
			if (pClassInfo->levelInfo[level].basehealth == 0)
			{
				sLog.outErrorDb("Class %i Level %i does not have health/mana data. Using stats data of level %i.", class_, level + 1, level);
				pClassInfo->levelInfo[level] = pClassInfo->levelInfo[level - 1];
			}
		}
	}
	// Fill gaps and check integrity
	for (int class_ = 1; class_ < MAX_CLASSES; ++class_)
	{
		PlayerInfo* pInfo = &playerInfo[class_];

		// skip non loaded combinations
		if (!pInfo->displayId_m || !pInfo->displayId_f)
			continue;
	}

	// Loading xp per level data
	{
		uint32 count = 0;
		sXpForLevelEntry.Load();
		for (uint32 i = 1; i < sXpForLevelEntry.GetMaxEntry(); ++i)
		{
			XpForLevel const* xpForLevelEntry = sXpForLevelEntry.LookupEntry<XpForLevel>(i);
			if (!xpForLevelEntry)
			{
				sLog.outErrorDb("ObjectMgr::XpForLevel: bad template! for '%d'", i);
				sXpForLevelEntry.EraseEntry(i);
				continue;
			}
			++count;
		}
		sLog.outString();
		sLog.outString(">> Loaded %u xp for level definitions", count);
	}
}
void ObjectMgr::GetPlayerClassLevelInfo(uint32 class_, uint32 level, PlayerClassLevelInfo* info) const
{
	if (level < 1 || class_ >= MAX_CLASSES)
		return;

	PlayerClassInfo const* pInfo = &playerClassInfo[class_];

	if (level > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
		level = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

	*info = pInfo->levelInfo[level - 1];
}
void ObjectMgr::SetHighestGuids()
{
	QueryResult* result = CharacterDatabase.Query("SELECT MAX(guid) FROM characters");
	if (result)
	{
		m_CharGuids.Set((*result)[0].GetUInt32() + 1);
		delete result;
	}

	/*result = WorldDatabase.Query("SELECT MAX(guid) FROM creature");
	if (result)
	{
		m_FirstTemporaryCreatureGuid = (*result)[0].GetUInt32() + 1;
		delete result;
	}

	result = CharacterDatabase.Query("SELECT MAX(guid) FROM item_instance");
	if (result)
	{
		m_ItemGuids.Set((*result)[0].GetUInt32() + 1);
		delete result;
	}
	// Cleanup other tables from nonexistent guids (>=m_hiItemGuid)
	CharacterDatabase.BeginTransaction();
	CharacterDatabase.PExecute("DELETE FROM character_inventory WHERE item >= '%u'", m_ItemGuids.GetNextAfterMaxUsed());
	CharacterDatabase.PExecute("DELETE FROM mail_items WHERE item_guid >= '%u'", m_ItemGuids.GetNextAfterMaxUsed());
	CharacterDatabase.PExecute("DELETE FROM auction WHERE itemguid >= '%u'", m_ItemGuids.GetNextAfterMaxUsed());
	CharacterDatabase.CommitTransaction();

	result = WorldDatabase.Query("SELECT MAX(guid) FROM gameobject");
	if (result)
	{
		m_FirstTemporaryGameObjectGuid = (*result)[0].GetUInt32() + 1;
		delete result;
	}

	result = CharacterDatabase.Query("SELECT MAX(id) FROM auction");
	if (result)
	{
		m_AuctionIds.Set((*result)[0].GetUInt32() + 1);
		delete result;
	}

	result = CharacterDatabase.Query("SELECT MAX(id) FROM mail");
	if (result)
	{
		m_MailIds.Set((*result)[0].GetUInt32() + 1);
		delete result;
	}

	result = CharacterDatabase.Query("SELECT MAX(id) FROM item_text");
	if (result)
	{
		m_ItemTextIds.Set((*result)[0].GetUInt32() + 1);
		delete result;
	}

	result = CharacterDatabase.Query("SELECT MAX(guid) FROM corpse");
	if (result)
	{
		m_CorpseGuids.Set((*result)[0].GetUInt32() + 1);
		delete result;
	}

	result = CharacterDatabase.Query("SELECT MAX(guildid) FROM guild");
	if (result)
	{
		m_GuildIds.Set((*result)[0].GetUInt32() + 1);
		delete result;
	}

	result = CharacterDatabase.Query("SELECT MAX(groupId) FROM groups");
	if (result)
	{
		m_GroupIds.Set((*result)[0].GetUInt32() + 1);
		delete result;
	}

	// setup reserved ranges for static guids spawn
	m_StaticCreatureGuids.Set(m_FirstTemporaryCreatureGuid);
	m_FirstTemporaryCreatureGuid += sWorld.getConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE);

	m_StaticGameObjectGuids.Set(m_FirstTemporaryGameObjectGuid);
	m_FirstTemporaryGameObjectGuid += sWorld.getConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT);
	*/
}

uint32 ObjectMgr::GetBaseXP(uint32 level) const
{
	return sXpForLevelEntry.LookupEntry<XpForLevel>(level)->xpForNextLevel;
}

uint32 ObjectMgr::GetXPForLevel(uint32 level) const
{
	if (level <= 0)
		level = 1;
	return sXpForLevelEntry.LookupEntry<XpForLevel>(level)->xpForNextLevel;
}

uint8 ObjectMgr::CheckPlayerName(const std::string& name, bool create)
{
	std::wstring wname;
	if (!Utf8toWStr(name, wname))
		return CHAR_NAME_INVALID_CHARACTER;
	if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos)
	{
		sLog.outError("Error name contain invalid character\n");
		return CHAR_NAME_INVALID_CHARACTER;
	}
	if (wname.size() > 15)
		return CHAR_NAME_TOO_LONG;
	
	uint32 minName = sWorld.getConfig(CONFIG_UINT32_MIN_PLAYER_NAME);
	if (wname.size() < minName)
		return CHAR_NAME_TOO_SHORT;

	uint32 strictMask = sWorld.getConfig(CONFIG_UINT32_STRICT_PLAYER_NAMES);
	/*if (!isValidString(wname, strictMask, false, create))
		return CHAR_NAME_MIXED_LANGUAGES;
		*/
	return CHAR_NAME_SUCCESS;
}

bool ObjectMgr::IsValidCharterName(const std::string& name)
{
	std::wstring wname;
	if (!Utf8toWStr(name, wname))
		return false;

	if (wname.size() > 24)
		return false;

	uint32 minName = sWorld.getConfig(CONFIG_UINT32_MIN_CHARTER_NAME);
	if (wname.size() < minName)
		return false;

	uint32 strictMask = sWorld.getConfig(CONFIG_UINT32_STRICT_CHARTER_NAMES);

	return true;
}

void ObjectMgr::LoadActiveEntities(Map* _map)
{
	// Special case on startup - load continents
	if (!_map)
	{
		uint32 continents[] = { 0, 1 };
		for (int i = 0; i < countof(continents); ++i)
		{
			_map = sMapMgr.FindMap(continents[i]);
			if (!_map)
				_map = sMapMgr.CreateMap(continents[i], nullptr);

			if (_map)
				LoadActiveEntities(_map);
			else
				sLog.outError("ObjectMgr::LoadActiveEntities - Unable to create Map %u", continents[i]);
		}

		return;
	}
	/*
	// Load active objects for _map
	if (sWorld.isForceLoadMap(_map->GetId()))
	{
		for (CreatureDataMap::const_iterator itr = mCreatureDataMap.begin(); itr != mCreatureDataMap.end(); ++itr)
		{
			if (itr->second.mapid == _map->GetId())
				_map->ForceLoadGrid(itr->second.posX, itr->second.posY);
		}
	}
	else                                                    // Normal case - Load all npcs that are active
	{
		std::pair<ActiveCreatureGuidsOnMap::const_iterator, ActiveCreatureGuidsOnMap::const_iterator> bounds = m_activeCreatures.equal_range(_map->GetId());
		for (ActiveCreatureGuidsOnMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
		{
			CreatureData const& data = mCreatureDataMap[itr->second];
			_map->ForceLoadGrid(data.posX, data.posY);
		}
	}

	// Load Transports on Map _map
	*/
}
/* ********************************************************************************************* */
/* *                                Static Wrappers                                              */
/* ********************************************************************************************* */
Player* ObjectMgr::GetPlayer(ObjectGuid guid, bool inWorld /*=true*/) { return ObjectAccessor::FindPlayer(guid, inWorld); }
#include "Player.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "../Server/Opcodes.h"
#include "../World/World.h"
#include <WorldPacket.h>
#include "../Server/WorldSession.h"
#include "UpdateData.h"
#include "Util.h"
#include "Database/DatabaseImpl.h"
#include "DBStorage/SQLStorages.h"
#include "ObjectMgr.h"
#include <cmath>


//== Player ====================================================

Player::Player(WorldSession* session) : Unit()
{
	m_objectType |= TYPEMASK_PLAYER;
	m_objectTypeId = TYPEID_PLAYER;

	m_valuesCount = PLAYER_END;

	m_session = session;

	if (GetSession()->GetSecurity() >= SEC_GAMEMASTER)
	{
	}

	// players always accept
	if (GetSession()->GetSecurity() == SEC_PLAYER)
	{
	}

	m_logintime = time(nullptr);
	m_Last_tick = m_logintime;
	plrList.clear();
}
void	Player::LogoutList()
{
	std::lock_guard<std::mutex> guard(mutexPlayerList);
	for (auto it = plrList.begin(); it != plrList.end();)
	{
		it = plrList.erase(it++);
	}
	plrList.clear();
}
Player::~Player()
{
	CleanupsBeforeDelete();
}

void Player::CleanupsBeforeDelete()
{
	Unit::CleanupsBeforeDelete();
}
void Player::AddToWorld()
{
	///- Do not add/remove the player from the object storage
	///- It will crash when updating the ObjectAccessor
	///- The player should only be added when logging in
	Unit::AddToWorld();
}
void Player::RemoveFromWorld()
{
	// cleanup
	if (IsInWorld())
	{
		///- Release charmed creatures, unsummon totems and remove pets/guardians
	}
	Unit::RemoveFromWorld();
}
bool Player::isInList(uint64 guid)
{
	std::lock_guard<std::mutex> guard(mutexPlayerList);
	auto it = plrList.find(guid);
	if (it == plrList.end())
	{
		return false;
	}
	if (it->second != NULL)
	{
		if (it->second->IsInWorld() == true)
		{
			if (it->second->GetGUID() == guid)
				return true;
		}
	}
	return false;
}
/*void Player::checkListClear()
{
	std::lock_guard<std::mutex> guard(mutexPlayerList);
	for (auto it = plrList.begin(); it != plrList.end();)
	{
		if (it->second != NULL)
		{
			if (it->second->IsInWorld() == false)
			{
				//sLog.outDebug("Player '%d' has been remove from plrList in check list clear", it->first);
				removeFromList(it->first);
			}
			else
				it++;
		}
		else
		{
			//sLog.outDebug("nill pointer has been remove from plrList in check list clear");
			removeFromList(it->first);
		}
	}
}*/
void Player::addToList(Player* other)
{
	std::lock_guard<std::mutex> guard(mutexPlayerList);
	plrList.insert(std::make_pair(other->GetGUID(), other));
	Unit::BuildCreateUpdateBlockForPlayer(other);
	//sLog.outDebug("Player '%s' has been added to '%s' List", other->GetName(), this->GetName());
}
void Player::removeFromList(uint64 other)
{
	std::lock_guard<std::mutex> guard(mutexPlayerList);
	for (auto it = plrList.begin(); it != plrList.end();)
	{
		if (it->second != NULL)
		{
			if (it->first == other)
			{
				it = plrList.erase(it++);
				break;
			}
			else
				it++;
		}
	}
}
void Player::SendToOther(WorldPacket& packet, bool immediate)
{
	std::lock_guard<std::mutex> guard(mutexPlayerList);
	for (auto it = plrList.begin(); it != plrList.end();)
	{
		if (it->second != NULL)
		{
			if (it->second->IsInWorld() == true)
			{
				it->second->GetSession()->SendPacket(&packet, immediate);
			}
		}
		it++;
	}
}
void Player::UpdateObject()
{
	WorldPacket packet(SMSG_UPDATE_OBJECT, 200);

	packet << uint8(UPDATETYPE_VALUES);
	packet << uint8(m_objectTypeId);
	packet << GetGUIDLow();

	uint32 oldSize = packet.size();
	uint32 newSize = 0;
	for (uint16 index = 0; index < GetValuesCount(); ++index)
	{
		if (m_changedValues[index] == true)
		{
			newSize += 6;
			packet << index;
			packet << GetUInt32Value(index);
		}
	}
	packet.resize(oldSize + newSize);
	/// need to iterate over the list
	SendToOther(packet);
	GetSession()->SendPacket(&packet);
	ClearUpdateMask(false);
}
uint32 Player::GetLevelFromDB(uint32 guid)
{
	uint32 lowguid = guid;

	QueryResult* result = CharacterDatabase.PQuery("SELECT level FROM characters WHERE guid='%u'", lowguid);
	if (!result)
		return 0;

	Field* fields = result->Fetch();
	uint32 level = fields[0].GetUInt32();
	delete result;

	return level;
}

bool Player::LoadPositionFromDB(ObjectGuid guid, uint32& mapid, float& x, float& y, float& z, float& o, bool& in_flight)
{
	QueryResult* result = CharacterDatabase.PQuery("SELECT position_x,position_y,position_z,orientation,map,taxi_path FROM characters WHERE guid = '%u'", guid.GetCounter());
	if (!result)
		return false;

	Field* fields = result->Fetch();

	x = fields[0].GetFloat();
	y = fields[1].GetFloat();
	z = fields[2].GetFloat();
	o = fields[3].GetFloat();
	mapid = fields[4].GetUInt32();
	in_flight = !fields[5].GetCppString().empty();

	delete result;
	return true;
}
void Player::Update(uint32 update_diff, uint32 p_time)
{
	if (!IsInWorld())
		return;

	Unit::Update(update_diff, p_time);

	time_t now = time(nullptr);

	// Played time
	if (now > m_Last_tick)
	{
		uint32 elapsed = uint32(now - m_Last_tick);
		m_Last_tick = now;
	}
	//checkListClear();

	// REGEN
	if (m_regenTimer)
	{
		if (update_diff >= m_regenTimer)
			m_regenTimer = 0;
		else
			m_regenTimer -= update_diff;
	}
	if (isAlive())
	{
		RegenerateAll();
	}
}
void Player::Regenerate(Powers power)
{
	uint32 curValue = GetPower(power);
	uint32 maxValue = GetMaxPower(power);

	float addvalue = 0.0f;

	switch (power)
	{
	case POWER_ENERGY:                                  // Regenerate energy (rogue)
	{
		float EnergyRate = sWorld.getConfig(CONFIG_FLOAT_RATE_POWER_ENERGY);
		float maxPower = GetMaxPower(Powers::POWER_ENERGY);
		addvalue = (0.31f + 1.22f) * EnergyRate;
		break;
	}
	case POWER_HEALTH:
		break;
	}

	curValue += uint32(addvalue);
	if (curValue > maxValue)
		curValue = maxValue;
	SetPower(power, curValue);
}
void Player::RegenerateHealth()
{
	uint32 curValue = GetHealth();
	uint32 maxValue = GetMaxHealth();
	if (curValue >= maxValue)
		return;

	float HealthIncreaseRate = sWorld.getConfig(CONFIG_FLOAT_RATE_HEALTH);

	float addvalue = 0.0f;
	float maxHealth = GetMaxHealth();
	addvalue = (0.31f + 1.22f) * HealthIncreaseRate;

	if (addvalue < 0)
		addvalue = 0;

	ModifyHealth(int32(addvalue));
}
void Player::RegenerateAll()
{
	if (m_regenTimer != 0)
		return;

	// Not in combat or they have regeneration
	/*if (!isInCombat() || HasAuraType(SPELL_AURA_MOD_REGEN_DURING_COMBAT) ||
	HasAuraType(SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT) || IsPolymorphed())
	{
	RegenerateHealth();
	if (!isInCombat() && !HasAuraType(SPELL_AURA_INTERRUPT_REGEN))
	Regenerate(POWER_RAGE);
	}*/
	RegenerateHealth();
	Regenerate(POWER_ENERGY);
	m_regenTimer = REGEN_TIME_FULL;
}
void Player::outDebugStatsValues() const
{
	// optimize disabled debug output
	//if (!sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG) || sLog.HasLogFilter(LOG_FILTER_PLAYER_STATS))
	//	return;

	sLog.outDetail("HP is: \t\t\t%u/%u\t\tEP is: \t\t\t%u/%u", GetHealth(), GetMaxHealth(), GetPower(POWER_ENERGY),GetMaxPower(POWER_ENERGY));
	sLog.outDetail("STRENGTH is: \t\t\t%f\t\tSTAMINA is: \t\t\t%f", GetStat(STAT_STR), GetStat(STAT_STA));
	sLog.outDetail("INTELLECT is: \t\t\t%f\t\tHEALTH is: \t\t\t%f", GetStat(STAT_INT), GetStat(STAT_HEA));
	//sLog.outDebug("Armor is: \t\t%u\t\tBlock is: \t\t%f", GetArmor(), GetFloatValue(PLAYER_BLOCK_PERCENTAGE));
	sLog.outDebug("MIN_DAMAGE is: \t\t%f\tMAX_DAMAGE is: \t\t%f", GetFloatValue(UNIT_FIELD_MINDAMAGE), GetFloatValue(UNIT_FIELD_MAXDAMAGE));
	sLog.outDebug("MIN_OFFHAND_DAMAGE is: \t%f\tMAX_OFFHAND_DAMAGE is: \t%f", GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE), GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE));
	sLog.outDebug("MIN_RANGED_DAMAGE is: \t%f\tMAX_RANGED_DAMAGE is: \t%f", GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE), GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE));
	//sLog.outDebug("ATTACK_TIME is: \t%u\t\tRANGE_ATTACK_TIME is: \t%u", GetAttackTime(BASE_ATTACK), GetAttackTime(RANGED_ATTACK));
}
void Player::InitStatsForLevel(bool reapplyMods)
{
	//if (reapplyMods)                                        // reapply stats values only on .reset stats (level) command
	//	_RemoveAllStatBonuses();
	PlayerClassLevelInfo classInfo;
	uint32 level = getLevel();
	uint32 plClass = getClass();
	sObjectMgr.GetPlayerClassLevelInfo(plClass, level, &classInfo);
	SetPlayerNextXpLevel(sObjectMgr.GetXPForLevel(level));

	// reset before any aura state sources (health set/aura apply)
	//setAuraState(0);

	//UpdateSkillsForLevel();

	// set default cast time multiplier
	setCastSpeed(1.0f);

	// save base values
	for (int i = STAT_STR; i < MAX_STATS; ++i)
		SetStat(Stats(i), 0);
	SetCreateHealth(classInfo.basehealth);

	// set create powers
	SetCreateEnery(classInfo.baseenergy);

	/*
	MISSING ARMOR STATS SPELL ETC
	*/
	SetFloatValue(UNIT_FIELD_BASEATTACKTIME, 2000.0f);
	SetFloatValue(UNIT_FIELD_BASEATTACKTIME + 1, 2000.0f);  // offhand attack time
	SetFloatValue(UNIT_FIELD_RANGEDATTACKTIME, 2000.0f);

	SetFloatValue(UNIT_FIELD_MINDAMAGE, 0.0f);
	SetFloatValue(UNIT_FIELD_MAXDAMAGE, 0.0f);
	SetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, 0.0f);
	SetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, 0.0f);
	SetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, 0.0f);
	SetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, 0.0f);

	SetInt32Value(UNIT_FIELD_ATTACK_POWER, 0);
	SetInt32Value(UNIT_FIELD_ATTACK_POWER_MODS, 0);
	SetFloatValue(UNIT_FIELD_ATTACK_POWER_MULTIPLIER, 0.0f);
	SetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER, 0);
	SetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER_MODS, 0);
	SetFloatValue(UNIT_FIELD_RANGED_ATTACK_POWER_MULTIPLIER, 0.0f);

	// Base crit values (will be recalculated in UpdateAllStats() at loading and in _ApplyAllStatBonuses() at reset
	SetFloatValue(PLAYER_CRIT_PERCENTAGE, 0.0f);
	SetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE, 0.0f);

	SetFloatValue(PLAYER_PARRY_PERCENTAGE, 0.0f);
	SetFloatValue(PLAYER_BLOCK_PERCENTAGE, 0.0f);

	// Dodge percentage
	SetFloatValue(PLAYER_DODGE_PERCENTAGE, 0.0f);
	/*
		FLAG SYSTEM
	*/
	RemoveFlag(UNIT_FIELD_FLAGS,
		UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_DISABLE_MOVE |
		UNIT_FLAG_OOC_NOT_ATTACKABLE | UNIT_FLAG_PASSIVE |
		UNIT_FLAG_SILENCED | UNIT_FLAG_PACIFIED |
		UNIT_FLAG_IN_COMBAT | UNIT_FLAG_NOT_SELECTABLE |
		UNIT_FLAG_SKINNABLE);
	SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE);
	RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK | PLAYER_FLAGS_DND | PLAYER_FLAGS_GM | PLAYER_FLAGS_GHOST | PLAYER_FLAGS_FFA_PVP);
	// save new stats
	for (int i = POWER_ENERGY; i < MAX_POWERS; ++i)
		SetMaxPower(Powers(i), GetCreatePowers(Powers(i)));
	SetMaxHealth(classInfo.basehealth);                     // stamina bonus will applied later
															// set current level health and mana/energy to maximum after applying all mods.
	SetHealth(GetMaxHealth());
	SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
}
bool Player::LoadFromDB(ObjectGuid guid, SqlQueryHolder* holder)
{
	//			0     1        2     3      4       5    6   7      8            9             10
	// SELECT guid, account, name, class, gender, level, xp, money, playerBytes, playerBytes2, playerFlags,"
	// 11          12          13          14   15           16      17    18 
	//"position_x, position_y, position_z, map, orientation, online, zone, sppoint,"
	// 19		20		21		22		23			
	//"health, energy, power1, guildid, actionBars FROM characters WHERE guid = '%u'", GUID_LOPART(m_guid));
	QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADFROM);

	if (!result)
	{
		sLog.outError("%s not found in table `characters`, can't load. ", guid.GetString().c_str());
		return false;
	}

	Field* fields = result->Fetch();

	uint32 dbAccountId = fields[1].GetUInt32();

	// check if the character's account in the db and the logged in account match.
	// player should be able to load/delete character only with correct account!
	if (dbAccountId != GetSession()->GetAccountId())
	{
		sLog.outError("%s loading from wrong account (is: %u, should be: %u)",
			guid.GetString().c_str(), GetSession()->GetAccountId(), dbAccountId);
		delete result;
		return false;
	}

	Object::_Create(guid.GetCounter(), 0, HIGHGUID_PLAYER);

	m_name = fields[2].GetCppString();
	// check name limitations
	/*if (ObjectMgr::CheckPlayerName(m_name) != CHAR_NAME_SUCCESS ||
	(GetSession()->GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(m_name)))
	{
	delete result;
	CharacterDatabase.PExecute("UPDATE characters SET at_login = at_login | '%u' WHERE guid ='%u'",
	uint32(AT_LOGIN_RENAME), guid.GetCounter());
	return false;
	}*/

	// overwrite some data fields
	setClass(fields[3].GetUInt8());							// class
	uint8 gender = fields[4].GetUInt8() & 0x01;             // allowed only 1 bit values male/female cases (for fit drunk gender part)
	setGender(gender);										// gender

															//SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_SUPPORTABLE | UNIT_BYTE2_FLAG_UNK5);

	SetLevel(fields[5].GetUInt8());

	SetPlayerXp(fields[6].GetUInt32());
	uint32 money = fields[7].GetUInt32();
	if (money > MAX_MONEY_AMOUNT)
		money = MAX_MONEY_AMOUNT;
	SetMoney(money);
	SetUInt32Value(PLAYER_BYTES, fields[8].GetUInt32());
	SetUInt32Value(PLAYER_BYTES_2, fields[9].GetUInt32());
	SetUInt32Value(PLAYER_FLAGS, fields[10].GetUInt32());

	// Action bars state
	//SetByteValue(PLAYER_FIELD_BYTES, 2, fields[25].GetUInt8());

	// cleanup inventory related item value fields (its will be filled correctly in _LoadInventory)


	DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "Load Basic value of player %s is: ", m_name.c_str());
	outDebugStatsValues();

	// load home bind and check in same time class, it used later for restore broken positions

	Relocate(fields[11].GetFloat(), fields[12].GetFloat(), fields[13].GetFloat(), fields[15].GetFloat());
	SetLocationMapId(fields[14].GetUInt32());


	// reset stats before loading any modifiers
	InitStatsForLevel();

	// load the player's map here if it's not already loaded
	SetMap(sMapMgr.CreateMap(GetMapId(), this));

	time_t now = time(nullptr);

	// load skills after InitStatsForLevel because it triggering aura apply also
	_LoadSkills(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSKILLS));
	_LoadStats(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSTATS));
	// restore remembered power/health values (but not more max values)
	uint32 savedhealth = fields[19].GetUInt32();
	SetHealth(savedhealth > GetMaxHealth() ? GetMaxHealth() : savedhealth);
	for (uint32 i = 0; i < MAX_POWERS; ++i)
	{
		uint32 savedpower = fields[20 + i].GetUInt32();
		SetPower(Powers(i), savedpower > GetMaxPower(Powers(i)) ? GetMaxPower(Powers(i)) : savedpower);
	}

	DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "The value of player %s after load item and aura is: ", m_name.c_str());
	outDebugStatsValues();
	// all fields read
	delete result;

	// GM state
	if (GetSession()->GetSecurity() > SEC_PLAYER)
	{
	}
	return true;
}
bool Player::Create(uint32 guidlow, const std::string& name, uint8 class_, uint8 gender, uint8 hair, uint8 hairColor, uint8 jaw, uint8 skin, uint8 nose, uint8 eyes, uint8 outfitId)
{
	// FIXME: outfitId not used in player creating

	Object::_Create(guidlow, 0, HIGHGUID_PLAYER);

	m_name = name;
	PlayerInfo const* info = sObjectMgr.GetPlayerInfo(class_);
	if (!info)
	{
		sLog.outError("Player have incorrect class. Can't be loaded.");
		return false;
	}
	// player store gender in single bit
	if (gender != uint8(GENDER_MALE) && gender != uint8(GENDER_FEMALE))
	{
		sLog.outError("Invalid gender %u at player creating", uint32(gender));
		return false;
	}
	/*for (int i = 0; i < PLAYER_SLOTS_COUNT; ++i)
	m_items[i] = nullptr;*/

	SetLocationMapId(info->mapId);
	Relocate(info->positionX, info->positionY, info->positionZ, info->orientation);
	SetMap(sMapMgr.CreateMap(info->mapId, this));

	setClass(class_);
	setGender(gender);
	SetPowerType(POWER_ENERGY);

	SetByteValue(PLAYER_BYTES, 0, skin);
	SetByteValue(PLAYER_BYTES, 1, hair);
	sLog.outError("HAIR SELECTED! %d", GetByteValue(PLAYER_BYTES, 1));
	sLog.outError("HAIR SELECTED! %d", hair);
	SetByteValue(PLAYER_BYTES, 2, hairColor);
	SetByteValue(PLAYER_BYTES, 3, jaw);
	SetByteValue(PLAYER_BYTES_2, 0, skin);
	SetByteValue(PLAYER_BYTES_2, 1, nose);
	SetByteValue(PLAYER_BYTES_2, 2, eyes);
	SetMoney(0);
	SetLevel(1);
	guildID = 0;
	// Played time
	m_Last_tick = time(nullptr);
	// base stats and related field values
	InitStatsForLevel();														// apply original stats mods before spell loading or item equipment that call before equip _RemoveStatsMods()
																				//UpdateMaxHealth();                                      // Update max Health (for add bonus from stamina)
																				// temp need to delete after adding stat system !
	SetMaxHealth(GetCreateHealth());

	SetHealth(GetMaxHealth());
	//UpdateMaxPower(POWER_ENERGY);                         // Update max Mana (for add bonus from intellect)
	// temp need to delete after adding stat system !
	SetMaxPower(POWER_ENERGY, GetCreateEnergy());
	SetPower(POWER_ENERGY, GetMaxPower(POWER_ENERGY));
	return true;
}
void Player::DeleteFromDB(uint32 playerguid, uint32 accountId, bool updateRealmChars, bool deleteFinally)
{
	// for nonexistent account avoid update realm
	if (accountId == 0)
		updateRealmChars = false;

	uint32 charDelete_method = sWorld.getConfig(CONFIG_UINT32_CHARDELETE_METHOD);
	uint32 charDelete_minLvl = sWorld.getConfig(CONFIG_UINT32_CHARDELETE_MIN_LEVEL);

	// if we want to finally delete the character or the character does not meet the level requirement, we set it to mode 0
	if (deleteFinally || Player::GetLevelFromDB(playerguid) < charDelete_minLvl)
		charDelete_method = 0;

	uint32 lowguid = playerguid;

	// convert corpse to bones if exist (to prevent exiting Corpse in World without DB entry)
	// bones will be deleted by corpse/bones deleting thread shortly
	//sObjectAccessor.ConvertCorpseForPlayer(playerguid);

	// remove from guild
	
	// the player was uninvited already on logout so just remove from group
	
	// remove signs from petitions (also remove petitions if owner);

	switch (charDelete_method)
	{
		// completely remove from the database
	case 0:
	{
		// return back all mails with COD and Item                 0  1           2              3      4       5          6     7
		
		// unsummon and delete for pets in world is not required: player deleted from CLI or character list with not loaded pet.
		// Get guids of character's pets, will deleted in transaction

		// delete char from friends list when selected chars is online (non existing - error)
		// NOW we can finally clear other DB data related to character

		// cleanup friends for online players, offline case will cleanup later in code
		CharacterDatabase.PExecute("DELETE FROM character_stats WHERE guid = '%u'", lowguid);
		CharacterDatabase.PExecute("DELETE FROM character_skills WHERE guid = '%u'", lowguid);
		CharacterDatabase.PExecute("DELETE FROM characters WHERE guid = '%u'", lowguid);
		CharacterDatabase.CommitTransaction();
		break;
	}
	// The character gets unlinked from the account, the name gets freed up and appears as deleted ingame
	case 1:
		CharacterDatabase.PExecute("UPDATE characters SET deleteInfos_Name=name, deleteInfos_Account=account, deleteDate='" UI64FMTD "', name='', account=0 WHERE guid=%u", uint64(time(nullptr)), lowguid);
		break;
	default:
		sLog.outError("Player::DeleteFromDB: Unsupported delete method: %u.", charDelete_method);
	}

	//if (updateRealmChars)
		//sWorld.UpdateRealmCharCount(accountId);
}
void Player::BuildCreateUpdateBlockForPlayer(Player* target) const
{
	if (target == this)
	{
		// do item
	}
	Unit::BuildCreateUpdateBlockForPlayer(target);
}
void Player::CreateObject()
{
}
void Player::DeleteObject()
{
	WorldPacket packet(SMSG_DESTROY_OBJECT, 4);
	packet << GetGUIDLow();

	SendToOther(packet);
}
void Player::_LoadSkills(QueryResult* result)
{
	//                                                           0      1     
	// SetPQuery(PLAYER_LOGIN_QUERY_LOADSKILLS,          "SELECT skill, value FROM character_skills WHERE guid = '%u'", GUID_LOPART(m_guid));

	uint32 count = 0;
	if (result)
	{
		do
		{
			Field* fields = result->Fetch();

			uint16 skill = fields[0].GetUInt16();
			uint16 value = fields[1].GetUInt16();
		} while (result->NextRow());
		delete result;
	}

	/*for (; count < PLAYER_MAX_SKILLS; ++count)
	{
		SetUInt32Value(PLAYER_SKILL_INDEX(count), 0);
		SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(count), 0);
		SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(count), 0);
	}*/
}
void Player::_LoadStats(QueryResult* result)
{
	uint32 count = 0;
	if (result)
	{
		do
		{
			Field* fields = result->Fetch();
			SetStat(Stats(STAT_STR), fields[1].GetUInt32());
			SetStat(Stats(STAT_STA), fields[2].GetUInt32());
			SetStat(Stats(STAT_INT), fields[3].GetUInt32());
			SetStat(Stats(STAT_HEA), fields[4].GetUInt32());
			SetFloatValue(PLAYER_BLOCK_PERCENTAGE, fields[5].GetFloat());
			SetFloatValue(PLAYER_DODGE_PERCENTAGE, fields[6].GetFloat());
			SetFloatValue(PLAYER_PARRY_PERCENTAGE, fields[7].GetFloat());
			SetFloatValue(PLAYER_CRIT_PERCENTAGE, fields[8].GetFloat());
			SetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE, fields[9].GetFloat());
			SetInt32Value(UNIT_FIELD_ATTACK_POWER, fields[10].GetUInt32());
			SetInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER, fields[11].GetUInt32());
		} while (result->NextRow());
		delete result;
	}
}
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

void Player::SaveToDB()
{
	DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "The value of player %s at save: ", m_name.c_str());
	outDebugStatsValues();

	CharacterDatabase.BeginTransaction();

	static SqlStatementID delChar;
	static SqlStatementID insChar;

	SqlStatement stmt = CharacterDatabase.CreateStatement(delChar, "DELETE FROM characters WHERE guid = ?");
	stmt.PExecute(GetGUIDLow());
	SqlStatement uberInsert = CharacterDatabase.CreateStatement(insChar, "INSERT INTO characters (guid, account, name, class, gender, level, health, energy, power1, xp, money, playerBytes, "
		"playerBytes2, playerFlags, position_x, position_y, position_z, map, orientation, online, zone, sppoint, guildid, actionBars) "
		"VALUES ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) ");

	uberInsert.addUInt32(GetGUIDLow());							// guid	0
	uberInsert.addUInt32(GetSession()->GetAccountId());			// account	1
	uberInsert.addString(m_name);								// name	2
	uberInsert.addUInt8(getClass());							// class	3
	uberInsert.addUInt8(getGender());							// gender	4
	uberInsert.addUInt32((getLevel() > 0) ? getLevel() : 1);	// level	5

	uberInsert.addUInt32(GetHealth());							// health	9
	for (uint32 i = 0; i < MAX_POWERS; ++i)
		uberInsert.addUInt32(GetPower(Powers(i)));				// energy -> rp	7 8

	uberInsert.addUInt32(GetPlayerXp());					// xp				9
	uberInsert.addUInt32(GetMoney());						// money			10
	uberInsert.addUInt32(GetUInt32Value(PLAYER_BYTES));		// playerBytes		11
	uberInsert.addUInt32(GetUInt32Value(PLAYER_BYTES_2));	// playerBytes2		12
	uberInsert.addUInt32(GetUInt32Value(PLAYER_FLAGS));		// playerFlags		13


	uberInsert.addFloat(finiteAlways(GetPositionX()));		// x	14
	uberInsert.addFloat(finiteAlways(GetPositionY()));		// y	15
	uberInsert.addFloat(finiteAlways(GetPositionZ()));		// z	16
	uberInsert.addUInt32(GetMapId());						// map	17
	uberInsert.addFloat(finiteAlways(GetOrientation()));	// o	18


	uberInsert.addUInt32(0);								// online		19
	uberInsert.addUInt32(0);								// zone			20
	uberInsert.addUInt32(0);								// sppoint		21
	uberInsert.addUInt32(0);								// guildid		22
	uberInsert.addUInt32(0);								// actionbar	23
	sLog.outDebug("Creating character with value:\n"
		"GUID: %d\n"
		"AccountID: %d\n"
		"name: %s\n"
		"class: %d\n"
		"gender: %d\n"
		"level: %d\n"
		"health: %d\n"
		"energy: %d\n"
		"power1: %d\n"
		"xp: %d\n"
		"money: %d\n"
		"bytes: %d\n"
		"bytes2: %d\n"
		"flags: %d\n"
		"x: %f\n"
		"y: %f\n"
		"z: %f\n"
		"o: %f\n"
		"map: %d\n"
		"online: %d\n"
		"zone: %d\n"
		"sppoint: %d\n"
		"guildid: %d\n"
		"actionbar: %d", GetGUIDLow(), GetSession()->GetAccountId(), m_name.c_str(), getClass(), getGender(), getLevel(), GetHealth(), GetPower(Powers(0)), GetPower(Powers(1)),
		GetPlayerXp(), GetMoney(), GetUInt32Value(PLAYER_BYTES), GetUInt32Value(PLAYER_BYTES_2), GetUInt32Value(PLAYER_FLAGS), GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation(), GetMapId(), 0, 0, 0, 0, 0);
	//uberInsert.addUInt16(uint16(GetUInt32Value(PLAYER_BYTES_3) & 0xFFFE));

	uberInsert.Execute();


	// save skills

	// save stats
	_SaveStats();
	CharacterDatabase.CommitTransaction();
	// check if stats should only be saved on logout
	// save stats can be out of transaction
	//if (m_session->isLogingOut() || !sWorld.getConfig(CONFIG_BOOL_STATS_SAVE_ONLY_ON_LOGOUT))
		//_SaveStats();
}
void Player::_SaveStats()
{
	// check if stat saving is enabled and if char level is high enough
	if (!sWorld.getConfig(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE) || getLevel() < sWorld.getConfig(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE))
		return;

	static SqlStatementID delStats;
	static SqlStatementID insertStats;
	SqlStatement stmt = CharacterDatabase.CreateStatement(delStats, "DELETE FROM character_stats WHERE guid = ?");
	stmt.PExecute(GetGUIDLow());
	stmt = CharacterDatabase.CreateStatement(insertStats, "INSERT INTO character_stats (guid, strength, stamina, intellect, health,"
		"blockPct, dodgePct, parryPct, critPct, rangedCritPct, attackPower, rangedAttackPower) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

	stmt.addUInt32(GetGUIDLow());
	for (int i = 0; i < MAX_STATS; ++i)
		stmt.addFloat(GetStat(Stats(i)));

	stmt.addFloat(GetFloatValue(PLAYER_BLOCK_PERCENTAGE));
	stmt.addFloat(GetFloatValue(PLAYER_DODGE_PERCENTAGE));
	stmt.addFloat(GetFloatValue(PLAYER_PARRY_PERCENTAGE));
	stmt.addFloat(GetFloatValue(PLAYER_CRIT_PERCENTAGE));
	stmt.addFloat(GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE));
	stmt.addUInt32(GetUInt32Value(UNIT_FIELD_ATTACK_POWER));
	stmt.addUInt32(GetUInt32Value(UNIT_FIELD_RANGED_ATTACK_POWER));

	stmt.Execute();
}
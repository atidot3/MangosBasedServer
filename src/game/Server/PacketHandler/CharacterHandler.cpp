#include <Common.h>
#include <Database/DatabaseEnv.h>
#include <WorldPacket.h>
#include "../SharedDefine.h"
#include "../WorldSession.h"
#include "../Opcodes.h"
#include <Log.h>
#include "../../World/World.h"
#include <Database/DatabaseImpl.h>
#include "../../Object/Player.h"
#include "ObjectMgr.h"
#include <Util\Util.h>

class LoginQueryHolder : public SqlQueryHolder
{
private:
	uint32 m_accountId;
	ObjectGuid m_guid;
public:
	LoginQueryHolder(uint32 accountId, ObjectGuid guid)
		: m_accountId(accountId), m_guid(guid) { }
	ObjectGuid GetGuid() const { return m_guid; }
	uint32 GetAccountId() const { return m_accountId; }
	bool Initialize();
};
bool LoginQueryHolder::Initialize()
{
	SetSize(MAX_PLAYER_LOGIN_QUERY);

	bool res = true;

	// NOTE: all fields in `characters` must be read to prevent lost character data at next save in case wrong DB structure.
	// !!! NOTE: including unused `zone`,`online`
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADFROM, "SELECT guid, account, name, class, gender, level, xp, money, playerBytes, playerBytes2, playerFlags, "
		"position_x, position_y, position_z, map, orientation, online, zone, sppoint, "
		"health, energy, power1, guildid, actionBars FROM characters WHERE guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSKILLS, "SELECT skill, value FROM character_skills WHERE guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSTATS, "SELECT strength, stamina, intellect, health, blockPct, dodgePct, parryPct, "
		"critPct, rangedCritPct, attackPower, rangedAttackPower FROM character_stats WHERE guid = '%u'", m_guid.GetCounter());
	/*res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGROUP, "SELECT groupId FROM group_member WHERE memberGuid ='%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES, "SELECT id, permanent, map, resettime FROM character_instance LEFT JOIN instance ON instance = id WHERE guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADAURAS, "SELECT caster_guid,item_guid,spell,stackcount,remaincharges,basepoints0,basepoints1,basepoints2,periodictime0,periodictime1,periodictime2,maxduration,remaintime,effIndexMask FROM character_aura WHERE guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLS, "SELECT spell,active,disabled FROM character_spell WHERE guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADQUESTSTATUS, "SELECT quest,status,rewarded,explored,timer,mobcount1,mobcount2,mobcount3,mobcount4,itemcount1,itemcount2,itemcount3,itemcount4 FROM character_queststatus WHERE guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADHONORCP, "SELECT victim_type,victim,honor,date,type FROM character_honor_cp WHERE guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADREPUTATION, "SELECT faction,standing,flags FROM character_reputation WHERE guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADINVENTORY, "SELECT data,bag,slot,item,item_template FROM character_inventory JOIN item_instance ON character_inventory.item = item_instance.guid WHERE character_inventory.guid = '%u' ORDER BY bag,slot", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADITEMLOOT, "SELECT guid,itemid,amount,property FROM item_loot WHERE owner_guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADACTIONS, "SELECT button,action,type FROM character_action WHERE guid = '%u' ORDER BY button", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSOCIALLIST, "SELECT friend,flags FROM character_social WHERE guid = '%u' LIMIT 255", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADHOMEBIND, "SELECT map,zone,position_x,position_y,position_z FROM character_homebind WHERE guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS, "SELECT spell,item,time FROM character_spell_cooldown WHERE guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADGUILD, "SELECT guildid,rank FROM guild_member WHERE guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADBGDATA, "SELECT instance_id, team, join_x, join_y, join_z, join_o, join_map FROM character_battleground_data WHERE guid = '%u'", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILS, "SELECT id,messageType,sender,receiver,subject,itemTextId,expire_time,deliver_time,money,cod,checked,stationery,mailTemplateId,has_items FROM mail WHERE receiver = '%u' ORDER BY id DESC", m_guid.GetCounter());
	res &= SetPQuery(PLAYER_LOGIN_QUERY_LOADMAILEDITEMS, "SELECT data, mail_id, item_guid, item_template FROM mail_items JOIN item_instance ON item_guid = guid WHERE receiver = '%u'", m_guid.GetCounter());
	*/
	return res;
}
class CharacterHandler
{
public:
	void HandleCharEnumCallback(QueryResult* result, uint32 account)
	{
		WorldSession* session = sWorld.FindSession(account);
		if (!session)
		{
			delete result;
			return;
		}
		session->HandleCharEnum(result);
	}
	void HandlePlayerLoginCallback(QueryResult* /*dummy*/, SqlQueryHolder* holder)
	{
		if (!holder) return;
		WorldSession* session = sWorld.FindSession(((LoginQueryHolder*)holder)->GetAccountId());
		if (!session)
		{
			delete holder;
			return;
		}
		session->HandlePlayerLogin((LoginQueryHolder*)holder);
	}
} chrHandler;

void WorldSession::HandleCharEnumOpcode(WorldPacket& /*recv_data*/)
{
	/// get all the data necessary for loading all characters (along with their pets) on the account
	CharacterDatabase.AsyncPQuery(&chrHandler, &CharacterHandler::HandleCharEnumCallback, GetAccountId(),
		//           0               1                2                3                 4                  5                       6                        7
		"SELECT characters.guid, characters.name, characters.class, characters.gender, characters.money, characters.playerBytes, characters.playerBytes2, characters.level, "
		//   8             9               10                     11                     12                     13                    14
		"characters.zone, characters.map, characters.position_x, characters.position_y, characters.position_z, characters.guildid, characters.playerFlags "
		"FROM characters WHERE characters.account = '%u' ORDER BY characters.guid",
		GetAccountId());
}
void WorldSession::HandleCharEnum(QueryResult* result)
{
	WorldPacket data(SMSG_CHAR_ENUM, 100);                  // we guess size
	uint8 num = 0;

	data << num;
	if (result)
	{
		do
		{
			uint32 guidlow = (*result)[0].GetUInt32();
			DETAIL_LOG("Build enum data for char guid %u from account %u.", guidlow, GetAccountId());
			Field* fields = result->Fetch();
			uint32 guid = fields[0].GetUInt32();
			uint8 pClass = fields[2].GetUInt8();

			// data << ObjectGuid(HIGHGUID_PLAYER, guid); // what ?
			data << guid;
			data << fields[1].GetString();                       // name
			data << uint8(pClass);                               // class
			data << uint8(fields[3].GetUInt8());                 // gender

			data << uint32(fields[4].GetUInt32());               // money

			uint32 playerBytes = fields[5].GetUInt32();
			data << uint8(playerBytes);                          // hair
			data << uint8(playerBytes >> 8);                     // hairColor
			data << uint8(playerBytes >> 16);                    // jaw
			data << uint8(playerBytes >> 24);                    // skin

			uint32 playerBytes2 = fields[6].GetUInt32();
			data << uint8(playerBytes2);						 // nose
			data << uint8(playerBytes2 >> 8);					 // eyes

			data << uint8(fields[7].GetUInt8());                 // level
			data << uint32(fields[8].GetUInt32());               // zone
			data << uint32(fields[9].GetUInt32());               // map

			data << fields[10].GetFloat();                       // x
			data << fields[11].GetFloat();                       // y
			data << fields[12].GetFloat();                       // z
																 //if (Player::BuildEnumData(result, &data))
			num++;

			sLog.outDetail("character = '%d' filled data", guid);
		} while (result->NextRow());

		delete result;
	}
	data.put<uint8>(0, num); // set amount of character at the first place :)
	SendPacket(&data);
}
void WorldSession::HandlePlayerLoginOpcode(WorldPacket& recv_data) /* PLAYER LOGIN TO GO IN GAME */
{
	uint32 id;
	
	recv_data >> id;
	ObjectGuid playerGuid(HIGHGUID_PLAYER, uint32(id));// character guid from database i guess

	sLog.outError("High = %d, Entry = %d, counter = %d", playerGuid.GetHigh(), playerGuid.GetEntry(), playerGuid.GetCounter());
	if (PlayerLoading() || GetPlayer() != nullptr)
	{
		sLog.outError("Player tryes to login again, AccountId = %d", GetAccountId());
		return;
	}

	m_playerLoading = true;
	LoginQueryHolder* holder = new LoginQueryHolder(GetAccountId(), playerGuid);
	if (!holder->Initialize())
	{
		delete holder;                                      // delete all unprocessed queries
		m_playerLoading = false;
		return;
	}
	CharacterDatabase.DelayQueryHolder(&chrHandler, &CharacterHandler::HandlePlayerLoginCallback, holder);
}
void WorldSession::HandlePlayerLogin(LoginQueryHolder* holder)
{
	ObjectGuid playerGuid = holder->GetGuid();
	Player* pCurrChar = new Player(this);
	
	if (!pCurrChar->LoadFromDB(playerGuid, holder))
	{
		KickPlayer();                                       // disconnect client, player no set to session and it will not deleted or saved at kick
		delete pCurrChar;                                   // delete it manually
		delete holder;                                      // delete all unprocessed queries
		m_playerLoading = false;
		return;
	}
	SetPlayer(pCurrChar);
	WorldPacket data(SMSG_LOGIN_VERIFY_WORLD, 20);
	data << pCurrChar->GetMapId();
	data << pCurrChar->GetPositionX();
	data << pCurrChar->GetPositionY();
	data << pCurrChar->GetPositionZ();
	data << pCurrChar->GetOrientation();
	SendPacket(&data);
	/*data.Initialize(SMSG_ACCOUNT_DATA_TIMES, 128);
	for (int i = 0; i < 32; ++i)
		data << uint32(0);
	SendPacket(&data);*/

	// Send MOTD (1.12.1 not have SMSG_MOTD, so do it in another way)
	// QueryResult *result = CharacterDatabase.PQuery("SELECT guildid,rank FROM guild_member WHERE guid = '%u'",pCurrChar->GetGUIDLow());

	sObjectAccessor.AddObject(pCurrChar);
	


	WorldPacket finished(SMSG_LOGIN_FINISHED, 4);
	finished << pCurrChar->GetMapId();
	SendPacket(&finished);
}
void WorldSession::HandlePlayerEnterWorldfinished(WorldPacket& recv_data)
{
	Player* me = GetPlayer();
	me->GetMap()->Add(me);
	m_playerLoading = false;
}
void WorldSession::HandleCharDeleteOpcode(WorldPacket& recv_data)
{
	uint32 uid;
	uint32 accountId = 0;
	std::string name;

	recv_data >> uid;

	QueryResult* result = CharacterDatabase.PQuery("SELECT account,name FROM characters WHERE guid='%u'", uid);
	if (result)
	{
		Field* fields = result->Fetch();
		accountId = fields[0].GetUInt32();
		name = fields[1].GetCppString();
		delete result;
	}
	if (accountId != GetAccountId())
	{
		WorldPacket data(SMSG_CHAR_DELETE, 1 + 4);
		data << (uint8)CHAR_DELETE_FAILED;
		data << (uint32)uid;
		return;
	}
	std::string IP_str = GetRemoteAddress();
	BASIC_LOG("Account: %d (IP: %s) Delete Character:[%s] (guid: %u)", GetAccountId(), IP_str.c_str(), name.c_str(), uid);
	sLog.outChar("Account: %d (IP: %s) Delete Character:[%s] (guid: %u)", GetAccountId(), IP_str.c_str(), name.c_str(), uid);

	/// need to create procedure to remove all information about the character ont the database
	Player::DeleteFromDB(uid, GetAccountId());

	WorldPacket data(SMSG_CHAR_DELETE, 1 + 4);
	data << (uint8)CHAR_DELETE_SUCCESS;
	data << (uint32)uid;

	SendPacket(&data);
}
#define MAX_PLAYER_NAME          12                         // max allowed by client name length
#define MAX_INTERNAL_PLAYER_NAME 15                         // max server internal player name length ( > MAX_PLAYER_NAME for support declined names )
#define MAX_CHARTER_NAME         24                         // max allowed by client name length

bool normalizePlayerName(std::string& name)
{
	if (name.empty())
		return false;

	wchar_t wstr_buf[MAX_INTERNAL_PLAYER_NAME + 1];
	size_t wstr_len = MAX_INTERNAL_PLAYER_NAME;

	if (!Utf8toWStr(name, &wstr_buf[0], wstr_len))
		return false;

	wstr_buf[0] = wcharToUpper(wstr_buf[0]);
	for (size_t i = 1; i < wstr_len; ++i)
		wstr_buf[i] = wcharToLower(wstr_buf[i]);

	if (!WStrToUtf8(wstr_buf, wstr_len, name))
		return false;

	return true;
}
void WorldSession::HandleCharCreateOpcode(WorldPacket& recv_data)
{
	sLog.outDetail("Handle character create");

	std::string name;
	uint8 class_;
	// extract other data required for player creating
	uint8 gender, hair, hairColor, jaw, skin, nose, eyes;

	recv_data >> name;

	recv_data >> class_;

	recv_data >> gender;
	recv_data >> skin;
	recv_data >> hair;
	recv_data >> hairColor;
	recv_data >> jaw;
	recv_data >> nose;
	recv_data >> eyes;
	WorldPacket data(SMSG_CHAR_CREATE, 1);                  // returned with diff.values in all cases

															// prevent character creating with invalid name
	if (!normalizePlayerName(name))
	{
		data << (uint8)CHAR_NAME_NO_NAME;
		SendPacket(&data);
		sLog.outError("Account:[%d] but tried to Create character with empty [name]", GetAccountId());
		return;
	}

	uint8 res = ObjectMgr::CheckPlayerName(name, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        data << uint8(res);
        SendPacket(&data);
        return;
    }

	if (sObjectMgr.GetPlayerGuidByName(name))
	{
		data << (uint8)CHAR_CREATE_NAME_IN_USE;
		SendPacket(&data);
		return;
	}
	QueryResult* resultacct = LoginDatabase.PQuery("SELECT SUM(numchars) FROM realmcharacters WHERE acctid = '%u'", GetAccountId());
	if (resultacct)
	{
		Field* fields = resultacct->Fetch();
		uint32 acctcharcount = fields[0].GetUInt32();
		delete resultacct;

		if (acctcharcount >= sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_ACCOUNT))
		{
			data << (uint8)CHAR_CREATE_ACCOUNT_LIMIT;
			SendPacket(&data);
			return;
		}
	}
	QueryResult* result = CharacterDatabase.PQuery("SELECT COUNT(guid) FROM characters WHERE account = '%u'", GetAccountId());
	uint8 charcount = 0;
	if (result)
	{
		Field* fields = result->Fetch();
		charcount = fields[0].GetUInt8();
		delete result;

		if (charcount >= sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM))
		{
			data << (uint8)CHAR_CREATE_SERVER_LIMIT;
			SendPacket(&data);
			return;
		}
	}
	Player* pNewChar = new Player(this);
	if (!pNewChar->Create(sObjectMgr.GeneratePlayerLowGuid(), name, class_, gender, hair, hairColor, jaw, skin, nose, eyes, 0/*OUTFITID*/))
    {
        // Player not create (class problem?)
        delete pNewChar;

        data << (uint8)CHAR_CREATE_ERROR;
        SendPacket(&data);

        return;
    }
	//pNewChar->SetAtLoginFlag(AT_LOGIN_FIRST);               // First login
	// Player created, save it now
	pNewChar->SaveToDB();

	charcount += 1;

	LoginDatabase.PExecute("DELETE FROM realmcharacters WHERE acctid= '%u' AND realmid = '%u'", GetAccountId(), realmID);
	LoginDatabase.PExecute("INSERT INTO realmcharacters (numchars, acctid, realmid) VALUES (%u, %u, %u)", charcount, GetAccountId(), realmID);

	data << (uint8)CHAR_CREATE_SUCCESS;
	SendPacket(&data);

	const std::string &IP_str = GetRemoteAddress();
	BASIC_LOG("Account: %d (IP: %s) Create Character:[%s] (guid: %u)", GetAccountId(), IP_str.c_str(), name.c_str(), pNewChar->GetGUIDLow());
	sLog.outChar("Account: %d (IP: %s) Create Character:[%s] (guid: %u)", GetAccountId(), IP_str.c_str(), name.c_str(), pNewChar->GetGUIDLow());
	delete pNewChar;  // created only to call SaveToDB()
}
void WorldSession::HandleLogoutCancelOpcode(WorldPacket& /*recv_data*/)
{
}
void WorldSession::HandlePlayerLogoutOpcode(WorldPacket& /*recv_data*/)
{
}
void WorldSession::HandleLogoutRequestOpcode(WorldPacket& /*recv_data*/)
{
}
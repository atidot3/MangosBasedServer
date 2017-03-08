#include "World.h"
#include "../Server/WorldSocket.h"
#include <Database/DatabaseEnv.h>
#include <Database/DatabaseImpl.h>
#include <Config/Config.h>
#include <Define.h>
#include <Log.h>
#include <Util.h>

#include "../Server/WorldSession.h"
#include "WorldPacket.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "../Map/MapManager.h"

#include "DBStorage/SQLStorages.h"

#include <algorithm>
#include <mutex>
#include <cstdarg>

#define MAX_PLAYER_NAME          12                         // max allowed by client name length
#define MAX_INTERNAL_PLAYER_NAME 15                         // max server internal player name length ( > MAX_PLAYER_NAME for support declined names )
#define MAX_PET_NAME             12                         // max allowed by client name length
#define MAX_CHARTER_NAME         24                         // max allowed by client name length

#define DEFAULT_WORLDSERVER_PORT	8085
#define DEFAULT_PLAYER_LIMIT		100
#define CONTACT_DISTANCE            0.5f
#define ATTACK_DISTANCE             5.0f
#define MIN_MAP_UPDATE_DELAY		50
// also see GT_MAX_LEVEL define
#define MAX_LEVEL					100

// Server side limitation. Base at used code requirements.
// also see MAX_LEVEL and GT_MAX_LEVEL define
#define STRONG_MAX_LEVEL			255
#define DEFAULT_MAX_LEVEL 60
#define MAX_MONEY_AMOUNT        (0x7FFFFFFF-1)
#define MAX_VISIBILITY_DISTANCE     333.0f
#define DEFAULT_VISIBILITY_DISTANCE 90.0f       // default visible distance, 90 yards on continents
#define DEFAULT_VISIBILITY_INSTANCE 120.0f      // default visible distance in instances, 120 yards
#define DEFAULT_VISIBILITY_BG       180.0f      // default visible distance in BG, 180 yards


INSTANTIATE_SINGLETON_1(World);

volatile bool World::m_stopEvent = false;
volatile uint32 World::m_worldLoopCounter = 0;
uint8 World::m_ExitCode = SHUTDOWN_EXIT_CODE;

float World::m_MaxVisibleDistanceOnContinents = DEFAULT_VISIBILITY_DISTANCE;
float World::m_MaxVisibleDistanceInInstances = DEFAULT_VISIBILITY_INSTANCE;
float World::m_MaxVisibleDistanceInBG = DEFAULT_VISIBILITY_BG;

float World::m_MaxVisibleDistanceInFlight = DEFAULT_VISIBILITY_DISTANCE;
float World::m_VisibleUnitGreyDistance = 0;
float World::m_VisibleObjectGreyDistance = 0;

float  World::m_relocation_lower_limit_sq = 10.f * 10.f;
uint32 World::m_relocation_ai_notify_delay = 1000u;

World::World() : mail_timer(0), mail_timer_expires(0)
{
	m_playerLimit = 0;
	m_allowMovement = true;
	m_ShutdownMask = 0;
	m_ShutdownTimer = 0;
	m_gameTime = time(nullptr);
	m_startTime = m_gameTime;
	m_maxActiveSessionCount = 0;
	m_maxQueuedSessionCount = 0;
	m_defaultDbcLocale = LOCALE_enUS;
	m_availableDbcLocaleMask = 0;

	for (int i = 0; i < CONFIG_UINT32_VALUE_COUNT; ++i)
		m_configUint32Values[i] = 0;

	for (int i = 0; i < CONFIG_INT32_VALUE_COUNT; ++i)
		m_configInt32Values[i] = 0;

	for (int i = 0; i < CONFIG_FLOAT_VALUE_COUNT; ++i)
		m_configFloatValues[i] = 0.0f;

	for (int i = 0; i < CONFIG_BOOL_VALUE_COUNT; ++i)
		m_configBoolValues[i] = false;
}
World::~World()
{
	// it is assumed that no other thread is accessing this data when the destructor is called.  therefore, no locks are necessary

	///- Empty the kicked session set
	for (auto const session : m_sessions)
		delete session.second;

	for (auto const cliCommand : m_cliCommandQueue)
		delete cliCommand;

	for (auto const session : m_sessionAddQueue)
		delete session;
}
/// Cleanups before world stop
void World::CleanupsBeforeStop()
{
	KickAll();                                       // save and kick all players
	UpdateSessions(1);                               // real players unload required UpdateSessions call
	//sBattleGroundMgr.DeleteAllBattleGrounds();       // unload battleground templates before different singletons destroyed
}
/// Kick (and save) all players
void World::KickAll()
{
	m_QueuedSessions.clear();                               // prevent send queue update packet and login queued sessions

															// session not removed at kick and will removed in next update tick
	for (SessionMap::const_iterator itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
		itr->second->KickPlayer();
}
/// Initialize config values
void World::LoadConfigSettings(bool reload)
{
	if (reload)
	{
		if (!sConfig.Reload())
		{
			sLog.outError("World settings reload fail: can't read settings from %s.", sConfig.GetFilename().c_str());
			return;
		}
	}

	///- Read the version of the configuration file and warn the user in case of emptiness or mismatch
	uint32 confVersion = sConfig.GetIntDefault("ConfVersion", 0);
	if (!confVersion)
	{
		sLog.outError("*****************************************************************************");
		sLog.outError(" WARNING: world.conf does not include a ConfVersion variable.");
		sLog.outError("          Your configuration file may be out of date!");
		sLog.outError("*****************************************************************************");
		Log::WaitBeforeContinueIfNeed();
	}
	else
	{
		if (confVersion < 0)
		{
			sLog.outError("*****************************************************************************");
			sLog.outError(" WARNING: Your world.conf version indicates your conf file is out of date!");
			sLog.outError("          Please check for updates, as your current default values may cause");
			sLog.outError("          unexpected behavior.");
			sLog.outError("*****************************************************************************");
			Log::WaitBeforeContinueIfNeed();
		}
	}

	///- Read the player limit and the Message of the day from the config file
	SetPlayerLimit(sConfig.GetIntDefault("PlayerLimit", DEFAULT_PLAYER_LIMIT), true);
	SetMotd(sConfig.GetStringDefault("Motd", "Welcome to the Massive Network Game Object Server."));

	///- Read all rates from the config file
	setConfigPos(CONFIG_FLOAT_RATE_HEALTH, "Rate.Health", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_POWER_ENERGY, "Rate.Energy", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_POWER_KI_INCOME, "Rate.Ki.Income", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_POWER_KI_LOSS, "Rate.Ki.Loss", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_SKILL_DISCOVERY, "Rate.Skill.Discovery", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_POOR, "Rate.Drop.Item.Poor", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_NORMAL, "Rate.Drop.Item.Normal", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_UNCOMMON, "Rate.Drop.Item.Uncommon", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_RARE, "Rate.Drop.Item.Rare", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_EPIC, "Rate.Drop.Item.Epic", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_LEGENDARY, "Rate.Drop.Item.Legendary", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_ARTIFACT, "Rate.Drop.Item.Artifact", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_REFERENCED, "Rate.Drop.Item.Referenced", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_QUEST, "Rate.Drop.Item.Quest", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_DROP_MONEY, "Rate.Drop.Money", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_PET_XP_KILL, "Rate.Pet.XP.Kill", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_XP_KILL, "Rate.XP.Kill", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_XP_QUEST, "Rate.XP.Quest", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_XP_EXPLORE, "Rate.XP.Explore", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_REPUTATION_GAIN, "Rate.Reputation.Gain", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_KILL, "Rate.Reputation.LowLevel.Kill", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_QUEST, "Rate.Reputation.LowLevel.Quest", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_NORMAL_DAMAGE, "Rate.Creature.Normal.Damage", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_DAMAGE, "Rate.Creature.Elite.Elite.Damage", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_DAMAGE, "Rate.Creature.Elite.RAREELITE.Damage", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_DAMAGE, "Rate.Creature.Elite.WORLDBOSS.Damage", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_DAMAGE, "Rate.Creature.Elite.RARE.Damage", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_NORMAL_HP, "Rate.Creature.Normal.HP", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_HP, "Rate.Creature.Elite.Elite.HP", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_HP, "Rate.Creature.Elite.RAREELITE.HP", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_HP, "Rate.Creature.Elite.WORLDBOSS.HP", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_HP, "Rate.Creature.Elite.RARE.HP", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_NORMAL_SPELLDAMAGE, "Rate.Creature.Normal.SpellDamage", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE, "Rate.Creature.Elite.Elite.SpellDamage", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_SPELLDAMAGE, "Rate.Creature.Elite.RAREELITE.SpellDamage", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_SPELLDAMAGE, "Rate.Creature.Elite.WORLDBOSS.SpellDamage", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_SPELLDAMAGE, "Rate.Creature.Elite.RARE.SpellDamage", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CREATURE_AGGRO, "Rate.Creature.Aggro", 1.0f);
	setConfig(CONFIG_FLOAT_RATE_REST_INGAME, "Rate.Rest.InGame", 1.0f);
	setConfig(CONFIG_FLOAT_RATE_REST_OFFLINE_IN_TAVERN_OR_CITY, "Rate.Rest.Offline.InTavernOrCity", 1.0f);
	setConfig(CONFIG_FLOAT_RATE_REST_OFFLINE_IN_WILDERNESS, "Rate.Rest.Offline.InWilderness", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_DAMAGE_FALL, "Rate.Damage.Fall", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_AUCTION_TIME, "Rate.Auction.Time", 1.0f);
	setConfig(CONFIG_FLOAT_RATE_AUCTION_DEPOSIT, "Rate.Auction.Deposit", 1.0f);
	setConfig(CONFIG_FLOAT_RATE_AUCTION_CUT, "Rate.Auction.Cut", 1.0f);
	setConfig(CONFIG_UINT32_AUCTION_DEPOSIT_MIN, "Auction.Deposit.Min", 0);
	setConfig(CONFIG_FLOAT_RATE_HONOR, "Rate.Honor", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_MINING_AMOUNT, "Rate.Mining.Amount", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_MINING_NEXT, "Rate.Mining.Next", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_INSTANCE_RESET_TIME, "Rate.InstanceResetTime", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_TALENT, "Rate.Talent", 1.0f);
	setConfigPos(CONFIG_FLOAT_RATE_CORPSE_DECAY_LOOTED, "Rate.Corpse.Decay.Looted", 0.0f);

	setConfigMinMax(CONFIG_FLOAT_RATE_TARGET_POS_RECALCULATION_RANGE, "TargetPosRecalculateRange", 1.5f, CONTACT_DISTANCE, ATTACK_DISTANCE);

	setConfigPos(CONFIG_FLOAT_RATE_DURABILITY_LOSS_DAMAGE, "DurabilityLossChance.Damage", 0.5f);
	setConfigPos(CONFIG_FLOAT_RATE_DURABILITY_LOSS_ABSORB, "DurabilityLossChance.Absorb", 0.5f);
	setConfigPos(CONFIG_FLOAT_RATE_DURABILITY_LOSS_PARRY, "DurabilityLossChance.Parry", 0.05f);
	setConfigPos(CONFIG_FLOAT_RATE_DURABILITY_LOSS_BLOCK, "DurabilityLossChance.Block", 0.05f);

	setConfigPos(CONFIG_FLOAT_LISTEN_RANGE_SAY, "ListenRange.Say", 40.0f);
	setConfigPos(CONFIG_FLOAT_LISTEN_RANGE_YELL, "ListenRange.Yell", 300.0f);
	setConfigPos(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE, "ListenRange.TextEmote", 40.0f);

	setConfigPos(CONFIG_FLOAT_GROUP_XP_DISTANCE, "MaxGroupXPDistance", 74.0f);
	setConfigPos(CONFIG_FLOAT_SIGHT_GUARDER, "GuarderSight", 50.0f);
	setConfigPos(CONFIG_FLOAT_SIGHT_MONSTER, "MonsterSight", 50.0f);

	setConfigPos(CONFIG_FLOAT_CREATURE_FAMILY_ASSISTANCE_RADIUS, "CreatureFamilyAssistanceRadius", 10.0f);
	setConfigPos(CONFIG_FLOAT_CREATURE_FAMILY_FLEE_ASSISTANCE_RADIUS, "CreatureFamilyFleeAssistanceRadius", 30.0f);

	///- Read other configuration items from the config file
	setConfigMinMax(CONFIG_UINT32_COMPRESSION, "Compression", 1, 1, 9);
	setConfig(CONFIG_BOOL_CLEAN_CHARACTER_DB, "CleanCharacterDB", true);
	setConfig(CONFIG_UINT32_MAX_WHOLIST_RETURNS, "MaxWhoListReturns", 49);

	
	setConfig(CONFIG_UINT32_INTERVAL_SAVE, "PlayerSave.Interval", 15 * MINUTE * IN_MILLISECONDS);
	setConfigMinMax(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE, "PlayerSave.Stats.MinLevel", 0, 0, MAX_LEVEL);
	setConfig(CONFIG_BOOL_STATS_SAVE_ONLY_ON_LOGOUT, "PlayerSave.Stats.SaveOnlyOnLogout", true);

	setConfigMin(CONFIG_UINT32_INTERVAL_MAPUPDATE, "MapUpdateInterval", 100, MIN_MAP_UPDATE_DELAY);
	/*if (reload)
		sMapMgr.SetMapUpdateInterval(getConfig(CONFIG_UINT32_INTERVAL_MAPUPDATE));*/

	setConfig(CONFIG_UINT32_INTERVAL_CHANGEWEATHER, "ChangeWeatherInterval", 10 * MINUTE * IN_MILLISECONDS);

	if (configNoReload(reload, CONFIG_UINT32_PORT_WORLD, "WorldServerPort", DEFAULT_WORLDSERVER_PORT))
		setConfig(CONFIG_UINT32_PORT_WORLD, "WorldServerPort", DEFAULT_WORLDSERVER_PORT);

	if (configNoReload(reload, CONFIG_UINT32_GAME_TYPE, "GameType", 0))
		setConfig(CONFIG_UINT32_GAME_TYPE, "GameType", 0);

	if (configNoReload(reload, CONFIG_UINT32_REALM_ZONE, "RealmZone", REALM_ZONE_DEVELOPMENT))
		setConfig(CONFIG_UINT32_REALM_ZONE, "RealmZone", REALM_ZONE_DEVELOPMENT);

	setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS, "AllowTwoSide.Accounts", false);
	setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT, "AllowTwoSide.Interaction.Chat", false);
	setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL, "AllowTwoSide.Interaction.Channel", false);
	setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP, "AllowTwoSide.Interaction.Group", false);
	setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD, "AllowTwoSide.Interaction.Guild", false);
	setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_TRADE, "AllowTwoSide.Interaction.Trade", false);
	setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION, "AllowTwoSide.Interaction.Auction", false);
	setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_MAIL, "AllowTwoSide.Interaction.Mail", false);
	setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST, "AllowTwoSide.WhoList", false);
	setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ADD_FRIEND, "AllowTwoSide.AddFriend", false);

	setConfig(CONFIG_UINT32_STRICT_PLAYER_NAMES, "StrictPlayerNames", 0);
	setConfig(CONFIG_UINT32_STRICT_CHARTER_NAMES, "StrictCharterNames", 0);
	setConfig(CONFIG_UINT32_STRICT_PET_NAMES, "StrictPetNames", 0);

	setConfigMinMax(CONFIG_UINT32_MIN_PLAYER_NAME, "MinPlayerName", 2, 1, MAX_PLAYER_NAME);
	setConfigMinMax(CONFIG_UINT32_MIN_CHARTER_NAME, "MinCharterName", 2, 1, MAX_CHARTER_NAME);
	setConfigMinMax(CONFIG_UINT32_MIN_PET_NAME, "MinPetName", 2, 1, MAX_PET_NAME);

	setConfig(CONFIG_UINT32_CHARACTERS_CREATING_DISABLED, "CharactersCreatingDisabled", 0);

	setConfigMinMax(CONFIG_UINT32_CHARACTERS_PER_REALM, "CharactersPerRealm", 10, 1, 10);

	// must be after CONFIG_UINT32_CHARACTERS_PER_REALM
	setConfigMin(CONFIG_UINT32_CHARACTERS_PER_ACCOUNT, "CharactersPerAccount", 50, getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM));

	setConfigMinMax(CONFIG_UINT32_SKIP_CINEMATICS, "SkipCinematics", 0, 0, 2);

	if (configNoReload(reload, CONFIG_UINT32_MAX_PLAYER_LEVEL, "MaxPlayerLevel", DEFAULT_MAX_LEVEL))
		setConfigMinMax(CONFIG_UINT32_MAX_PLAYER_LEVEL, "MaxPlayerLevel", DEFAULT_MAX_LEVEL, 1, DEFAULT_MAX_LEVEL);

	setConfigMinMax(CONFIG_UINT32_START_PLAYER_LEVEL, "StartPlayerLevel", 1, 1, getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));

	setConfigMinMax(CONFIG_UINT32_START_PLAYER_MONEY, "StartPlayerMoney", 0, 0, MAX_MONEY_AMOUNT);

	setConfig(CONFIG_UINT32_MAX_HONOR_POINTS, "MaxHonorPoints", 75000);

	setConfigMinMax(CONFIG_UINT32_START_HONOR_POINTS, "StartHonorPoints", 0, 0, getConfig(CONFIG_UINT32_MAX_HONOR_POINTS));

	setConfigMin(CONFIG_UINT32_MIN_HONOR_KILLS, "MinHonorKills", HONOR_STANDING_MIN_KILL, 1);

	setConfigMinMax(CONFIG_UINT32_MAINTENANCE_DAY, "MaintenanceDay", 4, 0, 6);

	setConfig(CONFIG_BOOL_ALL_TAXI_PATHS, "AllFlightPaths", false);

	setConfig(CONFIG_BOOL_INSTANCE_IGNORE_LEVEL, "Instance.IgnoreLevel", false);
	setConfig(CONFIG_BOOL_INSTANCE_IGNORE_RAID, "Instance.IgnoreRaid", false);

	setConfig(CONFIG_BOOL_CAST_UNSTUCK, "CastUnstuck", true);
	setConfig(CONFIG_UINT32_MAX_SPELL_CASTS_IN_CHAIN, "MaxSpellCastsInChain", 20);
	setConfig(CONFIG_UINT32_RABBIT_DAY, "RabbitDay", 0);

	setConfig(CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR, "Instance.ResetTimeHour", 4);
	setConfig(CONFIG_UINT32_INSTANCE_UNLOAD_DELAY, "Instance.UnloadDelay", 30 * MINUTE * IN_MILLISECONDS);

	setConfigMinMax(CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL, "MaxPrimaryTradeSkill", 2, 0, 10);

	setConfigMinMax(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_MAX_PRIMARY_COUNT, "TradeSkill.GMIgnore.MaxPrimarySkillsCount", SEC_CONSOLE, SEC_PLAYER, SEC_CONSOLE);
	setConfigMinMax(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_LEVEL, "TradeSkill.GMIgnore.Level", SEC_CONSOLE, SEC_PLAYER, SEC_CONSOLE);
	setConfigMinMax(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_SKILL, "TradeSkill.GMIgnore.Skill", SEC_CONSOLE, SEC_PLAYER, SEC_CONSOLE);

	setConfigMinMax(CONFIG_UINT32_MIN_PETITION_SIGNS, "MinPetitionSigns", 9, 0, 9);

	setConfig(CONFIG_UINT32_GM_LOGIN_STATE, "GM.LoginState", 2);
	setConfig(CONFIG_UINT32_GM_VISIBLE_STATE, "GM.Visible", 2);
	setConfig(CONFIG_UINT32_GM_ACCEPT_TICKETS, "GM.AcceptTickets", 2);
	setConfig(CONFIG_UINT32_GM_CHAT, "GM.Chat", 2);
	setConfig(CONFIG_UINT32_GM_WISPERING_TO, "GM.WhisperingTo", 2);

	setConfig(CONFIG_UINT32_GM_LEVEL_IN_GM_LIST, "GM.InGMList.Level", SEC_ADMINISTRATOR);
	setConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST, "GM.InWhoList.Level", SEC_ADMINISTRATOR);
	setConfig(CONFIG_BOOL_GM_LOG_TRADE, "GM.LogTrade", false);

	setConfigMinMax(CONFIG_UINT32_START_GM_LEVEL, "GM.StartLevel", 1, getConfig(CONFIG_UINT32_START_PLAYER_LEVEL), MAX_LEVEL);
	setConfig(CONFIG_BOOL_GM_LOWER_SECURITY, "GM.LowerSecurity", false);
	setConfig(CONFIG_UINT32_GM_INVISIBLE_AURA, "GM.InvisibleAura", 31748);

	setConfig(CONFIG_UINT32_GROUP_VISIBILITY, "Visibility.GroupMode", 0);

	setConfig(CONFIG_UINT32_MAIL_DELIVERY_DELAY, "MailDeliveryDelay", HOUR);

	setConfigMin(CONFIG_UINT32_MASS_MAILER_SEND_PER_TICK, "MassMailer.SendPerTick", 10, 1);

	setConfig(CONFIG_UINT32_UPTIME_UPDATE, "UpdateUptimeInterval", 10);
	if (reload)
	{
		m_timers[WUPDATE_UPTIME].SetInterval(getConfig(CONFIG_UINT32_UPTIME_UPDATE)*MINUTE * IN_MILLISECONDS);
		m_timers[WUPDATE_UPTIME].Reset();
	}

	setConfig(CONFIG_UINT32_SKILL_CHANCE_ORANGE, "SkillChance.Orange", 100);
	setConfig(CONFIG_UINT32_SKILL_CHANCE_YELLOW, "SkillChance.Yellow", 75);
	setConfig(CONFIG_UINT32_SKILL_CHANCE_GREEN, "SkillChance.Green", 25);
	setConfig(CONFIG_UINT32_SKILL_CHANCE_GREY, "SkillChance.Grey", 0);

	setConfig(CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS, "SkillChance.MiningSteps", 75);
	setConfig(CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS, "SkillChance.SkinningSteps", 75);

	setConfig(CONFIG_UINT32_SKILL_GAIN_CRAFTING, "SkillGain.Crafting", 1);
	setConfig(CONFIG_UINT32_SKILL_GAIN_DEFENSE, "SkillGain.Defense", 1);
	setConfig(CONFIG_UINT32_SKILL_GAIN_GATHERING, "SkillGain.Gathering", 1);
	setConfig(CONFIG_UINT32_SKILL_GAIN_WEAPON, "SkillGain.Weapon", 1);

	setConfig(CONFIG_BOOL_SKILL_FAIL_LOOT_FISHING, "SkillFail.Loot.Fishing", false);
	setConfig(CONFIG_BOOL_SKILL_FAIL_GAIN_FISHING, "SkillFail.Gain.Fishing", false);
	setConfig(CONFIG_BOOL_SKILL_FAIL_POSSIBLE_FISHINGPOOL, "SkillFail.Possible.FishingPool", true);

	setConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS, "MaxOverspeedPings", 2);
	if (getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS) != 0 && getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS) < 2)
	{
		sLog.outError("MaxOverspeedPings (%i) must be in range 2..infinity (or 0 to disable check). Set to 2.", getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS));
		setConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS, 2);
	}

	setConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY, "SaveRespawnTimeImmediately", true);
	setConfig(CONFIG_BOOL_WEATHER, "ActivateWeather", true);

	setConfig(CONFIG_BOOL_ALWAYS_MAX_SKILL_FOR_LEVEL, "AlwaysMaxSkillForLevel", false);

	setConfig(CONFIG_UINT32_CHATFLOOD_MESSAGE_COUNT, "ChatFlood.MessageCount", 10);
	setConfig(CONFIG_UINT32_CHATFLOOD_MESSAGE_DELAY, "ChatFlood.MessageDelay", 1);
	setConfig(CONFIG_UINT32_CHATFLOOD_MUTE_TIME, "ChatFlood.MuteTime", 10);

	setConfig(CONFIG_BOOL_EVENT_ANNOUNCE, "Event.Announce", false);

	setConfig(CONFIG_UINT32_CREATURE_FAMILY_ASSISTANCE_DELAY, "CreatureFamilyAssistanceDelay", 1500);
	setConfig(CONFIG_UINT32_CREATURE_FAMILY_FLEE_DELAY, "CreatureFamilyFleeDelay", 7000);

	setConfig(CONFIG_UINT32_WORLD_BOSS_LEVEL_DIFF, "WorldBossLevelDiff", 3);

	setConfigMinMax(CONFIG_INT32_QUEST_LOW_LEVEL_HIDE_DIFF, "Quests.LowLevelHideDiff", 4, -1, MAX_LEVEL);
	setConfigMinMax(CONFIG_INT32_QUEST_HIGH_LEVEL_HIDE_DIFF, "Quests.HighLevelHideDiff", 7, -1, MAX_LEVEL);

	setConfig(CONFIG_BOOL_QUEST_IGNORE_RAID, "Quests.IgnoreRaid", false);

	setConfig(CONFIG_BOOL_DETECT_POS_COLLISION, "DetectPosCollision", true);

	setConfig(CONFIG_BOOL_RESTRICTED_LFG_CHANNEL, "Channel.RestrictedLfg", true);
	setConfig(CONFIG_BOOL_SILENTLY_GM_JOIN_TO_CHANNEL, "Channel.SilentlyGMJoin", false);

	setConfig(CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING, "ChatFakeMessagePreventing", false);

	setConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY, "ChatStrictLinkChecking.Severity", 0);
	setConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK, "ChatStrictLinkChecking.Kick", 0);

	setConfig(CONFIG_BOOL_CORPSE_EMPTY_LOOT_SHOW, "Corpse.EmptyLootShow", true);
	setConfig(CONFIG_UINT32_CORPSE_DECAY_NORMAL, "Corpse.Decay.NORMAL", 300);
	setConfig(CONFIG_UINT32_CORPSE_DECAY_RARE, "Corpse.Decay.RARE", 900);
	setConfig(CONFIG_UINT32_CORPSE_DECAY_ELITE, "Corpse.Decay.ELITE", 600);
	setConfig(CONFIG_UINT32_CORPSE_DECAY_RAREELITE, "Corpse.Decay.RAREELITE", 1200);
	setConfig(CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS, "Corpse.Decay.WORLDBOSS", 3600);

	setConfig(CONFIG_INT32_DEATH_SICKNESS_LEVEL, "Death.SicknessLevel", 11);

	setConfig(CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVP, "Death.CorpseReclaimDelay.PvP", true);
	setConfig(CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVE, "Death.CorpseReclaimDelay.PvE", true);
	setConfig(CONFIG_BOOL_DEATH_BONES_WORLD, "Death.Bones.World", true);
	setConfig(CONFIG_BOOL_DEATH_BONES_BG, "Death.Bones.Battleground", true);
	setConfigMinMax(CONFIG_FLOAT_GHOST_RUN_SPEED_WORLD, "Death.Ghost.RunSpeed.World", 1.0f, 0.1f, 10.0f);
	setConfigMinMax(CONFIG_FLOAT_GHOST_RUN_SPEED_BG, "Death.Ghost.RunSpeed.Battleground", 1.0f, 0.1f, 10.0f);

	setConfig(CONFIG_FLOAT_THREAT_RADIUS, "ThreatRadius", 100.0f);
	setConfigMin(CONFIG_UINT32_CREATURE_RESPAWN_AGGRO_DELAY, "CreatureRespawnAggroDelay", 5000, 0);

	setConfig(CONFIG_BOOL_BATTLEGROUND_CAST_DESERTER, "Battleground.CastDeserter", true);
	setConfigMinMax(CONFIG_UINT32_BATTLEGROUND_QUEUE_ANNOUNCER_JOIN, "Battleground.QueueAnnouncer.Join", 0, 0, 2);
	setConfig(CONFIG_BOOL_BATTLEGROUND_QUEUE_ANNOUNCER_START, "Battleground.QueueAnnouncer.Start", false);
	setConfig(CONFIG_BOOL_BATTLEGROUND_SCORE_STATISTICS, "Battleground.ScoreStatistics", false);
	setConfig(CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE, "Battleground.InvitationType", 0);
	setConfig(CONFIG_UINT32_BATTLEGROUND_PREMATURE_FINISH_TIMER, "BattleGround.PrematureFinishTimer", 5 * MINUTE * IN_MILLISECONDS);
	setConfig(CONFIG_UINT32_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH, "BattleGround.PremadeGroupWaitForMatch", 0);
	setConfig(CONFIG_BOOL_OUTDOORPVP_SI_ENABLED, "OutdoorPvp.SIEnabled", true);
	setConfig(CONFIG_BOOL_OUTDOORPVP_EP_ENABLED, "OutdoorPvp.EPEnabled", true);

	setConfig(CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET, "Network.KickOnBadPacket", false);

	setConfig(CONFIG_BOOL_PLAYER_COMMANDS, "PlayerCommands", true);

	setConfig(CONFIG_UINT32_INSTANT_LOGOUT, "InstantLogout", SEC_MODERATOR);

	setConfigMin(CONFIG_UINT32_GUILD_EVENT_LOG_COUNT, "Guild.EventLogRecordsCount", GUILD_EVENTLOG_MAX_RECORDS, GUILD_EVENTLOG_MAX_RECORDS);

	setConfig(CONFIG_UINT32_TIMERBAR_FATIGUE_GMLEVEL, "TimerBar.Fatigue.GMLevel", SEC_CONSOLE);
	setConfig(CONFIG_UINT32_TIMERBAR_FATIGUE_MAX, "TimerBar.Fatigue.Max", 60);
	setConfig(CONFIG_UINT32_TIMERBAR_BREATH_GMLEVEL, "TimerBar.Breath.GMLevel", SEC_CONSOLE);
	setConfig(CONFIG_UINT32_TIMERBAR_BREATH_MAX, "TimerBar.Breath.Max", 60);
	setConfig(CONFIG_UINT32_TIMERBAR_FIRE_GMLEVEL, "TimerBar.Fire.GMLevel", SEC_CONSOLE);
	setConfig(CONFIG_UINT32_TIMERBAR_FIRE_MAX, "TimerBar.Fire.Max", 1);

	setConfig(CONFIG_BOOL_PET_UNSUMMON_AT_MOUNT, "PetUnsummonAtMount", false);

	m_relocation_ai_notify_delay = sConfig.GetIntDefault("Visibility.AIRelocationNotifyDelay", 1000u);
	m_relocation_lower_limit_sq = pow(sConfig.GetFloatDefault("Visibility.RelocationLowerLimit", 10), 2);

	m_VisibleUnitGreyDistance = sConfig.GetFloatDefault("Visibility.Distance.Grey.Unit", 1);
	if (m_VisibleUnitGreyDistance >  MAX_VISIBILITY_DISTANCE)
	{
		sLog.outError("Visibility.Distance.Grey.Unit can't be greater %f", MAX_VISIBILITY_DISTANCE);
		m_VisibleUnitGreyDistance = MAX_VISIBILITY_DISTANCE;
	}
	m_VisibleObjectGreyDistance = sConfig.GetFloatDefault("Visibility.Distance.Grey.Object", 10);
	if (m_VisibleObjectGreyDistance >  MAX_VISIBILITY_DISTANCE)
	{
		sLog.outError("Visibility.Distance.Grey.Object can't be greater %f", MAX_VISIBILITY_DISTANCE);
		m_VisibleObjectGreyDistance = MAX_VISIBILITY_DISTANCE;
	}

	// visibility on continents
	m_MaxVisibleDistanceOnContinents = sConfig.GetFloatDefault("Visibility.Distance.Continents", DEFAULT_VISIBILITY_DISTANCE);
	if (m_MaxVisibleDistanceOnContinents < 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO))
	{
		sLog.outError("Visibility.Distance.Continents can't be less max aggro radius %f", 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO));
		m_MaxVisibleDistanceOnContinents = 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO);
	}
	else if (m_MaxVisibleDistanceOnContinents + m_VisibleUnitGreyDistance >  MAX_VISIBILITY_DISTANCE)
	{
		sLog.outError("Visibility.Distance.Continents can't be greater %f", MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance);
		m_MaxVisibleDistanceOnContinents = MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance;
	}

	// visibility in instances
	m_MaxVisibleDistanceInInstances = sConfig.GetFloatDefault("Visibility.Distance.Instances", DEFAULT_VISIBILITY_INSTANCE);
	if (m_MaxVisibleDistanceInInstances < 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO))
	{
		sLog.outError("Visibility.Distance.Instances can't be less max aggro radius %f", 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO));
		m_MaxVisibleDistanceInInstances = 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO);
	}
	else if (m_MaxVisibleDistanceInInstances + m_VisibleUnitGreyDistance >  MAX_VISIBILITY_DISTANCE)
	{
		sLog.outError("Visibility.Distance.Instances can't be greater %f", MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance);
		m_MaxVisibleDistanceInInstances = MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance;
	}

	// visibility in BG
	m_MaxVisibleDistanceInBG = sConfig.GetFloatDefault("Visibility.Distance.BG", DEFAULT_VISIBILITY_BG);
	if (m_MaxVisibleDistanceInBG < 45 * sWorld.getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO))
	{
		sLog.outError("Visibility.Distance.BG can't be less max aggro radius %f", 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO));
		m_MaxVisibleDistanceInBG = 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO);
	}
	else if (m_MaxVisibleDistanceInBG + m_VisibleUnitGreyDistance >  MAX_VISIBILITY_DISTANCE)
	{
		sLog.outError("Visibility.Distance.BG can't be greater %f", MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance);
		m_MaxVisibleDistanceInBG = MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance;
	}

	m_MaxVisibleDistanceInFlight = sConfig.GetFloatDefault("Visibility.Distance.InFlight", DEFAULT_VISIBILITY_DISTANCE);
	if (m_MaxVisibleDistanceInFlight + m_VisibleObjectGreyDistance > MAX_VISIBILITY_DISTANCE)
	{
		sLog.outError("Visibility.Distance.InFlight can't be greater %f", MAX_VISIBILITY_DISTANCE - m_VisibleObjectGreyDistance);
		m_MaxVisibleDistanceInFlight = MAX_VISIBILITY_DISTANCE - m_VisibleObjectGreyDistance;
	}

	///- Load the CharDelete related config options
	setConfigMinMax(CONFIG_UINT32_CHARDELETE_METHOD, "CharDelete.Method", 0, 0, 1);
	setConfigMinMax(CONFIG_UINT32_CHARDELETE_MIN_LEVEL, "CharDelete.MinLevel", 0, 0, getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));
	setConfig(CONFIG_UINT32_CHARDELETE_KEEP_DAYS, "CharDelete.KeepDays", 30);

	if (configNoReload(reload, CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE, "GuidReserveSize.Creature", 100))
		setConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE, "GuidReserveSize.Creature", 100);
	if (configNoReload(reload, CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT, "GuidReserveSize.GameObject", 100))
		setConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT, "GuidReserveSize.GameObject", 100);

	///- Read the "Data" directory from the config file
	std::string dataPath = sConfig.GetStringDefault("DataDir", "./");

	// for empty string use current dir as for absent case
	if (dataPath.empty())
		dataPath = "./";
	// normalize dir path to path/ or path\ form
	else if (dataPath.at(dataPath.length() - 1) != '/' && dataPath.at(dataPath.length() - 1) != '\\')
		dataPath.append("/");

	if (reload)
	{
		if (dataPath != m_dataPath)
			sLog.outError("DataDir option can't be changed at mangosd.conf reload, using current value (%s).", m_dataPath.c_str());
	}
	else
	{
		m_dataPath = dataPath;
		sLog.outString("Using DataDir %s", m_dataPath.c_str());
	}

	sLog.outString();
}

/// Initialize the World
void World::SetInitialWorldSettings()
{
	///- Initialize the random number generator
	srand((unsigned int)time(nullptr));

	///- Time server startup
	uint32 uStartTime = WorldTimer::getMSTime();

	///- Initialize detour memory management
	//dtAllocSetCustom(dtCustomAlloc, dtCustomFree);

	///- Initialize config settings
	LoadConfigSettings();

	///- Loading strings. Getting no records means core load has to be canceled because no error message can be output.
	sLog.outString("Loading Origin strings...");
	/*if (!sObjectMgr.LoadMangosStrings())
	{
		Log::WaitBeforeContinueIfNeed();
		exit(1);                                            // Error message displayed in function already
	}*/

	///- Update the realm entry in the database with the realm type from the config file
	// No SQL injection as values are treated as integers

	// not send custom type REALM_FFA_PVP to realm list
	uint32 server_type = IsFFAPvPRealm() ? uint32(REALM_TYPE_PVP) : getConfig(CONFIG_UINT32_GAME_TYPE);
	uint32 realm_zone = getConfig(CONFIG_UINT32_REALM_ZONE);
	LoginDatabase.PExecute("UPDATE realmlist SET icon = %u, timezone = %u WHERE id = '%u'", server_type, realm_zone, realmID);

	///- Remove the bones (they should not exist in DB though) and old corpses after a restart
	CharacterDatabase.PExecute("DELETE FROM corpse WHERE corpse_type = '0' OR time < (UNIX_TIMESTAMP()-'%u')", 3 * DAY);
	
	sLog.outString("Loading DBStore data...");
	sObjectMgr.LoadPlayerInfo();
	sMapEntry.Load();
	for (uint32 i = 1; i < sMapEntry.GetMaxEntry(); ++i)
	{
		MapEntry const* mapEntry = sMapEntry.LookupEntry<MapEntry>(i);
		sLog.outDetail("mapentry: %d", mapEntry->MapID);
		if (!mapEntry)
		{
			sLog.outErrorDb("ObjectMgr::MapEntry: bad template! for '%d'", i);
			sMapEntry.EraseEntry(i);
			continue;
		}
	}
	sAreaEntry.Load();
	for (uint32 i = 1; i < sAreaEntry.GetMaxEntry(); ++i)
	{
		AreaTable const* areaEntry = sAreaEntry.LookupEntry<AreaTable>(i);
		if (!areaEntry)
		{
			sLog.outErrorDb("ObjectMgr::AreaTable: bad template! for '%d'", i);
			sAreaEntry.EraseEntry(i);
			continue;
		}
	}
	///- Init highest guids before any guid using table loading to prevent using not initialized guids in some code.
	sObjectMgr.SetHighestGuids();                           // must be after packing instances
	sLog.outString();
	/*sLog.outString("Loading Script Names...");
	

	sLog.outString("Loading WorldTemplate...");
	

	sLog.outString("Loading InstanceTemplate...");


	sLog.outString("Loading SkillLineAbilityMultiMap Data...");
	

	sLog.outString("Loading SkillRaceClassInfoMultiMap Data...");
	
	///- Clean up and pack instances
	sLog.outString("Cleaning up instances...");
	
	sLog.outString("Packing instances...");
	
	sLog.outString("Packing groups...");
	
															///- Init highest guids before any guid using table loading to prevent using not initialized guids in some code.
	sLog.outString();

	sLog.outString("Loading Page Texts...");
	
	sLog.outString("Loading Game Object Templates...");     // must be after LoadPageTexts
	
	sLog.outString("Loading GameObject models...");
	
	sLog.outString();

	sLog.outString("Loading Spell Chain Data...");
	
	sLog.outString("Loading Spell Elixir types...");
	
	sLog.outString("Loading Spell Facing Flags...");
	
	sLog.outString("Loading Spell Learn Skills...");
	
	sLog.outString("Loading Spell Learn Spells...");
	
	sLog.outString("Loading Spell Proc Event conditions...");
	
	sLog.outString("Loading Spell Bonus Data...");
	
	sLog.outString("Loading Spell Proc Item Enchant...");
	
	sLog.outString("Loading Aggro Spells Definitions...");

	sLog.outString("Loading NPC Texts...");
	
	sLog.outString("Loading Item Random Enchantments Table...");
	
	sLog.outString("Loading Item Templates...");            // must be after LoadRandomEnchantmentsTable and LoadPageTexts
	
	sLog.outString("Loading Item Texts...");
	
	sLog.outString("Loading Creature Model Based Info Data...");
	
	sLog.outString("Loading Equipment templates...");
	
	sLog.outString("Loading Creature Stats...");
	
	sLog.outString("Loading Creature templates...");
	
	sLog.outString("Loading Creature template spells...");
	
	sLog.outString("Loading SpellsScriptTarget...");
	
	sLog.outString("Loading ItemRequiredTarget...");
	
	sLog.outString("Loading Reputation Reward Rates...");
	
	sLog.outString("Loading Creature Reputation OnKill Data...");
	
	sLog.outString("Loading Reputation Spillover Data...");
	
	sLog.outString("Loading Points Of Interest Data...");
	
	sLog.outString("Loading Pet Create Spells...");
	
	sLog.outString("Loading Creature Data...");
	
	sLog.outString("Loading Creature Addon Data...");

	sLog.outString(">>> Creature Addon Data loaded");
	sLog.outString();

	sLog.outString("Loading Gameobject Data...");
	
	sLog.outString("Loading CreatureLinking Data...");      // must be after Creatures
	
	sLog.outString("Loading Objects Pooling Data...");
	
	sLog.outString("Loading Weather Data...");
	
	sLog.outString("Loading Quests...");
	
	sLog.outString("Loading Quests Relations...");
	
	sLog.outString(">>> Quests Relations loaded");
	sLog.outString();

	sLog.outString("Loading Game Event Data...");           // must be after sPoolMgr.LoadFromDB and quests to properly load pool events and quests for events
	
	sLog.outString(">>> Game Event Data loaded");
	sLog.outString();

	// Load Conditions
	sLog.outString("Loading Conditions...");
	

	sLog.outString("Creating map persistent states for non-instanceable maps...");     // must be after PackInstances(), LoadCreatures(), sPoolMgr.LoadFromDB(), sGameEventMgr.LoadFromDB();
	
	sLog.outString();

	sLog.outString("Loading Creature Respawn Data...");     // must be after LoadCreatures(), and sMapPersistentStateMgr.InitWorldMaps()
	
	sLog.outString("Loading Gameobject Respawn Data...");   // must be after LoadGameObjects(), and sMapPersistentStateMgr.InitWorldMaps()
	
	sLog.outString("Loading SpellArea Data...");            // must be after quest load
	
	sLog.outString("Loading AreaTrigger definitions...");
	
	sLog.outString("Loading Quest Area Triggers...");
	
	sLog.outString("Loading Tavern Area Triggers...");

	sLog.outString("Loading AreaTrigger script names...");
	
	sLog.outString("Loading event id script names...");
	
	sLog.outString("Loading Graveyard-zone links...");
	
	sLog.outString("Loading spell target destination coordinates...");
	
	sLog.outString("Loading SpellAffect definitions...");
	
	sLog.outString("Loading spell pet auras...");
	
	sLog.outString("Loading Player Create Info & Level Stats...");
	
	sLog.outString(">>> Player Create Info & Level Stats loaded");
	sLog.outString();

	sLog.outString("Loading Exploration BaseXP Data...");
	
	sLog.outString("Loading Pet Name Parts...");
	
	sLog.outString();

	sLog.outString("Loading the max pet number...");
	
	sLog.outString("Loading pet level stats...");
	
	sLog.outString("Loading Player Corpses...");
	
	sLog.outString("Loading Loot Tables...");
	
	sLog.outString(">>> Loot Tables loaded");
	sLog.outString();

	sLog.outString("Loading Skill Fishing base level requirements...");
	
	sLog.outString("Loading Npc Text Id...");
	
	sLog.outString("Loading Gossip scripts...");
	

	sLog.outString("Loading Vendors...");
	
	sLog.outString("Loading Trainers...");
	
	sLog.outString("Loading Waypoint scripts...");          // before loading from creature_movement
	
	sLog.outString("Loading Waypoints...");
	
	sLog.outString("Loading ReservedNames...");
	
	sLog.outString("Loading GameObjects for quests...");
	
	sLog.outString("Loading BattleMasters...");
	
	sLog.outString("Loading BattleGround event indexes...");
	
	sLog.outString("Loading GameTeleports...");
	
	///- Loading localization data
	sLog.outString("Loading Localization strings...");
	
	sLog.outString(">>> Localization strings loaded");
	sLog.outString();

	///- Load dynamic data tables from the database
	sLog.outString("Loading Auctions...");
	
	sLog.outString(">>> Auctions loaded");
	sLog.outString();

	sLog.outString("Loading Guilds...");
	
	sLog.outString("Loading Groups...");
	
	sLog.outString("Returning old mails...");

	sLog.outString("Loading GM tickets...");
	
	///- Load and initialize DBScripts Engine
	sLog.outString("Loading DB-Scripts Engine...");
	
	sLog.outString(">>> Scripts loaded");
	sLog.outString();

	sLog.outString("Loading Scripts text locales...");      // must be after Load*Scripts calls
	
	///- Load and initialize EventAI Scripts
	sLog.outString("Loading CreatureEventAI Texts...");
	
	sLog.outString("Loading CreatureEventAI Summons...");
	
	sLog.outString("Loading CreatureEventAI Scripts...");
	
	///- Load and initialize scripting library
	/*sLog.outString("Initializing Scripting Library...");
	switch (sScriptMgr.LoadScriptLibrary(MANGOS_SCRIPT_NAME))
	{
	case SCRIPT_LOAD_OK:
		sLog.outString("Scripting library loaded.");
		break;
	case SCRIPT_LOAD_ERR_NOT_FOUND:
		sLog.outError("Scripting library not found or not accessible.");
		break;
	case SCRIPT_LOAD_ERR_WRONG_API:
		sLog.outError("Scripting library has wrong list functions (outdated?).");
		break;
	case SCRIPT_LOAD_ERR_OUTDATED:
		sLog.outError("Scripting library build for old mangosd revision. You need rebuild it.");
		break;
	}
	sLog.outString();*/

	///- Initialize game time and timers
	sLog.outString("Initialize game time and timers");
	m_gameTime = time(nullptr);
	m_startTime = m_gameTime;

	tm local;
	time_t curr;
	time(&curr);
	local = *(localtime(&curr));                            // dereference and assign
	char isoDate[128];
	sprintf(isoDate, "%04d-%02d-%02d %02d:%02d:%02d",
		local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);

	LoginDatabase.PExecute("INSERT INTO uptime (realmid, starttime, startstring, uptime) VALUES('%u', " UI64FMTD ", '%s', 0)",
		realmID, uint64(m_startTime), isoDate);

	m_timers[WUPDATE_AUCTIONS].SetInterval(MINUTE * IN_MILLISECONDS);
	m_timers[WUPDATE_UPTIME].SetInterval(getConfig(CONFIG_UINT32_UPTIME_UPDATE)*MINUTE * IN_MILLISECONDS);
	// Update "uptime" table based on configuration entry in minutes.
	m_timers[WUPDATE_CORPSES].SetInterval(20 * MINUTE * IN_MILLISECONDS);
	m_timers[WUPDATE_DELETECHARS].SetInterval(DAY * IN_MILLISECONDS); // check for chars to delete every day

																	  // for AhBot
	m_timers[WUPDATE_AHBOT].SetInterval(20 * IN_MILLISECONDS); // every 20 sec

															   // to set mailtimer to return mails every day between 4 and 5 am
															   // mailtimer is increased when updating auctions
															   // one second is 1000 -(tested on win system)
	mail_timer = uint32((((localtime(&m_gameTime)->tm_hour + 20) % 24) * HOUR * IN_MILLISECONDS) / m_timers[WUPDATE_AUCTIONS].GetInterval());
	// 1440
	mail_timer_expires = uint32((DAY * IN_MILLISECONDS) / (m_timers[WUPDATE_AUCTIONS].GetInterval()));
	DEBUG_LOG("Mail timer set to: %u, mail return is called every %u minutes", mail_timer, mail_timer_expires);

	///- Initialize static helper structures
	///AIRegistry::Initialize();
	//::InitVisibleBits();

	///- Initialize MapManager
	sLog.outString("Starting Map System");
	sMapMgr.Initialize();
	sLog.outString();

	///- Initialize Battlegrounds
	sLog.outString("Starting BattleGround System");
	
	///- Initialize Outdoor PvP
	sLog.outString("Starting Outdoor PvP System");
	
	// Not sure if this can be moved up in the sequence (with static data loading) as it uses MapManager
	sLog.outString("Loading Transports...");
	
	sLog.outString("Deleting expired bans...");
	LoginDatabase.Execute("DELETE FROM ip_banned WHERE unbandate<=UNIX_TIMESTAMP() AND unbandate<>bandate");
	sLog.outString();

	sLog.outString("Starting server Maintenance system...");
	InitServerMaintenanceCheck();

	sLog.outString("Loading Honor Standing list...");
	
	sLog.outString("Starting Game Event system...");
	//uint32 nextGameEvent = sGameEventMgr.Initialize();
	//m_timers[WUPDATE_EVENTS].SetInterval(nextGameEvent);    // depend on next event
	sLog.outString();

	sLog.outString("Loading grids for active creatures or transports...");
	sLog.outString();

	// Delete all characters which have been deleted X days before
	
	sLog.outString("Initialize AuctionHouseBot...");
	sLog.outString();

	sLog.outString("---------------------------------------");
	sLog.outString("      ORIGIN : World initialized       ");
	sLog.outString("---------------------------------------");
	sLog.outString();

	uint32 uStartInterval = WorldTimer::getMSTimeDiff(uStartTime, WorldTimer::getMSTime());
	sLog.outString("SERVER STARTUP TIME: %i minutes %i seconds", uStartInterval / 60000, (uStartInterval % 60000) / 1000);
	sLog.outString();


	/*sLog.outDetail("virtualisating map 0 and 1");

	uint32 continents[] = { 0, 1 };
	Map* _map = nullptr;
	for (int i = 0; i < countof(continents); ++i)
	{
		_map = sMapMgr.FindMap(continents[i]);
		if (!_map)
			_map = sMapMgr.CreateMap(continents[i], nullptr);
		else
			sLog.outError("ObjectMgr::LoadActiveEntities - Unable to create Map %u", continents[i]);
	}*/
}
void World::SetPlayerLimit(int32 limit, bool needUpdate)
{
	if (limit < -SEC_ADMINISTRATOR)
		limit = -SEC_ADMINISTRATOR;

	// lock update need
	bool db_update_need = needUpdate || (limit < 0) != (m_playerLimit < 0) || (limit < 0 && m_playerLimit < 0 && limit != m_playerLimit);

	m_playerLimit = limit;

	if (db_update_need)
		LoginDatabase.PExecute("UPDATE realmlist SET allowedSecurityLevel = '%u' WHERE id = '%u'",
			uint32(GetPlayerSecurityLimit()), realmID);
}
void World::setConfig(eConfigUInt32Values index, char const* fieldname, uint32 defvalue)
{
	setConfig(index, sConfig.GetIntDefault(fieldname, defvalue));
	if (int32(getConfig(index)) < 0)
	{
		sLog.outError("%s (%i) can't be negative. Using %u instead.", fieldname, int32(getConfig(index)), defvalue);
		setConfig(index, defvalue);
	}
}

void World::setConfig(eConfigInt32Values index, char const* fieldname, int32 defvalue)
{
	setConfig(index, sConfig.GetIntDefault(fieldname, defvalue));
}

void World::setConfig(eConfigFloatValues index, char const* fieldname, float defvalue)
{
	setConfig(index, sConfig.GetFloatDefault(fieldname, defvalue));
}

void World::setConfig(eConfigBoolValues index, char const* fieldname, bool defvalue)
{
	setConfig(index, sConfig.GetBoolDefault(fieldname, defvalue));
}

void World::setConfigPos(eConfigFloatValues index, char const* fieldname, float defvalue)
{
	setConfig(index, fieldname, defvalue);
	if (getConfig(index) < 0.0f)
	{
		sLog.outError("%s (%f) can't be negative. Using %f instead.", fieldname, getConfig(index), defvalue);
		setConfig(index, defvalue);
	}
}

void World::setConfigMin(eConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 minvalue)
{
	setConfig(index, fieldname, defvalue);
	if (getConfig(index) < minvalue)
	{
		sLog.outError("%s (%u) must be >= %u. Using %u instead.", fieldname, getConfig(index), minvalue, minvalue);
		setConfig(index, minvalue);
	}
}

void World::setConfigMin(eConfigInt32Values index, char const* fieldname, int32 defvalue, int32 minvalue)
{
	setConfig(index, fieldname, defvalue);
	if (getConfig(index) < minvalue)
	{
		sLog.outError("%s (%i) must be >= %i. Using %i instead.", fieldname, getConfig(index), minvalue, minvalue);
		setConfig(index, minvalue);
	}
}

void World::setConfigMin(eConfigFloatValues index, char const* fieldname, float defvalue, float minvalue)
{
	setConfig(index, fieldname, defvalue);
	if (getConfig(index) < minvalue)
	{
		sLog.outError("%s (%f) must be >= %f. Using %f instead.", fieldname, getConfig(index), minvalue, minvalue);
		setConfig(index, minvalue);
	}
}

void World::setConfigMinMax(eConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 minvalue, uint32 maxvalue)
{
	setConfig(index, fieldname, defvalue);
	if (getConfig(index) < minvalue)
	{
		sLog.outError("%s (%u) must be in range %u...%u. Using %u instead.", fieldname, getConfig(index), minvalue, maxvalue, minvalue);
		setConfig(index, minvalue);
	}
	else if (getConfig(index) > maxvalue)
	{
		sLog.outError("%s (%u) must be in range %u...%u. Using %u instead.", fieldname, getConfig(index), minvalue, maxvalue, maxvalue);
		setConfig(index, maxvalue);
	}
}

void World::setConfigMinMax(eConfigInt32Values index, char const* fieldname, int32 defvalue, int32 minvalue, int32 maxvalue)
{
	setConfig(index, fieldname, defvalue);
	if (getConfig(index) < minvalue)
	{
		sLog.outError("%s (%i) must be in range %i...%i. Using %i instead.", fieldname, getConfig(index), minvalue, maxvalue, minvalue);
		setConfig(index, minvalue);
	}
	else if (getConfig(index) > maxvalue)
	{
		sLog.outError("%s (%i) must be in range %i...%i. Using %i instead.", fieldname, getConfig(index), minvalue, maxvalue, maxvalue);
		setConfig(index, maxvalue);
	}
}

void World::setConfigMinMax(eConfigFloatValues index, char const* fieldname, float defvalue, float minvalue, float maxvalue)
{
	setConfig(index, fieldname, defvalue);
	if (getConfig(index) < minvalue)
	{
		sLog.outError("%s (%f) must be in range %f...%f. Using %f instead.", fieldname, getConfig(index), minvalue, maxvalue, minvalue);
		setConfig(index, minvalue);
	}
	else if (getConfig(index) > maxvalue)
	{
		sLog.outError("%s (%f) must be in range %f...%f. Using %f instead.", fieldname, getConfig(index), minvalue, maxvalue, maxvalue);
		setConfig(index, maxvalue);
	}
}

void World::InitServerMaintenanceCheck()
{
	QueryResult* result = CharacterDatabase.Query("SELECT NextMaintenanceDate FROM saved_variables");
	if (!result)
	{
		DEBUG_LOG("Maintenance date not found in SavedVariables, reseting it now.");
		uint32 mDate = GetDateLastMaintenanceDay();
		m_NextMaintenanceDate = mDate == GetDateToday() ? mDate : mDate + 7;
		CharacterDatabase.PExecute("INSERT INTO saved_variables (NextMaintenanceDate) VALUES ('" UI64FMTD "')", uint64(m_NextMaintenanceDate));
	}
	else
	{
		m_NextMaintenanceDate = (*result)[0].GetUInt64();
		delete result;
	}

	if (m_NextMaintenanceDate <= GetDateToday())
		ServerMaintenanceStart();

	DEBUG_LOG("Server maintenance check initialized.");
}
bool World::configNoReload(bool reload, eConfigUInt32Values index, char const* fieldname, uint32 defvalue)
{
	if (!reload)
		return true;

	uint32 val = sConfig.GetIntDefault(fieldname, defvalue);
	if (val != getConfig(index))
		sLog.outError("%s option can't be changed at mangosd.conf reload, using current value (%u).", fieldname, getConfig(index));

	return false;
}

bool World::configNoReload(bool reload, eConfigInt32Values index, char const* fieldname, int32 defvalue)
{
	if (!reload)
		return true;

	int32 val = sConfig.GetIntDefault(fieldname, defvalue);
	if (val != getConfig(index))
		sLog.outError("%s option can't be changed at mangosd.conf reload, using current value (%i).", fieldname, getConfig(index));

	return false;
}

bool World::configNoReload(bool reload, eConfigFloatValues index, char const* fieldname, float defvalue)
{
	if (!reload)
		return true;

	float val = sConfig.GetFloatDefault(fieldname, defvalue);
	if (val != getConfig(index))
		sLog.outError("%s option can't be changed at mangosd.conf reload, using current value (%f).", fieldname, getConfig(index));

	return false;
}

bool World::configNoReload(bool reload, eConfigBoolValues index, char const* fieldname, bool defvalue)
{
	if (!reload)
		return true;

	bool val = sConfig.GetBoolDefault(fieldname, defvalue);
	if (val != getConfig(index))
		sLog.outError("%s option can't be changed at mangosd.conf reload, using current value (%s).", fieldname, getConfig(index) ? "'true'" : "'false'");

	return false;
}
void World::ServerMaintenanceStart()
{
	uint32 LastWeekEnd = GetDateLastMaintenanceDay();
	m_NextMaintenanceDate = LastWeekEnd + 7; // next maintenance begin

	if (m_NextMaintenanceDate <= GetDateToday())            // avoid loop in manually case, maybe useless
		m_NextMaintenanceDate += 7;

	// flushing rank points list ( standing must be reloaded after server maintenance )
	/*bjectMgr.FlushRankPoints(LastWeekEnd);

	// save and update all online players
	for (SessionMap::iterator itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
		if (itr->second->GetPlayer() && itr->second->GetPlayer()->IsInWorld())
			itr->second->GetPlayer()->SaveToDB();
			*/
	CharacterDatabase.PExecute("UPDATE saved_variables SET NextMaintenanceDate = '" UI64FMTD "'", uint64(m_NextMaintenanceDate));
}
void World::UpdateResultQueue()
{
	// process async result queues
	CharacterDatabase.ProcessResultQueue();
	WorldDatabase.ProcessResultQueue();
	LoginDatabase.ProcessResultQueue();
}
/// Update the World !
void World::Update(uint32 diff)
{
	/// <li> Handle session updates
	UpdateSessions(diff);
	/// <li> Update uptime table
	if (m_timers[WUPDATE_UPTIME].Passed())
	{
		uint32 tmpDiff = uint32(m_gameTime - m_startTime);
		uint32 maxClientsNum = GetMaxActiveSessionCount();

		m_timers[WUPDATE_UPTIME].Reset();
		LoginDatabase.PExecute("UPDATE uptime SET uptime = %u, maxplayers = %u WHERE realmid = %u AND starttime = " UI64FMTD, tmpDiff, maxClientsNum, realmID, uint64(m_startTime));
	}
	/// <li> Handle all other objects
	///- Update objects (maps, transport, creatures,...)
	sMapMgr.Update(diff);

	/// delete old character

	// execute callbacks from sql queries that were queued recently
	UpdateResultQueue();

	/// process game event

	/// </ul>
	///- Move all creatures with "delayed move" and remove and delete all objects with "delayed remove"
	sMapMgr.RemoveAllObjectsInRemoveList();

	ProcessCliCommands();
}
void World::ProcessCliCommands()
{
	std::lock_guard<std::mutex> guard(m_cliCommandQueueLock);

	while (!m_cliCommandQueue.empty())
	{
		auto const command = m_cliCommandQueue.front();
		m_cliCommandQueue.pop_front();

		DEBUG_LOG("CLI command under processing...");

		//CliHandler handler(command->m_cliAccountId, command->m_cliAccessLevel, command->m_print);
		//handler.ParseCommands(&command->m_command[0]);

		//if (command->m_commandFinished)
			//command->m_commandFinished(!handler.HasSentErrorMessage());

		delete command;
	}
}
void World::AddSession(WorldSession* s)
{
	std::lock_guard<std::mutex> guard(m_sessionAddQueueLock);

	m_sessionAddQueue.push_back(s);
}
bool World::RemoveQueuedSession(WorldSession* sess)
{
	// sessions count including queued to remove (if removed_session set)
	uint32 sessions = GetActiveSessionCount();

	uint32 position = 1;
	Queue::iterator iter = m_QueuedSessions.begin();

	// search to remove and count skipped positions
	bool found = false;

	for (; iter != m_QueuedSessions.end(); ++iter, ++position)
	{
		if (*iter == sess)
		{
			sess->SetInQueue(false);
			iter = m_QueuedSessions.erase(iter);
			found = true;                                   // removing queued session
			break;
		}
	}

	// iter point to next socked after removed or end()
	// position store position of removed socket and then new position next socket after removed

	// if session not queued then we need decrease sessions count
	if (!found && sessions)
		--sessions;

	// accept first in queue
	if ((!m_playerLimit || (int32)sessions < m_playerLimit) && !m_QueuedSessions.empty())
	{
		WorldSession* pop_sess = m_QueuedSessions.front();
		pop_sess->SetInQueue(false);
		pop_sess->SendAuthWaitQue(0);
		m_QueuedSessions.pop_front();

		// update iter to point first queued socket or end() if queue is empty now
		iter = m_QueuedSessions.begin();
		position = 1;
	}

	// update position from iter to end()
	// iter point to first not updated socket, position store new position
	for (; iter != m_QueuedSessions.end(); ++iter, ++position)
		(*iter)->SendAuthWaitQue(position);

	return found;
}
void World::UpdateMaxSessionCounters()
{
	m_maxActiveSessionCount = std::max(m_maxActiveSessionCount, uint32(m_sessions.size() - m_QueuedSessions.size()));
	m_maxQueuedSessionCount = std::max(m_maxQueuedSessionCount, uint32(m_QueuedSessions.size()));
}
void World::AddQueuedSession(WorldSession* sess)
{
	sess->SetInQueue(true);
	m_QueuedSessions.push_back(sess);

	// [-ZERO] Possible wrong
	// The 1st SMSG_AUTH_RESPONSE needs to contain other info too.
	WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4);
	packet << uint8(AUTH_WAIT_QUEUE);
	packet << uint32(GetQueuedSessionPos(sess));            // position in queue
	packet << uint32(0);
	sess->SendPacket(&packet);
}

/// Remove a given session
bool World::RemoveSession(uint32 id)
{
	///- Find the session, kick the user, but we can't delete session at this moment to prevent iterator invalidation
	SessionMap::const_iterator itr = m_sessions.find(id);

	if (itr != m_sessions.end() && itr->second)
	{
		if (itr->second->PlayerLoading())
			return false;
		itr->second->KickPlayer();
	}

	return true;
}
/// Find a session by its id
WorldSession* World::FindSession(uint32 id) const
{
	SessionMap::const_iterator itr = m_sessions.find(id);

	if (itr != m_sessions.end())
		return itr->second;                                 // also can return nullptr for kicked session
	else
		return nullptr;
}
void World::AddSession_(WorldSession* s)
{
	ORIGIN_ASSERT(s);

	// NOTE - Still there is race condition in WorldSession* being used in the Sockets

	///- kick already loaded player with same account (if any) and remove session
	///- if player is in loading and want to load again, return
	if (!RemoveSession(s->GetAccountId()))
	{
		sLog.outError("World::AddSession_ kicking player");
		s->KickPlayer();
		delete s;                                           // session not added yet in session list, so not listed in queue
		return;
	}
	
	// decrease session counts only at not reconnection case
	bool decrease_session = true;

	// if session already exist, prepare to it deleting at next world update
	// NOTE - KickPlayer() should be called on "old" in RemoveSession()
	{
		SessionMap::const_iterator old = m_sessions.find(s->GetAccountId());

		if (old != m_sessions.end())
		{
			// prevent decrease sessions count if session queued
			if (RemoveQueuedSession(old->second))
				decrease_session = false;
		}
	}

	m_sessions[s->GetAccountId()] = s;

	uint32 Sessions = GetActiveAndQueuedSessionCount();
	uint32 pLimit = GetPlayerAmountLimit();
	uint32 QueueSize = GetQueuedSessionCount();             // number of players in the queue

															// so we don't count the user trying to
															// login as a session and queue the socket that we are using
	if (decrease_session)
		--Sessions;
	if (pLimit > 0 && Sessions >= pLimit && s->GetSecurity() == SEC_PLAYER)
	{
		sLog.outDetail("sending AUTH_QUEUE");
		AddQueuedSession(s);
		UpdateMaxSessionCounters();
		DETAIL_LOG("PlayerQueue: Account id %u is in Queue Position (%u).", s->GetAccountId(), ++QueueSize);
		sLog.outDetail("PlayerQueue: Account id %u is in Queue Position (%u).", s->GetAccountId(), ++QueueSize);
		return;
	}

	// Checked for 1.12.2
	WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
	packet << uint8(AUTH_OK);
	packet << uint32(0);
	s->SendPacket(&packet);
	sLog.outDetail("sending AUTH_OK");
	UpdateMaxSessionCounters();

	// Updates the population
	if (pLimit > 0)
	{
		float popu = float(GetActiveSessionCount());        // updated number of users on the server
		popu /= pLimit;
		popu *= 2;

		static SqlStatementID id;

		SqlStatement stmt = LoginDatabase.CreateStatement(id, "UPDATE realmlist SET population = ? WHERE id = ?");
		stmt.PExecute(popu, realmID);

		DETAIL_LOG("Server Population (%f).", popu);
	}
}
int32 World::GetQueuedSessionPos(WorldSession* sess)
{
	uint32 position = 1;

	for (Queue::const_iterator iter = m_QueuedSessions.begin(); iter != m_QueuedSessions.end(); ++iter, ++position)
		if ((*iter) == sess)
			return position;

	return 0;
}
void World::UpdateSessions(uint32 /*diff*/)
{
	///- Add new sessions
	{
		std::lock_guard<std::mutex> guard(m_sessionAddQueueLock);

		for (auto const &session : m_sessionAddQueue)
			AddSession_(session);

		m_sessionAddQueue.clear();
	}

	///- Then send an update signal to remaining ones
	for (SessionMap::iterator itr = m_sessions.begin(); itr != m_sessions.end(); )
	{
		///- and remove not active sessions from the list
		WorldSession* pSession = itr->second;
		WorldSessionFilter updater(pSession);

		// if WorldSession::Update fails, it means that the session should be destroyed
		if (!pSession->Update(updater))
		{
			sLog.outError("Session must be destroyed");
			RemoveQueuedSession(pSession);
			itr = m_sessions.erase(itr);
			delete pSession;
		}
		else
			++itr;
	}
}
#ifndef __UNIT_H
#define __UNIT_H

#include "Common.h"
#include "Object.h"
#include "../Server/Opcodes.h"
#include "../Server/SharedDefine.h"
#include "UpdateFields.h"
#include "WorldPacket.h"
#include "Timer.h"

#include <list>

// Regeneration defines
#define REGEN_TIME_FULL     2000                            // For this time difference is computed regen value
#define MAX_MOVE_TYPE		3

// byte flags value (UNIT_FIELD_BYTES_2,1)
enum UnitBytes2_Flags
{
	UNIT_BYTE2_FLAG_UNK0 = 0x01,
	UNIT_BYTE2_FLAG_UNK1 = 0x02,
	UNIT_BYTE2_FLAG_UNK2 = 0x04,
	UNIT_BYTE2_FLAG_SUPPORTABLE = 0x08,                     // allows for being targeted for healing/bandaging by friendlies
	UNIT_BYTE2_FLAG_AURAS = 0x10,                     // show possitive auras as positive, and allow its dispel
	UNIT_BYTE2_FLAG_UNK5 = 0x20,                     // show negative auras as positive, *not* allowing dispel (at least for pets)
	UNIT_BYTE2_FLAG_UNK6 = 0x40,
	UNIT_BYTE2_FLAG_UNK7 = 0x80
};

enum DeathState
{
	ALIVE = 0,                                     // show as alive
	JUST_DIED = 1,                                     // temporary state at die, for creature auto converted to CORPSE, for player at next update call
	CORPSE = 2,                                     // corpse state, for player this also meaning that player not leave corpse
	DEAD = 3,                                     // for creature despawned state (corpse despawned), for player CORPSE/DEAD not clear way switches (FIXME), and use m_deathtimer > 0 check for real corpse state
	JUST_ALIVED = 4,                                     // temporary state at resurrection, for creature auto converted to ALIVE, for player at next update call
};
enum UnitFlags
{
	UNIT_FLAG_NONE = 0x00000000,
	UNIT_FLAG_NON_ATTACKABLE = 0x00000001,
	UNIT_FLAG_DISABLE_MOVE = 0x00000002,
	UNIT_FLAG_PVP_ATTACKABLE = 0x00000004,
	UNIT_FLAG_RENAME = 0x0000008,
	UNIT_FLAG_RESTING = 0x00000010,
	UNIT_FLAG_OOC_NOT_ATTACKABLE = 0x00000020, // (OOC Out Of Combat)
	UNIT_FLAG_PASSIVE = 0x00000040,           // makes you unable to attack everything.
	UNIT_FLAG_PVP = 0x00000100,
	UNIT_FLAG_SILENCED = 0x00000200,           // silenced, 2.1.1
	UNIT_FLAG_PACIFIED = 0x00001000,
	UNIT_FLAG_DISABLE_ROTATE = 0x00002000,
	UNIT_FLAG_IN_COMBAT = 0x00040000,
	UNIT_FLAG_NOT_SELECTABLE = 0x00080000,
	UNIT_FLAG_SKINNABLE = 0x02000000,
	UNIT_FLAG_AURAS_VISIBLE = 0x04000000,           // magic detect
	UNIT_FLAG_SHEATHE = 0x08000000,
};
/// Non Player Character flags
enum NPCFlags
{
	UNIT_NPC_FLAG_NONE = 0x00000000,
	UNIT_NPC_FLAG_GOSSIP = 0x00000001,
	UNIT_NPC_FLAG_QUESTGIVER = 0x00000002,
	UNIT_NPC_FLAG_VENDOR = 0x00000004,
	UNIT_NPC_FLAG_FLIGHTMASTER = 0x00000008,
	UNIT_NPC_FLAG_TRAINER = 0x00000010,
	UNIT_NPC_FLAG_SPIRITHEALER = 0x00000020,
	UNIT_NPC_FLAG_SPIRITGUIDE = 0x00000040,
	UNIT_NPC_FLAG_INNKEEPER = 0x00000080,
	UNIT_NPC_FLAG_BANKER = 0x00000100,
	UNIT_NPC_FLAG_PETITIONER = 0x00000200,
	UNIT_NPC_FLAG_TABARDDESIGNER = 0x00000400,
	UNIT_NPC_FLAG_BATTLEMASTER = 0x00000800,
	UNIT_NPC_FLAG_AUCTIONEER = 0x00001000,
	UNIT_NPC_FLAG_STABLEMASTER = 0x00002000,
	UNIT_NPC_FLAG_REPAIR = 0x00004000,
	UNIT_NPC_FLAG_OUTDOORPVP = 0x20000000,
};
enum UnitMoveType
{
	MOVE_WALK = 0,
	MOVE_RUN = 1,
	MOVE_RUN_BACK = 2
};
class Unit : public WorldObject
{
public:
	virtual ~Unit();

	void AddToWorld() override;
	void RemoveFromWorld() override;
	void CleanupsBeforeDelete() override;               // used in ~Creature/~Player (or before mass creature delete to remove cross-references to already deleted units)
	void Update(uint32 update_diff, uint32 time) override;

	virtual bool IsInWater() const;
	virtual bool IsUnderWater() const;
	// stat system
	uint32 getLevel() const { return GetUInt32Value(UNIT_FIELD_LEVEL); }
	virtual uint32 GetLevelForTarget(Unit const* /*target*/) const { return getLevel(); }

	void SetLevel(uint32 lvl);
	void setClass(uint8 val) { SetByteValue(UNIT_FIELD_BYTES_0, 0, val); }
	uint8 getClass() const { return GetByteValue(UNIT_FIELD_BYTES_0, 0); }
	uint32 getClassMask() const { return 1 << (getClass() - 1); }
	void setGender(uint8 val) { SetUInt16Value(PLAYER_BYTES_3, 0, val); }
	uint8 getGender() const { return GetUInt16Value(PLAYER_BYTES_3, 0); }

	void SetHealth(uint32 val);
	void SetMaxHealth(uint32 val);
	void SetHealthPercent(float percent);
	int32 ModifyHealth(int32 val);
	uint32 GetHealth()    const;
	uint32 GetMaxHealth() const;
	float GetHealthPercent() const;

	float GetStat(Stats stat) const;
	void SetStat(Stats stat, int32 val);
	//uint32 GetArmor() const { return GetResistance(SPELL_SCHOOL_NORMAL); }
	//void SetArmor(int32 val) { SetResistance(SPELL_SCHOOL_NORMAL, val); }

	void SetCreateHealth(uint32 val) { SetUInt32Value(UNIT_FIELD_BASE_HEALTH, val); }
	uint32 GetCreateHealth() const { return GetUInt32Value(UNIT_FIELD_BASE_HEALTH); }
	void SetCreateEnery(uint32 val) { SetUInt32Value(UNIT_FIELD_BASE_ENERGY, val); }
	uint32 GetCreateEnergy() const { return GetUInt32Value(UNIT_FIELD_BASE_ENERGY); }
	uint32 GetCreatePowers(Powers power) const;
	
	void SetPowerType(Powers power);
	uint32 GetPower(Powers power) const;
	uint32 GetMaxPower(Powers power) const;
	void SetPower(Powers power, uint32 val);
	void SetMaxPower(Powers power, uint32 val);
	int32 ModifyPower(Powers power, int32 val);
	void ApplyPowerMod(Powers power, uint32 val, bool apply);
	void ApplyMaxPowerMod(Powers power, uint32 val, bool apply);
	void setCastSpeed(float val) { castSpeed = val; }
	float getCastSpeed()const { return castSpeed; }
	void setAuraState(AuraState state) { aurastate = state; }
	AuraState getAurastate() { return aurastate; }
	/*virtual bool CanSwim() const = 0;
	virtual bool CanFly() const = 0;*/

	bool isAlive() const { return (m_deathState == ALIVE); };
	bool isDead() const { return (m_deathState == DEAD || m_deathState == CORPSE); };

	void CreateObject() override;
	void UpdateObject() override;
	void DeleteObject() override;

	void		UpdateSpeed(UnitMoveType mtype, bool forced, float ratio = 1.0f);
	float		GetSpeed(UnitMoveType mtype) const;
	float		GetSpeedRate(UnitMoveType mtype) const { return m_speed_rate[mtype]; }
	void		SetSpeedRate(UnitMoveType mtype, float rate, bool forced = false);
protected:
	explicit Unit();
	float		createdstats[MAX_STATS];
	uint32		stats[MAX_STATS];
	float		m_speed_rate[MAX_MOVE_TYPE];

	AuraState	aurastate;
	float		castSpeed;


	uint32 m_regenTimer;
	DeathState m_deathState;
};

#endif

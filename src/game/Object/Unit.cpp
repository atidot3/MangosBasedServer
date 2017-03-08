#include "Unit.h"
#include "Log.h"
#include "../Server/Opcodes.h"
#include "WorldPacket.h"
#include "../Server/WorldSession.h"
#include "../World/World.h"
#include "ObjectGuid.h"
#include "Util.h"

#include <math.h>
#include <array>

Unit::Unit()
{
	m_objectType |= TYPEMASK_UNIT;
	m_objectTypeId = TYPEID_UNIT;
	m_updateFlag = (UPDATEFLAG_ALL | UPDATEFLAG_LIVING | UPDATEFLAG_HAS_POSITION);
	m_regenTimer = 0;
	m_deathState = ALIVE;
	for (int i = 0; i < MAX_MOVE_TYPE; ++i)
		m_speed_rate[i] = 1.0f;
}

Unit::~Unit()
{
}
bool Unit::IsInWater() const
{
	return false;
}
void Unit::SetLevel(uint32 lvl)
{
	SetUInt32Value(UNIT_FIELD_LEVEL, lvl);
	//// group update
	//if ((GetTypeId() == TYPEID_PLAYER) && ((Player*)this)->GetGroup())
	//	((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_LEVEL);
}
void Unit::SetHealth(uint32 val)
{
	uint32 maxHealth = GetMaxHealth();
	if (maxHealth < val)
		val = maxHealth;

	SetUInt32Value(UNIT_FIELD_HEALTH, val);
}
uint32 Unit::GetPower(Powers power) const
{
	return GetUInt32Value(UNIT_FIELD_POWER1 + power);
}
void Unit::SetMaxPower(Powers power, uint32 val)
{
	uint32 cur_power = GetPower(power);
	switch (power)
	{
		case Powers::POWER_ENERGY:
		{
			SetStatInt32Value(UNIT_FIELD_MAXPOWER1 + power, val);
			break;
		}
		case Powers::POWER_MANA:
		{
			SetStatInt32Value(UNIT_FIELD_MAXPOWER1 + power, val);
			break;
		}
	}
	if (val < cur_power)
		SetPower(power, val);
}
uint32 Unit::GetMaxPower(Powers power) const
{
	const uint32 pow = GetUInt32Value(UNIT_FIELD_MAXPOWER1 + power);
	return pow;
}
void Unit::SetStat(Stats stat, int32 val)
{
	switch(stat)
	{
		case Stats::STAT_STR:
		{
			stats[0] = val;
			break;
		}
		case Stats::STAT_STA:
		{
			stats[1] = val;
			break;
		}
		case Stats::STAT_INT:
		{
			stats[2] = val;
			break;
		}
		case Stats::STAT_HEA:
		{
			stats[3] = val;
			break;
		}
	}
}
float Unit::GetStat(Stats stat) const
{ 
	switch (stat)
	{
		case Stats::STAT_STR:
		{
			return stats[0];
		}
		case Stats::STAT_STA:
		{
			return stats[1];
		}
		case Stats::STAT_INT:
		{
			return stats[2];
		}
		case Stats::STAT_HEA:
		{
			return stats[3];
		}
	}
	return 0;
}
void Unit::SetMaxHealth(uint32 val)
{
	uint32 health = GetHealth();
	SetUInt32Value(UNIT_FIELD_MAXHEALTH, val);

	if (val < health)
		SetHealth(val);
}
void Unit::SetPowerType(Powers new_powertype)
{
	// set power type
	SetByteValue(UNIT_FIELD_BYTES_0, 3, new_powertype);

	// special cases for power type switching (druid and pets only)
	if (GetTypeId() == TYPEID_PLAYER)
	{
		uint32 maxValue = GetCreatePowers(new_powertype);
		uint32 curValue = maxValue;

		// special cases with current power = 0
		if (new_powertype == POWER_MANA)
			curValue = 0;

		// set power (except for mana)
		if (new_powertype != POWER_ENERGY)
		{
			SetMaxPower(new_powertype, maxValue);
			SetPower(new_powertype, curValue);
		}
	}
}
uint32 Unit::GetCreatePowers(Powers power) const
{
	switch (power)
	{
	case POWER_HEALTH:      return 0;                   // is it really should be here?
	case POWER_ENERGY:      return GetCreateEnergy();
	case POWER_MANA:			return 0; // 0 cause rp point need to be charged or increse while fighting
	}

	return 0;
}
void Unit::SetPower(Powers power, uint32 val)
{
	if (GetPower(power) == val)
		return;

	uint32 maxPower = GetMaxPower(power);
	if (maxPower < val)
		val = maxPower;

	switch (power)
	{
		case Powers::POWER_ENERGY:
		{
			SetStatInt32Value(UNIT_FIELD_POWER1 + power, val);
		}
		case Powers::POWER_MANA:
		{
			SetStatInt32Value(UNIT_FIELD_POWER1 + power, val);
		}
	}
}
bool Unit::IsUnderWater() const
{
	return false;
}
void Unit::Update(uint32 update_diff, uint32 p_time)
{
	if (!IsInWorld())
		return;
}
void Unit::AddToWorld()
{
	Object::AddToWorld();
}
void Unit::RemoveFromWorld()
{
	Object::RemoveFromWorld();
}
void Unit::CleanupsBeforeDelete()
{
	WorldObject::CleanupsBeforeDelete();
}
int32 Unit::ModifyHealth(int32 dVal)
{
	if (dVal == 0)
		return 0;
	int32 curHealth = (int32)GetHealth();
	int32 val = dVal + curHealth;
	if (val <= 0)
	{
		SetHealth(0);
		return -curHealth;
	}
	int32 maxHealth = (int32)GetMaxHealth();
	int32 gain;
	if (val < maxHealth)
	{
		SetHealth(val);
		gain = val - curHealth;
	}
	else
	{
		SetHealth(maxHealth);
		gain = maxHealth - curHealth;
	}
	return gain;
}
void Unit::SetHealthPercent(float percent)
{
	uint32 newHealth = GetMaxHealth() * percent / 100.0f;
	SetHealth(newHealth);
}
float Unit::GetHealthPercent() const
{ 
	return (GetHealth() * 100.0f) / GetMaxHealth();
}
uint32 Unit::GetHealth()    const
{
	return GetUInt32Value(UNIT_FIELD_HEALTH);
}
uint32 Unit::GetMaxHealth() const
{
	return GetUInt32Value(UNIT_FIELD_MAXHEALTH);
}
int32 Unit::ModifyPower(Powers power, int32 dVal)
{
	if (dVal == 0)
		return 0;
	int32 curPower = (int32)GetPower(power);
	int32 val = dVal + curPower;
	if (val <= 0)
	{
		SetPower(power, 0);
		return -curPower;
	}
	int32 maxPower = (int32)GetMaxPower(power);
	int32 gain;
	if (val < maxPower)
	{
		SetPower(power, val);
		gain = val - curPower;
	}
	else
	{
		SetPower(power, maxPower);
		gain = maxPower - curPower;
	}
	return gain;
}
void Unit::ApplyPowerMod(Powers power, uint32 val, bool apply)
{
	
}
void Unit::ApplyMaxPowerMod(Powers power, uint32 val, bool apply)
{

}
void Unit::CreateObject()
{
}
void Unit::UpdateObject()
{
	ClearUpdateMask(false);
}
void Unit::DeleteObject()
{
}
void Unit::UpdateSpeed(UnitMoveType mtype, bool forced, float ratio)
{
}
float Unit::GetSpeed(UnitMoveType mtype) const
{
	return 0;
}
void Unit::SetSpeedRate(UnitMoveType mtype, float rate, bool forced)
{
}
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

#include "Object.h"
#include "../Server/SharedDefine.h"
#include "WorldPacket.h"
#include "../Server/Opcodes.h"
#include "Log.h"
#include "../World/World.h"
#include "Player.h"
#include "ObjectGuid.h"
#include "UpdateData.h"
#include "UpdateMask.h"
#include "Util.h"

Object::Object() : m_updateFlag(0)
{
	m_objectTypeId = TYPEID_OBJECT;
	m_objectType = TYPEMASK_OBJECT;

	m_uint32Values = nullptr;
	m_valuesCount = 0;

	m_inWorld = false;
	m_objectUpdated = false;
}
void Object::ClearUpdateMask(bool remove)
{
	if (m_uint32Values)
	{
		for (uint16 index = 0; index < m_valuesCount; ++index)
			m_changedValues[index] = false;
	}

	if (m_objectUpdated)
	{
		if (remove)
			RemoveFromClientUpdateList();
		m_objectUpdated = false;
	}
}
void Object::RemoveFromClientUpdateList()
{
	sLog.outError("Unexpected call of Object::RemoveFromClientUpdateList for object (TypeId: %u Update fields: %u)", GetTypeId(), m_valuesCount);
	ORIGIN_ASSERT(false);
}
Object::~Object()
{
	if (IsInWorld())
	{
		///- Do NOT call RemoveFromWorld here, if the object is a player it will crash
		sLog.outError("Object::~Object (GUID: %u TypeId: %u) deleted but still in world!!", GetGUIDLow(), GetTypeId());
		ORIGIN_ASSERT(false);
	}

	if (m_objectUpdated)
	{
		sLog.outError("Object::~Object (GUID: %u TypeId: %u) deleted but still have updated status!!", GetGUIDLow(), GetTypeId());
		ORIGIN_ASSERT(false);
	}

	delete[] m_uint32Values;
}
void Object::_InitValues()
{
	m_uint32Values = new uint32[m_valuesCount];
	memset(m_uint32Values, 0, m_valuesCount * sizeof(uint32));

	m_changedValues.resize(m_valuesCount, false);

	m_objectUpdated = false;
}

void Object::_Create(uint32 guidlow, uint32 entry, HighGuid guidhigh)
{
	if (!m_uint32Values)
		_InitValues();

	ObjectGuid _guid = ObjectGuid(guidhigh, entry, guidlow);
	SetGuidValue(OBJECT_FIELD_GUID, _guid);
	SetUInt32Value(OBJECT_FIELD_TYPE, m_objectType);
	m_PackGUID.Set(_guid);
}

void Object::SetObjectScale(float newScale)
{
	SetFloatValue(OBJECT_FIELD_SCALE_X, newScale);
}

bool Object::LoadValues(const char* data)
{
	if (!m_uint32Values) _InitValues();

	Tokens tokens = StrSplit(data, " ");

	if (tokens.size() != m_valuesCount)
		return false;

	Tokens::iterator iter;
	int index;
	for (iter = tokens.begin(), index = 0; index < m_valuesCount; ++iter, ++index)
	{
		m_uint32Values[index] = std::stoul((*iter).c_str());
	}

	return true;
}
bool Object::PrintIndexError(uint32 index, bool set) const
{
	sLog.outError("Attempt %s nonexistent value field: %u (count: %u) for object typeid: %u type mask: %u", (set ? "set value to" : "get value from"), index, m_valuesCount, GetTypeId(), m_objectType);

	// ASSERT must fail after function call
	return false;
}

bool Object::PrintEntryError(char const* descr) const
{
	sLog.outError("Object Type %u, Entry %u (lowguid %u) with invalid call for %s", GetTypeId(), GetEntry(), GetObjectGuid().GetCounter(), descr);

	// always false for continue assert fail
	return false;
}
WorldObject::WorldObject() :
	m_mapId(0), m_InstanceId(0),
	m_isActiveObject(false)
{
}

void WorldObject::CleanupsBeforeDelete()
{
	RemoveFromWorld();
}

void WorldObject::_Create(uint32 guidlow, HighGuid guidhigh)
{
	Object::_Create(guidlow, 0, guidhigh);
}

// slow
float WorldObject::GetDistance(const WorldObject* obj) const
{
	float dx = GetPositionX() - obj->GetPositionX();
	float dy = GetPositionY() - obj->GetPositionY();
	float dz = GetPositionZ() - obj->GetPositionZ();
	float sizefactor = GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();
	float dist = sqrt((dx * dx) + (dy * dy) + (dz * dz)) - sizefactor;
	return (dist > 0 ? dist : 0);
}

float WorldObject::GetDistance2d(float x, float y) const
{
	float dx = GetPositionX() - x;
	float dy = GetPositionY() - y;
	float sizefactor = GetObjectBoundingRadius();
	float dist = sqrt((dx * dx) + (dy * dy)) - sizefactor;
	return (dist > 0 ? dist : 0);
}

float WorldObject::GetDistance(float x, float y, float z) const
{
	float dx = GetPositionX() - x;
	float dy = GetPositionY() - y;
	float dz = GetPositionZ() - z;
	float sizefactor = GetObjectBoundingRadius();
	float dist = sqrt((dx * dx) + (dy * dy) + (dz * dz)) - sizefactor;
	return (dist > 0 ? dist : 0);
}

float WorldObject::GetDistance2d(const WorldObject* obj) const
{
	float dx = GetPositionX() - obj->GetPositionX();
	float dy = GetPositionY() - obj->GetPositionY();
	float sizefactor = GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();
	float dist = sqrt((dx * dx) + (dy * dy)) - sizefactor;
	return (dist > 0 ? dist : 0);
}

float WorldObject::GetDistanceZ(const WorldObject* obj) const
{
	float dz = fabs(GetPositionZ() - obj->GetPositionZ());
	float sizefactor = GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();
	float dist = dz - sizefactor;
	return (dist > 0 ? dist : 0);
}

bool WorldObject::IsWithinDist3d(float x, float y, float z, float dist2compare) const
{
	float dx = GetPositionX() - x;
	float dy = GetPositionY() - y;
	float dz = GetPositionZ() - z;
	float distsq = dx * dx + dy * dy + dz * dz;

	float sizefactor = GetObjectBoundingRadius();
	float maxdist = dist2compare + sizefactor;

	return distsq < maxdist * maxdist;
}

bool WorldObject::IsWithinDist2d(float x, float y, float dist2compare) const
{
	float dx = GetPositionX() - x;
	float dy = GetPositionY() - y;
	float distsq = dx * dx + dy * dy;

	float sizefactor = GetObjectBoundingRadius();
	float maxdist = dist2compare + sizefactor;

	return distsq < maxdist * maxdist;
}

bool WorldObject::_IsWithinDist(WorldObject const* obj, float dist2compare, bool is3D) const
{
	float dx = GetPositionX() - obj->GetPositionX();
	float dy = GetPositionY() - obj->GetPositionY();
	float distsq = dx * dx + dy * dy;
	if (is3D)
	{
		float dz = GetPositionZ() - obj->GetPositionZ();
		distsq += dz * dz;
	}
	float sizefactor = GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();
	float maxdist = dist2compare + sizefactor;

	return distsq < maxdist * maxdist;
}

bool WorldObject::GetDistanceOrder(WorldObject const* obj1, WorldObject const* obj2, bool is3D /* = true */) const
{
	float dx1 = GetPositionX() - obj1->GetPositionX();
	float dy1 = GetPositionY() - obj1->GetPositionY();
	float distsq1 = dx1 * dx1 + dy1 * dy1;
	if (is3D)
	{
		float dz1 = GetPositionZ() - obj1->GetPositionZ();
		distsq1 += dz1 * dz1;
	}

	float dx2 = GetPositionX() - obj2->GetPositionX();
	float dy2 = GetPositionY() - obj2->GetPositionY();
	float distsq2 = dx2 * dx2 + dy2 * dy2;
	if (is3D)
	{
		float dz2 = GetPositionZ() - obj2->GetPositionZ();
		distsq2 += dz2 * dz2;
	}

	return distsq1 < distsq2;
}

bool WorldObject::IsInRange(WorldObject const* obj, float minRange, float maxRange, bool is3D /* = true */) const
{
	float dx = GetPositionX() - obj->GetPositionX();
	float dy = GetPositionY() - obj->GetPositionY();
	float distsq = dx * dx + dy * dy;
	if (is3D)
	{
		float dz = GetPositionZ() - obj->GetPositionZ();
		distsq += dz * dz;
	}

	float sizefactor = GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();

	// check only for real range
	if (minRange > 0.0f)
	{
		float mindist = minRange + sizefactor;
		if (distsq < mindist * mindist)
			return false;
	}

	float maxdist = maxRange + sizefactor;
	return distsq < maxdist * maxdist;
}

bool WorldObject::IsInRange2d(float x, float y, float minRange, float maxRange) const
{
	float dx = GetPositionX() - x;
	float dy = GetPositionY() - y;
	float distsq = dx * dx + dy * dy;

	float sizefactor = GetObjectBoundingRadius();

	// check only for real range
	if (minRange > 0.0f)
	{
		float mindist = minRange + sizefactor;
		if (distsq < mindist * mindist)
			return false;
	}

	float maxdist = maxRange + sizefactor;
	return distsq < maxdist * maxdist;
}

bool WorldObject::IsInRange3d(float x, float y, float z, float minRange, float maxRange) const
{
	float dx = GetPositionX() - x;
	float dy = GetPositionY() - y;
	float dz = GetPositionZ() - z;
	float distsq = dx * dx + dy * dy + dz * dz;

	float sizefactor = GetObjectBoundingRadius();

	// check only for real range
	if (minRange > 0.0f)
	{
		float mindist = minRange + sizefactor;
		if (distsq < mindist * mindist)
			return false;
	}

	float maxdist = maxRange + sizefactor;
	return distsq < maxdist * maxdist;
}

float WorldObject::GetAngle(const WorldObject* obj) const
{
	if (!obj)
		return 0.0f;

	// Rework the assert, when more cases where such a call can happen have been fixed
	// ORIGIN_ASSERT(obj != this || PrintEntryError("GetAngle (for self)"));
	if (obj == this)
	{
		sLog.outError("INVALID CALL for GetAngle for %s", obj->GetGuidStr().c_str());
		return 0.0f;
	}
	return GetAngle(obj->GetPositionX(), obj->GetPositionY());
}

// Return angle in range 0..2*pi
float WorldObject::GetAngle(const float x, const float y) const
{
	float dx = x - GetPositionX();
	float dy = y - GetPositionY();

	float ang = atan2(dy, dx);                              // returns value between -Pi..Pi
	ang = (ang >= 0) ? ang : 2 * M_PI_F + ang;
	return ang;
}

bool WorldObject::PrintCoordinatesError(float x, float y, float z, char const* descr) const
{
	sLog.outError("%s with invalid %s coordinates: mapid = %uu, x = %f, y = %f, z = %f", GetGuidStr().c_str(), descr, GetMapId(), x, y, z);
	return false;                                           // always false for continue assert fail
}
void WorldObject::SetMap(Map* map)
{
	ORIGIN_ASSERT(map);
	m_currMap = map;
	// lets save current map's Id/instanceId
	m_mapId = map->GetId();
	//m_InstanceId = map->GetInstanceId();
}
void WorldObject::Relocate(float x, float y, float z, float orientation)
{
	m_position.x = x;
	m_position.y = y;
	m_position.z = z;
	m_position.o = orientation;

	/*if (isType(TYPEMASK_UNIT))
	((Unit*)this)->m_movementInfo.ChangePosition(x, y, z, orientation);*/
}
void WorldObject::Relocate(float x, float y, float z)
{
	m_position.x = x;
	m_position.y = y;
	m_position.z = z;

	/*if (isType(TYPEMASK_UNIT))
	((Unit*)this)->m_movementInfo.ChangePosition(x, y, z, GetOrientation());*/
}

void Object::SetInt32Value(uint16 index, int32 value)
{
	ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

	if (m_int32Values[index] != value)
	{
		m_int32Values[index] = value;
		m_changedValues[index] = true;
		MarkForClientUpdate();
	}
}

void Object::SetUInt32Value(uint16 index, uint32 value)
{
	ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

	if (m_uint32Values[index] != value)
	{
		m_uint32Values[index] = value;
		m_changedValues[index] = true;
		MarkForClientUpdate();
	}
}

void Object::SetUInt64Value(uint16 index, uint64 value)
{
	ORIGIN_ASSERT(index + 1 < m_valuesCount || PrintIndexError(index, true));
	if (*((uint64*) & (m_uint32Values[index])) != value)
	{
		m_uint32Values[index] = *((uint32*)&value);
		m_uint32Values[index + 1] = *(((uint32*)&value) + 1);
		m_changedValues[index] = true;
		m_changedValues[index + 1] = true;
		MarkForClientUpdate();
	}
}

void Object::SetFloatValue(uint16 index, float value)
{
	ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

	if (m_floatValues[index] != value)
	{
		m_floatValues[index] = value;
		m_changedValues[index] = true;
		MarkForClientUpdate();
	}
}

void Object::SetByteValue(uint16 index, uint8 offset, uint8 value)
{
	ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

	if (offset > 4)
	{
		sLog.outError("Object::SetByteValue: wrong offset %u", offset);
		return;
	}

	if (uint8(m_uint32Values[index] >> (offset * 8)) != value)
	{
		m_uint32Values[index] &= ~uint32(uint32(0xFF) << (offset * 8));
		m_uint32Values[index] |= uint32(uint32(value) << (offset * 8));
		m_changedValues[index] = true;
		MarkForClientUpdate();
	}
}

void Object::SetUInt16Value(uint16 index, uint8 offset, uint16 value)
{
	ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

	if (offset > 2)
	{
		sLog.outError("Object::SetUInt16Value: wrong offset %u", offset);
		return;
	}

	if (uint16(m_uint32Values[index] >> (offset * 16)) != value)
	{
		m_uint32Values[index] &= ~uint32(uint32(0xFFFF) << (offset * 16));
		m_uint32Values[index] |= uint32(uint32(value) << (offset * 16));
		m_changedValues[index] = true;
		MarkForClientUpdate();
	}
}

void Object::SetStatFloatValue(uint16 index, float value)
{
	if (value < 0)
		value = 0.0f;

	SetFloatValue(index, value);
}

void Object::SetStatInt32Value(uint16 index, int32 value)
{
	if (value < 0)
		value = 0;

	SetUInt32Value(index, uint32(value));
}
void Object::SetFlag(uint16 index, uint32 newFlag)
{
	ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, true));
	uint32 oldval = m_uint32Values[index];
	uint32 newval = oldval | newFlag;

	if (oldval != newval)
	{
		m_uint32Values[index] = newval;
		m_changedValues[index] = true;
		MarkForClientUpdate();
	}
}

void Object::RemoveFlag(uint16 index, uint32 oldFlag)
{
	ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, true));
	uint32 oldval = m_uint32Values[index];
	uint32 newval = oldval & ~oldFlag;

	if (oldval != newval)
	{
		m_uint32Values[index] = newval;
		m_changedValues[index] = true;
		MarkForClientUpdate();
	}
}
void Object::MarkForClientUpdate()
{
	if (m_inWorld)
	{
		if (!m_objectUpdated)
		{
			AddToClientUpdateList();
			m_objectUpdated = true;
		}
	}
}
void Object::AddToClientUpdateList()
{
	sLog.outError("Unexpected call of Object::AddToClientUpdateList for object (TypeId: %u Update fields: %u)", GetTypeId(), m_valuesCount);
	ORIGIN_ASSERT(false);
}
void WorldObject::AddToClientUpdateList()
{
	GetMap()->AddUpdateObject(this);
}
void WorldObject::RemoveFromClientUpdateList()
{
	GetMap()->RemoveUpdateObject(this);
}
void Object::DestroyForPlayer(Player* target) const
{
	ORIGIN_ASSERT(target);

	WorldPacket data(SMSG_DESTROY_OBJECT, 8);
	data << GetObjectGuid();
	target->GetSession()->SendPacket(&data);
}
void Object::CreateObject()
{
	sLog.outError("Unexpected call of Object::CreateObject for object (TypeId: %u Update fields: %u)", GetTypeId(), m_valuesCount);
	ORIGIN_ASSERT(false);
}
void Object::UpdateObject()
{
	sLog.outError("Unexpected call of Object::UpdateObject for object (TypeId: %u Update fields: %u)", GetTypeId(), m_valuesCount);
	ORIGIN_ASSERT(false);
}
void Object::DeleteObject()
{
	sLog.outError("Unexpected call of Object::DeleteObject for object (TypeId: %u Update fields: %u)", GetTypeId(), m_valuesCount);
	ORIGIN_ASSERT(false);
}
void Object::BuildCreateUpdateBlockForPlayer(Player* target) const
{
	if (!target)
		return;

	uint8  updatetype = UPDATETYPE_CREATE_OBJECT;
	uint8 updateFlags = m_updateFlag;

	/** lower flag1 **/
	if (target == this)
	{
		updateFlags = UPDATEFLAG_SELF;
	}

	if (m_itsNewObject)
	{
		switch (GetObjectGuid().GetHigh())
		{
		case HighGuid::HIGHGUID_DYNAMICOBJECT:
		case HighGuid::HIGHGUID_CORPSE:
		case HighGuid::HIGHGUID_PLAYER:
		case HighGuid::HIGHGUID_UNIT:
		case HighGuid::HIGHGUID_GAMEOBJECT:
			//updatetype = UPDATETYPE_CREATE_OBJECT2;
			//break;

		default:
			break;
		}
	}
	WorldPacket pPacket(SMSG_CREATE_OBJECT, 500);
	pPacket << updatetype;
	pPacket << updateFlags;
	pPacket << m_objectTypeId;
	pPacket << GetName();
	if (updateFlags != UPDATEFLAG_SELF) /// we don"t need to send our own location as we already did
	{
		pPacket << ((WorldObject*)this)->GetPositionX();
		pPacket << ((WorldObject*)this)->GetPositionY();
		pPacket << ((WorldObject*)this)->GetPositionZ();
		pPacket << ((WorldObject*)this)->GetOrientation();
	}

	CreateUpdateCountAndSize(&pPacket, updateFlags); // fill buffer and resize it
	
	target->GetSession()->SendPacket(&pPacket);
}
void Object::CreateUpdateCountAndSize(WorldPacket *buf, uint8 updateFlag) const
{
	uint32 oldSize = buf->size();
	uint32 newsize = 0;
	
	for (uint16 index = 0; index < GetValuesCount(); ++index)
	{
		if (GetUInt32Value(index) != 0)
		{
			newsize += 6;
			*buf << index;
			*buf << GetUInt32Value(index);
		}
	}
	buf->resize(newsize + oldSize);
}
const char* Object::GetName() const
{
	sLog.outError("Object::GetName shoudn't be called here !!");
	ORIGIN_ASSERT(false);
	return "";
}
const char*	WorldObject::GetName() const
{
	return m_name.c_str();
}
void WorldObject::CreateObject()
{
}
void WorldObject::UpdateObject()
{
	ClearUpdateMask(false);
}
void WorldObject::DeleteObject()
{
}

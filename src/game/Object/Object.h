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

#ifndef _OBJECT_H
#define _OBJECT_H

#include "Common.h"
#include "ByteBuffer.h"
#include "UpdateFields.h"
#include "UpdateData.h"
#include "ObjectGuid.h"
#include <Timer.h>

#include <set>

#define CONTACT_DISTANCE            0.5f
#define INTERACTION_DISTANCE        5.0f
#define ATTACK_DISTANCE             5.0f
#define INSPECT_DISTANCE            11.11f
#define TRADE_DISTANCE              11.11f
#define MAX_VISIBILITY_DISTANCE     333.0f      // max distance for visible object show, limited in 333 yards
#define DEFAULT_VISIBILITY_DISTANCE 90.0f       // default visible distance, 90 yards on continents
#define DEFAULT_VISIBILITY_INSTANCE 120.0f      // default visible distance in instances, 120 yards
#define DEFAULT_VISIBILITY_BG       180.0f      // default visible distance in BG, 180 yards

#define DEFAULT_WORLD_OBJECT_SIZE   1      // currently used (correctly?) for any non Unit world objects. This is actually the bounding_radius, like player/creature from creature_model_data
#define DEFAULT_OBJECT_SCALE        1.0f                    // non-Tauren player/item scale as default, npc/go from database, pets from dbc
#define DEFAULT_TAUREN_MALE_SCALE   1.35f                   // Tauren male player scale by default
#define DEFAULT_TAUREN_FEMALE_SCALE 1.25f                   // Tauren female player scale by default

#define MAX_STEALTH_DETECT_RANGE    45.0f

enum TempSummonType
{
	TEMPSUMMON_MANUAL_DESPAWN = 0,             // despawns when UnSummon() is called
	TEMPSUMMON_DEAD_DESPAWN = 1,             // despawns when the creature disappears
	TEMPSUMMON_CORPSE_DESPAWN = 2,             // despawns instantly after death
	TEMPSUMMON_CORPSE_TIMED_DESPAWN = 3,             // despawns after a specified time after death (or when the creature disappears)
	TEMPSUMMON_TIMED_DESPAWN = 4,             // despawns after a specified time
	TEMPSUMMON_TIMED_OOC_DESPAWN = 5,             // despawns after a specified time after the creature is out of combat
	TEMPSUMMON_TIMED_OR_DEAD_DESPAWN = 6,             // despawns after a specified time OR when the creature disappears
	TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN = 7,             // despawns after a specified time OR when the creature dies
	TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN = 8,             // despawns after a specified time (OOC) OR when the creature disappears
	TEMPSUMMON_TIMED_OOC_OR_CORPSE_DESPAWN = 9,             // despawns after a specified time (OOC) OR when the creature dies
};

enum TempSummonLinkedAura
{
	TEMPSUMMON_LINKED_AURA_OWNER_CHECK = 0x00000001,
	TEMPSUMMON_LINKED_AURA_REMOVE_OWNER = 0x00000002
};

class Player;
class Unit;
class Map;
class WorldPacket;
class UpdateData;
class WorldSession;

typedef std::unordered_map<Player*, UpdateData> UpdateDataMapType;

struct Position
{
	Position() : x(0.0f), y(0.0f), z(0.0f), o(0.0f) {}
	float x, y, z, o;
};

struct WorldLocation
{
	uint32 mapid;
	float coord_x;
	float coord_y;
	float coord_z;
	float orientation;
	explicit WorldLocation(uint32 _mapid = 0, float _x = 0, float _y = 0, float _z = 0, float _o = 0)
		: mapid(_mapid), coord_x(_x), coord_y(_y), coord_z(_z), orientation(_o) {}
	WorldLocation(WorldLocation const& loc)
		: mapid(loc.mapid), coord_x(loc.coord_x), coord_y(loc.coord_y), coord_z(loc.coord_z), orientation(loc.orientation) {}
};


// use this class to measure time between world update ticks
// essential for units updating their spells after cells become active
class WorldUpdateCounter
{
public:
	WorldUpdateCounter() : m_tmStart(0) {}

	time_t timeElapsed()
	{
		if (!m_tmStart)
			m_tmStart = WorldTimer::tickPrevTime();

		return WorldTimer::getMSTimeDiff(m_tmStart, WorldTimer::tickTime());
	}

	void Reset() { m_tmStart = WorldTimer::tickTime(); }

private:
	uint32 m_tmStart;
};

class Object
{
public:
	virtual ~Object();

	const bool& IsInWorld() const { return m_inWorld; }
	virtual void AddToWorld()
	{
		if (m_inWorld)
			return;

		m_inWorld = true;
		ClearUpdateMask(false);
	}
	virtual void RemoveFromWorld()
	{
		// if we remove from world then sending changes not required
		ClearUpdateMask(true);
		m_inWorld = false;
	}
	void ClearUpdateMask(bool remove);
	ObjectGuid const& GetObjectGuid() const { return GetGuidValue(OBJECT_FIELD_GUID); }
	const uint64& GetGUID() const { return GetUInt64Value(OBJECT_FIELD_GUID); } // DEPRECATED, not use, will removed soon
	uint32 GetGUIDLow() const { return GetObjectGuid().GetCounter(); }
	PackedGuid const& GetPackGUID() const { return m_PackGUID; }
	std::string GetGuidStr() const { return GetObjectGuid().GetString(); }

	uint32 GetEntry() const { return entry; }
	void SetEntry(uint32 _entry) { entry = _entry; }

	float GetObjectScale() const
	{
		return m_floatValues[OBJECT_FIELD_SCALE_X] ? m_floatValues[OBJECT_FIELD_SCALE_X] : DEFAULT_OBJECT_SCALE;
	}

	void SetObjectScale(float newScale);

	uint8 GetTypeId() const { return m_objectTypeId; }
	bool isType(TypeMask mask) const { return !!(mask & m_objectType); }

	const int32& GetInt32Value(uint16 index) const
	{
		ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, false));
		return m_int32Values[index];
	}
	const uint32& GetUInt32Value(uint16 index) const
	{
		ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, false));
		return m_uint32Values[index];
	}
	const uint64& GetUInt64Value(uint16 index) const
	{
		ORIGIN_ASSERT(index + 1 < m_valuesCount || PrintIndexError(index, false));
		return *((uint64*) & (m_uint32Values[index]));
	}
	const float& GetFloatValue(uint16 index) const
	{
		ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, false));
		return m_floatValues[index];
	}
	uint8 GetByteValue(uint16 index, uint8 offset) const
	{
		ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, false));
		ORIGIN_ASSERT(offset < 4);
		return *(((uint8*)&m_uint32Values[index]) + offset);
	}
	uint16 GetUInt16Value(uint16 index, uint8 offset) const
	{
		ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, false));
		ORIGIN_ASSERT(offset < 2);
		return *(((uint16*)&m_uint32Values[index]) + offset);
	}

	void SetFlag(uint16 index, uint32 newFlag);
	void RemoveFlag(uint16 index, uint32 oldFlag);
	void ToggleFlag(uint16 index, uint32 flag)
	{
		if (HasFlag(index, flag))
			RemoveFlag(index, flag);
		else
			SetFlag(index, flag);
	}
	bool HasFlag(uint16 index, uint32 flag) const
	{
		ORIGIN_ASSERT(index < m_valuesCount || PrintIndexError(index, false));
		return (m_uint32Values[index] & flag) != 0;
	}
	ObjectGuid const& GetGuidValue(uint16 index) const { return *reinterpret_cast<ObjectGuid const*>(&GetUInt64Value(index)); }
	bool LoadValues(const char* data);
	uint16 GetValuesCount() const { return m_valuesCount; }
	virtual bool HasQuest(uint32 /* quest_id */) const { return false; }
	virtual bool HasInvolvedQuest(uint32 /* quest_id */) const { return false; }
	void SetItsNewObject(bool enable) { m_itsNewObject = enable; }
	void SetGuidValue(uint16 index, ObjectGuid const& value) { SetUInt64Value(index, value.GetRawValue()); }
	virtual const char* GetName() const;
protected:
	Object();

	void _InitValues();
	void _Create(uint32 guidlow, uint32 entry, HighGuid guidhigh);

	uint16 m_objectType;

	uint8 m_objectTypeId;
	uint8 m_updateFlag;

	union
	{
		int32*  m_int32Values;
		uint32* m_uint32Values;
		float*  m_floatValues;
	};

	std::vector<bool> m_changedValues;

	uint16 m_valuesCount;

	bool m_objectUpdated;
public:
	virtual void CreateObject();
	virtual void UpdateObject();
	virtual void DeleteObject();
private:
	bool m_inWorld;
	bool m_itsNewObject;

	PackedGuid m_PackGUID;

	Object(const Object&);                              // prevent generation copy constructor
	Object& operator=(Object const&);                   // prevent generation assigment operator

	uint32		entry;
public:
	// for output helpfull error messages from ASSERTs
	bool PrintIndexError(uint32 index, bool set) const;
	bool PrintEntryError(char const* descr) const;
protected:
	virtual void AddToClientUpdateList();
	virtual void RemoveFromClientUpdateList();
	virtual void BuildCreateUpdateBlockForPlayer(Player* target) const;
	void CreateUpdateCountAndSize(WorldPacket *buf, uint8 updateFlag) const;
	void SetUInt32Value(uint16 index, uint32 value);
	void SetUInt64Value(uint16 index, uint64 value);
	void SetFloatValue(uint16 index, float value);
	void SetInt32Value(uint16 index, int32 value);
	void SetByteValue(uint16 index, uint8 offset, uint8 value);
	void SetUInt16Value(uint16 index, uint8 offset, uint16 value);
	void SetStatFloatValue(uint16 index, float value);
	void MarkForClientUpdate();
	void SetStatInt32Value(uint16 index, int32 value);
	virtual void DestroyForPlayer(Player* target) const;
};

struct WorldObjectChangeAccumulator;

class WorldObject : public Object
{
	friend struct WorldObjectChangeAccumulator;

public:

	// class is used to manipulate with WorldUpdateCounter
	// it is needed in order to get time diff between two object's Update() calls
	class UpdateHelper
	{
	public:
		explicit UpdateHelper(WorldObject* obj) : m_obj(obj) {}
		~UpdateHelper() { }

		void Update(uint32 time_diff)
		{
			m_obj->Update(m_obj->m_updateTracker.timeElapsed(), time_diff);
			m_obj->m_updateTracker.Reset();
		}

	private:
		UpdateHelper(const UpdateHelper&);
		UpdateHelper& operator=(const UpdateHelper&);

		WorldObject* const m_obj;
	};

	virtual ~WorldObject() {}

	virtual void Update(uint32 /*update_diff*/, uint32 /*time_diff*/) {}

	void _Create(uint32 guidlow, HighGuid guidhigh);

	void Relocate(float x, float y, float z, float orientation);
	void Relocate(float x, float y, float z);
	void SetOrientation(float orientation);
	float GetPositionX() const { return m_position.x; }
	float GetPositionY() const { return m_position.y; }
	float GetPositionZ() const { return m_position.z; }
	void GetPosition(float& x, float& y, float& z) const
	{
		x = m_position.x; y = m_position.y; z = m_position.z;
	}
	void GetPosition(WorldLocation& loc) const
	{
		loc.mapid = m_mapId; GetPosition(loc.coord_x, loc.coord_y, loc.coord_z); loc.orientation = GetOrientation();
	}
	float GetOrientation() const { return m_position.o; }

	virtual float GetObjectBoundingRadius() const { return DEFAULT_WORLD_OBJECT_SIZE; }

	void SetMap(Map* map);
	Map* GetMap() const { ORIGIN_ASSERT(m_currMap); return m_currMap; }
	// used to check all object's GetMap() calls when object is not in world!
	void ResetMap() { m_currMap = nullptr; }
	uint32 GetMapId() const { return m_mapId; }
	uint32 GetInstanceId() const { return m_InstanceId; }

	const char* GetName() const;
	void SetName(const std::string& newname) { m_name = newname; }

	virtual const char* GetNameForLocaleIdx(int32 /*locale_idx*/) const { return GetName(); }

	float GetDistance(const WorldObject* obj) const;
	float GetDistance(float x, float y, float z) const;
	float GetDistance2d(const WorldObject* obj) const;
	float GetDistance2d(float x, float y) const;
	float GetDistanceZ(const WorldObject* obj) const;
	
	bool IsWithinDist3d(float x, float y, float z, float dist2compare) const;
	bool IsWithinDist2d(float x, float y, float dist2compare) const;
	bool _IsWithinDist(WorldObject const* obj, float dist2compare, bool is3D) const;

	// use only if you will sure about placing both object at same map
	bool IsWithinDist(WorldObject const* obj, float dist2compare, bool is3D = true) const
	{
		return obj && _IsWithinDist(obj, dist2compare, is3D);
	}

	bool GetDistanceOrder(WorldObject const* obj1, WorldObject const* obj2, bool is3D = true) const;
	bool IsInRange(WorldObject const* obj, float minRange, float maxRange, bool is3D = true) const;
	bool IsInRange2d(float x, float y, float minRange, float maxRange) const;
	bool IsInRange3d(float x, float y, float z, float minRange, float maxRange) const;

	float GetAngle(const WorldObject* obj) const;
	float GetAngle(const float x, const float y) const;

	virtual void CleanupsBeforeDelete();                // used in destructor or explicitly before mass creature delete to remove cross-references to already deleted units

	virtual void SaveRespawnTime() {}

	bool isActiveObject() const { return m_isActiveObject; }
	void SetActiveObjectState(bool active);
	// ASSERT print helper
	bool PrintCoordinatesError(float x, float y, float z, char const* descr) const;
protected:
	explicit WorldObject();

	// these functions are used mostly for Relocate() and Corpse/Player specific stuff...
	// use them ONLY in LoadFromDB()/Create() funcs and nowhere else!
	// mapId/instanceId should be set in SetMap() function!
	void SetLocationMapId(uint32 _mapId) { m_mapId = _mapId; }
	void SetLocationInstanceId(uint32 _instanceId) { m_InstanceId = _instanceId; }

	void AddToClientUpdateList() override;
	void RemoveFromClientUpdateList() override;

	std::string m_name;

	void CreateObject() override;
	void UpdateObject() override;
	void DeleteObject() override;
private:
	Map* m_currMap;                                     // current object's Map location
	uint32 m_mapId;                                     // object at map with map_id
	uint32 m_InstanceId;                                // in map copy with instance id

	Position m_position;
	WorldUpdateCounter m_updateTracker;
	bool m_isActiveObject;
};

#endif

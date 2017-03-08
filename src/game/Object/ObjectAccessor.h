#ifndef ORIGIN_OBJECTACCESSOR_H
#define ORIGIN_OBJECTACCESSOR_H

#include "Common.h"
#include "../Define.h"
#include "Config/Singleton.h"
#include "Config/ThreadingModel.h"

#include "UpdateData.h"

#include "Object.h"
#include "Player.h"

#include <mutex>

class Unit;
class WorldObject;
class Map;

template <class T>
class HashMapHolder
{
public:

	typedef std::unordered_map<ObjectGuid, T*>   MapType;
	typedef std::mutex LockType;
	typedef std::lock_guard<std::mutex> ReadGuard;
	typedef std::lock_guard<std::mutex> WriteGuard;

	static void Insert(T* o)
	{
		WriteGuard guard(i_lock);
		m_objectMap[o->GetObjectGuid()] = o;
	}

	static void Remove(T* o)
	{
		WriteGuard guard(i_lock);
		m_objectMap.erase(o->GetObjectGuid());
	}

	static T* Find(ObjectGuid guid)
	{
		ReadGuard guard(i_lock);
		typename MapType::iterator itr = m_objectMap.find(guid);
		return (itr != m_objectMap.end()) ? itr->second : nullptr;
	}

	static MapType& GetContainer() { return m_objectMap; }

	static LockType& GetLock() { return i_lock; }

private:

	// Non instanceable only static
	HashMapHolder() {}

	static LockType i_lock;
	static MapType  m_objectMap;
};

class ObjectAccessor : public Origin::Singleton<ObjectAccessor, Origin::ClassLevelLockable<ObjectAccessor, std::mutex> >
{
	friend class Origin::OperatorNew<ObjectAccessor>;

	ObjectAccessor();
	~ObjectAccessor();
	ObjectAccessor(const ObjectAccessor&);
	ObjectAccessor& operator=(const ObjectAccessor&);

public:
	// Search player at any map in world and other objects at same map with `obj`
	// Note: recommended use Map::GetUnit version if player also expected at same map only
	static Unit* GetUnit(WorldObject const& obj, ObjectGuid guid);

	// Player access
	static Player* FindPlayer(ObjectGuid guid, bool inWorld = true);// if need player at specific map better use Map::GetPlayer
	static Player* FindPlayerByName(const char* name);
	static void KickPlayer(ObjectGuid guid);

	HashMapHolder<Player>::MapType& GetPlayers()
	{
		return HashMapHolder<Player>::GetContainer();
	}

	void SaveAllPlayers();

	// For call from Player/Corpse AddToWorld/RemoveFromWorld only
	void AddObject(Player* object) { HashMapHolder<Player>::Insert(object); }
	void RemoveObject(Player* object) { HashMapHolder<Player>::Remove(object); }

private:
	typedef std::mutex LockType;
	typedef Origin::GeneralLock<LockType > Guard;

	LockType i_playerGuard;
	LockType i_corpseGuard;
};

#define sObjectAccessor ObjectAccessor::Instance()

#endif

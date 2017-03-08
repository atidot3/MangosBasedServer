#ifndef ORIGIN_MAP_H
#define ORIGIN_MAP_H

#include "Common.h"
#include "../Define.h"
#include <Config/ThreadingModel.h>
#include "../Object/Object.h"
#include "../Server/SharedDefine.h"
#include "MapRefManager.h"
#include "../Server/DBStorage/DBStorageStructure.h"
#include "../Util/Timer.h"

#include <bitset>

class Unit;
class WorldPacket;

class Map
{
	friend class MapReference;
protected:
	Map(uint32 id, time_t, uint32 InstanceId);

public:
	virtual ~Map();

	bool CanUnload(uint32 diff)
	{
		if (!m_unloadTimer) return false;
		if (m_unloadTimer <= diff) return true;
		m_unloadTimer -= diff;
		return false;

	}
	void Update(const uint32&);
	uint32 GetId(void) const { return i_id; }
	bool Instanceable() const { return i_mapEntry && i_mapEntry->Instanceable(); }
	bool IsDungeon() const { return i_mapEntry && i_mapEntry->IsDungeon(); }
	bool IsRaid() const { return i_mapEntry && i_mapEntry->IsRaid(); }
	bool IsBattleGround() const { return i_mapEntry && i_mapEntry->IsBattleGround(); }
	bool IsContinent() const { return i_mapEntry && i_mapEntry->IsContinent(); }

	virtual void InitVisibilityDistance();
	virtual void RemoveAllObjectsInRemoveList();

	typedef MapRefManager PlayerList;
	PlayerList const& GetPlayers() const { return m_mapRefManager; }
	bool HavePlayers() const { return !m_mapRefManager.isEmpty(); }

	virtual bool Add(Player*);
	virtual void Remove(Player*, bool);
	template<class T> void Add(T*);
	template<class T> void Remove(T*, bool);
	static void DeleteFromWorld(Player* player);        // player object will deleted at call

	Player* GetPlayer(ObjectGuid guid);
	Unit* GetUnit(ObjectGuid guid);                     // only use if sure that need objects at current map, specially for player case

	void	SendInitSelf(Player*);

	void AddUpdateObject(Object* obj)
	{
		i_objectsToClientUpdate.insert(obj);
	}

	void RemoveUpdateObject(Object* obj)
	{
		i_objectsToClientUpdate.erase(obj);
	}
private:
	void SendObjectUpdates();
	std::set<Object*> i_objectsToClientUpdate;
protected:
	MapRefManager m_mapRefManager;
	MapRefManager::iterator m_mapRefIter;
	MapEntry const* i_mapEntry;
	uint32 i_id;
	uint32 i_InstanceId;
	uint32 m_unloadTimer;
	float m_VisibleDistance;
	std::set<WorldObject*> i_objectsToRemove;
};

class WorldMap : public Map
{
private:
	//using Map::GetPersistentState;                      // hide in subclass for overwrite
public:
	WorldMap(uint32 id, time_t expiry = 0) : Map(id, expiry, 0) {}
	~WorldMap() {}

	// can't be nullptr for loaded map
	//WorldPersistentState* GetPersistanceState() const;
};
#endif
#ifndef ORIGIN_MAPMANAGER_H
#define ORIGIN_MAPMANAGER_H

#include "Common.h"
#include "../Define.h"
#include "Config/Singleton.h"
#include "Map.h"

#define MIN_MAP_UPDATE_DELAY    50


class Transport;
class BattleGround;

struct MapID
{
	explicit MapID(uint32 id) : nMapId(id), nInstanceId(0) {}
	MapID(uint32 id, uint32 instid) : nMapId(id), nInstanceId(instid) {}

	bool operator<(const MapID& val) const
	{
		if (nMapId == val.nMapId)
			return nInstanceId < val.nInstanceId;

		return nMapId < val.nMapId;
	}

	bool operator==(const MapID& val) const { return nMapId == val.nMapId && nInstanceId == val.nInstanceId; }

	uint32 nMapId;
	uint32 nInstanceId;
};

class MapManager : public Origin::Singleton<MapManager, Origin::ClassLevelLockable<MapManager, std::recursive_mutex> >
{
	friend class Origin::OperatorNew<MapManager>;

	typedef std::recursive_mutex LOCK_TYPE;
	typedef std::lock_guard<LOCK_TYPE> LOCK_TYPE_GUARD;
	typedef Origin::ClassLevelLockable<MapManager, std::recursive_mutex>::Lock Guard;

public:
	typedef std::map<MapID, Map* > MapMapType;

	Map* CreateMap(uint32, const WorldObject* obj);
	Map* FindMap(uint32 mapid, uint32 instanceId = 0) const;

	// only const version for outer users
	void DeleteInstance(uint32 mapid, uint32 instanceId);

	void Initialize(void);
	void Update(uint32);

	void SetMapUpdateInterval(uint32 t)
	{
		if (t > MIN_MAP_UPDATE_DELAY)
			t = MIN_MAP_UPDATE_DELAY;

		i_timer.SetInterval(t);
		i_timer.Reset();
	}

	void UnloadAll();

	static bool IsValidMAP(uint32 mapid);

	/*static bool IsValidMapCoord(uint32 mapid, float x, float y)
	{
		return IsValidMAP(mapid) && Origin::IsValidMapCoord(x, y);
	}

	static bool IsValidMapCoord(uint32 mapid, float x, float y, float z)
	{
		return IsValidMAP(mapid) && Origin::IsValidMapCoord(x, y, z);
	}

	static bool IsValidMapCoord(uint32 mapid, float x, float y, float z, float o)
	{
		return IsValidMAP(mapid) && Origin::IsValidMapCoord(x, y, z, o);
	}

	static bool IsValidMapCoord(WorldLocation const& loc)
	{
		return IsValidMapCoord(loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z, loc.orientation);
	}*/

	// modulos a radian orientation to the range of 0..2PI
	static float NormalizeOrientation(float o)
	{
		// fmod only supports positive numbers. Thus we have
		// to emulate negative numbers
		if (o < 0)
		{
			float mod = o * -1;
			mod = fmod(mod, 2.0f * M_PI_F);
			mod = -mod + 2.0f * M_PI_F;
			return mod;
		}
		return fmod(o, 2.0f * M_PI_F);
	}

	void RemoveAllObjectsInRemoveList();

	uint32 GenerateInstanceId() { return ++i_MaxInstanceId; }
	void InitMaxInstanceId();
	void InitializeVisibilityDistanceInfo();

	uint32 GetNumInstances();
	uint32 GetNumPlayersInInstances();


	// get list of all maps
	const MapMapType& Maps() const { return i_maps; }

	template<typename Do>
	void DoForAllMapsWithMapId(uint32 mapId, Do& _do);

private:

	MapManager();
	~MapManager();

	MapManager(const MapManager&);
	MapManager& operator=(const MapManager&);

	Map* CreateInstance(uint32 id, Player* player);

	MapMapType i_maps;
	IntervalTimer i_timer;

	uint32 i_MaxInstanceId;
};

template<typename Do>
inline void MapManager::DoForAllMapsWithMapId(uint32 mapId, Do& _do)
{
	MapMapType::const_iterator start = i_maps.lower_bound(MapID(mapId, 0));
	MapMapType::const_iterator end = i_maps.lower_bound(MapID(mapId + 1, 0));
	for (MapMapType::const_iterator itr = start; itr != end; ++itr)
		_do(itr->second);
}

#define sMapMgr MapManager::Instance()

#endif

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

#include "MapManager.h"

#include "Config/Singleton.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "../World/World.h"
#include "DBStorage/SQLStorages.h"


/*
#include "MapPersistentStateMgr.h"
#include "Transports.h"
#include "ObjectMgr.h"
*/

#define CLASS_LOCK Origin::ClassLevelLockable<MapManager, std::recursive_mutex>
INSTANTIATE_SINGLETON_2(MapManager, CLASS_LOCK);
INSTANTIATE_CLASS_MUTEX(MapManager, std::recursive_mutex);

MapManager::MapManager()
{
	i_timer.SetInterval(sWorld.getConfig(CONFIG_UINT32_INTERVAL_MAPUPDATE));
}

MapManager::~MapManager()
{
	for (MapMapType::iterator iter = i_maps.begin(); iter != i_maps.end(); ++iter)
	{
		sLog.outDetail("Removing map: '%d'", iter->second->GetId());
		delete iter->second;
	}
}

void
MapManager::Initialize()
{
	InitMaxInstanceId();
}

void MapManager::InitializeVisibilityDistanceInfo()
{
	for (MapMapType::iterator iter = i_maps.begin(); iter != i_maps.end(); ++iter)
		(*iter).second->InitVisibilityDistance();
}

/// @param id - MapId of the to be created map. @param obj WorldObject for which the map is to be created. Must be player for Instancable maps.
Map* MapManager::CreateMap(uint32 id, const WorldObject* obj)
{
	Guard _guard(*this);

	const MapEntry* entry = sMapEntry.LookupEntry<MapEntry>(id);
	if (!entry)
		return nullptr;

	Map* m;
	if (entry->Instanceable())
	{
		ORIGIN_ASSERT(obj && obj->GetTypeId() == TYPEID_PLAYER);
		// create DungeonMap object
		m = CreateInstance(id, (Player*)obj);
		// Load active objects for this map
		//sObjectMgr.LoadActiveEntities(m);
	}
	else
	{
		// create regular non-instanceable map
		m = FindMap(id);
		if (m == nullptr)
		{
			m = new WorldMap(id/*, i_gridCleanUpDelay*/);
			// add map into container
			i_maps[MapID(id)] = m;

			// non-instanceable maps always expected have saved state
			//m->CreateInstanceData(true);
		}
	}

	return m;
}

Map* MapManager::FindMap(uint32 mapid, uint32 instanceId) const
{
	Guard guard(*this);

	MapMapType::const_iterator iter = i_maps.find(MapID(mapid, instanceId));
	if (iter == i_maps.end())
		return nullptr;

	// this is a small workaround for transports
	if (instanceId == 0 && iter->second->Instanceable())
	{
		assert(false);
		return nullptr;
	}

	return iter->second;
}

void MapManager::DeleteInstance(uint32 mapid, uint32 instanceId)
{
	Guard _guard(*this);

	MapMapType::iterator iter = i_maps.find(MapID(mapid, instanceId));
	if (iter != i_maps.end())
	{
		Map* pMap = iter->second;
		if (pMap->Instanceable())
		{
			i_maps.erase(iter);

			//pMap->UnloadAll(true);
			delete pMap;
		}
	}
}

void MapManager::Update(uint32 diff)
{
	i_timer.Update(diff);
	if (!i_timer.Passed())
		return;

	for (MapMapType::iterator iter = i_maps.begin(); iter != i_maps.end(); ++iter)
		iter->second->Update((uint32)i_timer.GetCurrent());

	// remove all maps which can be unloaded
	MapMapType::iterator iter = i_maps.begin();
	while (iter != i_maps.end())
	{
		Map* pMap = iter->second;
		// check if map can be unloaded
		if (pMap->CanUnload((uint32)i_timer.GetCurrent()))
		{
			//pMap->UnloadAll(true);
			delete pMap;

			i_maps.erase(iter++);
		}
		else
			++iter;
	}

	i_timer.SetCurrent(0);
}

void MapManager::RemoveAllObjectsInRemoveList()
{
	for (MapMapType::iterator iter = i_maps.begin(); iter != i_maps.end(); ++iter)
		iter->second->RemoveAllObjectsInRemoveList();
}

bool MapManager::IsValidMAP(uint32 mapid)
{
	MapEntry const* mEntry = sMapEntry.LookupEntry<MapEntry>(mapid);
	return mEntry && (!mEntry->IsDungeon()/* || ObjectMgr::GetInstanceTemplate(mapid)*/);
	// TODO: add check for battleground template
}

void MapManager::UnloadAll()
{
	/*for (MapMapType::iterator iter = i_maps.begin(); iter != i_maps.end(); ++iter)
		iter->second->UnloadAll(true);*/

	while (!i_maps.empty())
	{
		delete i_maps.begin()->second;
		i_maps.erase(i_maps.begin());
	}
}

void MapManager::InitMaxInstanceId()
{
	i_MaxInstanceId = 0;

	QueryResult* result = CharacterDatabase.Query("SELECT MAX(id) FROM instance");
	if (result)
	{
		i_MaxInstanceId = result->Fetch()[0].GetUInt32();
		delete result;
	}
}

uint32 MapManager::GetNumInstances()
{
	uint32 ret = 0;
	for (MapMapType::iterator itr = i_maps.begin(); itr != i_maps.end(); ++itr)
	{
		Map* map = itr->second;
		if (!map->IsDungeon()) continue;
		ret += 1;
	}
	return ret;
}

uint32 MapManager::GetNumPlayersInInstances()
{
	uint32 ret = 0;
	for (MapMapType::iterator itr = i_maps.begin(); itr != i_maps.end(); ++itr)
	{
		Map* map = itr->second;
		if (!map->IsDungeon()) continue;
		ret += map->GetPlayers().getSize();
	}
	return ret;
}

///// returns a new or existing Instance
///// in case of battlegrounds it will only return an existing map, those maps are created by bg-system
Map* MapManager::CreateInstance(uint32 id, Player* player)
{
	Map* map = nullptr;
	Map* pNewMap = nullptr;
	uint32 NewInstanceId;                                    // instanceId of the resulting map
	const MapEntry* entry = sMapEntry.LookupEntry<MapEntry>(id);

	if (entry->IsBattleGround())
	{
		// find existing bg map for player
		/*NewInstanceId = player->GetBattleGroundId();
		ORIGIN_ASSERT(NewInstanceId);
		map = FindMap(id, NewInstanceId);
		ORIGIN_ASSERT(map);*/
	}
	/*else if (DungeonPersistentState* pSave = player->GetBoundInstanceSaveForSelfOrGroup(id))
	{
		// solo/perm/group
		NewInstanceId = pSave->GetInstanceId();
		map = FindMap(id, NewInstanceId);
		// it is possible that the save exists but the map doesn't
		if (!map)
			pNewMap = CreateDungeonMap(id, NewInstanceId, pSave);
	}*/
	else
	{
		// if no instanceId via group members or instance saves is found
		// the instance will be created for the first time
		//NewInstanceId = GenerateInstanceId();

		//pNewMap = CreateDungeonMap(id, NewInstanceId);
	}

	// add a new map object into the registry
	if (pNewMap)
	{
		i_maps[MapID(id, NewInstanceId)] = pNewMap;
		map = pNewMap;
	}

	return map;
}
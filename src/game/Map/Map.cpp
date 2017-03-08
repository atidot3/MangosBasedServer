#include "Map.h"
#include "MapManager.h"
#include "Player.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "../World/World.h"
#include "MapRefManager.h"
#include "DBStorage/SQLStorages.h"

#define DEFAULT_VISIBILITY_DISTANCE 90.0f

Map::~Map()
{
}
Map::Map(uint32 id, time_t expiry, uint32 InstanceId)
	: i_mapEntry(sMapEntry.LookupEntry<MapEntry>(id)),
	i_id(id), i_InstanceId(InstanceId), m_unloadTimer(0),
	m_VisibleDistance(DEFAULT_VISIBILITY_DISTANCE)
{
}
void Map::InitVisibilityDistance()
{
	// init visibility for continents
	m_VisibleDistance = World::GetMaxVisibleDistanceOnContinents();
}
void Map::Update(const uint32& t_diff)
{
	/// update worldsessions for existing players
	for (m_mapRefIter = m_mapRefManager.begin(); m_mapRefIter != m_mapRefManager.end(); ++m_mapRefIter)
	{
		Player* plr = m_mapRefIter->getSource();
		if (plr && plr->IsInWorld())
		{
			WorldSession* pSession = plr->GetSession();
			MapSessionFilter updater(pSession);

			pSession->Update(updater);
		}
	}
	/// update players at tick
	for (m_mapRefIter = m_mapRefManager.begin(); m_mapRefIter != m_mapRefManager.end(); ++m_mapRefIter)
	{
		Player* plr = m_mapRefIter->getSource();
		if (plr && plr->IsInWorld())
		{
			WorldObject::UpdateHelper helper(plr);
			helper.Update(t_diff);
		}
	}
	/// get all player around ?
	for (m_mapRefIter = m_mapRefManager.begin(); m_mapRefIter != m_mapRefManager.end(); ++m_mapRefIter)
	{
		Player* plr = m_mapRefIter->getSource();
		if (plr && plr->IsInWorld())
		{
			for (MapRefManager::iterator m_mapRefIterbis = m_mapRefManager.begin(); m_mapRefIterbis != m_mapRefManager.end(); ++m_mapRefIterbis)
			{
				Player* plrbis = m_mapRefIterbis->getSource();
				if (plrbis && plrbis->IsInWorld() && plr->GetGUID() != plrbis->GetGUID())
				{
					
					float distance = plr->GetDistance(plrbis);
					if (distance <= 5000)
					{
						if (plr->isInList(plrbis->GetGUID()) == false)
						{
							plr->addToList(plrbis);
						}
					}
					else
					{
						if (plr->isInList(plrbis->GetGUID()) == true)
						{
							WorldPacket packet(SMSG_DESTROY_OBJECT, 4);
							packet << (uint32)plrbis->GetGUIDLow();
							plr->GetSession()->SendPacket(&packet);

							plr->removeFromList(plrbis->GetGUID());
						}
					}
				}
			}
		}
	}
	/// for creature

	/// Send world objects and item update field changes
	SendObjectUpdates();
}
void Map::Remove(Player* player, bool remove)
{
	// destruct for player ?
	WorldPacket packet(SMSG_DESTROY_OBJECT, 4);
	packet << (uint32)player->GetGUIDLow();

	for (m_mapRefIter = m_mapRefManager.begin(); m_mapRefIter != m_mapRefManager.end(); ++m_mapRefIter)
	{
		Player* plr = m_mapRefIter->getSource();
		if (player != plr)
		{
			plr->GetSession()->SendPacket(&packet);
		}
	}
//	if (i_data) // INSTANCE DATA
//		i_data->OnPlayerLeave(player);

	if (remove)
		player->CleanupsBeforeDelete();
	else
		player->RemoveFromWorld();

	// this may be called during Map::Update
	// after decrement+unlink, ++m_mapRefIter will continue correctly
	// when the first element of the list is being removed
	// nocheck_prev will return the padding element of the RefManager
	// instead of nullptr in the case of prev
	if (m_mapRefIter == player->GetMapRef())
		m_mapRefIter = m_mapRefIter->nocheck_prev();
	
	player->GetMapRef().unlink();
	
	//SendRemoveTransports(player);
	//UpdateObjectVisibility(player, cell, p);

	player->ResetMap();
	if (remove)
		DeleteFromWorld(player);
}

template<class T>
void
Map::Remove(T* obj, bool remove)
{
	ORIGIN_ASSERT(obj);
	if (obj->isActiveObject())
		RemoveFromActive(obj);

	if (remove)
		obj->CleanupsBeforeDelete();
	else
		obj->RemoveFromWorld();

	UpdateObjectVisibility(obj, p);                   // i think will be better to call this function while object still in grid, this changes nothing but logically is better(as for me)
	obj->ResetMap();
	if (remove)
	{
		// if option set then object already saved at this moment
		if (!sWorld.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY))
			obj->SaveRespawnTime();

		// Note: In case resurrectable corpse and pet its removed from global lists in own destructor
		delete obj;
	}
}

template<class T>
void
Map::Add(T* obj)
{
	ORIGIN_ASSERT(obj);

	obj->SetMap(this);

	obj->AddToWorld();

	if (obj->isActiveObject())
		AddToActive(obj);

	obj->SetItsNewObject(true);
	UpdateObjectVisibility(obj, p);
	obj->SetItsNewObject(false);
}
void Map::SendInitSelf(Player *p)
{
	if (!p)
		return;
	p->BuildCreateUpdateBlockForPlayer(p); // SEND CREATION TO ME !
}
bool Map::Add(Player* player)
{
	player->GetMapRef().link(this, player);
	player->SetMap(this);

	player->AddToWorld();

	// init my stats etc and send to me
	SendInitSelf(player);
	//SendInitTransports(player);

	// update player state for other player and visa-versa
	// should get all player etc

	//if (i_data)
		//i_data->OnPlayerEnter(player);

	return true;
}
void Map::DeleteFromWorld(Player* pl)
{
	sObjectAccessor.RemoveObject(pl);
	delete pl;
}
void Map::RemoveAllObjectsInRemoveList()
{
	if (i_objectsToRemove.empty())
		return;

	while (!i_objectsToRemove.empty())
	{
		WorldObject* obj = *i_objectsToRemove.begin();
		i_objectsToRemove.erase(i_objectsToRemove.begin());

		/*switch (obj->GetTypeId())
		{
		case TYPEID_CORPSE:
		{
			// ??? WTF
			Corpse* corpse = GetCorpse(obj->GetObjectGuid());
			if (!corpse)
				sLog.outError("Try delete corpse/bones %u that not in map", obj->GetGUIDLow());
			else
				Remove(corpse, true);
			break;
		}
		case TYPEID_DYNAMICOBJECT:
			Remove((DynamicObject*)obj, true);
			break;
		case TYPEID_GAMEOBJECT:
			Remove((GameObject*)obj, true);
			break;
		case TYPEID_UNIT:
			Remove((Creature*)obj, true);
			break;
		default:
			sLog.outError("Non-grid object (TypeId: %u) in grid object removing list, ignored.", obj->GetTypeId());
			break;
		}*/
	}
}
void Map::SendObjectUpdates()
{
	while (!i_objectsToClientUpdate.empty())
	{
		Object* obj = *i_objectsToClientUpdate.begin();
		i_objectsToClientUpdate.erase(i_objectsToClientUpdate.begin());
		// COMPLETLY TEMPORARY !!!!!
		obj->UpdateObject();
		//SendInitSelf((Player*)obj);
		//obj->BuildUpdateData(update_players);
	}
}
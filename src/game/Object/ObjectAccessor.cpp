#include "ObjectAccessor.h"
//#include "ObjectMgr.h"
#include "Config/Singleton.h"
#include "Player.h"
//#include "Item.h"
//#include "Corpse.h"
#include "MapManager.h"
#include "Map.h"
#include "ObjectGuid.h"
#include "World.h"

#include <mutex>

#define CLASS_LOCK Origin::ClassLevelLockable<ObjectAccessor, std::mutex>
INSTANTIATE_SINGLETON_2(ObjectAccessor, CLASS_LOCK);
INSTANTIATE_CLASS_MUTEX(ObjectAccessor, std::mutex);

ObjectAccessor::ObjectAccessor() {}
ObjectAccessor::~ObjectAccessor()
{
	/*for (Player2CorpsesMapType::const_iterator itr = i_player2corpse.begin(); itr != i_player2corpse.end(); ++itr)
	{
		itr->second->RemoveFromWorld();
		delete itr->second;
	}*/
}

Unit*
ObjectAccessor::GetUnit(WorldObject const& u, ObjectGuid guid)
{
	if (!guid)
		return nullptr;

	if (guid.IsPlayer())
		return FindPlayer(guid);

	if (!u.IsInWorld())
		return nullptr;

	return nullptr;
	//return u.GetMap()->GetAnyTypeCreature(guid); // NEED CREATURE
}

Player* ObjectAccessor::FindPlayer(ObjectGuid guid, bool inWorld /*= true*/)
{
	if (!guid)
		return nullptr;

	Player* plr = HashMapHolder<Player>::Find(guid);
	if (!plr || (!plr->IsInWorld() && inWorld))
		return nullptr;

	return plr;
}

Player* ObjectAccessor::FindPlayerByName(const char* name)
{
	HashMapHolder<Player>::ReadGuard g(HashMapHolder<Player>::GetLock());
	HashMapHolder<Player>::MapType& m = sObjectAccessor.GetPlayers();
	for (HashMapHolder<Player>::MapType::iterator iter = m.begin(); iter != m.end(); ++iter)
		if (iter->second->IsInWorld() && (::strcmp(name, iter->second->GetName()) == 0))
			return iter->second;

	return nullptr;
}

void
ObjectAccessor::SaveAllPlayers()
{
	HashMapHolder<Player>::ReadGuard g(HashMapHolder<Player>::GetLock());
	HashMapHolder<Player>::MapType& m = sObjectAccessor.GetPlayers();
	for (HashMapHolder<Player>::MapType::iterator itr = m.begin(); itr != m.end(); ++itr)
		itr->second->SaveToDB();
}

void ObjectAccessor::KickPlayer(ObjectGuid guid)
{
	if (Player* p = ObjectAccessor::FindPlayer(guid, false))
	{
		WorldSession* s = p->GetSession();
		s->KickPlayer();                            // mark session to remove at next session list update
		s->LogoutPlayer(false);                     // logout player without waiting next session list update
	}
}

/// Define the static member of HashMapHolder

template <class T> typename HashMapHolder<T>::MapType HashMapHolder<T>::m_objectMap;
template <class T> std::mutex HashMapHolder<T>::i_lock;

/// Global definitions for the hashmap storage

template class HashMapHolder<Player>;

#ifndef _MAPREFERENCE_H
#define _MAPREFERENCE_H

#include <LinkedReference/Reference.h>
#include "Map.h"

class MapReference : public Reference<Map, Player>
{
protected:
	void targetObjectBuildLink() override
	{
		// called from link()
		getTarget()->m_mapRefManager.insertFirst(this);
		getTarget()->m_mapRefManager.incSize();
	}
	void targetObjectDestroyLink() override
	{
		// called from unlink()
		if (isValid()) getTarget()->m_mapRefManager.decSize();
	}
	void sourceObjectDestroyLink() override
	{
		// called from invalidate()
		getTarget()->m_mapRefManager.decSize();
	}
public:
	MapReference() : Reference<Map, Player>() {}
	~MapReference() { unlink(); }
	MapReference* next() { return (MapReference*)Reference<Map, Player>::next(); }
	MapReference const* next() const { return (MapReference const*)Reference<Map, Player>::next(); }
	MapReference* nockeck_prev() { return (MapReference*)Reference<Map, Player>::nocheck_prev(); }
	MapReference const* nocheck_prev() const { return (MapReference const*)Reference<Map, Player>::nocheck_prev(); }
};
#endif
